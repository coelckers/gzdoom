// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2008-2018 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//



#include <future>

#include "p_local.h"
#include "c_dispatch.h"
#include "r_defs.h"
#include "g_levellocals.h"
#include "r_sections.h"
#include "earcut.hpp"
#include "stats.h"
#include "p_setup.h"
#include "c_dispatch.h"
#include "memarena.h"

using DoublePoint = std::pair<DVector2, DVector2>;

template<> struct THashTraits<DoublePoint>
{
	hash_t Hash(const DoublePoint &key)
	{
		return (hash_t)SuperFastHash((const char*)(const void*)&key, sizeof(key));
	}
	int Compare(const DoublePoint &left, const DoublePoint &right) { return left != right; }
};

struct WorkSectionLine
{
	vertex_t *start;
	vertex_t *end;
	side_t *sidedef;
	seg_t *refseg;
	int tempindex;
	int mygroup;
	WorkSectionLine *partner;
	bool flagged;
};


struct WorkSection
{
	int sectorindex;
	int mapsection;
	bool hasminisegs;
	bool bad;	// Did not produce a proper area and cannot be triangulated by tesselation.
	TArray<WorkSectionLine*>segments;
	TArray<side_t *> originalSides;	// The segs will lose some of these while working on them.
	TArray<int> subsectors;
};

struct TriangleWorkData
{
	BoundingRect boundingBox;
	unsigned boundingLoopStart;
};

struct GroupWork
{
	WorkSection *section;
	TriangleWorkData *work;
	int index;
};

struct Group
{
	TArray<GroupWork> groupedSections;
	TMap<int, bool> sideMap;
	TArray<WorkSectionLine*> segments;
	TArray<int> subsectors;
};

class FSectionCreator
{
	FLevelLocals *Level;
	FMemArena allocator;
	TArray<WorkSectionLine *>AllAllocatedLines;

	bool verbose = false;
	TMap<int, TArray<int>> subsectormap;
	TArray<WorkSection> sections;

	TArray<TriangleWorkData> triangles;

	TArray<Group> groups;
	TArray<int> groupForSection;

public:

	FSectionCreator(FLevelLocals *l)
	{
		Level = l;
		// These must be manually destroyed but not deleted.
		for (auto line : AllAllocatedLines)
		{
			line->~WorkSectionLine();
		}
	}

	// Allocate these from a static allocator so that they remain fixed in memory and avoid excessive small allocations.
	WorkSectionLine *NewLine()
	{
		void *mem = allocator.Alloc(sizeof(WorkSectionLine));
		auto line = new(mem) WorkSectionLine;
		AllAllocatedLines.Push(line);
		return line;
	}

	int IndexOf(WorkSectionLine *line)
	{
		return AllAllocatedLines.Find(line);	// slow, but only used for debugging.
	}

	int MakeKey(int sector, int mapsection)
	{
		return sector + (mapsection << 16);
	}

	int MakeKey(const subsector_t &sub)
	{
		return MakeKey(sub.render_sector->Index(), sub.mapsection);
	}

	int SectorFromKey(int key)
	{
		return key & 65535;
	}

	int MapsectionFromKey(int key)
	{
		return key >> 16;
	}

	//==========================================================================
	//
	// Groups subsectors by both sectors and mapsection
	// The parts of a sector in a single map section is the upper boundary
	// for what a sector section may contain.
	//
	//==========================================================================

	void GroupSubsectors()
	{
		for (auto &sub : Level->subsectors)
		{
			int key = MakeKey(sub);
			auto &array = subsectormap[key];
			array.Push(sub.Index());
		}
	}

	//==========================================================================
	//
	// Go through the list and split each element further into 
	// connected groups of subsectors.
	//
	//==========================================================================

	TArray < TArray<int>> CompileSections()
	{
		TMap<int, TArray<int>>::Pair *pair;
		TMap<int, TArray<int>>::Iterator it(subsectormap);
		TArray<TArray<int>> rawsections;	// list of unprocessed subsectors. Sector and mapsection can be retrieved from the elements so aren't stored.

		while (it.NextPair(pair))
		{
			CompileSections(pair->Value, rawsections);
		}

		// Make sure that all subsectors have a sector. In some degenerate cases a subsector may come up empty.
		// An example is in Doom.wad E3M4 near linedef 1087. With the grouping data here this is relatively easy to fix.
		sector_t *lastsector = &Level->sectors[0];
		for (auto &rawsection : rawsections)
		{
			sector_t *mysector = nullptr;
			bool missing = false;
			for (auto num : rawsection)
			{
				auto &sub = Level->subsectors[num];
				if (sub.sector == nullptr) missing = true;
				else mysector = sub.sector;
			}
			// Should the worst case happen and no sector be found, use the last used one. Subsectors must not be sector-less!
			if (mysector == nullptr) mysector = lastsector;
			else lastsector = mysector;
			for (auto num : rawsection)
			{
				auto &sub = Level->subsectors[num];
				if (sub.sector == nullptr)
				{
					sub.sector = mysector;
				}
			}
		}

		subsectormap.Clear();
		return rawsections;
	}

	//==========================================================================
	//
	// Find all groups of connected subsectors and put them into the group list
	//
	//==========================================================================

	void CompileSections(TArray<int> &list, TArray<TArray<int>>&rawsections)
	{
		TArray<int> sublist;
		TArray<seg_t *> seglist;
		while (list.Size() > 0)
		{
			sublist.Clear();
			seglist.Clear();
			int index;
			list.Pop(index);
			auto sub = &Level->subsectors[index];

			auto collect = [&](subsector_t *sub)
			{
				sublist.Push(sub->Index());
				for (unsigned i = 0; i < sub->numlines; i++)
				{
					if (sub->firstline[i].PartnerSeg && sub->firstline[i].Subsector->render_sector == sub->firstline[i].PartnerSeg->Subsector->render_sector)
					{
						seglist.Push(sub->firstline[i].PartnerSeg);
					}
				}
			};

			collect(sub);

			for (unsigned i = 0; i < seglist.Size(); i++)
			{
				auto subi = seglist[i]->Subsector->Index();

				for (unsigned j = 0; j < list.Size(); j++)
				{
					if (subi == list[j])
					{
						collect(&Level->subsectors[subi]);
						list.Delete(j);
						j--;
					}
				}
			}
			rawsections.Push(std::move(sublist));
		}
	}


	//==========================================================================
	//
	//
	//==========================================================================

	void MakeOutlines()
	{
		auto rawsections = CompileSections();
		TArray<WorkSectionLine *> lineForSeg(Level->segs.Size(), true);
		memset(lineForSeg.Data(), 0, sizeof(WorkSectionLine*) * Level->segs.Size());
		for (auto &list : rawsections)
		{
			MakeOutline(list, lineForSeg);
		}
		rawsections.Reset();

		// Assign partners after everything has been collected
		for (auto &section : sections)
		{
			for (auto seg : section.segments)
			{
				if (seg->refseg && seg->refseg->PartnerSeg)
				{
					seg->partner = lineForSeg[seg->refseg->PartnerSeg->Index()];
				}
			}
		}
	}

	//==========================================================================
	//
	// Creates an outline for a given section
	//
	//==========================================================================

	void MakeOutline(TArray<int> &rawsection, TArray<WorkSectionLine *> &lineForSeg)
	{
		TArray<side_t *> foundsides;
		TArray<seg_t *> outersegs;
		TArray<seg_t *> loopedsegs;
		bool hasminisegs = false;
		bool bad = false;

		// Collect all the segs that make up the outline of this section.
		for (auto j : rawsection)
		{
			auto sub = &Level->subsectors[j];

			for (unsigned i = 0; i < sub->numlines; i++)
			{
				if (!sub->firstline[i].PartnerSeg || sub->firstline[i].Subsector->render_sector != sub->firstline[i].PartnerSeg->Subsector->render_sector)
				{
					outersegs.Push(&sub->firstline[i]);
					if (sub->firstline[i].sidedef == nullptr) hasminisegs = true;
				}
				if (sub->firstline[i].sidedef)
				{
					foundsides.Push(sub->firstline[i].sidedef);	// This may contain duplicate. Let's deal with those later.
				}
			}
		}

		// Loop until all segs have been used.
		seg_t *seg = nullptr;
		unsigned startindex = 0;
		while (outersegs.Size() > 0)
		{
			if (seg == nullptr)
			{
				for (unsigned i = 0; i < outersegs.Size(); i++)
				{
					if (outersegs[i]->sidedef != nullptr && outersegs[i]->sidedef->V1() == outersegs[i]->v1)
					{
						seg = outersegs[i];
						outersegs.Delete(i);
						break;
					}
				}
				if (seg == nullptr)
				{
					// There's only minisegs left. Most likely this is just node garbage.
					// Todo: Need to check.
					seg = outersegs[0];
					outersegs.Delete(0);
				}
				startindex = loopedsegs.Push(seg);
			}

			// Find the next seg in the loop.

			auto segangle = VecToAngle(seg->v2->fPos() - seg->v1->fPos());

			seg_t *pick = nullptr;
			int pickindex = -1;
			for (unsigned i = 0; i < outersegs.Size(); i++)
			{
				auto secondseg = outersegs[i];
				if (secondseg->v1->fPos() == seg->v2->fPos())
				{
					// This should never choose a miniseg over a real sidedef. 
					if (pick == nullptr || (pick->sidedef == nullptr && secondseg->sidedef != nullptr))
					{
						pick = secondseg;
						pickindex = i;
					}
					else if (pick->sidedef == nullptr || secondseg->sidedef != nullptr)
					{
						// If there's more than one pick the one with the smallest angle.
						auto pickangle = deltaangle(segangle, VecToAngle(pick->v2->fPos() - pick->v1->fPos()));
						auto secondangle = deltaangle(segangle, VecToAngle(secondseg->v2->fPos() - secondseg->v1->fPos()));

						if (secondangle < pickangle)
						{
							pick = secondseg;
							pickindex = i;
						}
					}
				}
			}
			if (pick)
			{
				loopedsegs.Push(pick);
				outersegs.Delete(pickindex);
				seg = pick;
			}
			else
			{
				// There was no more seg connecting to the last one. We should have reached the beginning of the loop again.
				if (loopedsegs.Last() == nullptr || loopedsegs[startindex]->v1->fPos() != loopedsegs.Last()->v2->fPos())
				{
					// Did not find another one but have an unclosed loop. This should never happen and would indicate broken nodes.
					// Error out and let the calling code deal with it.
					DPrintf(DMSG_NOTIFY, "Unclosed loop in sector %d at position (%d, %d)\n", loopedsegs[0]->Subsector->render_sector->Index(), (int)loopedsegs[0]->v1->fX(), (int)loopedsegs[0]->v1->fY());
					bad = true;
				}
				seg = nullptr;
				loopedsegs.Push(nullptr);	// A separator is not really needed but useful for debugging.
			}
		}
		if (loopedsegs.Size() > 0)
		{
			auto sector = loopedsegs[0]->Subsector->render_sector->Index();
			auto mapsec = loopedsegs[0]->Subsector->mapsection;

			TArray<WorkSectionLine *> sectionlines(loopedsegs.Size(), true);
			for (unsigned i = 0; i < loopedsegs.Size(); i++)
			{
				if (loopedsegs[i])
				{
					sectionlines[i] = NewLine();
					*sectionlines[i] = { loopedsegs[i]->v1, loopedsegs[i]->v2, loopedsegs[i]->sidedef, loopedsegs[i], -1, (int)sections.Size(), nullptr };
					lineForSeg[loopedsegs[i]->Index()] = sectionlines[i];
				}
				else
				{
					sectionlines[i] = NewLine();
					*sectionlines[i] = { nullptr, nullptr, nullptr, nullptr, -1, (int)sections.Size(), nullptr };
				}
			}
			sections.Reserve(1);
			auto &section = sections.Last();
			section.sectorindex = sector;
			section.mapsection = mapsec;
			section.hasminisegs = hasminisegs;
			section.bad = bad;
			section.originalSides = std::move(foundsides);
			section.segments = std::move(sectionlines);
			section.subsectors = std::move(rawsection);
		}
	}


	//=============================================================================
	//
	// Tries to merge continuous segs on the same sidedef
	//
	//=============================================================================
	void MergeLines()
	{
		for (auto &build : sections)
		{
			for (int i = (int)build.segments.Size() - 1; i > 0; i--)
			{
				auto ln1 = build.segments[i];
				auto ln2 = build.segments[i - 1];

				if (ln1->sidedef && ln2->sidedef && ln1->sidedef == ln2->sidedef)
				{
					if (ln1->partner == nullptr && ln2->partner == nullptr)
					{
						// identical references. These 2 lines can be merged.
						ln2->end = ln1->end;
						build.segments.Delete(i);
					}
					else if (ln1->partner && ln2->partner && ln1->partner->mygroup == ln2->partner->mygroup)
					{
						auto &section = sections[ln1->partner->mygroup];
						auto index1 = section.segments.Find(ln1->partner);	// note that ln1 and ln2 are ordered backward, so the partners are ordered forward.
						auto index2 = section.segments.Find(ln2->partner);
						if (index2 == index1 + 1 && index2 < section.segments.Size())
						{
							// Merge both sides at once to ensure the data remains consistent.
							ln2->partner->start = ln1->partner->start;
							section.segments.Delete(index1);
							ln2->end = ln1->end;
							build.segments.Delete(i);
						}
					}
				}
			}
		}
	}


	//=============================================================================
	//
	//
	//
	//=============================================================================

	void FindOuterLoops()
	{
		triangles.Resize(sections.Size());
		for (unsigned int i = 0; i < sections.Size(); i++)
		{
			auto &section = sections[i];
			auto &work = triangles[i];

			BoundingRect bounds(false);
			BoundingRect loopBounds(false);
			BoundingRect outermostBounds(false);
			unsigned outermoststart = ~0u;
			unsigned loopstart = 0;
			bool ispolyorg = false;
			for (unsigned i = 0; i <= section.segments.Size(); i++)
			{
				if (i < section.segments.Size() && section.segments[i]->refseg)
				{
					ispolyorg |= !!(section.segments[i]->refseg->Subsector->flags & SSECF_POLYORG);
					loopBounds.addVertex(section.segments[i]->start->fX(), section.segments[i]->start->fY());
					loopBounds.addVertex(section.segments[i]->end->fX(), section.segments[i]->end->fY());
					bounds.addVertex(section.segments[i]->start->fX(), section.segments[i]->start->fY());
					bounds.addVertex(section.segments[i]->end->fX(), section.segments[i]->end->fY());
				}
				else
				{
					if (outermostBounds.left == 1e32 || loopBounds.contains(outermostBounds))
					{
						outermostBounds = loopBounds;
						outermoststart = loopstart;
						loopstart = i + 1;
					}
					loopBounds.setEmpty();
				}
			}
			work.boundingLoopStart = outermoststart;
			work.boundingBox = outermostBounds;
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	void GroupSections()
	{
		TArray<GroupWork> workingSet;

		GroupWork first = { &sections[0], &triangles[0], 0 };
		workingSet.Push(first);
		groupForSection.Resize(sections.Size());
		for (auto &i : groupForSection) i = -1;

		for (unsigned i = 1; i < sections.Size(); i++)
		{
			auto sect = &sections[i];
			if (sect->segments.Size() == 0) continue;
			if (sect->sectorindex == first.section->sectorindex && sect->mapsection == first.section->mapsection)
			{
				workingSet.Push({ sect, &triangles[i], (int)i });
			}
			else
			{
				GroupWorkingSet(workingSet);
				workingSet.Clear();
				first = { &sections[i], &triangles[i], (int)i };
				workingSet.Push(first);
			}
		}
		GroupWorkingSet(workingSet);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	void GroupWorkingSet(TArray<GroupWork> &workingSet)
	{
		const double MAX_GROUP_DIST = 256;
		TArray<GroupWork> build;
		TArray<int> subsectorcopy;

		if (workingSet.Size() == 1)
		{
			groupForSection[workingSet[0].index] = groups.Size();
			Group g;
			g.subsectors = std::move(workingSet[0].section->subsectors);
			g.groupedSections = std::move(workingSet);
			groups.Push(std::move(g));
			return;
		}

		while (workingSet.Size() > 0)
		{
			build.Clear();
			build.Push(workingSet[0]);
			groupForSection[workingSet[0].index] = groups.Size();
			subsectorcopy = std::move(workingSet[0].section->subsectors);
			workingSet.Delete(0);


			// Don't use iterators here. These arrays are modified inside.
			for (unsigned j = 0; j < build.Size(); j++)
			{
				auto current = build[j];
				for (int i = 0; i < (int)workingSet.Size(); i++)
				{
					// Are both sections close together?
					double dist = current.work->boundingBox.distanceTo(workingSet[i].work->boundingBox);
					if (dist < MAX_GROUP_DIST)
					{
						build.Push(workingSet[i]);
						groupForSection[workingSet[i].index] = groups.Size();
						subsectorcopy.Append(workingSet[i].section->subsectors);
						workingSet.Delete(i);
						i--;
						continue;
					}
					// Also put in the same group if they share a sidedef.
					bool sharing_sd = CheckForSharedSidedef(*current.section, *workingSet[i].section);
					if (sharing_sd)
					{
						build.Push(workingSet[i]);
						groupForSection[workingSet[i].index] = groups.Size();
						subsectorcopy.Append(workingSet[i].section->subsectors);
						workingSet.Delete(i);
						i--;
						continue;
					}
				}
			}
			Group g;
			g.groupedSections = std::move(build);
			g.subsectors = std::move(subsectorcopy);
			groups.Push(std::move(g));
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	bool CheckForSharedSidedef(WorkSection &set1, WorkSection &set2)
	{
		for (auto seg : set1.segments)
		{
			if (seg->sidedef != nullptr)
			{
				for (auto seg2 : set2.segments)
				{
					if (seg2->sidedef == seg->sidedef) return true;
				}
			}
		}
		return false;
	}



	//=============================================================================
	//
	//
	//
	//=============================================================================

	void ConstructOutput(FSectionContainer &output)
	{
		output.allSections.Resize(groups.Size());
		output.allIndices.Resize(2*Level->sectors.Size());
		output.firstSectionForSectorPtr = &output.allIndices[0];
		output.numberOfSectionForSectorPtr = &output.allIndices[Level->sectors.Size()];
		memset(output.firstSectionForSectorPtr, -1, sizeof(int) * Level->sectors.Size());
		memset(output.numberOfSectionForSectorPtr, 0, sizeof(int) * Level->sectors.Size());

		unsigned numsegments = 0;
		unsigned numsides = 0;

		// First index all the line segments here so that we can later do some quick referencing via the global array and count them and the distinct number of sidedefs in the section.
		for (auto &group : groups)
		{
			for (auto &work : group.groupedSections)
			{
				auto &section = *work.section;
				for (auto segment : section.segments)
				{
					if (segment->refseg)
					{
						segment->tempindex = numsegments++;
						group.segments.Push(segment);
					}
				}
				for (auto side : section.originalSides)
				{
					// This is only used for attaching lights to those sidedefs which are not part of the outline.
					if (side->linedef && side->linedef->sidedef[1] && side->linedef->sidedef[0]->sector == side->linedef->sidedef[1]->sector)
						group.sideMap[side->Index()] = true;
				}
			}
			numsides += group.sideMap.CountUsed();
		}
		output.allLines.Resize(numsegments);
		output.allSides.Resize(numsides);
		output.allSubsectors.Resize(Level->subsectors.Size());

		numsegments = 0;
		numsides = 0;
		unsigned numsubsectors = 0;

		// Now piece it all together
		unsigned curgroup = 0;
		for (auto &group : groups)
		{
			FSection &dest = output.allSections[curgroup];
			dest.sector = &Level->sectors[group.groupedSections[0].section->sectorindex];
			dest.mapsection = (short)group.groupedSections[0].section->mapsection;
			dest.hacked = false;
			dest.lighthead = nullptr;
			dest.validcount = 0;
			dest.segments.Set(&output.allLines[numsegments], group.segments.Size());
			dest.sides.Set(&output.allSides[numsides], group.sideMap.CountUsed());
			dest.subsectors.Set(&output.allSubsectors[numsubsectors], group.subsectors.Size());
			dest.vertexindex = -1;
			dest.vertexcount = 0;
			dest.flags = 0;
			dest.bounds.setEmpty();
			numsegments += group.segments.Size();

			if (output.firstSectionForSectorPtr[dest.sector->Index()] == -1)
				output.firstSectionForSectorPtr[dest.sector->Index()] = curgroup;

			output.numberOfSectionForSectorPtr[dest.sector->Index()]++;

			for (auto &segment : group.segments)
			{
				// Use the indices calculated above to store these elements.
				auto &fseg = output.allLines[segment->tempindex];
				fseg.start = segment->start;
				fseg.end = segment->end;
				fseg.partner = segment->partner == nullptr ? nullptr : &output.allLines[segment->partner->tempindex];
				fseg.sidedef = segment->sidedef;
				fseg.section = &dest;
				dest.bounds.addVertex(fseg.start->fX(), fseg.start->fY());
				dest.bounds.addVertex(fseg.end->fX(), fseg.end->fY());
			}
			TMap<int, bool>::Iterator it(group.sideMap);
			TMap<int, bool>::Pair *pair;
			while (it.NextPair(pair))
			{
				output.allSides[numsides++] = &Level->sides[pair->Key];
			}
			for (auto ssi : group.subsectors)
			{
				output.allSubsectors[numsubsectors++] = &Level->subsectors[ssi];
				Level->subsectors[ssi].section = &output.allSections[curgroup];
			}
			curgroup++;
		}
	}

	//=============================================================================
	//
	// Check if some subsectors have come up empty on sections.
	// In this case assign the best fit from the containing sector.
	// This is only to ensure that the section pointer is not null.
	// These are always degenerate and do not produce any actual render output.
	//
	//=============================================================================

	void FixMissingReferences()
	{
		for (auto &sub : Level->subsectors)
		{
			if (sub.section == nullptr)
			{
				int sector = sub.sector->Index();
				int mapsection = sub.mapsection;
				auto sections = Level->sections.SectionsForSector(sector);
				FSection *bestfit = nullptr;
				for (auto &section : sections)
				{
					if (bestfit == nullptr)
					{
						bestfit = &section;
					}
					else if (bestfit->mapsection != section.mapsection && section.mapsection == mapsection)
					{
						bestfit = &section;
					}
					else if (section.mapsection == mapsection)
					{
						BoundingRect rc;
						for (unsigned i = 0; i < sub.numlines; i++)
						{
							rc.addVertex(sub.firstline[i].v1->fX(), sub.firstline[i].v1->fY());
							rc.addVertex(sub.firstline[i].v2->fX(), sub.firstline[i].v2->fY());
						}
						// Pick the one closer to this subsector. 
						if (rc.distanceTo(section.bounds) < rc.distanceTo(bestfit->bounds))
						{
							bestfit = &section;
						}
					}
				}
				// This should really never happen, but better be safe than sorry and assign at least something.
				if (bestfit == nullptr) bestfit = &Level->sections.allSections[0];
				sub.section = bestfit;
			}
		}

		// Set all empty sectors' initial index to 0, so that we do not have to range-check each access.
		for (unsigned i = 0; i < Level->sectors.Size(); i++)
		{
			if (Level->sections.firstSectionForSectorPtr[i] < 0)
			{
				Level->sections.firstSectionForSectorPtr[i] = 0;
				if (Level->sections.numberOfSectionForSectorPtr[i] > 0)
				{
					// This should never happen.
					Printf("Warning: Sector %d has a non-empty section list with no address\n", i);
					Level->sections.numberOfSectionForSectorPtr = 0;
				}
			}
		}
	}

};


//=============================================================================
//
//
//
//=============================================================================

void PrintSections(FLevelLocals *Level)
{
	FSectionContainer &container = Level->sections;
	Printf("Sections for %s\n", Level->MapName.GetChars());
	for (unsigned i = 0; i < container.allSections.Size(); i++)
	{
		auto &section = container.allSections[i];

		Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "\n\nStart of section %d sector %d, mapsection %d, bounds=(%2.3f, %2.3f, %2.3f, %2.3f)\n", i, section.sector->Index(), section.mapsection,
			section.bounds.left, section.bounds.top, section.bounds.right, section.bounds.bottom);

		for (unsigned j = 0; j < section.segments.Size(); j++)
		{
			auto &seg = section.segments[j];
			if (j > 0 && seg.start != section.segments[j - 1].end)
			{
				Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "\n");
			}
			FString partnerstring;
			if (seg.partner)
			{
				if (seg.partner->sidedef) partnerstring.Format(", partner = %d (line %d)", seg.partner->sidedef->Index(), seg.partner->sidedef->linedef->Index());
				else partnerstring = ", partner = seg";
			}
			else if (seg.sidedef && seg.sidedef->linedef)
			{
				partnerstring = ", one-sided line";
			}
			if (seg.sidedef)
			{
				Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "segment for sidedef %d (line %d) from (%2.6f, %2.6f) to (%2.6f, %2.6f)%s\n", 
					seg.sidedef->Index(), seg.sidedef->linedef->Index(), seg.start->fX(), seg.start->fY(), seg.end->fX(), seg.end->fY(), partnerstring.GetChars());
			}
			else
			{
				Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "segment for seg from (%2.6f, %2.6f) to (%2.6f, %2.6f)%s\n", 
					seg.start->fX(), seg.start->fY(), seg.end->fX(), seg.end->fY(), partnerstring.GetChars());
			}
		}
	}
	Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "%d sectors, %d subsectors, %d sections\n\n", Level->sectors.Size(), Level->subsectors.Size(), container.allSections.Size());
}


//=============================================================================
//
//
//
//=============================================================================

void CreateSections(FLevelLocals *Level)
{
	FSectionCreator creat(Level);
	creat.GroupSubsectors();
	creat.MakeOutlines();
	creat.MergeLines();
	creat.FindOuterLoops();
	creat.GroupSections();
	creat.ConstructOutput(Level->sections);
	creat.FixMissingReferences();
}


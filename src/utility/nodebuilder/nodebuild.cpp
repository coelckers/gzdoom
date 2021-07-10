/*
** nodebuild.cpp
**
** The main logic for the internal node builder.
**
**---------------------------------------------------------------------------
** Copyright 2002-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "doomdata.h"
#include "nodebuild.h"

const int MaxSegs = 64;
const int SplitCost = 8;
const int AAPreference = 16;

#if 0
#define D(x) x
#else
#define D(x) do{}while(0)
#endif

FNodeBuilder::FNodeBuilder(FLevel &lev)
: Level(lev), GLNodes(false), SegsStuffed(0)
{
	VertexMap = NULL;
	OldVertexTable = NULL;
}

FNodeBuilder::FNodeBuilder (FLevel &lev,
							TArray<FPolyStart> &polyspots, TArray<FPolyStart> &anchors,
							bool makeGLNodes)
	: Level(lev), GLNodes(makeGLNodes), SegsStuffed(0)
{
	VertexMap = new FVertexMap (*this, Level.MinX, Level.MinY, Level.MaxX, Level.MaxY);
	FindUsedVertices (Level.Vertices, Level.NumVertices);
	MakeSegsFromSides ();
	FindPolyContainers (polyspots, anchors);
	GroupSegPlanes ();
	BuildTree ();
}

FNodeBuilder::~FNodeBuilder()
{
	if (VertexMap != NULL)
	{
		delete VertexMap;
	}
	if (OldVertexTable != NULL)
	{
		delete[] OldVertexTable;
	}
}

void FNodeBuilder::BuildMini(bool makeGLNodes)
{
	GLNodes = makeGLNodes;
	GroupSegPlanesSimple();
	BuildTree();
}

void FNodeBuilder::Clear()
{
	SegsStuffed = 0;
	Nodes.Clear();
	Subsectors.Clear();
	SubsectorSets.Clear();
	Segs.Clear();
	Vertices.Clear();
	SegList.Clear();
	PlaneChecked.Clear();
	Planes.Clear();
	Touched.Clear();
	Colinear.Clear();
	SplitSharers.Clear();
	if (VertexMap == NULL)
	{
		VertexMap = new FVertexMapSimple(*this);
	}
}

void FNodeBuilder::BuildTree ()
{
	fixed_t bbox[4];

	HackSeg = UINT_MAX;
	HackMate = UINT_MAX;
	CreateNode (0, Segs.Size(), bbox);
	CreateSubsectorsForReal ();
}

int FNodeBuilder::CreateNode (uint32_t set, unsigned int count, fixed_t bbox[4])
{
	node_t node;
	int skip, selstat;
	uint32_t splitseg;

	skip = int(count / MaxSegs);

	// When building GL nodes, count may not be an exact count of the number of segs
	// in the set. That's okay, because we just use it to get a skip count, so an
	// estimate is fine.
	if ((selstat = SelectSplitter (set, node, splitseg, skip, true)) > 0 ||
		(skip > 0 && (selstat = SelectSplitter (set, node, splitseg, 1, true)) > 0) ||
		(selstat < 0 && (SelectSplitter (set, node, splitseg, skip, false) > 0 ||
						(skip > 0 && SelectSplitter (set, node, splitseg, 1, false)))) ||
		CheckSubsector (set, node, splitseg))
	{
		// Create a normal node
		uint32_t set1, set2;
		unsigned int count1, count2;

		SplitSegs (set, node, splitseg, set1, set2, count1, count2);
		D(PrintSet (1, set1));
		D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "(%d,%d) delta (%d,%d) from seg %d\n", node.x>>16, node.y>>16, node.dx>>16, node.dy>>16, splitseg));
		D(PrintSet (2, set2));
		node.intchildren[0] = CreateNode (set1, count1, node.nb_bbox[0]);
		node.intchildren[1] = CreateNode (set2, count2, node.nb_bbox[1]);
		bbox[BOXTOP] = MAX (node.nb_bbox[0][BOXTOP], node.nb_bbox[1][BOXTOP]);
		bbox[BOXBOTTOM] = MIN (node.nb_bbox[0][BOXBOTTOM], node.nb_bbox[1][BOXBOTTOM]);
		bbox[BOXLEFT] = MIN (node.nb_bbox[0][BOXLEFT], node.nb_bbox[1][BOXLEFT]);
		bbox[BOXRIGHT] = MAX (node.nb_bbox[0][BOXRIGHT], node.nb_bbox[1][BOXRIGHT]);
		return (int)Nodes.Push (node);
	}
	else
	{
		return 0x80000000 | CreateSubsector (set, bbox);
	}
}

int FNodeBuilder::CreateSubsector (uint32_t set, fixed_t bbox[4])
{
	int ssnum, count;

	bbox[BOXTOP] = bbox[BOXRIGHT] = INT_MIN;
	bbox[BOXBOTTOM] = bbox[BOXLEFT] = INT_MAX;

	D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Subsector from set %d\n", set));

	assert (set != UINT_MAX);

	// We cannot actually create the subsector now because the node building
	// process might split a seg in this subsector (because all partner segs
	// must use the same pair of vertices), adding a new seg that hasn't been
	// created yet. After all the nodes are built, then we can create the
	// actual subsectors using the CreateSubsectorsForReal function below.
	ssnum = (int)SubsectorSets.Push (set);

	count = 0;
	while (set != UINT_MAX)
	{
		AddSegToBBox (bbox, &Segs[set]);
		set = Segs[set].next;
		count++;
	}

	SegsStuffed += count;

	D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "bbox (%d,%d)-(%d,%d)\n", bbox[BOXLEFT]>>16, bbox[BOXBOTTOM]>>16, bbox[BOXRIGHT]>>16, bbox[BOXTOP]>>16));

	return ssnum;
}

void FNodeBuilder::CreateSubsectorsForReal ()
{
	subsector_t sub;
	unsigned int i;

	sub.sector = NULL;
	sub.polys = NULL;
	sub.BSP = NULL;
	sub.flags = 0;
	sub.render_sector = NULL;

	for (i = 0; i < SubsectorSets.Size(); ++i)
	{
		uint32_t set = SubsectorSets[i];
		uint32_t firstline = (uint32_t)SegList.Size();

		while (set != UINT_MAX)
		{
			USegPtr ptr;

			ptr.SegPtr = &Segs[set];
			SegList.Push (ptr);
			set = ptr.SegPtr->next;
		}
		sub.numlines = (uint32_t)(SegList.Size() - firstline);
		sub.firstline = (seg_t *)(size_t)firstline;

		// Sort segs by linedef for special effects
		qsort (&SegList[firstline], sub.numlines, sizeof(USegPtr), SortSegs);

		// Convert seg pointers into indices
		D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Output subsector %d:\n", Subsectors.Size()));
		for (unsigned int i = firstline; i < SegList.Size(); ++i)
		{
			D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "  Seg %5d%c%d(%5d,%5d)-%d(%5d,%5d)  [%08x,%08x]-[%08x,%08x]\n", SegList[i].SegPtr - &Segs[0],
				SegList[i].SegPtr->linedef == -1 ? '+' : ' ',
				SegList[i].SegPtr->v1,
				Vertices[SegList[i].SegPtr->v1].x>>16,
				Vertices[SegList[i].SegPtr->v1].y>>16,
				SegList[i].SegPtr->v2,
				Vertices[SegList[i].SegPtr->v2].x>>16,
				Vertices[SegList[i].SegPtr->v2].y>>16,
				Vertices[SegList[i].SegPtr->v1].x, Vertices[SegList[i].SegPtr->v1].y,
				Vertices[SegList[i].SegPtr->v2].x, Vertices[SegList[i].SegPtr->v2].y));
			SegList[i].SegNum = uint32_t(SegList[i].SegPtr - &Segs[0]);
		}
		Subsectors.Push (sub);
	}
}

int FNodeBuilder::SortSegs (const void *a, const void *b)
{
	const FPrivSeg *x = ((const USegPtr *)a)->SegPtr;
	const FPrivSeg *y = ((const USegPtr *)b)->SegPtr;

	// Segs are grouped into three categories in this order:
	//
	// 1. Segs with different front and back sectors (or no back at all).
	// 2. Segs with the same front and back sectors.
	// 3. Minisegs.
	//
	// Within the first two sets, segs are also sorted by linedef.
	//
	// Note that when GL subsectors are written, the segs will be reordered
	// so that they are in clockwise order, and extra minisegs will be added
	// as needed to close the subsector. But the first seg used will still be
	// the first seg chosen here.

	int xtype, ytype;

	if (x->linedef == -1)
	{
		xtype = 2;
	}
	else if (x->frontsector == x->backsector)
	{
		xtype = 1;
	}
	else
	{
		xtype = 0;
	}

	if (y->linedef == -1)
	{
		ytype = 2;
	}
	else if (y->frontsector == y->backsector)
	{
		ytype = 1;
	}
	else
	{
		ytype = 0;
	}

	if (xtype != ytype)
	{
		return xtype - ytype;
	}
	else if (xtype < 2)
	{
		return x->linedef - y->linedef;
	}
	else
	{
		return 0;
	}
}

// Given a set of segs, checks to make sure they all belong to a single
// sector. If so, false is returned, and they become a subsector. If not,
// a splitter is synthesized, and true is returned to continue processing
// down this branch of the tree.

bool FNodeBuilder::CheckSubsector (uint32_t set, node_t &node, uint32_t &splitseg)
{
	sector_t *sec;
	uint32_t seg;

	sec = NULL;
	seg = set;

	do
	{
		D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, " - seg %d%c(%d,%d)-(%d,%d) line %d front %d back %d\n", seg,
			Segs[seg].linedef == -1 ? '+' : ' ',
			Vertices[Segs[seg].v1].x>>16, Vertices[Segs[seg].v1].y>>16,
			Vertices[Segs[seg].v2].x>>16, Vertices[Segs[seg].v2].y>>16,
			Segs[seg].linedef,
			Segs[seg].frontsector == NULL ? -1 : Segs[seg].frontsector - sectors,
			Segs[seg].backsector == NULL ? -1 : Segs[seg].backsector - sectors));
		if (Segs[seg].linedef != -1 &&
			Segs[seg].frontsector != sec
			// Segs with the same front and back sectors are allowed to reside
			// in a subsector with segs from a different sector, because the
			// only effect they can have on the display is to place masked
			// mid textures in the scene. Since minisegs only mark subsector
			// boundaries, their sector information is unimportant.
			//
			// Update: Lines with the same front and back sectors *can* affect
			// the display if their subsector does not match their front sector.
			/*&& Segs[seg].frontsector != Segs[seg].backsector*/)
		{
			if (sec == NULL)
			{
				sec = Segs[seg].frontsector;
			}
			else
			{
				break;
			}
		}
		seg = Segs[seg].next;
	} while (seg != UINT_MAX);

	if (seg == UINT_MAX)
	{ // It's a valid non-GL subsector, and probably a valid GL subsector too.
		if (GLNodes)
		{
			return CheckSubsectorOverlappingSegs (set, node, splitseg);
		}
		return false;
	}

	D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Need to synthesize a splitter for set %d on seg %d\n", set, seg));
	splitseg = UINT_MAX;

	// This is a very simple and cheap "fix" for subsectors with segs
	// from multiple sectors, and it seems ZenNode does something
	// similar. It is the only technique I could find that makes the
	// "transparent water" in nb_bmtrk.wad work properly.
	return ShoveSegBehind (set, node, seg, UINT_MAX);
}

// When creating GL nodes, we need to check for segs with the same start and
// end vertices and split them into two subsectors.

bool FNodeBuilder::CheckSubsectorOverlappingSegs (uint32_t set, node_t &node, uint32_t &splitseg)
{
	int v1, v2;
	uint32_t seg1, seg2;

	for (seg1 = set; seg1 != UINT_MAX; seg1 = Segs[seg1].next)
	{
		if (Segs[seg1].linedef == -1)
		{ // Do not check minisegs.
			continue;
		}
		v1 = Segs[seg1].v1;
		v2 = Segs[seg1].v2;
		for (seg2 = Segs[seg1].next; seg2 != UINT_MAX; seg2 = Segs[seg2].next)
		{
			if (Segs[seg2].v1 == v1 && Segs[seg2].v2 == v2)
			{
				if (Segs[seg2].linedef == -1)
				{ // Do not put minisegs into a new subsector.
					std::swap (seg1, seg2);
				}
				D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Need to synthesize a splitter for set %d on seg %d (ov)\n", set, seg2));
				splitseg = UINT_MAX;

				return ShoveSegBehind (set, node, seg2, seg1);
			}
		}
	}
	// It really is a good subsector.
	return false;
}

// The seg is marked to indicate that it should be forced to the
// back of the splitter. Because these segs already form a convex
// set, all the other segs will be in front of the splitter. Since
// the splitter is formed from this seg, the back of the splitter
// will have a one-dimensional subsector. SplitSegs() will add one
// or two new minisegs to close it: If mate is UINT_MAX, then a
// new seg is created to replace this one on the front of the
// splitter. Otherwise, mate takes its place. In either case, the
// seg in front of the splitter is partnered with a new miniseg on
// the back so that the back will have two segs.

bool FNodeBuilder::ShoveSegBehind (uint32_t set, node_t &node, uint32_t seg, uint32_t mate)
{
	SetNodeFromSeg (node, &Segs[seg]);
	HackSeg = seg;
	HackMate = mate;
	if (!Segs[seg].planefront)
	{
		node.x += node.dx;
		node.y += node.dy;
		node.dx = -node.dx;
		node.dy = -node.dy;
	}
	return Heuristic (node, set, false) > 0;
}

// Splitters are chosen to coincide with segs in the given set. To reduce the
// number of segs that need to be considered as splitters, segs are grouped into
// according to the planes that they lie on. Because one seg on the plane is just
// as good as any other seg on the plane at defining a split, only one seg from
// each unique plane needs to be considered as a splitter. A result of 0 means
// this set is a convex region. A result of -1 means that there were possible
// splitters, but they all split segs we want to keep intact.
int FNodeBuilder::SelectSplitter (uint32_t set, node_t &node, uint32_t &splitseg, int step, bool nosplit)
{
	int stepleft;
	int bestvalue;
	uint32_t bestseg;
	uint32_t seg;
	bool nosplitters = false;

	bestvalue = 0;
	bestseg = UINT_MAX;

	seg = set;
	stepleft = 0;

	memset (&PlaneChecked[0], 0, PlaneChecked.Size());

	D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Processing set %d\n", set));

	while (seg != UINT_MAX)
	{
		FPrivSeg *pseg = &Segs[seg];

		if (--stepleft <= 0)
		{
			int l = pseg->planenum >> 3;
			int r = 1 << (pseg->planenum & 7);

			if (l < 0 || (PlaneChecked[l] & r) == 0)
			{
				if (l >= 0)
				{
					PlaneChecked[l] |= r;
				}

				stepleft = step;
				SetNodeFromSeg (node, pseg);

				int value = Heuristic (node, set, nosplit);

				D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Seg %5d, ld %d (%5d,%5d)-(%5d,%5d) scores %d\n", seg, Segs[seg].linedef, node.x>>16, node.y>>16,
					(node.x+node.dx)>>16, (node.y+node.dy)>>16, value));

				if (value > bestvalue)
				{
					bestvalue = value;
					bestseg = seg;
				}
				else if (value < 0)
				{
					nosplitters = true;
				}
			}
		}

		seg = pseg->next;
	}

	if (bestseg == UINT_MAX)
	{ // No lines split any others into two sets, so this is a convex region.
	D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "set %d, step %d, nosplit %d has no good splitter (%d)\n", set, step, nosplit, nosplitters));
		return nosplitters ? -1 : 0;
	}

	D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "split seg %u in set %d, score %d, step %d, nosplit %d\n", bestseg, set, bestvalue, step, nosplit));

	splitseg = bestseg;
	SetNodeFromSeg (node, &Segs[bestseg]);
	return 1;
}

// Given a splitter (node), returns a score based on how "good" the resulting
// split in a set of segs is. Higher scores are better. -1 means this splitter
// splits something it shouldn't and will only be returned if honorNoSplit is
// true. A score of 0 means that the splitter does not split any of the segs
// in the set.

int FNodeBuilder::Heuristic (node_t &node, uint32_t set, bool honorNoSplit)
{
	// Set the initial score above 0 so that near vertex anti-weighting is less likely to produce a negative score.
	int score = 1000000;
	int segsInSet = 0;
	int counts[2] = { 0, 0 };
	int realSegs[2] = { 0, 0 };
	int specialSegs[2] = { 0, 0 };
	uint32_t i = set;
	int sidev[2];
	int side;
	bool splitter = false;
	unsigned int max, m2, p, q;
	double frac;

	Touched.Clear ();
	Colinear.Clear ();

	while (i != UINT_MAX)
	{
		const FPrivSeg *test = &Segs[i];

		if (HackSeg == i)
		{
			side = 1;
		}
		else
		{
			side = ClassifyLine (node, &Vertices[test->v1], &Vertices[test->v2], sidev);
		}
		switch (side)
		{
		case 0:	// Seg is on only one side of the partition
		case 1:
			// If we don't split this line, but it abuts the splitter, also reject it.
			// The "right" thing to do in this case is to only reject it if there is
			// another nosplit seg from the same sector at this vertex. Note that a line
			// that lies exactly on top of the splitter is okay.
			if (test->loopnum && honorNoSplit && (sidev[0] == 0 || sidev[1] == 0))
			{
				if ((sidev[0] | sidev[1]) != 0)
				{
					max = Touched.Size();
					for (p = 0; p < max; ++p)
					{
						if (Touched[p] == test->loopnum)
						{
							break;
						}
					}
					if (p == max)
					{
						Touched.Push (test->loopnum);
					}
				}
				else
				{
					max = Colinear.Size();
					for (p = 0; p < max; ++p)
					{
						if (Colinear[p] == test->loopnum)
						{
							break;
						}
					}
					if (p == max)
					{
						Colinear.Push (test->loopnum);
					}
				}
			}

			counts[side]++;
			if (test->linedef != -1)
			{
				realSegs[side]++;
				if (test->frontsector == test->backsector)
				{
					specialSegs[side]++;
				}
				// Add some weight to the score for unsplit lines
				score += SplitCost;	
			}
			else
			{
				// Minisegs don't count quite as much for nosplitting
				score += SplitCost / 4;
			}
			break;

		default:	// Seg is cut by the partition
			// If we are not allowed to split this seg, reject this splitter
			if (test->loopnum)
			{
				if (honorNoSplit)
				{
					D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Splits seg %d\n", i));
					return -1;
				}
				else
				{
					splitter = true;
				}
			}

			// Splitters that are too close to a vertex are bad.
			frac = InterceptVector (node, *test);
			if (frac < 0.001 || frac > 0.999)
			{
				FPrivVert *v1 = &Vertices[test->v1];
				FPrivVert *v2 = &Vertices[test->v2];
				double x = v1->x, y = v1->y;
				x += frac * (v2->x - x);
				y += frac * (v2->y - y);
				if (fabs(x - v1->x) < VERTEX_EPSILON+1 && fabs(y - v1->y) < VERTEX_EPSILON+1)
				{
					D(Printf("Splitter will produce same start vertex as seg %d\n", i));
					return -1;
				}
				if (fabs(x - v2->x) < VERTEX_EPSILON+1 && fabs(y - v2->y) < VERTEX_EPSILON+1)
				{
					D(Printf("Splitter will produce same end vertex as seg %d\n", i));
					return -1;
				}
				if (frac > 0.999)
				{
					frac = 1 - frac;
				}
				int penalty = int(1 / frac);
				score = MAX(score - penalty, 1);
				D(Printf ("Penalized splitter by %d for being near endpt of seg %d (%f).\n", penalty, i, frac));
			}

			counts[0]++;
			counts[1]++;
			if (test->linedef != -1)
			{
				realSegs[0]++;
				realSegs[1]++;
				if (test->frontsector == test->backsector)
				{
					specialSegs[0]++;
					specialSegs[1]++;
				}
			}
			break;
		}

		segsInSet++;
		i = test->next;
	}

	// If this line is outside all the others, return a special score
	if (counts[0] == 0 || counts[1] == 0)
	{
		return 0;
	}

	// A splitter must have at least one real seg on each side.
	// Otherwise, a subsector could be left without any way to easily
	// determine which sector it lies inside.
	if (realSegs[0] == 0 || realSegs[1] == 0)
	{
		D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Leaves a side with only mini segs\n"));
		return -1;
	}

	// Try to avoid splits that leave only "special" segs, so that the generated
	// subsectors have a better chance of choosing the correct sector. This situation
	// is not neccesarily bad, just undesirable.
	if (honorNoSplit && (specialSegs[0] == realSegs[0] || specialSegs[1] == realSegs[1]))
	{
		D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Leaves a side with only special segs\n"));
		return -1;
	}

	// If this splitter intersects any vertices of segs that should not be split,
	// check if it is also colinear with another seg from the same sector. If it
	// is, the splitter is okay. If not, it should be rejected. Why? Assuming that
	// polyobject containers are convex (which they should be), a splitter that
	// is colinear with one of the sector's segs and crosses the vertex of another
	// seg of that sector must be crossing the container's corner and does not
	// actually split the container.

	max = Touched.Size ();
	m2 = Colinear.Size ();

	// If honorNoSplit is false, then both these lists will be empty.

	// If the splitter touches some vertices without being colinear to any, we
	// can skip further checks and reject this right away.
	if (m2 == 0 && max > 0)
	{
		return -1;
	}

	for (p = 0; p < max; ++p)
	{
		int look = Touched[p];
		for (q = 0; q < m2; ++q)
		{
			if (look == Colinear[q])
			{
				break;
			}
		}
		if (q == m2)
		{ // Not a good one
			return -1;
		}
	}

	// Doom maps are primarily axis-aligned lines, so it's usually a good
	// idea to prefer axis-aligned splitters over diagonal ones. Doom originally
	// had special-casing for orthogonal lines, so they performed better. ZDoom
	// does not care about the line's direction, so this is merely a choice to
	// try and improve the final tree.

	if ((node.dx == 0) || (node.dy == 0))
	{
		// If we have to split a seg we would prefer to keep unsplit, give
		// extra precedence to orthogonal lines so that the polyobjects
		// outside the entrance to MAP06 in Hexen MAP02 display properly.
		if (splitter)
		{
			score += segsInSet*8;
		}
		else
		{
			score += segsInSet/AAPreference;
		}
	}

	score += (counts[0] + counts[1]) - abs(counts[0] - counts[1]);

	return score;
}

void FNodeBuilder::SplitSegs (uint32_t set, node_t &node, uint32_t splitseg, uint32_t &outset0, uint32_t &outset1, unsigned int &count0, unsigned int &count1)
{
	unsigned int _count0 = 0;
	unsigned int _count1 = 0;
	outset0 = UINT_MAX;
	outset1 = UINT_MAX;

	Events.DeleteAll ();
	SplitSharers.Clear ();

	while (set != UINT_MAX)
	{
		bool hack;
		FPrivSeg *seg = &Segs[set];
		int next = seg->next;

		int sidev[2], side;

		if (HackSeg == set)
		{
			HackSeg = UINT_MAX;
			side = 1;
			sidev[0] = sidev[1] = 0;
			hack = true;
		}
		else
		{
			side = ClassifyLine (node, &Vertices[seg->v1], &Vertices[seg->v2], sidev);
			hack = false;
		}

		switch (side)
		{
		case 0: // seg is entirely in front
			seg->next = outset0;
			outset0 = set;
			_count0++;
			break;

		case 1: // seg is entirely in back
			seg->next = outset1;
			outset1 = set;
			_count1++;
			break;

		default: // seg needs to be split
			double frac;
			FPrivVert newvert;
			unsigned int vertnum;
			int seg2;

			frac = InterceptVector (node, *seg);
			newvert.x = Vertices[seg->v1].x;
			newvert.y = Vertices[seg->v1].y;
			newvert.x += fixed_t(frac * (double(Vertices[seg->v2].x) - newvert.x));
			newvert.y += fixed_t(frac * (double(Vertices[seg->v2].y) - newvert.y));
			vertnum = VertexMap->SelectVertexClose (newvert);

			if (vertnum != (unsigned int)seg->v1 && vertnum != (unsigned int)seg->v2)
			{
				seg2 = SplitSeg(set, vertnum, sidev[0]);

				Segs[seg2].next = outset0;
				outset0 = seg2;
				Segs[set].next = outset1;
				outset1 = set;
				_count0++;
				_count1++;

				// Also split the seg on the back side
				if (Segs[set].partner != UINT_MAX)
				{
					int partner1 = Segs[set].partner;
					int partner2 = SplitSeg(partner1, vertnum, sidev[1]);
					// The newly created seg stays in the same set as the
					// back seg because it has not been considered for splitting
					// yet. If it had been, then the front seg would have already
					// been split, and we would not be in this default case.
					// Moreover, the back seg may not even be in the set being
					// split, so we must not move its pieces into the out sets.
					Segs[partner1].next = partner2;
					Segs[partner2].partner = seg2;
					Segs[seg2].partner = partner2;
				}

				if (GLNodes)
				{
					AddIntersection(node, vertnum);
				}
			}
			else
			{
				// all that matters here is to prevent a crash so we must make sure that we do not end up with all segs being sorted to the same side - even if this may not be correct.
				// But if we do not do that this code would not be able to move on. Just discarding the seg is also not an option because it won't guarantee that we achieve an actual split.
				if (_count0 == 0)
				{
					side = 0;
					seg->next = outset0;
					outset0 = set;
					_count0++;
				}
				else
				{
					side = 1;
					seg->next = outset1;
					outset1 = set;
					_count1++;
				}
			}
			break;
		}
		if (side >= 0 && GLNodes)
		{
			if (sidev[0] == 0)
			{
				double dist1 = AddIntersection (node, seg->v1);
				if (sidev[1] == 0)
				{
					double dist2 = AddIntersection (node, seg->v2);
					FSplitSharer share = { dist1, set, dist2 > dist1 };
					SplitSharers.Push (share);
				}
			}
			else if (sidev[1] == 0)
			{
				AddIntersection (node, seg->v2);
			}
		}
		if (hack && GLNodes)
		{
			uint32_t newback, newfront;

			newback = AddMiniseg (seg->v2, seg->v1, UINT_MAX, set, splitseg);
			if (HackMate == UINT_MAX)
			{
				newfront = AddMiniseg (Segs[set].v1, Segs[set].v2, newback, set, splitseg);
				Segs[newfront].next = outset0;
				outset0 = newfront;
			}
			else
			{
				newfront = HackMate;
				Segs[newfront].partner = newback;
				Segs[newback].partner = newfront;
			}
			Segs[newback].frontsector = Segs[newback].backsector =
				Segs[newfront].frontsector = Segs[newfront].backsector =
				Segs[set].frontsector;

			Segs[newback].next = outset1;
			outset1 = newback;
		}
		set = next;
	}
	FixSplitSharers (node);
	if (GLNodes)
	{
		AddMinisegs (node, splitseg, outset0, outset1);
	}
	count0 = _count0;
	count1 = _count1;
}

void FNodeBuilder::SetNodeFromSeg (node_t &node, const FPrivSeg *pseg) const
{
	if (pseg->planenum >= 0)
	{
		FSimpleLine *pline = &Planes[pseg->planenum];
		node.x = pline->x;
		node.y = pline->y;
		node.dx = pline->dx;
		node.dy = pline->dy;
	}
	else
	{
		node.x = Vertices[pseg->v1].x;
		node.y = Vertices[pseg->v1].y;
		node.dx = Vertices[pseg->v2].x - node.x;
		node.dy = Vertices[pseg->v2].y - node.y;
	}
}

uint32_t FNodeBuilder::SplitSeg (uint32_t segnum, int splitvert, int v1InFront)
{
	double dx, dy;
	FPrivSeg newseg;
	int newnum = (int)Segs.Size();

	newseg = Segs[segnum];
	dx = double(Vertices[splitvert].x - Vertices[newseg.v1].x);
	dy = double(Vertices[splitvert].y - Vertices[newseg.v1].y);
	if (v1InFront > 0)
	{
		newseg.v1 = splitvert;
		Segs[segnum].v2 = splitvert;

		RemoveSegFromVert2 (segnum, newseg.v2);

		newseg.nextforvert = Vertices[splitvert].segs;
		Vertices[splitvert].segs = newnum;

		newseg.nextforvert2 = Vertices[newseg.v2].segs2;
		Vertices[newseg.v2].segs2 = newnum;

		Segs[segnum].nextforvert2 = Vertices[splitvert].segs2;
		Vertices[splitvert].segs2 = segnum;
	}
	else
	{
		Segs[segnum].v1 = splitvert;
		newseg.v2 = splitvert;

		RemoveSegFromVert1 (segnum, newseg.v1);

		newseg.nextforvert = Vertices[newseg.v1].segs;
		Vertices[newseg.v1].segs = newnum;

		newseg.nextforvert2 = Vertices[splitvert].segs2;
		Vertices[splitvert].segs2 = newnum;

		Segs[segnum].nextforvert = Vertices[splitvert].segs;
		Vertices[splitvert].segs = segnum;
	}

	Segs.Push (newseg);

	D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Split seg %d to get seg %d\n", segnum, newnum));

	return newnum;
}

void FNodeBuilder::RemoveSegFromVert1 (uint32_t segnum, int vertnum)
{
	FPrivVert *v = &Vertices[vertnum];

	if (v->segs == segnum)
	{
		v->segs = Segs[segnum].nextforvert;
	}
	else
	{
		uint32_t prev, curr;
		prev = 0;
		curr = v->segs;
		while (curr != UINT_MAX && curr != segnum)
		{
			prev = curr;
			curr = Segs[curr].nextforvert;
		}
		if (curr == segnum)
		{
			Segs[prev].nextforvert = Segs[curr].nextforvert;
		}
	}
}

void FNodeBuilder::RemoveSegFromVert2 (uint32_t segnum, int vertnum)
{
	FPrivVert *v = &Vertices[vertnum];

	if (v->segs2 == segnum)
	{
		v->segs2 = Segs[segnum].nextforvert2;
	}
	else
	{
		uint32_t prev, curr;
		prev = 0;
		curr = v->segs2;
		while (curr != UINT_MAX && curr != segnum)
		{
			prev = curr;
			curr = Segs[curr].nextforvert2;
		}
		if (curr == segnum)
		{
			Segs[prev].nextforvert2 = Segs[curr].nextforvert2;
		}
	}
}

double FNodeBuilder::InterceptVector (const node_t &splitter, const FPrivSeg &seg)
{
	double v2x = (double)Vertices[seg.v1].x;
	double v2y = (double)Vertices[seg.v1].y;
	double v2dx = (double)Vertices[seg.v2].x - v2x;
	double v2dy = (double)Vertices[seg.v2].y - v2y;
	double v1dx = (double)splitter.dx;
	double v1dy = (double)splitter.dy;

	double den = v1dy*v2dx - v1dx*v2dy;

	if (den == 0.0)
		return 0;		// parallel

	double v1x = (double)splitter.x;
	double v1y = (double)splitter.y;

	double num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
	double frac = num / den;

	return frac;
}

void FNodeBuilder::PrintSet (int l, uint32_t set)
{
	Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "set %d:\n", l);
	for (; set != UINT_MAX; set = Segs[set].next)
	{
		Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "\t%u(%d)%c%d(%d,%d)-%d(%d,%d)\n", set, Segs[set].frontsector->sectornum,
			Segs[set].linedef == -1 ? '+' : ':',
			Segs[set].v1,
			Vertices[Segs[set].v1].x>>16, Vertices[Segs[set].v1].y>>16,
			Segs[set].v2,
			Vertices[Segs[set].v2].x>>16, Vertices[Segs[set].v2].y>>16);
	}
	Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "*\n");
}

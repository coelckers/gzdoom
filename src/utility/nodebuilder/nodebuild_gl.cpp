/*
** nodebuild_gl.cpp
**
** Extra functions for the node builder to create minisegs.
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

#include "doomtype.h"
#include "nodebuild.h"

static inline void Warn (const char *format, ...)
{
}

static const angle_t ANGLE_EPSILON = 5000;

#if 0
#define D(x) x
#else
#define D(x) do{}while(0)
#endif

double FNodeBuilder::AddIntersection (const node_t &node, int vertex)
{
	static const FEventInfo defaultInfo =
	{
		-1, UINT_MAX
	};

	// Calculate signed distance of intersection vertex from start of splitter.
	// Only ordering is important, so we don't need a sqrt.
	FPrivVert *v = &Vertices[vertex];
	double dist = (double(v->x) - node.x)*(node.dx) + (double(v->y) - node.y)*(node.dy);

	FEvent *event = Events.FindEvent (dist);
	if (event == NULL)
	{
		event = Events.GetNewNode ();
		event->Distance = dist;
		event->Info = defaultInfo;
		event->Info.Vertex = vertex;
		Events.Insert (event);
	}
	return dist;
}

// If there are any segs on the splitter that span more than two events, they
// must be split. Alien Vendetta is one example wad that is quite bad about
// having overlapping lines. If we skip this step, these segs will still be
// split later, but minisegs will erroneously be added for them, and partner
// seg information will be messed up in the generated tree.
void FNodeBuilder::FixSplitSharers (const node_t &node)
{
	D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "events:\n"));
	D(Events.PrintTree());
	for (unsigned int i = 0; i < SplitSharers.Size(); ++i)
	{
		uint32_t seg = SplitSharers[i].Seg;
		int v2 = Segs[seg].v2;
		FEvent *event = Events.FindEvent (SplitSharers[i].Distance);
		FEvent *next;

		if (event == NULL)
		{ // Should not happen
			continue;
		}

		// Use the CRT's printf so the formatting matches ZDBSP's
		D(char buff[200]);
		D(sprintf(buff, "Considering events on seg %d(%d[%d,%d]->%d[%d,%d]) [%g:%g]\n", seg,
			Segs[seg].v1,
			Vertices[Segs[seg].v1].x>>16,
			Vertices[Segs[seg].v1].y>>16,
			Segs[seg].v2,
			Vertices[Segs[seg].v2].x>>16,
			Vertices[Segs[seg].v2].y>>16,
			SplitSharers[i].Distance, event->Distance));
		D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "%s", buff));

		if (SplitSharers[i].Forward)
		{
			event = Events.GetSuccessor (event);
			if (event == NULL)
			{
				continue;
			}
			next = Events.GetSuccessor (event);
		}
		else
		{
			event = Events.GetPredecessor (event);
			if (event == NULL)
			{
				continue;
			}
			next = Events.GetPredecessor (event);
		}

		while (event != NULL && next != NULL && event->Info.Vertex != v2)
		{
			D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Forced split of seg %d(%d->%d) at %d(%d,%d)\n", seg,
				Segs[seg].v1, Segs[seg].v2,
				event->Info.Vertex,
				Vertices[event->Info.Vertex].x>>16,
				Vertices[event->Info.Vertex].y>>16));

			uint32_t newseg = SplitSeg (seg, event->Info.Vertex, 1);

			Segs[newseg].next = Segs[seg].next;
			Segs[seg].next = newseg;

			uint32_t partner = Segs[seg].partner;
			if (partner != UINT_MAX)
			{
				int endpartner = SplitSeg (partner, event->Info.Vertex, 1);

				Segs[endpartner].next = Segs[partner].next;
				Segs[partner].next = endpartner;

				Segs[seg].partner = endpartner;
				Segs[partner].partner = newseg;
			}

			seg = newseg;
			if (SplitSharers[i].Forward)
			{
				event = next;
				next = Events.GetSuccessor (next);
			}
			else
			{
				event = next;
				next = Events.GetPredecessor (next);
			}
		}
	}
}

void FNodeBuilder::AddMinisegs (const node_t &node, uint32_t splitseg, uint32_t &fset, uint32_t &bset)
{
	FEvent *event = Events.GetMinimum (), *prev = NULL;

	while (event != NULL)
	{
		if (prev != NULL)
		{
			uint32_t fseg1, bseg1, fseg2, bseg2;
			uint32_t fnseg, bnseg;

			// Minisegs should only be added when they can create valid loops on both the front and
			// back of the splitter. This means some subsectors could be unclosed if their sectors
			// are unclosed, but at least we won't be needlessly creating subsectors in void space.
			// Unclosed subsectors can be closed trivially once the BSP tree is complete.

			if ((fseg1 = CheckLoopStart (node.dx, node.dy, prev->Info.Vertex, event->Info.Vertex)) != UINT_MAX &&
				(bseg1 = CheckLoopStart (-node.dx, -node.dy, event->Info.Vertex, prev->Info.Vertex)) != UINT_MAX &&
				(fseg2 = CheckLoopEnd (node.dx, node.dy, event->Info.Vertex)) != UINT_MAX &&
				(bseg2 = CheckLoopEnd (-node.dx, -node.dy, prev->Info.Vertex)) != UINT_MAX)
			{
				// Add miniseg on the front side
				fnseg = AddMiniseg (prev->Info.Vertex, event->Info.Vertex, UINT_MAX, fseg1, splitseg);
				Segs[fnseg].next = fset;
				fset = fnseg;

				// Add miniseg on the back side
				bnseg = AddMiniseg (event->Info.Vertex, prev->Info.Vertex, fnseg, bseg1, splitseg);
				Segs[bnseg].next = bset;
				bset = bnseg;

				sector_t *fsector, *bsector;

				fsector = Segs[fseg1].frontsector;
				bsector = Segs[bseg1].frontsector;

				Segs[fnseg].frontsector = fsector;
				Segs[fnseg].backsector = bsector;
				Segs[bnseg].frontsector = bsector;
				Segs[bnseg].backsector = fsector;

				// Only print the warning if this might be bad.
				if (fsector != bsector &&
					fsector != Segs[fseg1].backsector &&
					bsector != Segs[bseg1].backsector)
				{
					Warn ("Sectors %d at (%d,%d) and %d at (%d,%d) don't match.\n",
						Segs[fseg1].frontsector,
						Vertices[prev->Info.Vertex].x>>FRACBITS, Vertices[prev->Info.Vertex].y>>FRACBITS,
						Segs[bseg1].frontsector,
						Vertices[event->Info.Vertex].x>>FRACBITS, Vertices[event->Info.Vertex].y>>FRACBITS
						);
				}

				D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "**Minisegs** %d/%d added %d(%d,%d)->%d(%d,%d)\n", fnseg, bnseg,
					prev->Info.Vertex,
					Vertices[prev->Info.Vertex].x>>16, Vertices[prev->Info.Vertex].y>>16,
					event->Info.Vertex,
					Vertices[event->Info.Vertex].x>>16, Vertices[event->Info.Vertex].y>>16));
			}
		}
		prev = event;
		event = Events.GetSuccessor (event);
	}
}

uint32_t FNodeBuilder::AddMiniseg (int v1, int v2, uint32_t partner, uint32_t seg1, uint32_t splitseg)
{
	uint32_t nseg;
	FPrivSeg *seg = &Segs[seg1];
	FPrivSeg newseg;

	newseg.sidedef = NO_SIDE;
	newseg.linedef = -1;
	newseg.loopnum = 0;
	newseg.next = UINT_MAX;
	newseg.planefront = true;
	newseg.hashnext = NULL;
	newseg.storedseg = UINT_MAX;
	newseg.frontsector = NULL;
	newseg.backsector = NULL;

	if (splitseg != UINT_MAX)
	{
		newseg.planenum = Segs[splitseg].planenum;
	}
	else
	{
		newseg.planenum = -1;
	}

	newseg.v1 = v1;
	newseg.v2 = v2;
	newseg.nextforvert = Vertices[v1].segs;
	newseg.nextforvert2 = Vertices[v2].segs2;
	newseg.next = seg->next;
	if (partner != UINT_MAX)
	{
		newseg.partner = partner;
	}
	else
	{
		newseg.partner = UINT_MAX;
	}
	nseg = Segs.Push (newseg);
	if (newseg.partner != UINT_MAX)
	{
		Segs[partner].partner = nseg;
	}
	Vertices[v1].segs = nseg;
	Vertices[v2].segs2 = nseg;
	//Printf ("Between %d and %d::::\n", seg1, seg2);
	return nseg;
}

uint32_t FNodeBuilder::CheckLoopStart (fixed_t dx, fixed_t dy, int vertex, int vertex2)
{
	FPrivVert *v = &Vertices[vertex];
	angle_t splitAngle = PointToAngle (dx, dy);
	uint32_t segnum;
	angle_t bestang;
	uint32_t bestseg;

	// Find the seg ending at this vertex that forms the smallest angle
	// to the splitter.
	segnum = v->segs2;
	bestang = ANGLE_MAX;
	bestseg = UINT_MAX;
	while (segnum != UINT_MAX)
	{
		FPrivSeg *seg = &Segs[segnum];
		angle_t segAngle = PointToAngle (Vertices[seg->v1].x - v->x, Vertices[seg->v1].y - v->y);
		angle_t diff = splitAngle - segAngle;

		if (diff < ANGLE_EPSILON &&
			PointOnSide (Vertices[seg->v1].x, Vertices[seg->v1].y, v->x, v->y, dx, dy) == 0)
		{
			// If a seg lies right on the splitter, don't count it
		}
		else
		{
			if (diff <= bestang)
			{
				bestang = diff;
				bestseg = segnum;
			}
		}
		segnum = seg->nextforvert2;
	}
	if (bestseg == UINT_MAX)
	{
		return UINT_MAX;
	}
	// Now make sure there are no segs starting at this vertex that form
	// an even smaller angle to the splitter.
	segnum = v->segs;
	while (segnum != UINT_MAX)
	{
		FPrivSeg *seg = &Segs[segnum];
		if (seg->v2 == vertex2)
		{
			return UINT_MAX;
		}
		angle_t segAngle = PointToAngle (Vertices[seg->v2].x - v->x, Vertices[seg->v2].y - v->y);
		angle_t diff = splitAngle - segAngle;
		if (diff < bestang && seg->partner != bestseg)
		{
			return UINT_MAX;
		}
		segnum = seg->nextforvert;
	}
	return bestseg;
}

uint32_t FNodeBuilder::CheckLoopEnd (fixed_t dx, fixed_t dy, int vertex)
{
	FPrivVert *v = &Vertices[vertex];
	angle_t splitAngle = PointToAngle (dx, dy) + ANGLE_180;
	uint32_t segnum;
	angle_t bestang;
	uint32_t bestseg;

	// Find the seg starting at this vertex that forms the smallest angle
	// to the splitter.
	segnum = v->segs;
	bestang = ANGLE_MAX;
	bestseg = UINT_MAX;
	while (segnum != UINT_MAX)
	{
		FPrivSeg *seg = &Segs[segnum];
		angle_t segAngle = PointToAngle (Vertices[seg->v2].x - v->x, Vertices[seg->v2].y - v->y);
		angle_t diff = segAngle - splitAngle;

		if (diff < ANGLE_EPSILON &&
			PointOnSide (Vertices[seg->v1].x, Vertices[seg->v1].y, v->x, v->y, dx, dy) == 0)
		{
			// If a seg lies right on the splitter, don't count it
		}
		else
		{
			if (diff <= bestang)
			{
				bestang = diff;
				bestseg = segnum;
			}
		}
		segnum = seg->nextforvert;
	}
	if (bestseg == UINT_MAX)
	{
		return UINT_MAX;
	}
	// Now make sure there are no segs ending at this vertex that form
	// an even smaller angle to the splitter.
	segnum = v->segs2;
	while (segnum != UINT_MAX)
	{
		FPrivSeg *seg = &Segs[segnum];
		angle_t segAngle = PointToAngle (Vertices[seg->v1].x - v->x, Vertices[seg->v1].y - v->y);
		angle_t diff = segAngle - splitAngle;
		if (diff < bestang && seg->partner != bestseg)
		{
			return UINT_MAX;
		}
		segnum = seg->nextforvert2;
	}
	return bestseg;
}

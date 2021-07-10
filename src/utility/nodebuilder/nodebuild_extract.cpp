/*
** nodebuild_extract.cpp
**
** Converts the nodes, segs, and subsectors from the node builder's
** internal format to the format used by the rest of the game.
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

#include <string.h>
#include <float.h>

#include "nodebuild.h"
#include "g_levellocals.h"

#if 0
#define D(x) x
#define DD 1
#else
#define D(x) do{}while(0)
#undef DD
#endif

void FNodeBuilder::Extract (FLevelLocals &theLevel)
{
	int i;

	auto &outVerts = theLevel.vertexes; 
	int vertCount = Vertices.Size ();
	outVerts.Alloc(vertCount);

	for (i = 0; i < vertCount; ++i)
	{
		outVerts[i].set(Vertices[i].x, Vertices[i].y);
	}

	auto &outSubs = theLevel.subsectors;
	auto subCount = Subsectors.Size();
	outSubs.Alloc(subCount);
	memset(&outSubs[0], 0, subCount * sizeof(subsector_t));

	auto &outNodes = theLevel.nodes;
	auto nodeCount = Nodes.Size ();
	outNodes.Alloc(nodeCount);

	memcpy (&outNodes[0], &Nodes[0], nodeCount*sizeof(node_t));
	for (unsigned i = 0; i < nodeCount; ++i)
	{
		D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Node %d:  Splitter[%08x,%08x] [%08x,%08x]\n", i,
			outNodes[i].x, outNodes[i].y, outNodes[i].dx, outNodes[i].dy));
		// Go backwards because on 64-bit systems, both of the intchildren are
		// inside the first in-game child.
		for (int j = 1; j >= 0; --j)
		{
			if (outNodes[i].intchildren[j] & 0x80000000)
			{
				D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "  subsector %d\n", outNodes[i].intchildren[j] & 0x7FFFFFFF));
				outNodes[i].children[j] = (uint8_t *)(&outSubs[(outNodes[i].intchildren[j] & 0x7fffffff)]) + 1;
			}
			else
			{
				D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "  node %d\n", outNodes[i].intchildren[j]));
				outNodes[i].children[j] = &outNodes[outNodes[i].intchildren[j]];
			}
		}
		for (int j = 0; j < 2; ++j)
		{
			for (int k = 0; k < 4; ++k)
			{
				outNodes[i].bbox[j][k] = FIXED2FLOAT(outNodes[i].nb_bbox[j][k]);
			}
		}
	}

	auto &outSegs = theLevel.segs;
	if (GLNodes)
	{
		TArray<glseg_t> segs (Segs.Size()*5/4);

		for (unsigned i = 0; i < subCount; ++i)
		{
			uint32_t numsegs = CloseSubsector (segs, i, &outVerts[0]);
			outSubs[i].numlines = numsegs;
			outSubs[i].firstline = (seg_t *)(size_t)(segs.Size() - numsegs);
		}

		auto segCount = segs.Size ();
		outSegs.Alloc(segCount);

		for (unsigned i = 0; i < segCount; ++i)
		{
			outSegs[i] = *(seg_t *)&segs[i];

			if (segs[i].Partner != UINT_MAX)
			{
				const uint32_t storedseg = Segs[segs[i].Partner].storedseg;
				outSegs[i].PartnerSeg = UINT_MAX == storedseg ? nullptr : &outSegs[storedseg];
			}
			else
			{
				outSegs[i].PartnerSeg = nullptr;
			}
		}
	}
	else
	{
		memcpy (&outSubs[0], &Subsectors[0], subCount*sizeof(subsector_t));
		auto segCount = Segs.Size ();
		outSegs.Alloc(segCount);
		for (unsigned i = 0; i < segCount; ++i)
		{
			const FPrivSeg *org = &Segs[SegList[i].SegNum];
			seg_t *out = &outSegs[i];

			D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Seg %d: v1(%d) -> v2(%d)\n", i, org->v1, org->v2));

			out->v1 = &outVerts[org->v1];
			out->v2 = &outVerts[org->v2];
			out->backsector = org->backsector;
			out->frontsector = org->frontsector;
			out->linedef = Level.Lines + org->linedef;
			out->sidedef = Level.Sides + org->sidedef;
			out->PartnerSeg = nullptr;
		}
	}
	for (unsigned i = 0; i < subCount; ++i)
	{
		outSubs[i].firstline = &outSegs[(size_t)outSubs[i].firstline];
	}

	D(Printf("%i segs, %i nodes, %i subsectors\n", segCount, nodeCount, subCount));

	for (i = 0; i < Level.NumLines; ++i)
	{
		Level.Lines[i].v1 = &outVerts[(size_t)Level.Lines[i].v1];
		Level.Lines[i].v2 = &outVerts[(size_t)Level.Lines[i].v2];
	}
}

void FNodeBuilder::ExtractMini (FMiniBSP *bsp)
{
	unsigned int i;

	bsp->bDirty = false;
	bsp->Verts.Resize(Vertices.Size());
	for (i = 0; i < Vertices.Size(); ++i)
	{
		bsp->Verts[i].set(Vertices[i].x, Vertices[i].y);
	}

	bsp->Subsectors.Resize(Subsectors.Size());
	memset(&bsp->Subsectors[0], 0, Subsectors.Size() * sizeof(subsector_t));

	bsp->Nodes.Resize(Nodes.Size());
	memcpy(&bsp->Nodes[0], &Nodes[0], Nodes.Size()*sizeof(node_t));
	for (i = 0; i < Nodes.Size(); ++i)
	{
		D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Node %d:\n", i));
		// Go backwards because on 64-bit systems, both of the intchildren are
		// inside the first in-game child.
		for (int j = 1; j >= 0; --j)
		{
			if (bsp->Nodes[i].intchildren[j] & 0x80000000)
			{
				D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "  subsector %d\n", bsp->Nodes[i].intchildren[j] & 0x7FFFFFFF));
				bsp->Nodes[i].children[j] = (uint8_t *)&bsp->Subsectors[bsp->Nodes[i].intchildren[j] & 0x7fffffff] + 1;
			}
			else
			{
				D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "  node %d\n", bsp->Nodes[i].intchildren[j]));
				bsp->Nodes[i].children[j] = &bsp->Nodes[bsp->Nodes[i].intchildren[j]];
			}
		}
		for (int j = 0; j < 2; ++j)
		{
			for (int k = 0; k < 4; ++k)
			{
				bsp->Nodes[i].bbox[j][k] = FIXED2FLOAT(bsp->Nodes[i].nb_bbox[j][k]);
			}
		}
	}

	if (GLNodes)
	{
		TArray<glseg_t> glsegs;
		for (i = 0; i < Subsectors.Size(); ++i)
		{
			uint32_t numsegs = CloseSubsector (glsegs, i, &bsp->Verts[0]);
			bsp->Subsectors[i].numlines = numsegs;
			bsp->Subsectors[i].firstline = &bsp->Segs[bsp->Segs.Size() - numsegs];
		}
		bsp->Segs.Resize(glsegs.Size());
		for (i = 0; i < glsegs.Size(); ++i)
		{
			bsp->Segs[i] = *(seg_t *)&glsegs[i];
		}
	}
	else
	{
		memcpy(&bsp->Subsectors[0], &Subsectors[0], Subsectors.Size()*sizeof(subsector_t));
		bsp->Segs.Resize(Segs.Size());
		for (i = 0; i < Segs.Size(); ++i)
		{
			const FPrivSeg *org = &Segs[SegList[i].SegNum];
			seg_t *out = &bsp->Segs[i];

			D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Seg %d: v1(%d) -> v2(%d)\n", i, org->v1, org->v2));

			out->v1 = &bsp->Verts[org->v1];
			out->v2 = &bsp->Verts[org->v2];
			out->backsector = org->backsector;
			out->frontsector = org->frontsector;
			if (org->sidedef != int(NO_SIDE))
			{
				out->linedef = Level.Lines + org->linedef;
				out->sidedef = Level.Sides + org->sidedef;
			}
			else	// part of a miniseg
			{
				out->linedef = NULL;
				out->sidedef = NULL;
			}
		}
		for (i = 0; i < bsp->Subsectors.Size(); ++i)
		{
			bsp->Subsectors[i].firstline = &bsp->Segs[(size_t)bsp->Subsectors[i].firstline];
		}
	}
}

int FNodeBuilder::CloseSubsector (TArray<glseg_t> &segs, int subsector, vertex_t *outVerts)
{
	FPrivSeg *seg, *prev;
	angle_t prevAngle;
	double accumx, accumy;
	fixed_t midx, midy;
	int firstVert;
	uint32_t first, max, count, i, j;
	bool diffplanes;
	int firstplane;

	first = (uint32_t)(size_t)Subsectors[subsector].firstline;
	max = first + Subsectors[subsector].numlines;
	count = 0;

	accumx = accumy = 0.0;
	diffplanes = false;
	firstplane = Segs[SegList[first].SegNum].planenum;

	// Calculate the midpoint of the subsector and also check for degenerate subsectors.
	// A subsector is degenerate if it exists in only one dimension, which can be
	// detected when all the segs lie in the same plane. This can happen if you have
	// outward-facing lines in the void that don't point toward any sector. (Some of the
	// polyobjects in Hexen are constructed like this.)
	for (i = first; i < max; ++i)
	{
		seg = &Segs[SegList[i].SegNum];
		accumx += double(Vertices[seg->v1].x) + double(Vertices[seg->v2].x);
		accumy += double(Vertices[seg->v1].y) + double(Vertices[seg->v2].y);
		if (firstplane != seg->planenum)
		{
			diffplanes = true;
		}
	}

	midx = fixed_t(accumx / (max - first) / 2);
	midy = fixed_t(accumy / (max - first) / 2);

	seg = &Segs[SegList[first].SegNum];
	prevAngle = PointToAngle (Vertices[seg->v1].x - midx, Vertices[seg->v1].y - midy);
	seg->storedseg = PushGLSeg (segs, seg, outVerts);
	count = 1;
	prev = seg;
	firstVert = seg->v1;

#ifdef DD
	Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "--%d--\n", subsector);
	for (j = first; j < max; ++j)
	{
		seg = &Segs[SegList[j].SegNum];
		angle_t ang = PointToAngle (Vertices[seg->v1].x - midx, Vertices[seg->v1].y - midy);
		Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "%d%c %5d(%5d,%5d)->%5d(%5d,%5d) - %3.5f  %d,%d  [%08x,%08x]-[%08x,%08x]\n", j,
			seg->linedef == -1 ? '+' : ':',
			seg->v1, Vertices[seg->v1].x>>16, Vertices[seg->v1].y>>16,
			seg->v2, Vertices[seg->v2].x>>16, Vertices[seg->v2].y>>16,
			double(ang/2)*180/(1<<30),
			seg->planenum, seg->planefront,
			Vertices[seg->v1].x, Vertices[seg->v1].y,
			Vertices[seg->v2].x, Vertices[seg->v2].y);
	}
#endif

	if (diffplanes)
	{ // A well-behaved subsector. Output the segs sorted by the angle formed by connecting
	  // the subsector's center to their first vertex.

		D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Well behaved subsector\n"));
		for (i = first + 1; i < max; ++i)
		{
			angle_t bestdiff = ANGLE_MAX;
			FPrivSeg *bestseg = NULL;
			uint32_t bestj = UINT_MAX;
			j = first;
			do
			{
				seg = &Segs[SegList[j].SegNum];
				angle_t ang = PointToAngle (Vertices[seg->v1].x - midx, Vertices[seg->v1].y - midy);
				angle_t diff = prevAngle - ang;
				if (seg->v1 == prev->v2)
				{
					bestdiff = diff;
					bestseg = seg;
					bestj = j;
					break;
				}
				if (diff < bestdiff && diff > 0)
				{
					bestdiff = diff;
					bestseg = seg;
					bestj = j;
				}
			}
			while (++j < max);
			// Is a NULL bestseg actually okay?
			if (bestseg != NULL)
			{
				seg = bestseg;
			}
			if (prev->v2 != seg->v1)
			{
				// Add a new miniseg to connect the two segs
				PushConnectingGLSeg (subsector, segs, &outVerts[prev->v2], &outVerts[seg->v1]);
				count++;
			}
#ifdef DD
			Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "+%d\n", bestj);
#endif
			prevAngle -= bestdiff;
			seg->storedseg = PushGLSeg (segs, seg, outVerts);
			count++;
			prev = seg;
			if (seg->v2 == firstVert)
			{
				prev = seg;
				break;
			}
		}
#ifdef DD
		Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "\n");
#endif
	}
	else
	{ // A degenerate subsector. These are handled in three stages:
	  // Stage 1. Proceed in the same direction as the start seg until we
	  //          hit the seg furthest from it.
	  // Stage 2. Reverse direction and proceed until we hit the seg
	  //          furthest from the start seg.
	  // Stage 3. Reverse direction again and insert segs until we get
	  //          to the start seg.
	  // A dot product serves to determine distance from the start seg.

		D(Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "degenerate subsector\n"));

		// Stage 1. Go forward.
		count += OutputDegenerateSubsector (segs, subsector, true, 0, prev, outVerts);

		// Stage 2. Go backward.
		count += OutputDegenerateSubsector (segs, subsector, false, DBL_MAX, prev, outVerts);

		// Stage 3. Go forward again.
		count += OutputDegenerateSubsector (segs, subsector, true, -DBL_MAX, prev, outVerts);
	}

	if (prev->v2 != firstVert)
	{
		PushConnectingGLSeg (subsector, segs, &outVerts[prev->v2], &outVerts[firstVert]);
		count++;
	}
#ifdef DD
	Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Output GL subsector %d:\n", subsector);
	for (i = segs.Size() - count; i < (int)segs.Size(); ++i)
	{
		Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "  Seg %5d%c(%5d,%5d)-(%5d,%5d)  [%08x,%08x]-[%08x,%08x]\n", i,
			segs[i].linedef == NULL ? '+' : ' ',
			segs[i].v1->fixX()>>16,
			segs[i].v1->fixY()>>16,
			segs[i].v2->fixX()>>16,
			segs[i].v2->fixY()>>16,
			segs[i].v1->fixX(),
			segs[i].v1->fixY(),
			segs[i].v2->fixX(),
			segs[i].v2->fixY());
	}
#endif

	return count;
}

int FNodeBuilder::OutputDegenerateSubsector (TArray<glseg_t> &segs, int subsector, bool bForward, double lastdot, FPrivSeg *&prev, vertex_t *outVerts)
{
	static const double bestinit[2] = { -DBL_MAX, DBL_MAX };
	FPrivSeg *seg;
	int i, j, first, max, count;
	double dot, x1, y1, dx, dy, dx2, dy2;
	bool wantside;

	first = (uint32_t)(size_t)Subsectors[subsector].firstline;
	max = first + Subsectors[subsector].numlines;
	count = 0;

	seg = &Segs[SegList[first].SegNum];
	x1 = Vertices[seg->v1].x;
	y1 = Vertices[seg->v1].y;
	dx = Vertices[seg->v2].x - x1;
	dy = Vertices[seg->v2].y - y1;
	wantside = seg->planefront ^ !bForward;

	for (i = first + 1; i < max; ++i)
	{
		double bestdot = bestinit[bForward];
		FPrivSeg *bestseg = NULL;
		for (j = first + 1; j < max; ++j)
		{
			seg = &Segs[SegList[j].SegNum];
			if (seg->planefront != wantside)
			{
				continue;
			}
			dx2 = Vertices[seg->v1].x - x1;
			dy2 = Vertices[seg->v1].y - y1;
			dot = dx*dx2 + dy*dy2;

			if (bForward)
			{
				if (dot < bestdot && dot > lastdot)
				{
					bestdot = dot;
					bestseg = seg;
				}
			}
			else
			{
				if (dot > bestdot && dot < lastdot)
				{
					bestdot = dot;
					bestseg = seg;
				}
			}
		}
		if (bestseg != NULL)
		{
			if (prev->v2 != bestseg->v1)
			{
				PushConnectingGLSeg (subsector, segs, &outVerts[prev->v2], &outVerts[bestseg->v1]);
				count++;
			}
			seg->storedseg = PushGLSeg (segs, bestseg, outVerts);
			count++;
			prev = bestseg;
			lastdot = bestdot;
		}
	}
	return count;
}

uint32_t FNodeBuilder::PushGLSeg (TArray<glseg_t> &segs, const FPrivSeg *seg, vertex_t *outVerts)
{
	glseg_t newseg;

	newseg.v1 = outVerts + seg->v1;
	newseg.v2 = outVerts + seg->v2;
	newseg.backsector = seg->backsector;
	newseg.frontsector = seg->frontsector;
	if (seg->linedef != -1)
	{
		newseg.linedef = Level.Lines + seg->linedef;
		newseg.sidedef = Level.Sides + seg->sidedef;
	}
	else
	{
		newseg.linedef = NULL;
		newseg.sidedef = NULL;
	}
	newseg.Partner = seg->partner;
	return (uint32_t)segs.Push (newseg);
}

void FNodeBuilder::PushConnectingGLSeg (int subsector, TArray<glseg_t> &segs, vertex_t *v1, vertex_t *v2)
{
	glseg_t newseg;

	newseg.v1 = v1;
	newseg.v2 = v2;
	newseg.backsector = NULL;
	newseg.frontsector = NULL;
	newseg.linedef = NULL;
	newseg.sidedef = NULL;
	newseg.Partner = UINT_MAX;
	segs.Push (newseg);
}

//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
// Copyright 2010 James Haley
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Do all the WAD I/O, get map description,
//		set up initial state and misc. LUTs.
//
//-----------------------------------------------------------------------------

/* For code that originates from ZDoom the following applies:
**
**---------------------------------------------------------------------------
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


#include <math.h>
#include "maploader.h"
#include "c_cvars.h"
#include "actor.h"
#include "g_levellocals.h"
#include "p_lnspec.h"

#include "v_text.h"
#include "p_setup.h"
#include "gi.h"
#include "engineerrors.h"
#include "types.h"
#include "filesystem.h"
#include "p_conversation.h"
#include "v_video.h"
#include "i_time.h"
#include "m_argv.h"
#include "fragglescript/t_fs.h"
#include "swrenderer/r_swrenderer.h"
#include "flatvertices.h"
#include "xlat/xlat.h"
#include "vm.h"
#include "texturemanager.h"
#include "hw_vertexbuilder.h"
#if __has_include(<alloca.h>)
#include <alloca.h>
#endif

enum
{
	MISSING_TEXTURE_WARN_LIMIT = 20
};

CVAR (Bool, genblockmap, false, CVAR_SERVERINFO|CVAR_GLOBALCONFIG);
CVAR (Bool, gennodes, false, CVAR_SERVERINFO|CVAR_GLOBALCONFIG);

inline bool P_LoadBuildMap(uint8_t *mapdata, size_t len, FMapThing **things, int *numthings)
{
	return false;
}

//===========================================================================
//
// Now that ZDoom again gives the option of using Doom's original teleport
// behavior, only teleport dests in a sector with a 0 tag need to be
// given a TID. And since Doom format maps don't have TIDs, we can safely
// give them TID 1.
//
//===========================================================================

void MapLoader::TranslateTeleportThings ()
{
	AActor *dest;
	auto iterator = Level->GetThinkerIterator<AActor>(NAME_TeleportDest);
	bool foundSomething = false;
	
	while ( (dest = iterator.Next()) )
	{
		if (!Level->SectorHasTags(dest->Sector))
		{
			dest->SetTID(1);
			foundSomething = true;
		}
	}
	
	if (foundSomething)
	{
		for (auto &line : Level->lines)
		{
			if (line.special == Teleport)
			{
				if (line.args[1] == 0)
				{
					line.args[0] = 1;
				}
			}
			else if (line.special == Teleport_NoFog)
			{
				if (line.args[2] == 0)
				{
					line.args[0] = 1;
				}
			}
			else if (line.special == Teleport_ZombieChanger)
			{
				if (line.args[1] == 0)
				{
					line.args[0] = 1;
				}
			}
		}
	}
}


//===========================================================================
//
// Sets a sidedef's texture and prints a message if it's not present.
//
//===========================================================================

void MapLoader::SetTexture (side_t *side, int position, const char *name, FMissingTextureTracker &track)
{
	static const char *positionnames[] = { "top", "middle", "bottom" };
	static const char *sidenames[] = { "first", "second" };

	FTextureID texture = TexMan.CheckForTexture (name, ETextureType::Wall,
			FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);

	if (!texture.Exists())
	{
		if (++track[name].Count <= MISSING_TEXTURE_WARN_LIMIT)
		{
			// Print an error that lists all references to this sidedef.
			// We must scan the linedefs manually for all references to this sidedef.
			for(unsigned i = 0; i < Level->lines.Size(); i++)
			{
				for(int j = 0; j < 2; j++)
				{
					if (Level->lines[i].sidedef[j] == side)
					{
						Printf(TEXTCOLOR_RED "Unknown %s texture '"
							TEXTCOLOR_ORANGE "%s" TEXTCOLOR_RED
							"' on %s side of linedef %d\n",
							positionnames[position], name, sidenames[j], i);
					}
				}
			}
		}
		texture = TexMan.GetDefaultTexture();
	}
	side->SetTexture(position, texture);
}

//===========================================================================
//
// Sets a sidedef's texture and prints a message if it's not present.
// (Passing index separately is for UDMF which does not have sectors allocated yet)
//
//===========================================================================

void MapLoader::SetTexture (sector_t *sector, int index, int position, const char *name, FMissingTextureTracker &track, bool truncate)
{
	static const char *positionnames[] = { "floor", "ceiling" };
	char name8[9];
	if (truncate)
	{
		strncpy(name8, name, 8);
		name8[8] = 0;
		name = name8;
	}

	FTextureID texture = TexMan.CheckForTexture (name, ETextureType::Flat,
			FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);

	if (!texture.Exists())
	{
		if (++track[name].Count <= MISSING_TEXTURE_WARN_LIMIT)
		{
			Printf(TEXTCOLOR_RED"Unknown %s texture '"
				TEXTCOLOR_ORANGE "%s" TEXTCOLOR_RED
				"' in sector %d\n",
				positionnames[position], name, index);
		}
		texture = TexMan.GetDefaultTexture();
	}
	sector->SetTexture(position, texture);
}

//===========================================================================
//
// SummarizeMissingTextures
//
// Lists textures that were missing more than MISSING_TEXTURE_WARN_LIMIT
// times.
//
//===========================================================================

void MapLoader::SummarizeMissingTextures(const FMissingTextureTracker &missing)
{
	FMissingTextureTracker::ConstIterator it(missing);
	FMissingTextureTracker::ConstPair *pair;

	while (it.NextPair(pair))
	{
		if (pair->Value.Count > MISSING_TEXTURE_WARN_LIMIT)
		{
			Printf(TEXTCOLOR_RED "Missing texture '"
				TEXTCOLOR_ORANGE "%s" TEXTCOLOR_RED
				"' is used %d more times\n",
				pair->Key.GetChars(), pair->Value.Count - MISSING_TEXTURE_WARN_LIMIT);
		}
	}
}

//===========================================================================
//
// [RH] Figure out blends for deep water sectors
//
//===========================================================================

void MapLoader::SetTexture (side_t *side, int position, uint32_t *blend, const char *name)
{
	FTextureID texture;
	if ((*blend = R_ColormapNumForName (name)) == 0)
	{
		texture = TexMan.CheckForTexture (name, ETextureType::Wall,
			FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);
		if (!texture.Exists())
		{
			char name2[9];
			char *stop;
			strncpy (name2, name, 8);
			name2[8] = 0;
			*blend = strtoul (name2, &stop, 16);
			texture = FNullTextureID();
		}
		else
		{
			*blend = 0;
		}
	}
	else
	{
		texture = FNullTextureID();
	}
	side->SetTexture(position, texture);
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::SetTextureNoErr (side_t *side, int position, uint32_t *color, const char *name, bool *validcolor, bool isFog)
{
	FTextureID texture;
	*validcolor = false;
	texture = TexMan.CheckForTexture (name, ETextureType::Wall,	
		FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);
	if (!texture.Exists())
	{
		char name2[9];
		char *stop;
		strncpy (name2, name+1, 7);
		name2[7] = 0;
		if (*name != '#')
		{
			*color = strtoul (name, &stop, 16);
			texture = FNullTextureID();
			*validcolor = (*stop == 0) && (stop >= name + 2) && (stop <= name + 6);
			return;
		}
		else	// Support for Legacy's color format!
		{
			int l=(int)strlen(name);
			texture = FNullTextureID();
			*validcolor = false;
			if (l>=7) 
			{
				for(stop=name2;stop<name2+6;stop++) if (!isxdigit(*stop)) *stop='0';

				int factor = l==7? 0 : clamp<int> ((name2[6]&223)-'A', 0, 25);

				name2[6]=0; int blue=strtol(name2+4,nullptr,16);
				name2[4]=0; int green=strtol(name2+2,nullptr,16);
				name2[2]=0; int red=strtol(name2,nullptr,16);

				if (!isFog) 
				{
					if (factor==0) 
					{
						*validcolor=false;
						return;
					}
					factor = factor * 255 / 25;
				}
				else
				{
					factor=0;
				}

				*color=MAKEARGB(factor, red, green, blue);
				texture = FNullTextureID();
				*validcolor = true;
				return;
			}
		}
		texture = FNullTextureID();
	}
	side->SetTexture(position, texture);
}

//===========================================================================
//
// Sound enviroment handling
//
//===========================================================================

void MapLoader::FloodZone (sector_t *sec, int zonenum)
{
	if (sec->ZoneNumber == zonenum)
		return;

	sec->ZoneNumber = zonenum;

	for (auto check : sec->Lines)
	{
		sector_t *other;

		if (check->sidedef[1] == nullptr || (check->flags & ML_ZONEBOUNDARY))
			continue;

		if (check->frontsector == sec)
		{
			assert(check->backsector != nullptr);
			other = check->backsector;
		}
		else
		{
			assert(check->frontsector != nullptr);
			other = check->frontsector;
		}

		if (other->ZoneNumber != zonenum)
			FloodZone (other, zonenum);
	}
}

void MapLoader::FloodZones ()
{
	int z = 0, i;
	ReverbContainer *reverb;

	for (auto &sec : Level->sectors)
	{
		if (sec.ZoneNumber == 0xFFFF)
		{
			FloodZone (&sec, z++);
		}
	}
	Level->Zones.Resize(z);
	reverb = S_FindEnvironment(Level->DefaultEnvironment);
	if (reverb == nullptr)
	{
		Printf("Sound environment %d, %d not found\n", Level->DefaultEnvironment >> 8, Level->DefaultEnvironment & 255);
		reverb = DefaultEnvironments[0];
	}
	for (i = 0; i < z; ++i)
	{
		Level->Zones[i].Environment = reverb;
	}
}

//===========================================================================
//
// P_LoadVertexes
//
//===========================================================================

void MapLoader::LoadVertexes(MapData * map)
{
	// Determine number of vertices:
	//	total lump length / vertex record length.
	unsigned numvertexes = map->Size(ML_VERTEXES) / sizeof(mapvertex_t);

	if (numvertexes == 0)
	{
		I_Error("Map has no vertices.\n");
	}

	// Allocate memory for buffer.
	Level->vertexes.Alloc(numvertexes);

	auto &fr = map->Reader(ML_VERTEXES);

	// Copy and convert vertex coordinates, internal representation as fixed.
	for (auto &v : Level->vertexes)
	{
		int16_t x = fr.ReadInt16();
		int16_t y = fr.ReadInt16();

		v.set(double(x), double(y));
	}
}

//===========================================================================
//
// P_LoadZSegs
//
//===========================================================================

void MapLoader::LoadZSegs (FileReader &data)
{
	for (auto &seg : Level->segs)
	{
		line_t *ldef;
		uint32_t v1 = data.ReadUInt32();
		uint32_t v2 = data.ReadUInt32();
		uint16_t line = data.ReadUInt16();
		uint8_t side = data.ReadUInt8();

		seg.v1 = &Level->vertexes[v1];
		seg.v2 = &Level->vertexes[v2];
		seg.linedef = ldef = &Level->lines[line];
		seg.sidedef = ldef->sidedef[side];
		seg.frontsector = ldef->sidedef[side]->sector;
		if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != nullptr)
		{
			seg.backsector = ldef->sidedef[side^1]->sector;
		}
		else
		{
			seg.backsector = 0;
			ldef->flags &= ~ML_TWOSIDED;
		}
	}
}

//===========================================================================
//
// P_LoadGLZSegs
//
// This is the GL nodes version of the above function.
//
//===========================================================================

void MapLoader::LoadGLZSegs (FileReader &data, int type)
{
	for (unsigned i = 0; i < Level->subsectors.Size(); ++i)
	{

		for (size_t j = 0; j < Level->subsectors[i].numlines; ++j)
		{
			seg_t *seg;
			uint32_t v1 = data.ReadUInt32();
			uint32_t partner = data.ReadUInt32();
			uint32_t line;

			if (type >= 2)
			{
				line = data.ReadUInt32();
			}
			else
			{
				line = data.ReadUInt16();
				if (line == 0xffff) line = 0xffffffff;
			}
			uint8_t side = data.ReadUInt8();

			seg = Level->subsectors[i].firstline + j;
			seg->v1 = &Level->vertexes[v1];
			if (j == 0)
			{
				seg[Level->subsectors[i].numlines - 1].v2 = seg->v1;
			}
			else
			{
				seg[-1].v2 = seg->v1;
			}
			
			seg->PartnerSeg = partner == 0xffffffffu? nullptr : &Level->segs[partner];
			if (line != 0xFFFFFFFF)
			{
				line_t *ldef;

				seg->linedef = ldef = &Level->lines[line];
				seg->sidedef = ldef->sidedef[side];
				seg->frontsector = ldef->sidedef[side]->sector;
				if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != nullptr)
				{
					seg->backsector = ldef->sidedef[side^1]->sector;
				}
				else
				{
					seg->backsector = 0;
					ldef->flags &= ~ML_TWOSIDED;
				}
			}
			else
			{
				seg->linedef = nullptr;
				seg->sidedef = nullptr;
				seg->frontsector = seg->backsector = Level->subsectors[i].firstline->frontsector;
			}
		}
	}
}

//===========================================================================
//
// P_LoadZNodes
//
//===========================================================================

void MapLoader::LoadZNodes(FileReader &data, int glnodes)
{
	// Read extra vertices added during node building
	unsigned int i;

	uint32_t orgVerts = data.ReadUInt32();
	uint32_t newVerts = data.ReadUInt32();
	if (orgVerts > Level->vertexes.Size())
	{ // These nodes are based on a map with more vertex data than we have.
	  // We can't use them.
		throw CRecoverableError("Incorrect number of vertexes in nodes.\n");
	}
	auto oldvertexes = &Level->vertexes[0];
	if (orgVerts + newVerts != Level->vertexes.Size())
	{
		Level->vertexes.Reserve(newVerts);
	}
	for (i = 0; i < newVerts; ++i)
	{
		fixed_t x = data.ReadInt32();
		fixed_t y = data.ReadInt32();
		Level->vertexes[i + orgVerts].set(x, y);
	}
	if (oldvertexes != &Level->vertexes[0])
	{
		for (auto &line : Level->lines)
		{
			line.v1 = line.v1 - oldvertexes + &Level->vertexes[0];
			line.v2 = line.v2 - oldvertexes + &Level->vertexes[0];
		}
	}

	// Read the subsectors
	uint32_t currSeg;
	uint32_t numSubs = data.ReadUInt32();
	Level->subsectors.Alloc(numSubs);
	memset (&Level->subsectors[0], 0, Level->subsectors.Size()*sizeof(subsector_t));

	for (i = currSeg = 0; i < numSubs; ++i)
	{
		uint32_t numsegs = data.ReadUInt32();
		Level->subsectors[i].firstline = (seg_t *)(size_t)currSeg;		// Oh damn. I should have stored the seg count sooner.
		Level->subsectors[i].numlines = numsegs;
		currSeg += numsegs;
	}

	// Read the segs
	uint32_t numSegs = data.ReadUInt32();

	// The number of segs stored should match the number of
	// segs used by subsectors.
	if (numSegs != currSeg)
	{
		throw CRecoverableError("Incorrect number of segs in nodes.\n");
	}

	Level->segs.Alloc(numSegs);
	memset (&Level->segs[0], 0, numSegs*sizeof(seg_t));

	for (auto &sub : Level->subsectors)
	{
		sub.firstline = &Level->segs[(size_t)sub.firstline];
	}

	if (glnodes == 0)
	{
		LoadZSegs (data);
	}
	else
	{
		LoadGLZSegs (data, glnodes);
	}

	// Read nodes
	uint32_t numNodes = data.ReadUInt32();

	auto &nodes = Level->nodes;
	nodes.Alloc(numNodes);
	memset (&nodes[0], 0, sizeof(node_t)*numNodes);

	for (i = 0; i < numNodes; ++i)
	{
		if (glnodes < 3)
		{
			nodes[i].x = data.ReadInt16() * FRACUNIT;
			nodes[i].y = data.ReadInt16() * FRACUNIT;
			nodes[i].dx = data.ReadInt16() * FRACUNIT;
			nodes[i].dy = data.ReadInt16() * FRACUNIT;
		}
		else
		{
			nodes[i].x = data.ReadInt32();
			nodes[i].y = data.ReadInt32();
			nodes[i].dx = data.ReadInt32();
			nodes[i].dy = data.ReadInt32();
		}
		for (int j = 0; j < 2; ++j)
		{
			for (int k = 0; k < 4; ++k)
			{
				nodes[i].bbox[j][k] = data.ReadInt16();
			}
		}
		for (int m = 0; m < 2; ++m)
		{
			uint32_t child = data.ReadUInt32();
			if (child & 0x80000000)
			{
				nodes[i].children[m] = (uint8_t *)&Level->subsectors[child & 0x7FFFFFFF] + 1;
			}
			else
			{
				nodes[i].children[m] = &nodes[child];
			}
		}
	}
}

//===========================================================================
//
//
//
//===========================================================================

bool MapLoader::LoadExtendedNodes (FileReader &dalump, uint32_t id)
{
	int type;
	bool compressed;

	switch (id)
	{
	case MAKE_ID('Z','N','O','D'):
		type = 0;
		compressed = true;
		break;

	case MAKE_ID('Z','G','L','N'):
		type = 1;
		compressed = true;
		break;

	case MAKE_ID('Z','G','L','2'):
		type = 2;
		compressed = true;
		break;

	case MAKE_ID('Z','G','L','3'):
		type = 3;
		compressed = true;
		break;

	case MAKE_ID('X','N','O','D'):
		type = 0;
		compressed = false;
		break;

	case MAKE_ID('X','G','L','N'):
		type = 1;
		compressed = false;
		break;

	case MAKE_ID('X','G','L','2'):
		type = 2;
		compressed = false;
		break;

	case MAKE_ID('X','G','L','3'):
		type = 3;
		compressed = false;
		break;

	default:
		return false;
	}
	
	try
	{
		if (compressed)
		{
			FileReader zip;
			try
			{
				if (zip.OpenDecompressor(dalump, -1, METHOD_ZLIB, false, [](const char* err) { I_Error("%s", err); }))
				{
					LoadZNodes(zip, type);
				}
				else
				{
					Printf("Error loading nodes: Corrupt data.\n");
					return false;
				}
			}
			catch (const CRecoverableError& err)
			{
				Printf("Error loading nodes: %s.\n", err.what());

				ForceNodeBuild = true;
				Level->subsectors.Clear();
				Level->segs.Clear();
				Level->nodes.Clear();
				return false;
			}
		}
		else
		{
			LoadZNodes(dalump, type);
		}
		return true;
	}
	catch (CRecoverableError &error)
	{
		Printf("Error loading nodes: %s\n", error.GetMessage());

		ForceNodeBuild = true;
		Level->subsectors.Clear();
		Level->segs.Clear();
		Level->nodes.Clear();
		return false;
	}

}



//===========================================================================
//
// P_CheckV4Nodes
// http://www.sbsoftware.com/files/DeePBSPV4specs.txt
//
//===========================================================================

static bool P_CheckV4Nodes(MapData *map)
{
	if (map->Size(ML_NODES) == 0)
	{
		return false;
	}

	char header[8];

	map->Read(ML_NODES, header, 8);
	return !memcmp(header, "xNd4\0\0\0\0", 8);
}


//===========================================================================
//
// P_LoadSegs
//
// killough 5/3/98: reformatted, cleaned up
//
//===========================================================================

struct badseg
{
	badseg(int t, int s, int d) : badtype(t), badsegnum(s), baddata(d) {}
	int badtype;
	int badsegnum;
	int baddata;
};

template<class segtype>
bool MapLoader::LoadSegs (MapData * map)
{
	uint32_t numvertexes = Level->vertexes.Size();
	TArray<uint8_t> vertchanged(numvertexes, true);
	uint32_t segangle;
	//int ptp_angle;		// phares 10/4/98
	//int delta_angle;	// phares 10/4/98
	uint32_t vnum1,vnum2;	// phares 10/4/98
	int lumplen = map->Size(ML_SEGS);

	memset(vertchanged.Data(), 0, numvertexes); // phares 10/4/98

	unsigned numsegs = lumplen / sizeof(segtype);

	if (numsegs == 0)
	{
		Printf ("This map has no segs.\n");
		Level->subsectors.Clear();
		Level->nodes.Clear();
		return false;
	}

	Level->segs.Alloc(numsegs);
	auto &segs = Level->segs;
	memset (&segs[0], 0, numsegs*sizeof(seg_t));

	auto data = map->Read(ML_SEGS);

	for (auto &sub : Level->subsectors)
	{
		sub.firstline = &segs[(size_t)sub.firstline];
	}

	// phares: 10/4/98: Vertchanged is an array that represents the vertices.
	// Mark those used by linedefs. A marked vertex is one that is not a
	// candidate for movement further down.

	for (auto &line : Level->lines)
	{
		vertchanged[Index(line.v1)] = vertchanged[Index(line.v2)] = 1;
	}

	try
	{
		for (unsigned i = 0; i < numsegs; i++)
		{
			seg_t *li = &segs[i];
			segtype *ml = ((segtype *) data.Data()) + i;

			int side, linedef;
			line_t *ldef;

			vnum1 = ml->V1();
			vnum2 = ml->V2();

			if (vnum1 >= numvertexes || vnum2 >= numvertexes)
			{
				throw badseg(0, i, MAX(vnum1, vnum2));
			}

			li->v1 = &Level->vertexes[vnum1];
			li->v2 = &Level->vertexes[vnum2];

			segangle = (uint16_t)LittleShort(ml->angle);

			// phares 10/4/98: In the case of a lineseg that was created by splitting
			// another line, it appears that the line angle is inherited from the
			// father line. Due to roundoff, the new vertex may have been placed 'off
			// the line'. When you get close to such a line, and it is very short,
			// it's possible that the roundoff error causes 'firelines', the thin
			// lines that can draw from screen top to screen bottom occasionally. This
			// is due to all the angle calculations that are done based on the line
			// angle, the angles from the viewer to the vertices, and the viewer's
			// angle in the world. In the case of firelines, the rounded-off position
			// of one of the vertices determines one of these angles, and introduces
			// an error in the scaling factor for mapping textures and determining
			// where on the screen the ceiling and floor spans should be shown. For a
			// fireline, the engine thinks the ceiling bottom and floor top are at the
			// midpoint of the screen. So you get ceilings drawn all the way down to the
			// screen midpoint, and floors drawn all the way up. Thus 'firelines'. The
			// name comes from the original sighting, which involved a fire texture.
			//
			// To correct this, reset the vertex that was added so that it sits ON the
			// split line.
			//
			// To know which of the two vertices was added, its number is greater than
			// that of the last of the author-created vertices. If both vertices of the
			// line were added by splitting, pick the higher-numbered one. Once you've
			// changed a vertex, don't change it again if it shows up in another seg.
			//
			// To determine if there's an error in the first place, find the
			// angle of the line between the two seg vertices. If it's one degree or more
			// off, then move one vertex. This may seem insignificant, but one degree
			// errors _can_ cause firelines.

			DAngle ptp_angle = (li->v2->fPos() - li->v1->fPos()).Angle();
			DAngle seg_angle = AngleToFloat(segangle << 16);
			DAngle delta_angle = absangle(ptp_angle, seg_angle);

			if (delta_angle >= 1.)
			{
				double dis = (li->v2->fPos() - li->v1->fPos()).Length();
				DVector2 delta = seg_angle.ToVector(dis);
				if ((vnum2 > vnum1) && (vertchanged[vnum2] == 0))
				{
					li->v2->set(li->v1->fPos() + delta);
					vertchanged[vnum2] = 1; // this was changed
				}
				else if (vertchanged[vnum1] == 0)
				{
					li->v1->set(li->v2->fPos() - delta);
					vertchanged[vnum1] = 1; // this was changed
				}
			}

			linedef = LittleShort(ml->linedef);
			if ((unsigned)linedef >= Level->lines.Size())
			{
				throw badseg(1, i, linedef);
			}
			ldef = &Level->lines[linedef];
			li->linedef = ldef;
			side = LittleShort(ml->side);
			if (side != 0 && side != 1)
			{
				throw badseg(3, i, side);
			}
			if ((unsigned)(Index(ldef->sidedef[side])) >= Level->sides.Size())
			{
				throw badseg(2, i, Index(ldef->sidedef[side]));
			}
			li->sidedef = ldef->sidedef[side];
			li->frontsector = ldef->sidedef[side]->sector;

			// killough 5/3/98: ignore 2s flag if second sidedef missing:
			if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != nullptr)
			{
				li->backsector = ldef->sidedef[side^1]->sector;
			}
			else
			{
				li->backsector = 0;
				ldef->flags &= ~ML_TWOSIDED;
			}
		}
	}
	catch (const badseg &bad) // the preferred way is to catch by (const) reference.
	{
		switch (bad.badtype)
		{
		case 0:
			Printf ("Seg %d references a nonexistant vertex %d (max %d).\n", bad.badsegnum, bad.baddata, numvertexes);
			break;

		case 1:
			Printf ("Seg %d references a nonexistant linedef %d (max %u).\n", bad.badsegnum, bad.baddata, Level->lines.Size());
			break;

		case 2:
			Printf ("The linedef for seg %d references a nonexistant sidedef %d (max %d).\n", bad.badsegnum, bad.baddata, Level->sides.Size());
			break;

		case 3:
			Printf("Sidedef reference in seg %d is %d (must be 0 or 1).\n", bad.badsegnum, bad.baddata);
			break;
		}
		Printf ("The BSP will be rebuilt.\n");
		Level->segs.Clear();
		Level->subsectors.Clear();
		Level->nodes.Clear();
		return false;
	}
	return true;
}


//===========================================================================
//
// P_LoadSubsectors
//
//===========================================================================

template<class subsectortype, class segtype>
bool MapLoader::LoadSubsectors (MapData * map)
{
	uint32_t maxseg = map->Size(ML_SEGS) / sizeof(segtype);

	unsigned numsubsectors = map->Size(ML_SSECTORS) / sizeof(subsectortype);

	if (numsubsectors == 0 || maxseg == 0 )
	{
		Printf ("This map has an incomplete BSP tree.\n");
		Level->nodes.Clear();
		return false;
	}

	auto &subsectors = Level->subsectors;
	subsectors.Alloc(numsubsectors);
	auto &fr = map->Reader(ML_SSECTORS);
		
	memset (&subsectors[0], 0, numsubsectors*sizeof(subsector_t));
	
	for (unsigned i = 0; i < numsubsectors; i++)
	{
		subsectortype subd;

		subd.numsegs = sizeof(subd.numsegs) == 2 ? fr.ReadUInt16() : fr.ReadUInt32();
		subd.firstseg = sizeof(subd.firstseg) == 2 ? fr.ReadUInt16() : fr.ReadUInt32();

		if (subd.numsegs == 0)
		{
			Printf ("Subsector %i is empty.\n", i);
			Level->subsectors.Clear();
			Level->nodes.Clear();
			return false;
		}

		subsectors[i].numlines = subd.numsegs;
		subsectors[i].firstline = (seg_t *)(size_t)subd.firstseg;

		if ((size_t)subsectors[i].firstline >= maxseg)
		{
			Printf ("Subsector %d contains invalid segs %u-%u\n"
				"The BSP will be rebuilt.\n", i, (unsigned)((size_t)subsectors[i].firstline),
				(unsigned)((size_t)subsectors[i].firstline) + subsectors[i].numlines - 1);
			Level->nodes.Clear();
			Level->subsectors.Clear();
			return false;
		}
		else if ((size_t)subsectors[i].firstline + subsectors[i].numlines > maxseg)
		{
			Printf ("Subsector %d contains invalid segs %u-%u\n"
				"The BSP will be rebuilt.\n", i, maxseg,
				(unsigned)((size_t)subsectors[i].firstline) + subsectors[i].numlines - 1);

			Level->nodes.Clear();
			Level->subsectors.Clear();
			return false;
		}
	}
	return true;
}


//===========================================================================
//
// P_LoadSectors
//
//===========================================================================

void MapLoader::LoadSectors (MapData *map, FMissingTextureTracker &missingtex)
{
	mapsector_t			*ms;
	sector_t*			ss;
	int					defSeqType;
	int					lumplen = map->Size(ML_SECTORS);

	unsigned numsectors = lumplen / sizeof(mapsector_t);
	Level->sectors.Alloc(numsectors);
	Level->extsectors.Alloc(numsectors);
	auto sectors = &Level->sectors[0];
	memset (sectors, 0, numsectors*sizeof(sector_t));

	if (Level->flags & LEVEL_SNDSEQTOTALCTRL)
		defSeqType = 0;
	else
		defSeqType = -1;

	auto msp = map->Read(ML_SECTORS);
	ms = (mapsector_t*)msp.Data();
	ss = sectors;
	
	for (unsigned i = 0; i < numsectors; i++, ss++, ms++)
	{
		ss->e = &Level->extsectors[i];
		ss->Level = Level;
		if (!map->HasBehavior) ss->Flags |= SECF_FLOORDROP;
		ss->SetPlaneTexZ(sector_t::floor, (double)LittleShort(ms->floorheight));
		ss->floorplane.set(0, 0, 1., -ss->GetPlaneTexZ(sector_t::floor));
		ss->SetPlaneTexZ(sector_t::ceiling, (double)LittleShort(ms->ceilingheight));
		ss->ceilingplane.set(0, 0, -1., ss->GetPlaneTexZ(sector_t::ceiling));
		SetTexture(ss, i, sector_t::floor, ms->floorpic, missingtex, true);
		SetTexture(ss, i, sector_t::ceiling, ms->ceilingpic, missingtex, true);
		ss->lightlevel = LittleShort(ms->lightlevel);
		if (map->HasBehavior)
			ss->special = LittleShort(ms->special);
		else	// [RH] Translate to new sector special
			ss->special = Level->TranslateSectorSpecial (LittleShort(ms->special));
		Level->tagManager.AddSectorTag(i, LittleShort(ms->tag));
		ss->thinglist = nullptr;
		ss->touching_thinglist = nullptr;		// phares 3/14/98
		ss->sectorportal_thinglist = nullptr;
		ss->touching_renderthings = nullptr;
		ss->seqType = defSeqType;
		ss->SeqName = NAME_None;
		ss->nextsec = -1;	//jff 2/26/98 add fields to support locking out
		ss->prevsec = -1;	// stair retriggering until build completes
		memset(ss->SpecialColors, -1, sizeof(ss->SpecialColors));
		memset(ss->AdditiveColors, 0, sizeof(ss->AdditiveColors));

		ss->SetAlpha(sector_t::floor, 1.);
		ss->SetAlpha(sector_t::ceiling, 1.);
		ss->SetXScale(sector_t::floor, 1.);	// [RH] floor and ceiling scaling
		ss->SetYScale(sector_t::floor, 1.);
		ss->SetXScale(sector_t::ceiling, 1.);
		ss->SetYScale(sector_t::ceiling, 1.);

		ss->heightsec = nullptr;	// sector used to get floor and ceiling height
		// killough 3/7/98: end changes

		ss->gravity = 1.f;	// [RH] Default sector gravity of 1.0
		ss->ZoneNumber = 0xFFFF;
		ss->terrainnum[sector_t::ceiling] = ss->terrainnum[sector_t::floor] = -1;



		// [RH] Sectors default to white light with the default fade.
		//		If they are outside (have a sky ceiling), they use the outside fog.
		ss->Colormap.LightColor = PalEntry(255, 255, 255);
		if (Level->outsidefog != 0xff000000 && (ss->GetTexture(sector_t::ceiling) == skyflatnum || (ss->special&0xff) == Sector_Outside))
		{
			ss->Colormap.FadeColor.SetRGB(Level->outsidefog);
		}
		else  if (Level->flags & LEVEL_HASFADETABLE)
		{
			ss->Colormap.FadeColor= 0x939393;	// The true color software renderer needs this. (The hardware renderer will ignore this value if LEVEL_HASFADETABLE is set.)
		}
		else
		{
			ss->Colormap.FadeColor.SetRGB(Level->fadeto);
		}


		// killough 8/28/98: initialize all sectors to normal friction
		ss->friction = ORIG_FRICTION;
		ss->movefactor = ORIG_FRICTION_FACTOR;
		ss->sectornum = i;
		ss->ibocount = -1;
	}
}


//===========================================================================
//
// P_LoadNodes
//
//===========================================================================

template<class nodetype, class subsectortype>
bool MapLoader::LoadNodes (MapData * map)
{
	FileData	data;
	int 		j;
	int 		k;
	nodetype	*mn;
	node_t* 	no;
	uint16_t*		used;
	int			lumplen = map->Size(ML_NODES);
	int			maxss = map->Size(ML_SSECTORS) / sizeof(subsectortype);

	unsigned numnodes = (lumplen - nodetype::NF_LUMPOFFSET) / sizeof(nodetype);

	if ((numnodes == 0 && maxss != 1) || maxss == 0)
	{
		return false;
	}
	
	auto &nodes = Level->nodes;
	nodes.Alloc(numnodes);		
	used = (uint16_t *)alloca (sizeof(uint16_t)*numnodes);
	memset (used, 0, sizeof(uint16_t)*numnodes);

	auto mnp = map->Read(ML_NODES);
	mn = (nodetype*)(mnp.Data() + nodetype::NF_LUMPOFFSET);
	no = &nodes[0];
	
	for (unsigned i = 0; i < numnodes; i++, no++, mn++)
	{
		no->x = LittleShort(mn->x)<<FRACBITS;
		no->y = LittleShort(mn->y)<<FRACBITS;
		no->dx = LittleShort(mn->dx)<<FRACBITS;
		no->dy = LittleShort(mn->dy)<<FRACBITS;
		for (j = 0; j < 2; j++)
		{
			int child = mn->Child(j);
			if (child & nodetype::NF_SUBSECTOR)
			{
				child &= ~nodetype::NF_SUBSECTOR;
				if (child >= maxss)
				{
					Printf ("BSP node %d references invalid subsector %d.\n"
						"The BSP will be rebuilt.\n", i, child);
					Level->nodes.Clear();
					return false;
				}
				no->children[j] = (uint8_t *)&Level->subsectors[child] + 1;
			}
			else if ((unsigned)child >= numnodes)
			{
				Printf ("BSP node %d references invalid node %d.\n"
					"The BSP will be rebuilt.\n", i, Index(((node_t *)no->children[j])));
				Level->nodes.Clear();
				return false;
			}
			else if (used[child])
			{
				Printf ("BSP node %d references node %d,\n"
					"which is already used by node %d.\n"
					"The BSP will be rebuilt.\n", i, child, used[child]-1);
				Level->nodes.Clear();
				return false;
			}
			else
			{
				no->children[j] = &nodes[child];
				used[child] = j + 1;
			}
			for (k = 0; k < 4; k++)
			{
				no->bbox[j][k] = (float)LittleShort(mn->bbox[j][k]);
			}
		}
	}
	return true;
}

//===========================================================================
//
// SetMapThingUserData
//
//===========================================================================

void MapLoader::SetMapThingUserData(AActor *actor, unsigned udi)
{
	if (actor == nullptr)
	{
		return;
	}
	while (MapThingsUserData[udi].Key != NAME_None)
	{
		FName varname = MapThingsUserData[udi].Key;
		PField *var = dyn_cast<PField>(actor->GetClass()->FindSymbol(varname, true));

		if (var == nullptr || (var->Flags & (VARF_Native|VARF_Private|VARF_Protected|VARF_Static)) || !var->Type->isScalar())
		{
			DPrintf(DMSG_WARNING, "%s is not a writable user variable in class %s\n", varname.GetChars(),
				actor->GetClass()->TypeName.GetChars());
		}
		else
		{ // Set the value of the specified user variable.
			void *addr = reinterpret_cast<uint8_t *>(actor) + var->Offset;
			if (var->Type == TypeString)
			{
				var->Type->InitializeValue(addr, &MapThingsUserData[udi].StringVal);
			}
			else if (var->Type->isFloat())
			{
				var->Type->SetValue(addr, MapThingsUserData[udi].FloatVal);
			}
			else if (var->Type->isInt() || var->Type == TypeBool)
			{
				var->Type->SetValue(addr, MapThingsUserData[udi].IntVal);
			}
		}

		udi++;
	}
}

//===========================================================================
//
// P_LoadThings
//
//===========================================================================

uint16_t MakeSkill(int flags)
{
	uint16_t res = 0;
	if (flags & 1) res |= 1+2;
	if (flags & 2) res |= 4;
	if (flags & 4) res |= 8+16;
	return res;
}

void MapLoader::LoadThings (MapData * map)
{
	mapthing_t *mt;
	auto mtp = map->Read(ML_THINGS);
	int numthings = mtp.Size() / sizeof(mapthing_t);
	mt = (mapthing_t*)mtp.Data();

	MapThingsConverted.Resize(numthings);
	FMapThing *mti = &MapThingsConverted[0];

	// [RH] ZDoom now uses Hexen-style maps as its native format.
	//		Since this is the only place where Doom-style Things are ever
	//		referenced, we translate them into a Hexen-style thing.
	for (int i=0 ; i < numthings; i++, mt++)
	{
		// [RH] At this point, monsters unique to Doom II were weeded out
		//		if the IWAD wasn't for Doom II. P_SpawnMapThing() can now
		//		handle these and more cases better, so we just pass it
		//		everything and let it decide what to do with them.

		// [RH] Need to translate the spawn flags to Hexen format.
		short flags = LittleShort(mt->options);

		memset (&mti[i], 0, sizeof(mti[i]));

		mti[i].Gravity = 1;
		mti[i].Conversation = 0;
		mti[i].SkillFilter = MakeSkill(flags);
		mti[i].ClassFilter = 0xffff;	// Doom map format doesn't have class flags so spawn for all player classes
		mti[i].RenderStyle = STYLE_Count;
		mti[i].Alpha = -1;
		mti[i].Health = 1;
		mti[i].FloatbobPhase = -1;

		mti[i].pos.X = LittleShort(mt->x);
		mti[i].pos.Y = LittleShort(mt->y);
		mti[i].angle = LittleShort(mt->angle);
		mti[i].EdNum = LittleShort(mt->type);
		mti[i].info = DoomEdMap.CheckKey(mti[i].EdNum);


#ifndef NO_EDATA
		if (mti[i].info != nullptr && mti[i].info->Special == SMT_EDThing)
		{
			ProcessEDMapthing(&mti[i], flags);
		}
		else
#endif
		{
			flags &= ~MTF_SKILLMASK;
			mti[i].flags = (short)((flags & 0xf) | 0x7e0);
			if (gameinfo.gametype == GAME_Strife)
			{
				mti[i].flags &= ~MTF_AMBUSH;
				if (flags & STF_SHADOW)			mti[i].flags |= MTF_SHADOW;
				if (flags & STF_ALTSHADOW)		mti[i].flags |= MTF_ALTSHADOW;
				if (flags & STF_STANDSTILL)		mti[i].flags |= MTF_STANDSTILL;
				if (flags & STF_AMBUSH)			mti[i].flags |= MTF_AMBUSH;
				if (flags & STF_FRIENDLY)		mti[i].flags |= MTF_FRIENDLY;
			}
			else
			{
				if (flags & BTF_BADEDITORCHECK)
				{
					flags &= 0x1F;
				}
				if (flags & BTF_NOTDEATHMATCH)	mti[i].flags &= ~MTF_DEATHMATCH;
				if (flags & BTF_NOTCOOPERATIVE)	mti[i].flags &= ~MTF_COOPERATIVE;
				if (flags & BTF_FRIENDLY)		mti[i].flags |= MTF_FRIENDLY;
			}
			if (flags & BTF_NOTSINGLE)			mti[i].flags &= ~MTF_SINGLE;
		}
	}
}

//===========================================================================
//
// [RH]
// P_LoadThings2
//
// Same as P_LoadThings() except it assumes Things are
// saved Hexen-style. Position also controls which single-
// player start spots are spawned by filtering out those
// whose first parameter don't match position.
//
//===========================================================================

void MapLoader::LoadThings2 (MapData * map)
{
	int	lumplen = map->Size(ML_THINGS);
	int numthings = lumplen / sizeof(mapthinghexen_t);

	MapThingsConverted.Resize(numthings);
	FMapThing *mti = &MapThingsConverted[0];

	auto mtp = map->Read(ML_THINGS);
	mapthinghexen_t *mth = (mapthinghexen_t*)mtp.Data();

	for(int i = 0; i< numthings; i++)
	{
		memset (&mti[i], 0, sizeof(mti[i]));

		mti[i].thingid = LittleShort(mth[i].thingid);
		mti[i].pos.X = LittleShort(mth[i].x);
		mti[i].pos.Y = LittleShort(mth[i].y);
		mti[i].pos.Z = LittleShort(mth[i].z);
		mti[i].angle = LittleShort(mth[i].angle);
		mti[i].EdNum = LittleShort(mth[i].type);
		mti[i].info = DoomEdMap.CheckKey(mti[i].EdNum);
		mti[i].flags = LittleShort(mth[i].flags);
		mti[i].special = mth[i].special;
		for(int j=0;j<5;j++) mti[i].args[j] = mth[i].args[j];
		mti[i].SkillFilter = MakeSkill(mti[i].flags);
		mti[i].ClassFilter = (mti[i].flags & MTF_CLASS_MASK) >> MTF_CLASS_SHIFT;
		mti[i].flags &= ~(MTF_SKILLMASK|MTF_CLASS_MASK);
		if (Level->flags2 & LEVEL2_HEXENHACK)
		{
			mti[i].flags &= 0x7ff;	// mask out Strife flags if playing an original Hexen map.
		}

		mti[i].Gravity = 1;
		mti[i].RenderStyle = STYLE_Count;
		mti[i].Alpha = -1;
		mti[i].Health = 1;
		mti[i].FloatbobPhase = -1;
		mti[i].friendlyseeblocks = -1;
	}
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::SpawnThings (int position)
{
	int numthings = MapThingsConverted.Size();

	for (int i=0; i < numthings; i++)
	{
		AActor *actor = Level->SpawnMapThing (i, &MapThingsConverted[i], position);
		unsigned *udi = MapThingsUserDataIndex.CheckKey((unsigned)i);
		if (udi != nullptr)
		{
			SetMapThingUserData(actor, *udi);
		}
	}
}


//===========================================================================
//
// P_LoadLineDefs
//
// killough 4/4/98: split into two functions, to allow sidedef overloading
//
// [RH] Actually split into four functions to allow for Hexen and Doom
//		linedefs.
//
//===========================================================================

//===========================================================================
//
// [RH] Set line id (as appropriate) here
// for Doom format maps this must be done in P_TranslateLineDef because
// the tag doesn't always go into the first arg.
//
//===========================================================================

void MapLoader::SetLineID (int i, line_t *ld)
{
	if (Level->maptype == MAPTYPE_HEXEN)	
	{
		int setid = -1;
		switch (ld->special)
		{
		case Line_SetIdentification:
			if (!(Level->flags2 & LEVEL2_HEXENHACK))
			{
				setid = ld->args[0] + 256 * ld->args[4];
				ld->flags |= ld->args[1]<<16;
			}
			else
			{
				setid = ld->args[0];
			}
			ld->special = 0;
			break;

		case TranslucentLine:
			setid = ld->args[0];
			ld->flags |= ld->args[3]<<16;
			break;

		case Teleport_Line:
		case Scroll_Texture_Model:
			setid = ld->args[0];
			break;

		case Polyobj_StartLine:
			setid = ld->args[3];
			break;

		case Polyobj_ExplicitLine:
			setid = ld->args[4];
			break;
			
		case Plane_Align:
			if (!(Level->ib_compatflags & BCOMPATF_NOSLOPEID)) setid = ld->args[2];
			break;
			
		case Static_Init:
			if (ld->args[1] == Init_SectorLink) setid = ld->args[0];
			break;

		case Line_SetPortal:
			setid = ld->args[1]; // 0 = target id, 1 = this id, 2 = plane anchor
			break;
		}
		if (setid != -1)
		{
			Level->tagManager.AddLineID(i, setid);
		}
	}
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::SaveLineSpecial (line_t *ld)
{
	if (ld->sidedef[0] == nullptr)
		return;

	uint32_t sidenum = Index(ld->sidedef[0]);
	// killough 4/4/98: support special sidedef interpretation below
	// [RH] Save Static_Init only if it's interested in the textures
	if	(ld->special != Static_Init || ld->args[1] == Init_Color)
	{
		sidetemp[sidenum].a.special = ld->special;
		sidetemp[sidenum].a.tag = ld->args[0];
	}
	else
	{
		sidetemp[sidenum].a.special = 0;
	}
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::FinishLoadingLineDef(line_t *ld, int alpha)
{
	bool additive = false;

	ld->frontsector = ld->sidedef[0] != nullptr ? ld->sidedef[0]->sector : nullptr;
	ld->backsector  = ld->sidedef[1] != nullptr ? ld->sidedef[1]->sector : nullptr;
	double dx = (ld->v2->fX() - ld->v1->fX());
	double dy = (ld->v2->fY() - ld->v1->fY());
	int linenum = Index(ld);

	if (ld->frontsector == nullptr)
	{
		Printf ("Line %d has no front sector\n", linemap[linenum]);
	}

	// [RH] Set some new sidedef properties
	int len = (int)(g_sqrt (dx*dx + dy*dy) + 0.5f);

	if (ld->sidedef[0] != nullptr)
	{
		ld->sidedef[0]->linedef = ld;
		ld->sidedef[0]->TexelLength = len;

	}
	if (ld->sidedef[1] != nullptr)
	{
		ld->sidedef[1]->linedef = ld;
		ld->sidedef[1]->TexelLength = len;
	}

	switch (ld->special)
	{						// killough 4/11/98: handle special types
	case TranslucentLine:			// killough 4/11/98: translucent 2s textures
		// [RH] Second arg controls how opaque it is.
		if (alpha == SHRT_MIN)
		{
			alpha = ld->args[1];
			additive = !!ld->args[2];
		}
		else if (alpha < 0)
		{
			alpha = -alpha;
			additive = true;
		}

		double dalpha = alpha / 255.;
		if (!ld->args[0])
		{
			ld->alpha = dalpha;
			if (additive)
			{
				ld->flags |= ML_ADDTRANS;
			}
		}
		else
		{
			for (unsigned j = 0; j < Level->lines.Size(); j++)
			{
				if (Level->LineHasId(j, ld->args[0]))
				{
					Level->lines[j].alpha = dalpha;
					if (additive)
					{
						Level->lines[j].flags |= ML_ADDTRANS;
					}
				}
			}
		}
		ld->special = 0;
		break;
	}
}

//===========================================================================
//
// killough 4/4/98: delay using sidedefs until they are loaded
//
//===========================================================================

void MapLoader::FinishLoadingLineDefs ()
{
	for (auto &line : Level->lines)
	{
		FinishLoadingLineDef(&line, sidetemp[Index(line.sidedef[0])].a.alpha);
	}
}

//===========================================================================
//
//
//
//===========================================================================

 void MapLoader::SetSideNum (side_t **sidenum_p, uint16_t sidenum)
{
	if (sidenum == NO_INDEX)
	{
		*sidenum_p = nullptr;
	}
	else if (sidecount < (int)Level->sides.Size())
	{
		sidetemp[sidecount].a.map = sidenum;
		*sidenum_p = &Level->sides[sidecount++];
	}
	else
	{
		I_Error ("%d sidedefs is not enough\n", sidecount);
	}
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::LoadLineDefs (MapData * map)
{
	int i, skipped;
	line_t *ld;
	maplinedef_t *mld;
		
	auto mldf = map->Read(ML_LINEDEFS);
	int numlines = mldf.Size() / sizeof(maplinedef_t);
	int numsides = map->Size(ML_SIDEDEFS) / sizeof(mapsidedef_t);
	linemap.Resize(numlines);

	// [RH] Count the number of sidedef references. This is the number of
	// sidedefs we need. The actual number in the SIDEDEFS lump might be less.
	// Lines with 0 length are also removed.

	for (skipped = sidecount = i = 0; i < numlines; )
	{
		mld = ((maplinedef_t*)mldf.Data()) + i;
		unsigned v1 = LittleShort(mld->v1);
		unsigned v2 = LittleShort(mld->v2);

		if (v1 >= Level->vertexes.Size() || v2 >= Level->vertexes.Size())
		{
			I_Error ("Line %d has invalid vertices: %d and/or %d.\nThe map only contains %u vertices.", i+skipped, v1, v2, Level->vertexes.Size());
		}
		else if (v1 == v2 ||
			(Level->vertexes[v1].fX() == Level->vertexes[v2].fX() && Level->vertexes[v1].fY() == Level->vertexes[v2].fY()))
		{
			Printf ("Removing 0-length line %d\n", i+skipped);
			memmove (mld, mld+1, sizeof(*mld)*(numlines-i-1));
			ForceNodeBuild = true;
			skipped++;
			numlines--;
		}
		else
		{
			sidecount++;
			if (LittleShort(mld->sidenum[1]) != NO_INDEX)
				sidecount++;
			linemap[i] = i+skipped;
			i++;
		}
	}
	Level->lines.Alloc(numlines);
	memset(&Level->lines[0], 0, numlines * sizeof(line_t));

	AllocateSideDefs (map, sidecount);

	mld = (maplinedef_t *)mldf.Data();
	ld = &Level->lines[0];
	for (i = 0; i < numlines; i++, mld++, ld++)
	{
		ld->alpha = 1.;	// [RH] Opaque by default
		ld->portalindex = UINT_MAX;
		ld->portaltransferred = UINT_MAX;

		// [RH] Translate old linedef special and flags to be
		//		compatible with the new format.

		mld->special = LittleShort(mld->special);
		mld->tag = LittleShort(mld->tag);
		mld->flags = LittleShort(mld->flags);
		Level->TranslateLineDef (ld, mld, -1);
		// do not assign the tag for Extradata lines.
		if (ld->special != Static_Init || (ld->args[1] != Init_EDLine && ld->args[1] != Init_EDSector))
		{
			Level->tagManager.AddLineID(i, mld->tag);
		}
#ifndef NO_EDATA
		if (ld->special == Static_Init && ld->args[1] == Init_EDLine)
		{
			ProcessEDLinedef(ld, mld->tag);
		}
#endif
		// cph 2006/09/30 - fix sidedef errors right away.
		for (int j=0; j < 2; j++)
		{
			if (LittleShort(mld->sidenum[j]) != NO_INDEX && mld->sidenum[j] >= numsides)
			{
				mld->sidenum[j] = 0; // dummy sidedef
				Printf("Linedef %d has a bad sidedef\n", i);
			}
		}
		// patch missing first sides instead of crashing out.
		// Visual glitches are better than not being able to play.
		if (LittleShort(mld->sidenum[0]) == NO_INDEX)
		{
			Printf("Line %d has no first side.\n", i);
			mld->sidenum[0] = 0;
		}

		ld->v1 = &Level->vertexes[LittleShort(mld->v1)];
		ld->v2 = &Level->vertexes[LittleShort(mld->v2)];

		SetSideNum (&ld->sidedef[0], LittleShort(mld->sidenum[0]));
		SetSideNum (&ld->sidedef[1], LittleShort(mld->sidenum[1]));

		ld->AdjustLine ();
		SaveLineSpecial (ld);
		if (Level->flags2 & LEVEL2_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
		if (Level->flags2 & LEVEL2_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
		if (Level->flags2 & LEVEL2_CHECKSWITCHRANGE) ld->flags |= ML_CHECKSWITCHRANGE;
	}
}

//===========================================================================
//
// [RH] Same as P_LoadLineDefs() except it uses Hexen-style LineDefs.
//
//===========================================================================

void MapLoader::LoadLineDefs2 (MapData * map)
{
	int i, skipped;
	line_t *ld;
	int lumplen = map->Size(ML_LINEDEFS);
	maplinedef2_t *mld;
		
	int numlines = lumplen / sizeof(maplinedef2_t);
	linemap.Resize(numlines);

	auto mldf =	map->Read(ML_LINEDEFS);

	// [RH] Remove any lines that have 0 length and count sidedefs used
	for (skipped = sidecount = i = 0; i < numlines; )
	{
		mld = ((maplinedef2_t*)mldf.Data()) + i;

		if (mld->v1 == mld->v2 ||
			(Level->vertexes[LittleShort(mld->v1)].fX() == Level->vertexes[LittleShort(mld->v2)].fX() &&
			 Level->vertexes[LittleShort(mld->v1)].fY() == Level->vertexes[LittleShort(mld->v2)].fY()))
		{
			Printf ("Removing 0-length line %d\n", i+skipped);
			memmove (mld, mld+1, sizeof(*mld)*(numlines-i-1));
			skipped++;
			numlines--;
		}
		else
		{
			// patch missing first sides instead of crashing out.
			// Visual glitches are better than not being able to play.
			if (LittleShort(mld->sidenum[0]) == NO_INDEX)
			{
				Printf("Line %d has no first side.\n", i);
				mld->sidenum[0] = 0;
			}
			sidecount++;
			if (LittleShort(mld->sidenum[1]) != NO_INDEX)
				sidecount++;
			linemap[i] = i+skipped;
			i++;
		}
	}
	if (skipped > 0)
	{
		ForceNodeBuild = true;
	}
	Level->lines.Alloc(numlines);
	memset(&Level->lines[0], 0, numlines * sizeof(line_t));

	AllocateSideDefs (map, sidecount);

	mld = (maplinedef2_t *)mldf.Data();
	ld = &Level->lines[0];
	for (i = 0; i < numlines; i++, mld++, ld++)
	{
		int j;

		ld->portalindex = UINT_MAX;
		ld->portaltransferred = UINT_MAX;

		for (j = 0; j < 5; j++)
			ld->args[j] = mld->args[j];

		ld->flags = LittleShort(mld->flags);
		ld->special = mld->special;

		ld->v1 = &Level->vertexes[LittleShort(mld->v1)];
		ld->v2 = &Level->vertexes[LittleShort(mld->v2)];
		ld->alpha = 1.;	// [RH] Opaque by default

		SetSideNum (&ld->sidedef[0], LittleShort(mld->sidenum[0]));
		SetSideNum (&ld->sidedef[1], LittleShort(mld->sidenum[1]));

		ld->AdjustLine ();
		SetLineID(i, ld);
		SaveLineSpecial (ld);
		if (Level->flags2 & LEVEL2_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
		if (Level->flags2 & LEVEL2_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
		if (Level->flags2 & LEVEL2_CHECKSWITCHRANGE) ld->flags |= ML_CHECKSWITCHRANGE;

		// convert the activation type
		ld->activation = 1 << GET_SPAC(ld->flags);
		if (ld->activation == SPAC_AnyCross)
		{ // this is really PTouch
			ld->activation = SPAC_Impact | SPAC_PCross;
		}
		else if (ld->activation == SPAC_Impact)
		{ // In non-UMDF maps, Impact implies PCross
			ld->activation = SPAC_Impact | SPAC_PCross;
		}
		ld->flags &= ~ML_SPAC_MASK;
	}
}


//===========================================================================
//
//
//
//===========================================================================

 void MapLoader::AllocateSideDefs (MapData *map, int count)
{
	int i;

	Level->sides.Alloc(count);
	memset(&Level->sides[0], 0, count * sizeof(side_t));

	sidetemp.Resize(MAX<int>(count, Level->vertexes.Size()));
	for (i = 0; i < count; i++)
	{
		sidetemp[i].a.special = sidetemp[i].a.tag = 0;
		sidetemp[i].a.alpha = SHRT_MIN;
		sidetemp[i].a.map = NO_SIDE;
	}
	int numsides = map->Size(ML_SIDEDEFS) / sizeof(mapsidedef_t);
	if (count < numsides)
	{
		Printf ("Map has %d unused sidedefs\n", numsides - count);
	}
	numsides = count;
	sidecount = 0;
}

//===========================================================================
//
// [RH] Group sidedefs into loops so that we can easily determine
// what walls any particular wall neighbors.
//
//===========================================================================

void MapLoader::LoopSidedefs (bool firstloop)
{
	int i;

	int numsides = Level->sides.Size();
	sidetemp.Resize(MAX<int>(Level->vertexes.Size(), numsides));

	for (i = 0; i < (int)Level->vertexes.Size(); ++i)
	{
		sidetemp[i].b.first = NO_SIDE;
		sidetemp[i].b.next = NO_SIDE;
	}
	for (; i < numsides; ++i)
	{
		sidetemp[i].b.next = NO_SIDE;
	}

	for (i = 0; i < numsides; ++i)
	{
		// For each vertex, build a list of sidedefs that use that vertex
		// as their left edge.
		line_t *line = Level->sides[i].linedef;
		int lineside = (line->sidedef[0] != &Level->sides[i]);
		int vert = lineside ? Index(line->v2) : Index(line->v1);
		
		sidetemp[i].b.lineside = lineside;
		sidetemp[i].b.next = sidetemp[vert].b.first;
		sidetemp[vert].b.first = i;

		// Set each side so that it is the only member of its loop
		Level->sides[i].LeftSide = NO_SIDE;
		Level->sides[i].RightSide = NO_SIDE;
	}

	// For each side, find the side that is to its right and set the
	// loop pointers accordingly. If two sides share a left vertex, the
	// one that forms the smallest angle is assumed to be the right one.
	for (i = 0; i < numsides; ++i)
	{
		uint32_t right;
		line_t *line = Level->sides[i].linedef;

		// If the side's line only exists in a single sector,
		// then consider that line to be a self-contained loop
		// instead of as part of another loop
		if (line->frontsector == line->backsector)
		{
			side_t* rightside = line->sidedef[!sidetemp[i].b.lineside];

			if (nullptr == rightside)
			{
				// There is no right side!
				if (firstloop) Printf ("Line %d's right edge is unconnected\n", linemap[Index(line)]);
				continue;
			}

			right = Index(rightside);
		}
		else
		{
			if (sidetemp[i].b.lineside)
			{
				right = Index(line->v1);
			}
			else
			{
				right = Index(line->v2);
			}

			right = sidetemp[right].b.first;

			if (right == NO_SIDE)
			{ 
				// There is no right side!
				if (firstloop) Printf ("Line %d's right edge is unconnected\n", linemap[Index(line)]);
				continue;
			}

			if (sidetemp[right].b.next != NO_SIDE)
			{
				int bestright = right;	// Shut up, GCC
				DAngle bestang = 360.;
				line_t *leftline, *rightline;
				DAngle ang1, ang2, ang;

				leftline = Level->sides[i].linedef;
				ang1 = leftline->Delta().Angle();
				if (!sidetemp[i].b.lineside)
				{
					ang1 += 180;
				}

				while (right != NO_SIDE)
				{
					if (Level->sides[right].LeftSide == NO_SIDE)
					{
						rightline = Level->sides[right].linedef;
						if (rightline->frontsector != rightline->backsector)
						{
							ang2 = rightline->Delta().Angle();
							if (sidetemp[right].b.lineside)
							{
								ang2 += 180;
							}

							ang = (ang2 - ang1).Normalized360();

							if (ang != 0 && ang <= bestang)
							{
								bestright = right;
								bestang = ang;
							}
						}
					}
					right = sidetemp[right].b.next;
				}
				right = bestright;
			}
		}
		assert((unsigned)i<(unsigned)numsides);
		assert(right<(unsigned)numsides);
		Level->sides[i].RightSide = right;
		Level->sides[right].LeftSide = i;
	}

	// We keep the sidedef init info around until after polyobjects are initialized,
	// so don't delete just yet.
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::ProcessSideTextures(bool checktranmap, side_t *sd, sector_t *sec, intmapsidedef_t *msd, int special, int tag, short *alpha, FMissingTextureTracker &missingtex)
{
	switch (special)
	{
	case Transfer_Heights:	// variable colormap via 242 linedef
		  // [RH] The colormap num we get here isn't really a colormap,
		  //	  but a packed ARGB word for blending, so we also allow
		  //	  the blend to be specified directly by the texture names
		  //	  instead of figuring something out from the colormap.
		if (sec != nullptr)
		{
			SetTexture (sd, side_t::bottom, &sec->bottommap, msd->bottomtexture);
			SetTexture (sd, side_t::mid, &sec->midmap, msd->midtexture);
			SetTexture (sd, side_t::top, &sec->topmap, msd->toptexture);
		}
		break;

	case Static_Init:
		// [RH] Set sector color and fog
		// upper "texture" is light color
		// lower "texture" is fog color
		{
			uint32_t color = MAKERGB(255,255,255), fog = 0;
			bool colorgood, foggood;

			SetTextureNoErr (sd, side_t::bottom, &fog, msd->bottomtexture, &foggood, true);
			SetTextureNoErr (sd, side_t::top, &color, msd->toptexture, &colorgood, false);
			SetTexture(sd, side_t::mid, msd->midtexture, missingtex);

			if (colorgood | foggood)
			{
				for (unsigned s = 0; s < Level->sectors.Size(); s++)
				{
					if (Level->SectorHasTag(s, tag))
					{
						if (colorgood)
						{
							Level->sectors[s].Colormap.LightColor.SetRGB(color);
							Level->sectors[s].Colormap.BlendFactor = APART(color);
						}
						if (foggood) Level->sectors[s].Colormap.FadeColor.SetRGB(fog);
					}
				}
			}
		}
		break;

	case Sector_Set3DFloor:
		if (msd->toptexture[0]=='#')
		{
			sd->SetTexture(side_t::top, FNullTextureID() +(int)(-strtoll(&msd->toptexture[1], nullptr, 10)));	// store the alpha as a negative texture index
														// This will be sorted out by the 3D-floor code later.
		}
		else
		{
			SetTexture(sd, side_t::top, msd->toptexture, missingtex);
		}

		SetTexture(sd, side_t::mid, msd->midtexture, missingtex);
		SetTexture(sd, side_t::bottom, msd->bottomtexture, missingtex);
		break;

	case TranslucentLine:	// killough 4/11/98: apply translucency to 2s normal texture
		if (checktranmap)
		{
			int lumpnum;

			if (strnicmp ("TRANMAP", msd->midtexture, 8) == 0)
			{
				// The translator set the alpha argument already; no reason to do it again.
				sd->SetTexture(side_t::mid, FNullTextureID());
			}
			else if ((lumpnum = fileSystem.CheckNumForName (msd->midtexture)) > 0 &&
				fileSystem.FileLength (lumpnum) == 65536)
			{
				auto fr = fileSystem.OpenFileReader(lumpnum);
				*alpha = (short)GPalette.DetermineTranslucency (fr);

				if (developer >= DMSG_NOTIFY)
				{
					char lumpname[9];
					lumpname[8] = 0;
					fileSystem.GetFileShortName(lumpname, lumpnum);
					if (*alpha < 0) Printf("%s appears to be additive translucency %d (%d%%)\n", lumpname, -*alpha, -*alpha * 100 / 255);
					else Printf("%s appears to be translucency %d (%d%%)\n", lumpname, *alpha, *alpha * 100 / 255);
				}

				sd->SetTexture(side_t::mid, FNullTextureID());
			}
			else
			{
				SetTexture(sd, side_t::mid, msd->midtexture, missingtex);
			}

			SetTexture(sd, side_t::top, msd->toptexture, missingtex);
			SetTexture(sd, side_t::bottom, msd->bottomtexture, missingtex);
			break;
		}
		// Fallthrough for Hexen maps is intentional
		[[fallthrough]];

	default:			// normal cases

		SetTexture(sd, side_t::mid, msd->midtexture, missingtex);
		SetTexture(sd, side_t::top, msd->toptexture, missingtex);
		SetTexture(sd, side_t::bottom, msd->bottomtexture, missingtex);
		break;
	}
}

//===========================================================================
//
// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up
//
//===========================================================================

void MapLoader::LoadSideDefs2 (MapData *map, FMissingTextureTracker &missingtex)
{
	auto msdf = map->Read(ML_SIDEDEFS);

	for (unsigned i = 0; i < Level->sides.Size(); i++)
	{
		mapsidedef_t *msd = ((mapsidedef_t*)msdf.Data()) + sidetemp[i].a.map;
		side_t *sd = &Level->sides[i];
		sector_t *sec;

		// [RH] The Doom renderer ignored the patch y locations when
		// drawing mid textures. ZDoom does not, so fix the laser beams in Strife.
		if (gameinfo.gametype == GAME_Strife &&
			strncmp (msd->midtexture, "LASERB01", 8) == 0)
		{
			msd->rowoffset += 102;
		}

		sd->SetTextureXOffset(LittleShort(msd->textureoffset));
		sd->SetTextureYOffset(LittleShort(msd->rowoffset));
		sd->SetTextureXScale(1.);
		sd->SetTextureYScale(1.);
		sd->linedef = nullptr;
		sd->Flags = 0;
		sd->UDMFIndex = i;

		// killough 4/4/98: allow sidedef texture names to be overloaded
		// killough 4/11/98: refined to allow colormaps to work as wall
		// textures if invalid as colormaps but valid as textures.
		// cph 2006/09/30 - catch out-of-range sector numbers; use sector 0 instead
		if ((unsigned)LittleShort(msd->sector)>=Level->sectors.Size())
		{
			Printf (PRINT_HIGH, "Sidedef %d has a bad sector\n", i);
			sd->sector = sec = &Level->sectors[0];
		}
		else
		{
			sd->sector = sec = &Level->sectors[LittleShort(msd->sector)];
		}

		intmapsidedef_t imsd;
		imsd.toptexture.CopyCStrPart(msd->toptexture, 8);
		imsd.midtexture.CopyCStrPart(msd->midtexture, 8);
		imsd.bottomtexture.CopyCStrPart(msd->bottomtexture, 8);

		ProcessSideTextures(!map->HasBehavior, sd, sec, &imsd, 
							  sidetemp[i].a.special, sidetemp[i].a.tag, &sidetemp[i].a.alpha, missingtex);
	}
}


//===========================================================================
//
// [RH] My own blockmap builder, not Killough's or TeamTNT's.
//
// Killough's turned out not to be correct enough, and I had
// written this for ZDBSP before I discovered that, so
// replacing the one he wrote for MBF seemed like the easiest
// thing to do. (Doom E3M6, near vertex 0--the one furthest east
// on the map--had problems.)
//
// Using a hash table to get the minimum possible blockmap size
// seems like overkill, but I wanted to change the code as little
// as possible from its ZDBSP incarnation.
//
//===========================================================================

static unsigned int BlockHash (TArray<int> *block)
{
	int hash = 0;
	int *ar = &(*block)[0];
	for (size_t i = 0; i < block->Size(); ++i)
	{
		hash = hash * 12235 + ar[i];
	}
	return hash & 0x7fffffff;
}

static bool BlockCompare (TArray<int> *block1, TArray<int> *block2)
{
	size_t size = block1->Size();

	if (size != block2->Size())
	{
		return false;
	}
	if (size == 0)
	{
		return true;
	}
	int *ar1 = &(*block1)[0];
	int *ar2 = &(*block2)[0];
	for (size_t i = 0; i < size; ++i)
	{
		if (ar1[i] != ar2[i])
		{
			return false;
		}
	}
	return true;
}

static void CreatePackedBlockmap (TArray<int> &BlockMap, TArray<int> *blocks, int bmapwidth, int bmapheight)
{
	int buckets[4096];
	int hashblock;
	TArray<int> *block;
	int zero = 0;
	int terminator = -1;
	int *array;
	int i, hash;
	int hashed = 0, nothashed = 0;

	TArray<int> hashes(bmapwidth * bmapheight, true);

	memset (hashes.Data(), 0xff, sizeof(int)*bmapwidth*bmapheight);
	memset (buckets, 0xff, sizeof(buckets));

	for (i = 0; i < bmapwidth * bmapheight; ++i)
	{
		block = &blocks[i];
		hash = BlockHash (block) % 4096;
		hashblock = buckets[hash];
		while (hashblock != -1)
		{
			if (BlockCompare (block, &blocks[hashblock]))
			{
				break;
			}
			hashblock = hashes[hashblock];
		}
		if (hashblock != -1)
		{
			BlockMap[4+i] = BlockMap[4+hashblock];
			hashed++;
		}
		else
		{
			hashes[i] = buckets[hash];
			buckets[hash] = i;
			BlockMap[4+i] = BlockMap.Size ();
			BlockMap.Push (zero);
			array = &(*block)[0];
			for (size_t j = 0; j < block->Size(); ++j)
			{
				BlockMap.Push (array[j]);
			}
			BlockMap.Push (terminator);
			nothashed++;
		}
	}
}


void MapLoader::CreateBlockMap ()
{
	enum
	{
		BLOCKBITS = 7,
		BLOCKSIZE = 128
	};

	TArray<int> *block, *endblock;
	TArray<TArray<int>> BlockLists;
	int adder;
	int bmapwidth, bmapheight;
	double dminx, dmaxx, dminy, dmaxy;
	int minx, maxx, miny, maxy;
	int line;

	if (Level->vertexes.Size() == 0)
		return;

	// Find map extents for the blockmap
	dminx = dmaxx = Level->vertexes[0].fX();
	dminy = dmaxy = Level->vertexes[0].fY();

	for (auto &vert : Level->vertexes)
	{
			 if (vert.fX() < dminx) dminx = vert.fX();
		else if (vert.fX() > dmaxx) dmaxx = vert.fX();
			 if (vert.fY() < dminy) dminy = vert.fY();
		else if (vert.fY() > dmaxy) dmaxy = vert.fY();
	}

	minx = int(dminx);
	miny = int(dminy);
	maxx = int(dmaxx);
	maxy = int(dmaxy);

	bmapwidth =	 ((maxx - minx) >> BLOCKBITS) + 1;
	bmapheight = ((maxy - miny) >> BLOCKBITS) + 1;

	TArray<int> BlockMap (bmapwidth * bmapheight * 3 + 4);

	adder = minx;			BlockMap.Push (adder);
	adder = miny;			BlockMap.Push (adder);
	adder = bmapwidth;		BlockMap.Push (adder);
	adder = bmapheight;		BlockMap.Push (adder);

	BlockLists.Resize(bmapwidth * bmapheight);

	for (line = 0; line < (int)Level->lines.Size(); ++line)
	{
		int x1 = int(Level->lines[line].v1->fX());
		int y1 = int(Level->lines[line].v1->fY());
		int x2 = int(Level->lines[line].v2->fX());
		int y2 = int(Level->lines[line].v2->fY());
		int dx = x2 - x1;
		int dy = y2 - y1;
		int bx = (x1 - minx) >> BLOCKBITS;
		int by = (y1 - miny) >> BLOCKBITS;
		int bx2 = (x2 - minx) >> BLOCKBITS;
		int by2 = (y2 - miny) >> BLOCKBITS;

		block = &BlockLists[bx + by * bmapwidth];
		endblock = &BlockLists[bx2 + by2 * bmapwidth];

		if (block == endblock)	// Single block
		{
			block->Push (line);
		}
		else if (by == by2)		// Horizontal line
		{
			if (bx > bx2)
			{
				std::swap (block, endblock);
			}
			do
			{
				block->Push (line);
				block += 1;
			} while (block <= endblock);
		}
		else if (bx == bx2)	// Vertical line
		{
			if (by > by2)
			{
				std::swap (block, endblock);
			}
			do
			{
				block->Push (line);
				block += bmapwidth;
			} while (block <= endblock);
		}
		else				// Diagonal line
		{
			int xchange = (dx < 0) ? -1 : 1;
			int ychange = (dy < 0) ? -1 : 1;
			int ymove = ychange * bmapwidth;
			int adx = abs (dx);
			int ady = abs (dy);

			if (adx == ady)		// 45 degrees
			{
				int xb = (x1 - minx) & (BLOCKSIZE-1);
				int yb = (y1 - miny) & (BLOCKSIZE-1);
				if (dx < 0)
				{
					xb = BLOCKSIZE-xb;
				}
				if (dy < 0)
				{
					yb = BLOCKSIZE-yb;
				}
				if (xb < yb)
					adx--;
			}
			if (adx >= ady)		// X-major
			{
				int yadd = dy < 0 ? -1 : BLOCKSIZE;
				do
				{
					int stop = (Scale ((by << BLOCKBITS) + yadd - (y1 - miny), dx, dy) + (x1 - minx)) >> BLOCKBITS;
					while (bx != stop)
					{
						block->Push (line);
						block += xchange;
						bx += xchange;
					}
					block->Push (line);
					block += ymove;
					by += ychange;
				} while (by != by2);
				while (block != endblock)
				{
					block->Push (line);
					block += xchange;
				}
				block->Push (line);
			}
			else					// Y-major
			{
				int xadd = dx < 0 ? -1 : BLOCKSIZE;
				do
				{
					int stop = (Scale ((bx << BLOCKBITS) + xadd - (x1 - minx), dy, dx) + (y1 - miny)) >> BLOCKBITS;
					while (by != stop)
					{
						block->Push (line);
						block += ymove;
						by += ychange;
					}
					block->Push (line);
					block += xchange;
					bx += xchange;
				} while (bx != bx2);
				while (block != endblock)
				{
					block->Push (line);
					block += ymove;
				}
				block->Push (line);
			}
		}
	}

	BlockMap.Reserve (bmapwidth * bmapheight);
	CreatePackedBlockmap (BlockMap, BlockLists.Data(), bmapwidth, bmapheight);

	Level->blockmap.blockmaplump = new int[BlockMap.Size()];
	for (unsigned int ii = 0; ii < BlockMap.Size(); ++ii)
	{
		Level->blockmap.blockmaplump[ii] = BlockMap[ii];
	}
}



//===========================================================================
//
// P_VerifyBlockMap
//
// haleyjd 03/04/10: do verification on validity of blockmap.
//
//===========================================================================

bool FBlockmap::VerifyBlockMap(int count, unsigned numlines)
{
	int x, y;
	int *maxoffs = blockmaplump + count;

	int bmapwidth = blockmaplump[2];
	int bmapheight = blockmaplump[3];

	for(y = 0; y < bmapheight; y++)
	{
		for(x = 0; x < bmapwidth; x++)
		{
			int offset;
			int *list, *tmplist;
			int *blockoffset;

			offset = y * bmapwidth + x;
			blockoffset = blockmaplump + offset + 4;


			// check that block offset is in bounds
			if(blockoffset >= maxoffs)
			{
				Printf(PRINT_HIGH, "VerifyBlockMap: block offset overflow\n");
				return false;
			}

			offset = *blockoffset;         

			// check that list offset is in bounds
			if(offset < 4 || offset >= count)
			{
				Printf(PRINT_HIGH, "VerifyBlockMap: list offset overflow\n");
				return false;
			}

			list   = blockmaplump + offset;

			// scan forward for a -1 terminator before maxoffs
			for(tmplist = list; ; tmplist++)
			{
				// we have overflowed the lump?
				if(tmplist >= maxoffs)
				{
					Printf(PRINT_HIGH, "VerifyBlockMap: open blocklist\n");
					return false;
				}
				if(*tmplist == -1) // found -1
					break;
			}

			// there's some node builder which carelessly removed the initial 0-entry.
			// Rather than second-guessing the intent, let's just discard such blockmaps entirely
			// to be on the safe side.
			if (*list != 0)
			{
				Printf(PRINT_HIGH, "VerifyBlockMap: first entry is not 0.\n");
				return false;
			}

			// scan the list for out-of-range linedef indicies in list
			for(tmplist = list; *tmplist != -1; tmplist++)
			{
				if((unsigned)*tmplist >= numlines)
				{
					Printf(PRINT_HIGH, "VerifyBlockMap: index >= numlines\n");
					return false;
				}
			}
		}
	}

	return true;
}

//===========================================================================
//
// P_LoadBlockMap
//
// killough 3/1/98: substantially modified to work
// towards removing blockmap limit (a wad limitation)
//
// killough 3/30/98: Rewritten to remove blockmap limit
//
//===========================================================================

void MapLoader::LoadBlockMap (MapData * map)
{
	int count = map->Size(ML_BLOCKMAP);

	if (ForceNodeBuild || genblockmap ||
		count/2 >= 0x10000 || count == 0 ||
		Args->CheckParm("-blockmap")
		)
	{
		DPrintf (DMSG_SPAMMY, "Generating BLOCKMAP\n");
		CreateBlockMap ();
	}
	else
	{
		auto data = map->Read(ML_BLOCKMAP);
		const short *wadblockmaplump = (short *)data.Data();
		int i;

		count/=2;
		Level->blockmap.blockmaplump = new int[count];

		// killough 3/1/98: Expand wad blockmap into larger internal one,
		// by treating all offsets except -1 as unsigned and zero-extending
		// them. This potentially doubles the size of blockmaps allowed,
		// because Doom originally considered the offsets as always signed.

		Level->blockmap.blockmaplump[0] = LittleShort(wadblockmaplump[0]);
		Level->blockmap.blockmaplump[1] = LittleShort(wadblockmaplump[1]);
		Level->blockmap.blockmaplump[2] = (uint32_t)(LittleShort(wadblockmaplump[2])) & 0xffff;
		Level->blockmap.blockmaplump[3] = (uint32_t)(LittleShort(wadblockmaplump[3])) & 0xffff;

		for (i = 4; i < count; i++)
		{
			short t = LittleShort(wadblockmaplump[i]);          // killough 3/1/98
			Level->blockmap.blockmaplump[i] = t == -1 ? (uint32_t)0xffffffff : (uint32_t) t & 0xffff;
		}

		if (!Level->blockmap.VerifyBlockMap(count, Level->lines.Size()))
		{
			DPrintf (DMSG_SPAMMY, "Generating BLOCKMAP\n");
			CreateBlockMap();
		}

	}

	Level->blockmap.bmaporgx = Level->blockmap.blockmaplump[0];
	Level->blockmap.bmaporgy = Level->blockmap.blockmaplump[1];
	Level->blockmap.bmapwidth = Level->blockmap.blockmaplump[2];
	Level->blockmap.bmapheight = Level->blockmap.blockmaplump[3];

	// clear out mobj chains
	count = Level->blockmap.bmapwidth*Level->blockmap.bmapheight;
	Level->blockmap.blocklinks = new FBlockNode *[count];
	memset (Level->blockmap.blocklinks, 0, count*sizeof(*Level->blockmap.blocklinks));
	Level->blockmap.blockmap = Level->blockmap.blockmaplump+4;
}

//===========================================================================
//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
//===========================================================================

void MapLoader::GroupLines (bool buildmap)
{
	int 				total;
	sector_t*			sector;
	FBoundingBox		bbox;
	bool				flaggedNoFronts = false;
	unsigned int		jj;

	// look up sector number for each subsector
	for (auto &sub : Level->subsectors)
	{
		sub.sector = sub.firstline->sidedef->sector;
	}
	for (auto &sub : Level->subsectors)
	{
		for (jj = 0; jj < sub.numlines; ++jj)
		{
			sub.firstline[jj].Subsector = &sub;
		}
	}

	// count number of lines in each sector
	total = 0;
	for (unsigned i = 0; i < Level->lines.Size(); i++)
	{
		auto li = &Level->lines[i];
		if (li->frontsector == nullptr)
		{
			if (!flaggedNoFronts)
			{
				flaggedNoFronts = true;
				Printf ("The following lines do not have a front sidedef:\n");
			}
			Printf (" %d\n", i);
		}
		else
		{
			li->frontsector->Lines.Count++;
			total++;
		}

		if (li->backsector && li->backsector != li->frontsector)
		{
			li->backsector->Lines.Count++;
			total++;
		}
	}
	if (flaggedNoFronts)
	{
		I_Error ("You need to fix these lines to play this map.\n");
	}

	// build line tables for each sector
	Level->linebuffer.Alloc(total);
	line_t **lineb_p = &Level->linebuffer[0];
	auto numsectors = Level->sectors.Size();
	TArray<unsigned> linesDoneInEachSector(numsectors, true);
	memset (linesDoneInEachSector.Data(), 0, sizeof(int)*numsectors);

	sector = &Level->sectors[0];
	for (unsigned i = 0; i < numsectors; i++, sector++)
	{
		if (sector->Lines.Count == 0)
		{
			Printf ("Sector %i (tag %i) has no lines\n", i, Level->GetFirstSectorTag(Index(sector)));
			// 0 the sector's tag so that no specials can use it
			Level->tagManager.RemoveSectorTags(i);
		}
		else
		{
			sector->Lines.Array = lineb_p;
			lineb_p += sector->Lines.Count;
		}
	}

	for (unsigned i = 0; i < Level->lines.Size(); i++)
	{
		auto li = &Level->lines[i];
		if (li->frontsector != nullptr)
		{
			li->frontsector->Lines[linesDoneInEachSector[Index(li->frontsector)]++] = li;
		}
		if (li->backsector != nullptr && li->backsector != li->frontsector)
		{
			li->backsector->Lines[linesDoneInEachSector[Index(li->backsector)]++] = li;
		}
	}
	
	sector = &Level->sectors[0];
	for (unsigned i = 0; i < numsectors; ++i, ++sector)
	{
		if (linesDoneInEachSector[i] != sector->Lines.Size())
		{
			I_Error("P_GroupLines: miscounted");
		}
		if (sector->Lines.Size() > 3)
		{
			bbox.ClearBox();
			for (auto li : sector->Lines)
			{
				bbox.AddToBox(li->v1->fPos());
				bbox.AddToBox(li->v2->fPos());
			}

			// set the center to the middle of the bounding box
			sector->centerspot.X = (bbox.Right() + bbox.Left()) / 2;
			sector->centerspot.Y = (bbox.Top() + bbox.Bottom()) / 2;
		}
		else if (sector->Lines.Size() > 0)
		{
			// For triangular sectors the above does not calculate good points unless the longest of the triangle's lines is perfectly horizontal and vertical
			DVector2 pos = { 0,0 };
			for (auto ln : sector->Lines)
			{
				pos += ln->v1->fPos() + ln->v2->fPos();
			}
			sector->centerspot = pos / (2 * sector->Lines.Size());
		}
	}

	// killough 1/30/98: Create xref tables for tags
	Level->tagManager.HashTags();

	if (!buildmap)
	{
		SetSlopes ();
	}
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::LoadReject (MapData * map, bool junk)
{
	const int neededsize = (Level->sectors.Size() * Level->sectors.Size() + 7) >> 3;
	int rejectsize;

	if (!map->CheckName(ML_REJECT, "REJECT"))
	{
		rejectsize = 0;
	}
	else
	{
		rejectsize = junk ? 0 : map->Size(ML_REJECT);
	}

	if (rejectsize < neededsize)
	{
		if (rejectsize > 0)
		{
			Printf ("REJECT is %d byte%s too small.\n", neededsize - rejectsize,
				neededsize-rejectsize==1?"":"s");
		}
		Level->rejectmatrix.Reset();
	}
	else
	{
		// Check if the reject has some actual content. If not, free it.
		rejectsize = MIN (rejectsize, neededsize);
		Level->rejectmatrix.Alloc(rejectsize);

		map->Read (ML_REJECT, &Level->rejectmatrix[0], rejectsize);

		int qwords = rejectsize / 8;
		int i;

		if (qwords > 0)
		{
			const uint64_t *qreject = (const uint64_t *)&Level->rejectmatrix[0];

			i = 0;
			do
			{
				if (qreject[i] != 0)
					return;
			} while (++i < qwords);
		}
		rejectsize &= 7;
		qwords *= 8;
		for (i = 0; i < rejectsize; ++i)
		{
			if (Level->rejectmatrix[qwords + i] != 0)
				return;
		}

		// Reject has no data, so pretend it isn't there.
		Level->rejectmatrix.Reset();
	}
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::LoadBehavior(MapData * map)
{
	if (map->Size(ML_BEHAVIOR) > 0)
	{
		Level->Behaviors.LoadModule(-1, &map->Reader(ML_BEHAVIOR), map->Size(ML_BEHAVIOR), map->lumpnum);
	}
	if (!Level->Behaviors.CheckAllGood())
	{
		Printf("ACS scripts unloaded.\n");
		Level->Behaviors.UnloadModules();
	}
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::GetPolySpots (MapData * map, TArray<FNodeBuilder::FPolyStart> &spots, TArray<FNodeBuilder::FPolyStart> &anchors)
{
	//if (map->HasBehavior)
	{
		for (unsigned int i = 0; i < MapThingsConverted.Size(); ++i)
		{
			FDoomEdEntry *mentry = MapThingsConverted[i].info;
			if (mentry != nullptr && mentry->Type == nullptr && mentry->Special >= SMT_PolyAnchor && mentry->Special <= SMT_PolySpawnHurt)
			{
				FNodeBuilder::FPolyStart newvert;
				newvert.x = FLOAT2FIXED(MapThingsConverted[i].pos.X);
				newvert.y = FLOAT2FIXED(MapThingsConverted[i].pos.Y);
				newvert.polynum = MapThingsConverted[i].angle;
				if (mentry->Special == SMT_PolyAnchor)
				{
					anchors.Push (newvert);
				}
				else
				{
					spots.Push (newvert);
				}
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void MapLoader::CalcIndices()
{
	// sectornums were already initialized because some init code needs them.
	for (unsigned int i = 0; i < Level->vertexes.Size(); ++i)
	{
		Level->vertexes[i].vertexnum = i;
	}
	for (unsigned int i = 0; i < Level->lines.Size(); ++i)
	{
		Level->lines[i].linenum = i;
	}
	for (unsigned int i = 0; i < Level->sides.Size(); ++i)
	{
		Level->sides[i].sidenum = i;
	}
	for (unsigned int i = 0; i < Level->segs.Size(); ++i)
	{
		Level->segs[i].segnum = i;
	}
	for (unsigned int i = 0; i < Level->subsectors.Size(); ++i)
	{
		Level->subsectors[i].subsectornum = i;
	}
	for (unsigned int i = 0; i < Level->nodes.Size(); ++i)
	{
		Level->nodes[i].nodenum = i;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void MapLoader::LoadLevel(MapData *map, const char *lumpname, int position)
{
	const int *oldvertextable  = nullptr;

	// note: most of this ordering is important 
	ForceNodeBuild = gennodes;

	// [RH] Load in the BEHAVIOR lump
	if (map->HasBehavior)
	{
		LoadBehavior(map);
		Level->maptype = MAPTYPE_HEXEN;
	}
	else
	{
		// We need translators only for Doom format maps.
		const char *translator;

		if (!Level->info->Translator.IsEmpty())
		{
			// The map defines its own translator.
			translator = Level->info->Translator.GetChars();
		}
		else
		{
			// Has the user overridden the game's default translator with a commandline parameter?
			translator = Args->CheckValue("-xlat");
			if (translator == nullptr)
			{
				// Use the game's default.
				translator = gameinfo.translator.GetChars();
			}
		}
		Level->Translator = P_LoadTranslator(translator);
		Level->maptype = MAPTYPE_DOOM;
	}
	if (map->isText)
	{
		Level->maptype = MAPTYPE_UDMF;
	}
	FName checksum = CheckCompatibility(map);
	if (Level->ib_compatflags & BCOMPATF_REBUILDNODES)
	{
		ForceNodeBuild = true;
	}
	T_LoadScripts(Level, map);

	if (!map->HasBehavior || map->isText)
	{
		// Doom format and UDMF text maps get strict monster activation unless the mapinfo
		// specifies differently.
		if (!(Level->flags2 & LEVEL2_LAXACTIVATIONMAPINFO))
		{
			Level->flags2 &= ~LEVEL2_LAXMONSTERACTIVATION;
		}
	}

	if (!map->HasBehavior && !map->isText)
	{
		Level->flags2 |= LEVEL2_DUMMYSWITCHES;
	}

	Level->Behaviors.LoadDefaultModules();
	LoadMapinfoACSLump();


	LoadStrifeConversations(map, lumpname);

	FMissingTextureTracker missingtex;

	if (!map->isText)
	{
		LoadVertexes(map);

		// Check for maps without any BSP data at all (e.g. SLIGE)
		LoadSectors(map, missingtex);

		if (!map->HasBehavior)
			LoadLineDefs(map);
		else
			LoadLineDefs2(map);	// [RH] Load Hexen-style linedefs

		LoadSideDefs2(map, missingtex);

		FinishLoadingLineDefs();

		if (!map->HasBehavior)
			LoadThings(map);
		else
			LoadThings2(map);	// [RH] Load Hexen-style things
	}
	else
	{
		ParseTextMap(map, missingtex);
	}

	CalcIndices();
	PostProcessLevel(checksum);

	LoopSidedefs(true);

	SummarizeMissingTextures(missingtex);
	bool reloop = false;

	if (!ForceNodeBuild)
	{
		// Check for compressed nodes first, then uncompressed nodes
		FileReader *fr = nullptr;
		uint32_t id = MAKE_ID('X', 'x', 'X', 'x'), idcheck = 0, idcheck2 = 0, idcheck3 = 0, idcheck4 = 0, idcheck5 = 0, idcheck6 = 0;

		if (map->Size(ML_ZNODES) != 0)
		{
			// Test normal nodes first
			fr = &map->Reader(ML_ZNODES);
			idcheck = MAKE_ID('Z', 'N', 'O', 'D');
			idcheck2 = MAKE_ID('X', 'N', 'O', 'D');
		}
		else if (map->Size(ML_GLZNODES) != 0)
		{
			fr = &map->Reader(ML_GLZNODES);
			idcheck = MAKE_ID('Z', 'G', 'L', 'N');
			idcheck2 = MAKE_ID('Z', 'G', 'L', '2');
			idcheck3 = MAKE_ID('Z', 'G', 'L', '3');
			idcheck4 = MAKE_ID('X', 'G', 'L', 'N');
			idcheck5 = MAKE_ID('X', 'G', 'L', '2');
			idcheck6 = MAKE_ID('X', 'G', 'L', '3');
		}

		bool NodesLoaded = false;
		if (fr != nullptr && fr->isOpen()) fr->Read(&id, 4);
		if (id != 0 && (id == idcheck || id == idcheck2 || id == idcheck3 || id == idcheck4 || id == idcheck5 || id == idcheck6))
		{
			NodesLoaded = LoadExtendedNodes(*fr, id);
		}
		else if (!map->isText)	// regular nodes are not supported for text maps
		{
			// If all 3 node related lumps are empty there's no need to output a message.
			// This just means that the map has no nodes and the engine is supposed to build them.
			if (map->Size(ML_SEGS) != 0 || map->Size(ML_SSECTORS) != 0 || map->Size(ML_NODES) != 0)
			{
				if (!P_CheckV4Nodes(map))
				{
					NodesLoaded = LoadSubsectors<mapsubsector_t, mapseg_t>(map) &&
						LoadNodes<mapnode_t, mapsubsector_t>(map) &&
					 	LoadSegs<mapseg_t>(map);
				}
				else
				{
					NodesLoaded = LoadSubsectors<mapsubsector4_t, mapseg4_t>(map) &&
						LoadNodes<mapnode4_t, mapsubsector4_t>(map) &&
						LoadSegs<mapseg4_t>(map);
				}
			}
		}

		// If loading the regular nodes failed try GL nodes before considering a rebuild
		if (!NodesLoaded)
		{
			if (LoadGLNodes(map))
				reloop = true;
			else
				ForceNodeBuild = true;
		}
	}
	else reloop = true;

	uint64_t startTime = 0, endTime = 0;

	bool BuildGLNodes;

	// The node builder needs these indices.
	for (unsigned int i = 0; i < Level->sides.Size(); ++i)
	{
		Level->sides[i].sidenum = i;
	}

	if (ForceNodeBuild)
	{
		BuildGLNodes = true;
		// In case the compatibility handler made changes to the map's layout
		for(auto &line : Level->lines)
		{
			line.AdjustLine();
		}

		startTime = I_msTime();
		TArray<FNodeBuilder::FPolyStart> polyspots, anchors;
		GetPolySpots(map, polyspots, anchors);
		FNodeBuilder::FLevel leveldata =
		{
			&Level->vertexes[0], (int)Level->vertexes.Size(),
			&Level->sides[0], (int)Level->sides.Size(),
			&Level->lines[0], (int)Level->lines.Size(),
			0, 0, 0, 0
		};
		leveldata.FindMapBounds();

		FNodeBuilder builder(leveldata, polyspots, anchors, BuildGLNodes);
		builder.Extract(*Level);
		endTime = I_msTime();
		DPrintf(DMSG_NOTIFY, "BSP generation took %.3f sec (%d segs)\n", (endTime - startTime) * 0.001, Level->segs.Size());
		oldvertextable = builder.GetOldVertexTable();
		reloop = true;
	}
	else
	{
		BuildGLNodes = false;
		// Older ZDBSPs had problems with compressed sidedefs and assigned wrong sides to the segs if both sides were the same sidedef.
		for (auto &seg : Level->segs)
		{
			if (seg.backsector == seg.frontsector && seg.linedef)
			{
				double d1 = (seg.v1->fPos() - seg.linedef->v1->fPos()).LengthSquared();
				double d2 = (seg.v2->fPos() - seg.linedef->v1->fPos()).LengthSquared();

				if (d2 < d1)	// backside
				{
					seg.sidedef = seg.linedef->sidedef[1];
				}
				else	// front side
				{
					seg.sidedef = seg.linedef->sidedef[0];
				}
			}
		}
	}

	// Build GL nodes if we want a textured automap or GL nodes are forced to be built.
	// If the original nodes being loaded are not GL nodes they will be kept around for
	// use in P_PointInSubsector to avoid problems with maps that depend on the specific
	// nodes they were built with (P:AR E1M3 is a good example for a map where this is the case.)
	reloop |= CheckNodes(map, BuildGLNodes, (uint32_t)(endTime - startTime));
	
	// set the head node for gameplay purposes. If the separate gamenodes array is not empty, use that, otherwise use the render nodes.
	Level->headgamenode = Level->gamenodes.Size() > 0 ? &Level->gamenodes[Level->gamenodes.Size() - 1] : Level->nodes.Size() ? &Level->nodes[Level->nodes.Size() - 1] : nullptr;

	LoadBlockMap(map);

	LoadReject(map, false);
	GroupLines(false);
	FloodZones();
	SetRenderSector();
	FixMinisegReferences();
	FixHoles();

	// Create the item indices, after the last function which may change the data has run.
	CalcIndices();

	Level->bodyqueslot = 0;
	// phares 8/10/98: Clear body queue so the corpses from previous games are
	// not assumed to be from this one.

	for (auto & p : Level->bodyque)
		p = nullptr;

	CreateSections(Level);

	// [RH] Spawn slope creating things first.
	SpawnSlopeMakers(&MapThingsConverted[0], &MapThingsConverted[MapThingsConverted.Size()], oldvertextable);
	CopySlopes();

	// Spawn 3d floors - must be done before spawning things so it can't be done in P_SpawnSpecials
	Spawn3DFloors();

	SpawnThings(position);

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (Level->PlayerInGame(i) && Level->Players[i]->mo != nullptr)
			Level->Players[i]->health = Level->Players[i]->mo->health;
	}
	if (!map->HasBehavior && !map->isText)
		TranslateTeleportThings();	// [RH] Assign teleport destination TIDs

	if (oldvertextable != nullptr)
	{
		delete[] oldvertextable;
	}

	// set up world state
	SpawnSpecials();

	// disable reflective planes on sloped sectors.
	for (auto &sec : Level->sectors)
	{
		if (sec.floorplane.isSlope()) sec.reflect[sector_t::floor] = 0;
		if (sec.ceilingplane.isSlope()) sec.reflect[sector_t::ceiling] = 0;

		sec.CheckExColorFlag();
	}
	for (auto &node : Level->nodes)
	{
		double fdx = FIXED2DBL(node.dx);
		double fdy = FIXED2DBL(node.dy);
		node.len = (float)g_sqrt(fdx * fdx + fdy * fdy);
	}

	InitRenderInfo();				// create hardware independent renderer resources for the level. This must be done BEFORE the PolyObj Spawn!!!
	Level->ClearDynamic3DFloorData();	// CreateVBO must be run on the plain 3D floor data.
	CreateVBO(screen->mVertexData, Level->sectors);

	for (auto &sec : Level->sectors)
	{
		P_Recalculate3DFloors(&sec);
	}

	SWRenderer->SetColormap(Level);	//The SW renderer needs to do some special setup for the level's default colormap.
	InitPortalGroups(Level);
	P_InitHealthGroups(Level);

	if (reloop) LoopSidedefs(false);
	PO_Init();				// Initialize the polyobjs
	if (!Level->IsReentering())
		Level->FinalizePortals();	// finalize line portals after polyobjects have been initialized. This info is needed for properly flagging them.

	Level->aabbTree = new DoomLevelAABBTree(Level);
}


/*
 ** g_dumpinfo.cpp
 ** diagnostic CCMDs that output info about the current game
 **
 **---------------------------------------------------------------------------
 ** Copyright 1998-2016 Randy Heit
 ** Copyright 2003-2018 Christoph Oelckers
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

#include "c_dispatch.h"
#include "g_levellocals.h"
#include "a_dynlight.h"
#include "a_sharedglobal.h"
#include "d_net.h"
#include "p_setup.h"
#include "filesystem.h"
#include "v_text.h"
#include "c_functions.h"
#include "gstrings.h"
#include "texturemanager.h"

//==========================================================================
//
// CCMDs
//
//==========================================================================

CCMD(listlights)
{
	int walls, sectors;
	int allwalls=0, allsectors=0, allsubsecs = 0;
	int i=0, shadowcount = 0;
	FDynamicLight * dl;
	
	for (auto Level : AllLevels())
	{
		Printf("Lights for %s\n", Level->MapName.GetChars());
		for (dl = Level->lights; dl; dl = dl->next)
		{
			walls=0;
			sectors=0;
			Printf("%s at (%f, %f, %f), color = 0x%02x%02x%02x, radius = %f %s %s",
				   dl->target->GetClass()->TypeName.GetChars(),
				   dl->X(), dl->Y(), dl->Z(), dl->GetRed(), dl->GetGreen(), dl->GetBlue(),
				   dl->radius, dl->IsAttenuated()? "attenuated" : "", dl->shadowmapped? "shadowmapped" : "");
			i++;
			shadowcount += dl->shadowmapped;
			
			if (dl->target)
			{
				FTextureID spr = sprites[dl->target->sprite].GetSpriteFrame(dl->target->frame, 0, 0., nullptr);
				Printf(", frame = %s ", TexMan.GetGameTexture(spr)->GetName().GetChars());
			}
			
			
			FLightNode * node;
			
			node=dl->touching_sides;
			
			while (node)
			{
				walls++;
				allwalls++;
				node = node->nextTarget;
			}
			
			
			node = dl->touching_sector;
			
			while (node)
			{
				allsectors++;
				sectors++;
				node = node->nextTarget;
			}
			Printf("- %d walls, %d sectors\n", walls, sectors);
			
		}
		Printf("%i dynamic lights, %d shadowmapped, %d walls, %d sectors\n\n\n", i, shadowcount, allwalls, allsectors);
	}
}

CCMD (countdecals)
{
	for (auto Level : AllLevels())
	{
		auto iterator = Level->GetThinkerIterator<DImpactDecal>(NAME_None, STAT_AUTODECAL);
		int count = 0;
		
		while (iterator.Next())
			count++;
		
		Printf("%s: Counted %d impact decals, level counter is at %d\n", Level->MapName.GetChars(), count, Level->ImpactDecalCount);
	}
}

CCMD (spray)
{
	if (players[consoleplayer].mo == NULL || argv.argc() < 2)
	{
		Printf ("Usage: spray <decal>\n");
		return;
	}
	
	Net_WriteByte (DEM_SPRAY);
	Net_WriteString (argv[1]);
}

//==========================================================================
//
// CCMD mapchecksum
//
//==========================================================================

CCMD (mapchecksum)
{
	MapData *map;
	uint8_t cksum[16];
	
	if (argv.argc() < 2)
	{
		Printf("Usage: mapchecksum <map> ...\n");
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		map = P_OpenMapData(argv[i], true);
		if (map == NULL)
		{
			Printf("Cannot load %s as a map\n", argv[i]);
		}
		else
		{
			map->GetChecksum(cksum);
			const char *wadname = fileSystem.GetResourceFileName(fileSystem.GetFileContainer(map->lumpnum));
			delete map;
			for (size_t j = 0; j < sizeof(cksum); ++j)
			{
				Printf("%02X", cksum[j]);
			}
			Printf(" // %s %s\n", wadname, argv[i]);
		}
	}
}

//==========================================================================
//
// CCMD hiddencompatflags
//
//==========================================================================

CCMD (hiddencompatflags)
{
	for(auto Level : AllLevels())
	{
		Printf("%s: %08x %08x %08x\n", Level->MapName.GetChars(), Level->ii_compatflags, Level->ii_compatflags2, Level->ib_compatflags);
	}
}

CCMD(dumpportals)
{
	for (auto Level : AllLevels())
	{
		Printf("Portal groups for %s\n", Level->MapName.GetChars());
		for (unsigned i = 0; i < Level->portalGroups.Size(); i++)
		{
			auto p = Level->portalGroups[i];
			double xdisp = p->mDisplacement.X;
			double ydisp = p->mDisplacement.Y;
			Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Portal #%d, %s, displacement = (%f,%f)\n", i, p->plane == 0 ? "floor" : "ceiling",
				   xdisp, ydisp);
			Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Coverage:\n");
			for (auto &sub : Level->subsectors)
			{
				auto port = sub.render_sector->GetPortalGroup(p->plane);
				if (port == p)
				{
					Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "\tSubsector %d (%d):\n\t\t", sub.Index(), sub.render_sector->sectornum);
					for (unsigned k = 0; k < sub.numlines; k++)
					{
						Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "(%.3f,%.3f), ", sub.firstline[k].v1->fX() + xdisp, sub.firstline[k].v1->fY() + ydisp);
					}
					Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "\n\t\tCovered by subsectors:\n");
					FPortalCoverage *cov = &sub.portalcoverage[p->plane];
					for (int l = 0; l < cov->sscount; l++)
					{
						subsector_t *csub = &Level->subsectors[cov->subsectors[l]];
						Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "\t\t\t%5d (%4d): ", cov->subsectors[l], csub->render_sector->sectornum);
						for (unsigned m = 0; m < csub->numlines; m++)
						{
							Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "(%.3f,%.3f), ", csub->firstline[m].v1->fX(), csub->firstline[m].v1->fY());
						}
						Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "\n");
					}
				}
			}
		}
	}
}

ADD_STAT (interpolations)
{
	FString out;
	for (auto Level : AllLevels())
	{
		if (out.Len() > 0) out << '\n';
		out.AppendFormat("%s: %d interpolations", Level->MapName.GetChars(), Level->interpolator.CountInterpolations ());
		
	}
	return out;
}

CCMD(printsections)
{
	void PrintSections(FLevelLocals *Level);
	for (auto Level : AllLevels())
	{
		PrintSections(Level);
	}
}

CCMD(dumptags)
{
	for (auto Level : AllLevels())
	{
		Level->tagManager.DumpTags();
	}
}



CCMD(dump3df)
{
	if (argv.argc() > 1)
	{
		// Print 3D floor info for a single sector.
		// This only checks the primary level.
		int sec = (int)strtoll(argv[1], NULL, 10);
		if ((unsigned)sec >= primaryLevel->sectors.Size())
		{
			Printf("Sector %d does not exist.\n", sec);
			return;
		}
		sector_t *sector = &primaryLevel->sectors[sec];
		TArray<F3DFloor*> & ffloors = sector->e->XFloor.ffloors;

		for (unsigned int i = 0; i < ffloors.Size(); i++)
		{
			double height = ffloors[i]->top.plane->ZatPoint(sector->centerspot);
			double bheight = ffloors[i]->bottom.plane->ZatPoint(sector->centerspot);

			IGNORE_FORMAT_PRE
				Printf("FFloor %d @ top = %f (model = %d), bottom = %f (model = %d), flags = %B, alpha = %d %s %s\n",
					i, height, ffloors[i]->top.model->sectornum,
					bheight, ffloors[i]->bottom.model->sectornum,
					ffloors[i]->flags, ffloors[i]->alpha, (ffloors[i]->flags&FF_EXISTS) ? "Exists" : "", (ffloors[i]->flags&FF_DYNAMIC) ? "Dynamic" : "");
			IGNORE_FORMAT_POST
		}
	}
}

//============================================================================
//
// print the group link table to the console
//
//============================================================================

CCMD(dumplinktable)
{
	for (auto Level : AllLevels())
	{
		Printf("Portal displacements for %s:\n", Level->MapName.GetChars());
		for (int x = 1; x < Level->Displacements.size; x++)
		{
			for (int y = 1; y < Level->Displacements.size; y++)
			{
				FDisplacement &disp = Level->Displacements(x, y);
				Printf("%c%c(%6d, %6d)", TEXTCOLOR_ESCAPE, 'C' + disp.indirect, int(disp.pos.X), int(disp.pos.Y));
			}
			Printf("\n");
		}
	}
}


//===========================================================================
//
// CCMD printinv
//
// Prints the console player's current inventory.
//
//===========================================================================

CCMD(printinv)
{
	int pnum = consoleplayer;

#ifdef _DEBUG
	// Only allow peeking on other players' inventory in debug builds.
	if (argv.argc() > 1)
	{
		pnum = atoi(argv[1]);
		if (pnum < 0 || pnum >= MAXPLAYERS)
		{
			return;
		}
	}
#endif
	C_PrintInv(players[pnum].mo);
}

CCMD(targetinv)
{
	FTranslatedLineTarget t;

	if (CheckCheatmode() || players[consoleplayer].mo == NULL)
		return;

	C_AimLine(&t, true);

	if (t.linetarget)
	{
		C_PrintInv(t.linetarget);
	}
	else Printf("No target found. Targetinv cannot find actors that have "
		"the NOBLOCKMAP flag or have height/radius of 0.\n");
}


//==========================================================================
//
// Lists all currently defined maps
//
//==========================================================================

CCMD(listmaps)
{
	for (unsigned i = 0; i < wadlevelinfos.Size(); i++)
	{
		level_info_t *info = &wadlevelinfos[i];
		MapData *map = P_OpenMapData(info->MapName, true);

		if (map != NULL)
		{
			if (argv.argc() == 1 
			    || CheckWildcards(argv[1], info->MapName.GetChars()) 
			    || CheckWildcards(argv[1], info->LookupLevelName().GetChars())
			    || CheckWildcards(argv[1], fileSystem.GetResourceFileName(fileSystem.GetFileContainer(map->lumpnum))))
			{
				Printf("%s: '%s' (%s)\n", info->MapName.GetChars(), info->LookupLevelName().GetChars(),
					fileSystem.GetResourceFileName(fileSystem.GetFileContainer(map->lumpnum)));
			}
			delete map;
		}
	}
}

//==========================================================================
//
// For testing sky fog sheets
//
//==========================================================================
CCMD(skyfog)
{
	if (argv.argc() > 1)
	{
		// Do this only on the primary level.
		primaryLevel->skyfog = MAX(0, (int)strtoull(argv[1], NULL, 0));
	}
}


//==========================================================================
//
//
//==========================================================================

CCMD(listsnapshots)
{
	for (unsigned i = 0; i < wadlevelinfos.Size(); ++i)
	{
		FCompressedBuffer *snapshot = &wadlevelinfos[i].Snapshot;
		if (snapshot->mBuffer != nullptr)
		{
			Printf("%s (%u -> %u bytes)\n", wadlevelinfos[i].MapName.GetChars(), snapshot->mCompressedSize, snapshot->mSize);
		}
	}
}

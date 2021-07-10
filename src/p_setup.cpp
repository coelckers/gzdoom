//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
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
//
//-----------------------------------------------------------------------------


#include <math.h>
#ifdef _MSC_VER
#include <malloc.h>		// for alloca()
#endif

#include "templates.h"
#include "d_player.h"
#include "m_argv.h"
#include "g_game.h"
#include "filesystem.h"
#include "p_local.h"
#include "p_effect.h"
#include "p_terrain.h"
#include "nodebuild.h"
#include "p_lnspec.h"
#include "c_console.h"
#include "p_acs.h"
#include "announcer.h"
#include "wi_stuff.h"
#include "engineerrors.h"
#include "gi.h"
#include "p_conversation.h"
#include "a_keys.h"
#include "s_sndseq.h"
#include "sbar.h"
#include "p_setup.h"
#include "r_data/r_interpolate.h"
#include "r_sky.h"
#include "cmdlib.h"
#include "md5.h"
#include "po_man.h"
#include "swrenderer/r_renderer.h"
#include "p_blockmap.h"
#include "r_utility.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "c_dispatch.h"
#include "a_dynlight.h"
#include "events.h"
#include "p_destructible.h"
#include "types.h"
#include "i_time.h"
#include "vm.h"
#include "a_specialspot.h"
#include "maploader/maploader.h"
#include "p_acs.h"
#include "am_map.h"
#include "i_system.h"
#include "v_video.h"
#include "fragglescript/t_script.h"
#include "s_music.h"
#include "animations.h"
#include "texturemanager.h"
#include "p_lnspec.h"
#include "d_main.h"

extern AActor *SpawnMapThing (int index, FMapThing *mthing, int position);

extern unsigned int R_OldBlend;


//===========================================================================
//
// P_PrecacheLevel
//
// Preloads all relevant graphics for the Level->
//
//===========================================================================
void hw_PrecacheTexture(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist);

static void AddToList(uint8_t *hitlist, FTextureID texid, int bitmask)
{
	if (hitlist[texid.GetIndex()] & bitmask) return;	// already done, no need to process everything again.
	hitlist[texid.GetIndex()] |= (uint8_t)bitmask;

	const auto addAnimations = [hitlist, bitmask](const FTextureID texid)
	{
		for (auto anim : TexAnim.GetAnimations())
		{
			if (texid == anim->BasePic || (!anim->bDiscrete && anim->BasePic < texid && texid < anim->BasePic + anim->NumFrames))
			{
				for (int i = anim->BasePic.GetIndex(); i < anim->BasePic.GetIndex() + anim->NumFrames; i++)
				{
					hitlist[i] |= (uint8_t)bitmask;
				}
			}
		}
	};

	addAnimations(texid);

	auto switchdef = TexAnim.FindSwitch(texid);
	if (switchdef)
	{
		const FSwitchDef *const pair = switchdef->PairDef;
		const uint16_t numFrames = switchdef->NumFrames;
		const uint16_t pairNumFrames = pair->NumFrames;

		for (int i = 0; i < numFrames; i++)
		{
			hitlist[switchdef->frames[i].Texture.GetIndex()] |= (uint8_t)bitmask;
		}
		for (int i = 0; i < pairNumFrames; i++)
		{
			hitlist[pair->frames[i].Texture.GetIndex()] |= (uint8_t)bitmask;
		}

		if (numFrames == 1 && pairNumFrames == 1)
		{
			// Switch can still be animated via BOOM binary definition from ANIMATED lump
			addAnimations(switchdef->frames[0].Texture);
			addAnimations(pair->frames[0].Texture);
		}
	}

	auto adoor = TexAnim.FindAnimatedDoor(texid);
	if (adoor)
	{
		for (int i = 0; i < adoor->NumTextureFrames; i++)
		{
			hitlist[adoor->TextureFrames[i].GetIndex()] |= (uint8_t)bitmask;
		}
	}
}

static void PrecacheLevel(FLevelLocals *Level)
{
	if (demoplayback)
		return;

	int i;
	TMap<PClassActor *, bool> actorhitlist;
	int cnt = TexMan.NumTextures();
	TArray<uint8_t> hitlist(cnt, true);

	memset(hitlist.Data(), 0, cnt);

	AActor *actor;
	auto iterator = Level->GetThinkerIterator<AActor>();

	while ((actor = iterator.Next()))
	{
		actorhitlist[actor->GetClass()] = true;
	}

	for (auto n : gameinfo.PrecachedClasses)
	{
		PClassActor *cls = PClass::FindActor(n);
		if (cls != nullptr) actorhitlist[cls] = true;
	}
	for (unsigned i = 0; i < Level->info->PrecacheClasses.Size(); i++)
	{
		// Level->info can only store names, no class pointers.
		PClassActor *cls = PClass::FindActor(Level->info->PrecacheClasses[i]);
		if (cls != nullptr) actorhitlist[cls] = true;
	}

	for (i = Level->sectors.Size() - 1; i >= 0; i--)
	{
		AddToList(hitlist.Data(), Level->sectors[i].GetTexture(sector_t::floor), FTextureManager::HIT_Flat);
		AddToList(hitlist.Data(), Level->sectors[i].GetTexture(sector_t::ceiling), FTextureManager::HIT_Flat);
	}

	for (i = Level->sides.Size() - 1; i >= 0; i--)
	{
		auto &sd = Level->sides[i];
		int hitflag = FTextureManager::HIT_Wall;
		// Only precache skyboxes when this is actually used as a sky transfer.
		if (sd.linedef->sidedef[0] == &sd && sd.linedef->special == Static_Init && sd.linedef->args[1] == Init_TransferSky) hitflag |= FTextureManager::HIT_Sky;
		AddToList(hitlist.Data(), sd.GetTexture(side_t::top), hitflag);
		AddToList(hitlist.Data(), sd.GetTexture(side_t::mid), FTextureManager::HIT_Wall);
		AddToList(hitlist.Data(), sd.GetTexture(side_t::bottom), hitflag);
	}


	if (Level->skytexture1.isValid())
	{
		AddToList(hitlist.Data(), Level->skytexture1, FTextureManager::HIT_Sky);
	}
	if (Level->skytexture2.isValid())
	{
		AddToList(hitlist.Data(), Level->skytexture2, FTextureManager::HIT_Sky);
	}

	for (auto n : gameinfo.PrecachedTextures)
	{
		FTextureID tex = TexMan.CheckForTexture(n, ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
		if (tex.Exists()) AddToList(hitlist.Data(), tex, FTextureManager::HIT_Wall);
	}
	for (unsigned i = 0; i < Level->info->PrecacheTextures.Size(); i++)
	{
		FTextureID tex = TexMan.CheckForTexture(Level->info->PrecacheTextures[i], ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
		if (tex.Exists()) AddToList(hitlist.Data(), tex, FTextureManager::HIT_Wall);
	}

	// This is just a temporary solution, until the hardware renderer's texture manager is in a better state.
	if (!V_IsHardwareRenderer())
		SWRenderer->Precache(hitlist.Data(), actorhitlist);
	else
		hw_PrecacheTexture(hitlist.Data(), actorhitlist);

}

//============================================================================
//
// clears all portal data for a new level start
//
//============================================================================

void FLevelLocals::ClearPortals()
{
	Displacements.Create(1);
	linePortals.Clear();
	linkedPortals.Clear();
	sectorPortals.Resize(2);
	PortalBlockmap.Clear();

	// The first entry must always be the default skybox. This is what every sector gets by default.
	memset(&sectorPortals[0], 0, sizeof(sectorPortals[0]));
	sectorPortals[0].mType = PORTS_SKYVIEWPOINT;
	sectorPortals[0].mFlags = PORTSF_SKYFLATONLY;
	// The second entry will be the default sky. This is for forcing a regular sky through the skybox picker
	memset(&sectorPortals[1], 0, sizeof(sectorPortals[0]));
	sectorPortals[1].mType = PORTS_SKYVIEWPOINT;
	sectorPortals[1].mFlags = PORTSF_SKYFLATONLY;

	// also clear the render data
	for (auto &sub : subsectors)
	{
		for (int j = 0; j < 2; j++)
		{
			if (sub.portalcoverage[j].subsectors != nullptr)
			{
				delete[] sub.portalcoverage[j].subsectors;
				sub.portalcoverage[j].subsectors = nullptr;
			}
		}
	}
	for (unsigned i = 0; i < portalGroups.Size(); i++)
	{
		delete portalGroups[i];
	}
	portalGroups.Clear();
	linePortalSpans.Clear();
}

//==========================================================================
//
//
//
//==========================================================================

void FLevelLocals::ClearLevelData()
{
	{
		auto it = GetThinkerIterator<AActor>(NAME_None, STAT_TRAVELLING);
		for (AActor *actor = it.Next(); actor != nullptr; actor = it.Next())
		{
			actor->BlockingLine = nullptr;
			actor->BlockingFloor = actor->BlockingCeiling = actor->Blocking3DFloor = nullptr;
		}
	}
	
	interpolator.ClearInterpolations();	// [RH] Nothing to interpolate on a fresh level.
	Thinkers.DestroyAllThinkers();
	ClearAllSubsectorLinks(); // can't be done as part of the polyobj deletion process.

	total_monsters = total_items = total_secrets =
	killed_monsters = found_items = found_secrets = 0;

	max_velocity = avg_velocity = 0;

	for (int i = 0; i < 4; i++)
	{
		UDMFKeys[i].Clear();
	}
	
	SN_StopAllSequences(this);

	FStrifeDialogueNode *node;
	
	while (StrifeDialogues.Pop (node))
	{
		delete node;
	}
	
	DialogueRoots.Clear();
	ClassRoots.Clear();

	for (auto &sub : subsectors)
	{
		if (sub.BSP != nullptr) delete sub.BSP;
	}
	ClearPortals();

	tagManager.Clear();
	ClearTIDHashes();
	if (SpotState) SpotState->Destroy();
	SpotState = nullptr;
	ACSThinker = nullptr;
	FraggleScriptThinker = nullptr;
	CorpseQueue.Clear();
	canvasTextureInfo.EmptyList();
	sections.Clear();
	segs.Clear();
	extsectors.Clear();
	sectors.Clear();
	linebuffer.Clear();
	subsectorbuffer.Clear();
	lines.Clear();
	sides.Clear();
	segbuffer.Clear();
	loadsectors.Clear();
	loadlines.Clear();
	loadsides.Clear();
	vertexes.Clear();
	nodes.Clear();
	gamenodes.Reset();
	subsectors.Clear();
	gamesubsectors.Reset();
	rejectmatrix.Clear();
	Zones.Clear();
	blockmap.Clear();
	Polyobjects.Clear();

	for (auto &pb : PolyBlockMap)
	{
		polyblock_t *link = pb;
		while (link != nullptr)
		{
			polyblock_t *next = link->next;
			delete link;
			link = next;
		}
	}
	PolyBlockMap.Reset();

	deathmatchstarts.Clear();
	AllPlayerStarts.Clear();
	memset(playerstarts, 0, sizeof(playerstarts));
	Scrolls.Clear();
	if (automap) automap->Destroy();
	Behaviors.UnloadModules();
	localEventManager->Shutdown();
	if (aabbTree) delete aabbTree;
	aabbTree = nullptr;
	if (screen)
		screen->SetAABBTree(nullptr);
}

//==========================================================================
//
//
//
//==========================================================================

void P_FreeLevelData ()
{
	R_FreePastViewers();

	for (auto Level : AllLevels())
	{
		Level->ClearLevelData();
	}
	// primaryLevel->FreeSecondaryLevels();
}

//===========================================================================
//
// P_SetupLevel
//
// [RH] position indicates the start spot to spawn at
//
//===========================================================================

void P_SetupLevel(FLevelLocals *Level, int position, bool newGame)
{
	int i;

	Level->ShaderStartTime = I_msTimeFS(); // indicate to the shader system that the level just started

	// This is motivated as follows:

	Level->maptype = MAPTYPE_UNKNOWN;

	if (!savegamerestore)
	{
		Level->SetMusicVolume(Level->MusicVolume);
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			Level->Players[i]->killcount = Level->Players[i]->secretcount
				= Level->Players[i]->itemcount = 0;
		}
	}
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		Level->Players[i]->mo = nullptr;
	}
	GPalette.ClearTranslationSlot(TRANSLATION_LevelScripted);


	// Initial height of PointOfView will be set by player think.
	auto p = Level->GetConsolePlayer();
	if (p) p->viewz = NO_VALUE;

	// Make sure all sounds are stopped before Z_FreeTags.
	S_Start();
	S_ResetMusic();

	// Don't start the music if loading a savegame, because the music is stored there.
	// Don't start the music if revisiting a level in a hub for the same reason.
	if (!primaryLevel->IsReentering())
	{
		primaryLevel->SetMusic();
	}

	// [RH] clear out the mid-screen message
	C_MidPrint(nullptr, nullptr);

	// Free all level data from the previous map
	P_FreeLevelData();

	MapData *map = P_OpenMapData(Level->MapName, true);
	if (map == nullptr)
	{
		I_Error("Unable to open map '%s'\n", Level->MapName.GetChars());
	}

	// [ZZ] init per-map static handlers. we need to call this before everything is set up because otherwise scripts don't receive PlayerEntered event
	//      (which happens at god-knows-what stage in this function, but definitely not the last part, because otherwise it'd work to put E_InitStaticHandlers before the player spawning)
	Level->localEventManager->InitStaticHandlers(Level, true);

	// generate a checksum for the level, to be included and checked with savegames.
	map->GetChecksum(Level->md5);
	// find map num
	Level->lumpnum = map->lumpnum;

	if (newGame)
	{
		Level->localEventManager->NewGame();
	}

	MapLoader loader(Level);
	loader.LoadLevel(map, Level->MapName.GetChars(), position);
	delete map;

	// if deathmatch, randomly spawn the active players
	if (deathmatch)
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (Level->PlayerInGame(i))
			{
				Level->Players[i]->mo = nullptr;
				Level->DeathMatchSpawnPlayer(i);
			}
		}
	}
	// the same, but for random single/coop player starts
	else if (Level->flags2 & LEVEL2_RANDOMPLAYERSTARTS)
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (Level->PlayerInGame(i))
			{
				Level->Players[i]->mo = nullptr;
				FPlayerStart *mthing = Level->PickPlayerStart(i);
				Level->SpawnPlayer(mthing, i, (Level->flags2 & LEVEL2_PRERAISEWEAPON) ? SPF_WEAPONFULLYUP : 0);
			}
		}
	}

	// [SP] move unfriendly players around
	// horribly hacky - yes, this needs rewritten.
	if (Level->deathmatchstarts.Size() > 0)
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			auto p = Level->Players[i];
			if (Level->PlayerInGame(i) && p->mo != nullptr)
			{
				if (!(p->mo->flags & MF_FRIENDLY))
				{
					AActor * oldSpawn = p->mo;
					Level->DeathMatchSpawnPlayer(i);
					oldSpawn->Destroy();
				}
			}
		}
	}


	// Don't count monsters in end-of-level sectors if option is on
	if (dmflags2 & DF2_NOCOUNTENDMONST)
	{
		auto it = Level->GetThinkerIterator<AActor>();
		AActor * mo;

		while ((mo = it.Next()))
		{
			if (mo->flags & MF_COUNTKILL)
			{
				if (mo->Sector->damageamount > 0 && (mo->Sector->Flags & (SECF_ENDGODMODE | SECF_ENDLEVEL)) == (SECF_ENDGODMODE | SECF_ENDLEVEL))
				{
					mo->ClearCounters();
				}
			}
		}
	}

	T_PreprocessScripts(Level);        // preprocess FraggleScript scripts

	// build subsector connect matrix
	//	UNUSED P_ConnectSubsectors ();

	R_OldBlend = 0xffffffff;

	// [RH] Remove all particles
	P_ClearParticles(Level);

	// preload graphics and sounds
	if (precache)
	{
		PrecacheLevel(Level);
		S_PrecacheLevel(Level);
	}

	if (deathmatch)
	{
		AnnounceGameStart();
	}

	// This check was previously done at run time each time the heightsec was checked.
	// However, since 3D floors are static data, we can easily precalculate this and store it in the sector's flags for quick access.
	for (auto &s : Level->sectors)
	{
		if (s.heightsec != nullptr)
		{
			// If any of these 3D floors render their planes, ignore heightsec.
			for (auto &ff : s.e->XFloor.ffloors)
			{
				if ((ff->flags & (FF_EXISTS | FF_RENDERPLANES)) == (FF_EXISTS | FF_RENDERPLANES))
				{
					s.MoreFlags |= SECMF_IGNOREHEIGHTSEC;	// mark the heightsec inactive.
				}
			}
		}
	}

	// Create a backup of the map data so the savegame code can toss out all fields that haven't changed in order to reduce processing time and file size.
	// Note that we want binary identity here, so assignment is not sufficient because it won't initialize any padding bytes.
	// Note that none of these structures may contain non POD fields anyway.
	Level->loadsectors.Resize(Level->sectors.Size());
	memcpy(&Level->loadsectors[0], &Level->sectors[0], Level->sectors.Size() * sizeof(Level->sectors[0]));
	Level->loadlines.Resize(Level->lines.Size());
	memcpy(&Level->loadlines[0], &Level->lines[0], Level->lines.Size() * sizeof(Level->lines[0]));
	Level->loadsides.Resize(Level->sides.Size());
	memcpy(&Level->loadsides[0], &Level->sides[0], Level->sides.Size() * sizeof(Level->sides[0]));

	Level->automap = AM_Create(Level);
	Level->automap->LevelInit();
	Level->SetCompatLineOnSide(true);

	// [RH] Start lightning, if MAPINFO tells us to
	if (Level->flags & LEVEL_STARTLIGHTNING)
	{
		Level->StartLightning();
	}

	auto it = Level->GetThinkerIterator<AActor>();
	AActor* ac;
	// Initial setup of the dynamic lights.
	while ((ac = it.Next()))
	{
		ac->SetDynamicLights();
	}
}

//
// P_Init
//
void P_Init ()
{
	P_InitEffects ();		// [RH]
	P_InitTerrainTypes ();
	P_InitKeyMessages ();
	R_InitSprites ();
}

void P_Shutdown ()
{
	for (auto Level : AllLevels())
	{
		Level->Thinkers.DestroyThinkersInList(STAT_STATIC);
	}
	P_FreeLevelData ();
	// [ZZ] delete global event handlers
	staticEventManager.Shutdown();	// clear out the handlers before starting the engine shutdown
	ST_Clear();
	for (auto &p : players)
	{
		if (p.psprites != nullptr) p.psprites->Destroy();
	}
}

//==========================================================================
//
// dumpgeometry
//
//==========================================================================

CCMD(dumpgeometry)
{
	for (auto Level : AllLevels())
	{
		Printf("Geometry for %s\n", Level->MapName.GetChars());
		for (auto &sector : Level->sectors)
		{
			Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "Sector %d\n", sector.sectornum);
			for (int j = 0; j<sector.subsectorcount; j++)
			{
				subsector_t * sub = sector.subsectors[j];
				
				Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "    Subsector %d - real sector = %d - %s\n", int(sub->Index()), sub->sector->sectornum, sub->hacked & 1 ? "hacked" : "");
				for (uint32_t k = 0; k<sub->numlines; k++)
				{
					seg_t * seg = sub->firstline + k;
					if (seg->linedef)
					{
						Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, linedef %d, side %d",
							seg->v1->fX(), seg->v1->fY(), seg->v2->fX(), seg->v2->fY(),
							seg->Index(), seg->linedef->Index(), seg->sidedef != seg->linedef->sidedef[0]);
					}
					else
					{
						Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, miniseg",
							seg->v1->fX(), seg->v1->fY(), seg->v2->fX(), seg->v2->fY(), seg->Index());
					}
					if (seg->PartnerSeg)
					{
						subsector_t * sub2 = seg->PartnerSeg->Subsector;
						Printf(PRINT_DEFAULT, PRINTF_LOGONLY, ", back sector = %d, real back sector = %d", sub2->render_sector->sectornum, seg->PartnerSeg->frontsector->sectornum);
					}
					else if (seg->backsector)
					{
						Printf(PRINT_DEFAULT, PRINTF_LOGONLY, ", back sector = %d (no partnerseg)", seg->backsector->sectornum);
					}
					
					Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "\n");
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

CCMD(listmapsections)
{
	for (auto Level : AllLevels())
	{
		Printf("Map sections for %s:\n", Level->MapName.GetChars());
		for (int i = 0; i < 100; i++)
		{
			for (auto &sub : Level->subsectors)
			{
				if (sub.mapsection == i)
				{
					Printf("Mapsection %d, sector %d, line %d\n", i, sub.render_sector->Index(), sub.firstline->linedef->Index());
					break;
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

CUSTOM_CVAR(Bool, forcewater, false, CVAR_ARCHIVE | CVAR_SERVERINFO)
{
	if (gamestate == GS_LEVEL) for (auto Level : AllLevels())
	{
		for (auto &sec : Level->sectors)
		{
			sector_t *hsec = sec.GetHeightSec();
			if (hsec && !(hsec->MoreFlags & SECMF_UNDERWATER))
			{
				if (self)
				{
					hsec->MoreFlags |= SECMF_FORCEDUNDERWATER;
				}
				else
				{
					hsec->MoreFlags &= ~SECMF_FORCEDUNDERWATER;
				}
			}
		}
	}
}


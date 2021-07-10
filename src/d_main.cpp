//-----------------------------------------------------------------------------
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
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
//
// DESCRIPTION:
//		DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//		plus functions to determine game mode (shareware, registered),
//		parse command line parameters, configure game parameters (turbo),
//		and call the startup functions.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#ifdef _WIN32
#include <direct.h>
#endif

#ifdef HAVE_FPU_CONTROL
#include <fpu_control.h>
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#include <math.h>
#include <assert.h>

#include "engineerrors.h"

#include "i_time.h"
#include "d_gui.h"
#include "m_random.h"
#include "doomdef.h"
#include "doomstat.h"
#include "gstrings.h"
#include "filesystem.h"
#include "s_sound.h"
#include "v_video.h"
#include "intermission/intermission.h"
#include "f_wipe.h"
#include "m_argv.h"
#include "m_misc.h"
#include "menu.h"
#include "doommenu.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "i_sound.h"
#include "i_video.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "p_setup.h"
#include "r_utility.h"
#include "r_sky.h"
#include "d_main.h"
#include "d_dehacked.h"
#include "cmdlib.h"
#include "v_text.h"
#include "gi.h"
#include "a_dynlight.h"
#include "gameconfigfile.h"
#include "sbar.h"
#include "decallib.h"
#include "version.h"
#include "st_start.h"
#include "teaminfo.h"
#include "hardware.h"
#include "sbarinfo.h"
#include "d_net.h"
#include "d_event.h"
#include "d_netinf.h"
#include "m_cheat.h"
#include "m_joy.h"
#include "v_draw.h"
#include "po_man.h"
#include "p_local.h"
#include "autosegs.h"
#include "fragglescript/t_fs.h"
#include "g_levellocals.h"
#include "events.h"
#include "vm.h"
#include "types.h"
#include "i_system.h"
#include "g_cvars.h"
#include "r_data/r_vanillatrans.h"
#include "s_music.h"
#include "swrenderer/r_swcolormaps.h"
#include "findfile.h"
#include "md5.h"
#include "c_buttons.h"
#include "d_buttons.h"
#include "i_interface.h"
#include "animations.h"
#include "texturemanager.h"
#include "formats/multipatchtexture.h"
#include "scriptutil.h"
#include "v_palette.h"
#include "texturemanager.h"
#include "hw_clock.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "doomfont.h"

#ifdef __unix__
#include "i_system.h"  // for SHARE_DIR
#endif // __unix__

EXTERN_CVAR(Bool, hud_althud)
EXTERN_CVAR(Int, vr_mode)
EXTERN_CVAR(Bool, cl_customizeinvulmap)
EXTERN_CVAR(Bool, log_vgafont)
EXTERN_CVAR(Bool, dlg_vgafont)

void DrawHUD();
void D_DoAnonStats();
void I_DetectOS();
void UpdateGenericUI(bool cvar);


// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void I_SetWindowTitle(const char* caption);
extern void ReadStatistics();
extern void M_SetDefaultMode ();
extern void G_NewInit ();
extern void SetupPlayerClasses ();
void DeinitMenus();
void CloseNetwork();
void P_Shutdown();
void M_SaveDefaultsFinal();
void R_Shutdown();
void I_ShutdownInput();
void SetConsoleNotifyBuffer();

const FIWADInfo *D_FindIWAD(TArray<FString> &wadfiles, const char *iwad, const char *basewad);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

bool D_CheckNetGame ();
void D_ProcessEvents ();
void G_BuildTiccmd (ticcmd_t* cmd);
void D_DoAdvanceDemo ();
void D_LoadWadSettings ();
void ParseGLDefs();
void DrawFullscreenSubtitle(const char *text);
void D_Cleanup();
void FreeSBarInfoScript();
void I_UpdateWindowTitle();
void S_ParseMusInfo();

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void D_DoomLoop ();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR (Float, turbo)
EXTERN_CVAR (Bool, freelook)
EXTERN_CVAR (Float, m_pitch)
EXTERN_CVAR (Float, m_yaw)
EXTERN_CVAR (Bool, invertmouse)
EXTERN_CVAR (Bool, invertmousex)
EXTERN_CVAR (Bool, lookstrafe)
EXTERN_CVAR (Int, screenblocks)
EXTERN_CVAR (Bool, sv_cheats)
EXTERN_CVAR (Bool, sv_unlimited_pickup)
EXTERN_CVAR (Bool, r_drawplayersprites)
EXTERN_CVAR (Bool, show_messages)
EXTERN_CVAR(Bool, ticker)

extern bool setmodeneeded;
extern bool demorecording;
bool M_DemoNoPlay;	// [RH] if true, then skip any demos in the loop
extern bool insave;
extern TDeletingArray<FLightDefaults *> LightDefaults;
extern FName MessageBoxClass;

const char* iwad_folders[13] = { "flats/", "textures/", "hires/", "sprites/", "voxels/", "colormaps/", "acs/", "maps/", "voices/", "patches/", "graphics/", "sounds/", "music/" };
const char* iwad_reserved[12] = { "mapinfo", "zmapinfo", "gameinfo", "sndinfo", "sbarinfo", "menudef", "gldefs", "animdefs", "decorate", "zscript", "iwadinfo", "maps/" };


CUSTOM_CVAR(Float, i_timescale, 1.0f, CVAR_NOINITCALL | CVAR_VIRTUAL)
{
	if (netgame)
	{
		Printf("Time scale cannot be changed in net games.\n");
		self = 1.0f;
	}
	else if (self >= 0.05f)
	{
		I_FreezeTime(true);
		TimeScale = self;
		I_FreezeTime(false);
	}
	else
	{
		Printf("Time scale must be at least 0.05!\n");
	}
}


// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Int, vid_rendermode, 4, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0 || self > 4)
	{
		self = 4;
	}
	else if (self == 2 || self == 3)
	{
		self = self - 2; // softpoly to software
	}

	if (usergame)
	{
		// [SP] Update pitch limits to the netgame/gamesim.
		players[consoleplayer].SendPitchLimits();
	}
	screen->SetTextureFilterMode();

	// No further checks needed. All this changes now is which scene drawer the render backend calls.
}

CUSTOM_CVAR (Int, fraglimit, 0, CVAR_SERVERINFO)
{
	// Check for the fraglimit being hit because the fraglimit is being
	// lowered below somebody's current frag count.
	if (deathmatch && self > 0)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && self <= D_GetFragCount(&players[i]))
			{
				Printf ("%s\n", GStrings("TXT_FRAGLIMIT"));
				primaryLevel->ExitLevel (0, false);
				break;
			}
		}
	}
}

CVAR (Float, timelimit, 0.f, CVAR_SERVERINFO);
CVAR (Int, wipetype, 1, CVAR_ARCHIVE);
CVAR (Int, snd_drawoutput, 0, 0);
CUSTOM_CVAR (String, vid_cursor, "None", CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	bool res = false;
	

	if (!stricmp(self, "None" ) && gameinfo.CursorPic.IsNotEmpty())
	{
		res = I_SetCursor(TexMan.GetGameTextureByName(gameinfo.CursorPic));
	}
	else
	{
		res = I_SetCursor(TexMan.GetGameTextureByName(self));
	}
	if (!res)
	{
		I_SetCursor(TexMan.GetGameTextureByName("cursor"));
	}
}

// Controlled by startup dialog
CVAR (Bool, disableautoload, false, CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG)
CVAR (Bool, autoloadbrightmaps, false, CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG)
CVAR (Bool, autoloadlights, false, CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG)
CVAR (Bool, autoloadwidescreen, true, CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG)
CVAR (Bool, r_debug_disable_vis_filter, false, 0)
CVAR(Bool, vid_fps, false, 0)
CVAR(Int, vid_showpalette, 0, 0)

bool hud_toggled = false;
bool wantToRestart;
bool DrawFSHUD;				// [RH] Draw fullscreen HUD?
TArray<FString> allwads;
bool devparm;				// started game with -devparm
const char *D_DrawIcon;	// [RH] Patch name of icon to draw on next refresh
int NoWipe;				// [RH] Allow wipe? (Needs to be set each time)
bool singletics = false;	// debug flag to cancel adaptiveness
FString startmap;
bool autostart;
bool advancedemo;
FILE *debugfile;
FILE *hashfile;
gamestate_t wipegamestate = GS_DEMOSCREEN;	// can be -1 to force a wipe
bool PageBlank;
FGameTexture *Advisory;
FTextureID Page;
const char *Subtitle;
bool nospriterename;
FStartupInfo GameStartupInfo;
FString lastIWAD;
int restart = 0;
bool AppActive = true;
bool playedtitlemusic;

cycle_t FrameCycles;

// [SP] Store the capabilities of the renderer in a global variable, to prevent excessive per-frame processing
uint32_t r_renderercaps = 0;


// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int demosequence;
static int pagetic;

// CODE --------------------------------------------------------------------

void D_GrabCVarDefaults()
{
	int lump, lastlump = 0;
	int lumpversion, gamelastrunversion;
	gamelastrunversion = atoi(LASTRUNVERSION);

	while ((lump = fileSystem.FindLump("DEFCVARS", &lastlump)) != -1)
	{
		// don't parse from wads
		if (lastlump > fileSystem.GetLastEntry(fileSystem.GetMaxIwadNum()))
		{
			// would rather put this in a modal of some sort, but this will have to do.
			Printf(TEXTCOLOR_RED "Cannot load DEFCVARS from a wadfile!\n");
			break;
		}

		FScanner sc(lump);

		sc.MustGetString();
		if (!sc.Compare("version"))
			sc.ScriptError("Must declare version for defcvars! (currently: %i)", gamelastrunversion);
		sc.MustGetNumber();
		lumpversion = sc.Number;
		if (lumpversion > gamelastrunversion)
			sc.ScriptError("Unsupported version %i (%i supported)", lumpversion, gamelastrunversion);
		if (lumpversion < 219)
			sc.ScriptError("Version must be at least 219 (current version %i)", gamelastrunversion);

		FBaseCVar* var;
		FString CurrentFindCVar;

		while (sc.GetString())
		{
			if (sc.Compare("set"))
			{
				sc.MustGetString();
			}

			CurrentFindCVar = sc.String;
			if (lumpversion < 220)
			{
				CurrentFindCVar.ToLower();

				// these two got renamed
				if (strcmp(CurrentFindCVar, "gamma") == 0)
				{
					CurrentFindCVar = "vid_gamma";
				}
				if (strcmp(CurrentFindCVar, "fullscreen") == 0)
				{
					CurrentFindCVar = "vid_fullscreen";
				}

				// this was removed
				if (strcmp(CurrentFindCVar, "cd_drive") == 0)
					break;
			}
			if (lumpversion < 221)
			{
				// removed cvar
				// this one doesn't matter as much, since it depended on platform-specific values,
				// and is something the user should change anyhow, so, let's just throw this value
				// out.
				if (strcmp(CurrentFindCVar, "mouse_sensitivity") == 0)
					break;
				if (strcmp(CurrentFindCVar, "m_noprescale") == 0)
					break;
			}

			var = FindCVar(CurrentFindCVar, NULL);
			if (var != NULL)
			{
				if (var->GetFlags() & CVAR_ARCHIVE)
				{
					UCVarValue val;

					sc.MustGetString();
					val.String = const_cast<char*>(sc.String);
					var->SetGenericRepDefault(val, CVAR_String);
				}
				else
				{
					sc.ScriptError("Cannot set cvar default for non-config cvar '%s'", sc.String);
				}
			}
			else
			{
				sc.ScriptError("Unknown cvar '%s'", sc.String);
			}
		}
	}
}

//==========================================================================
//
// D_ToggleHud
//
// Turns off 2D drawing temporarily.
//
//==========================================================================

void D_ToggleHud()
{
	static int saved_screenblocks;
	static bool saved_drawplayersprite, saved_showmessages;

	if ((hud_toggled = !hud_toggled))
	{
		saved_screenblocks = screenblocks;
		saved_drawplayersprite = r_drawplayersprites;
		saved_showmessages = show_messages;
		screenblocks = 12;
		r_drawplayersprites = false;
		show_messages = false;
		C_HideConsole();
		M_ClearMenus();
	}
	else
	{
		screenblocks = saved_screenblocks;
		r_drawplayersprites = saved_drawplayersprite;
		show_messages = saved_showmessages;
	}
}
CCMD(togglehud)
{
	D_ToggleHud();
}

CVAR(Float, m_pitch, 1.f, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)		// Mouse speeds
CVAR(Float, m_yaw, 1.f, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)

//==========================================================================
//
// Render wrapper.
// This function contains all the needed setup and cleanup for starting a render job.
//
//==========================================================================

void D_Render(std::function<void()> action, bool interpolate)
{
	for (auto Level : AllLevels())
	{
		// Check for the presence of dynamic lights at the start of the frame once.
		if ((gl_lights && vid_rendermode == 4) || (r_dynlights && vid_rendermode != 4))
		{
			Level->HasDynamicLights = !!Level->lights;
		}
		else Level->HasDynamicLights = false;	// lights are off so effectively we have none.
		if (interpolate) Level->interpolator.DoInterpolations(I_GetTimeFrac());
		P_FindParticleSubsectors(Level);
		PO_LinkToSubsectors(Level);
	}
	action();

	if (interpolate) for (auto Level : AllLevels())
	{
		Level->interpolator.RestoreInterpolations();
	}
}

//==========================================================================
//
// CVAR dmflags
//
//==========================================================================

CUSTOM_CVAR (Int, dmflags, 0, CVAR_SERVERINFO | CVAR_NOINITCALL)
{
	// In case DF_NO_FREELOOK was changed, reinitialize the sky
	// map. (If no freelook, then no need to stretch the sky.)
	R_InitSkyMap ();

	if (self & DF_NO_FREELOOK)
	{
		Net_WriteByte (DEM_CENTERVIEW);
	}
	// If nofov is set, force everybody to the arbitrator's FOV.
	if ((self & DF_NO_FOV) && consoleplayer == Net_Arbitrator)
	{
		float fov;

		Net_WriteByte (DEM_FOV);

		// If the game is started with DF_NO_FOV set, the arbitrator's
		// DesiredFOV will not be set when this callback is run, so
		// be sure not to transmit a 0 FOV.
		fov = players[consoleplayer].DesiredFOV;
		if (fov == 0)
		{
			fov = 90;
		}
		Net_WriteFloat (fov);
	}
}

CVAR (Flag, sv_nohealth,		dmflags, DF_NO_HEALTH);
CVAR (Flag, sv_noitems,			dmflags, DF_NO_ITEMS);
CVAR (Flag, sv_weaponstay,		dmflags, DF_WEAPONS_STAY);
CVAR (Flag, sv_falldamage,		dmflags, DF_FORCE_FALLINGHX);
CVAR (Flag, sv_oldfalldamage,	dmflags, DF_FORCE_FALLINGZD);
CVAR (Flag, sv_samelevel,		dmflags, DF_SAME_LEVEL);
CVAR (Flag, sv_spawnfarthest,	dmflags, DF_SPAWN_FARTHEST);
CVAR (Flag, sv_forcerespawn,	dmflags, DF_FORCE_RESPAWN);
CVAR (Flag, sv_noarmor,			dmflags, DF_NO_ARMOR);
CVAR (Flag, sv_noexit,			dmflags, DF_NO_EXIT);
CVAR (Flag, sv_infiniteammo,	dmflags, DF_INFINITE_AMMO);
CVAR (Flag, sv_nomonsters,		dmflags, DF_NO_MONSTERS);
CVAR (Flag, sv_monsterrespawn,	dmflags, DF_MONSTERS_RESPAWN);
CVAR (Flag, sv_itemrespawn,		dmflags, DF_ITEMS_RESPAWN);
CVAR (Flag, sv_fastmonsters,	dmflags, DF_FAST_MONSTERS);
CVAR (Flag, sv_nojump,			dmflags, DF_NO_JUMP);
CVAR (Flag, sv_allowjump,		dmflags, DF_YES_JUMP);
CVAR (Flag, sv_nofreelook,		dmflags, DF_NO_FREELOOK);
CVAR (Flag, sv_allowfreelook,	dmflags, DF_YES_FREELOOK);
CVAR (Flag, sv_nofov,			dmflags, DF_NO_FOV);
CVAR (Flag, sv_noweaponspawn,	dmflags, DF_NO_COOP_WEAPON_SPAWN);
CVAR (Flag, sv_nocrouch,		dmflags, DF_NO_CROUCH);
CVAR (Flag, sv_allowcrouch,		dmflags, DF_YES_CROUCH);
CVAR (Flag, sv_cooploseinventory,	dmflags, DF_COOP_LOSE_INVENTORY);
CVAR (Flag, sv_cooplosekeys,	dmflags, DF_COOP_LOSE_KEYS);
CVAR (Flag, sv_cooploseweapons,	dmflags, DF_COOP_LOSE_WEAPONS);
CVAR (Flag, sv_cooplosearmor,	dmflags, DF_COOP_LOSE_ARMOR);
CVAR (Flag, sv_cooplosepowerups,	dmflags, DF_COOP_LOSE_POWERUPS);
CVAR (Flag, sv_cooploseammo,	dmflags, DF_COOP_LOSE_AMMO);
CVAR (Flag, sv_coophalveammo,	dmflags, DF_COOP_HALVE_AMMO);
CVAR (Flag, sv_instantreaction,	dmflags, DF_INSTANT_REACTION);

// Some (hopefully cleaner) interface to these settings.
CVAR (Mask, sv_crouch,			dmflags, DF_NO_CROUCH|DF_YES_CROUCH);
CVAR (Mask, sv_jump,			dmflags, DF_NO_JUMP|DF_YES_JUMP);
CVAR (Mask, sv_fallingdamage,	dmflags, DF_FORCE_FALLINGHX|DF_FORCE_FALLINGZD);
CVAR (Mask, sv_freelook,		dmflags, DF_NO_FREELOOK|DF_YES_FREELOOK);

//==========================================================================
//
// CVAR dmflags2
//
// [RH] From Skull Tag. Some of these were already done as separate cvars
// (such as bfgaiming), but I collected them here like Skull Tag does.
//
//==========================================================================

CUSTOM_CVAR (Int, dmflags2, 0, CVAR_SERVERINFO | CVAR_NOINITCALL)
{
	// Stop the automap if we aren't allowed to use it.
	if ((self & DF2_NO_AUTOMAP) && automapactive)
		AM_Stop ();

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		player_t *p = &players[i];

		if (!playeringame[i])
			continue;

		// Revert our view to our own eyes if spying someone else.
		if (self & DF2_DISALLOW_SPYING)
		{
			// The player isn't looking through its own eyes, so make it.
			if (p->camera != p->mo)
			{
				p->camera = p->mo;

				S_UpdateSounds (p->camera);
				StatusBar->AttachToPlayer (p);

				if (demoplayback || multiplayer)
					StatusBar->ShowPlayerName ();
			}
		}

		// Come out of chasecam mode if we're not allowed to use chasecam.
		if (!(dmflags2 & DF2_CHASECAM) && CheckCheatmode(false))
		{
			// Take us out of chasecam mode only.
			if (p->cheats & CF_CHASECAM)
				cht_DoCheat (p, CHT_CHASECAM);
		}
	}
}

CVAR (Flag, sv_weapondrop,			dmflags2, DF2_YES_WEAPONDROP);
CVAR (Flag, sv_noteamswitch,		dmflags2, DF2_NO_TEAM_SWITCH);
CVAR (Flag, sv_doubleammo,			dmflags2, DF2_YES_DOUBLEAMMO);
CVAR (Flag, sv_degeneration,		dmflags2, DF2_YES_DEGENERATION);
CVAR (Flag, sv_nobfgaim,			dmflags2, DF2_NO_FREEAIMBFG);
CVAR (Flag, sv_barrelrespawn,		dmflags2, DF2_BARRELS_RESPAWN);
CVAR (Flag, sv_keepfrags,			dmflags2, DF2_YES_KEEPFRAGS);
CVAR (Flag, sv_norespawn,			dmflags2, DF2_NO_RESPAWN);
CVAR (Flag, sv_losefrag,			dmflags2, DF2_YES_LOSEFRAG);
CVAR (Flag, sv_respawnprotect,		dmflags2, DF2_YES_RESPAWN_INVUL);
CVAR (Flag, sv_samespawnspot,		dmflags2, DF2_SAME_SPAWN_SPOT);
CVAR (Flag, sv_infiniteinventory,	dmflags2, DF2_INFINITE_INVENTORY);
CVAR (Flag, sv_killallmonsters,		dmflags2, DF2_KILL_MONSTERS);
CVAR (Flag, sv_noautomap,			dmflags2, DF2_NO_AUTOMAP);
CVAR (Flag, sv_noautomapallies,		dmflags2, DF2_NO_AUTOMAP_ALLIES);
CVAR (Flag, sv_disallowspying,		dmflags2, DF2_DISALLOW_SPYING);
CVAR (Flag, sv_chasecam,			dmflags2, DF2_CHASECAM);
CVAR (Flag, sv_disallowsuicide,		dmflags2, DF2_NOSUICIDE);
CVAR (Flag, sv_noautoaim,			dmflags2, DF2_NOAUTOAIM);
CVAR (Flag, sv_dontcheckammo,		dmflags2, DF2_DONTCHECKAMMO);
CVAR (Flag, sv_killbossmonst,		dmflags2, DF2_KILLBOSSMONST);
CVAR (Flag, sv_nocountendmonst,		dmflags2, DF2_NOCOUNTENDMONST);
CVAR (Flag, sv_respawnsuper,		dmflags2, DF2_RESPAWN_SUPER);
CVAR (Flag, sv_nothingspawn,		dmflags2, DF2_NO_COOP_THING_SPAWN);
CVAR (Flag, sv_alwaysspawnmulti,	dmflags2, DF2_ALWAYS_SPAWN_MULTI);

//==========================================================================
//
// CVAR compatflags
//
//==========================================================================

EXTERN_CVAR(Int, compatmode)

CUSTOM_CVAR (Int, compatflags, 0, CVAR_ARCHIVE|CVAR_SERVERINFO | CVAR_NOINITCALL)
{
	for (auto Level : AllLevels())
	{
		Level->ApplyCompatibility();
	}
}

CUSTOM_CVAR (Int, compatflags2, 0, CVAR_ARCHIVE|CVAR_SERVERINFO | CVAR_NOINITCALL)
{
	for (auto Level : AllLevels())
	{
		Level->ApplyCompatibility2();
		Level->SetCompatLineOnSide(true);
	}
}

CUSTOM_CVAR(Int, compatmode, 0, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	int v, w = 0;

	switch (self)
	{
	default:
	case 0:
		v = 0;
		break;

	case 1:	// Doom2.exe compatible with a few relaxed settings
		v = COMPATF_SHORTTEX | COMPATF_STAIRINDEX | COMPATF_USEBLOCKING | COMPATF_NODOORLIGHT | COMPATF_SPRITESORT |
			COMPATF_TRACE | COMPATF_MISSILECLIP | COMPATF_SOUNDTARGET | COMPATF_DEHHEALTH | COMPATF_CROSSDROPOFF |
			COMPATF_LIGHT | COMPATF_MASKEDMIDTEX;
		w = COMPATF2_FLOORMOVE | COMPATF2_EXPLODE1;
		break;

	case 2:	// same as 1 but stricter (NO_PASSMOBJ and INVISIBILITY are also set)
		v = COMPATF_SHORTTEX | COMPATF_STAIRINDEX | COMPATF_USEBLOCKING | COMPATF_NODOORLIGHT | COMPATF_SPRITESORT |
			COMPATF_TRACE | COMPATF_MISSILECLIP | COMPATF_SOUNDTARGET | COMPATF_NO_PASSMOBJ | COMPATF_LIMITPAIN |
			COMPATF_DEHHEALTH | COMPATF_INVISIBILITY | COMPATF_CROSSDROPOFF | COMPATF_CORPSEGIBS | COMPATF_HITSCAN |
			COMPATF_WALLRUN | COMPATF_NOTOSSDROPS | COMPATF_LIGHT | COMPATF_MASKEDMIDTEX;
		w = COMPATF2_BADANGLES | COMPATF2_FLOORMOVE | COMPATF2_POINTONLINE | COMPATF2_EXPLODE2;
		break;

	case 3: // Boom compat mode
		v = COMPATF_TRACE|COMPATF_SOUNDTARGET|COMPATF_BOOMSCROLL|COMPATF_MISSILECLIP|COMPATF_MASKEDMIDTEX;
		w = COMPATF2_EXPLODE1;
		break;

	case 4: // Old ZDoom compat mode
		v = COMPATF_SOUNDTARGET | COMPATF_LIGHT;
		w = COMPATF2_MULTIEXIT | COMPATF2_TELEPORT | COMPATF2_PUSHWINDOW | COMPATF2_CHECKSWITCHRANGE;
		break;

	case 5: // MBF compat mode
		v = COMPATF_TRACE | COMPATF_SOUNDTARGET | COMPATF_BOOMSCROLL | COMPATF_MISSILECLIP | COMPATF_MUSHROOM |
			COMPATF_MBFMONSTERMOVE | COMPATF_NOBLOCKFRIENDS | COMPATF_MASKEDMIDTEX;
		w = COMPATF2_EXPLODE1;
		break;

	case 6:	// Boom with some added settings to reenable some 'broken' behavior
		v = COMPATF_TRACE | COMPATF_SOUNDTARGET | COMPATF_BOOMSCROLL | COMPATF_MISSILECLIP | COMPATF_NO_PASSMOBJ |
			COMPATF_INVISIBILITY | COMPATF_CORPSEGIBS | COMPATF_HITSCAN | COMPATF_WALLRUN | COMPATF_NOTOSSDROPS | COMPATF_MASKEDMIDTEX;
		w = COMPATF2_POINTONLINE | COMPATF2_EXPLODE2;
		break;

	case 7: // Stricter MBF compatibility
		v = COMPATF_CORPSEGIBS | COMPATF_NOBLOCKFRIENDS | COMPATF_MBFMONSTERMOVE | COMPATF_INVISIBILITY |
			COMPATF_NOTOSSDROPS | COMPATF_MUSHROOM | COMPATF_NO_PASSMOBJ | COMPATF_BOOMSCROLL | COMPATF_WALLRUN |
			COMPATF_TRACE | COMPATF_HITSCAN | COMPATF_MISSILECLIP | COMPATF_MASKEDMIDTEX | COMPATF_SOUNDTARGET;
		w = COMPATF2_POINTONLINE | COMPATF2_EXPLODE1 | COMPATF2_EXPLODE2;
		break;
	}
	compatflags = v;
	compatflags2 = w;
}

CVAR (Flag, compat_shortTex,			compatflags,  COMPATF_SHORTTEX);
CVAR (Flag, compat_stairs,				compatflags,  COMPATF_STAIRINDEX);
CVAR (Flag, compat_limitpain,			compatflags,  COMPATF_LIMITPAIN);
CVAR (Flag, compat_silentpickup,		compatflags,  COMPATF_SILENTPICKUP);
CVAR (Flag, compat_nopassover,			compatflags,  COMPATF_NO_PASSMOBJ);
CVAR (Flag, compat_soundslots,			compatflags,  COMPATF_MAGICSILENCE);
CVAR (Flag, compat_wallrun,				compatflags,  COMPATF_WALLRUN);
CVAR (Flag, compat_notossdrops,			compatflags,  COMPATF_NOTOSSDROPS);
CVAR (Flag, compat_useblocking,			compatflags,  COMPATF_USEBLOCKING);
CVAR (Flag, compat_nodoorlight,			compatflags,  COMPATF_NODOORLIGHT);
CVAR (Flag, compat_ravenscroll,			compatflags,  COMPATF_RAVENSCROLL);
CVAR (Flag, compat_soundtarget,			compatflags,  COMPATF_SOUNDTARGET);
CVAR (Flag, compat_dehhealth,			compatflags,  COMPATF_DEHHEALTH);
CVAR (Flag, compat_trace,				compatflags,  COMPATF_TRACE);
CVAR (Flag, compat_dropoff,				compatflags,  COMPATF_DROPOFF);
CVAR (Flag, compat_boomscroll,			compatflags,  COMPATF_BOOMSCROLL);
CVAR (Flag, compat_invisibility,		compatflags,  COMPATF_INVISIBILITY);
CVAR (Flag, compat_silentinstantfloors,	compatflags,  COMPATF_SILENT_INSTANT_FLOORS);
CVAR (Flag, compat_sectorsounds,		compatflags,  COMPATF_SECTORSOUNDS);
CVAR (Flag, compat_missileclip,			compatflags,  COMPATF_MISSILECLIP);
CVAR (Flag, compat_crossdropoff,		compatflags,  COMPATF_CROSSDROPOFF);
CVAR (Flag, compat_anybossdeath,		compatflags,  COMPATF_ANYBOSSDEATH);
CVAR (Flag, compat_minotaur,			compatflags,  COMPATF_MINOTAUR);
CVAR (Flag, compat_mushroom,			compatflags,  COMPATF_MUSHROOM);
CVAR (Flag, compat_mbfmonstermove,		compatflags,  COMPATF_MBFMONSTERMOVE);
CVAR (Flag, compat_corpsegibs,			compatflags,  COMPATF_CORPSEGIBS);
CVAR (Flag, compat_noblockfriends,		compatflags,  COMPATF_NOBLOCKFRIENDS);
CVAR (Flag, compat_spritesort,			compatflags,  COMPATF_SPRITESORT);
CVAR (Flag, compat_hitscan,				compatflags,  COMPATF_HITSCAN);
CVAR (Flag, compat_light,				compatflags,  COMPATF_LIGHT);
CVAR (Flag, compat_polyobj,				compatflags,  COMPATF_POLYOBJ);
CVAR (Flag, compat_maskedmidtex,		compatflags,  COMPATF_MASKEDMIDTEX);
CVAR (Flag, compat_badangles,			compatflags2, COMPATF2_BADANGLES);
CVAR (Flag, compat_floormove,			compatflags2, COMPATF2_FLOORMOVE);
CVAR (Flag, compat_soundcutoff,			compatflags2, COMPATF2_SOUNDCUTOFF);
CVAR (Flag, compat_pointonline,			compatflags2, COMPATF2_POINTONLINE);
CVAR (Flag, compat_multiexit,			compatflags2, COMPATF2_MULTIEXIT);
CVAR (Flag, compat_teleport,			compatflags2, COMPATF2_TELEPORT);
CVAR (Flag, compat_pushwindow,			compatflags2, COMPATF2_PUSHWINDOW);
CVAR (Flag, compat_checkswitchrange,	compatflags2, COMPATF2_CHECKSWITCHRANGE);
CVAR (Flag, compat_explode1,			compatflags2, COMPATF2_EXPLODE1);
CVAR (Flag, compat_explode2,			compatflags2, COMPATF2_EXPLODE2);
CVAR (Flag, compat_railing,				compatflags2, COMPATF2_RAILING);

CVAR(Bool, vid_activeinbackground, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Int, gl_tonemap)
static uint32_t GetCaps()
{
	ActorRenderFeatureFlags FlagSet;
	if (!V_IsHardwareRenderer())
	{
		FlagSet = RFF_UNCLIPPEDTEX;

		if (V_IsTrueColor())
			FlagSet |= RFF_TRUECOLOR;
		else
			FlagSet |= RFF_COLORMAP;

	}
	else
	{
		// describe our basic feature set
		FlagSet = RFF_FLATSPRITES | RFF_MODELS | RFF_SLOPE3DFLOORS |
			RFF_TILTPITCH | RFF_ROLLSPRITES | RFF_POLYGONAL | RFF_MATSHADER | RFF_POSTSHADER | RFF_BRIGHTMAP;

		if (gl_tonemap != 5) // not running palette tonemap shader
			FlagSet |= RFF_TRUECOLOR;
	}

	if (r_drawvoxels)
		FlagSet |= RFF_VOXELS;
	return (uint32_t)FlagSet;
}

//==========================================================================
//
// 
//
//==========================================================================

static void DrawPaletteTester(int paletteno)
{
	int blocksize = screen->GetHeight() / 50;

	int t = paletteno;
	int k = 0;
	for (int i = 0; i < 16; ++i)
	{
		for (int j = 0; j < 16; ++j)
		{
			PalEntry pe;
			if (t > 1)
			{
				auto palette = GPalette.GetTranslation(TRANSLATION_Standard, t - 2)->Palette;
				pe = palette[k];
			}
			else GPalette.BaseColors[k];
			k++;
			Dim(twod, pe, 1.f, j * blocksize, i * blocksize, blocksize, blocksize);
		}
	}
}

//==========================================================================
//
// DFrameBuffer :: DrawRateStuff
//
// Draws the fps counter, dot ticker, and palette debug.
//
//==========================================================================
uint64_t LastCount;

static void DrawRateStuff()
{
	static uint64_t LastMS = 0, LastSec = 0, FrameCount = 0, LastTic = 0;

	// Draws frame time and cumulative fps
	if (vid_fps)
	{
		uint64_t ms = screen->FrameTime;
		uint64_t howlong = ms - LastMS;
		if ((signed)howlong >= 0)
		{
			char fpsbuff[40];
			int chars;
			int rate_x;

			int textScale = active_con_scale(twod);

			chars = mysnprintf(fpsbuff, countof(fpsbuff), "%2llu ms (%3llu fps)", (unsigned long long)howlong, (unsigned long long)LastCount);
			rate_x = screen->GetWidth() / textScale - NewConsoleFont->StringWidth(&fpsbuff[0]);
			ClearRect(twod, rate_x * textScale, 0, screen->GetWidth(), NewConsoleFont->GetHeight() * textScale, GPalette.BlackIndex, 0);
			DrawText(twod, NewConsoleFont, CR_WHITE, rate_x, 0, (char*)&fpsbuff[0],
				DTA_VirtualWidth, screen->GetWidth() / textScale,
				DTA_VirtualHeight, screen->GetHeight() / textScale,
				DTA_KeepRatio, true, TAG_DONE);

			uint32_t thisSec = (uint32_t)(ms / 1000);
			if (LastSec < thisSec)
			{
				LastCount = FrameCount / (thisSec - LastSec);
				LastSec = thisSec;
				FrameCount = 0;
			}
			FrameCount++;
		}
		LastMS = ms;
	}

	int Height = screen->GetHeight();

	// draws little dots on the bottom of the screen
	if (ticker)
	{
		int64_t t = I_GetTime();
		int64_t tics = t - LastTic;

		LastTic = t;
		if (tics > 20) tics = 20;

		int i;
		for (i = 0; i < tics * 2; i += 2)		ClearRect(twod, i, Height - 1, i + 1, Height, 255, 0);
		for (; i < 20 * 2; i += 2)			ClearRect(twod, i, Height - 1, i + 1, Height, 0, 0);
	}

	// draws the palette for debugging
	if (vid_showpalette)
	{
		DrawPaletteTester(vid_showpalette);
	}
}

static void End2DAndUpdate()
{
	DrawRateStuff();
	twod->End();
	CheckBench();
	screen->Update();
}

//==========================================================================
//
// D_Display
//
// Draw current display, possibly wiping it from the previous
//
//==========================================================================

void D_Display ()
{
	FGameTexture *wipe = nullptr;
	int wipe_type;
	sector_t *viewsec;

	if (nodrawers || screen == NULL)
		return; 				// for comparative timing / profiling
	
	if (!AppActive && (screen->IsFullscreen() || !vid_activeinbackground))
	{
		return;
	}

	cycle_t cycles;
	
	cycles.Reset();
	cycles.Clock();

	r_UseVanillaTransparency = UseVanillaTransparency(); // [SP] Cache UseVanillaTransparency() call
	r_renderercaps = GetCaps(); // [SP] Get the current capabilities of the renderer

	if (players[consoleplayer].camera == NULL)
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
	}

    auto &vp = r_viewpoint;
	if (viewactive)
	{
		DAngle fov = 90.f;
		AActor *cam = players[consoleplayer].camera;
		if (cam)
		{
			if (cam->player)
				fov = cam->player->FOV;
			else fov = cam->CameraFOV;
		}
		R_SetFOV(vp, fov);
	}

	// fullscreen toggle has been requested
	if (setmodeneeded)
	{
		setmodeneeded = false;
		screen->ToggleFullscreen(vid_fullscreen);
		V_OutputResized(screen->GetWidth(), screen->GetHeight());
	}

	// change the view size if needed
	if (setsizeneeded)
	{
		if (StatusBar == nullptr)
		{
			viewwidth = SCREENWIDTH;
			viewheight = SCREENHEIGHT;
			setsizeneeded = false;
		}
		else
		{
			R_ExecuteSetViewSize (vp, r_viewwindow);
	}
	}

	// [RH] Allow temporarily disabling wipes
	if (NoWipe)
	{
		NoWipe--;
		wipe = nullptr;
		wipegamestate = gamestate;
	}
	// No wipes when in a stereo3D VR mode
	else if (gamestate != wipegamestate && gamestate != GS_FULLCONSOLE && gamestate != GS_TITLELEVEL)
	{
		if (vr_mode == 0 || vid_rendermode != 4)
		{
			// save the current screen if about to wipe
			wipe = MakeGameTexture(screen->WipeStartScreen(), nullptr, ETextureType::SWCanvas);

			switch (wipegamestate)
			{
			default:
				wipe_type = wipetype;
				break;

			case GS_FORCEWIPEFADE:
				wipe_type = wipe_Fade;
				break;

			case GS_FORCEWIPEBURN:
				wipe_type = wipe_Burn;
				break;

			case GS_FORCEWIPEMELT:
				wipe_type = wipe_Melt;
				break;
			}
		}

		wipegamestate = gamestate;
	}
	else
	{
		wipe = nullptr;
	}
	
	screen->FrameTime = I_msTimeFS();
	TexAnim.UpdateAnimations(screen->FrameTime);
	R_UpdateSky(screen->FrameTime);
	screen->BeginFrame();
	twod->ClearClipRect();
	if ((gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL) && gametic != 0)
	{
		// [ZZ] execute event hook that we just started the frame
		//E_RenderFrame();
		//
		
		D_Render([&]()
		{
			viewsec = RenderView(&players[consoleplayer]);
		}, true);

		twod->Begin(screen->GetWidth(), screen->GetHeight());
		if (!hud_toggled)
		{
			V_DrawBlend(viewsec);
			if (automapactive)
			{
				primaryLevel->automap->Drawer ((hud_althud && viewheight == SCREENHEIGHT) ? viewheight : StatusBar->GetTopOfStatusbar());
			}
		
			// for timing the statusbar code.
			//cycle_t stb;
			//stb.Reset();
			//stb.Clock();
			if (!automapactive || viewactive)
			{
				StatusBar->RefreshViewBorder ();
			}
			if (hud_althud && viewheight == SCREENHEIGHT && screenblocks > 10)
			{
				StatusBar->DrawBottomStuff (HUD_AltHud);
				if (DrawFSHUD || automapactive) StatusBar->DrawAltHUD();
				if (players[consoleplayer].camera && players[consoleplayer].camera->player && !automapactive)
				{
					StatusBar->DrawCrosshair();
				}
				StatusBar->CallDraw (HUD_AltHud, vp.TicFrac);
				StatusBar->DrawTopStuff (HUD_AltHud);
			}
			else if (viewheight == SCREENHEIGHT && viewactive && screenblocks > 10)
			{
				EHudState state = DrawFSHUD ? HUD_Fullscreen : HUD_None;
				StatusBar->DrawBottomStuff (state);
				StatusBar->CallDraw (state, vp.TicFrac);
				StatusBar->DrawTopStuff (state);
			}
			else
			{
				StatusBar->DrawBottomStuff (HUD_StatusBar);
				StatusBar->CallDraw (HUD_StatusBar, vp.TicFrac);
				StatusBar->DrawTopStuff (HUD_StatusBar);
			}
			//stb.Unclock();
			//Printf("Stbar = %f\n", stb.TimeMS());
		}
	}
	else
	{
		twod->Begin(screen->GetWidth(), screen->GetHeight());
		switch (gamestate)
		{
			case GS_FULLCONSOLE:
				D_PageDrawer();
				C_DrawConsole ();
				M_Drawer ();
				End2DAndUpdate ();
				return;
				
			case GS_INTERMISSION:
				WI_Drawer ();
				break;
				
			case GS_FINALE:
				F_Drawer ();
				break;
				
			case GS_DEMOSCREEN:
				D_PageDrawer ();
				break;
				
			default:
				break;
		}
	}
	if (!hud_toggled)
	{
		CT_Drawer ();

		// draw pause pic
		if ((paused || pauseext) && menuactive == MENU_Off)
		{
			auto tex = TexMan.GetGameTextureByName(gameinfo.PauseSign, true);
			double x = (SCREENWIDTH - tex->GetDisplayWidth() * CleanXfac)/2 +
				tex->GetDisplayLeftOffset() * CleanXfac;
			DrawTexture(twod, tex, x, 4, DTA_CleanNoMove, true, TAG_DONE);
			if (paused && multiplayer)
			{
				FFont *font = generic_ui? NewSmallFont : SmallFont;
				FString pstring = GStrings("TXT_BY");
				pstring.Substitute("%s", players[paused - 1].userinfo.GetName());
				DrawText(twod, font, CR_RED,
					(twod->GetWidth() - font->StringWidth(pstring)*CleanXfac) / 2,
					(tex->GetDisplayHeight() * CleanYfac) + 4, pstring, DTA_CleanNoMove, true, TAG_DONE);
			}
		}

		// [RH] Draw icon, if any
		if (D_DrawIcon)
		{
			FTextureID picnum = TexMan.CheckForTexture (D_DrawIcon, ETextureType::MiscPatch);

			D_DrawIcon = NULL;
			if (picnum.isValid())
			{
				auto tex = TexMan.GetGameTexture(picnum);
				DrawTexture(twod, tex, 160 - tex->GetDisplayWidth()/2, 100 - tex->GetDisplayHeight()/2,
					DTA_320x200, true, TAG_DONE);
			}
			NoWipe = 10;
		}

		if (snd_drawoutput)
		{
			GSnd->DrawWaveDebug(snd_drawoutput);
		}
	}

	if (!wipe || NoWipe < 0 || wipe_type == wipe_None || hud_toggled)
	{
		if (wipe != nullptr) delete wipe;
		wipe = nullptr;
		NetUpdate ();			// send out any new accumulation
		// normal update
		// draw ZScript UI stuff
		C_DrawConsole ();	// draw console
		M_Drawer ();			// menu is drawn even on top of everything
		if (!hud_toggled)
			FStat::PrintStat (twod);
		End2DAndUpdate ();
	}
	else
	{
		// wipe update
		uint64_t wipestart, nowtime, diff;
		bool done;

		GSnd->SetSfxPaused(true, 1);
		I_FreezeTime(true);
		twod->End();
		auto wipend = MakeGameTexture(screen->WipeEndScreen(), nullptr, ETextureType::SWCanvas);
		auto wiper = Wiper::Create(wipe_type);
		wiper->SetTextures(wipe, wipend);

		wipestart = I_msTime();
		NetUpdate();		// send out any new accumulation

		do
		{
			do
			{
				I_WaitVBL(2);
				nowtime = I_msTime();
				diff = (nowtime - wipestart) * 40 / 1000;	// Using 35 here feels too slow.
			} while (diff < 1);
			wipestart = nowtime;
			twod->Begin(screen->GetWidth(), screen->GetHeight());
			done = wiper->Run(1);
			C_DrawConsole ();	// console and
			M_Drawer ();			// menu are drawn even on top of wipes
			End2DAndUpdate ();
			NetUpdate ();			// [RH] not sure this is needed anymore
		} while (!done);
		delete wiper;
		I_FreezeTime(false);
		GSnd->SetSfxPaused(false, 1);
	}
	cycles.Unclock();
	FrameCycles = cycles;
}

//==========================================================================
//
// D_ErrorCleanup ()
//
// Cleanup after a recoverable error or a restart
//==========================================================================

void D_ErrorCleanup ()
{
	savegamerestore = false;
	primaryLevel->BotInfo.RemoveAllBots (primaryLevel, true);
	D_QuitNetGame ();
	if (demorecording || demoplayback)
		G_CheckDemoStatus ();
	Net_ClearBuffers ();
	G_NewInit ();
	M_ClearMenus ();
	singletics = false;
	playeringame[0] = 1;
	players[0].playerstate = PST_LIVE;
	gameaction = ga_fullconsole;
	if (gamestate == GS_DEMOSCREEN)
	{
		menuactive = MENU_Off;
	}
	if (gamestate == GS_INTERMISSION) gamestate = GS_DEMOSCREEN;
	insave = false;
	ClearGlobalVMStack();
}

//==========================================================================
//
// D_DoomLoop
//
// Manages timing and IO, calls all ?_Responder, ?_Ticker, and ?_Drawer,
// calls I_GetTime, I_StartFrame, and I_StartTic
//
//==========================================================================

void D_DoomLoop ()
{
	int lasttic = 0;

	// Clamp the timer to TICRATE until the playloop has been entered.
	r_NoInterpolate = true;
	Page.SetInvalid();
	Subtitle = nullptr;
	Advisory = nullptr;

	vid_cursor.Callback();

	for (;;)
	{
		try
		{
			// frame syncronous IO operations
			if (gametic > lasttic)
			{
				lasttic = gametic;
				I_StartFrame ();
			}
			I_SetFrameTime();

			// process one or more tics
			if (singletics)
			{
				I_StartTic ();
				D_ProcessEvents ();
				G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
				if (advancedemo)
					D_DoAdvanceDemo ();
				C_Ticker ();
				M_Ticker ();
				G_Ticker ();
				// [RH] Use the consoleplayer's camera to update sounds
				S_UpdateSounds (players[consoleplayer].camera);	// move positional sounds
				gametic++;
				maketic++;
				GC::CheckGC ();
				Net_NewMakeTic ();
			}
			else
			{
				TryRunTics (); // will run at least one tic
			}
			// Update display, next frame, with current state.
			I_StartTic ();
			D_Display ();
			S_UpdateMusic();
			if (wantToRestart)
			{
				wantToRestart = false;
				return;
			}
		}
		catch (CRecoverableError &error)
		{
			if (error.GetMessage ())
			{
				Printf (PRINT_DEFAULT, PRINTF_BOLD, "\n%s\n", error.GetMessage());
			}
			D_ErrorCleanup ();
		}
		catch (CVMAbortException &error)
		{
			error.MaybePrintMessage();
			Printf("%s", error.stacktrace.GetChars());
			D_ErrorCleanup();
		}
	}
}

//==========================================================================
//
// D_PageTicker
//
//==========================================================================

void D_PageTicker (void)
{
	if (--pagetic < 0)
		D_AdvanceDemo ();
}

//==========================================================================
//
// D_PageDrawer
//
//==========================================================================

void D_PageDrawer (void)
{
	ClearRect(twod, 0, 0, SCREENWIDTH, SCREENHEIGHT, 0, 0);
	if (Page.Exists())
	{
		DrawTexture(twod, TexMan.GetGameTexture(Page, true), 0, 0,
			DTA_Fullscreen, true,
			DTA_Masked, false,
			DTA_BilinearFilter, true,
			TAG_DONE);
	}
	if (Subtitle != nullptr)
	{
		DrawFullscreenSubtitle(GStrings[Subtitle]);
	}
	if (Advisory != nullptr)
	{
		DrawTexture(twod, Advisory, 4, 160, DTA_320x200, true, TAG_DONE);
	}
}

//==========================================================================
//
// D_AdvanceDemo
//
// Called after each demo or intro demosequence finishes
//
//==========================================================================

void D_AdvanceDemo (void)
{
	advancedemo = true;
}

//==========================================================================
//
// D_DoStrifeAdvanceDemo
//
//==========================================================================

void D_DoStrifeAdvanceDemo ()
{
	static const char *const fullVoices[6] =
	{
		"svox/pro1", "svox/pro2", "svox/pro3", "svox/pro4", "svox/pro5", "svox/pro6"
	};
	static const char *const teaserVoices[6] =
	{
		"svox/voc91", "svox/voc92", "svox/voc93", "svox/voc94", "svox/voc95", "svox/voc96"
	};
	const char *const *voices = gameinfo.flags & GI_SHAREWARE ? teaserVoices : fullVoices;
	const char *pagename = nullptr;
	const char *subtitle = nullptr;

	gamestate = GS_DEMOSCREEN;
	PageBlank = false;

	switch (demosequence)
	{
	default:
	case 0:
		pagetic = 6 * TICRATE;
		pagename = "TITLEPIC";
		if (fileSystem.CheckNumForName ("d_logo", ns_music) < 0)
		{ // strife0.wad does not have d_logo
			S_StartMusic ("");
		}
		else
		{
			S_StartMusic ("d_logo");
		}
		C_HideConsole ();
		break;

	case 1:
		// [RH] Strife fades to black and then to the Rogue logo, but
		// I think it looks better if it doesn't fade.
		pagetic = 10 * TICRATE/35;
		pagename = "";	// PANEL0, but strife0.wad doesn't have it, so don't use it.
		PageBlank = true;
		S_Sound (CHAN_VOICE, CHANF_UI, "bishop/active", 1, ATTN_NORM);
		break;

	case 2:
		pagetic = 4 * TICRATE;
		pagename = "RGELOGO";
		break;

	case 3:
		pagetic = 7 * TICRATE;
		pagename = "PANEL1";
		subtitle = "TXT_SUB_INTRO1";
		S_Sound (CHAN_VOICE, CHANF_UI, voices[0], 1, ATTN_NORM);
		// The new Strife teaser has D_FMINTR.
		// The full retail Strife has D_INTRO.
		// And the old Strife teaser has both. (I do not know which one it actually uses, nor do I care.)
		S_StartMusic (gameinfo.flags & GI_TEASER2 ? "d_fmintr" : "d_intro");
		break;

	case 4:
		pagetic = 9 * TICRATE;
		pagename = "PANEL2";
		subtitle = "TXT_SUB_INTRO2";
		S_Sound (CHAN_VOICE, CHANF_UI, voices[1], 1, ATTN_NORM);
		break;

	case 5:
		pagetic = 12 * TICRATE;
		pagename = "PANEL3";
		subtitle = "TXT_SUB_INTRO3";
		S_Sound (CHAN_VOICE, CHANF_UI, voices[2], 1, ATTN_NORM);
		break;

	case 6:
		pagetic = 11 * TICRATE;
		pagename = "PANEL4";
		subtitle = "TXT_SUB_INTRO4";
		S_Sound (CHAN_VOICE, CHANF_UI, voices[3], 1, ATTN_NORM);
		break;

	case 7:
		pagetic = 10 * TICRATE;
		pagename = "PANEL5";
		subtitle = "TXT_SUB_INTRO5";
		S_Sound (CHAN_VOICE, CHANF_UI, voices[4], 1, ATTN_NORM);
		break;

	case 8:
		pagetic = 16 * TICRATE;
		pagename = "PANEL6";
		subtitle = "TXT_SUB_INTRO6";
		S_Sound (CHAN_VOICE, CHANF_UI, voices[5], 1, ATTN_NORM);
		break;

	case 9:
		pagetic = 6 * TICRATE;
		pagename = "vellogo";
		wipegamestate = GS_FORCEWIPEFADE;
		break;

	case 10:
		pagetic = 12 * TICRATE;
		pagename = "CREDIT";
		wipegamestate = GS_FORCEWIPEFADE;
		break;
	}
	if (demosequence++ > 10)
		demosequence = 0;
	if (demosequence == 9 && !(gameinfo.flags & GI_SHAREWARE))
		demosequence = 10;

	if (pagename != nullptr)
	{
		Page = TexMan.CheckForTexture(pagename, ETextureType::MiscPatch);
		Subtitle = subtitle;
	}
	else
	{
		Subtitle = nullptr;
	}
}

//==========================================================================
//
// D_DoAdvanceDemo
//
//==========================================================================

void D_DoAdvanceDemo (void)
{
	static char demoname[8] = "DEMO1";
	static int democount = 0;
	static int pagecount;
	FString pagename;

	advancedemo = false;

	if (gameaction != ga_nothing)
	{
		return;
	}

	players[consoleplayer].playerstate = PST_LIVE;	// not reborn
	usergame = false;				// no save / end game here
	paused = 0;

	// [RH] If you want something more dynamic for your title, create a map
	// and name it TITLEMAP. That map will be loaded and used as the title.

	if (P_CheckMapData("TITLEMAP"))
	{
		G_InitNew ("TITLEMAP", true);
		return;
	}

	if (gameinfo.gametype == GAME_Strife)
	{
		D_DoStrifeAdvanceDemo ();
		return;
	}

	switch (demosequence)
	{
	case 3:
		if (gameinfo.advisoryTime)
		{
			Advisory = TexMan.GetGameTextureByName("ADVISOR");
			demosequence = 1;
			pagetic = (int)(gameinfo.advisoryTime * TICRATE);
			break;
		}
		// fall through to case 1 if no advisory notice
		[[fallthrough]];

	case 1:
		Advisory = NULL;
		if (!M_DemoNoPlay)
		{
			democount++;
			mysnprintf (demoname + 4, countof(demoname) - 4, "%d", democount);
			if (fileSystem.CheckNumForName (demoname) < 0)
			{
				demosequence = 0;
				democount = 0;
				// falls through to case 0 below
			}
			else
			{
				singledemo = false;
				G_DeferedPlayDemo (demoname);
				demosequence = 2;
				break;
			}
		}
		[[fallthrough]];

	default:
	case 0:
		gamestate = GS_DEMOSCREEN;
		pagename = gameinfo.TitlePage;
		pagetic = (int)(gameinfo.titleTime * TICRATE);
		if (!playedtitlemusic) S_ChangeMusic (gameinfo.titleMusic, gameinfo.titleOrder, false);
		playedtitlemusic = true;
		demosequence = 3;
		pagecount = 0;
		C_HideConsole ();
		break;

	case 2:
		pagetic = (int)(gameinfo.pageTime * TICRATE);
		gamestate = GS_DEMOSCREEN;
		if (gameinfo.creditPages.Size() > 0)
		{
			pagename = gameinfo.creditPages[pagecount].GetChars();
			pagecount = (pagecount+1) % gameinfo.creditPages.Size();
		}
		demosequence = 1;
		break;
	}


	if (pagename.IsNotEmpty())
	{
		Page = TexMan.CheckForTexture(pagename, ETextureType::MiscPatch);
	}
}

//==========================================================================
//
// D_StartTitle
//
//==========================================================================

void D_StartTitle (void)
{
	playedtitlemusic = false;
	gameaction = ga_nothing;
	demosequence = -1;
	D_AdvanceDemo ();
}

//==========================================================================
//
// Cmd_Endgame
//
// [RH] Quit the current game and go to fullscreen console
//
//==========================================================================

CCMD (endgame)
{
	if (!netgame)
	{
		gameaction = ga_fullconsole;
		demosequence = -1;
		G_CheckDemoStatus();
	}
}

//==========================================================================
//
// ParseCVarInfo
//
//==========================================================================

void ParseCVarInfo()
{
	int lump, lastlump = 0;
	bool addedcvars = false;

	while ((lump = fileSystem.FindLump("CVARINFO", &lastlump)) != -1)
	{
		FScanner sc(lump);
		sc.SetCMode(true);

		while (sc.GetToken())
		{
			FString cvarname;
			char *cvardefault = NULL;
			ECVarType cvartype = CVAR_Dummy;
			int cvarflags = CVAR_MOD|CVAR_ARCHIVE;
			FBaseCVar *cvar;

			// Check for flag tokens.
			while (sc.TokenType == TK_Identifier)
			{
				if (stricmp(sc.String, "server") == 0)
				{
					cvarflags |= CVAR_SERVERINFO;
				}
				else if (stricmp(sc.String, "user") == 0)
				{
					cvarflags |= CVAR_USERINFO;
				}
				else if (stricmp(sc.String, "noarchive") == 0)
				{
					cvarflags &= ~CVAR_ARCHIVE;
				}
				else if (stricmp(sc.String, "cheat") == 0)
				{
					cvarflags |= CVAR_CHEAT;
				}
				else if (stricmp(sc.String, "latch") == 0)
				{
					cvarflags |= CVAR_LATCH;
				}
				else if (stricmp(sc.String, "nosave") == 0)
				{
					cvarflags |= CVAR_CONFIG_ONLY;
				}
				else
				{
					sc.ScriptError("Unknown cvar attribute '%s'", sc.String);
				}
				sc.MustGetAnyToken();
			}

			// Possibility of defining a cvar as 'server nosave' or 'user nosave' is kept for
			// compatibility reasons.
			if (cvarflags & CVAR_CONFIG_ONLY)
			{
				cvarflags &= ~CVAR_SERVERINFO;
				cvarflags &= ~CVAR_USERINFO;
			}

			// Do some sanity checks.
			// No need to check server-nosave and user-nosave combinations because they
			// are made impossible right above.
			if ((cvarflags & (CVAR_SERVERINFO|CVAR_USERINFO|CVAR_CONFIG_ONLY)) == 0 ||
				(cvarflags & (CVAR_SERVERINFO|CVAR_USERINFO)) == (CVAR_SERVERINFO|CVAR_USERINFO))
			{
				sc.ScriptError("One of 'server', 'user', or 'nosave' must be specified");
			}
			// The next token must be the cvar type.
			if (sc.TokenType == TK_Bool)
			{
				cvartype = CVAR_Bool;
			}
			else if (sc.TokenType == TK_Int)
			{
				cvartype = CVAR_Int;
			}
			else if (sc.TokenType == TK_Float)
			{
				cvartype = CVAR_Float;
			}
			else if (sc.TokenType == TK_Color)
			{
				cvartype = CVAR_Color;
			}
			else if (sc.TokenType == TK_String)
			{
				cvartype = CVAR_String;
			}
			else
			{
				sc.ScriptError("Bad cvar type '%s'", sc.String);
			}
			// The next token must be the cvar name.
			sc.MustGetToken(TK_Identifier);
			if (FindCVar(sc.String, NULL) != NULL)
			{
				sc.ScriptError("cvar '%s' already exists", sc.String);
			}
			cvarname = sc.String;
			// A default value is optional and signalled by a '=' token.
			if (sc.CheckToken('='))
			{
				switch (cvartype)
				{
				case CVAR_Bool:
					if (!sc.CheckToken(TK_True) && !sc.CheckToken(TK_False))
					{
						sc.ScriptError("Expected true or false");
					}
					cvardefault = sc.String;
					break;
				case CVAR_Int:
					sc.MustGetNumber();
					cvardefault = sc.String;
					break;
				case CVAR_Float:
					sc.MustGetFloat();
					cvardefault = sc.String;
					break;
				default:
					sc.MustGetString();
					cvardefault = sc.String;
					break;
				}
			}
			// Now create the cvar.
			cvar = C_CreateCVar(cvarname, cvartype, cvarflags);
			if (cvardefault != NULL)
			{
				UCVarValue val;
				val.String = cvardefault;
				cvar->SetGenericRepDefault(val, CVAR_String);
			}
			// To be like C and ACS, require a semicolon after everything.
			sc.MustGetToken(';');
			addedcvars = true;
		}
	}
	// Only load mod cvars from the config if we defined some, so we don't
	// clutter up the cvar space when not playing mods with custom cvars.
	if (addedcvars)
	{
		GameConfig->DoModSetup (gameinfo.ConfigName);
	}
}	

//==========================================================================
//
// ConsiderPatches
//
// Tries to add any deh/bex patches from the command line.
//
//==========================================================================

bool ConsiderPatches (const char *arg)
{
	int i, argc;
	FString *args;
	const char *f;

	argc = Args->CheckParmList(arg, &args);
	for (i = 0; i < argc; ++i)
	{
		if ( (f = BaseFileSearch(args[i], ".deh", false, GameConfig)) ||
			 (f = BaseFileSearch(args[i], ".bex", false, GameConfig)) )
		{
			D_LoadDehFile(f);
		}
	}
	return argc > 0;
}

//==========================================================================
//
// D_MultiExec
//
//==========================================================================

FExecList *D_MultiExec (FArgs *list, FExecList *exec)
{
	for (int i = 0; i < list->NumArgs(); ++i)
	{
		exec = C_ParseExecFile(list->GetArg(i), exec);
	}
	return exec;
}

static void GetCmdLineFiles(TArray<FString> &wadfiles)
{
	FString *args;
	int i, argc;

	argc = Args->CheckParmList("-file", &args);
	for (i = 0; i < argc; ++i)
	{
		D_AddWildFile(wadfiles, args[i], ".wad", GameConfig);
	}
}

static void CopyFiles(TArray<FString> &to, TArray<FString> &from)
{
	unsigned int ndx = to.Reserve(from.Size());
	for(unsigned i=0;i<from.Size(); i++)
	{
		to[ndx+i] = from[i];
	}
}

static FString ParseGameInfo(TArray<FString> &pwads, const char *fn, const char *data, int size)
{
	FScanner sc;
	FString iwad;
	int pos = 0;

	const char *lastSlash = strrchr (fn, '/');

	sc.OpenMem("GAMEINFO", data, size);
	while(sc.GetToken())
	{
		sc.TokenMustBe(TK_Identifier);
		FString nextKey = sc.String;
		sc.MustGetToken('=');
		if (!nextKey.CompareNoCase("IWAD"))
		{
			sc.MustGetString();
			iwad = sc.String;
		}
		else if (!nextKey.CompareNoCase("LOAD"))
		{
			do
			{
				sc.MustGetString();

				// Try looking for the wad in the same directory as the .wad
				// before looking for it in the current directory.

				FString checkpath;
				if (lastSlash != NULL)
				{
					checkpath = FString(fn, (lastSlash - fn) + 1);
					checkpath += sc.String;
				}
				else
				{
					checkpath = sc.String;
				}
				if (!FileExists(checkpath))
				{
					pos += D_AddFile(pwads, sc.String, true, pos, GameConfig);
				}
				else
				{
					pos += D_AddFile(pwads, checkpath, true, pos, GameConfig);
				}
			}
			while (sc.CheckToken(','));
		}
		else if (!nextKey.CompareNoCase("NOSPRITERENAME"))
		{
			sc.MustGetString();
			nospriterename = sc.Compare("true");
		}
		else if (!nextKey.CompareNoCase("STARTUPTITLE"))
		{
			sc.MustGetString();
			GameStartupInfo.Name = sc.String;
		}
		else if (!nextKey.CompareNoCase("STARTUPCOLORS"))
		{
			sc.MustGetString();
			GameStartupInfo.FgColor = V_GetColor(NULL, sc);
			sc.MustGetStringName(",");
			sc.MustGetString();
			GameStartupInfo.BkColor = V_GetColor(NULL, sc);
		}
		else if (!nextKey.CompareNoCase("STARTUPTYPE"))
		{
			sc.MustGetString();
			FString sttype = sc.String;
			if (!sttype.CompareNoCase("DOOM"))
				GameStartupInfo.Type = FStartupInfo::DoomStartup;
			else if (!sttype.CompareNoCase("HERETIC"))
				GameStartupInfo.Type = FStartupInfo::HereticStartup;
			else if (!sttype.CompareNoCase("HEXEN"))
				GameStartupInfo.Type = FStartupInfo::HexenStartup;
			else if (!sttype.CompareNoCase("STRIFE"))
				GameStartupInfo.Type = FStartupInfo::StrifeStartup;
			else GameStartupInfo.Type = FStartupInfo::DefaultStartup;
		}
		else if (!nextKey.CompareNoCase("STARTUPSONG"))
		{
			sc.MustGetString();
			GameStartupInfo.Song = sc.String;
		}
		else if (!nextKey.CompareNoCase("LOADLIGHTS"))
		{
			sc.MustGetNumber();
			GameStartupInfo.LoadLights = !!sc.Number;
		}
		else if (!nextKey.CompareNoCase("LOADBRIGHTMAPS"))
		{
			sc.MustGetNumber();
			GameStartupInfo.LoadBrightmaps = !!sc.Number;
		}
		else if (!nextKey.CompareNoCase("LOADWIDESCREEN"))
		{
			sc.MustGetNumber();
			GameStartupInfo.LoadWidescreen = !!sc.Number;
		}
		else
		{
			// Silently ignore unknown properties
			do
			{
				sc.MustGetAnyToken();
			}
			while(sc.CheckToken(','));
		}
	}
	return iwad;
}

static FString CheckGameInfo(TArray<FString> & pwads)
{
	FileSystem check;

	LumpFilterInfo lfi;
	for (auto p : iwad_folders) lfi.reservedFolders.Push(p);
	for (auto p : iwad_reserved) lfi.requiredPrefixes.Push(p);

	// Open the entire list as a temporary file system and look for a GAMEINFO lump. The last one will automatically win.
	check.InitMultipleFiles(pwads, true, &lfi);
	if (check.GetNumEntries() > 0)
	{
		int num = check.CheckNumForName("GAMEINFO");
		if (num >= 0)
		{
			// Found one!
			auto data = check.GetFileData(num);
			auto wadname = check.GetResourceFileName(check.GetFileContainer(num));
			return ParseGameInfo(pwads, wadname, (const char*)data.Data(), data.Size());
		}
	}
	return "";
}

//==========================================================================
//
// Checks the IWAD for MAP01 and if found sets GI_MAPxx
//
//==========================================================================

static void SetMapxxFlag()
{
	int lump_name = fileSystem.CheckNumForName("MAP01", ns_global, fileSystem.GetIwadNum());
	int lump_wad = fileSystem.CheckNumForFullName("maps/map01.wad", fileSystem.GetIwadNum());
	int lump_map = fileSystem.CheckNumForFullName("maps/map01.map", fileSystem.GetIwadNum());

	if (lump_name >= 0 || lump_wad >= 0 || lump_map >= 0) gameinfo.flags |= GI_MAPxx;
}

//==========================================================================
//
// Initialize
//
//==========================================================================

static void D_DoomInit()
{
	// Set the FPU precision to 53 significant bits. This is the default
	// for Visual C++, but not for GCC, so some slight math variances
	// might crop up if we leave it alone.
#if defined(_FPU_GETCW) && defined(_FPU_EXTENDED) && defined(_FPU_DOUBLE)
	{
		int cw;
		_FPU_GETCW(cw);
		cw = (cw & ~_FPU_EXTENDED) | _FPU_DOUBLE;
		_FPU_SETCW(cw);
	}
#elif defined(_PC_53)
// On the x64 architecture, changing the floating point precision is not supported.
#ifndef _WIN64
	int cfp = _control87(_PC_53, _MCW_PC);
#endif
#endif

	// Check response files before coalescing file parameters.
	M_FindResponseFile ();

	// Combine different file parameters with their pre-switch bits.
	Args->CollectFiles("-deh", ".deh");
	Args->CollectFiles("-bex", ".bex");
	Args->CollectFiles("-exec", ".cfg");
	Args->CollectFiles("-playdemo", ".lmp");
	Args->CollectFiles("-file", NULL);	// anything left goes after -file

	gamestate = GS_STARTUP;

	const char *v = Args->CheckValue("-rngseed");
	if (v)
	{
		rngseed = staticrngseed = atoi(v);
		use_staticrng = true;
		if (!batchrun) Printf("D_DoomInit: Static RNGseed %d set.\n", rngseed);
	}
	else
	{
		rngseed = I_MakeRNGSeed();
		use_staticrng = false;
	}
	srand(rngseed);
		
	FRandom::StaticClearRandom ();

	if (!batchrun) Printf ("M_LoadDefaults: Load system defaults.\n");
	M_LoadDefaults ();			// load before initing other systems
}

//==========================================================================
//
// AddAutoloadFiles
//
//==========================================================================

static void AddAutoloadFiles(const char *autoname)
{
	LumpFilterIWAD.Format("%s.", autoname);	// The '.' is appened to simplify parsing the string 

	// [SP] Dialog reaction - load lights.pk3 and brightmaps.pk3 based on user choices
	if (!(gameinfo.flags & GI_SHAREWARE))
	{
		if (GameStartupInfo.LoadLights == 1 || (GameStartupInfo.LoadLights != 0 && autoloadlights))
		{
			const char *lightswad = BaseFileSearch ("lights.pk3", NULL, true, GameConfig);
			if (lightswad)
				D_AddFile (allwads, lightswad, true, -1, GameConfig);
		}
		if (GameStartupInfo.LoadBrightmaps == 1 || (GameStartupInfo.LoadBrightmaps != 0 && autoloadbrightmaps))
		{
			const char *bmwad = BaseFileSearch ("brightmaps.pk3", NULL, true, GameConfig);
			if (bmwad)
				D_AddFile (allwads, bmwad, true, -1, GameConfig);
		}
		if (GameStartupInfo.LoadWidescreen == 1 || (GameStartupInfo.LoadWidescreen != 0 && autoloadwidescreen))
		{
			const char *wswad = BaseFileSearch ("game_widescreen_gfx.pk3", NULL, true, GameConfig);
			if (wswad)
				D_AddFile (allwads, wswad, true, -1, GameConfig);
		}
	}

	if (!(gameinfo.flags & GI_SHAREWARE) && !Args->CheckParm("-noautoload") && !disableautoload)
	{
		FString file;

		// [RH] zvox.wad - A wad I had intended to be automatically generated
		// from Q2's pak0.pak so the female and cyborg player could have
		// voices. I never got around to writing the utility to do it, though.
		// And I probably never will now. But I know at least one person uses
		// it for something else, so this gets to stay here.
		const char *wad = BaseFileSearch ("zvox.wad", NULL, false, GameConfig);
		if (wad)
			D_AddFile (allwads, wad, true, -1, GameConfig);
	
		// [RH] Add any .wad files in the skins directory
#ifdef __unix__
		file = SHARE_DIR;
#else
		file = progdir;
#endif
		file += "skins";
		D_AddDirectory (allwads, file, "*.wad", GameConfig);

#ifdef __unix__
		file = NicePath("$HOME/" GAME_DIR "/skins");
		D_AddDirectory (allwads, file, "*.wad", GameConfig);
#endif	

		// Add common (global) wads
		D_AddConfigFiles(allwads, "Global.Autoload", "*.wad", GameConfig);

		long len;
		int lastpos = -1;

		while ((len = LumpFilterIWAD.IndexOf('.', lastpos+1)) > 0)
		{
			file = LumpFilterIWAD.Left(len) + ".Autoload";
			D_AddConfigFiles(allwads, file, "*.wad", GameConfig);
			lastpos = len;
		}
	}
}

//==========================================================================
//
// CheckCmdLine
//
//==========================================================================

static void CheckCmdLine()
{
	int flags = dmflags;
	int p;
	const char *v;

	if (!batchrun) Printf ("Checking cmd-line parameters...\n");
	if (Args->CheckParm ("-nomonsters"))	flags |= DF_NO_MONSTERS;
	if (Args->CheckParm ("-respawn"))		flags |= DF_MONSTERS_RESPAWN;
	if (Args->CheckParm ("-fast"))			flags |= DF_FAST_MONSTERS;

	devparm = !!Args->CheckParm ("-devparm");

	if (Args->CheckParm ("-altdeath"))
	{
		deathmatch = 1;
		flags |= DF_ITEMS_RESPAWN;
	}
	else if (Args->CheckParm ("-deathmatch"))
	{
		deathmatch = 1;
		flags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN;
	}

	dmflags = flags;

	// get skill / episode / map from parms
	if (gameinfo.gametype != GAME_Hexen)
	{
		startmap = (gameinfo.flags & GI_MAPxx) ? "MAP01" : "E1M1";
	}
	else
	{
		startmap = "&wt@01";
	}
	autostart = StoredWarp.IsNotEmpty();
				
	const char *val = Args->CheckValue ("-skill");
	if (val)
	{
		gameskill = val[0] - '1';
		autostart = true;
	}

	p = Args->CheckParm ("-warp");
	if (p && p < Args->NumArgs() - 1)
	{
		int ep, map;

		if (gameinfo.flags & GI_MAPxx)
		{
			ep = 1;
			map = atoi (Args->GetArg(p+1));
		}
		else 
		{
			ep = atoi (Args->GetArg(p+1));
			map = p < Args->NumArgs() - 2 ? atoi (Args->GetArg(p+2)) : 10;
			if (map < 1 || map > 9)
			{
				map = ep;
				ep = 1;
			}
		}

		startmap = CalcMapName (ep, map);
		autostart = true;
	}

	// [RH] Hack to handle +map. The standard console command line handler
	// won't be able to handle it, so we take it out of the command line and set
	// it up like -warp.
	FString mapvalue = Args->TakeValue("+map");
	if (mapvalue.IsNotEmpty())
	{
		if (!P_CheckMapData(mapvalue))
		{
			Printf ("Can't find map %s\n", mapvalue.GetChars());
		}
		else
		{
			startmap = mapvalue;
			autostart = true;
		}
	}

	if (devparm)
	{
		Printf ("%s", GStrings("D_DEVSTR"));
	}

	// turbo option  // [RH] (now a cvar)
	v = Args->CheckValue("-turbo");
	if (v != NULL)
	{
		double amt = atof(v);
		Printf ("turbo scale: %.0f%%\n", amt);
		turbo = (float)amt;
	}

	v = Args->CheckValue ("-timer");
	if (v)
	{
		double time = strtod (v, NULL);
		Printf ("Levels will end after %g minute%s.\n", time, time > 1 ? "s" : "");
		timelimit = (float)time;
	}

	v = Args->CheckValue ("-avg");
	if (v)
	{
		Printf ("Austin Virtual Gaming: Levels will end after 20 minutes\n");
		timelimit = 20.f;
	}

	//
	//  Build status bar line!
	//
	if (deathmatch)
		StartScreen->AppendStatusLine("DeathMatch...");
	if (dmflags & DF_NO_MONSTERS)
		StartScreen->AppendStatusLine("No Monsters...");
	if (dmflags & DF_MONSTERS_RESPAWN)
		StartScreen->AppendStatusLine("Respawning...");
	if (autostart)
	{
		FString temp;
		temp.Format ("Warp to map %s, Skill %d ", startmap.GetChars(), gameskill + 1);
		StartScreen->AppendStatusLine(temp);
	}
}

static void NewFailure ()
{
    I_FatalError ("Failed to allocate memory from system heap");
}


//==========================================================================
//
// RenameSprites
//
// Renames sprites in IWADs so that unique actors can have unique sprites,
// making it possible to import any actor from any game into any other
// game without jumping through hoops to resolve duplicate sprite names.
// You just need to know what the sprite's new name is.
//
//==========================================================================

static void RenameSprites(FileSystem &fileSystem, const TArray<FString>& deletelumps)
{
	bool renameAll;
	bool MNTRZfound = false;
	const char* altbigfont = gameinfo.gametype == GAME_Strife ? "SBIGFONT" : (gameinfo.gametype & GAME_Raven) ? "HBIGFONT" : "DBIGFONT";

	static const uint32_t HereticRenames[] =
	{ MAKE_ID('H','E','A','D'), MAKE_ID('L','I','C','H'),		// Ironlich
	};

	static const uint32_t HexenRenames[] =
	{ MAKE_ID('B','A','R','L'), MAKE_ID('Z','B','A','R'),		// ZBarrel
	  MAKE_ID('A','R','M','1'), MAKE_ID('A','R','_','1'),		// MeshArmor
	  MAKE_ID('A','R','M','2'), MAKE_ID('A','R','_','2'),		// FalconShield
	  MAKE_ID('A','R','M','3'), MAKE_ID('A','R','_','3'),		// PlatinumHelm
	  MAKE_ID('A','R','M','4'), MAKE_ID('A','R','_','4'),		// AmuletOfWarding
	  MAKE_ID('S','U','I','T'), MAKE_ID('Z','S','U','I'),		// ZSuitOfArmor and ZArmorChunk
	  MAKE_ID('T','R','E','1'), MAKE_ID('Z','T','R','E'),		// ZTree and ZTreeDead
	  MAKE_ID('T','R','E','2'), MAKE_ID('T','R','E','S'),		// ZTreeSwamp150
	  MAKE_ID('C','A','N','D'), MAKE_ID('B','C','A','N'),		// ZBlueCandle
	  MAKE_ID('R','O','C','K'), MAKE_ID('R','O','K','K'),		// rocks and dirt in a_debris.cpp
	  MAKE_ID('W','A','T','R'), MAKE_ID('H','W','A','T'),		// Strife also has WATR
	  MAKE_ID('G','I','B','S'), MAKE_ID('P','O','L','5'),		// RealGibs
	  MAKE_ID('E','G','G','M'), MAKE_ID('P','R','K','M'),		// PorkFX
	  MAKE_ID('I','N','V','U'), MAKE_ID('D','E','F','N'),		// Icon of the Defender
	};

	static const uint32_t StrifeRenames[] =
	{ MAKE_ID('M','I','S','L'), MAKE_ID('S','M','I','S'),		// lots of places
	  MAKE_ID('A','R','M','1'), MAKE_ID('A','R','M','3'),		// MetalArmor
	  MAKE_ID('A','R','M','2'), MAKE_ID('A','R','M','4'),		// LeatherArmor
	  MAKE_ID('P','M','A','P'), MAKE_ID('S','M','A','P'),		// StrifeMap
	  MAKE_ID('T','L','M','P'), MAKE_ID('T','E','C','H'),		// TechLampSilver and TechLampBrass
	  MAKE_ID('T','R','E','1'), MAKE_ID('T','R','E','T'),		// TreeStub
	  MAKE_ID('B','A','R','1'), MAKE_ID('B','A','R','C'),		// BarricadeColumn
	  MAKE_ID('S','H','T','2'), MAKE_ID('M','P','U','F'),		// MaulerPuff
	  MAKE_ID('B','A','R','L'), MAKE_ID('B','B','A','R'),		// StrifeBurningBarrel
	  MAKE_ID('T','R','C','H'), MAKE_ID('T','R','H','L'),		// SmallTorchLit
	  MAKE_ID('S','H','R','D'), MAKE_ID('S','H','A','R'),		// glass shards
	  MAKE_ID('B','L','S','T'), MAKE_ID('M','A','U','L'),		// Mauler
	  MAKE_ID('L','O','G','G'), MAKE_ID('L','O','G','W'),		// StickInWater
	  MAKE_ID('V','A','S','E'), MAKE_ID('V','A','Z','E'),		// Pot and Pitcher
	  MAKE_ID('C','N','D','L'), MAKE_ID('K','N','D','L'),		// Candle
	  MAKE_ID('P','O','T','1'), MAKE_ID('M','P','O','T'),		// MetalPot
	  MAKE_ID('S','P','I','D'), MAKE_ID('S','T','L','K'),		// Stalker
	};

	const uint32_t* renames;
	int numrenames;

	switch (gameinfo.gametype)
	{
	case GAME_Doom:
	default:
		// Doom's sprites don't get renamed.
		renames = nullptr;
		numrenames = 0;
		break;

	case GAME_Heretic:
		renames = HereticRenames;
		numrenames = sizeof(HereticRenames) / 8;
		break;

	case GAME_Hexen:
		renames = HexenRenames;
		numrenames = sizeof(HexenRenames) / 8;
		break;

	case GAME_Strife:
		renames = StrifeRenames;
		numrenames = sizeof(StrifeRenames) / 8;
		break;
	}

	unsigned NumFiles = fileSystem.GetNumEntries();

	for (uint32_t i = 0; i < NumFiles; i++)
	{
		// check for full Minotaur animations. If this is not found
		// some frames need to be renamed.
		if (fileSystem.GetFileNamespace(i) == ns_sprites)
		{
			auto& shortName = fileSystem.GetShortName(i);
			if (shortName.dword == MAKE_ID('M', 'N', 'T', 'R') && shortName.String[4] == 'Z')
			{
				MNTRZfound = true;
				break;
			}
		}
	}

	renameAll = !!Args->CheckParm("-oldsprites") || nospriterename;

	for (uint32_t i = 0; i < NumFiles; i++)
	{
		auto& shortName = fileSystem.GetShortName(i);
		if (fileSystem.GetFileNamespace(i) == ns_sprites)
		{
			// Only sprites in the IWAD normally get renamed
			if (renameAll || fileSystem.GetFileContainer(i) == fileSystem.GetIwadNum())
			{
				for (int j = 0; j < numrenames; ++j)
				{
					if (shortName.dword == renames[j * 2])
					{
						shortName.dword = renames[j * 2 + 1];
					}
				}
				if (gameinfo.gametype == GAME_Hexen)
				{
					if (fileSystem.CheckFileName(i, "ARTIINVU"))
					{
						shortName.String[4] = 'D'; shortName.String[5] = 'E';
						shortName.String[6] = 'F'; shortName.String[7] = 'N';
					}
				}
			}

			if (!MNTRZfound)
			{
				if (shortName.dword == MAKE_ID('M', 'N', 'T', 'R'))
				{
					for (size_t fi : {4, 6})
					{
						if (shortName.String[fi] >= 'F' && shortName.String[fi] <= 'K')
						{
							shortName.String[fi] += 'U' - 'F';
						}
					}
				}
			}

			// When not playing Doom rename all BLOD sprites to BLUD so that
			// the same blood states can be used everywhere
			if (!(gameinfo.gametype & GAME_DoomChex))
			{
				if (shortName.dword == MAKE_ID('B', 'L', 'O', 'D'))
				{
					shortName.dword = MAKE_ID('B', 'L', 'U', 'D');
				}
			}
		}
		else if (fileSystem.GetFileNamespace(i) == ns_global)
		{
			int fn = fileSystem.GetFileContainer(i);
			if (fn >= fileSystem.GetIwadNum() && fn <= fileSystem.GetMaxIwadNum() && deletelumps.Find(shortName.String) < deletelumps.Size())
			{
				shortName.String[0] = 0;	// Lump must be deleted from directory.
			}
			// Rename the game specific big font lumps so that the font manager does not have to do problematic special checks for them.
			else if (!strcmp(shortName.String, altbigfont))
			{
				strcpy(shortName.String, "BIGFONT");
			}
		}
	}
}

//==========================================================================
//
// RenameNerve
//
// Renames map headers and map name pictures in nerve.wad so as to load it
// alongside Doom II and offer both episodes without causing conflicts.
// MD5 checksum for NERVE.WAD: 967d5ae23daf45196212ae1b605da3b0 (3,819,855)
// MD5 checksum for Unity version of NERVE.WAD: 4214c47651b63ee2257b1c2490a518c9 (3,821,966)
//
//==========================================================================
void RenameNerve(FileSystem& fileSystem)
{
	if (gameinfo.gametype != GAME_Doom)
		return;

	const int numnerveversions = 4;

	bool found = false;
	uint8_t cksum[16];
	static const uint8_t nerve[numnerveversions][16] = {
			{ 0x96, 0x7d, 0x5a, 0xe2, 0x3d, 0xaf, 0x45, 0x19,
				0x62, 0x12, 0xae, 0x1b, 0x60, 0x5d, 0xa3, 0xb0 },
			{ 0x42, 0x14, 0xc4, 0x76, 0x51, 0xb6, 0x3e, 0xe2,
				0x25, 0x7b, 0x1c, 0x24, 0x90, 0xa5, 0x18, 0xc9 },
			{ 0x35, 0x44, 0xe1, 0x90, 0x30, 0x91, 0xc5, 0x0b,
				0xa5, 0x00, 0x49, 0xdb, 0x74, 0xcd, 0x7d, 0x25 },
			{ 0x91, 0x43, 0xb3, 0x92, 0x39, 0x2a, 0x7a, 0xc8,
				0x70, 0xc2, 0xca, 0x36, 0xac, 0x65, 0xaf, 0x45 }
	};
	size_t nervesize[numnerveversions] = { 3819855, 3821966, 3821885, 4003409 }; // NERVE.WAD's file size
	int w = fileSystem.GetIwadNum();
	while (++w < fileSystem.GetNumWads())
	{
		auto fr = fileSystem.GetFileReader(w);
		int isizecheck = -1;
		if (fr == NULL)
		{
			continue;
		}
		for (int icheck = 0; icheck < numnerveversions; icheck++)
			if (fr->GetLength() == (long)nervesize[icheck])
				isizecheck = icheck;
		if (isizecheck == -1)
		{
			// Skip MD5 computation when there is a
			// cheaper way to know this is not the file
			continue;
		}
		fr->Seek(0, FileReader::SeekSet);
		MD5Context md5;
		md5Update(*fr, md5, (unsigned)fr->GetLength());
		md5.Final(cksum);
		if (memcmp(nerve[isizecheck], cksum, 16) == 0)
		{
			found = true;
			break;
		}
	}

	if (!found)
		return;

	for (int i = fileSystem.GetFirstEntry(w); i <= fileSystem.GetLastEntry(w); i++)
	{
		auto& shortName = fileSystem.GetShortName(i);
		// Only rename the maps from NERVE.WAD
		if (shortName.dword == MAKE_ID('C', 'W', 'I', 'L'))
		{
			shortName.String[0] = 'N';
		}
		else if (shortName.dword == MAKE_ID('M', 'A', 'P', '0'))
		{
			shortName.String[6] = shortName.String[4];
			shortName.String[5] = '0';
			shortName.String[4] = 'L';
			shortName.dword = MAKE_ID('L', 'E', 'V', 'E');
		}
	}
}

//==========================================================================
//
// FixMacHexen
//
// Discard all extra lumps in Mac version of Hexen IWAD (demo or full)
// to avoid any issues caused by names of these lumps, including:
//  * Wrong height of small font
//  * Broken life bar of mage class
//
//==========================================================================

void FixMacHexen(FileSystem& fileSystem)
{
	if (GAME_Hexen != gameinfo.gametype)
	{
		return;
	}

	FileReader* reader = fileSystem.GetFileReader(fileSystem.GetIwadNum());
	auto iwadSize = reader->GetLength();

	static const long DEMO_SIZE = 13596228;
	static const long BETA_SIZE = 13749984;
	static const long FULL_SIZE = 21078584;

	if (DEMO_SIZE != iwadSize
		&& BETA_SIZE != iwadSize
		&& FULL_SIZE != iwadSize)
	{
		return;
	}

	reader->Seek(0, FileReader::SeekSet);

	uint8_t checksum[16];
	MD5Context md5;

	md5Update(*reader, md5, (unsigned)iwadSize);
	md5.Final(checksum);

	static const uint8_t HEXEN_DEMO_MD5[16] =
	{
		0x92, 0x5f, 0x9f, 0x50, 0x00, 0xe1, 0x7d, 0xc8,
		0x4b, 0x0a, 0x6a, 0x3b, 0xed, 0x3a, 0x6f, 0x31
	};

	static const uint8_t HEXEN_BETA_MD5[16] =
	{
		0x2a, 0xf1, 0xb2, 0x7c, 0xd1, 0x1f, 0xb1, 0x59,
		0xe6, 0x08, 0x47, 0x2a, 0x1b, 0x53, 0xe4, 0x0e
	};

	static const uint8_t HEXEN_FULL_MD5[16] =
	{
		0xb6, 0x81, 0x40, 0xa7, 0x96, 0xf6, 0xfd, 0x7f,
		0x3a, 0x5d, 0x32, 0x26, 0xa3, 0x2b, 0x93, 0xbe
	};

	const bool isBeta = 0 == memcmp(HEXEN_BETA_MD5, checksum, sizeof checksum);

	if (!isBeta
		&& 0 != memcmp(HEXEN_DEMO_MD5, checksum, sizeof checksum)
		&& 0 != memcmp(HEXEN_FULL_MD5, checksum, sizeof checksum))
	{
		return;
	}

	static const int EXTRA_LUMPS = 299;

	// Hexen Beta is very similar to Demo but it has MAP41: Maze at the end of the WAD
	// So keep this map if it's present but discard all extra lumps

	const int lastLump = fileSystem.GetLastEntry(fileSystem.GetIwadNum()) - (isBeta ? 12 : 0);
	assert(fileSystem.GetFirstEntry(fileSystem.GetIwadNum()) + 299 < lastLump);

	for (int i = lastLump - EXTRA_LUMPS + 1; i <= lastLump; ++i)
	{
		auto& shortName = fileSystem.GetShortName(i);
		shortName.String[0] = '\0';
	}
}

static void FindStrifeTeaserVoices(FileSystem& fileSystem)
{
	if (gameinfo.gametype == GAME_Strife && gameinfo.flags & GI_SHAREWARE)
	{
		unsigned NumFiles = fileSystem.GetNumEntries();
		for (uint32_t i = 0; i < NumFiles; i++)
		{
			auto& shortName = fileSystem.GetShortName(i);
			if (fileSystem.GetFileNamespace(i) == ns_global)
			{
				// Only sprites in the IWAD normally get renamed
				if (fileSystem.GetFileContainer(i) == fileSystem.GetIwadNum())
				{
					if (shortName.String[0] == 'V' &&
						shortName.String[1] == 'O' &&
						shortName.String[2] == 'C')
					{
						int j;

						for (j = 3; j < 8; ++j)
						{
							if (shortName.String[j] != 0 && !isdigit(shortName.String[j]))
								break;
						}
						if (j == 8)
						{
							fileSystem.SetFileNamespace(i, ns_strifevoices);
						}
					}
				}
			}
		}
	}
}


static const char *DoomButtons[] =
{
	"am_panleft",
	"user2" ,
	"jump" ,
	"right" ,
	"zoom" ,
	"back" ,
	"am_zoomin",
	"reload" ,
	"lookdown" ,
	"am_zoomout",
	"user4" ,
	"attack" ,
	"user1" ,
	"klook" ,
	"forward" ,
	"movedown" ,
	"altattack" ,
	"moveleft" ,
	"moveright" ,
	"am_panright",
	"am_panup" ,
	"mlook" ,
	"crouch" ,
	"left" ,
	"lookup" ,
	"user3" ,
	"strafe" ,
	"am_pandown",
	"showscores" ,
	"speed" ,
	"use" ,
	"moveup" };

CVAR(Bool, lookspring, true, CVAR_ARCHIVE);	// Generate centerview when -mlook encountered?
EXTERN_CVAR(String, language)

CUSTOM_CVAR(Int, mouse_capturemode, 1, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)
{
	if (self < 0)
	{
		self = 0;
	}
	else if (self > 2)
	{
		self = 2;
	}
}


void Mlook_ReleaseHandler()
{
	if (lookspring)
	{
		Net_WriteByte(DEM_CENTERVIEW);
	}
}

int StrTable_GetGender()
{
	return players[consoleplayer].userinfo.GetGender();
}

bool StrTable_ValidFilter(const char* str)
{
	if (gameinfo.gametype == GAME_Strife && (gameinfo.flags & GI_SHAREWARE) && !stricmp(str, "strifeteaser")) return true;
	return stricmp(str, GameNames[gameinfo.gametype]) == 0;
}

bool System_WantGuiCapture()
{
	bool wantCapt;

	if (menuactive == MENU_Off)
	{
		wantCapt = ConsoleState == c_down || ConsoleState == c_falling || chatmodeon;
	}
	else
	{
		wantCapt = (menuactive == MENU_On || menuactive == MENU_OnNoPause);
	}

	// [ZZ] check active event handlers that want the UI processing
	if (!wantCapt && primaryLevel->localEventManager->CheckUiProcessors())
		wantCapt = true;

	return wantCapt;
}

bool System_WantLeftButton()
{
	return (gamestate == GS_DEMOSCREEN || gamestate == GS_TITLELEVEL);
}

static bool System_DispatchEvent(event_t* ev)
{
	if (ev->type == EV_Mouse && menuactive == MENU_Off && ConsoleState != c_down && ConsoleState != c_falling && !primaryLevel->localEventManager->Responder(ev) && !paused)
	{
		if (buttonMap.ButtonDown(Button_Mlook) || freelook)
		{
			int look = int(ev->y * m_pitch * 16.0);
			if (invertmouse) look = -look;
			G_AddViewPitch(look, true);
			ev->y = 0;
		}
		if (!buttonMap.ButtonDown(Button_Strafe) && !lookstrafe)
		{
			int turn = int(ev->x * m_yaw * 8.0);
			if (invertmousex)
				turn = -turn;
			G_AddViewAngle(turn, true);
			ev->x = 0;
		}
		if (ev->x == 0 && ev->y == 0)
		{
			return true;
		}
	}
	return false;
}

bool System_NetGame()
{
	return netgame;
}

bool System_WantNativeMouse()
{
	return primaryLevel->localEventManager->CheckRequireMouse();
}

static bool System_CaptureModeInGame()
{
	if (demoplayback || paused) return false;
	switch (mouse_capturemode)
	{
	default:
	case 0:
		return gamestate == GS_LEVEL;
	case 1:
		return gamestate == GS_LEVEL || gamestate == GS_INTERMISSION || gamestate == GS_FINALE;
	case 2:
		return true;
	}
}

static void System_PlayStartupSound(const char* sndname)
{
	S_Sound(CHAN_BODY, 0, sndname, 1, ATTN_NONE);
}

static bool System_IsSpecialUI()
{
	return (generic_ui || !!log_vgafont || !!dlg_vgafont || ConsoleState != c_up || multiplayer ||
		(menuactive == MENU_On && CurrentMenu && !CurrentMenu->IsKindOf("ConversationMenu")));

}

static bool System_DisableTextureFilter()
{
	return !V_IsHardwareRenderer();
}

static void System_OnScreenSizeChanged()
{
	if (StatusBar != NULL)
	{
		StatusBar->CallScreenSizeChanged();
	}
	// Reload crosshair if transitioned to a different size
	ST_LoadCrosshair(true);
	if (primaryLevel && primaryLevel->automap)
		primaryLevel->automap->NewResolution();
}

IntRect System_GetSceneRect()
{
	IntRect mSceneViewport;
	// Special handling so the view with a visible status bar displays properly
	int height, width;
	if (screenblocks >= 10)
	{
		height = screen->GetHeight();
		width = screen->GetWidth();
	}
	else
	{
		height = (screenblocks * screen->GetHeight() / 10) & ~7;
		width = (screenblocks * screen->GetWidth() / 10);
	}

	mSceneViewport.left = viewwindowx;
	mSceneViewport.top = screen->GetHeight() - (height + viewwindowy - ((height - viewheight) / 2));
	mSceneViewport.width = viewwidth;
	mSceneViewport.height = height;
	return mSceneViewport;
}

FString System_GetLocationDescription()
{
	auto& vp = r_viewpoint;
	auto Level = vp.ViewLevel;
	return FStringf("Map %s: \"%s\",\nx = %1.4f, y = %1.4f, z = %1.4f, angle = %1.4f, pitch = %1.4f\n%llu fps\n\n",
		Level->MapName.GetChars(), Level->LevelName.GetChars(), vp.Pos.X, vp.Pos.Y, vp.Pos.Z, vp.Angles.Yaw.Degrees, vp.Angles.Pitch.Degrees, (unsigned long long)LastCount);

}

FString System_GetPlayerName(int node)
{
	return players[node].userinfo.GetName();
}

void System_ConsoleToggled(int state)
{
	if (state == c_falling && hud_toggled)
		D_ToggleHud();
}

//==========================================================================
//
// DoomSpecificInfo
//
// Called by the crash logger to get application-specific information.
//
//==========================================================================

void System_CrashInfo(char* buffer, size_t bufflen, const char *lfstr)
{
	const char* arg;
	char* const buffend = buffer + bufflen - 2;	// -2 for CRLF at end
	int i;

	buffer += mysnprintf(buffer, buffend - buffer, GAMENAME " version %s (%s)", GetVersionString(), GetGitHash());

	buffer += snprintf(buffer, buffend - buffer, "%sCommand line:", lfstr);
	for (i = 0; i < Args->NumArgs(); ++i)
	{
		buffer += snprintf(buffer, buffend - buffer, " %s", Args->GetArg(i));
	}

	for (i = 0; (arg = fileSystem.GetResourceFileName(i)) != NULL; ++i)
	{
		buffer += mysnprintf(buffer, buffend - buffer, "%sWad %d: %s", lfstr, i, arg);
	}

	if (gamestate != GS_LEVEL && gamestate != GS_TITLELEVEL)
	{
		buffer += mysnprintf(buffer, buffend - buffer, "%s%sNot in a level.", lfstr, lfstr);
	}
	else
	{
		buffer += mysnprintf(buffer, buffend - buffer, "%s%sCurrent map: %s", lfstr, lfstr, primaryLevel->MapName.GetChars());

		if (!viewactive)
		{
			buffer += mysnprintf(buffer, buffend - buffer, "%s%sView not active.", lfstr, lfstr);
		}
		else
		{
			auto& vp = r_viewpoint;
			buffer += mysnprintf(buffer, buffend - buffer, "%s%sviewx = %f", lfstr, lfstr, vp.Pos.X);
			buffer += mysnprintf(buffer, buffend - buffer, "%sviewy = %f", lfstr, vp.Pos.Y);
			buffer += mysnprintf(buffer, buffend - buffer, "%sviewz = %f", lfstr, vp.Pos.Z);
			buffer += mysnprintf(buffer, buffend - buffer, "%sviewangle = %f", lfstr, vp.Angles.Yaw.Degrees);
		}
	}
	buffer += mysnprintf(buffer, buffend - buffer, "%s", lfstr);
	*buffer = 0;
}

void System_M_Dim();


static void PatchTextures()
{
	// The Hexen scripts use BLANK as a blank texture, even though it's really not.
	// I guess the Doom renderer must have clipped away the line at the bottom of
	// the texture so it wasn't visible. Change its use type to a blank null texture to really make it blank.
	if (gameinfo.gametype == GAME_Hexen)
	{
		FTextureID tex = TexMan.CheckForTexture("BLANK", ETextureType::Wall, false);
		if (tex.Exists())
		{
			auto texture = TexMan.GetGameTexture(tex, false);
			texture->SetUseType(ETextureType::Null);
		}
	}
}

//==========================================================================
//
// this gets called during texture creation to fix known problems
//
//==========================================================================

static void CheckForHacks(BuildInfo& buildinfo)
{
	if (buildinfo.Parts.Size() == 0)
	{
		return;
	}

	// Heretic sky textures are marked as only 128 pixels tall,
	// even though they are really 200 pixels tall.
	if (gameinfo.gametype == GAME_Heretic &&
		buildinfo.Name.Len() == 4 &&
		buildinfo.Name[0] == 'S' &&
		buildinfo.Name[1] == 'K' &&
		buildinfo.Name[2] == 'Y' &&
		buildinfo.Name[3] >= '1' &&
		buildinfo.Name[3] <= '3' &&
		buildinfo.Height == 128 &&
		buildinfo.Parts.Size() == 1)
	{ 
		// This must alter the size of both the texture image and the game texture.
		buildinfo.Height = buildinfo.Parts[0].TexImage->GetImage()->GetHeight();
		buildinfo.texture->SetSize(buildinfo.Width, buildinfo.Height);
		return;
	}

	// The Doom E1 sky has its patch's y offset at -8 instead of 0.
	if (gameinfo.gametype == GAME_Doom &&
		!(gameinfo.flags & GI_MAPxx) &&
		buildinfo.Name.Len() == 4 &&
		buildinfo.Parts.Size() == 1 &&
		buildinfo.Height == 128 &&
		buildinfo.Parts[0].OriginY == -8 &&
		buildinfo.Name[0] == 'S' &&
		buildinfo.Name[1] == 'K' &&
		buildinfo.Name[2] == 'Y' &&
		buildinfo.Name[3] == '1')
	{
		buildinfo.Parts[0].OriginY = 0;
		return;
	}

	// BIGDOOR7 in Doom also has patches at y offset -4 instead of 0.
	if (gameinfo.gametype == GAME_Doom &&
		!(gameinfo.flags & GI_MAPxx) &&
		buildinfo.Name.CompareNoCase("BIGDOOR7") == 0 &&
		buildinfo.Parts.Size() == 2 &&
		buildinfo.Height == 128 &&
		buildinfo.Parts[0].OriginY == -4 &&
		buildinfo.Parts[1].OriginY == -4)
	{
		buildinfo.Parts[0].OriginY = 0;
		buildinfo.Parts[1].OriginY = 0;
		return;
	}
}

static void FixWideStatusBar()
{
	FGameTexture* sbartex = TexMan.FindGameTexture("stbar", ETextureType::MiscPatch);

	// only adjust offsets if none already exist and if the texture is not scaled. For scaled textures a proper offset is needed.
	if (sbartex && sbartex->GetTexelWidth() > 320 && sbartex->GetScaleX() == 1 &&
		!sbartex->GetTexelLeftOffset(0) && !sbartex->GetTexelTopOffset(0))
	{
		sbartex->SetOffsets(0, (sbartex->GetTexelWidth() - 320) / 2, 0);
		sbartex->SetOffsets(1, (sbartex->GetTexelWidth() - 320) / 2, 0);
	}
}

//==========================================================================
//
//
//
//==========================================================================

static void Doom_CastSpriteIDToString(FString* a, unsigned int b) 
{ 
	*a = (b >= sprites.Size()) ? "TNT1" : sprites[b].name; 
}


extern DThinker* NextToThink;

static void GC_MarkGameRoots()
{
	GC::Mark(DIntermissionController::CurrentIntermission);
	GC::Mark(staticEventManager.FirstEventHandler);
	GC::Mark(staticEventManager.LastEventHandler);
	for (auto Level : AllLevels())
		Level->Mark();

	// Mark players.
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
			players[i].PropagateMark();
	}

	// NextToThink must not be freed while thinkers are ticking.
	GC::Mark(NextToThink);
}

bool  CheckSkipGameOptionBlock(const char* str);

//==========================================================================
//
// D_DoomMain
//
//==========================================================================

static int D_DoomMain_Internal (void)
{
	int p;
	const char *v;
	const char *wad;
	TArray<FString> pwads;
	FString *args;
	int argcount;	
	FIWadManager *iwad_man;

	GC::AddMarkerFunc(GC_MarkGameRoots);
	VM_CastSpriteIDToString = Doom_CastSpriteIDToString;

	// Set up the button list. Mlook and Klook need a bit of extra treatment.
	buttonMap.SetButtons(DoomButtons, countof(DoomButtons));
	buttonMap.GetButton(Button_Mlook)->ReleaseHandler = Mlook_ReleaseHandler;
	buttonMap.GetButton(Button_Mlook)->bReleaseLock = true;
	buttonMap.GetButton(Button_Klook)->bReleaseLock = true;

	sysCallbacks = {
		System_WantGuiCapture,
		System_WantLeftButton,
		System_NetGame,
		System_WantNativeMouse,
		System_CaptureModeInGame,
		System_CrashInfo,
		System_PlayStartupSound,
		System_IsSpecialUI,
		System_DisableTextureFilter,
		System_OnScreenSizeChanged,
		System_GetSceneRect,
		System_GetLocationDescription,
		System_M_Dim,
		System_GetPlayerName,
		System_DispatchEvent,
		StrTable_ValidFilter,
		StrTable_GetGender,
		nullptr,
		CheckSkipGameOptionBlock,
		System_ConsoleToggled,
	};

	
	std::set_new_handler(NewFailure);
	const char *batchout = Args->CheckValue("-errorlog");
	
	C_InitConsole(80*8, 25*8, false);
	I_DetectOS();

	// +logfile gets checked too late to catch the full startup log in the logfile so do some extra check for it here.
	FString logfile = Args->TakeValue("+logfile");
	if (logfile.IsNotEmpty())
	{
		execLogfile(logfile);
	}
	else if (batchout != NULL && *batchout != 0)
	{
		batchrun = true;
		nosound = true;
		execLogfile(batchout, true);
		Printf("Command line: ");
		for (int i = 0; i < Args->NumArgs(); i++)
		{
			Printf("%s ", Args->GetArg(i));
		}
		Printf("\n");
	}

	if (Args->CheckParm("-hashfiles"))
	{
		const char *filename = "fileinfo.txt";
		Printf("Hashing loaded content to: %s\n", filename);
		hashfile = fopen(filename, "w");
		if (hashfile)
		{
			fprintf(hashfile, "%s version %s (%s)\n", GAMENAME, GetVersionString(), GetGitHash());
#ifdef __VERSION__
			fprintf(hashfile, "Compiler version: %s\n", __VERSION__);
#endif
			fprintf(hashfile, "Command line:");
			for (int i = 0; i < Args->NumArgs(); ++i)
			{
				fprintf(hashfile, " %s", Args->GetArg(i));
			}
			fprintf(hashfile, "\n");
		}
	}

	if (!batchrun) Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "%s version %s\n", GAMENAME, GetVersionString());

	D_DoomInit();

	extern void D_ConfirmSendStats();
	D_ConfirmSendStats();

	// [RH] Make sure zdoom.pk3 is always loaded,
	// as it contains magic stuff we need.
	wad = BaseFileSearch (BASEWAD, NULL, true, GameConfig);
	if (wad == NULL)
	{
		I_FatalError ("Cannot find " BASEWAD);
	}
	FString basewad = wad;

	FString optionalwad = BaseFileSearch(OPTIONALWAD, NULL, true, GameConfig);

	iwad_man = new FIWadManager(basewad, optionalwad);

	// Now that we have the IWADINFO, initialize the autoload ini sections.
	GameConfig->DoAutoloadSetup(iwad_man);

	// Prevent the game from starting if the savegame passed to -loadgame is invalid
	v = Args->CheckValue("-loadgame");
	if (v)
	{
		FString file(v);
		FixPathSeperator(file);
		DefaultExtension(file, "." SAVEGAME_EXT);
		if (!FileExists(file))
		{
			I_FatalError("Cannot find savegame %s", file.GetChars());
		}
	}

	// reinit from here

	do
	{
		PClass::StaticInit();
		PType::StaticInit();

		if (restart)
		{
			C_InitConsole(SCREENWIDTH, SCREENHEIGHT, false);
		}
		nospriterename = false;

		// Load zdoom.pk3 alone so that we can get access to the internal gameinfos before 
		// the IWAD is known.

		GetCmdLineFiles(pwads);
		FString iwad = CheckGameInfo(pwads);

		// The IWAD selection dialogue does not show in fullscreen so if the
		// restart is initiated without a defined IWAD assume for now that it's not going to change.
		if (iwad.IsEmpty()) iwad = lastIWAD;

		if (iwad_man == NULL)
		{
			iwad_man = new FIWadManager(basewad, optionalwad);
		}
		const FIWADInfo *iwad_info = iwad_man->FindIWAD(allwads, iwad, basewad, optionalwad);
		if (!iwad_info) return 0;	// user exited the selection popup via cancel button.
		gameinfo.gametype = iwad_info->gametype;
		gameinfo.flags = iwad_info->flags;
		gameinfo.nokeyboardcheats = iwad_info->nokeyboardcheats;
		gameinfo.ConfigName = iwad_info->Configname;
		lastIWAD = iwad;


		if ((gameinfo.flags & GI_SHAREWARE) && pwads.Size() > 0)
		{
			I_FatalError ("You cannot -file with the shareware version. Register!");
		}

		FBaseCVar::DisableCallbacks();
		GameConfig->DoGameSetup (gameinfo.ConfigName);

		AddAutoloadFiles(iwad_info->Autoname);

		// Process automatically executed files
		FExecList *exec;
		FArgs *execFiles = new FArgs;
		if (!(Args->CheckParm("-noautoexec")))
			GameConfig->AddAutoexec(execFiles, gameinfo.ConfigName);
		exec = D_MultiExec(execFiles, NULL);
		delete execFiles;

		// Process .cfg files at the start of the command line.
		execFiles = Args->GatherFiles ("-exec");
		exec = D_MultiExec(execFiles, exec);
		delete execFiles;

		// [RH] process all + commands on the command line
		exec = C_ParseCmdLineParams(exec);

		CopyFiles(allwads, pwads);
		if (exec != NULL)
		{
			exec->AddPullins(allwads, GameConfig);
		}

		// Since this function will never leave we must delete this array here manually.
		pwads.Clear();
		pwads.ShrinkToFit();

		if (hashfile)
		{
			Printf("Notice: File hashing is incredibly verbose. Expect loading files to take much longer than usual.\n");
		}

		if (!batchrun) Printf ("W_Init: Init WADfiles.\n");

		LumpFilterInfo lfi;
		lfi.dotFilter = LumpFilterIWAD;

		static const struct { int match; const char* name; } blanket[] =
		{
			{ GAME_Raven,			"game-Raven" },
			{ GAME_DoomStrifeChex,	"game-DoomStrifeChex" },
			{ GAME_DoomChex,		"game-DoomChex" },
			{ GAME_Any, NULL }
		};

		for (auto& inf : blanket)
		{
			if (gameinfo.gametype & inf.match) lfi.gameTypeFilter.Push(inf.name);
		}
		lfi.gameTypeFilter.Push(FStringf("game-%s", GameTypeName()));

		for (auto p : iwad_folders) lfi.reservedFolders.Push(p);
		for (auto p : iwad_reserved) lfi.requiredPrefixes.Push(p);

		lfi.postprocessFunc = [&]()
		{
			RenameNerve(fileSystem);
			RenameSprites(fileSystem, iwad_info->DeleteLumps);
			FixMacHexen(fileSystem);
			FindStrifeTeaserVoices(fileSystem);
		};

		fileSystem.InitMultipleFiles (allwads, false, &lfi);
		allwads.Clear();
		allwads.ShrinkToFit();
		SetMapxxFlag();

		D_GrabCVarDefaults(); //parse DEFCVARS

		GameConfig->DoKeySetup(gameinfo.ConfigName);

		// Now that wads are loaded, define mod-specific cvars.
		ParseCVarInfo();

		// Actually exec command line commands and exec files.
		if (exec != NULL)
		{
			exec->ExecCommands();
			delete exec;
			exec = NULL;
		}

		// [RH] Initialize localizable strings.
		GStrings.LoadStrings (language);

		V_InitFontColors ();

		// [RH] Moved these up here so that we can do most of our
		//		startup output in a fullscreen console.

		CT_Init ();

		if (!restart)
		{
			if (!batchrun) Printf ("I_Init: Setting up machine state.\n");
			CheckCPUID(&CPU);
			CalculateCPUSpeed();
			auto ci = DumpCPUInfo(&CPU);
			Printf("%s", ci.GetChars());
		}

		// [RH] Initialize palette management
		InitPalette ();
		
		if (!batchrun) Printf ("V_Init: allocate screen.\n");
		if (!restart)
		{
			V_InitScreenSize();
		}
		
		if (!restart)
		{
			// This allocates a dummy framebuffer as a stand-in until V_Init2 is called.
			V_InitScreen ();
		}
		
		if (restart)
		{
			// Update screen palette when restarting
			screen->UpdatePalette();
		}

		// Base systems have been inited; enable cvar callbacks
		FBaseCVar::EnableCallbacks ();

		if (!batchrun) Printf ("S_Init: Setting up sound.\n");
		S_Init ();

		if (!batchrun) Printf ("ST_Init: Init startup screen.\n");
		if (!restart)
		{
			if (GameStartupInfo.Type == FStartupInfo::DefaultStartup)
			{
				switch (gameinfo.gametype)
				{
				case GAME_Hexen:
					GameStartupInfo.Type = FStartupInfo::HexenStartup;
					break;

				case GAME_Heretic:
					GameStartupInfo.Type = FStartupInfo::HereticStartup;
					break;

				case GAME_Strife:
					GameStartupInfo.Type = FStartupInfo::StrifeStartup;
					break;

				default:
					break;
				}
			}
			StartScreen = FStartupScreen::CreateInstance (TexMan.GuesstimateNumTextures() + 5);
		}
		else
		{
			StartScreen = new FStartupScreen(0);
		}

		CheckCmdLine();

		// [RH] Load sound environments
		S_ParseReverbDef ();

		// [RH] Parse any SNDINFO lumps
		if (!batchrun) Printf ("S_InitData: Load sound definitions.\n");
		S_InitData ();

		// [RH] Parse through all loaded mapinfo lumps
		if (!batchrun) Printf ("G_ParseMapInfo: Load map definitions.\n");
		G_ParseMapInfo (iwad_info->MapInfo);
		MessageBoxClass = gameinfo.MessageBoxClass;
		endoomName = gameinfo.Endoom;
		menuBlurAmount = gameinfo.bluramount;
		ReadStatistics();

		// MUSINFO must be parsed after MAPINFO
		S_ParseMusInfo();

		if (!batchrun) Printf ("Texman.Init: Init texture manager.\n");
		UpdateUpscaleMask();
		SpriteFrames.Clear();
		TexMan.Init([]() { StartScreen->Progress(); }, CheckForHacks);
		PatchTextures();
		TexAnim.Init();
		C_InitConback(TexMan.CheckForTexture(gameinfo.BorderFlat, ETextureType::Flat), true, 0.25);

		FixWideStatusBar();

		StartScreen->Progress();
		V_InitFonts();
		InitDoomFonts();
		V_LoadTranslations();
		UpdateGenericUI(false);

		// [CW] Parse any TEAMINFO lumps.
		if (!batchrun) Printf ("ParseTeamInfo: Load team definitions.\n");
		TeamLibrary.ParseTeamInfo ();

		R_ParseTrnslate();
		PClassActor::StaticInit ();

		// [GRB] Initialize player class list
		SetupPlayerClasses ();

		// [RH] Load custom key and weapon settings from WADs
		D_LoadWadSettings ();

		// [GRB] Check if someone used clearplayerclasses but not addplayerclass
		if (PlayerClasses.Size () == 0)
		{
			I_FatalError ("No player classes defined");
		}

		StartScreen->Progress ();

		ParseGLDefs();

		if (!batchrun) Printf ("R_Init: Init %s refresh subsystem.\n", gameinfo.ConfigName.GetChars());
		StartScreen->LoadingStatus ("Loading graphics", 0x3f);
		R_Init ();

		if (!batchrun) Printf ("DecalLibrary: Load decals.\n");
		DecalLibrary.ReadAllDecals ();

		// Load embedded Dehacked patches
		D_LoadDehLumps(FromIWAD);

		// [RH] Add any .deh and .bex files on the command line.
		// If there are none, try adding any in the config file.
		// Note that the command line overrides defaults from the config.

		if ((ConsiderPatches("-deh") | ConsiderPatches("-bex")) == 0 &&
			gameinfo.gametype == GAME_Doom && GameConfig->SetSection ("Doom.DefaultDehacked"))
		{
			const char *key;
			const char *value;

			while (GameConfig->NextInSection (key, value))
			{
				if (stricmp (key, "Path") == 0 && FileExists (value))
				{
					if (!batchrun) Printf ("Applying patch %s\n", value);
					D_LoadDehFile(value);
				}
			}
		}

		// Load embedded Dehacked patches
		D_LoadDehLumps(FromPWADs);

		// Create replacements for dehacked pickups
		FinishDehPatch();

		if (!batchrun) Printf("M_Init: Init menus.\n");
		SetDefaultMenuColors();
		M_Init();
		M_CreateGameMenus();


		// clean up the compiler symbols which are not needed any longer.
		RemoveUnusedSymbols();

		InitActorNumsFromMapinfo();
		InitSpawnablesFromMapinfo();
		PClassActor::StaticSetActorNums();

		//Added by MC:
		primaryLevel->BotInfo.getspawned.Clear();
		argcount = Args->CheckParmList("-bots", &args);
		for (p = 0; p < argcount; ++p)
		{
			primaryLevel->BotInfo.getspawned.Push(args[p]);
		}
		primaryLevel->BotInfo.spawn_tries = 0;
		primaryLevel->BotInfo.wanted_botnum = primaryLevel->BotInfo.getspawned.Size();

		if (!batchrun) Printf ("P_Init: Init Playloop state.\n");
		StartScreen->LoadingStatus ("Init game engine", 0x3f);
		AM_StaticInit();
		P_Init ();

		P_SetupWeapons_ntohton();

		//SBarInfo support. Note that the first SBARINFO lump contains the mugshot definition so it even needs to be read when a regular status bar is being used.
		SBarInfo::Load();

		if (!batchrun)
		{
			// [RH] User-configurable startup strings. Because BOOM does.
			static const char *startupString[5] = {
				"STARTUP1", "STARTUP2", "STARTUP3", "STARTUP4", "STARTUP5"
			};
			for (p = 0; p < 5; ++p)
			{
				// At this point we cannot use the player's gender info yet so force 'male' here.
				const char *str = GStrings.GetString(startupString[p], nullptr, 0);
				if (str != NULL && str[0] != '\0')
				{
					Printf("%s\n", str);
				}
			}
		}

		if (!restart)
		{
			if (!batchrun) Printf ("D_CheckNetGame: Checking network game status.\n");
			StartScreen->LoadingStatus ("Checking network game status.", 0x3f);
			if (!D_CheckNetGame ())
			{
				return 0;
			}
		}

		// [SP] Force vanilla transparency auto-detection to re-detect our game lumps now
		UpdateVanillaTransparency();

		// [RH] Lock any cvars that should be locked now that we're
		// about to begin the game.
		FBaseCVar::EnableNoSet ();

		delete iwad_man;	// now we won't need this anymore
		iwad_man = NULL;

		// [RH] Run any saved commands from the command line or autoexec.cfg now.
		gamestate = GS_FULLCONSOLE;
		Net_NewMakeTic ();
		C_RunDelayedCommands();
		gamestate = GS_STARTUP;

		// enable custom invulnerability map here
		if (cl_customizeinvulmap)
			R_UpdateInvulnerabilityColormap();

		if (!restart)
		{
			// start the apropriate game based on parms
			v = Args->CheckValue ("-record");

			if (v)
			{
				G_RecordDemo (v);
				autostart = true;
			}

			delete StartScreen;
			StartScreen = NULL;
			S_Sound (CHAN_BODY, 0, "misc/startupdone", 1, ATTN_NONE);

			if (Args->CheckParm("-norun") || batchrun)
			{
				return 1337; // special exit
			}

			V_Init2();
			twod->fullscreenautoaspect = gameinfo.fullscreenautoaspect;
			// Initialize the size of the 2D drawer so that an attempt to access it outside the draw code won't crash.
			twod->Begin(screen->GetWidth(), screen->GetHeight());
			twod->End();
			UpdateJoystickMenu(NULL);
			UpdateVRModes();

			v = Args->CheckValue ("-loadgame");
			if (v)
			{
				FString file(v);
				FixPathSeperator (file);
				DefaultExtension (file, "." SAVEGAME_EXT);
				G_LoadGame (file);
			}

			v = Args->CheckValue("-playdemo");
			if (v != NULL)
			{
				singledemo = true;				// quit after one demo
				G_DeferedPlayDemo (v);
				D_DoomLoop ();	// never returns
			}
			else
			{
				v = Args->CheckValue("-timedemo");
				if (v)
				{
					G_TimeDemo(v);
					D_DoomLoop();	// never returns
				}
				else
				{
					if (gameaction != ga_loadgame && gameaction != ga_loadgamehidecon)
					{
						if (autostart || netgame)
						{
							// Do not do any screenwipes when autostarting a game.
							if (!Args->CheckParm("-warpwipe"))
							{
								NoWipe = TICRATE;
							}
							CheckWarpTransMap(startmap, true);
							if (demorecording)
								G_BeginRecording(startmap);
							G_InitNew(startmap, false);
							if (StoredWarp.IsNotEmpty())
							{
								AddCommandString(StoredWarp);
								StoredWarp = "";
							}
						}
						else
						{
							D_StartTitle();				// start up intro loop
						}
					}
					else if (demorecording)
					{
						G_BeginRecording(NULL);
					}
				}
			}
		}
		else
		{
			// These calls from inside V_Init2 are still necessary
			C_NewModeAdjust();
			D_StartTitle ();				// start up intro loop
			setmodeneeded = false;			// This may be set to true here, but isn't needed for a restart
		}

		D_DoAnonStats();

		I_UpdateWindowTitle();

		D_DoomLoop ();		// this only returns if a 'restart' CCMD is given.
		// 
		// Clean up after a restart
		//

		D_Cleanup();

		gamestate = GS_STARTUP;
	}
	while (1);
}

int GameMain()
{
	int ret = 0;
	GameTicRate = TICRATE;

	ConsoleCallbacks cb = {
		D_UserInfoChanged,
		D_SendServerInfoChange,
		D_SendServerFlagChange,
		G_GetUserCVar,
		[]() { return gamestate != GS_FULLCONSOLE && gamestate != GS_STARTUP; }
	};

	C_InstallHandlers(&cb);
	SetConsoleNotifyBuffer();

	try
	{
		ret = D_DoomMain_Internal();
	}
	catch (const CExitEvent &exit)	// This is a regular exit initiated from deeply nested code.
	{
		ret = exit.Reason();
	}
	catch (const std::exception &error)
	{
		I_ShowFatalError(error.what());
		ret = -1;
	}
	// Unless something really bad happened, the game should only exit through this single point in the code.
	// No more 'exit', please.
	D_Cleanup();
	CloseNetwork();
	GC::FinalGC = true;
	GC::FullGC();
	GC::DelSoftRootHead();	// the soft root head will not be collected by a GC so we have to do it explicitly
	C_DeinitConsole();
	R_DeinitColormaps();
	R_Shutdown();
	I_ShutdownGraphics();
	I_ShutdownInput();
	M_SaveDefaultsFinal();
	DeleteStartupScreen();
	delete Args;
	Args = nullptr;
	return ret;
}

//==========================================================================
//
// clean up the resources
//
//==========================================================================

void D_Cleanup()
{
	if (demorecording)
	{
		G_CheckDemoStatus();
	}

	// Music and sound should be stopped first
	S_StopMusic(true);
	S_ClearSoundData();
	S_UnloadReverbDef();
	G_ClearMapinfo();

	M_ClearMenus();					// close menu if open
	F_EndFinale();					// If an intermission is active, end it now
	AM_ClearColorsets();
	DeinitSWColorMaps();
	FreeSBarInfoScript();
	
	// clean up game state
	ST_Clear();
	D_ErrorCleanup ();
	P_Shutdown();
	
	M_SaveDefaults(NULL);			// save config before the restart
	
	// delete all data that cannot be left until reinitialization
	CleanSWDrawer();
	V_ClearFonts();					// must clear global font pointers
	ColorSets.Clear();
	PainFlashes.Clear();
	gameinfo.~gameinfo_t();
	new (&gameinfo) gameinfo_t;		// Reset gameinfo
	S_Shutdown();					// free all channels and delete playlist
	C_ClearAliases();				// CCMDs won't be reinitialized so these need to be deleted here
	DestroyCVarsFlagged(CVAR_MOD);	// Delete any cvar left by mods
	DeinitMenus();
	savegameManager.ClearSaveGames();
	LightDefaults.DeleteAndClear();			// this can leak heap memory if it isn't cleared.
	TexAnim.DeleteAll();
	TexMan.DeleteAll();
	
	// delete GameStartupInfo data
	GameStartupInfo.Name = "";
	GameStartupInfo.BkColor = GameStartupInfo.FgColor = GameStartupInfo.Type = 0;
	GameStartupInfo.LoadWidescreen = GameStartupInfo.LoadLights = GameStartupInfo.LoadBrightmaps = -1;
	
	GC::FullGC();					// clean up before taking down the object list.
	
	// Delete the reference to the VM functions here which were deleted and will be recreated after the restart.
	AutoSegs::ActionFunctons.ForEach([](AFuncDesc *afunc)
	{
		*(afunc->VMPointer) = NULL;
	});
	
	GC::DelSoftRootHead();
	
	for (auto& p : players)
	{
		p.PendingWeapon = nullptr;
	}
	PClassActor::AllActorClasses.Clear();
	ScriptUtil::Clear();
	PClass::StaticShutdown();
	
	GC::FullGC();					// perform one final garbage collection after shutdown
	
	assert(GC::Root == nullptr);
	
	restart++;
	PClass::bShutdown = false;
	PClass::bVMOperational = false;
}

//==========================================================================
//
// restart the game
//
//==========================================================================

UNSAFE_CCMD(restart)
{
	// remove command line args that would get in the way during restart
	Args->RemoveArgs("-iwad");
	Args->RemoveArgs("-deh");
	Args->RemoveArgs("-bex");
	Args->RemoveArgs("-playdemo");
	Args->RemoveArgs("-file");
	Args->RemoveArgs("-altdeath");
	Args->RemoveArgs("-deathmatch");
	Args->RemoveArgs("-skill");
	Args->RemoveArgs("-savedir");
	Args->RemoveArgs("-xlat");
	Args->RemoveArgs("-oldsprites");

	if (argv.argc() > 1)
	{
		for (int i = 1; i<argv.argc(); i++)
		{
			Args->AppendArg(argv[i]);
		}
	}

	wantToRestart = true;
}

DEFINE_FIELD_X(InputEventData, event_t, type)
DEFINE_FIELD_X(InputEventData, event_t, subtype)
DEFINE_FIELD_X(InputEventData, event_t, data1)
DEFINE_FIELD_X(InputEventData, event_t, data2)
DEFINE_FIELD_X(InputEventData, event_t, data3)
DEFINE_FIELD_X(InputEventData, event_t, x)
DEFINE_FIELD_X(InputEventData, event_t, y)

CUSTOM_CVAR(Int, I_FriendlyWindowTitle, 1, CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	I_UpdateWindowTitle();
}

void I_UpdateWindowTitle()
{
	FString titlestr;
	switch (I_FriendlyWindowTitle)
	{
	case 1:
		if (level.LevelName.IsNotEmpty())
		{
			titlestr.Format("%s - %s", level.LevelName.GetChars(), GameStartupInfo.Name.GetChars());
			break;
		}
		[[fallthrough]];
	case 2:
		titlestr = GameStartupInfo.Name;
		break;
	default:
		I_SetWindowTitle(NULL);
		return;
	}

	// Strip out any color escape sequences before setting a window title
	TArray<char> copy(titlestr.Len() + 1);
	const char* srcp = titlestr;
	char* dstp = copy.Data();

	while (*srcp != 0)
	{

		if (*srcp != TEXTCOLOR_ESCAPE)
		{
			*dstp++ = *srcp++;
		}
		else if (srcp[1] == '[')
		{
			srcp += 2;
			while (*srcp != ']' && *srcp != 0) srcp++;
			if (*srcp == ']') srcp++;
		}
		else
		{
			if (srcp[1] != 0) srcp += 2;
			else break;
		}
	}
	*dstp = 0;
	I_SetWindowTitle(copy.Data());
}

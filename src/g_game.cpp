//-----------------------------------------------------------------------------
//
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
//-----------------------------------------------------------------------------
//



#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <memory>

#include "i_time.h"
#include "templates.h"
#include "version.h"
#include "doomdef.h" 
#include "doomstat.h"
#include "d_protocol.h"
#include "d_netinf.h"
#include "intermission/intermission.h"
#include "m_argv.h"
#include "m_misc.h"
#include "menu.h"
#include "m_crc32.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "d_main.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "c_console.h"
#include "c_bind.h"
#include "c_dispatch.h"
#include "filesystem.h"
#include "p_local.h" 
#include "gstrings.h"
#include "r_sky.h"
#include "g_game.h"
#include "sbar.h"
#include "m_png.h"
#include "a_keys.h"
#include "cmdlib.h"
#include "d_net.h"
#include "d_event.h"
#include "p_acs.h"
#include "p_effect.h"
#include "m_joy.h"
#include "r_utility.h"
#include "a_morph.h"
#include "p_spec.h"
#include "serializer_doom.h"
#include "vm.h"
#include "dobjgc.h"
#include "gi.h"
#include "a_dynlight.h"
#include "i_system.h"
#include "p_conversation.h"
#include "v_palette.h"
#include "s_music.h"
#include "p_setup.h"

#include "v_video.h"
#include "g_hub.h"
#include "g_levellocals.h"
#include "events.h"
#include "c_buttons.h"
#include "d_buttons.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "doommenu.h"


static FRandom pr_dmspawn ("DMSpawn");
static FRandom pr_pspawn ("PlayerSpawn");

bool	G_CheckDemoStatus (void);
void	G_ReadDemoTiccmd (ticcmd_t *cmd, int player);
void	G_WriteDemoTiccmd (ticcmd_t *cmd, int player, int buf);
void	G_PlayerReborn (int player);

void	G_DoNewGame (void);
void	G_DoLoadGame (void);
void	G_DoPlayDemo (void);
void	G_DoCompleted (void);
void	G_DoVictory (void);
void	G_DoWorldDone (void);
void	G_DoSaveGame (bool okForQuicksave, bool forceQuicksave, FString filename, const char *description);
void	G_DoAutoSave ();
void	G_DoQuickSave ();

void STAT_Serialize(FSerializer &file);
bool WriteZip(const char *filename, TArray<FString> &filenames, TArray<FCompressedBuffer> &content);

FIntCVar gameskill ("skill", 2, CVAR_SERVERINFO|CVAR_LATCH);
CVAR(Bool, save_formatted, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)	// use formatted JSON for saves (more readable but a larger files and a bit slower.
CVAR (Int, deathmatch, 0, CVAR_SERVERINFO|CVAR_LATCH);
CVAR (Bool, chasedemo, false, 0);
CVAR (Bool, storesavepic, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, longsavemessages, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, save_dir, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR (Bool, cl_waitforsave, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, enablescriptscreenshot, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
EXTERN_CVAR (Float, con_midtime);

//==========================================================================
//
// CVAR displaynametags
//
// Selects whether to display name tags or not when changing weapons/items
//
//==========================================================================

CUSTOM_CVAR (Int, displaynametags, 0, CVAR_ARCHIVE)
{
	if (self < 0 || self > 3)
	{
		self = 0;
	}
}

CVAR(Int, nametagcolor, CR_GOLD, CVAR_ARCHIVE)

extern bool playedtitlemusic;

gameaction_t	gameaction;
gamestate_t 	gamestate = GS_STARTUP;
FName			SelectedSlideshow;		// what to start when ga_slideshow

int 			paused;
bool			pauseext;
bool 			sendpause;				// send a pause event next tic 
bool			sendsave;				// send a save event next tic 
bool			sendturn180;			// [RH] send a 180 degree turn next tic
bool 			usergame;				// ok to save / end game
bool			insave;					// Game is saving - used to block exit commands

bool			timingdemo; 			// if true, exit with report on completion 
bool 			nodrawers;				// for comparative timing purposes 
bool 			noblit; 				// for comparative timing purposes 

bool	 		viewactive;

bool			multiplayernext = false;		// [SP] Map coop/dm implementation
player_t		players[MAXPLAYERS];
bool			playeringame[MAXPLAYERS];

int 			gametic;

CVAR(Bool, demo_compress, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
FString			newdemoname;
FString			newdemomap;
FString			demoname;
bool 			demorecording;
bool 			demoplayback;
bool			demonew;				// [RH] Only used around G_InitNew for demos
int				demover;
uint8_t*			demobuffer;
uint8_t*			demo_p;
uint8_t*			democompspot;
uint8_t*			demobodyspot;
size_t			maxdemosize;
uint8_t*			zdemformend;			// end of FORM ZDEM chunk
uint8_t*			zdembodyend;			// end of ZDEM BODY chunk
bool 			singledemo; 			// quit after playing a demo from cmdline 
 
bool 			precache = true;		// if true, load all graphics at start 
  
short			consistancy[MAXPLAYERS][BACKUPTICS];
 
 
#define MAXPLMOVE				(forwardmove[1]) 
 
#define TURBOTHRESHOLD	12800

EXTERN_CVAR (Int, turnspeedwalkfast)
EXTERN_CVAR (Int, turnspeedsprintfast)
EXTERN_CVAR (Int, turnspeedwalkslow)
EXTERN_CVAR (Int, turnspeedsprintslow)

int				forwardmove[2], sidemove[2];
FIntCVar		*angleturn[4] = {&turnspeedwalkfast, &turnspeedsprintfast, &turnspeedwalkslow, &turnspeedsprintslow};
int				flyspeed[2] = {1*256, 3*256};
int				lookspeed[2] = {450, 512};

#define SLOWTURNTICS	6 

CVAR (Bool,		cl_run,			false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always run?
CVAR (Bool,		invertmouse,	false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Invert mouse look down/up?
CVAR (Bool,		invertmousex,	false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Invert mouse look left/right?
CVAR (Bool,		freelook,		true,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always mlook?
CVAR (Bool,		lookstrafe,		false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always strafe with mouse?
CVAR (Float,	m_forward,		1.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR (Float,	m_side,			2.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
 
int 			turnheld;								// for accelerative turning 
 
// mouse values are used once 
float 			mousex;
float 			mousey; 		

FString			savegamefile;
FString			savedescription;

// [RH] Name of screenshot file to generate (usually NULL)
FString			shotfile;

FString savename;
FString BackupSaveName;

bool SendLand;
const AActor *SendItemUse, *SendItemDrop;
int SendItemDropAmount;

extern uint8_t globalfreeze;

EXTERN_CVAR (Int, team)

CVAR (Bool, teamplay, false, CVAR_SERVERINFO)

// Workaround for x64 code generation bug in MSVC 2015 
// Optimized targets contain illegal instructions in the function below
#if defined _M_X64 && _MSC_VER < 1910
#pragma optimize("", off)
#endif // _M_X64 && _MSC_VER < 1910

// [RH] Allow turbo setting anytime during game
CUSTOM_CVAR (Float, turbo, 100.f, CVAR_NOINITCALL)
{
	if (self < 10.f)
	{
		self = 10.f;
	}
	else if (self > 255.f)
	{
		self = 255.f;
	}
	else
	{
		double scale = self * 0.01;

		forwardmove[0] = (int)(gameinfo.normforwardmove[0]*scale);
		forwardmove[1] = (int)(gameinfo.normforwardmove[1]*scale);
		sidemove[0] = (int)(gameinfo.normsidemove[0]*scale);
		sidemove[1] = (int)(gameinfo.normsidemove[1]*scale);
	}
}

#if defined _M_X64 && _MSC_VER < 1910
#pragma optimize("", on)
#endif // _M_X64 && _MSC_VER < 1910

CCMD (turnspeeds)
{
	if (argv.argc() == 1)
	{
		Printf ("Current turn speeds:\n"
				TEXTCOLOR_BLUE " turnspeedwalkfast: " TEXTCOLOR_GREEN " %d\n"
				TEXTCOLOR_BLUE " turnspeedsprintfast: " TEXTCOLOR_GREEN " %d\n"
				TEXTCOLOR_BLUE " turnspeedwalkslow: " TEXTCOLOR_GREEN " %d\n"
				TEXTCOLOR_BLUE " turnspeedsprintslow: " TEXTCOLOR_GREEN " %d\n", *turnspeedwalkfast,
			*turnspeedsprintfast, *turnspeedwalkslow, *turnspeedsprintslow);
	}
	else
	{
		int i;

		for (i = 1; i <= 4 && i < argv.argc(); ++i)
		{
			*angleturn[i-1] = atoi (argv[i]);
		}
		if (i <= 2)
		{
			*angleturn[1] = *angleturn[0] * 2;
		}
		if (i <= 3)
		{
			*angleturn[2] = *angleturn[0] / 2;
		}
		if (i <= 4)
		{
			*angleturn[3] = *angleturn[2];
		}
	}
}

CCMD (slot)
{
	if (argv.argc() > 1)
	{
		int slot = atoi (argv[1]);

		auto mo = players[consoleplayer].mo;
		if (slot < NUM_WEAPON_SLOTS && mo)
		{
			// Needs to be redone
			IFVIRTUALPTRNAME(mo, NAME_PlayerPawn, PickWeapon)
			{
				VMValue param[] = { mo, slot, !(dmflags2 & DF2_DONTCHECKAMMO) };
				VMReturn ret((void**)&SendItemUse);
				VMCall(func, param, 3, &ret, 1);
			}
		}

		// [Nash] Option to display the name of the weapon being switched to.
		if ((paused || pauseext) || players[consoleplayer].playerstate != PST_LIVE) return;
		if (SendItemUse != players[consoleplayer].ReadyWeapon && (displaynametags & 2) && StatusBar && SmallFont && SendItemUse)
		{
			StatusBar->AttachMessage(Create<DHUDMessageFadeOut>(nullptr, SendItemUse->GetTag(),
				1.5f, 0.90f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID('W', 'E', 'P', 'N'));
		}
	}
}

CCMD (centerview)
{
	Net_WriteByte (DEM_CENTERVIEW);
}

CCMD(crouch)
{
	Net_WriteByte(DEM_CROUCH);
}

CCMD (land)
{
	SendLand = true;
}

CCMD (pause)
{
	sendpause = true;
}

CCMD (turn180)
{
	sendturn180 = true;
}

CCMD (weapnext)
{
	auto mo = players[consoleplayer].mo;
	if (mo)
	{
		// Needs to be redone
		IFVIRTUALPTRNAME(mo, NAME_PlayerPawn, PickNextWeapon)
		{
			VMValue param[] = { mo };
			VMReturn ret((void**)&SendItemUse);
			VMCall(func, param, 1, &ret, 1);
		}
	}

	// [BC] Option to display the name of the weapon being cycled to.
	if ((paused || pauseext) || players[consoleplayer].playerstate != PST_LIVE) return;
	if ((displaynametags & 2) && StatusBar && SmallFont && SendItemUse)
	{
		StatusBar->AttachMessage(Create<DHUDMessageFadeOut>(nullptr, SendItemUse->GetTag(),
			1.5f, 0.90f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID( 'W', 'E', 'P', 'N' ));
	}
	if (SendItemUse != players[consoleplayer].ReadyWeapon)
	{
		S_Sound(CHAN_AUTO, 0, "misc/weaponchange", 1.0, ATTN_NONE);
	}
}

CCMD (weapprev)
{
	auto mo = players[consoleplayer].mo;
	if (mo)
	{
		// Needs to be redone
		IFVIRTUALPTRNAME(mo, NAME_PlayerPawn, PickPrevWeapon)
		{
			VMValue param[] = { mo };
			VMReturn ret((void**)&SendItemUse);
			VMCall(func, param, 1, &ret, 1);
		}
	}

	// [BC] Option to display the name of the weapon being cycled to.
	if ((paused || pauseext) || players[consoleplayer].playerstate != PST_LIVE) return;
	if ((displaynametags & 2) && StatusBar && SmallFont && SendItemUse)
	{
		StatusBar->AttachMessage(Create<DHUDMessageFadeOut>(nullptr, SendItemUse->GetTag(),
			1.5f, 0.90f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID( 'W', 'E', 'P', 'N' ));
	}
	if (SendItemUse != players[consoleplayer].ReadyWeapon)
	{
		S_Sound(CHAN_AUTO, 0, "misc/weaponchange", 1.0, ATTN_NONE);
	}
}

static void DisplayNameTag(AActor *actor)
{
	auto tag = actor->GetTag();
	if ((displaynametags & 1) && StatusBar && SmallFont)
		StatusBar->AttachMessage(Create<DHUDMessageFadeOut>(nullptr, tag,
			1.5f, 0.80f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID('S', 'I', 'N', 'V'));

}

DEFINE_ACTION_FUNCTION_NATIVE(AActor, DisplayNameTag, DisplayNameTag)
{
	PARAM_SELF_PROLOGUE(AActor);
	DisplayNameTag(self);
	return 0;
}

CCMD (invnext)
{
	if (players[consoleplayer].mo != nullptr)
	{
		IFVM(PlayerPawn, InvNext)
		{
			VMValue param = players[consoleplayer].mo;
			VMCall(func, &param, 1, nullptr, 0);
		}
	}
}

CCMD(invprev)
{
	if (players[consoleplayer].mo != nullptr)
	{
		IFVM(PlayerPawn, InvPrev)
		{
			VMValue param = players[consoleplayer].mo;
			VMCall(func, &param, 1, nullptr, 0);
		}
	}
}

CCMD (invuseall)
{
	SendItemUse = (const AActor *)1;
}

CCMD (invuse)
{
	if (players[consoleplayer].inventorytics == 0)
	{
		if (players[consoleplayer].mo) SendItemUse = players[consoleplayer].mo->PointerVar<AActor>(NAME_InvSel);
	}
	players[consoleplayer].inventorytics = 0;
}

CCMD(invquery)
{
	AActor *inv = players[consoleplayer].mo->PointerVar<AActor>(NAME_InvSel);
	if (inv != NULL)
	{
		Printf(PRINT_HIGH, "%s (%dx)\n", inv->GetTag(), inv->IntVar(NAME_Amount));
	}
}

CCMD (use)
{
	if (argv.argc() > 1 && players[consoleplayer].mo != NULL)
	{
		SendItemUse = players[consoleplayer].mo->FindInventory(argv[1]);
	}
}

CCMD (invdrop)
{
	if (players[consoleplayer].mo)
	{
		SendItemDrop = players[consoleplayer].mo->PointerVar<AActor>(NAME_InvSel);
		SendItemDropAmount = -1;
	}
}

CCMD (weapdrop)
{
	SendItemDrop = players[consoleplayer].ReadyWeapon;
	SendItemDropAmount = -1;
}

CCMD (drop)
{
	if (argv.argc() > 1 && players[consoleplayer].mo != NULL)
	{
		SendItemDrop = players[consoleplayer].mo->FindInventory(argv[1]);
		SendItemDropAmount = argv.argc() > 2 ? atoi(argv[2]) : -1;
	}
}

CCMD (useflechette)
{ 
	if (players[consoleplayer].mo == nullptr) return;
	IFVIRTUALPTRNAME(players[consoleplayer].mo, NAME_PlayerPawn, GetFlechetteItem)
	{
		VMValue params[] = { players[consoleplayer].mo };
		AActor *cls;
		VMReturn ret((void**)&cls);
		VMCall(func, params, 1, &ret, 1);

		if (cls != nullptr) SendItemUse = cls;
	}
}

CCMD (select)
{
	if (!players[consoleplayer].mo) return;
	auto user = players[consoleplayer].mo;
	if (argv.argc() > 1)
	{
		auto item = user->FindInventory(argv[1]);
		if (item != NULL)
		{
			user->PointerVar<AActor>(NAME_InvSel) = item;
		}
	}
	user->player->inventorytics = 5*TICRATE;
}

static inline int joyint(double val)
{
	if (val >= 0)
	{
		return int(ceil(val));
	}
	else
	{
		return int(floor(val));
	}
}

FBaseCVar* G_GetUserCVar(int playernum, const char* cvarname)
{
	if ((unsigned)playernum >= MAXPLAYERS || !playeringame[playernum])
	{
		return nullptr;
	}
	FBaseCVar** cvar_p = players[playernum].userinfo.CheckKey(FName(cvarname, true));
	FBaseCVar* cvar;
	if (cvar_p == nullptr || (cvar = *cvar_p) == nullptr || (cvar->GetFlags() & CVAR_IGNORE))
	{
		return nullptr;
	}
	return cvar;
}

static ticcmd_t emptycmd;

ticcmd_t* G_BaseTiccmd()
{
	return &emptycmd;
}


//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
void G_BuildTiccmd (ticcmd_t *cmd)
{
	int 		strafe;
	int 		speed;
	int 		forward;
	int 		side;
	int			fly;

	ticcmd_t	*base;

	base = G_BaseTiccmd (); 
	*cmd = *base;

	cmd->consistancy = consistancy[consoleplayer][(maketic/ticdup)%BACKUPTICS];

	strafe = buttonMap.ButtonDown(Button_Strafe);
	speed = buttonMap.ButtonDown(Button_Speed) ^ (int)cl_run;

	forward = side = fly = 0;

	// [RH] only use two stage accelerative turning on the keyboard
	//		and not the joystick, since we treat the joystick as
	//		the analog device it is.
	if (buttonMap.ButtonDown(Button_Left) || buttonMap.ButtonDown(Button_Right))
		turnheld += ticdup;
	else
		turnheld = 0;

	// let movement keys cancel each other out
	if (strafe)
	{
		if (buttonMap.ButtonDown(Button_Right))
			side += sidemove[speed];
		if (buttonMap.ButtonDown(Button_Left))
			side -= sidemove[speed];
	}
	else
	{
		int tspeed = speed;

		if (turnheld < SLOWTURNTICS)
			tspeed += 2;		// slow turn
		
		if (buttonMap.ButtonDown(Button_Right))
		{
			G_AddViewAngle (*angleturn[tspeed]);
		}
		if (buttonMap.ButtonDown(Button_Left))
		{
			G_AddViewAngle (-*angleturn[tspeed]);
		}
	}

	if (buttonMap.ButtonDown(Button_LookUp))
	{
		G_AddViewPitch (lookspeed[speed]);
	}
	if (buttonMap.ButtonDown(Button_LookDown))
	{
		G_AddViewPitch (-lookspeed[speed]);
	}

	if (buttonMap.ButtonDown(Button_MoveUp))
		fly += flyspeed[speed];
	if (buttonMap.ButtonDown(Button_MoveDown))
		fly -= flyspeed[speed];

	if (buttonMap.ButtonDown(Button_Klook))
	{
		if (buttonMap.ButtonDown(Button_Forward))
			G_AddViewPitch (lookspeed[speed]);
		if (buttonMap.ButtonDown(Button_Back))
			G_AddViewPitch (-lookspeed[speed]);
	}
	else
	{
		if (buttonMap.ButtonDown(Button_Forward))
			forward += forwardmove[speed];
		if (buttonMap.ButtonDown(Button_Back))
			forward -= forwardmove[speed];
	}

	if (buttonMap.ButtonDown(Button_MoveRight))
		side += sidemove[speed];
	if (buttonMap.ButtonDown(Button_MoveLeft))
		side -= sidemove[speed];

	// buttons
	if (buttonMap.ButtonDown(Button_Attack))		cmd->ucmd.buttons |= BT_ATTACK;
	if (buttonMap.ButtonDown(Button_AltAttack))		cmd->ucmd.buttons |= BT_ALTATTACK;
	if (buttonMap.ButtonDown(Button_Use))			cmd->ucmd.buttons |= BT_USE;
	if (buttonMap.ButtonDown(Button_Jump))			cmd->ucmd.buttons |= BT_JUMP;
	if (buttonMap.ButtonDown(Button_Crouch))		cmd->ucmd.buttons |= BT_CROUCH;
	if (buttonMap.ButtonDown(Button_Zoom))			cmd->ucmd.buttons |= BT_ZOOM;
	if (buttonMap.ButtonDown(Button_Reload))		cmd->ucmd.buttons |= BT_RELOAD;

	if (buttonMap.ButtonDown(Button_User1))			cmd->ucmd.buttons |= BT_USER1;
	if (buttonMap.ButtonDown(Button_User2))			cmd->ucmd.buttons |= BT_USER2;
	if (buttonMap.ButtonDown(Button_User3))			cmd->ucmd.buttons |= BT_USER3;
	if (buttonMap.ButtonDown(Button_User4))			cmd->ucmd.buttons |= BT_USER4;

	if (buttonMap.ButtonDown(Button_Speed))			cmd->ucmd.buttons |= BT_SPEED;
	if (buttonMap.ButtonDown(Button_Strafe))		cmd->ucmd.buttons |= BT_STRAFE;
	if (buttonMap.ButtonDown(Button_MoveRight))		cmd->ucmd.buttons |= BT_MOVERIGHT;
	if (buttonMap.ButtonDown(Button_MoveLeft))		cmd->ucmd.buttons |= BT_MOVELEFT;
	if (buttonMap.ButtonDown(Button_LookDown))		cmd->ucmd.buttons |= BT_LOOKDOWN;
	if (buttonMap.ButtonDown(Button_LookUp))		cmd->ucmd.buttons |= BT_LOOKUP;
	if (buttonMap.ButtonDown(Button_Back))			cmd->ucmd.buttons |= BT_BACK;
	if (buttonMap.ButtonDown(Button_Forward))		cmd->ucmd.buttons |= BT_FORWARD;
	if (buttonMap.ButtonDown(Button_Right))			cmd->ucmd.buttons |= BT_RIGHT;
	if (buttonMap.ButtonDown(Button_Left))			cmd->ucmd.buttons |= BT_LEFT;
	if (buttonMap.ButtonDown(Button_MoveDown))		cmd->ucmd.buttons |= BT_MOVEDOWN;
	if (buttonMap.ButtonDown(Button_MoveUp))		cmd->ucmd.buttons |= BT_MOVEUP;
	if (buttonMap.ButtonDown(Button_ShowScores))	cmd->ucmd.buttons |= BT_SHOWSCORES;
	if (speed) cmd->ucmd.buttons |= BT_RUN;

	// Handle joysticks/game controllers.
	float joyaxes[NUM_JOYAXIS];

	I_GetAxes(joyaxes);

	// Remap some axes depending on button state.
	if (buttonMap.ButtonDown(Button_Strafe) || (buttonMap.ButtonDown(Button_Mlook) && lookstrafe))
	{
		joyaxes[JOYAXIS_Side] = joyaxes[JOYAXIS_Yaw];
		joyaxes[JOYAXIS_Yaw] = 0;
	}
	if (buttonMap.ButtonDown(Button_Mlook))
	{
		joyaxes[JOYAXIS_Pitch] = joyaxes[JOYAXIS_Forward];
		joyaxes[JOYAXIS_Forward] = 0;
	}

	if (joyaxes[JOYAXIS_Pitch] != 0)
	{
		G_AddViewPitch(joyint(joyaxes[JOYAXIS_Pitch] * 2048));
	}
	if (joyaxes[JOYAXIS_Yaw] != 0)
	{
		G_AddViewAngle(joyint(-1280 * joyaxes[JOYAXIS_Yaw]));
	}

	side -= joyint(sidemove[speed] * joyaxes[JOYAXIS_Side]);
	forward += joyint(joyaxes[JOYAXIS_Forward] * forwardmove[speed]);
	fly += joyint(joyaxes[JOYAXIS_Up] * 2048);

	// Handle mice.
	if (!buttonMap.ButtonDown(Button_Mlook) && !freelook)
	{
		forward += xs_CRoundToInt(mousey * m_forward);
	}

	cmd->ucmd.pitch = LocalViewPitch >> 16;

	if (SendLand)
	{
		SendLand = false;
		fly = -32768;
	}

	if (strafe || lookstrafe)
		side += xs_CRoundToInt(mousex * m_side);

	mousex = mousey = 0;

	// Build command.
	if (forward > MAXPLMOVE)
		forward = MAXPLMOVE;
	else if (forward < -MAXPLMOVE)
		forward = -MAXPLMOVE;
	if (side > MAXPLMOVE)
		side = MAXPLMOVE;
	else if (side < -MAXPLMOVE)
		side = -MAXPLMOVE;

	cmd->ucmd.forwardmove += forward;
	cmd->ucmd.sidemove += side;
	cmd->ucmd.yaw = LocalViewAngle >> 16;
	cmd->ucmd.upmove = fly;
	LocalViewAngle = 0;
	LocalViewPitch = 0;

	// special buttons
	if (sendturn180)
	{
		sendturn180 = false;
		cmd->ucmd.buttons |= BT_TURN180;
	}
	if (sendpause)
	{
		sendpause = false;
		Net_WriteByte (DEM_PAUSE);
	}
	if (sendsave)
	{
		sendsave = false;
		Net_WriteByte (DEM_SAVEGAME);
		Net_WriteString (savegamefile);
		Net_WriteString (savedescription);
		savegamefile = "";
	}
	if (SendItemUse == (const AActor *)1)
	{
		Net_WriteByte (DEM_INVUSEALL);
		SendItemUse = NULL;
	}
	else if (SendItemUse != NULL)
	{
		Net_WriteByte (DEM_INVUSE);
		Net_WriteLong (SendItemUse->InventoryID);
		SendItemUse = NULL;
	}
	if (SendItemDrop != NULL)
	{
		Net_WriteByte (DEM_INVDROP);
		Net_WriteLong (SendItemDrop->InventoryID);
		Net_WriteLong(SendItemDropAmount);
		SendItemDrop = NULL;
	}

	cmd->ucmd.forwardmove <<= 8;
	cmd->ucmd.sidemove <<= 8;
}

static int LookAdjust(int look)
{
	look <<= 16;
	if (players[consoleplayer].playerstate != PST_DEAD &&		// No adjustment while dead.
		players[consoleplayer].ReadyWeapon != NULL)			// No adjustment if no weapon.
	{
		auto FOVScale = players[consoleplayer].ReadyWeapon->FloatVar(NAME_FOVScale);
		auto LookScale = players[consoleplayer].ReadyWeapon->FloatVar(NAME_LookScale);
		if (FOVScale > 0)		// No adjustment if it is non-positive.
		{
			look = int(look * FOVScale);
		}
		if (LookScale > 0)		// No adjustment if it is non-positive.
		{
			look = int(look * LookScale);
		}
	}
	return look;
}

void G_AddViewPitch (int look, bool mouse)
{
	if (gamestate == GS_TITLELEVEL)
	{
		return;
	}
	look = LookAdjust(look);
	if (!primaryLevel->IsFreelookAllowed())
	{
		LocalViewPitch = 0;
	}
	else if (look > 0)
	{
		// Avoid overflowing
		if (LocalViewPitch > INT_MAX - look)
		{
			LocalViewPitch = 0x78000000;
		}
		else
		{
			LocalViewPitch = MIN(LocalViewPitch + look, 0x78000000);
		}
	}
	else if (look < 0)
	{
		// Avoid overflowing
		if (LocalViewPitch < INT_MIN - look)
		{
			LocalViewPitch = -0x78000000;
		}
		else
		{
			LocalViewPitch = MAX(LocalViewPitch + look, -0x78000000);
		}
	}
	if (look != 0)
	{
		LocalKeyboardTurner = !mouse;
	}
}

void G_AddViewAngle (int yaw, bool mouse)
{
	if (gamestate == GS_TITLELEVEL)
	{
		return;

	}
	yaw = LookAdjust(yaw);
	LocalViewAngle -= yaw;
	if (yaw != 0)
	{
		LocalKeyboardTurner = !mouse;
	}
}

CVAR (Bool, bot_allowspy, false, 0)


enum {
	SPY_CANCEL = 0,
	SPY_NEXT,
	SPY_PREV,
};

// [RH] Spy mode has been separated into two console commands.
//		One goes forward; the other goes backward.
static void ChangeSpy (int changespy)
{
	// If you're not in a level, then you can't spy.
	if (gamestate != GS_LEVEL)
	{
		return;
	}

	// If not viewing through a player, return your eyes to your own head.
	if (players[consoleplayer].camera->player == NULL)
	{
		// When watching demos, you will just have to wait until your player
		// has done this for you, since it could desync otherwise.
		if (!demoplayback)
		{
			Net_WriteByte(DEM_REVERTCAMERA);
		}
		return;
	}

	// We may not be allowed to spy on anyone.
	if (dmflags2 & DF2_DISALLOW_SPYING)
		return;

	// Otherwise, cycle to the next player.
	bool checkTeam = !demoplayback && deathmatch;
	int pnum = consoleplayer;
	if (changespy != SPY_CANCEL) 
	{
		player_t *player = players[consoleplayer].camera->player;
		// only use the camera as starting index if it's a valid player.
		if (player != NULL) pnum = int(players[consoleplayer].camera->player - players);

		int step = (changespy == SPY_NEXT) ? 1 : -1;

		do
		{
			pnum += step;
			pnum &= MAXPLAYERS-1;
			if (playeringame[pnum] &&
				(!checkTeam || players[pnum].mo->IsTeammate (players[consoleplayer].mo) ||
				(bot_allowspy && players[pnum].Bot != NULL)))
			{
				break;
			}
		} while (pnum != consoleplayer);
	}

	players[consoleplayer].camera = players[pnum].mo;
	S_UpdateSounds(players[consoleplayer].camera);
	StatusBar->AttachToPlayer (&players[pnum]);
	if (demoplayback || multiplayer)
	{
		StatusBar->ShowPlayerName ();
	}
}

CCMD (spynext)
{
	// allow spy mode changes even during the demo
	ChangeSpy (SPY_NEXT);
}

CCMD (spyprev)
{
	// allow spy mode changes even during the demo
	ChangeSpy (SPY_PREV);
}

CCMD (spycancel)
{
	// allow spy mode changes even during the demo
	ChangeSpy (SPY_CANCEL);
}

//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//
bool G_Responder (event_t *ev)
{
	// check events
	if (ev->type != EV_Mouse && primaryLevel->localEventManager->Responder(ev)) // [ZZ] ZScript ate the event // update 07.03.17: mouse events are handled directly
		return true;
	
	// any other key pops up menu if in demos
	// [RH] But only if the key isn't bound to a "special" command
	if (gameaction == ga_nothing && 
		(demoplayback || gamestate == GS_DEMOSCREEN || gamestate == GS_TITLELEVEL))
	{
		const char *cmd = Bindings.GetBind (ev->data1);

		if (ev->type == EV_KeyDown)
		{

			if (!cmd || (
				strnicmp (cmd, "menu_", 5) &&
				stricmp (cmd, "toggleconsole") &&
				stricmp (cmd, "sizeup") &&
				stricmp (cmd, "sizedown") &&
				stricmp (cmd, "togglemap") &&
				stricmp (cmd, "spynext") &&
				stricmp (cmd, "spyprev") &&
				stricmp (cmd, "chase") &&
				stricmp (cmd, "+showscores") &&
				stricmp (cmd, "bumpgamma") &&
				stricmp (cmd, "screenshot")))
			{
				M_StartControlPanel(true);
				M_SetMenu(NAME_Mainmenu, -1);
				return true;
			}
			else
			{
				return C_DoKey (ev, &Bindings, &DoubleBindings);
			}
		}
		if (cmd && cmd[0] == '+')
			return C_DoKey (ev, &Bindings, &DoubleBindings);

		return false;
	}

	if (CT_Responder (ev))
		return true;			// chat ate the event

	if (gamestate == GS_LEVEL)
	{
		if (ST_Responder (ev))
			return true;		// status window ate it
		if (!viewactive && primaryLevel->automap->Responder (ev, false))
			return true;		// automap ate it
	}
	else if (gamestate == GS_FINALE)
	{
		if (F_Responder (ev))
			return true;		// finale ate the event
	}

	switch (ev->type)
	{
	case EV_KeyDown:
		if (C_DoKey (ev, &Bindings, &DoubleBindings))
			return true;
		break;

	case EV_KeyUp:
		C_DoKey (ev, &Bindings, &DoubleBindings);
		break;

	// [RH] mouse buttons are sent as key up/down events
	case EV_Mouse: 
		mousex = ev->x;
		mousey = ev->y;
		break;
	}

	// [RH] If the view is active, give the automap a chance at
	// the events *last* so that any bound keys get precedence.

	if (gamestate == GS_LEVEL && viewactive && primaryLevel->automap)
		return primaryLevel->automap->Responder (ev, true);

	return (ev->type == EV_KeyDown ||
			ev->type == EV_Mouse);
}


static void G_FullConsole()
{
	int oldgs = gamestate;

	if (hud_toggled)
		D_ToggleHud();
	if (demoplayback)
		G_CheckDemoStatus();
	D_QuitNetGame();
	advancedemo = false;
	C_FullConsole();

	if (oldgs != GS_STARTUP)
	{
		primaryLevel->Music = "";
		S_Start();
		S_StopMusic(true);
		P_FreeLevelData();
	}

}

//==========================================================================
//
// FRandom :: StaticSumSeeds
//
// This function produces a uint32_t that can be used to check the consistancy
// of network games between different machines. Only a select few RNGs are
// used for the sum, because not all RNGs are important to network sync.
//
//==========================================================================

extern FRandom pr_spawnmobj;
extern FRandom pr_acs;
extern FRandom pr_chase;
extern FRandom pr_damagemobj;

static uint32_t StaticSumSeeds()
{
	return
		pr_spawnmobj.Seed() +
		pr_acs.Seed() +
		pr_chase.Seed() +
		pr_damagemobj.Seed();
}

//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker ()
{
	int i;
	gamestate_t	oldgamestate;

	// do player reborns if needed
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			if (players[i].playerstate == PST_GONE)
			{
				G_DoPlayerPop(i);
			}
			if (players[i].playerstate == PST_REBORN || players[i].playerstate == PST_ENTER)
			{
				primaryLevel->DoReborn(i, false);
			}
		}
	}

	if (ToggleFullscreen)
	{
		ToggleFullscreen = false;
		AddCommandString ("toggle vid_fullscreen");
	}

	// do things to change the game state
	oldgamestate = gamestate;
	while (gameaction != ga_nothing)
	{
		if (gameaction == ga_newgame2)
		{
			gameaction = ga_newgame;
			break;
		}
		switch (gameaction)
		{
		case ga_recordgame:
			G_CheckDemoStatus();
			G_RecordDemo(newdemoname);
			G_BeginRecording(newdemomap);
			[[fallthrough]];
		case ga_newgame2:	// Silence GCC (see above)
		case ga_newgame:
			G_DoNewGame ();
			break;
		case ga_loadgame:
		case ga_loadgamehidecon:
		case ga_autoloadgame:
			G_DoLoadGame ();
			break;
		case ga_savegame:
			G_DoSaveGame (true, false, savegamefile, savedescription);
			gameaction = ga_nothing;
			savegamefile = "";
			savedescription = "";
			break;
		case ga_autosave:
			G_DoAutoSave ();
			gameaction = ga_nothing;
			break;
		case ga_loadgameplaydemo:
			G_DoLoadGame ();
			// fallthrough
		case  ga_playdemo:
			G_DoPlayDemo ();
			break;
		case ga_completed:
			G_DoCompleted ();
			break;
		case ga_slideshow:
			if (gamestate == GS_LEVEL) F_StartIntermission(SelectedSlideshow, FSTATE_InLevel);
			break;
		case ga_worlddone:
			G_DoWorldDone ();
			break;
		case ga_screenshot:
			M_ScreenShot (shotfile);
			shotfile = "";
			gameaction = ga_nothing;
			break;
		case ga_fullconsole:
			G_FullConsole ();
			gameaction = ga_nothing;
			break;
		case ga_togglemap:
			AM_ToggleMap ();
			gameaction = ga_nothing;
			break;
		case ga_resumeconversation:
			P_ResumeConversation ();
			gameaction = ga_nothing;
			break;
		default:
		case ga_nothing:
			break;
		}
		C_AdjustBottom ();
	}

	if (oldgamestate != gamestate)
	{
		if (oldgamestate == GS_FINALE)
		{
			F_EndFinale ();
		}
	}

	// get commands, check consistancy, and build new consistancy check
	int buf = (gametic/ticdup)%BACKUPTICS;

	// [RH] Include some random seeds and player stuff in the consistancy
	// check, not just the player's x position like BOOM.
	uint32_t rngsum = StaticSumSeeds ();

	//Added by MC: For some of that bot stuff. The main bot function.
	primaryLevel->BotInfo.Main (primaryLevel);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			ticcmd_t *cmd = &players[i].cmd;
			ticcmd_t *newcmd = &netcmds[i][buf];

			if ((gametic % ticdup) == 0)
			{
				RunNetSpecs (i, buf);
			}
			if (demorecording)
			{
				G_WriteDemoTiccmd (newcmd, i, buf);
			}
			players[i].oldbuttons = cmd->ucmd.buttons;
			// If the user alt-tabbed away, paused gets set to -1. In this case,
			// we do not want to read more demo commands until paused is no
			// longer negative.
			if (demoplayback)
			{
				G_ReadDemoTiccmd (cmd, i);
			}
			else
			{
				memcpy(cmd, newcmd, sizeof(ticcmd_t));
			}

			// check for turbo cheats
			if (multiplayer && turbo > 100.f && cmd->ucmd.forwardmove > TURBOTHRESHOLD &&
				!(gametic&31) && ((gametic>>5)&(MAXPLAYERS-1)) == i )
			{
				Printf ("%s is turbo!\n", players[i].userinfo.GetName());
			}

			if (netgame && players[i].Bot == NULL && !demoplayback && (gametic%ticdup) == 0)
			{
				//players[i].inconsistant = 0;
				if (gametic > BACKUPTICS*ticdup && consistancy[i][buf] != cmd->consistancy)
				{
					players[i].inconsistant = gametic - BACKUPTICS*ticdup;
				}
				if (players[i].mo)
				{
					uint32_t sum = rngsum + int((players[i].mo->X() + players[i].mo->Y() + players[i].mo->Z())*257) + players[i].mo->Angles.Yaw.BAMs() + players[i].mo->Angles.Pitch.BAMs();
					sum ^= players[i].health;
					consistancy[i][buf] = sum;
				}
				else
				{
					consistancy[i][buf] = rngsum;
				}
			}
		}
	}

	// [ZZ] also tick the UI part of the events
	primaryLevel->localEventManager->UiTick();
	C_RunDelayedCommands();

	// do main actions
	switch (gamestate)
	{
	case GS_LEVEL:
		P_Ticker ();
		primaryLevel->automap->Ticker ();
		break;

	case GS_TITLELEVEL:
		P_Ticker ();
		break;

	case GS_INTERMISSION:
		WI_Ticker ();
		break;

	case GS_FINALE:
		F_Ticker ();
		break;

	case GS_DEMOSCREEN:
		D_PageTicker ();
		break;

	case GS_STARTUP:
		if (gameaction == ga_nothing)
		{
			gamestate = GS_FULLCONSOLE;
			gameaction = ga_fullconsole;
		}
		break;

	default:
		break;
	}

	// [MK] Additional ticker for UI events right after all others
	primaryLevel->localEventManager->PostUiTick();
}


//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Mobj
//

//
// G_PlayerFinishLevel
// Called when a player completes a level.
//
// flags is checked for RESETINVENTORY and RESETHEALTH only.

void G_PlayerFinishLevel (int player, EFinishLevelType mode, int flags)
{
	IFVM(PlayerPawn, PlayerFinishLevel)
	{
		VMValue params[] = { players[player].mo, mode, flags };
		VMCall(func, params, 3, nullptr, 0);
	}
}


//
// G_PlayerReborn
// Called after a player dies
// almost everything is cleared and initialized
//
void FLevelLocals::PlayerReborn (int player)
{
	player_t*	p;
	int 		frags[MAXPLAYERS];
	int			fragcount;	// [RH] Cumulative frags
	int 		killcount;
	int 		itemcount;
	int 		secretcount;
	int			chasecam;
	uint8_t		currclass;
	userinfo_t  userinfo;	// [RH] Save userinfo
	AActor *actor;
	PClassActor *cls;
	FString		log;
	DBot		*Bot;		//Added by MC:

	p = &players[player];

	memcpy (frags, p->frags, sizeof(frags));
	fragcount = p->fragcount;
	killcount = p->killcount;
	itemcount = p->itemcount;
	secretcount = p->secretcount;
	currclass = p->CurrentPlayerClass;
	userinfo.TransferFrom(p->userinfo);
	actor = p->mo;
	cls = p->cls;
	log = p->LogText;
	chasecam = p->cheats & CF_CHASECAM;
	Bot = p->Bot;			//Added by MC:
	const bool settings_controller = p->settings_controller;

	// Reset player structure to its defaults
	p->~player_t();
	::new(p) player_t;

	memcpy (p->frags, frags, sizeof(p->frags));
	p->health = actor->health;
	p->fragcount = fragcount;
	p->killcount = killcount;
	p->itemcount = itemcount;
	p->secretcount = secretcount;
	p->CurrentPlayerClass = currclass;
	p->userinfo.TransferFrom(userinfo);
	p->mo = actor;
	p->cls = cls;
	p->LogText = log;
	p->cheats |= chasecam;
	p->Bot = Bot;			//Added by MC:
	p->settings_controller = settings_controller;

	p->oldbuttons = ~0, p->attackdown = true; p->usedown = true;	// don't do anything immediately
	p->original_oldbuttons = ~0;
	p->playerstate = PST_LIVE;

	if (gamestate != GS_TITLELEVEL)
	{
		// [GRB] Give inventory specified in DECORATE

		IFVIRTUALPTRNAME(actor, NAME_PlayerPawn, GiveDefaultInventory)
		{
			VMValue params[1] = { actor };
			VMCall(func, params, 1, nullptr, 0);
		}
		p->ReadyWeapon = p->PendingWeapon;
	}

	//Added by MC: Init bot structure.
	if (p->Bot != NULL)
	{
		botskill_t skill = p->Bot->skill;
		p->Bot->Clear ();
		p->Bot->player = p;
		p->Bot->skill = skill;
	}
}

//
// G_CheckSpot	
// Returns false if the player cannot be respawned
// at the given mapthing spot  
// because something is occupying it 
//

bool FLevelLocals::CheckSpot (int playernum, FPlayerStart *mthing)
{
	DVector3 spot;
	double oldz;
	int i;

	if (mthing->type == 0) return false;

	spot = mthing->pos;

	if (!(flags & LEVEL_USEPLAYERSTARTZ))
	{
		spot.Z = 0;
	}
	spot.Z += PointInSector (spot)->floorplane.ZatPoint (spot);

	if (!players[playernum].mo)
	{ // first spawn of level, before corpses
		for (i = 0; i < playernum; i++)
			if (players[i].mo && players[i].mo->X() == spot.X && players[i].mo->Y() == spot.Y)
				return false;
		return true;
	}

	oldz = players[playernum].mo->Z();	// [RH] Need to save corpse's z-height
	players[playernum].mo->SetZ(spot.Z);		// [RH] Checks are now full 3-D

	// killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
	// corpse to detect collisions with other players in DM starts
	//
	// Old code:
	// if (!P_CheckPosition (players[playernum].mo, x, y))
	//    return false;

	players[playernum].mo->flags |=  MF_SOLID;
	i = P_CheckPosition(players[playernum].mo, spot);
	players[playernum].mo->flags &= ~MF_SOLID;
	players[playernum].mo->SetZ(oldz);	// [RH] Restore corpse's height
	if (!i)
		return false;

	return true;
}


//
// G_DeathMatchSpawnPlayer 
// Spawns a player at one of the random death match spots 
// called at level load and each death 
//

// [RH] Returns the distance of the closest player to the given mapthing
double FLevelLocals::PlayersRangeFromSpot (FPlayerStart *spot)
{
	double closest = INT_MAX;
	double distance;
	int i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || !players[i].mo || players[i].health <= 0)
			continue;

		distance = players[i].mo->Distance2D(spot->pos.X, spot->pos.Y);

		if (distance < closest)
			closest = distance;
	}

	return closest;
}

// [RH] Select the deathmatch spawn spot farthest from everyone.
FPlayerStart *FLevelLocals::SelectFarthestDeathmatchSpot (size_t selections)
{
	double bestdistance = 0;
	FPlayerStart *bestspot = NULL;
	unsigned int i;

	for (i = 0; i < selections; i++)
	{
		double distance = PlayersRangeFromSpot (&deathmatchstarts[i]);

		if (distance > bestdistance)
		{
			bestdistance = distance;
			bestspot = &deathmatchstarts[i];
		}
	}

	return bestspot;
}

// [RH] Select a deathmatch spawn spot at random (original mechanism)
FPlayerStart *FLevelLocals::SelectRandomDeathmatchSpot (int playernum, unsigned int selections)
{
	unsigned int i, j;

	for (j = 0; j < 20; j++)
	{
		i = pr_dmspawn() % selections;
		if (CheckSpot (playernum, &deathmatchstarts[i]) )
		{
			return &deathmatchstarts[i];
		}
	}

	// [RH] return a spot anyway, since we allow telefragging when a player spawns
	return &deathmatchstarts[i];
}

DEFINE_ACTION_FUNCTION(FLevelLocals, PickDeathmatchStart)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	unsigned int selections = self->deathmatchstarts.Size();
	DVector3 pos;
	int angle;
	if (selections == 0)
	{
		angle = INT_MAX;
		pos = DVector3(0, 0, 0);
	}
	else
	{
		unsigned int i = pr_dmspawn() % selections;
		angle = self->deathmatchstarts[i].angle;
		pos = self->deathmatchstarts[i].pos;
	}

	if (numret > 1)
	{
		ret[1].SetInt(angle);
		numret = 2;
	}
	if (numret > 0)
	{
		ret[0].SetVector(pos);
	}
	return numret;
}

void FLevelLocals::DeathMatchSpawnPlayer (int playernum)
{
	unsigned int selections;
	FPlayerStart *spot;

	selections = deathmatchstarts.Size ();
	// [RH] We can get by with just 1 deathmatch start
	if (selections < 1)
		I_Error ("No deathmatch starts");

	// At level start, none of the players have mobjs attached to them,
	// so we always use the random deathmatch spawn. During the game,
	// though, we use whatever dmflags specifies.
	if ((dmflags & DF_SPAWN_FARTHEST) && players[playernum].mo)
		spot = SelectFarthestDeathmatchSpot (selections);
	else
		spot = SelectRandomDeathmatchSpot (playernum, selections);

	if (spot == NULL)
	{ // No good spot, so the player will probably get stuck.
	  // We were probably using select farthest above, and all
	  // the spots were taken.
		spot = PickPlayerStart(playernum, PPS_FORCERANDOM);
		if (!CheckSpot(playernum, spot))
		{ // This map doesn't have enough coop spots for this player
		  // to use one.
			spot = SelectRandomDeathmatchSpot(playernum, selections);
			if (spot == NULL)
			{ // We have a player 1 start, right?
				spot = &playerstarts[0];
				if (spot->type == 0)
				{ // Fine, whatever.
					spot = &deathmatchstarts[0];
				}
			}
		}
	}
	AActor *mo = SpawnPlayer(spot, playernum);
	if (mo != NULL) P_PlayerStartStomp(mo);
}


//
// G_PickPlayerStart
//
FPlayerStart *FLevelLocals::PickPlayerStart(int playernum, int flags)
{
	if (AllPlayerStarts.Size() == 0) // No starts to pick
	{
		return NULL;
	}

	if ((flags2 & LEVEL2_RANDOMPLAYERSTARTS) || (flags & PPS_FORCERANDOM) ||
		playerstarts[playernum].type == 0)
	{
		if (!(flags & PPS_NOBLOCKINGCHECK))
		{
			TArray<FPlayerStart *> good_starts;
			unsigned int i;

			// Find all unblocked player starts.
			for (i = 0; i < AllPlayerStarts.Size(); ++i)
			{
				if (CheckSpot(playernum, &AllPlayerStarts[i]))
				{
					good_starts.Push(&AllPlayerStarts[i]);
				}
			}
			if (good_starts.Size() > 0)
			{ // Pick an open spot at random.
				return good_starts[pr_pspawn(good_starts.Size())];
			}
		}
		// Pick a spot at random, whether it's open or not.
		return &AllPlayerStarts[pr_pspawn(AllPlayerStarts.Size())];
	}
	return &playerstarts[playernum];
}

DEFINE_ACTION_FUNCTION(FLevelLocals, PickPlayerStart)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(playernum);
	PARAM_INT(flags);
	auto ps = self->PickPlayerStart(playernum, flags);
	if (numret > 1)
	{
		ret[1].SetInt(ps? ps->angle : 0);
		numret = 2;
	}
	if (numret > 0)
	{
		ret[0].SetVector(ps ? ps->pos : DVector3(0, 0, 0));
	}
	return numret;
}

//
// G_QueueBody
//
void FLevelLocals::QueueBody (AActor *body)
{
	// flush an old corpse if needed
	int modslot = bodyqueslot % BODYQUESIZE;
	bodyqueslot = modslot + 1;

	if (bodyqueslot >= BODYQUESIZE && bodyque[modslot] != NULL)
	{
		bodyque[modslot]->Destroy ();
	}
	bodyque[modslot] = body;

	// Copy the player's translation, so that if they change their color later, only
	// their current body will change and not all their old corpses.
	if (GetTranslationType(body->Translation) == TRANSLATION_Players ||
		GetTranslationType(body->Translation) == TRANSLATION_PlayersExtra)
	{
		// This needs to be able to handle multiple levels, in case a level with dead players is used as a secondary one later.
		GPalette.CopyTranslation(TRANSLATION(TRANSLATION_PlayerCorpses, modslot), body->Translation);
		body->Translation = TRANSLATION(TRANSLATION_PlayerCorpses, modslot);

	}

	const int skinidx = body->player->userinfo.GetSkin();

	if (0 != skinidx && !(body->flags4 & MF4_NOSKIN))
	{
		// Apply skin's scale to actor's scale, it will be lost otherwise
		const AActor *const defaultActor = body->GetDefault();
		const FPlayerSkin &skin = Skins[skinidx];

		body->Scale.X *= skin.Scale.X / defaultActor->Scale.X;
		body->Scale.Y *= skin.Scale.Y / defaultActor->Scale.Y;
	}

}

//
// G_DoReborn
//
EXTERN_CVAR(Bool, sv_singleplayerrespawn)
void FLevelLocals::DoReborn (int playernum, bool freshbot)
{
	if (!multiplayer && !(flags2 & LEVEL2_ALLOWRESPAWN) && !sv_singleplayerrespawn &&
		!G_SkillProperty(SKILLP_PlayerRespawn))
	{
		if (BackupSaveName.Len() > 0 && FileExists (BackupSaveName.GetChars()))
		{ // Load game from the last point it was saved
			savename = BackupSaveName;
			gameaction = ga_autoloadgame;
		}
		else
		{ // Reload the level from scratch
			bool indemo = demoplayback;
			BackupSaveName = "";
			G_InitNew (MapName, false);
			demoplayback = indemo;
		}
	}
	else
	{
		bool isUnfriendly;

		PlayerSpawnPickClass(playernum);

		// this condition should never be false
		assert(players[playernum].cls != NULL);

		if (players[playernum].cls != NULL)
		{
			isUnfriendly = !(GetDefaultByType(players[playernum].cls)->flags & MF_FRIENDLY);
			DPrintf(DMSG_NOTIFY, "Player class IS defined: unfriendly is %i\n", isUnfriendly);
		}
		else
		{
			// we shouldn't be here, but if we are, get the player's current status
			isUnfriendly = players[playernum].mo && !(players[playernum].mo->flags & MF_FRIENDLY);
			DPrintf(DMSG_NOTIFY, "Player class NOT defined: unfriendly is %i\n", isUnfriendly);
		}

		// respawn at the start
		// first disassociate the corpse
		if (players[playernum].mo)
		{
			QueueBody (players[playernum].mo);
			players[playernum].mo->player = NULL;
		}

		// spawn at random spot if in deathmatch
		if ((deathmatch || isUnfriendly) && (deathmatchstarts.Size () > 0))
		{
			DeathMatchSpawnPlayer (playernum);
			return;
		}

		if (!(flags2 & LEVEL2_RANDOMPLAYERSTARTS) &&
			playerstarts[playernum].type != 0 &&
			CheckSpot (playernum, &playerstarts[playernum]))
		{
			AActor *mo = SpawnPlayer(&playerstarts[playernum], playernum);
			if (mo != NULL) P_PlayerStartStomp(mo, true);
		}
		else
		{ // try to spawn at any random player's spot
			FPlayerStart *start = PickPlayerStart(playernum, PPS_FORCERANDOM);
			AActor *mo = SpawnPlayer(start, playernum);
			if (mo != NULL) P_PlayerStartStomp(mo, true);
		}
	}
}

//
// G_DoReborn
//
void G_DoPlayerPop(int playernum)
{
	playeringame[playernum] = false;

	FString message = GStrings(deathmatch? "TXT_LEFTWITHFRAGS" : "TXT_LEFTTHEGAME");
	message.Substitute("%s", players[playernum].userinfo.GetName());
	message.Substitute("%d", FStringf("%d", players[playernum].fragcount));
	Printf("%s\n", message.GetChars());

	// [RH] Revert each player to their own view if spying through the player who left
	for (int ii = 0; ii < MAXPLAYERS; ++ii)
	{
		if (playeringame[ii] && players[ii].camera == players[playernum].mo)
		{
			players[ii].camera = players[ii].mo;
			if (ii == consoleplayer && StatusBar != NULL)
			{
				StatusBar->AttachToPlayer(&players[ii]);
			}
		}
	}

	// [RH] Make the player disappear
	auto mo = players[playernum].mo;
	mo->Level->Behaviors.StopMyScripts(mo);
	// [ZZ] fire player disconnect hook
	mo->Level->localEventManager->PlayerDisconnected(playernum);
	// [RH] Let the scripts know the player left
	mo->Level->Behaviors.StartTypedScripts(SCRIPT_Disconnect, mo, true, playernum, true);
	if (mo != NULL)
	{
		P_DisconnectEffect(mo);
		mo->player = NULL;
		mo->Destroy();
		if (!(players[playernum].mo->ObjectFlags & OF_EuthanizeMe))
		{ // We just destroyed a morphed player, so now the original player
			// has taken their place. Destroy that one too.
			players[playernum].mo->Destroy();
		}
		players[playernum].mo = nullptr;
		players[playernum].camera = nullptr;
	}

	players[playernum].DestroyPSprites();
}

void G_ScreenShot (const char *filename)
{
	if (gameaction == ga_nothing)
	{
		shotfile = filename;
		gameaction = ga_screenshot;
	}
}



//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//
void G_LoadGame (const char* name, bool hidecon)
{
	if (name != NULL)
	{
		savename = name;
		gameaction = !hidecon ? ga_loadgame : ga_loadgamehidecon;
	}
}

static bool CheckSingleWad (const char *name, bool &printRequires, bool printwarn)
{
	if (name == NULL)
	{
		return true;
	}
	if (fileSystem.CheckIfResourceFileLoaded (name) < 0)
	{
		if (printwarn)
		{
			if (!printRequires)
			{
				Printf ("%s:\n%s", GStrings("TXT_SAVEGAMENEEDS"), name);
			}
			else
			{
				Printf (", %s", name);
			}
		}
		printRequires = true;
		return false;
	}
	return true;
}

// Return false if not all the needed wads have been loaded.
bool G_CheckSaveGameWads (FSerializer &arc, bool printwarn)
{
	bool printRequires = false;
	FString text;

	arc("Game WAD", text);
	CheckSingleWad (text, printRequires, printwarn);
	arc("Map WAD", text);
	CheckSingleWad (text, printRequires, printwarn);

	if (printRequires)
	{
		if (printwarn)
		{
			Printf ("\n");
		}
		return false;
	}

	return true;
}

static void LoadGameError(const char *label, const char *append = "")
{
	FString message = GStrings(label);
	message.Substitute("%s", savename);
	Printf ("%s %s\n", message.GetChars(), append);
}

void C_SerializeCVars(FSerializer& arc, const char* label, uint32_t filter)
{
	FBaseCVar* cvar;
	FString dump;

	if (arc.BeginObject(label))
	{
		if (arc.isWriting())
		{
			for (cvar = CVars; cvar != NULL; cvar = cvar->m_Next)
			{
				if ((cvar->Flags & filter) && !(cvar->Flags & (CVAR_NOSAVE | CVAR_IGNORE | CVAR_CONFIG_ONLY)))
				{
					UCVarValue val = cvar->GetGenericRep(CVAR_String);
					char* c = const_cast<char*>(val.String);
					arc(cvar->GetName(), c);
				}
			}
		}
		else
		{
			for (cvar = CVars; cvar != NULL; cvar = cvar->m_Next)
			{
				if ((cvar->Flags & filter) && !(cvar->Flags & (CVAR_NOSAVE | CVAR_IGNORE | CVAR_CONFIG_ONLY)))
				{
					UCVarValue val;
					char* c = nullptr;
					arc(cvar->GetName(), c);
					if (c != nullptr)
					{
						val.String = c;
						cvar->SetGenericRep(val, CVAR_String);
						delete[] c;
					}
				}
			}
		}
		arc.EndObject();
	}
}


void G_DoLoadGame ()
{
	bool hidecon;

	if (gameaction != ga_autoloadgame)
	{
		demoplayback = false;
	}
	hidecon = gameaction == ga_loadgamehidecon;
	gameaction = ga_nothing;

	std::unique_ptr<FResourceFile> resfile(FResourceFile::OpenResourceFile(savename.GetChars(), true, true));
	if (resfile == nullptr)
	{
		LoadGameError("TXT_COULDNOTREAD");
		return;
	}
	FResourceLump *info = resfile->FindLump("info.json");
	if (info == nullptr)
	{
		LoadGameError("TXT_NOINFOJSON");
		return;
	}

	SaveVersion = 0;

	void *data = info->Lock();
	FSerializer arc;
	if (!arc.OpenReader((const char *)data, info->LumpSize))
	{
		LoadGameError("TXT_FAILEDTOREADSG");
		return;
	}

	// Check whether this savegame actually has been created by a compatible engine.
	// Since there are ZDoom derivates using the exact same savegame format but
	// with mutual incompatibilities this check simplifies things significantly.
	FString savever, engine, map;
	arc("Save Version", SaveVersion);
	arc("Engine", engine);
	arc("Current Map", map);

	if (engine.CompareNoCase(GAMESIG) != 0)
	{
		// Make a special case for the message printed for old savegames that don't
		// have this information.
		if (engine.IsEmpty())
		{
			LoadGameError("TXT_INCOMPATIBLESG");
		}
		else
		{
			LoadGameError("TXT_IOTHERENGINESG", engine.GetChars());
		}
		return;
	}

	if (SaveVersion < MINSAVEVER || SaveVersion > SAVEVER)
	{
		FString message;
		if (SaveVersion < MINSAVEVER)
		{
			message = GStrings("TXT_TOOOLDSG");
			message.Substitute("%e", FStringf("%d", MINSAVEVER));
		}
		else
		{
			message = GStrings("TXT_TOONEWSG");
			message.Substitute("%e", FStringf("%d", SAVEVER));
		}
		message.Substitute("%d", FStringf("%d", SaveVersion));
		LoadGameError(message);
		return;
	}

	if (!G_CheckSaveGameWads(arc, true))
	{
		return;
	}

	if (map.IsEmpty())
	{
		LoadGameError("TXT_NOMAPSG");
		return;
	}

	// Now that it looks like we can load this save, hide the fullscreen console if it was up
	// when the game was selected from the menu.
	if (hidecon && gamestate == GS_FULLCONSOLE)
	{
		gamestate = GS_HIDECONSOLE;
	}
	// we are done with info.json.
	arc.Close();

	info = resfile->FindLump("globals.json");
	if (info == nullptr)
	{
		LoadGameError("TXT_NOGLOBALSJSON");
		return;
	}

	data = info->Lock();
	if (!arc.OpenReader((const char *)data, info->LumpSize))
	{
		LoadGameError("TXT_SGINFOERR");
		return;
	}


	// Read intermission data for hubs
	G_SerializeHub(arc);

	primaryLevel->BotInfo.RemoveAllBots(primaryLevel, true);

	savegamerestore = true;		// Use the player actors in the savegame

	FString cvar;
	arc("importantcvars", cvar);
	if (!cvar.IsEmpty())
	{
		uint8_t *vars_p = (uint8_t *)cvar.GetChars();
		C_ReadCVars(&vars_p);
	}
	else
	{
		C_SerializeCVars(arc, "servercvars", CVAR_SERVERINFO);
	}

	uint32_t time[2] = { 1,0 };

	arc("ticrate", time[0])
		("leveltime", time[1])
		("globalfreeze", globalfreeze);
	// dearchive all the modifications
	level.time = Scale(time[1], TICRATE, time[0]);

	G_ReadSnapshots(resfile.get());
	resfile.reset(nullptr);	// we no longer need the resource file below this point
	G_ReadVisited(arc);

	// load a base level
	bool demoplaybacksave = demoplayback;
	G_InitNew(map, false);
	demoplayback = demoplaybacksave;
	savegamerestore = false;

	STAT_Serialize(arc);
	FRandom::StaticReadRNGState(arc);
	P_ReadACSDefereds(arc);
	P_ReadACSVars(arc);

	NextSkill = -1;
	arc("nextskill", NextSkill);

	if (level.info != nullptr)
		level.info->Snapshot.Clean();

	BackupSaveName = savename;

	// At this point, the GC threshold is likely a lot higher than the
	// amount of memory in use, so bring it down now by starting a
	// collection.
	GC::StartCollection();
}


//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//
void G_SaveGame (const char *filename, const char *description)
{
	if (sendsave || gameaction == ga_savegame)
	{
		Printf ("%s\n", GStrings("TXT_SAVEPENDING"));
	}
    else if (!usergame)
	{
		Printf ("%s\n", GStrings("TXT_NOTSAVEABLE"));
    }
    else if (gamestate != GS_LEVEL)
	{
		Printf ("%s\n", GStrings("TXT_NOTINLEVEL"));
    }
    else if (players[consoleplayer].health <= 0 && !multiplayer)
    {
		Printf ("%s\n", GStrings("TXT_SPPLAYERDEAD"));
    }
	else
	{
		savegamefile = filename;
		savedescription = description;
		sendsave = true;
	}
}

FString G_BuildSaveName (const char *prefix, int slot)
{
	FString name;
	FString leader;
	const char *slash = "";

	leader = Args->CheckValue ("-savedir");
	if (leader.IsEmpty())
	{
		leader = save_dir;
		if (leader.IsEmpty())
		{
			leader = M_GetSavegamesPath();
		}
	}
	size_t len = leader.Len();
	if (leader[0] != '\0' && leader[len-1] != '\\' && leader[len-1] != '/')
	{
		slash = "/";
	}
	name << leader << slash;
	name = NicePath(name);
	CreatePath(name);
	name << prefix;
	if (slot >= 0)
	{
		name.AppendFormat("%d." SAVEGAME_EXT, slot);
	}
	return name;
}

CVAR (Int, autosavenum, 0, CVAR_NOSET|CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
static int nextautosave = -1;
CVAR (Int, disableautosave, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, saveloadconfirmation, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // [mxd]
CUSTOM_CVAR (Int, autosavecount, 4, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
}
CVAR (Int, quicksavenum, -1, CVAR_NOSET|CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
static int lastquicksave = -1;
CVAR (Bool, quicksaverotation, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CUSTOM_CVAR (Int, quicksaverotationcount, 4, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
}

void G_DoAutoSave ()
{
	FString description;
	FString file;
	// Keep up to four autosaves at a time
	UCVarValue num;
	const char *readableTime;
	int count = autosavecount != 0 ? autosavecount : 1;
	
	if (nextautosave == -1) 
	{
		nextautosave = (autosavenum + 1) % count;
	}

	num.Int = nextautosave;
	autosavenum.ForceSet (num, CVAR_Int);

	file = G_BuildSaveName ("auto", nextautosave);

	// The hint flag is only relevant on the primary level.
	if (!(primaryLevel->flags2 & LEVEL2_NOAUTOSAVEHINT))
	{
		nextautosave = (nextautosave + 1) % count;
	}
	else
	{
		// This flag can only be used once per level
		primaryLevel->flags2 &= ~LEVEL2_NOAUTOSAVEHINT;
	}

	readableTime = myasctime ();
	description.Format("Autosave %s", readableTime);
	G_DoSaveGame (false, false, file, description);
}

void G_DoQuickSave ()
{
	FString description;
	FString file;
	// Keeps a rotating set of quicksaves
	UCVarValue num;
	const char *readableTime;
	int count = quicksaverotationcount != 0 ? quicksaverotationcount : 1;
	
	if (quicksavenum < 0) 
	{
		lastquicksave = 0;
	}
	else
	{
		lastquicksave = (quicksavenum + 1) % count;
	}

	num.Int = lastquicksave;
	quicksavenum.ForceSet (num, CVAR_Int);

	file = G_BuildSaveName ("quick", lastquicksave);

	readableTime = myasctime ();
	description.Format("Quicksave %s", readableTime);
	G_DoSaveGame (true, true, file, description);
}


static void PutSaveWads (FSerializer &arc)
{
	const char *name;

	// Name of IWAD
	name = fileSystem.GetResourceFileName (fileSystem.GetIwadNum());
	arc.AddString("Game WAD", name);

	// Name of wad the map resides in
	if (fileSystem.GetFileContainer (primaryLevel->lumpnum) > fileSystem.GetIwadNum())
	{
		name = fileSystem.GetResourceFileName (fileSystem.GetFileContainer (primaryLevel->lumpnum));
		arc.AddString("Map WAD", name);
	}
}

static void PutSaveComment (FSerializer &arc)
{
	int levelTime;

	FString comment = myasctime();

	arc.AddString("Creation Time", comment);

	// Get level name
	comment.Format("%s - %s\n", primaryLevel->MapName.GetChars(), primaryLevel->LevelName.GetChars());

	// Append elapsed time
	const char *const time = GStrings("SAVECOMMENT_TIME");
	levelTime = primaryLevel->time / TICRATE;
	comment.AppendFormat("%s: %02d:%02d:%02d", time, levelTime/3600, (levelTime%3600)/60, levelTime%60);

	// Write out the comment
	arc.AddString("Comment", comment);
}

static void PutSavePic (FileWriter *file, int width, int height)
{
	if (width <= 0 || height <= 0 || !storesavepic)
	{
		M_CreateDummyPNG (file);
	}
	else
	{
		D_Render([&]()
			{
				WriteSavePic(&players[consoleplayer], file, width, height);
			}, false);
	}
}

void G_DoSaveGame (bool okForQuicksave, bool forceQuicksave, FString filename, const char *description)
{
	TArray<FCompressedBuffer> savegame_content;
	TArray<FString> savegame_filenames;

	char buf[100];

	// Do not even try, if we're not in a level. (Can happen after
	// a demo finishes playback.)
	if (primaryLevel->lines.Size() == 0 || primaryLevel->sectors.Size() == 0 || gamestate != GS_LEVEL)
	{
		return;
	}

	if (demoplayback)
	{
		filename = G_BuildSaveName ("demosave." SAVEGAME_EXT, -1);
	}

	if (cl_waitforsave)
		I_FreezeTime(true);

	insave = true;
	try
	{
		level.SnapshotLevel();
	}
	catch(CRecoverableError &err)
	{
		// delete the snapshot. Since the save failed it is broken.
		insave = false;
		level.info->Snapshot.Clean();
		Printf(PRINT_HIGH, "Save failed\n");
		Printf(PRINT_HIGH, "%s\n", err.GetMessage());
		// The time freeze must be reset if the save fails.
		if (cl_waitforsave)
			I_FreezeTime(false);
		return;
	}
	catch (...)
	{
		insave = false;
		if (cl_waitforsave)
			I_FreezeTime(false);
		throw;
	}

	BufferWriter savepic;
	FSerializer savegameinfo;		// this is for displayable info about the savegame
	FSerializer savegameglobals;	// and this for non-level related info that must be saved.

	savegameinfo.OpenWriter(true);
	savegameglobals.OpenWriter(save_formatted);

	SaveVersion = SAVEVER;
	PutSavePic(&savepic, SAVEPICWIDTH, SAVEPICHEIGHT);
	mysnprintf(buf, countof(buf), GAMENAME " %s", GetVersionString());
	// put some basic info into the PNG so that this isn't lost when the image gets extracted.
	M_AppendPNGText(&savepic, "Software", buf);
	M_AppendPNGText(&savepic, "Title", description);
	M_AppendPNGText(&savepic, "Current Map", primaryLevel->MapName);
	M_FinishPNG(&savepic);

	int ver = SAVEVER;
	savegameinfo.AddString("Software", buf)
		.AddString("Engine", GAMESIG)
		("Save Version", ver)
		.AddString("Title", description)
		.AddString("Current Map", primaryLevel->MapName);


	PutSaveWads (savegameinfo);
	PutSaveComment (savegameinfo);

	// Intermission stats for hubs
	G_SerializeHub(savegameglobals);
	C_SerializeCVars(savegameglobals, "servercvars", CVAR_SERVERINFO);

	if (level.time != 0 || level.maptime != 0)
	{
		int tic = TICRATE;
		savegameglobals("ticrate", tic);
		savegameglobals("leveltime", level.time);
	}

	STAT_Serialize(savegameglobals);
	FRandom::StaticWriteRNGState(savegameglobals);
	P_WriteACSDefereds(savegameglobals);
	P_WriteACSVars(savegameglobals);
	G_WriteVisited(savegameglobals);


	if (NextSkill != -1)
	{
		savegameglobals("nextskill", NextSkill);
	}

	auto picdata = savepic.GetBuffer();
	FCompressedBuffer bufpng = { picdata->Size(), picdata->Size(), METHOD_STORED, 0, static_cast<unsigned int>(crc32(0, &(*picdata)[0], picdata->Size())), (char*)&(*picdata)[0] };

	savegame_content.Push(bufpng);
	savegame_filenames.Push("savepic.png");
	savegame_content.Push(savegameinfo.GetCompressedOutput());
	savegame_filenames.Push("info.json");
	savegame_content.Push(savegameglobals.GetCompressedOutput());
	savegame_filenames.Push("globals.json");

	G_WriteSnapshots (savegame_filenames, savegame_content);
	

	bool succeeded = false;

	if (WriteZip(filename, savegame_filenames, savegame_content))
	{
		// Check whether the file is ok by trying to open it.
		FResourceFile *test = FResourceFile::OpenResourceFile(filename, true);
		if (test != nullptr)
		{
			delete test;
			succeeded = true;
		}
	}

	if (succeeded)
	{
		savegameManager.NotifyNewSave(filename, description, okForQuicksave, forceQuicksave);
		BackupSaveName = filename;

		if (longsavemessages) Printf("%s (%s)\n", GStrings("GGSAVED"), filename.GetChars());
		else Printf("%s\n", GStrings("GGSAVED"));
	}
	else
	{
		Printf(PRINT_HIGH, "%s\n", GStrings("TXT_SAVEFAILED"));
	}


	// delete the JSON buffers we created just above. Everything else will
	// either still be needed or taken care of automatically.
	savegame_content[1].Clean();
	savegame_content[2].Clean();

	// We don't need the snapshot any longer.
	level.info->Snapshot.Clean();
		
	insave = false;

	if (cl_waitforsave)
		I_FreezeTime(false);
}




//
// DEMO RECORDING
//

void G_ReadDemoTiccmd (ticcmd_t *cmd, int player)
{
	int id = DEM_BAD;

	while (id != DEM_USERCMD && id != DEM_EMPTYUSERCMD)
	{
		if (!demorecording && demo_p >= zdembodyend)
		{
			// nothing left in the BODY chunk, so end playback.
			G_CheckDemoStatus ();
			break;
		}

		id = ReadByte (&demo_p);

		switch (id)
		{
		case DEM_STOP:
			// end of demo stream
			G_CheckDemoStatus ();
			break;

		case DEM_USERCMD:
			UnpackUserCmd (&cmd->ucmd, &cmd->ucmd, &demo_p);
			break;

		case DEM_EMPTYUSERCMD:
			// leave cmd->ucmd unchanged
			break;

		case DEM_DROPPLAYER:
			{
				uint8_t i = ReadByte (&demo_p);
				if (i < MAXPLAYERS)
				{
					playeringame[i] = false;
				}
			}
			break;

		default:
			Net_DoCommand (id, &demo_p, player);
			break;
		}
	}
} 

bool stoprecording;

CCMD (stop)
{
	stoprecording = true;
}

extern uint8_t *lenspot;

void G_WriteDemoTiccmd (ticcmd_t *cmd, int player, int buf)
{
	uint8_t *specdata;
	int speclen;

	if (stoprecording)
	{ // use "stop" console command to end demo recording
		G_CheckDemoStatus ();
		if (!netgame)
		{
			gameaction = ga_fullconsole;
		}
		return;
	}

	// [RH] Write any special "ticcmds" for this player to the demo
	if ((specdata = NetSpecs[player][buf].GetData (&speclen)) && gametic % ticdup == 0)
	{
		memcpy (demo_p, specdata, speclen);
		demo_p += speclen;
		NetSpecs[player][buf].SetData (NULL, 0);
	}

	// [RH] Now write out a "normal" ticcmd.
	WriteUserCmdMessage (&cmd->ucmd, &players[player].cmd.ucmd, &demo_p);

	// [RH] Bigger safety margin
	if (demo_p > demobuffer + maxdemosize - 64)
	{
		ptrdiff_t pos = demo_p - demobuffer;
		ptrdiff_t spot = lenspot - demobuffer;
		ptrdiff_t comp = democompspot - demobuffer;
		ptrdiff_t body = demobodyspot - demobuffer;
		// [RH] Allocate more space for the demo
		maxdemosize += 0x20000;
		demobuffer = (uint8_t *)M_Realloc (demobuffer, maxdemosize);
		demo_p = demobuffer + pos;
		lenspot = demobuffer + spot;
		democompspot = demobuffer + comp;
		demobodyspot = demobuffer + body;
	}
}



//
// G_RecordDemo
//
void G_RecordDemo (const char* name)
{
	usergame = false;
	demoname = name;
	FixPathSeperator (demoname);
	DefaultExtension (demoname, ".lmp");
	maxdemosize = 0x20000;
	demobuffer = (uint8_t *)M_Malloc (maxdemosize);
	demorecording = true; 
}


// [RH] Demos are now saved as IFF FORMs. I've also removed support
//		for earlier ZDEMs since I didn't want to bother supporting
//		something that probably wasn't used much (if at all).

void G_BeginRecording (const char *startmap)
{
	int i;

	if (startmap == NULL)
	{
		startmap = primaryLevel->MapName;
	}
	demo_p = demobuffer;

	WriteLong (FORM_ID, &demo_p);			// Write FORM ID
	demo_p += 4;							// Leave space for len
	WriteLong (ZDEM_ID, &demo_p);			// Write ZDEM ID

	// Write header chunk
	StartChunk (ZDHD_ID, &demo_p);
	WriteWord (DEMOGAMEVERSION, &demo_p);	// Write ZDoom version
	*demo_p++ = 2;							// Write minimum version needed to use this demo.
	*demo_p++ = 3;							// (Useful?)

	strcpy((char*)demo_p, startmap);		// Write name of map demo was recorded on.
	demo_p += strlen(startmap) + 1;
	WriteLong(rngseed, &demo_p);			// Write RNG seed
	*demo_p++ = consoleplayer;
	FinishChunk (&demo_p);

	// Write player info chunks
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			StartChunk (UINF_ID, &demo_p);
			WriteByte ((uint8_t)i, &demo_p);
			D_WriteUserInfoStrings (i, &demo_p);
			FinishChunk (&demo_p);
		}
	}

	// It is possible to start a "multiplayer" game with only one player,
	// so checking the number of players when playing back the demo is not
	// enough.
	if (multiplayer)
	{
		StartChunk (NETD_ID, &demo_p);
		FinishChunk (&demo_p);
	}

	// Write cvars chunk
	StartChunk (VARS_ID, &demo_p);
	C_WriteCVars (&demo_p, CVAR_SERVERINFO|CVAR_DEMOSAVE);
	FinishChunk (&demo_p);

	// Write weapon ordering chunk
	StartChunk (WEAP_ID, &demo_p);
	P_WriteDemoWeaponsChunk(&demo_p);
	FinishChunk (&demo_p);

	// Indicate body is compressed
	StartChunk (COMP_ID, &demo_p);
	democompspot = demo_p;
	WriteLong (0, &demo_p);
	FinishChunk (&demo_p);

	// Begin BODY chunk
	StartChunk (BODY_ID, &demo_p);
	demobodyspot = demo_p;
}


//
// G_PlayDemo
//

FString defdemoname;

void G_DeferedPlayDemo (const char *name)
{
	defdemoname = name;
	gameaction = (gameaction == ga_loadgame) ? ga_loadgameplaydemo : ga_playdemo;
}

UNSAFE_CCMD (playdemo)
{
	if (netgame)
	{
		Printf("End your current netgame first!\n");
		return;
	}
	if (demorecording)
	{
		Printf("End your current demo first!\n");
		return;
	}
	if (argv.argc() > 1)
	{
		G_DeferedPlayDemo (argv[1]);
		singledemo = true;
	}
}

UNSAFE_CCMD (timedemo)
{
	if (argv.argc() > 1)
	{
		G_TimeDemo (argv[1]);
		singledemo = true;
	}
}

// [RH] Process all the information in a FORM ZDEM
//		until a BODY chunk is entered.
bool G_ProcessIFFDemo (FString &mapname)
{
	bool headerHit = false;
	bool bodyHit = false;
	int numPlayers = 0;
	int id, len, i;
	uLong uncompSize = 0;
	uint8_t *nextchunk;

	demoplayback = true;

	for (i = 0; i < MAXPLAYERS; i++)
		playeringame[i] = 0;

	len = ReadLong (&demo_p);
	zdemformend = demo_p + len + (len & 1);

	// Check to make sure this is a ZDEM chunk file.
	// TODO: Support multiple FORM ZDEMs in a CAT. Might be useful.

	id = ReadLong (&demo_p);
	if (id != ZDEM_ID)
	{
		Printf ("Not a " GAMENAME " demo file!\n");
		return true;
	}

	// Process all chunks until a BODY chunk is encountered.

	while (demo_p < zdemformend && !bodyHit)
	{
		id = ReadLong (&demo_p);
		len = ReadLong (&demo_p);
		nextchunk = demo_p + len + (len & 1);
		if (nextchunk > zdemformend)
		{
			Printf ("Demo is mangled!\n");
			return true;
		}

		switch (id)
		{
		case ZDHD_ID:
			headerHit = true;

			demover = ReadWord (&demo_p);	// ZDoom version demo was created with
			if (demover < MINDEMOVERSION)
			{
				Printf ("Demo requires an older version of " GAMENAME "!\n");
				//return true;
			}
			if (ReadWord (&demo_p) > DEMOGAMEVERSION)	// Minimum ZDoom version
			{
				Printf ("Demo requires a newer version of " GAMENAME "!\n");
				return true;
			}
			if (demover >= 0x21a)
			{
				mapname = (char*)demo_p;
				demo_p += mapname.Len() + 1;
			}
			else
			{
				mapname = FString((char*)demo_p, 8);
				demo_p += 8;
			}
			rngseed = ReadLong (&demo_p);
			// Only reset the RNG if this demo is not in conjunction with a savegame.
			if (mapname[0] != 0)
			{
				FRandom::StaticClearRandom ();
			}
			consoleplayer = *demo_p++;
			break;

		case VARS_ID:
			C_ReadCVars (&demo_p);
			break;

		case UINF_ID:
			i = ReadByte (&demo_p);
			if (!playeringame[i])
			{
				playeringame[i] = 1;
				numPlayers++;
			}
			D_ReadUserInfoStrings (i, &demo_p, false);
			break;

		case NETD_ID:
			multiplayer = true;
			break;

		case WEAP_ID:
			P_ReadDemoWeaponsChunk(&demo_p);
			break;

		case BODY_ID:
			bodyHit = true;
			zdembodyend = demo_p + len;
			break;

		case COMP_ID:
			uncompSize = ReadLong (&demo_p);
			break;
		}

		if (!bodyHit)
			demo_p = nextchunk;
	}

	if (!headerHit)
	{
		Printf ("Demo has no header!\n");
		return true;
	}

	if (!numPlayers)
	{
		Printf ("Demo has no players!\n");
		return true;
	}

	if (!bodyHit)
	{
		zdembodyend = NULL;
		Printf ("Demo has no BODY chunk!\n");
		return true;
	}

	if (numPlayers > 1)
		multiplayer = netgame = true;

	if (uncompSize > 0)
	{
		uint8_t *uncompressed = (uint8_t*)M_Malloc(uncompSize);
		int r = uncompress (uncompressed, &uncompSize, demo_p, uLong(zdembodyend - demo_p));
		if (r != Z_OK)
		{
			Printf ("Could not decompress demo! %s\n", M_ZLibError(r).GetChars());
			M_Free(uncompressed);
			return true;
		}
		M_Free (demobuffer);
		zdembodyend = uncompressed + uncompSize;
		demobuffer = demo_p = uncompressed;
	}

	return false;
}

void G_DoPlayDemo (void)
{
	FString mapname;
	int demolump;

	gameaction = ga_nothing;

	// [RH] Allow for demos not loaded as lumps
	demolump = fileSystem.CheckNumForFullName (defdemoname, true);
	if (demolump >= 0)
	{
		int demolen = fileSystem.FileLength (demolump);
		demobuffer = (uint8_t *)M_Malloc(demolen);
		fileSystem.ReadFile (demolump, demobuffer);
	}
	else
	{
		FixPathSeperator (defdemoname);
		DefaultExtension (defdemoname, ".lmp");
		FileReader fr;
		if (!fr.OpenFile(defdemoname))
		{
			I_Error("Unable to open demo '%s'", defdemoname.GetChars());
		}
		auto len = fr.GetLength();
		demobuffer = (uint8_t*)M_Malloc(len);
		if (fr.Read(demobuffer, len) != len)
		{
			I_Error("Unable to read demo '%s'", defdemoname.GetChars());
		}
	}
	demo_p = demobuffer;

	if (singledemo) Printf ("Playing demo %s\n", defdemoname.GetChars());

	C_BackupCVars ();		// [RH] Save cvars that might be affected by demo

	if (ReadLong (&demo_p) != FORM_ID)
	{
		const char *eek = "Cannot play non-" GAMENAME " demos.\n";

		C_ForgetCVars();
		M_Free(demobuffer);
		demo_p = demobuffer = NULL;
		if (singledemo)
		{
			I_Error ("%s", eek);
		}
		else
		{
			//Printf (PRINTF_BOLD, "%s", eek);
			gameaction = ga_nothing;
		}
	}
	else if (G_ProcessIFFDemo (mapname))
	{
		C_RestoreCVars();
		gameaction = ga_nothing;
		demoplayback = false;
	}
	else
	{
		// don't spend a lot of time in loadlevel 
		precache = false;
		demonew = true;
		if (mapname.Len() != 0)
		{
			G_InitNew (mapname, false);
		}
		else if (primaryLevel->sectors.Size() == 0)
		{
			I_Error("Cannot play demo without its savegame\n");
		}
		C_HideConsole ();
		demonew = false;
		precache = true;

		usergame = false;
		demoplayback = true;
		playedtitlemusic = false;
	}
}

//
// G_TimeDemo
//
void G_TimeDemo (const char* name)
{
	nodrawers = !!Args->CheckParm ("-nodraw");
	noblit = !!Args->CheckParm ("-noblit");
	timingdemo = true;
	singletics = true;

	defdemoname = name;
	gameaction = (gameaction == ga_loadgame) ? ga_loadgameplaydemo : ga_playdemo;
}


/*
===================
=
= G_CheckDemoStatus
=
= Called after a death or level completion to allow demos to be cleaned up
= Returns true if a new demo loop action will take place
===================
*/

bool G_CheckDemoStatus (void)
{
	if (!demorecording)
	{ // [RH] Restore the player's userinfo settings.
		D_SetupUserInfo();
	}

	if (demoplayback)
	{
		extern int starttime;
		int endtime = 0;

		if (timingdemo)
			endtime = I_GetTime () - starttime;

		C_RestoreCVars ();		// [RH] Restore cvars demo might have changed
		M_Free (demobuffer);
		demobuffer = NULL;

		P_SetupWeapons_ntohton();
		demoplayback = false;
		netgame = false;
		multiplayer = false;
		singletics = false;
		for (int i = 1; i < MAXPLAYERS; i++)
			playeringame[i] = 0;
		consoleplayer = 0;
		players[0].camera = nullptr;
		if (StatusBar != NULL)
		{
			StatusBar->AttachToPlayer (&players[0]);
		}
		if (singledemo || timingdemo)
		{
			if (timingdemo)
			{
				// Trying to get back to a stable state after timing a demo
				// seems to cause problems. I don't feel like fixing that
				// right now.
				I_FatalError ("timed %i gametics in %i realtics (%.1f fps)\n"
							  "(This is not really an error.)", gametic,
							  endtime, (float)gametic/(float)endtime*(float)TICRATE);
			}
			else
			{
				Printf ("Demo ended.\n");
			}
			gameaction = ga_fullconsole;
			timingdemo = false;
			return false;
		}
		else
		{
			D_AdvanceDemo (); 
		}

		return true; 
	}

	if (demorecording)
	{
		uint8_t *formlen;

		WriteByte (DEM_STOP, &demo_p);

		if (demo_compress)
		{
			// Now that the entire BODY chunk has been created, replace it with
			// a compressed version. If the BODY successfully compresses, the
			// contents of the COMP chunk will be changed to indicate the
			// uncompressed size of the BODY.
			uLong len = uLong(demo_p - demobodyspot);
			uLong outlen = (len + len/100 + 12);
			TArray<Byte> compressed(outlen, true);
			int r = compress2 (compressed.Data(), &outlen, demobodyspot, len, 9);
			if (r == Z_OK && outlen < len)
			{
				formlen = democompspot;
				WriteLong (len, &democompspot);
				memcpy (demobodyspot, compressed.Data(), outlen);
				demo_p = demobodyspot + outlen;
			}
		}
		FinishChunk (&demo_p);
		formlen = demobuffer + 4;
		WriteLong (int(demo_p - demobuffer - 8), &formlen);

		auto fw = FileWriter::Open(demoname);
		bool saved = false;
		if (fw != nullptr)
		{
			const size_t size = demo_p - demobuffer;
			saved = fw->Write(demobuffer, size) == size;
			delete fw;
			if (!saved) remove(demoname);
		}
		M_Free (demobuffer); 
		demorecording = false;
		stoprecording = false;
		if (saved)
		{
			Printf ("Demo %s recorded\n", demoname.GetChars()); 
		}
		else
		{
			Printf ("Demo %s could not be saved\n", demoname.GetChars());
		}
	}

	return false; 
}

void G_StartSlideshow(FLevelLocals *Level, FName whichone)
{
	gameaction = ga_slideshow;
	SelectedSlideshow = whichone == NAME_None ? Level->info->slideshow : whichone;
}

DEFINE_ACTION_FUNCTION(FLevelLocals, StartSlideshow)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_NAME(whichone);
	G_StartSlideshow(self, whichone);
	return 0;
}

DEFINE_ACTION_FUNCTION(FLevelLocals, MakeScreenShot)
{
	if (enablescriptscreenshot)
	{
		G_ScreenShot("");
	}
	return 0;
}

void G_MakeAutoSave()
{
	if (gameaction == ga_nothing)
	{
		gameaction = ga_autosave;
	}
}

DEFINE_ACTION_FUNCTION(FLevelLocals, MakeAutoSave)
{
	G_MakeAutoSave();
	return 0;
}

DEFINE_GLOBAL(players)
DEFINE_GLOBAL(playeringame)
DEFINE_GLOBAL(PlayerClasses)
DEFINE_GLOBAL_NAMED(Skins, PlayerSkins)
DEFINE_GLOBAL(consoleplayer)
DEFINE_GLOBAL_NAMED(PClassActor::AllActorClasses, AllActorClasses)
DEFINE_GLOBAL_NAMED(primaryLevel, Level)
DEFINE_GLOBAL(validcount)
DEFINE_GLOBAL(multiplayer)
DEFINE_GLOBAL(gameaction)
DEFINE_GLOBAL(gamestate)
DEFINE_GLOBAL(skyflatnum)
DEFINE_GLOBAL(globalfreeze)
DEFINE_GLOBAL(gametic)
DEFINE_GLOBAL(demoplayback)
DEFINE_GLOBAL(automapactive);
DEFINE_GLOBAL(Net_Arbitrator);
DEFINE_GLOBAL(netgame);

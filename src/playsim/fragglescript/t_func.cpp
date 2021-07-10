// Emacs style mode select -*- C++ -*-
//---------------------------------------------------------------------------
//
// Copyright(C) 2000 Simon Howard
// Copyright(C) 2005-2008 Christoph Oelckers
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// Functions
//
// functions are stored as variables(see variable.c), the
// value being a pointer to a 'handler' function for the
// function. Arguments are stored in an argc/argv-style list
//
// this module contains all the handler functions for the
// basic FraggleScript Functions.
//
// By Simon Howard
//
//---------------------------------------------------------------------------
//

#include "templates.h"
#include "p_local.h"
#include "t_script.h"
#include "sbar.h"
#include "c_dispatch.h"
#include "d_player.h"
#include "filesystem.h"
#include "gi.h"
#include "v_font.h"
#include "serializer.h"
#include "p_spec.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "scriptutil.h"
#include "vm.h"
#include "a_lights.h"
#include "s_music.h"
#include "texturemanager.h"

static FRandom pr_script("FScript");

// functions. FParser::SF_ means Script Function not, well.. heh, me

/////////// actually running a function /////////////

//==========================================================================
//
// The Doom actors in their original order
//
//==========================================================================

static const char * const ActorNames_init[]=
{
	"DoomPlayer",
	"ZombieMan",
	"ShotgunGuy",
	"Archvile",
	"ArchvileFire",
	"Revenant",
	"RevenantTracer",
	"RevenantTracerSmoke",
	"Fatso",
	"FatShot",
	"ChaingunGuy",
	"DoomImp",
	"Demon",
	"Spectre",
	"Cacodemon",
	"BaronOfHell",
	"BaronBall",
	"HellKnight",
	"LostSoul",
	"SpiderMastermind",
	"Arachnotron",
	"Cyberdemon",
	"PainElemental",
	"WolfensteinSS",
	"CommanderKeen",
	"BossBrain",
	"BossEye",
	"BossTarget",
	"SpawnShot",
	"SpawnFire",
	"ExplosiveBarrel",
	"DoomImpBall",
	"CacodemonBall",
	"Rocket",
	"PlasmaBall",
	"BFGBall",
	"ArachnotronPlasma",
	"BulletPuff",
	"Blood",
	"TeleportFog",
	"ItemFog",
	"TeleportDest",
	"BFGExtra",
	"GreenArmor",
	"BlueArmor",
	"HealthBonus",
	"ArmorBonus",
	"BlueCard",
	"RedCard",
	"YellowCard",
	"YellowSkull",
	"RedSkull",
	"BlueSkull",
	"Stimpack",
	"Medikit",
	"Soulsphere",
	"InvulnerabilitySphere",
	"Berserk",
	"BlurSphere",
	"RadSuit",
	"Allmap",
	"Infrared",
	"Megasphere",
	"Clip",
	"ClipBox",
	"RocketAmmo",
	"RocketBox",
	"Cell",
	"CellPack",
	"Shell",
	"ShellBox",
	"Backpack",
	"BFG9000",
	"Chaingun",
	"Chainsaw",
	"RocketLauncher",
	"PlasmaRifle",
	"Shotgun",
	"SuperShotgun",
	"TechLamp",
	"TechLamp2",
	"Column",
	"TallGreenColumn",
	"ShortGreenColumn",
	"TallRedColumn",
	"ShortRedColumn",
	"SkullColumn",
	"HeartColumn",
	"EvilEye",
	"FloatingSkull",
	"TorchTree",
	"BlueTorch",
	"GreenTorch",
	"RedTorch",
	"ShortBlueTorch",
	"ShortGreenTorch",
	"ShortRedTorch",
	"Slalagtite",
	"TechPillar",
	"CandleStick",
	"Candelabra",
	"BloodyTwitch",
	"Meat2",
	"Meat3",
	"Meat4",
	"Meat5",
	"NonsolidMeat2",
	"NonsolidMeat4",
	"NonsolidMeat3",
	"NonsolidMeat5",
	"NonsolidTwitch",
	"DeadCacodemon",
	"DeadMarine",
	"DeadZombieMan",
	"DeadDemon",
	"DeadLostSoul",
	"DeadDoomImp",
	"DeadShotgunGuy",
	"GibbedMarine",
	"GibbedMarineExtra",
	"HeadsOnAStick",
	"Gibs",
	"HeadOnAStick",
	"HeadCandles",
	"DeadStick",
	"LiveStick",
	"BigTree",
	"BurningBarrel",
	"HangNoGuts",
	"HangBNoBrain",
	"HangTLookingDown",
	"HangTSkull",
	"HangTLookingUp",
	"HangTNoBrain",
	"ColonGibs",
	"SmallBloodPool",
	"BrainStem",
	"PointPusher",
	"PointPuller",
};

static PClassActor * ActorTypes[countof(ActorNames_init)];

//==========================================================================
//
// Some functions that take care of the major differences between the class
// based actor system from ZDoom and Doom's index based one
//
//==========================================================================

//==========================================================================
//
// Gets an actor class
// Input can be either a class name, an actor variable or a Doom index
// Doom index is only supported for the original things up to MBF
//
//==========================================================================
PClassActor * FParser::T_GetMobjType(svalue_t arg)
{
	PClassActor * pclass=NULL;
	
	if (arg.type==svt_string)
	{
		pclass=PClass::FindActor(arg.string);

		// invalid object to spawn
		if(!pclass) script_error("unknown object type: %s\n", arg.string.GetChars()); 
	}
	else if (arg.type==svt_mobj)
	{
		AActor * mo = actorvalue(arg);
		if (mo) pclass = mo->GetClass();
	}
	else
	{
		int objtype = intvalue(arg);
		if (objtype>=0 && objtype<int(countof(ActorTypes))) pclass=ActorTypes[objtype];
		else pclass=NULL;

		// invalid object to spawn
		if(!pclass) script_error("unknown object type: %i\n", objtype); 
	}
	return pclass;
}

//==========================================================================
//
// Gets a player index
// Input can be either an actor variable or an index value
//
//==========================================================================
int FParser::T_GetPlayerNum(const svalue_t &arg)
{
	int playernum;
	if(arg.type == svt_mobj)
	{
		if(!actorvalue(arg) || !arg.value.mobj->player)
		{
			// I prefer this not to make an error.
			// This way a player function used for a non-player
			// object will just do nothing
			//script_error("mobj not a player!\n");
			return -1;
		}
		playernum = Level->PlayerNum(arg.value.mobj->player);
	}
	else
		playernum = intvalue(arg);
	
	if(playernum < 0 || playernum > MAXPLAYERS)
	{
		return -1;
	}
	if(!Level->PlayerInGame(playernum)) // no error, just return -1
	{
		return -1;
	}
	return playernum;
}

AActor *FParser::T_GetPlayerActor(const svalue_t &arg)
{
	int num = T_GetPlayerNum(arg);
	return num == -1 ? nullptr : Level->Players[num]->mo;
}

PClassActor *T_ClassType(const svalue_t &arg)
{
	return PClass::FindActor(stringvalue(arg));
}

//==========================================================================
//
// Get an ammo type
// Input can be either a class name or a Doom index
// Doom index is only supported for the 4 original ammo types
//
//==========================================================================
static PClassActor * T_GetAmmo(const svalue_t &t)
{
	const char * p;

	if (t.type==svt_string)
	{
		p=stringvalue(t);
	}
	else	
	{
		// backwards compatibility with Legacy.
		// allow only Doom's standard types here!
		static const char * DefAmmo[]={"Clip","Shell","Cell","RocketAmmo"};
		int ammonum = intvalue(t);
		if(ammonum < 0 || ammonum >= 4)	
		{
			script_error("ammo number out of range: %i", ammonum);
			return NULL;
		}
		p=DefAmmo[ammonum];
	}
	auto am = PClass::FindActor(p);
	if (am == NULL || !am->IsDescendantOf(PClass::FindClass(NAME_Ammo)))
	{
		script_error("unknown ammo type : %s", p);
		return NULL;
	}
	return am;

}

//==========================================================================
//
// Finds a sound in the sound table and adds a new entry if it isn't defined
// It's too bad that this is necessary but FS doesn't know about this kind
// of sound management.
//
//==========================================================================
static FSoundID T_FindSound(const char * name)
{
	char buffer[40];
	FSoundID so=S_FindSound(name);

	if (so>0) return so;

	// Now it gets dirty!

	if (gameinfo.gametype & GAME_DoomStrifeChex)
	{
		mysnprintf(buffer, countof(buffer), "DS%.35s", name);
		if (fileSystem.CheckNumForName(buffer, ns_sounds)<0) strcpy(buffer, name);
	}
	else
	{
		strcpy(buffer, name);
		if (fileSystem.CheckNumForName(buffer, ns_sounds)<0) mysnprintf(buffer, countof(buffer), "DS%.35s", name);
	}

	int id = S_AddSound(name, buffer);
	soundEngine->HashSounds();
	return FSoundID(id);
}


//==========================================================================
//
// Creates a string out of a print argument list. This version does not
// have any length restrictions like the original FS versions had.
//
//==========================================================================
FString FParser::GetFormatString(int startarg)
{
	FString fmt="";
	for(int i=startarg; i<t_argc; i++) fmt += stringvalue(t_argv[i]);
	return fmt;
}

bool FParser::CheckArgs(int cnt)
{
	if (t_argc<cnt)
	{
		script_error("Insufficient parameters for '%s'\n", t_func.GetChars());
		return false;
	}
	return true;
}
//==========================================================================

//FUNCTIONS

// the actual handler functions for the
// functions themselves

// arguments are evaluated and passed to the
// handler functions using 't_argc' and 't_argv'
// in a similar way to the way C does with command
// line options.

// values can be returned from the functions using
// the variable 't_return'
//
//==========================================================================

//==========================================================================
//
// prints some text to the console and the notify buffer
//
//==========================================================================
void FParser::SF_Print(void)
{
	Printf(PRINT_HIGH, "%s\n", GetFormatString(0).GetChars());
}


//==========================================================================
//
// return a random number from 0 to 255
//
//==========================================================================
void FParser::SF_Rnd(void)
{
	t_return.type = svt_int;
	t_return.value.i = pr_script();
}

//==========================================================================
//
// looping section. using the rover, find the highest level
// loop we are currently in and return the DFsSection for it.
//
//==========================================================================

DFsSection *FParser::looping_section()
{
	DFsSection *best = NULL;         // highest level loop we're in
	// that has been found so far
	int n;
	
	// check thru all the hashchains
	int32_t rover_index = Script->MakeIndex(Rover);
	
	for(n=0; n<SECTIONSLOTS; n++)
	{
		DFsSection *current = Script->sections[n];
		
		// check all the sections in this hashchain
		while(current)
		{
			// a loop?
			
			if(current->type == st_loop)
				// check to see if it's a loop that we're inside
				if(rover_index >= current->start_index && rover_index <= current->end_index)
				{
					// a higher nesting level than the best one so far?
					if(!best || (current->start_index > best->start_index))
						best = current;     // save it
				}
			current = current->next;
		}
	}
	
	return best;    // return the best one found
}

//==========================================================================
//
// "continue;" in FraggleScript is a function
//
//==========================================================================

void FParser::SF_Continue(void)
{
	DFsSection *section;
	
	if(!(section = looping_section()) )       // no loop found
	{
		script_error("continue() not in loop\n");
		return;
	}
	
	Rover = Script->SectionEnd(section);      // jump to the closing brace
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Break(void)
{
	DFsSection *section;
	
	if(!(section = looping_section()) )
	{
		script_error("break() not in loop\n");
		return;
	}
	
	Rover = Script->SectionEnd(section) + 1;   // jump out of the loop
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Goto(void)
{
	if (CheckArgs(1))
	{
		// check argument is a labelptr
		
		if(t_argv[0].type != svt_label)
		{
			script_error("goto argument not a label\n");
			return;
		}
		
		// go there then if everythings fine
		Rover = Script->LabelValue(t_argv[0]);
	}	
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Return(void)
{
	throw CFsTerminator();
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Include(void)
{
	char tempstr[12];
	
	if (CheckArgs(1))
	{
		if(t_argv[0].type == svt_string)
		{
			strncpy(tempstr, t_argv[0].string, 8);
			tempstr[8]=0;
		}
		else
			mysnprintf(tempstr, countof(tempstr), "%i", (int)t_argv[0].value.i);
		
		Script->ParseInclude(Level, tempstr);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Input(void)
{
	Printf(PRINT_DEFAULT, PRINTF_BOLD,"input() function not available in Doom\n");
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Beep(void)
{
	S_Sound(CHAN_AUTO, 0, "misc/chat", 1.0f, ATTN_IDLE);
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Clock(void)
{
	t_return.type = svt_int;
	t_return.value.i = (gametic*100)/TICRATE;
}

/**************** doom stuff ****************/

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ExitLevel(void)
{
	Level->ExitLevel(0, false);
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Tip(void)
{
	if (t_argc>0 && Script->trigger &&
		Script->trigger->CheckLocalView()) 
	{
		C_MidPrint(nullptr, GetFormatString(0).GetChars());
	}
}

//==========================================================================
//
// FParser::SF_TimedTip
//
// Implements: void timedtip(int clocks, ...)
//
//==========================================================================

EXTERN_CVAR(Float, con_midtime)

void FParser::SF_TimedTip(void)
{
	if (CheckArgs(2))
	{
		float saved = con_midtime;
		con_midtime = intvalue(t_argv[0])/100.0f;
		C_MidPrint(nullptr, GetFormatString(1).GetChars());
		con_midtime=saved;
	}
}


//==========================================================================
//
// tip to a particular player
//
//==========================================================================

void FParser::SF_PlayerTip(void)
{
	if (CheckArgs(1))
	{
		int plnum = T_GetPlayerNum(t_argv[0]);
		if (plnum!=-1 && Level->Players[plnum]->mo->CheckLocalView())
		{
			C_MidPrint(nullptr, GetFormatString(1).GetChars());
		}
	}
}

//==========================================================================
//
// message player
//
//==========================================================================

void FParser::SF_Message(void)
{
	if (t_argc>0 && Script->trigger &&
		Script->trigger->CheckLocalView())
	{
		Printf(PRINT_HIGH, "%s\n", GetFormatString(0).GetChars());
	}
}

//==========================================================================
//
// message to a particular player
//
//==========================================================================

void FParser::SF_PlayerMsg(void)
{
	if (CheckArgs(1))
	{
		int plnum = T_GetPlayerNum(t_argv[0]);
		if (plnum!=-1 && Level->Players[plnum]->mo->CheckLocalView())
		{
			Printf(PRINT_HIGH, "%s\n", GetFormatString(1).GetChars());
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_PlayerInGame(void)
{
	if (CheckArgs(1))
	{
		int plnum = T_GetPlayerNum(t_argv[0]);

		if (plnum!=-1)
		{
			t_return.type = svt_int;
			t_return.value.i = Level->PlayerInGame(plnum);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_PlayerName(void)
{
	int plnum;
	
	if(!t_argc)
	{
		player_t *pl=NULL;
		if (Script->trigger) pl = Script->trigger->player;
		if(pl) plnum = Level->PlayerNum(pl);
		else plnum=-1;
	}
	else
		plnum = T_GetPlayerNum(t_argv[0]);
	
	if(plnum !=-1)
	{
		t_return.type = svt_string;
		t_return.string = Level->Players[plnum]->userinfo.GetName();
	}
	else
	{
		script_error("script not started by player\n");
	}
}

//==========================================================================
//
// object being controlled by player
//
//==========================================================================

void FParser::SF_PlayerObj(void)
{
	int plnum;

	if(!t_argc)
	{
		player_t *pl=NULL;
		if (Script->trigger) pl = Script->trigger->player;
		if(pl) plnum = Level->PlayerNum(pl);
		else plnum=-1;
	}
	else
		plnum = T_GetPlayerNum(t_argv[0]);

	if(plnum !=-1)
	{
		t_return.type = svt_mobj;
		t_return.value.mobj = Level->Players[plnum]->mo;
	}
	else
	{
		script_error("script not started by player\n");
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Player(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_int;
	
	if(mo && mo->player) // haleyjd: added mo->player
	{
		t_return.value.i = Level->PlayerNum(mo->player);
	}
	else
	{
		t_return.value.i = -1;
	}
}

//==========================================================================
//
// FParser::SF_Spawn
// 
// Implements: mobj spawn(int type, int x, int y, [int angle], [int z], [bool zrel])
//
//==========================================================================

void FParser::SF_Spawn(void)
{
	DVector3 pos;
	PClassActor *pclass;
	DAngle angle = 0.;
	
	if (CheckArgs(3))
	{
		if (!(pclass=T_GetMobjType(t_argv[0]))) return;
		
		pos.X = floatvalue(t_argv[1]);
		pos.Y = floatvalue(t_argv[2]);

		if(t_argc >= 5)
		{
			pos.Z = floatvalue(t_argv[4]);
			// [Graf Zahl] added option of spawning with a relative z coordinate
			if(t_argc > 5)
			{
				if (intvalue(t_argv[5])) pos.Z += Level->PointInSector(pos)->floorplane.ZatPoint(pos);
			}
		}
		else
		{
			// Legacy compatibility is more important than correctness.
			pos.Z = ONFLOORZ;// (GetDefaultByType(PClass)->flags & MF_SPAWNCEILING) ? ONCEILINGZ : ONFLOORZ;
		}
		
		if(t_argc >= 4)
		{
			angle = floatvalue(t_argv[3]);
		}
		
		t_return.type = svt_mobj;
		t_return.value.mobj = Spawn(Level, pclass, pos, ALLOW_REPLACE);

		if (t_return.value.mobj)		
		{
			t_return.value.mobj->Angles.Yaw = angle;

			if (!Level->info->fs_nocheckposition)
			{
				if (!P_TestMobjLocation(t_return.value.mobj))
				{
					if (t_return.value.mobj->flags&MF_COUNTKILL) Level->total_monsters--;
					if (t_return.value.mobj->flags&MF_COUNTITEM) Level->total_items--;
					t_return.value.mobj->Destroy();
					t_return.value.mobj = NULL;
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

void FParser::SF_RemoveObj(void)
{
	if (CheckArgs(1))
	{
		AActor * mo = actorvalue(t_argv[0]);
		if(mo)  // nullptr check
		{
			mo->ClearCounters();
			mo->Destroy();
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_KillObj(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;  // default to trigger object
	}

	if(mo) 
	{
		// ensure the thing can be killed
		mo->flags|=MF_SHOOTABLE;
		mo->flags2&=~(MF2_INVULNERABLE|MF2_DORMANT);
		// [GrafZahl] This called P_KillMobj directly 
		// which is a very bad thing to do!
		P_DamageMobj(mo, NULL, NULL, mo->health, NAME_Massacre);
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjX(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.setDouble(mo ? mo->X() : 0.);
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjY(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.setDouble(mo ? mo->Y() : 0.);
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjZ(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;	
	}

	t_return.setDouble(mo ? mo->Z() : 0.);
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjAngle(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.setDouble(mo ? mo->Angles.Yaw.Degrees : 0.);
}


//==========================================================================
//
//
//
//==========================================================================

// teleport: object, sector_tag
void FParser::SF_Teleport(void)
{
	int tag;
	AActor *mo;
	
	if (CheckArgs(1))
	{
		if(t_argc == 1)    // 1 argument: sector tag
		{
			mo = Script->trigger;   // default to trigger
			tag = intvalue(t_argv[0]);
		}
		else    // 2 or more
		{                       // teleport a given object
			mo = actorvalue(t_argv[0]);
			tag = intvalue(t_argv[1]);
		}
		
		if(mo)
			Level->EV_Teleport(0, tag, NULL, 0, mo, TELF_DESTFOG | TELF_SOURCEFOG);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SilentTeleport(void)
{
	int tag;
	AActor *mo;
	
	if (CheckArgs(1))
	{
		if(t_argc == 1)    // 1 argument: sector tag
		{
			mo = Script->trigger;   // default to trigger
			tag = intvalue(t_argv[0]);
		}
		else    // 2 or more
		{                       // teleport a given object
			mo = actorvalue(t_argv[0]);
			tag = intvalue(t_argv[1]);
		}
		
		if(mo)
			Level->EV_Teleport(0, tag, NULL, 0, mo, TELF_KEEPORIENTATION);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_DamageObj(void)
{
	AActor *mo;
	int damageamount;
	
	if (CheckArgs(1))
	{
		if(t_argc == 1)    // 1 argument: damage trigger by amount
		{
			mo = Script->trigger;   // default to trigger
			damageamount = intvalue(t_argv[0]);
		}
		else    // 2 or more
		{                       // damage a given object
			mo = actorvalue(t_argv[0]);
			damageamount = intvalue(t_argv[1]);
		}
		
		if(mo)
			P_DamageMobj(mo, NULL, Script->trigger, damageamount, NAME_None);
	}
}

//==========================================================================
//
//
//
//==========================================================================

// the tag number of the sector the thing is in
void FParser::SF_ObjSector(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_int;
	t_return.value.i = mo ? Level->GetFirstSectorTag(mo->Sector) : 0; // nullptr check
}

//==========================================================================
//
//
//
//==========================================================================

// the health number of an object
void FParser::SF_ObjHealth(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_int;
	t_return.value.i = mo ? mo->health : 0;
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjFlag(void)
{
	AActor *mo;
	int flagnum;
	
	if (CheckArgs(1))
	{
		if(t_argc == 1)         // use trigger, 1st is flag
		{
			// use trigger:
			mo = Script->trigger;
			flagnum = intvalue(t_argv[0]);
		}
		else if(t_argc == 2)	// specified object
		{
			mo = actorvalue(t_argv[0]);
			flagnum = intvalue(t_argv[1]);
		}
		else                     // >= 3 : SET flags
		{
			mo = actorvalue(t_argv[0]);
			flagnum = intvalue(t_argv[1]);
			
			if(mo && flagnum<26)          // nullptr check
			{
				uint32_t tempflags = mo->flags;

				// remove old bit
				tempflags &= ~(1 << flagnum);
				
				// make the new flag
				tempflags |= (!!intvalue(t_argv[2])) << flagnum;
				mo->flags = ActorFlags::FromInt (tempflags);
			}  
		}
		t_return.type = svt_int;  
		if (mo && flagnum<26)
		{
			t_return.value.i = !!(mo->flags & ActorFlags::FromInt(1 << flagnum));
		}
		else t_return.value.i = 0;
	}
}

//==========================================================================
//
//
//
//==========================================================================

// apply momentum to a thing
void FParser::SF_PushThing(void)
{
	if (CheckArgs(3))
	{
		AActor * mo = actorvalue(t_argv[0]);
		if(!mo) return;
	
		DAngle angle = floatvalue(t_argv[1]);
		double force = floatvalue(t_argv[2]);
		mo->Thrust(angle, force);
	}
}

//==========================================================================
//
//  FParser::SF_ReactionTime -- useful for freezing things
//
//==========================================================================


void FParser::SF_ReactionTime(void)
{
	if (CheckArgs(1))
	{
		AActor *mo = actorvalue(t_argv[0]);
	
		if(t_argc > 1)
		{
			if(mo) mo->reactiontime = (intvalue(t_argv[1]) * TICRATE) / 100;
		}
	
		t_return.type = svt_int;
		t_return.value.i = mo ? mo->reactiontime : 0;
	}
}

//==========================================================================
//
//  FParser::SF_MobjTarget   -- sets a thing's target field
//
//==========================================================================

// Sets a mobj's Target! >:)
void FParser::SF_MobjTarget(void)
{
	AActor*  mo;
	AActor*  target;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if(t_argc > 1)
		{
			target = actorvalue(t_argv[1]);
			if(mo && target && mo->SeeState) // haleyjd: added target check -- no NULL allowed
			{
				mo->target=target;
				mo->SetState(mo->SeeState);
				mo->flags|=MF_JUSTHIT;
			}
		}
		
		t_return.type = svt_mobj;
		t_return.value.mobj = mo ? mo->target : NULL;
	}
}

//==========================================================================
//
//  FParser::SF_MobjMomx, MobjMomy, MobjMomz -- momentum functions
//
//==========================================================================

void FParser::SF_MobjMomx(void)
{
	AActor*   mo;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if(t_argc > 1)
		{
			if(mo) 
				mo->Vel.X = floatvalue(t_argv[1]);
		}
		
		t_return.setDouble(mo ? mo->Vel.X : 0.);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MobjMomy(void)
{
	AActor*   mo;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if(t_argc > 1)
		{
			if(mo) 
				mo->Vel.Y = floatvalue(t_argv[1]);
		}
		
		t_return.setDouble(mo ? mo->Vel.Y : 0.);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MobjMomz(void)
{
	AActor*   mo;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if (t_argc > 1)
		{
			if (mo)
				mo->Vel.Z = floatvalue(t_argv[1]);
		}

		t_return.setDouble(mo ? mo->Vel.Z : 0.);
	}
}


//==========================================================================
//
//
//
//==========================================================================

/****************** Trig *********************/

void FParser::SF_PointToAngle(void)
{
	if (CheckArgs(4))
	{
		double x1 = floatvalue(t_argv[0]);
		double y1 = floatvalue(t_argv[1]);
		double x2 = floatvalue(t_argv[2]);
		double y2 = floatvalue(t_argv[3]);
		
		t_return.setDouble(DVector2(x2 - x1, y2 - y1).Angle().Normalized360().Degrees);
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_PointToDist(void)
{
	if (CheckArgs(4))
	{
		// Doing this in floating point is actually faster with modern computers!
		double x = floatvalue(t_argv[2]) - floatvalue(t_argv[0]);
		double y = floatvalue(t_argv[3]) - floatvalue(t_argv[1]);
		t_return.setDouble(g_sqrt(x*x+y*y));
	}
}


//==========================================================================
//
// setcamera(obj, [angle], [height], [pitch])
//
// [GrafZahl] This is a complete rewrite.
// Although both Eternity and Legacy implement this function
// they are mutually incompatible with each other and with ZDoom...
//
//==========================================================================

void FParser::SF_SetCamera(void)
{
	DAngle angle;
	player_t * player;
	AActor * newcamera;
	
	if (CheckArgs(1))
	{
		player = Script->trigger->player;
		if (!player) player = Level->Players[0];
		
		newcamera = actorvalue(t_argv[0]);
		if(!newcamera)
		{
			script_error("invalid location object for camera\n");
			return;         // nullptr check
		}
		
		angle = t_argc < 2 ? newcamera->Angles.Yaw : floatvalue(t_argv[1]);

		newcamera->specialf1 = newcamera->Angles.Yaw.Degrees;
		newcamera->specialf2 = newcamera->Z();
		double z = t_argc < 3 ? newcamera->Sector->floorplane.ZatPoint(newcamera) + 41 : floatvalue(t_argv[2]);
		newcamera->SetOrigin(newcamera->PosAtZ(z), false);
		newcamera->Angles.Yaw = angle;
		if (t_argc < 4) newcamera->Angles.Pitch = 0.;
		else newcamera->Angles.Pitch = clamp(floatvalue(t_argv[3]), -50., 50.) * (20. / 32.);
		player->camera=newcamera;
		newcamera->renderflags |= RF_NOINTERPOLATEVIEW;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ClearCamera(void)
{
	player_t * player;
	player = Script->trigger->player;
	if (!player) player = Level->Players[0];

	AActor * cam=player->camera;
	if (cam)
	{
		player->camera=player->mo;
		cam->Angles.Yaw = cam->specialf1;
		cam->SetZ(cam->specialf2);
	}

}



/*********** sounds ******************/

//==========================================================================
//
//
//
//==========================================================================

// start sound from thing
void FParser::SF_StartSound(void)
{
	AActor *mo;
	
	if (CheckArgs(2))
	{
		mo = actorvalue(t_argv[0]);
		
		if (mo)
		{
			S_Sound(mo, CHAN_BODY, 0, T_FindSound(stringvalue(t_argv[1])), 1, ATTN_NORM);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

// start sound from sector
void FParser::SF_StartSectorSound(void)
{
	sector_t *sector;
	int tagnum;
	
	if (CheckArgs(2))
	{
		tagnum = intvalue(t_argv[0]);
		
		int i=-1;
		auto itr = Level->GetSectorTagIterator(tagnum);
		while ((i = itr.Next()) >= 0)
		{
			sector = &Level->sectors[i];
			S_Sound(sector, CHAN_BODY, 0, T_FindSound(stringvalue(t_argv[1])), 1.0f, ATTN_NORM);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

// floor height of sector
void FParser::SF_FloorHeight(void)
{
	int tagnum;
	int secnum;
	double dest;
	double returnval = 1; // haleyjd: SoM's fixes
	
	if (CheckArgs(1))
	{
		tagnum = intvalue(t_argv[0]);
		
		if(t_argc > 1)          // > 1: set floor height
		{
			int i;
			int crush = (t_argc >= 3) ? intvalue(t_argv[2]) : false;
			
			i = -1;
			dest = floatvalue(t_argv[1]);
			
			// set all sectors with tag
			
			auto itr = Level->GetSectorTagIterator(tagnum);
			while ((i = itr.Next()) >= 0)
			{
				auto &sec = Level->sectors[i];
				if (sec.floordata) continue;	// don't move floors that are active!

				if (sec.MoveFloor(
					fabs(dest - sec.CenterFloor()),
					sec.floorplane.PointToDist (sec.centerspot, dest),
					crush? 10:-1, 
					(dest > sec.CenterFloor()) ? 1 : -1,
					false) == EMoveResult::crushed)
				{
					returnval = 0;
				}
			}
		}
		else
		{
			secnum = Level->FindFirstSectorFromTag(tagnum);
			if(secnum < 0)
			{ 
				script_error("sector not found with tagnum %i\n", tagnum); 
				return;
			}
			returnval = Level->sectors[secnum].CenterFloor();
		}
		
		// return floor height
		t_return.setDouble(returnval);
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MoveFloor(void)
{
	int secnum = -1;
	int tagnum, crush;
	double platspeed = 1, destheight;
	
	if (CheckArgs(2))
	{
		tagnum = intvalue(t_argv[0]);
		destheight = intvalue(t_argv[1]);
		platspeed = t_argc > 2 ? floatvalue(t_argv[2]) : 1.;
		crush = (t_argc > 3 ? intvalue(t_argv[3]) : -1);
		
		// move all sectors with tag
		
		auto itr = Level->GetSectorTagIterator(tagnum);
		while ((secnum = itr.Next()) >= 0)
		{
			Level->CreateFloor(&Level->sectors[secnum], DFloor::floorMoveToValue, NULL, platspeed, destheight, crush, 0, false, false);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

// ceiling height of sector
void FParser::SF_CeilingHeight(void)
{
	double dest;
	int secnum;
	int tagnum;
	double returnval = 1;
	
	if (CheckArgs(1))
	{
		tagnum = intvalue(t_argv[0]);
		
		if(t_argc > 1)          // > 1: set ceilheight
		{
			int i;
			int crush = (t_argc >= 3) ? intvalue(t_argv[2]) : false;
			
			i = -1;
			dest = floatvalue(t_argv[1]);
			
			// set all sectors with tag
			auto itr = Level->GetSectorTagIterator(tagnum);
			while ((i = itr.Next()) >= 0)
			{
				auto &sec = Level->sectors[i];
				if (sec.ceilingdata) continue;	// don't move ceilings that are active!

				if (sec.MoveCeiling(
					fabs(dest - sec.CenterCeiling()), 
					sec.ceilingplane.PointToDist (sec.centerspot, dest), 
					crush? 10:-1,
					(dest > sec.CenterCeiling()) ? 1 : -1,
					false) == EMoveResult::crushed)
				{
					returnval = 0;
				}
			}
		}
		else
		{
			secnum = Level->FindFirstSectorFromTag(tagnum);
			if(secnum < 0)
			{ 
				script_error("sector not found with tagnum %i\n", tagnum); 
				return;
			}
			returnval = Level->sectors[secnum].CenterCeiling();
		}
		
		// return ceiling height
		t_return.setDouble(returnval);
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MoveCeiling(void)
{
	int secnum = -1;
	int tagnum;
	double platspeed = 1, destheight;
	int crush;
	int silent;
	
	if (CheckArgs(2))
	{
		tagnum = intvalue(t_argv[0]);
		destheight = intvalue(t_argv[1]);
		platspeed = /*FLOORSPEED **/ (t_argc > 2 ? floatvalue(t_argv[2]) : 1);
		crush=t_argc>3 ? intvalue(t_argv[3]):-1;
		silent=t_argc>4 ? intvalue(t_argv[4]):1;
		
		// move all sectors with tag
		auto itr = Level->GetSectorTagIterator(tagnum);
		while ((secnum = itr.Next()) >= 0)
		{
			Level->CreateCeiling(&Level->sectors[secnum], DCeiling::ceilMoveToValue, NULL, tagnum, platspeed, platspeed, destheight, crush, silent | 4, 0, DCeiling::ECrushMode::crushDoom);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_LightLevel(void)
{
	sector_t *sector;
	int secnum;
	int tagnum;
	
	if (CheckArgs(1))
	{
		tagnum = intvalue(t_argv[0]);
		
		// argv is sector tag
		secnum = Level->FindFirstSectorFromTag(tagnum);
		
		if(secnum < 0)
		{ 
			return;
		}
		
		sector = &Level->sectors[secnum];
		
		if(t_argc > 1)          // > 1: set light level
		{
			int i = -1;
			
			// set all sectors with tag
			auto itr = Level->GetSectorTagIterator(tagnum);
			while ((i = itr.Next()) >= 0)
			{
				Level->sectors[i].SetLightLevel(intvalue(t_argv[1]));
			}
		}
		
		// return lightlevel
		t_return.type = svt_int;
		t_return.value.i = sector->lightlevel;
	}
}



//==========================================================================
//
// Simple light fade - locks lightingdata. For FParser::SF_FadeLight
//
//==========================================================================
class DLightLevel : public DLighting
{
	DECLARE_CLASS (DLightLevel, DLighting)

	unsigned char destlevel;
	unsigned char speed;

public:

	void Construct(sector_t * s,int destlevel,int speed);
	void	Serialize(FSerializer &arc);
	void		Tick ();
	void		OnDestroy() { Super::OnDestroy(); m_Sector->lightingdata = nullptr; }
};

IMPLEMENT_CLASS(DLightLevel, false, false)

void DLightLevel::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("destlevel", destlevel)
		("speed", speed);
}


//==========================================================================
// sf 13/10/99:
//
// T_LightFade()
//
// Just fade the light level in a sector to a new level
//
//==========================================================================

void DLightLevel::Tick()
{
	Super::Tick();
	if(m_Sector->lightlevel < destlevel)
	{
		// increase the lightlevel
		if(m_Sector->lightlevel + speed >= destlevel)
		{
			// stop changing light level
			m_Sector->SetLightLevel(destlevel);    // set to dest lightlevel
			Destroy();
		}
		else
		{
			m_Sector->ChangeLightLevel(speed);
		}
	}
	else
	{
		// decrease lightlevel
		if(m_Sector->lightlevel - speed <= destlevel)
		{
			// stop changing light level
			m_Sector->SetLightLevel(destlevel);    // set to dest lightlevel
			Destroy();
		}
		else
		{
			m_Sector->ChangeLightLevel(-speed);
		}
	}
}

//==========================================================================
//
//==========================================================================
void DLightLevel::Construct(sector_t * s,int _destlevel,int _speed)
{
	Super::Construct(s);
	destlevel=_destlevel;
	speed=_speed;
	s->lightingdata=this;
}

//==========================================================================
// sf 13/10/99:
//
// P_FadeLight()
//
// Fade all the lights in sectors with a particular tag to a new value
//
//==========================================================================
void FParser::SF_FadeLight(void)
{
	int sectag, destlevel, speed = 1;
	int i;
	
	if (CheckArgs(2))
	{
		sectag = intvalue(t_argv[0]);
		destlevel = intvalue(t_argv[1]);
		speed = t_argc>2 ? intvalue(t_argv[2]) : 1;
		
		auto it = Level->GetSectorTagIterator(sectag);
		while ((i = it.Next()) >= 0)
		{
			if (!Level->sectors[i].lightingdata) Level->CreateThinker<DLightLevel>(&Level->sectors[i],destlevel,speed);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================
void FParser::SF_FloorTexture(void)
{
	int tagnum, secnum;
	sector_t *sector;
	
	if (CheckArgs(1))
	{
		tagnum = intvalue(t_argv[0]);
		
		// argv is sector tag
		secnum = Level->FindFirstSectorFromTag(tagnum);
		
		if(secnum < 0)
		{ script_error("sector not found with tagnum %i\n", tagnum); return;}
		
		sector = &Level->sectors[secnum];
		
		if(t_argc > 1)
		{
			int i = -1;
			FTextureID picnum = TexMan.GetTextureID(t_argv[1].string, ETextureType::Flat, FTextureManager::TEXMAN_Overridable);
			
			// set all sectors with tag
			auto itr = Level->GetSectorTagIterator(tagnum);
			while ((i = itr.Next()) >= 0)
			{
				Level->sectors[i].SetTexture(sector_t::floor, picnum);
			}
		}
		
		t_return.type = svt_string;
		auto tex = TexMan.GetGameTexture(sector->GetTexture(sector_t::floor));
		t_return.string = tex? tex->GetName() : FString();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SectorColormap(void)
{
	// This doesn't work properly and it never will.
	// Whatever was done here originally, it is incompatible 
	// with Boom and ZDoom and doesn't work properly in Legacy either.
	
	// Making it no-op is probably the best thing one can do in this case.
	
	/*
	int tagnum, secnum;
	sector_t *sector;
	int c=2;
	int i = -1;

	if(t_argc<2)
	{ script_error("insufficient arguments to function\n"); return; }
	
	tagnum = intvalue(t_argv[0]);
	
	// argv is sector tag
	secnum = Level->FindFirstSectorFromTag(tagnum);
	
	if(secnum < 0)
	{ script_error("sector not found with tagnum %i\n", tagnum); return;}
	
	sector = &Level->sectors[secnum];

	if (t_argv[1].type==svt_string)
	{
		uint32_t cm = R_ColormapNumForName(t_argv[1].value.s);

	 	auto itr = Level->GetSectorTagIterator(tagnum);
		while ((i = itr.Next()) >= 0)
		{
			sectors[i].midmap=cm;
			sectors[i].heightsec=&Level->sectors[i];
		}
	}
	*/	
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_CeilingTexture(void)
{
	int tagnum, secnum;
	sector_t *sector;
	
	if (CheckArgs(1))
	{
		tagnum = intvalue(t_argv[0]);
		
		// argv is sector tag
		secnum = Level->FindFirstSectorFromTag(tagnum);
		
		if(secnum < 0)
		{ script_error("sector not found with tagnum %i\n", tagnum); return;}
		
		sector = &Level->sectors[secnum];
		
		if(t_argc > 1)
		{
			int i = -1;
			FTextureID picnum = TexMan.GetTextureID(t_argv[1].string, ETextureType::Flat, FTextureManager::TEXMAN_Overridable);
			
			// set all sectors with tag
			auto itr = Level->GetSectorTagIterator(tagnum);
			while ((i = itr.Next()) >= 0)
			{
				Level->sectors[i].SetTexture(sector_t::ceiling, picnum);
			}
		}
		
		t_return.type = svt_string;
		auto tex = TexMan.GetGameTexture(sector->GetTexture(sector_t::ceiling));
		t_return.string = tex? tex->GetName() : FString();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ChangeHubLevel(void)
{
	script_error("FS hub system permanently disabled\n");
}

// for start map: start new game on a particular skill
void FParser::SF_StartSkill(void)
{
	script_error("startskill is not supported by this implementation!\n");
}

//==========================================================================
//
// Doors
//
// opendoor(sectag, [delay], [speed])
//
//==========================================================================

void FParser::SF_OpenDoor(void)
{
	int speed, wait_time;
	int sectag;
	
	if (CheckArgs(1))
	{
		// got sector tag
		sectag = intvalue(t_argv[0]);
		if (sectag==0) return;	// tag 0 not allowed
		
		// door wait time
		if(t_argc > 1) wait_time = (intvalue(t_argv[1]) * TICRATE) / 100;
		else wait_time = 0;  // 0= stay open
		
		// door speed
		if(t_argc > 2) speed = intvalue(t_argv[2]);
		else speed = 1;    // 1= normal speed

		Level->EV_DoDoor(wait_time ? DDoor::doorRaise : DDoor::doorOpen, NULL, NULL, sectag, 2. * clamp(speed, 1, 127), wait_time, 0, 0);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_CloseDoor(void)
{
	int speed;
	int sectag;
	
	if (CheckArgs(1))
	{
		// got sector tag
		sectag = intvalue(t_argv[0]);
		if (sectag==0) return;	// tag 0 not allowed
		
		// door speed
		if(t_argc > 1) speed = intvalue(t_argv[1]);
		else speed = 1;    // 1= normal speed
		
		Level->EV_DoDoor(DDoor::doorClose, NULL, NULL, sectag, 2.*clamp(speed, 1, 127), 0, 0, 0);
	}
}

//==========================================================================
//
//
//
//==========================================================================

// run console cmd
void FParser::SF_RunCommand(void)
{
	FS_EmulateCmd(Level, GetFormatString(0).LockBuffer());
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_LineTrigger()
{
	if (CheckArgs(1))
	{
		line_t line;
		maplinedef_t mld;
		mld.special=intvalue(t_argv[0]);
		mld.tag=t_argc > 1 ? intvalue(t_argv[1]) : 0;
		Level->TranslateLineDef(&line, &mld);
		P_ExecuteSpecial(Level, line.special, NULL, Script->trigger, false, 
			line.args[0],line.args[1],line.args[2],line.args[3],line.args[4]); 
	}
}

//==========================================================================
//
//
//
//==========================================================================
bool FS_ChangeMusic(const char * string)
{
	char buffer[40];

	if (fileSystem.CheckNumForName(string, ns_music)<0 || !S_ChangeMusic(string,true))
	{
		// Retry with O_ prepended to the music name, then with D_
		mysnprintf(buffer, countof(buffer), "O_%s", string);
		if (fileSystem.CheckNumForName(buffer, ns_music)<0 || !S_ChangeMusic(buffer,true))
		{
			mysnprintf(buffer, countof(buffer), "D_%s", string);
			if (fileSystem.CheckNumForName(buffer, ns_music)<0) 
			{
				S_ChangeMusic(NULL, 0);
				return false;
			}
			else S_ChangeMusic(buffer,true);
		}
	}
	return true;
}

void FParser::SF_ChangeMusic(void)
{
	if (CheckArgs(1))
	{
		FS_ChangeMusic(stringvalue(t_argv[0]));
	}
}


//==========================================================================
//
// FParser::SF_SetLineBlocking()
//
//  Sets a line blocking or unblocking
//
//==========================================================================
void FParser::SF_SetLineBlocking(void)
{
	static unsigned short blocks[]={0,ML_BLOCKING,ML_BLOCKEVERYTHING};
	
	if (CheckArgs(2))
	{
		int blocking=intvalue(t_argv[1]);
		if (blocking>=0 && blocking<=2) 
		{
			blocking=blocks[blocking];
			int tag=intvalue(t_argv[0]);
			auto itr = Level->GetLineIdIterator(tag);
			int i;
			while ((i = itr.Next()) >= 0)
			{
				Level->lines[i].flags = (Level->lines[i].flags & ~(ML_BLOCKING | ML_BLOCKEVERYTHING)) | blocking;
			}
		}
	}
}

//==========================================================================
//
// similar, but monster blocking
//
//==========================================================================

void FParser::SF_SetLineMonsterBlocking(void)
{
	if (CheckArgs(2))
	{
		int blocking = intvalue(t_argv[1]) ? (int)ML_BLOCKMONSTERS : 0;
		int tag=intvalue(t_argv[0]);

		auto itr = Level->GetLineIdIterator(tag);
		int i;
		while ((i = itr.Next()) >= 0)
		{
			Level->lines[i].flags = (Level->lines[i].flags & ~ML_BLOCKMONSTERS) | blocking;
		}
	}
}



//==========================================================================
//
//FParser::SF_SetLineTexture
//
//  #2 in a not-so-long line of ACS-inspired functions
//  This one is *much* needed, IMO
//  
//	Eternity: setlinetexture(tag, side, position, texture)
//	Legacy:	  setlinetexture(tag, texture, side, sections)
//
//==========================================================================

void FParser::SF_SetLineTexture(void)
{
	int tag;
	int side;
	int position;
	const char *texture;
	FTextureID texturenum;
	int i;
	
	if (CheckArgs(4))
	{
		tag = intvalue(t_argv[0]);

		// the eternity version
		if (t_argv[3].type==svt_string)
		{
			side = intvalue(t_argv[1]);   
			if(side < 0 || side > 1)
			{
				script_error("invalid side number for texture change\n");
				return;
			}
			
			position = intvalue(t_argv[2]);
			if(position < 1 || position > 3)
			{
				script_error("invalid position for texture change\n");
				return;
			}
			position=3-position;
			
			texture = stringvalue(t_argv[3]);
			texturenum = TexMan.GetTextureID(texture, ETextureType::Wall, FTextureManager::TEXMAN_Overridable);
			
			auto itr = Level->GetLineIdIterator(tag);
			while ((i = itr.Next()) >= 0)
			{
				// bad sidedef, Hexen just SEGV'd here!
				if (Level->lines[i].sidedef[side] != NULL)
				{
					if (position >= 0 && position <= 2)
					{
						Level->lines[i].sidedef[side]->SetTexture(position, texturenum);
					}
				}
			}
		}
		else // and an improved legacy version
		{ 
			FTextureID picnum = TexMan.GetTextureID(t_argv[1].string, ETextureType::Wall, FTextureManager::TEXMAN_Overridable);
			side = !!intvalue(t_argv[2]); 
			int sections = intvalue(t_argv[3]); 
			
			// set all sectors with tag 
			auto itr = Level->GetLineIdIterator(tag);
			while ((i = itr.Next()) >= 0)
			{ 
				side_t *sided = Level->lines[i].sidedef[side];
				if(sided != NULL)
				{ 
					if(sections & 1) sided->SetTexture(side_t::top, picnum);
					if(sections & 2) sided->SetTexture(side_t::mid, picnum);
					if(sections & 4) sided->SetTexture(side_t::bottom, picnum);
				} 
			} 
		} 
	}
}


//==========================================================================
//
// SoM: Max, Min, Abs math functions.
//
//==========================================================================

void FParser::SF_Max(void)
{
	if (CheckArgs(2))
	{
		auto n1 = fixedvalue(t_argv[0]);
		auto n2 = fixedvalue(t_argv[1]);
		
		t_return.type = svt_fixed;
		t_return.value.f = (n1 > n2) ? n1 : n2;
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Min(void)
{
	if (CheckArgs(1))
	{
		auto n1 = fixedvalue(t_argv[0]);
		auto n2 = fixedvalue(t_argv[1]);
		
		t_return.type = svt_fixed;
		t_return.value.f = (n1 < n2) ? n1 : n2;
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Abs(void)
{
	
	if (CheckArgs(1))
	{
		auto n1 = fixedvalue(t_argv[0]);
		
		t_return.type = svt_fixed;
		t_return.value.f = (n1 < 0) ? -n1 : n1;
	}
}

//==========================================================================
// 
// FParser::SF_Gameskill, FParser::SF_Gamemode
//
//  Access functions are more elegant for these than variables, 
//  especially for the game mode, which doesn't exist as a numeric 
//  variable already.
//
//==========================================================================

void FParser::SF_Gameskill(void)
{
	t_return.type = svt_int;
	t_return.value.i = G_SkillProperty(SKILLP_ACSReturn) + 1;  // +1 for the user skill value
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Gamemode(void)
{
	t_return.type = svt_int;   
	if(!multiplayer)
	{
		t_return.value.i = 0; // single-player
	}
	else if(!deathmatch)
	{
		t_return.value.i = 1; // cooperative
	}
	else
		t_return.value.i = 2; // deathmatch
}

//==========================================================================
//
// FParser::SF_IsPlayerObj()
//
//  A function suggested by SoM to help the script coder prevent
//  exceptions related to calling player functions on non-player
//  objects.
//
//==========================================================================

void FParser::SF_IsPlayerObj(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_int;
	t_return.value.i = (mo && mo->player) ? 1 : 0;
}

//============================================================================
//
// Inventory stuff - mostly new to GZDoom
//
// all the original functions are still supported but they have not
// been expanded from their original and are limited as a result
//
// Since FraggleScript is rather hard coded to the original inventory
// handling of Doom this is quite messy.
//
//============================================================================


//============================================================================
//
// CheckInventory
//
// forward to the ACS equivalent.
//
//============================================================================

int CheckInventory(AActor *activator, const char *type, bool max = false);


//==========================================================================
//
// This function is just kept for backwards compatibility 
// and intentionally limited to thr standard keys!
// Use Give/Take/CheckInventory instead!
//
//==========================================================================

void FParser::SF_PlayerKeys(void)
{
	static const ENamedName DoomKeys[]={NAME_BlueCard, NAME_YellowCard, NAME_RedCard, NAME_BlueSkull, NAME_YellowSkull, NAME_RedSkull};
	int  playernum, keynum, givetake;
	FName keyname;
	
	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		if (playernum==-1) return;
		
		keynum = intvalue(t_argv[1]);
		if(keynum < 0 || keynum >= 6)
		{
			script_error("key number out of range: %i\n", keynum);
			return;
		}
		keyname=DoomKeys[keynum];
		
		if(t_argc == 2)
		{
			t_return.type = svt_int;
			t_return.value.i = CheckInventory(Level->Players[playernum]->mo, keyname.GetChars());
			return;
		}
		else
		{
			givetake = intvalue(t_argv[2]);
			ScriptUtil::Exec(givetake?NAME_GiveInventory : NAME_TakeInventory, ScriptUtil::Pointer, Level->Players[playernum]->mo, ScriptUtil::Int, keyname.GetIndex(), ScriptUtil::Int, 1, ScriptUtil::End);
			t_return.type = svt_int;
			t_return.value.i = 0;
		}
	}
}

//==========================================================================
//
// This function is just kept for backwards compatibility and intentionally limited!
// Use Give/Take/CheckInventory instead!
//
//==========================================================================

void FParser::SF_PlayerAmmo(void)
{
	if (CheckArgs(2))
	{
		t_return.type = svt_int;
		t_return.value.i = ScriptUtil::Exec("PlayerAmmo", ScriptUtil::Pointer, T_GetPlayerActor(t_argv[0]), ScriptUtil::Class, T_GetAmmo(t_argv[1]),
			ScriptUtil::Int, t_argc >= 3 ? intvalue(t_argv[2]) : INT_MIN, ScriptUtil::End);
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MaxPlayerAmmo()
{
	if (CheckArgs(2))
	{
		t_return.type = svt_int;
		t_return.value.i = ScriptUtil::Exec("MaxPlayerAmmo", ScriptUtil::Pointer, T_GetPlayerActor(t_argv[0]), ScriptUtil::Class, T_GetAmmo(t_argv[1]),
			ScriptUtil::Int, t_argc >= 3? intvalue(t_argv[2]) : INT_MIN, ScriptUtil::Int, t_argc >= 4 ? intvalue(t_argv[3]) : INT_MIN, ScriptUtil::End);
	}
}

//==========================================================================
//
// This function is just kept for backwards compatibility and 
// intentionally limited to the standard weapons!
// Use Give/Take/CheckInventory instead!
//
//==========================================================================

void FParser::SF_PlayerWeapon()
{
	static const char * const WeaponNames[]={
		"Fist", "Pistol", "Shotgun", "Chaingun", "RocketLauncher", 
		"PlasmaRifle", "BFG9000", "Chainsaw", "SuperShotgun" };


	int playernum;
	int weaponnum;
	int newweapon;
	
	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		weaponnum = intvalue(t_argv[1]);
		if (playernum==-1) return;
		if (weaponnum<0 || weaponnum>9)
		{
			script_error("weaponnum out of range! %d\n", weaponnum);
			return;
		}
		auto ti = PClass::FindActor(WeaponNames[weaponnum]);
		if (!ti || !ti->IsDescendantOf(NAME_Weapon))
		{
			script_error("incompatibility in playerweapon %d\n", weaponnum);
			return;
		}
		
		if (t_argc == 2)
		{
			AActor * wp = Level->Players[playernum]->mo->FindInventory(ti);
			t_return.type = svt_int;
			t_return.value.i = wp!=NULL;
			return;
		}
		else
		{
			AActor * wp = Level->Players[playernum]->mo->FindInventory(ti);

			newweapon = !!intvalue(t_argv[2]);
			if (!newweapon)
			{
				if (wp) 
				{
					wp->Destroy();
					// If the weapon is active pick a replacement. Legacy didn't do this!
					if (Level->Players[playernum]->PendingWeapon==wp) Level->Players[playernum]->PendingWeapon=(AActor*)WP_NOCHANGE;
					if (Level->Players[playernum]->ReadyWeapon==wp)
					{
						Level->Players[playernum]->ReadyWeapon=nullptr;
						
						IFVM(PlayerPawn, PickNewWeapon)
						{
							VMValue param[] = { Level->Players[playernum]->mo, (void*)nullptr };
							VMCall(func, param, 2, nullptr, 0);
						}
					}
				}
			}
			else 
			{
				if (!wp) 
				{
					auto pw=Level->Players[playernum]->PendingWeapon;
					Level->Players[playernum]->mo->GiveInventoryType(ti);
					Level->Players[playernum]->PendingWeapon=pw;
				}
			}
			
			t_return.type = svt_int;
			t_return.value.i = !!newweapon;
			return;
		}
	}
}

//==========================================================================
//
// This function is just kept for backwards compatibility and 
// intentionally limited to the standard weapons!
//
//==========================================================================

void FParser::SF_PlayerSelectedWeapon()
{
	int playernum;
	int weaponnum;


	static const char * const WeaponNames[]={
		"Fist", "Pistol", "Shotgun", "Chaingun", "RocketLauncher", 
		"PlasmaRifle", "BFG9000", "Chainsaw", "SuperShotgun" };


	if (CheckArgs(1))
	{
		playernum=T_GetPlayerNum(t_argv[0]);

		if(t_argc == 2)
		{
			weaponnum = intvalue(t_argv[1]);

			if (weaponnum<0 || weaponnum>=9)
			{
				script_error("weaponnum out of range! %d\n", weaponnum);
				return;
			}
			auto ti = PClass::FindActor(WeaponNames[weaponnum]);
			if (!ti || !ti->IsDescendantOf(NAME_Weapon))
			{
				script_error("incompatibility in playerweapon %d\n", weaponnum);
				return;
			}

			Level->Players[playernum]->PendingWeapon = Level->Players[playernum]->mo->FindInventory(ti);

		} 
		t_return.type = svt_int;
		for(int i=0;i<9;i++)
		{
			if (Level->Players[playernum]->ReadyWeapon->GetClass ()->TypeName == FName(WeaponNames[i]))
			{
				t_return.value.i=i;
				break;
			}
		}
	}
}

//==========================================================================
//
// new for GZDoom: named inventory handling
//
//==========================================================================

void FParser::SF_GiveInventory(void)
{
	int  playernum, count;
	
	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		if (playernum==-1) return;

		if(t_argc == 2) count=1;
		else count=intvalue(t_argv[2]);
		ScriptUtil::Exec(NAME_GiveInventory, ScriptUtil::Pointer, Level->Players[playernum]->mo, ScriptUtil::Int, FName(stringvalue(t_argv[1])).GetIndex(), ScriptUtil::Int, count, ScriptUtil::End);
		t_return.type = svt_int;
		t_return.value.i = 0;
	}
}

//==========================================================================
//
// new for GZDoom: named inventory handling
//
//==========================================================================

void FParser::SF_TakeInventory(void)
{
	int  playernum, count;

	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		if (playernum==-1) return;

		if(t_argc == 2) count=32767;
		else count=intvalue(t_argv[2]);
		ScriptUtil::Exec(NAME_TakeInventory, ScriptUtil::Pointer, Level->Players[playernum]->mo, ScriptUtil::Int, FName(stringvalue(t_argv[1])).GetIndex(), ScriptUtil::Int, count, ScriptUtil::End);
		t_return.type = svt_int;
		t_return.value.i = 0;
	}
}

//==========================================================================
//
// new for GZDoom: named inventory handling
//
//==========================================================================

void FParser::SF_CheckInventory(void)
{
	int  playernum;
	
	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		if (playernum==-1) 
		{
			t_return.value.i = 0;
			return;
		}
		t_return.type = svt_int;
		t_return.value.i = CheckInventory(Level->Players[playernum]->mo, stringvalue(t_argv[1]));
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SetWeapon()
{
	if (CheckArgs(2))
	{
		t_return.type = svt_int;
		t_return.value.i = ScriptUtil::Exec(NAME_SetWeapon, ScriptUtil::Pointer, T_GetPlayerActor(t_argv[0]), ScriptUtil::Class, T_ClassType(t_argv[1]), ScriptUtil::End);
	}
}

//==========================================================================
//
// movecamera(camera, targetobj, targetheight, movespeed, targetangle, anglespeed)
//
// This has been completely rewritten in a sane fashion, using actual vector math.
//
//==========================================================================

void FParser::SF_MoveCamera(void)
{
	if (CheckArgs(6))
	{
		AActor *cam = actorvalue(t_argv[0]);
		AActor *target = actorvalue(t_argv[1]);
		if(!cam || !target)
		{ 
			script_error("invalid target for camera\n"); return; 
		}

		double targetheight = floatvalue(t_argv[2]);
		double movespeed = floatvalue(t_argv[3]);
		DVector3 campos = cam->Pos();
		DVector3 targpos = DVector3(target->Pos(), targetheight);
		DVector3 movement = targpos - campos;
		double movelen = movement.Length();

		bool finishedmove = false;
		bool finishedangle = false;

		cam->radius = 1 / 8192.;
		cam->Height = 1 / 8192.;

		if (campos != targpos)
		{
			DVector3 movepos;
			finishedmove = (movespeed >= movelen);
			if (finishedmove) movepos = targpos;
			else movepos = campos + movement.Resized(movespeed);
			cam->SetOrigin(movepos, true);
		}
		else finishedmove = true;

		DAngle targetangle = DAngle(floatvalue(t_argv[4])).Normalized360();
		if (cam->Angles.Yaw != targetangle)
		{
			DAngle anglespeed = floatvalue(t_argv[5]);
			DAngle anglenow = targetangle;
			const DAngle diffangle = deltaangle(cam->Angles.Yaw, targetangle);

			if (movespeed > 0 && anglespeed == 0.)
			{
				if (!finishedmove)
				{
					anglenow = cam->Angles.Yaw + diffangle * movespeed / movelen;
				}
				else finishedangle = true;
			}
			else
			{
				if (diffangle > 0)
				{
					anglenow = (cam->Angles.Yaw + anglespeed).Normalized360();
				}
				else
				{
					anglenow = (cam->Angles.Yaw - anglespeed).Normalized360();
				}
				const DAngle diffangle2 = deltaangle(anglenow, targetangle);
				if (diffangle.Degrees * diffangle2.Degrees <= 0)
				{
					anglenow = targetangle;
					finishedangle = true;
				}
			}
			cam->SetAngle(anglenow, false);
		}
		else finishedangle = true;
		t_return.value.i = !(finishedmove & finishedangle);
		t_return.type = svt_int;
	}
}



//==========================================================================
//
// FParser::SF_ObjAwaken
//
// Implements: void objawaken([mobj mo])
//
//==========================================================================

void FParser::SF_ObjAwaken(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
   	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	if(mo)
	{
		mo->CallActivate(Script->trigger);
	}
}

//==========================================================================
//
// FParser::SF_AmbientSound
//
// Implements: void ambientsound(string name)
//
//==========================================================================

void FParser::SF_AmbientSound(void)
{
	if (CheckArgs(1))
	{
		S_Sound(CHAN_AUTO, 0, T_FindSound(stringvalue(t_argv[0])), 1, ATTN_NORM);
	}
}


//==========================================================================
// 
// FParser::SF_ExitSecret
//
// Implements: void exitsecret()
//
//==========================================================================

void FParser::SF_ExitSecret(void)
{
	Level->SecretExitLevel(0);
}


//==========================================================================
//
// Type forcing functions -- useful with arrays et al
//
//==========================================================================

void FParser::SF_MobjValue(void)
{
	if (CheckArgs(1))
	{
		t_return.type = svt_mobj;
		t_return.value.mobj = actorvalue(t_argv[0]);
	}
}

void FParser::SF_StringValue(void)
{  
	if (CheckArgs(1))
	{
		t_return.type = svt_string;
		if (t_argv[0].type == svt_string)
		{
			t_return.string = t_argv[0].string;
		}
		else
		{
			t_return.string = stringvalue(t_argv[0]);
		}
	}
}

void FParser::SF_IntValue(void)
{
	if (CheckArgs(1))
	{
		t_return.type = svt_int;
		t_return.value.i = intvalue(t_argv[0]);
	}
}

void FParser::SF_FixedValue(void)
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = fixedvalue(t_argv[0]);
	}
}


//==========================================================================
//
// Starting here are functions present in Legacy but not Eternity.
//
//==========================================================================

void FParser::SF_SpawnExplosion()
{
	DVector3 pos;
	AActor*   spawn;
	PClassActor * pclass;
	
	if (CheckArgs(3))
	{
		if (!(pclass=T_GetMobjType(t_argv[0]))) return;
		
		pos.X = floatvalue(t_argv[1]);
		pos.Y = floatvalue(t_argv[2]);
		if(t_argc > 3)
			pos.Z = floatvalue(t_argv[3]);
		else
			pos.Z = Level->PointInSector(pos)->floorplane.ZatPoint(pos);
		
		spawn = Spawn (Level, pclass, pos, ALLOW_REPLACE);
		t_return.type = svt_int;
		t_return.value.i=0;
		if (spawn)
		{
			spawn->ClearCounters();
			t_return.value.i = spawn->SetState(spawn->FindState(NAME_Death));
			if(spawn->DeathSound) S_Sound (spawn, CHAN_BODY, 0, spawn->DeathSound, 1, ATTN_NORM);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_RadiusAttack()
{
	AActor *spot;
	AActor *source;
	int damage;

	if (CheckArgs(3))
	{
		spot = actorvalue(t_argv[0]);
		source = actorvalue(t_argv[1]);
		damage = intvalue(t_argv[2]);

		if (spot && source)
		{
			P_RadiusAttack(spot, source, damage, damage, NAME_None, RADF_HURTSOURCE);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SetObjPosition()
{
	AActor* mobj;

	if (CheckArgs(2))
	{
		mobj = actorvalue(t_argv[0]);

		if (!mobj) return;

		mobj->SetOrigin(
			floatvalue(t_argv[1]),
			(t_argc >= 3)? floatvalue(t_argv[2]) : mobj->Y(),
			(t_argc >= 4)? floatvalue(t_argv[3]) : mobj->Z(), false);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_TestLocation()
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	if (mo)
	{
		t_return.type = svt_int;
		t_return.value.f = !!P_TestMobjLocation(mo);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_HealObj()  //no pain sound
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	if(t_argc < 2)
	{
		mo->health = mo->GetDefault()->health;
		if(mo->player) mo->player->health = mo->health;
	}

	else if (t_argc == 2)
	{
		mo->health += intvalue(t_argv[1]);
		if(mo->player) mo->player->health = mo->health;
	}

	else
		script_error("invalid number of arguments for objheal");
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjDead()
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_int;
	if(mo && (mo->health <= 0 || mo->flags&MF_CORPSE))
		t_return.value.i = 1;
	else
		t_return.value.i = 0;
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SpawnMissile()
{
	AActor *mobj;
	AActor *target;
	PClassActor * pclass;

	if (CheckArgs(3))
	{
		if (!(pclass=T_GetMobjType(t_argv[2]))) return;

		mobj = actorvalue(t_argv[0]);
		target = actorvalue(t_argv[1]);
		if (mobj && target) P_SpawnMissile(mobj, target, pclass);
	}
}

//==========================================================================
//
//checks to see if a Map Thing Number exists; used to avoid script errors
//
//==========================================================================

void FParser::SF_MapThingNumExist()
{
	auto &SpawnedThings = Level->FraggleScriptThinker->SpawnedThings;

	int intval;

	if (CheckArgs(1))
	{
		intval = intvalue(t_argv[0]);

		if (intval < 0 || intval >= int(SpawnedThings.Size()) || !SpawnedThings[intval])
		{
			t_return.type = svt_int;
			t_return.value.i = 0;
		}
		else
		{
			t_return.value.i = SpawnedThings[intval]->IsMapActor();
			t_return.type = svt_int;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MapThings()
{
	auto &SpawnedThings = Level->FraggleScriptThinker->SpawnedThings;

	t_return.type = svt_int;
	t_return.value.i = SpawnedThings.Size();
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjState()
{
	int state;
	AActor *mo = NULL;

	if (CheckArgs(1))
	{
		if(t_argc == 1)
		{
			mo = Script->trigger;
			state = intvalue(t_argv[0]);
		}

		else if(t_argc == 2)
		{
			mo = actorvalue(t_argv[0]);
			state = intvalue(t_argv[1]);
		}

		if (mo) 
		{
			static ENamedName statenames[]= {
				NAME_Spawn, NAME_See, NAME_Missile,	NAME_Melee,
				NAME_Pain, NAME_Death, NAME_Raise, NAME_XDeath, NAME_Crash };

			if (state <1 || state > 9)
			{
				script_error("objstate: invalid state");
				return;
			}

			t_return.type = svt_int;
			t_return.value.i = mo->SetState(mo->FindState(statenames[state]));
		}
	}
}


//==========================================================================
//
// only here for Legacy maps. The implementation of this function
// is completely useless.
//
//==========================================================================

void FParser::SF_LineFlag()
{
	line_t*  line;
	unsigned linenum;
	int      flagnum;
	
	if (CheckArgs(2))
	{
		linenum = intvalue(t_argv[0]);
		if(linenum >= Level->lines.Size())
		{
			script_error("LineFlag: Invalid line number.\n");
			return;
		}
		
		line = &Level->lines[linenum];
		
		flagnum = intvalue(t_argv[1]);
		if(flagnum < 0 || (flagnum > 8 && flagnum!=15))
		{
			script_error("LineFlag: Invalid flag number.\n");
			return;
		}
		
		if(t_argc > 2)
		{
			line->flags &= ~(1 << flagnum);
			if(intvalue(t_argv[2]))
				line->flags |= (1 << flagnum);
		}
		
		t_return.type = svt_int;
		t_return.value.i = line->flags & (1 << flagnum);
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_PlayerAddFrag()
{
	int playernum1;
	int playernum2;

	if (CheckArgs(1))
	{
		if (t_argc == 1)
		{
			playernum1 = T_GetPlayerNum(t_argv[0]);

			Level->Players[playernum1]->fragcount++;

			t_return.type = svt_int;
			t_return.value.f = Level->Players[playernum1]->fragcount;
		}

		else
		{
			playernum1 = T_GetPlayerNum(t_argv[0]);
			playernum2 = T_GetPlayerNum(t_argv[1]);

			Level->Players[playernum1]->frags[playernum2]++;

			t_return.type = svt_int;
			t_return.value.f = Level->Players[playernum1]->frags[playernum2];
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SkinColor()
{
	// Ignoring it for now.
}

void FParser::SF_PlayDemo()
{ 
	// Ignoring it for now.
}

void FParser::SF_CheckCVar()
{
	// can't be done so return 0.
}
//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Resurrect()
{

	AActor *mo;

	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);

		FState * state = mo->FindState(NAME_Raise);
		if (!state)  //Don't resurrect things that can't be resurrected
			return;

		mo->SetState(state);
		mo->Height = mo->GetDefault()->Height;
		mo->radius = mo->GetDefault()->radius;
		mo->Revive();
		mo->target = nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_LineAttack()
{
	AActor	*mo;
	int		damage;
	DAngle angle, slope;

	if (CheckArgs(3))
	{
		mo = actorvalue(t_argv[0]);
		damage = intvalue(t_argv[2]);

		angle = floatvalue(t_argv[1]);
		slope = P_AimLineAttack(mo, angle, MISSILERANGE);

		P_LineAttack(mo, angle, MISSILERANGE, slope, damage, NAME_Hitscan, NAME_BulletPuff);
	}
}

//==========================================================================
//
// This is a lousy hack. It only works for the standard actors
// and it is quite inefficient.
//
//==========================================================================

void FParser::SF_ObjType()
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	if (mo != NULL)
	{
		for (unsigned int i = 0; i < countof(ActorTypes); i++) if (mo->GetClass() == ActorTypes[i])
		{
			t_return.type = svt_int;
			t_return.value.i = i;
			return;
		}
	}
	t_return.type = svt_int;
	t_return.value.i = -1;
}

//==========================================================================
//
// some new math functions
//
//==========================================================================

void FParser::SF_Sin()
{
	if (CheckArgs(1))
	{
		t_return.setDouble(g_sin(floatvalue(t_argv[0])));
	}
}


void FParser::SF_ASin()
{
	if (CheckArgs(1))
	{
		t_return.setDouble(g_asin(floatvalue(t_argv[0])));
	}
}


void FParser::SF_Cos()
{
	if (CheckArgs(1))
	{
		t_return.setDouble(g_cos(floatvalue(t_argv[0])));
	}
}


void FParser::SF_ACos()
{
	if (CheckArgs(1))
	{
		t_return.setDouble(g_acos(floatvalue(t_argv[0])));
	}
}


void FParser::SF_Tan()
{
	if (CheckArgs(1))
	{
		t_return.setDouble(g_tan(floatvalue(t_argv[0])));
	}
}


void FParser::SF_ATan()
{
	if (CheckArgs(1))
	{
		t_return.setDouble(g_atan(floatvalue(t_argv[0])));
	}
}


void FParser::SF_Exp()
{
	if (CheckArgs(1))
	{
		t_return.setDouble(g_exp(floatvalue(t_argv[0])));
	}
}

void FParser::SF_Log()
{
	if (CheckArgs(1))
	{
		t_return.setDouble(g_log(floatvalue(t_argv[0])));
	}
}


void FParser::SF_Sqrt()
{
	if (CheckArgs(1))
	{
		t_return.setDouble(g_sqrt(floatvalue(t_argv[0])));
	}
}


void FParser::SF_Floor()
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = fixedvalue(t_argv[0]) & 0xffff0000;
	}
}


void FParser::SF_Pow()
{
	if (CheckArgs(2))
	{
		t_return.setDouble(g_pow(floatvalue(t_argv[0]), floatvalue(t_argv[1])));
	}
}

//==========================================================================
//
// HUD pics (not operational yet!)
//
//==========================================================================


void FParser::SF_NewHUPic()
{
	// disabled because it was never used and never tested
}

void FParser::SF_DeleteHUPic()
{
	// disabled because it was never used and never tested
}

void FParser::SF_ModifyHUPic()
{
	// disabled because it was never used and never tested
}

void FParser::SF_SetHUPicDisplay()
{
	// disabled because it was never used and never tested
}


//==========================================================================
//
// Yet to be made operational.
//
//==========================================================================

void FParser::SF_SetCorona(void)
{
	if(t_argc != 3)
	{
		script_error("incorrect arguments to function\n");
		return;
	}
	
	int num = intvalue(t_argv[0]);    // which corona we want to modify
	int what = intvalue(t_argv[1]);   // what we want to modify (type, color, offset,...)
	double val = floatvalue(t_argv[2]);   // the value of what we modify

  	/*
	switch (what)
	{
		case 0:
			lspr[num].type = ival;
			break;
		case 1:
			lspr[num].light_xoffset = fval;
			break;
		case 2:
			lspr[num].light_yoffset = fval;
			break;
		case 3:
			if (t_argv[2].type == svt_string)
				lspr[num].corona_color = String2Hex(t_argv[2].value.s);
			else
				memcpy(&lspr[num].corona_color, &ival, sizeof(int));
			break;
		case 4:
			lspr[num].corona_radius = fval;
			break;
		case 5:
			if (t_argv[2].type == svt_string)
				lspr[num].dynamic_color = String2Hex(t_argv[2].value.s);
			else
				memcpy(&lspr[num].dynamic_color, &ival, sizeof(int));
			break;
		case 6:
			lspr[num].dynamic_radius = fval;
			lspr[num].dynamic_sqrradius = g_sqrt(lspr[num].dynamic_radius);
			break;
		default:
			CONS_Printf("Error in setcorona\n");
			break;
	}
	*/

	// no use for this!
	t_return.type = svt_int;
	t_return.value.i = 0;
}

//==========================================================================
//
// new for GZDoom: Gets the levelnum
//
//==========================================================================

void FParser::SF_LevelNum()
{
	t_return.type = svt_int;
	t_return.value.f = Level->levelnum;
}


//==========================================================================
//
// new for GZDoom
//
//==========================================================================

void FParser::SF_MobjRadius(void)
{
	AActor*   mo;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if(t_argc > 1)
		{
			if(mo) 
				mo->radius = floatvalue(t_argv[1]);
		}
		t_return.setDouble(mo ? mo->radius : 0.);
	}
}


//==========================================================================
//
// new for GZDoom
//
//==========================================================================

void FParser::SF_MobjHeight(void)
{
	AActor*   mo;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if(t_argc > 1)
		{
			if(mo) 
				mo->Height = floatvalue(t_argv[1]);
		}
		t_return.setDouble(mo ? mo->Height : 0.);
	}
}


//==========================================================================
//
// new for GZDoom
//
//==========================================================================

void FParser::SF_ThingCount(void)
{
	PClassActor *pClass;
	AActor * mo;
	int count=0;
	bool replacemented = false;

	
	if (CheckArgs(1))
	{
		pClass=T_GetMobjType(t_argv[0]);
		if (!pClass) return;
		// If we want to count map items we must consider actor replacement
		pClass = pClass->GetReplacement(Level);
		
	again:
		auto it = Level->GetThinkerIterator<AActor>();

		if (t_argc<2 || intvalue(t_argv[1])==0 || pClass->IsDescendantOf(NAME_Inventory))
		{
			while ((mo=it.Next()))
			{
				if (mo->IsA(pClass))
				{
					if (mo->IsMapActor())
					{
						count++;
					}
				}
			}
		}
		else
		{
			while ((mo=it.Next()))
			{
				if (mo->IsA(pClass) && mo->health>0) count++;
			}
		}
		if (!replacemented)
		{
			// Again, with decorate replacements
			replacemented = true;
			PClassActor *newkind = pClass->GetReplacement(Level);
			if (newkind != pClass)
			{
				pClass = newkind;
				goto again;
			}
		}
		t_return.type = svt_int;
		t_return.value.i = count;
	}	
}

//==========================================================================
//
// new for GZDoom: Sets a sector color
//
//==========================================================================

void FParser::SF_SetColor(void)
{
	int tagnum, secnum;
	int c=2;
	int i = -1;
	PalEntry color=0;
	
	if (CheckArgs(2))
	{
		tagnum = intvalue(t_argv[0]);
		
		secnum = Level->FindFirstSectorFromTag(tagnum);
		
		if(secnum < 0)
		{ 
			return;
		}
		
		if(t_argc >1 && t_argc<4)
		{
			color=intvalue(t_argv[1]);
		}
		else if (t_argc>=4)
		{
			color.r=intvalue(t_argv[1]);
			color.g=intvalue(t_argv[2]);
			color.b=intvalue(t_argv[3]);
			color.a = 0;
		}
		else return;

		// set all sectors with tag
		auto itr = Level->GetSectorTagIterator(tagnum);
		while ((i = itr.Next()) >= 0)
		{
			Level->sectors[i].SetColor(color, 0);
		}
	}
}


//==========================================================================
//
// Spawns a projectile at a map spot
//
//==========================================================================

void FParser::SF_SpawnShot2(void)
{
	AActor *source = NULL;
	PClassActor * pclass;
	double z = 0;

	// t_argv[0] = type to spawn
	// t_argv[1] = source mobj, optional, -1 to default
	// shoots at source's angle

	if (CheckArgs(2))
	{
		if (t_argv[1].type == svt_int && t_argv[1].value.i < 0)
			source = Script->trigger;
		else
			source = actorvalue(t_argv[1]);

		if (t_argc > 2) z = floatvalue(t_argv[2]);

		if (!source)	return;

		if (!(pclass = T_GetMobjType(t_argv[0]))) return;

		t_return.type = svt_mobj;

		AActor *mo = Spawn(Level, pclass, source->PosPlusZ(z), ALLOW_REPLACE);
		if (mo)
		{
			S_Sound(mo, CHAN_VOICE, 0, mo->SeeSound, 1, ATTN_NORM);
			mo->target = source;
			mo->Angles.Yaw = source->Angles.Yaw;
			mo->Thrust();
			if (!P_CheckMissileSpawn(mo, source->radius)) mo = NULL;
		}
		t_return.value.mobj = mo;
	}
}



//==========================================================================
//
// new for GZDoom
//
//==========================================================================

void  FParser::SF_KillInSector()
{
	if (CheckArgs(1))
	{
		auto it = Level->GetThinkerIterator<AActor>();
		AActor * mo;
		int tag=intvalue(t_argv[0]);

		while ((mo=it.Next()))
		{
			if (mo->flags3&MF3_ISMONSTER && Level->SectorHasTag(mo->Sector, tag)) P_DamageMobj(mo, NULL, NULL, 1000000, NAME_Massacre);
		}
	}
}

//==========================================================================
//
// new for GZDoom: Sets a new line trigger type (Doom format!)
// (Sure, this is not particularly useful. But having it made it possible
//  to fix a few annoying bugs in some old maps ;) )
//
//==========================================================================

void FParser::SF_SetLineTrigger()
{
	int i,id,spec,tag(0);

	if (CheckArgs(2))
	{
		id=intvalue(t_argv[0]);
		spec=intvalue(t_argv[1]);
		if (t_argc>2) tag=intvalue(t_argv[2]);
		auto itr = Level->GetLineIdIterator(id);
		while ((i = itr.Next()) >= 0)
		{
			maplinedef_t mld;
			mld.special = spec;
			mld.tag = tag;
			mld.flags = 0;
			int f = Level->lines[i].flags;
			Level->TranslateLineDef(&Level->lines[i], &mld);
			Level->lines[i].flags = (Level->lines[i].flags & (ML_MONSTERSCANACTIVATE | ML_REPEAT_SPECIAL | ML_SPAC_MASK | ML_FIRSTSIDEONLY)) |
				(f & ~(ML_MONSTERSCANACTIVATE | ML_REPEAT_SPECIAL | ML_SPAC_MASK | ML_FIRSTSIDEONLY));

		}
	}
}


//==========================================================================
//
// new for GZDoom: Call a Hexen line special
//
//==========================================================================

void FParser::RunLineSpecial(const FLineSpecial *spec)
{

	if (CheckArgs(spec->min_args))
	{
		int args[5];
		for(int i=0;i<5;i++)
		{
			if (t_argc>i) args[i]=intvalue(t_argv[i]);
			else args[i] = 0;
		}
		t_return.value.i = P_ExecuteSpecial(Level, spec->number, NULL,Script->trigger,false, args[0],args[1],args[2],args[3],args[4]);
	}
}


//==========================================================================
//
//
//
//==========================================================================

DRunningScript *FParser::SaveCurrentScript()
{
	DFraggleThinker *th = Level->FraggleScriptThinker;
	if (th)
	{
		DRunningScript *runscr = Create<DRunningScript>(Script->trigger, Script, Script->MakeIndex(Rover));

		// hook into chain at start
		th->AddRunningScript(runscr);
		return runscr;
	}
	return NULL;
}

//==========================================================================
//
// script function
//
//==========================================================================

void FParser::SF_Wait()
{
	DRunningScript *runscr;
	
	if(t_argc != 1)
	{
		script_error("incorrect arguments to function\n");
		return;
	}
	
	runscr = SaveCurrentScript();
	
	runscr->wait_type = wt_delay;
	
	runscr->wait_data = (intvalue(t_argv[0]) * TICRATE) / 100;
	throw CFsTerminator();
}

//==========================================================================
//
// wait for sector with particular tag to stop moving
//
//==========================================================================

void FParser::SF_TagWait()
{
	DRunningScript *runscr;
	
	if(t_argc != 1)
	{
		script_error("incorrect arguments to function\n");
		return;
	}
	
	runscr = SaveCurrentScript();
	
	runscr->wait_type = wt_tagwait;
	runscr->wait_data = intvalue(t_argv[0]);
	throw CFsTerminator();
}

//==========================================================================
//
// wait for a script to finish
//
//==========================================================================

void FParser::SF_ScriptWait()
{
	DRunningScript *runscr;
	
	if(t_argc != 1)
	{
		script_error("insufficient arguments to function\n");
		return;
	}
	
	runscr = SaveCurrentScript();
	
	runscr->wait_type = wt_scriptwait;
	runscr->wait_data = intvalue(t_argv[0]);
	throw CFsTerminator();
}

//==========================================================================
//
// haleyjd 05/23/01: wait for a script to start (zdoom-inspired)
//
//==========================================================================

void FParser::SF_ScriptWaitPre()
{
	DRunningScript *runscr;
	
	if(t_argc != 1)
	{
		script_error("insufficient arguments to function\n");
		return;
	}
	
	runscr = SaveCurrentScript();
	runscr->wait_type = wt_scriptwaitpre;
	runscr->wait_data = intvalue(t_argv[0]);
	throw CFsTerminator();
}

//==========================================================================
//
// start a new script
//
//==========================================================================

void FParser::SF_StartScript()
{
	if(t_argc != 1)
	{
		script_error("incorrect arguments to function\n");
		return;
	}
	
	int snum = intvalue(t_argv[0]);

	if(snum < 0 || snum >= MAXSCRIPTS)
	{
		script_error("script number %d out of range\n",snum);
		return;
	}

	DFraggleThinker *th = Level->FraggleScriptThinker;
	if (th)
	{

		DFsScript *script = th->LevelScript->children[snum];
	
		if(!script)
		{
			script_error("script %i not defined\n", snum);
		}
		
		DRunningScript *runscr = Create<DRunningScript>(Script->trigger, script, 0);
		// hook into chain at start
		th->AddRunningScript(runscr);
	}
}

//==========================================================================
//
// checks if a script is running
//
//==========================================================================

void FParser::SF_ScriptRunning()
{
	DRunningScript *current;
	int snum = 0;
	
	if(t_argc < 1)
	{
		script_error("not enough arguments to function\n");
		return;
	}
	
	snum = intvalue(t_argv[0]);  
	
	for(current = Level->FraggleScriptThinker->RunningScripts->next; current; current=current->next)
	{
		if(current->script->scriptnum == snum)
		{
			// script found so return
			t_return.type = svt_int;
			t_return.value.i = 1;
			return;
		}
	}
	
	// script not found
	t_return.type = svt_int;
	t_return.value.i = 0;
}


//==========================================================================
//
// Init Functions
//
//==========================================================================

void DFraggleThinker::InitFunctions()
{
	for(unsigned i=0;i<countof(ActorNames_init);i++)
	{
		ActorTypes[i]=PClass::FindActor(ActorNames_init[i]);
	}

	DFsScript * gscr = GlobalScript;

	// add all the functions
	gscr->NewVariable("consoleplayer", svt_pInt)->value.pI = &consoleplayer;
	gscr->NewVariable("displayplayer", svt_pInt)->value.pI = &consoleplayer;
	gscr->NewVariable("zoom", svt_pInt)->value.pI = &zoom;
	gscr->NewVariable("fov", svt_pInt)->value.pI = &zoom;
	gscr->NewVariable("trigger", svt_pMobj)->value.pMobj = &trigger_obj;

	// Create constants for all existing line specials
	int max = P_GetMaxLineSpecial();
	for(int i=0; i<=max; i++)
	{
		const FLineSpecial *ls = P_GetLineSpecialInfo(i);

		if (ls != NULL && ls->max_args >= 0)	// specials with max args set to -1 can only be used in a map and are of no use hee.
		{
			gscr->NewVariable(ls->name, svt_linespec)->value.ls = ls;
		}
	}
	
	// important C-emulating stuff
	gscr->NewFunction("break", &FParser::SF_Break);
	gscr->NewFunction("continue", &FParser::SF_Continue);
	gscr->NewFunction("return", &FParser::SF_Return);
	gscr->NewFunction("goto", &FParser::SF_Goto);
	gscr->NewFunction("include", &FParser::SF_Include);
	
	// standard FraggleScript functions
	gscr->NewFunction("print", &FParser::SF_Print);
	gscr->NewFunction("rnd", &FParser::SF_Rnd);	// Legacy uses a normal rand() call for this which is extremely dangerous.
	gscr->NewFunction("prnd", &FParser::SF_Rnd);	// I am mapping rnd and prnd to the same named RNG which should eliminate any problem
	gscr->NewFunction("input", &FParser::SF_Input);
	gscr->NewFunction("beep", &FParser::SF_Beep);
	gscr->NewFunction("clock", &FParser::SF_Clock);
	gscr->NewFunction("wait", &FParser::SF_Wait);
	gscr->NewFunction("tagwait", &FParser::SF_TagWait);
	gscr->NewFunction("scriptwait", &FParser::SF_ScriptWait);
	gscr->NewFunction("startscript", &FParser::SF_StartScript);
	gscr->NewFunction("scriptrunning", &FParser::SF_ScriptRunning);
	
	// doom stuff
	gscr->NewFunction("startskill", &FParser::SF_StartSkill);
	gscr->NewFunction("exitlevel", &FParser::SF_ExitLevel);
	gscr->NewFunction("tip", &FParser::SF_Tip);
	gscr->NewFunction("timedtip", &FParser::SF_TimedTip);
	gscr->NewFunction("message", &FParser::SF_Message);
	gscr->NewFunction("gameskill", &FParser::SF_Gameskill);
	gscr->NewFunction("gamemode", &FParser::SF_Gamemode);
	
	// player stuff
	gscr->NewFunction("playermsg", &FParser::SF_PlayerMsg);
	gscr->NewFunction("playertip", &FParser::SF_PlayerTip);
	gscr->NewFunction("playeringame", &FParser::SF_PlayerInGame);
	gscr->NewFunction("playername", &FParser::SF_PlayerName);
	gscr->NewFunction("playeraddfrag", &FParser::SF_PlayerAddFrag);
	gscr->NewFunction("playerobj", &FParser::SF_PlayerObj);
	gscr->NewFunction("isplayerobj", &FParser::SF_IsPlayerObj);
	gscr->NewFunction("isobjplayer", &FParser::SF_IsPlayerObj);
	gscr->NewFunction("skincolor", &FParser::SF_SkinColor);
	gscr->NewFunction("playerkeys", &FParser::SF_PlayerKeys);
	gscr->NewFunction("playerammo", &FParser::SF_PlayerAmmo);
	gscr->NewFunction("maxplayerammo", &FParser::SF_MaxPlayerAmmo); 
	gscr->NewFunction("playerweapon", &FParser::SF_PlayerWeapon);
	gscr->NewFunction("playerselwep", &FParser::SF_PlayerSelectedWeapon);
	
	// mobj stuff
	gscr->NewFunction("spawn", &FParser::SF_Spawn);
	gscr->NewFunction("spawnexplosion", &FParser::SF_SpawnExplosion);
	gscr->NewFunction("radiusattack", &FParser::SF_RadiusAttack);
	gscr->NewFunction("kill", &FParser::SF_KillObj);
	gscr->NewFunction("removeobj", &FParser::SF_RemoveObj);
	gscr->NewFunction("objx", &FParser::SF_ObjX);
	gscr->NewFunction("objy", &FParser::SF_ObjY);
	gscr->NewFunction("objz", &FParser::SF_ObjZ);
	gscr->NewFunction("testlocation", &FParser::SF_TestLocation);
	gscr->NewFunction("teleport", &FParser::SF_Teleport);
	gscr->NewFunction("silentteleport", &FParser::SF_SilentTeleport);
	gscr->NewFunction("damageobj", &FParser::SF_DamageObj);
	gscr->NewFunction("healobj", &FParser::SF_HealObj);
	gscr->NewFunction("player", &FParser::SF_Player);
	gscr->NewFunction("objsector", &FParser::SF_ObjSector);
	gscr->NewFunction("objflag", &FParser::SF_ObjFlag);
	gscr->NewFunction("pushobj", &FParser::SF_PushThing);
	gscr->NewFunction("pushthing", &FParser::SF_PushThing);
	gscr->NewFunction("objangle", &FParser::SF_ObjAngle);
	gscr->NewFunction("objhealth", &FParser::SF_ObjHealth);
	gscr->NewFunction("objdead", &FParser::SF_ObjDead);
	gscr->NewFunction("reactiontime", &FParser::SF_ReactionTime);
	gscr->NewFunction("objreactiontime", &FParser::SF_ReactionTime);
	gscr->NewFunction("objtarget", &FParser::SF_MobjTarget);
	gscr->NewFunction("objmomx", &FParser::SF_MobjMomx);
	gscr->NewFunction("objmomy", &FParser::SF_MobjMomy);
	gscr->NewFunction("objmomz", &FParser::SF_MobjMomz);

	gscr->NewFunction("spawnmissile", &FParser::SF_SpawnMissile);
	gscr->NewFunction("mapthings", &FParser::SF_MapThings);
	gscr->NewFunction("objtype", &FParser::SF_ObjType);
	gscr->NewFunction("mapthingnumexist", &FParser::SF_MapThingNumExist);
	gscr->NewFunction("objstate", &FParser::SF_ObjState);
	gscr->NewFunction("resurrect", &FParser::SF_Resurrect);
	gscr->NewFunction("lineattack", &FParser::SF_LineAttack);
	gscr->NewFunction("setobjposition", &FParser::SF_SetObjPosition);

	// sector stuff
	gscr->NewFunction("floorheight", &FParser::SF_FloorHeight);
	gscr->NewFunction("floortext", &FParser::SF_FloorTexture);
	gscr->NewFunction("floortexture", &FParser::SF_FloorTexture);   // haleyjd: alias
	gscr->NewFunction("movefloor", &FParser::SF_MoveFloor);
	gscr->NewFunction("ceilheight", &FParser::SF_CeilingHeight);
	gscr->NewFunction("ceilingheight", &FParser::SF_CeilingHeight); // haleyjd: alias
	gscr->NewFunction("moveceil", &FParser::SF_MoveCeiling);
	gscr->NewFunction("moveceiling", &FParser::SF_MoveCeiling);     // haleyjd: aliases
	gscr->NewFunction("ceilingtexture", &FParser::SF_CeilingTexture);
	gscr->NewFunction("ceiltext", &FParser::SF_CeilingTexture);  // haleyjd: wrong
	gscr->NewFunction("lightlevel", &FParser::SF_LightLevel);    // handler - was
	gscr->NewFunction("fadelight", &FParser::SF_FadeLight);      // &FParser::SF_FloorTexture!
	gscr->NewFunction("colormap", &FParser::SF_SectorColormap);
	
	// cameras!
	gscr->NewFunction("setcamera", &FParser::SF_SetCamera);
	gscr->NewFunction("clearcamera", &FParser::SF_ClearCamera);
	gscr->NewFunction("movecamera", &FParser::SF_MoveCamera);
	
	// trig functions
	gscr->NewFunction("pointtoangle", &FParser::SF_PointToAngle);
	gscr->NewFunction("pointtodist", &FParser::SF_PointToDist);
	
	// sound functions
	gscr->NewFunction("startsound", &FParser::SF_StartSound);
	gscr->NewFunction("startsectorsound", &FParser::SF_StartSectorSound);
	gscr->NewFunction("ambientsound", &FParser::SF_AmbientSound);
	gscr->NewFunction("startambiantsound", &FParser::SF_AmbientSound);	// Legacy's incorrectly spelled name!
	gscr->NewFunction("changemusic", &FParser::SF_ChangeMusic);
	
	// hubs!
	gscr->NewFunction("changehublevel", &FParser::SF_ChangeHubLevel);
	
	// doors
	gscr->NewFunction("opendoor", &FParser::SF_OpenDoor);
	gscr->NewFunction("closedoor", &FParser::SF_CloseDoor);

	// HU Graphics
	gscr->NewFunction("newhupic", &FParser::SF_NewHUPic);
	gscr->NewFunction("createpic", &FParser::SF_NewHUPic);
	gscr->NewFunction("deletehupic", &FParser::SF_DeleteHUPic);
	gscr->NewFunction("modifyhupic", &FParser::SF_ModifyHUPic);
	gscr->NewFunction("modifypic", &FParser::SF_ModifyHUPic);
	gscr->NewFunction("sethupicdisplay", &FParser::SF_SetHUPicDisplay);
	gscr->NewFunction("setpicvisible", &FParser::SF_SetHUPicDisplay);

	//
	gscr->NewFunction("playdemo", &FParser::SF_PlayDemo);
	gscr->NewFunction("runcommand", &FParser::SF_RunCommand);
	gscr->NewFunction("checkcvar", &FParser::SF_CheckCVar);
	gscr->NewFunction("setlinetexture", &FParser::SF_SetLineTexture);
	gscr->NewFunction("linetrigger", &FParser::SF_LineTrigger);
	gscr->NewFunction("lineflag", &FParser::SF_LineFlag);

	//Hurdler: new math functions
	gscr->NewFunction("max", &FParser::SF_Max);
	gscr->NewFunction("min", &FParser::SF_Min);
	gscr->NewFunction("abs", &FParser::SF_Abs);

	gscr->NewFunction("sin", &FParser::SF_Sin);
	gscr->NewFunction("asin", &FParser::SF_ASin);
	gscr->NewFunction("cos", &FParser::SF_Cos);
	gscr->NewFunction("acos", &FParser::SF_ACos);
	gscr->NewFunction("tan", &FParser::SF_Tan);
	gscr->NewFunction("atan", &FParser::SF_ATan);
	gscr->NewFunction("exp", &FParser::SF_Exp);
	gscr->NewFunction("log", &FParser::SF_Log);
	gscr->NewFunction("sqrt", &FParser::SF_Sqrt);
	gscr->NewFunction("floor", &FParser::SF_Floor);
	gscr->NewFunction("pow", &FParser::SF_Pow);
	
	// Eternity extensions
	gscr->NewFunction("setlineblocking", &FParser::SF_SetLineBlocking);
	gscr->NewFunction("setlinetrigger", &FParser::SF_SetLineTrigger);
	gscr->NewFunction("setlinemnblock", &FParser::SF_SetLineMonsterBlocking);
	gscr->NewFunction("scriptwaitpre", &FParser::SF_ScriptWaitPre);
	gscr->NewFunction("exitsecret", &FParser::SF_ExitSecret);
	gscr->NewFunction("objawaken", &FParser::SF_ObjAwaken);
	
	// forced coercion functions
	gscr->NewFunction("mobjvalue", &FParser::SF_MobjValue);
	gscr->NewFunction("stringvalue", &FParser::SF_StringValue);
	gscr->NewFunction("intvalue", &FParser::SF_IntValue);
	gscr->NewFunction("fixedvalue", &FParser::SF_FixedValue);

	// new for GZDoom
	gscr->NewFunction("spawnshot2", &FParser::SF_SpawnShot2);
	gscr->NewFunction("setcolor", &FParser::SF_SetColor);
	gscr->NewFunction("objradius", &FParser::SF_MobjRadius);
	gscr->NewFunction("objheight", &FParser::SF_MobjHeight);
	gscr->NewFunction("thingcount", &FParser::SF_ThingCount);
	gscr->NewFunction("killinsector", &FParser::SF_KillInSector);
	gscr->NewFunction("levelnum", &FParser::SF_LevelNum);

	// new inventory
	gscr->NewFunction("giveinventory", &FParser::SF_GiveInventory);
	gscr->NewFunction("takeinventory", &FParser::SF_TakeInventory);
	gscr->NewFunction("checkinventory", &FParser::SF_CheckInventory);
	gscr->NewFunction("setweapon", &FParser::SF_SetWeapon);

	// Dummies - shut up warnings
	gscr->NewFunction("setcorona", &FParser::SF_SetCorona);
}


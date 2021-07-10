/*
** d_dehacked.cpp
** Parses dehacked/bex patches and changes game structures accordingly
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
** Much of this file is fudging code to compensate for the fact that most of
** what could be changed with Dehacked is no longer in the same state it was
** in as of Doom 1.9. There is a lump in zdoom.wad (DEHSUPP) that stores most
** of the lookup tables used so that they need not be loaded all the time.
** Also, their total size is less in the lump than when they were part of the
** executable, so it saves space on disk as well as in memory.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "doomtype.h"
#include "templates.h"
#include "doomstat.h"
#include "info.h"
#include "d_dehacked.h"
#include "g_level.h"
#include "cmdlib.h"
#include "gstrings.h"
#include "filesystem.h"
#include "d_player.h"
#include "r_state.h"
#include "c_dispatch.h"
#include "decallib.h"
#include "a_sharedglobal.h"
#include "engineerrors.h"
#include "p_effect.h"
#include "serializer.h"
#include "thingdef.h"
#include "v_text.h"
#include "vmbuilder.h"
#include "types.h"
#include "m_argv.h"
#include "actorptrselect.h"
#include "g_levellocals.h"

void JitDumpLog(FILE *file, VMScriptFunction *func);

// [SO] Just the way Randy said to do it :)
// [RH] Made this CVAR_SERVERINFO
CVAR (Int, infighting, 0, CVAR_SERVERINFO)

static bool LoadDehSupp ();
static void UnloadDehSupp ();


// This is a list of all the action functions used by each of Doom's states.
static TArray<PFunction *> Actions;

// These are the original heights of every Doom 2 thing. They are used if a patch
// specifies that a thing should be hanging from the ceiling but doesn't specify
// a height for the thing, since these are the heights it probably wants.
static TArray<int> OrgHeights;

// DeHackEd made the erroneous assumption that if a state didn't appear in
// Doom with an action function, then it was incorrect to assign it one.
// This is a list of the states that had action functions, so we can figure
// out where in the original list of states a DeHackEd codepointer is.
// (DeHackEd might also have done this for compatibility between Doom
// versions, because states could move around, but actions would never
// disappear, but that doesn't explain why frame patches specify an exact
// state rather than a code pointer.)
static TArray<int> CodePConv;

// Sprite names in the order Doom originally had them.
struct DEHSprName
{
	char c[5];
};
static TArray<DEHSprName> OrgSprNames;

struct StateMapper
{
	FState *State;
	int StateSpan;
	PClassActor *Owner;
	bool OwnerIsPickup;
};

static TArray<StateMapper> StateMap;

// Sound equivalences. When a patch tries to change a sound,
// use these sound names.
static TArray<FSoundID> SoundMap;

// Names of different actor types, in original Doom 2 order
static TArray<PClassActor *> InfoNames;

// bit flags for PatchThing (a .bex extension):
struct BitName
{
	char Name[20];
	uint8_t Bit;
	uint8_t WhichFlags;
};

static TArray<BitName> BitNames;

// Render styles
struct StyleName
{
	char Name[20];
	uint8_t Num;
};

static TArray<StyleName> StyleNames;

static TArray<PClassActor *> AmmoNames;
static TArray<PClassActor *> WeaponNames;

struct MBFArgs
{
	int64_t args[8];
	int argsused;
};
static TMap<FState*, MBFArgs> stateargs;
static FState* FindState(int statenum);

// DeHackEd trickery to support MBF-style parameters
// List of states that are hacked to use a codepointer
struct MBFParamState
{
	FState *state;
	int pointer;
	int argsused;
	int64_t* args;

	PClassActor* GetTypeArg(int i)
	{
		PClassActor* type = nullptr;
		int num = (int)args[i];
		if (num > 0 && num < int(InfoNames.Size())) type = InfoNames[num-1];	// Dehacked is 1-based.
		return type;
	}

	FState* GetStateArg(int i)
	{
		int num = (int)args[i];
		return FindState(num);
	}


	int GetIntArg(int i, int def = 0)
	{
		return argsused & (1 << i)? (int)args[i] : def;
	}

	int GetSoundArg(int i, int def = 0)
	{
		int num = argsused & (1 << i) ? (int)args[i] : def;
		if (num > 0 && num < int(SoundMap.Size())) return SoundMap[num];
		return 0;
	}

	double GetFloatArg(int i, double def = 0)
	{
		return argsused & (1 << i) ? FixedToFloat((fixed_t)args[i]) : def;
	}

	void ValidateArgCount(int num, const char* function)
	{
		if (argsused >= (1 << num))
		{
			Printf("Too many args for %s\n", function); // got no line number... :(
		}
	}
};
static TArray<MBFParamState> MBFParamStates;
// Data on how to correctly modify the codepointers
struct CodePointerAlias
{
	FName name;
	FString alias;
};
static TArray<CodePointerAlias> MBFCodePointers;

struct AmmoPerAttack
{
	ENamedName func;
	int ammocount;
	VMFunction *ptr;
};

struct DehBits
{
	const char* name;
	int value;
};


// Default ammo use of the various weapon attacks
static AmmoPerAttack AmmoPerAttacks[] = {
	{ NAME_A_Punch, 0},
	{ NAME_A_FirePistol, 1},
	{ NAME_A_FireShotgun, 1},
	{ NAME_A_FireShotgun2, 2},
	{ NAME_A_FireCGun, 1},
	{ NAME_A_FireMissile, 1},
	{ NAME_A_Saw, 0},
	{ NAME_A_FirePlasma, 1},
	{ NAME_A_FireBFG, -1},	// uses deh.BFGCells
	{ NAME_A_FireOldBFG, 1},
	{ NAME_A_FireRailgun, 1},
	{ NAME_None, 0}
};


// Miscellaneous info that used to be constant
DehInfo deh =
{
	100,	// .StartHealth
	 50,	// .StartBullets
	100,	// .MaxHealth
	200,	// .MaxArmor
	  1,	// .GreenAC
	  2,	// .BlueAC
	200,	// .MaxSoulsphere
	100,	// .SoulsphereHealth
	200,	// .MegasphereHealth
	100,	// .GodHealth
	200,	// .FAArmor
	  2,	// .FAAC
	200,	// .KFAArmor
	  2,	// .KFAAC
	"PLAY",	// Name of player sprite
	255,	// Rocket explosion style, 255=use cvar
	2./3.,		// Rocket explosion alpha
	false,	// .NoAutofreeze
	40,		// BFG cells per shot
};

DEFINE_GLOBAL(deh)
DEFINE_FIELD_X(DehInfo, DehInfo, MaxSoulsphere)
DEFINE_FIELD_X(DehInfo, DehInfo, ExplosionStyle)
DEFINE_FIELD_X(DehInfo, DehInfo, ExplosionAlpha)
DEFINE_FIELD_X(DehInfo, DehInfo, NoAutofreeze)
DEFINE_FIELD_X(DehInfo, DehInfo, BFGCells)
DEFINE_FIELD_X(DehInfo, DehInfo, BlueAC)
DEFINE_FIELD_X(DehInfo, DehInfo, MaxHealth)

// Doom identified pickup items by their sprites. ZDoom prefers to use their
// class type to identify them instead. To support the traditional Doom
// behavior, for every thing touched by dehacked that has the MF_PICKUP flag,
// a new subclass of DehackedPickup will be created with properties copied
// from the original actor's defaults. The original actor is then changed to
// spawn the new class.

TArray<PClassActor *> TouchedActors;

TArray<uint32_t> UnchangedSpriteNames;
bool changedStates;

// Sprite<->Class map for DehackedPickup::DetermineType
static struct DehSpriteMap
{
	char Sprite[5];
	const char *ClassName;
}
DehSpriteMappings[] =
{
	{ "AMMO", "ClipBox" },
	{ "ARM1", "GreenArmor" },
	{ "ARM2", "BlueArmor" },
	{ "BFUG", "BFG9000" },
	{ "BKEY", "BlueCard" },
	{ "BON1", "HealthBonus" },
	{ "BON2", "ArmorBonus" },
	{ "BPAK", "Backpack" },
	{ "BROK", "RocketBox" },
	{ "BSKU", "BlueSkull" },
	{ "CELL", "Cell" },
	{ "CELP", "CellPack" },
	{ "CLIP", "Clip" },
	{ "CSAW", "Chainsaw" },
	{ "LAUN", "RocketLauncher" },
	{ "MEDI", "Medikit" },
	{ "MEGA", "Megasphere" },
	{ "MGUN", "Chaingun" },
	{ "PINS", "BlurSphere" },
	{ "PINV", "InvulnerabilitySphere" },
	{ "PLAS", "PlasmaRifle" },
	{ "PMAP", "Allmap" },
	{ "PSTR", "Berserk" },
	{ "PVIS", "Infrared" },
	{ "RKEY", "RedCard" },
	{ "ROCK", "RocketAmmo" },
	{ "RSKU", "RedSkull" },
	{ "SBOX", "ShellBox" },
	{ "SGN2", "SuperShotgun" },
	{ "SHEL", "Shell" },
	{ "SHOT", "Shotgun" },
	{ "SOUL", "Soulsphere" },
	{ "STIM", "Stimpack" },
	{ "SUIT", "RadSuit" },
	{ "YKEY", "YellowCard" },
	{ "YSKU", "YellowSkull" }
};

#define LINESIZE 2048

#define CHECKKEY(a,b)		if (!stricmp (Line1, (a))) (b) = atoi(Line2);

static char* PatchFile, * PatchPt;
FString PatchName;
static int PatchSize;
static char *Line1, *Line2;
static int	 dversion, pversion;
static bool  including, includenotext;
static int LumpFileNum;

static const char *unknown_str = "Unknown key %s encountered in %s %d.\n";

static StringMap EnglishStrings, DehStrings;

// This is an offset to be used for computing the text stuff.
// Straight from the DeHackEd source which was
// Written by Greg Lewis, gregl@umich.edu.
static int toff[] = {129044, 129044, 129044, 129284, 129380};

struct Key {
	const char *name;
	ptrdiff_t offset;
};

static int PatchThing (int);
static int PatchSound (int);
static int PatchFrame (int);
static int PatchSprite (int);
static int PatchAmmo (int);
static int PatchWeapon (int);
static int PatchPointer (int);
static int PatchCheats (int);
static int PatchMisc (int);
static int PatchText (int);
static int PatchStrings (int);
static int PatchPars (int);
static int PatchCodePtrs (int);
static int PatchMusic (int);
static int DoInclude (int);
static bool DoDehPatch();

static const struct {
	const char *name;
	int (*func)(int);
} Modes[] = {
	// These appear in .deh and .bex files
	{ "Thing",		PatchThing },
	{ "Sound",		PatchSound },
	{ "Frame",		PatchFrame },
	{ "Sprite",		PatchSprite },
	{ "Ammo",		PatchAmmo },
	{ "Weapon",		PatchWeapon },
	{ "Pointer",	PatchPointer },
	{ "Cheat",		PatchCheats },
	{ "Misc",		PatchMisc },
	{ "Text",		PatchText },
	// These appear in .bex files
	{ "include",	DoInclude },
	{ "[STRINGS]",	PatchStrings },
	{ "[PARS]",		PatchPars },
	{ "[CODEPTR]",	PatchCodePtrs },
	{ "[MUSIC]",	PatchMusic },
	{ NULL, NULL },
};

static int HandleMode (const char *mode, int num);
static bool HandleKey (const struct Key *keys, void *structure, const char *key, int value);
static bool ReadChars (char **stuff, int size);
static char *igets (void);
static int GetLine (void);

inline double DEHToDouble(int acsval)
{
	return acsval / 65536.;
}

static void PushTouchedActor(PClassActor *cls)
{
	if (TouchedActors.Find(cls) == TouchedActors.Size())
		TouchedActors.Push(cls);
}


static int HandleMode (const char *mode, int num)
{
	int i = 0;
	while (Modes[i].name && stricmp (Modes[i].name, mode))
		i++;

	if (Modes[i].name)
		return Modes[i].func (num);

	// Handle unknown or unimplemented data
	Printf ("Unknown chunk %s encountered. Skipping.\n", mode);
	do
		i = GetLine ();
	while (i == 1);

	return i;
}

static bool HandleKey (const struct Key *keys, void *structure, const char *key, int value)
{
	while (keys->name && stricmp (keys->name, key))
		keys++;

	if (keys->name) {
		*((int *)(((uint8_t *)structure) + keys->offset)) = value;
		return false;
	}

	return true;
}

static int FindSprite (const char *sprname)
{
	uint32_t nameint;
	memcpy(&nameint, sprname, 4);
	auto f = UnchangedSpriteNames.Find(nameint);
	return f == UnchangedSpriteNames.Size() ? -1 : f;
}

static FState *FindState (int statenum)
{
	int stateacc;
	unsigned i;

	if (statenum == 0)
		return NULL;

	for (i = 0, stateacc = 1; i < StateMap.Size(); i++)
	{
		if (stateacc <= statenum && stateacc + StateMap[i].StateSpan > statenum)
		{
			if (StateMap[i].State != NULL)
			{
				if (StateMap[i].OwnerIsPickup)
				{
					PushTouchedActor(StateMap[i].Owner);
				}
				return StateMap[i].State + statenum - stateacc;
			}
			else return NULL;
		}
		stateacc += StateMap[i].StateSpan;
	}
	return NULL;
}

int FindStyle (const char *namestr)
{
	for(unsigned i = 0; i < StyleNames.Size(); i++)
	{
		if (!stricmp(StyleNames[i].Name, namestr)) return StyleNames[i].Num;
	}
	DPrintf(DMSG_ERROR, "Unknown render style %s\n", namestr);
	return -1;
}

static bool ReadChars (char **stuff, int size)
{
	char *str = *stuff;

	if (!size) {
		*str = 0;
		return true;
	}

	do {
		// Ignore carriage returns
		if (*PatchPt != '\r')
			*str++ = *PatchPt;
		else
			size++;

		PatchPt++;
	} while (--size && *PatchPt != 0);

	*str = 0;
	return true;
}

static void ReplaceSpecialChars (char *str)
{
	char *p = str, c;
	int i;

	while ( (c = *p++) ) {
		if (c != '\\') {
			*str++ = c;
		} else {
			switch (*p) {
				case 'n':
				case 'N':
					*str++ = '\n';
					break;
				case 't':
				case 'T':
					*str++ = '\t';
					break;
				case 'r':
				case 'R':
					*str++ = '\r';
					break;
				case 'x':
				case 'X':
					c = 0;
					p++;
					for (i = 0; i < 2; i++) {
						c <<= 4;
						if (*p >= '0' && *p <= '9')
							c += *p-'0';
						else if (*p >= 'a' && *p <= 'f')
							c += 10 + *p-'a';
						else if (*p >= 'A' && *p <= 'F')
							c += 10 + *p-'A';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c = 0;
					for (i = 0; i < 3; i++) {
						c <<= 3;
						if (*p >= '0' && *p <= '7')
							c += *p-'0';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				default:
					*str++ = *p;
					break;
			}
			p++;
		}
	}
	*str = 0;
}

static char *skipwhite (char *str)
{
	if (str)
		while (*str && isspace(*str))
			str++;
	return str;
}

static void stripwhite (char *str)
{
	char *end = str + strlen(str) - 1;

	while (end >= str && isspace(*end))
		end--;

	end[1] = '\0';
}

static char *igets (void)
{
	assert(PatchPt != nullptr);

	char *line;

	if (*PatchPt == '\0' || PatchPt >= PatchFile + PatchSize )
		return NULL;

	line = PatchPt;

	while (*PatchPt != '\n' && *PatchPt != '\0')
		PatchPt++;

	if (*PatchPt == '\n')
		*PatchPt++ = 0;

	return line;
}

static int GetLine (void)
{
	char *line, *line2;

	do {
		while ( (line = igets ()) )
			if (line[0] != '#')		// Skip comment lines
				break;

		if (!line)
			return 0;

		Line1 = skipwhite (line);
	} while (Line1 && *Line1 == 0);	// Loop until we get a line with
									// more than just whitespace.
	line = strchr (Line1, '=');

	if (line) {					// We have an '=' in the input line
		line2 = line;
		while (--line2 >= Line1)
			if (*line2 > ' ')
				break;

		if (line2 < Line1)
			return 0;			// Nothing before '='

		*(line2 + 1) = 0;

		line++;
		while (*line && *line <= ' ')
			line++;

		if (*line == 0)
			return 0;			// Nothing after '='

		Line2 = line;

		return 1;
	} else {					// No '=' in input line
		line = Line1 + 1;
		while (*line > ' ')
			line++;				// Get beyond first word

		*line++ = 0;
		while (*line && *line <= ' ')
			line++;				// Skip white space

		//.bex files don't have this restriction
		//if (*line == 0)
		//	return 0;			// No second word

		Line2 = line;

		return 2;
	}
}


// misc1 = vrange (arg +3), misc2 = hrange (arg+4)
static void CreateMushroomFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{ // A_Mushroom
	emitters.AddParameterPointerConst(PClass::FindClass("FatShot"));	// itemtype
	emitters.AddParameterIntConst(0);									// numspawns
	emitters.AddParameterIntConst(1);									// flag MSF_Classic
	emitters.AddParameterFloatConst(value1? DEHToDouble(value1) : 4.0);	// vrange
	emitters.AddParameterFloatConst(value2? DEHToDouble(value2) : 0.5);	// hrange
}

// misc1 = type (arg +0), misc2 = Z-pos (arg +2)
static void CreateSpawnFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{ // A_SpawnItem
	if (InfoNames[value1-1] == nullptr)
	{
		I_Error("No class found for dehackednum %d!\n", value1+1);
	}
	emitters.AddParameterPointerConst(InfoNames[value1-1]);	// itemtype
	emitters.AddParameterFloatConst(0);						// distance
	emitters.AddParameterFloatConst(value2);				// height
	emitters.AddParameterIntConst(0);						// useammo
	emitters.AddParameterIntConst(0);						// transfer_translation
}


// misc1 = angle (in degrees) (arg +0 but factor in current actor angle too)
static void CreateTurnFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{ // A_Turn
	emitters.AddParameterFloatConst(value1);				// angle
}

// misc1 = angle (in degrees) (arg +0)
static void CreateFaceFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{ // A_SetAngle
	emitters.AddParameterFloatConst(value1);				// angle
	emitters.AddParameterIntConst(0);						// flags
	emitters.AddParameterIntConst(AAPTR_DEFAULT);			// ptr
}

// misc1 = damage, misc 2 = sound
static void CreateScratchFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{ // A_CustomMeleeAttack
	emitters.AddParameterIntConst(value1);								// damage
	emitters.AddParameterIntConst(value2 ? (int)SoundMap[value2 - 1] : 0);	// hit sound
	emitters.AddParameterIntConst(0);									// miss sound
	emitters.AddParameterIntConst(NAME_None);							// damage type
	emitters.AddParameterIntConst(true);								// bleed
}

// misc1 = sound, misc2 = attenuation none (true) or normal (false)
static void CreatePlaySoundFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{ // A_PlaySound
	emitters.AddParameterIntConst(value1 ? (int)SoundMap[value1 - 1] : 0);	// soundid
	emitters.AddParameterIntConst(CHAN_BODY);							// channel
	emitters.AddParameterFloatConst(1);									// volume
	emitters.AddParameterIntConst(false);								// looping
	emitters.AddParameterFloatConst(value2 ? ATTN_NONE : ATTN_NORM);	// attenuation
	emitters.AddParameterIntConst(false);								// local
	emitters.AddParameterFloatConst(0.0);								// pitch
}

// misc1 = state, misc2 = probability
static void CreateRandomJumpFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{ // A_Jump
	auto symlabel = StateLabels.AddPointer(FindState(value1));

	emitters.AddParameterIntConst(value2);					// maxchance
	emitters.AddParameterIntConst(symlabel);				// jumpto
	emitters.AddReturn(REGT_POINTER);
}

// misc1 = Boom linedef type, misc2 = sector tag
static void CreateLineEffectFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{ // A_LineEffect
	// This is the second MBF codepointer that couldn't be translated easily.
	// Calling P_TranslateLineDef() here was a simple matter, as was adding an
	// extra parameter to A_CallSpecial so as to replicate the LINEDONE stuff,
	// but unfortunately DEHACKED lumps are processed before the map translation
	// arrays are initialized so this didn't work.
	emitters.AddParameterIntConst(value1);		// special
	emitters.AddParameterIntConst(value2);		// tag
}

// No misc, but it's basically A_Explode with an added effect
static void CreateNailBombFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{ // A_Explode
	// This one does not actually have MBF-style parameters. But since
	// we're aliasing it to an extension of A_Explode...
	emitters.AddParameterIntConst(-1);		// damage
	emitters.AddParameterIntConst(-1);		// distance
	emitters.AddParameterIntConst(1);		// flags (1=XF_HURTSOURCE)
	emitters.AddParameterIntConst(0);		// alert
	emitters.AddParameterIntConst(0);		// fulldamagedistance
	emitters.AddParameterIntConst(30);		// nails
	emitters.AddParameterIntConst(10);		// naildamage
	emitters.AddParameterPointerConst(PClass::FindClass(NAME_BulletPuff));	// itemtype
	emitters.AddParameterIntConst(NAME_None);	// damage type
}

static void CreateSpawnObjectFunc(FunctionCallEmitter& emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(8, "A_SpawnObject");
	emitters.AddParameterPointerConst(state->GetTypeArg(0));
	emitters.AddParameterFloatConst(state->GetFloatArg(1));
	emitters.AddParameterFloatConst(state->GetFloatArg(2));
	emitters.AddParameterFloatConst(state->GetFloatArg(3));
	emitters.AddParameterFloatConst(state->GetFloatArg(4));
	emitters.AddParameterFloatConst(state->GetFloatArg(5));
	emitters.AddParameterFloatConst(state->GetFloatArg(6));
	emitters.AddParameterFloatConst(state->GetFloatArg(7));
}

static void CreateMonsterProjectileFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(5, "A_MonsterProjectile");
	emitters.AddParameterPointerConst(state->GetTypeArg(0));
	emitters.AddParameterFloatConst(state->GetFloatArg(1));
	emitters.AddParameterFloatConst(state->GetFloatArg(2));
	emitters.AddParameterFloatConst(state->GetFloatArg(3));
	emitters.AddParameterFloatConst(state->GetFloatArg(4));
}

static void CreateMonsterBulletAttackFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(5, "A_MonsterBulletAttack");
	emitters.AddParameterFloatConst(state->GetFloatArg(0));
	emitters.AddParameterFloatConst(state->GetFloatArg(1));
	emitters.AddParameterIntConst(state->GetIntArg(2, 1));
	emitters.AddParameterIntConst(state->GetIntArg(3, 3));
	emitters.AddParameterIntConst(state->GetIntArg(4, 5));
}

static void CreateMonsterMeleeAttackFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(4, "A_MonsterMeleeAttack");
	emitters.AddParameterIntConst(state->GetIntArg(0, 3));
	emitters.AddParameterIntConst(state->GetIntArg(1, 8));
	emitters.AddParameterIntConst(state->GetIntArg(2, 0));
	emitters.AddParameterFloatConst(state->GetFloatArg(3));

}

static void CreateRadiusDamageFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(2, "A_RadiusDamage");
	emitters.AddParameterIntConst(state->GetIntArg(0, 0));
	emitters.AddParameterIntConst(state->GetIntArg(1, 0));
}

static void CreateHealChaseFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(2, "A_HealChase");
	emitters.AddParameterPointerConst(state->GetStateArg(0));
	emitters.AddParameterIntConst(state->GetSoundArg(1));
}

static void CreateSeekTracerFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(2, "A_SeekTracer");
	emitters.AddParameterFloatConst(state->GetFloatArg(0, 0));
	emitters.AddParameterFloatConst(state->GetFloatArg(1, 0));
}

static void CreateFindTracerFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(2, "A_FindTracer");
	emitters.AddParameterFloatConst(state->GetFloatArg(0));
	emitters.AddParameterIntConst(state->GetIntArg(1, 10));
}

static void CreateJumpIfHealthBelowFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(2, "A_JumpIfHealthBelow");
	emitters.AddParameterPointerConst(state->GetStateArg(0));
	emitters.AddParameterIntConst(state->GetIntArg(1));
}

static void CreateJumpIfFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(2, "A_JumpIf..");
	emitters.AddParameterPointerConst(state->GetStateArg(0));
	emitters.AddParameterFloatConst(state->GetFloatArg(1));
}

static void CreateJumpIfFlagSetFunc(FunctionCallEmitter& emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(2, "A_JumpIfFlagsSet");
	emitters.AddParameterPointerConst(state->GetStateArg(0));
	emitters.AddParameterIntConst(state->GetIntArg(1));
	emitters.AddParameterIntConst(state->GetIntArg(2));
}

static void CreateFlagSetFunc(FunctionCallEmitter& emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(2, "A_...Flags");
	emitters.AddParameterIntConst(state->GetIntArg(0));
	emitters.AddParameterIntConst(state->GetIntArg(1));
}

static void CreateWeaponProjectileFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(5, "A_WeaponProjectile");
	emitters.AddParameterPointerConst(state->GetTypeArg(0));
	emitters.AddParameterFloatConst(state->GetFloatArg(1));
	emitters.AddParameterFloatConst(state->GetFloatArg(2));
	emitters.AddParameterFloatConst(state->GetFloatArg(3));
	emitters.AddParameterFloatConst(state->GetFloatArg(4));
}

static void CreateWeaponBulletAttackFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(5, "A_WeaponBulletAttack");
	emitters.AddParameterFloatConst(state->GetFloatArg(0));
	emitters.AddParameterFloatConst(state->GetFloatArg(1));
	emitters.AddParameterIntConst(state->GetIntArg(2, 1));
	emitters.AddParameterIntConst(state->GetIntArg(3, 5));
	emitters.AddParameterIntConst(state->GetIntArg(4, 3));
}

static void CreateWeaponMeleeAttackFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(5, "A_WeaponBulletAttack");
	emitters.AddParameterIntConst(state->GetIntArg(0, 2));
	emitters.AddParameterIntConst(state->GetIntArg(1, 10));
	emitters.AddParameterFloatConst(state->GetFloatArg(2, 1));
	emitters.AddParameterIntConst(state->GetIntArg(3));
	emitters.AddParameterIntConst(state->GetIntArg(4));
}

static void CreateWeaponSoundFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(2, "A_WeaponSound");
	emitters.AddParameterIntConst(state->GetSoundArg(0));
	emitters.AddParameterIntConst(state->GetIntArg(1));
}

static void CreateWeaponJumpFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(2, "A_WeaponJump");
	emitters.AddParameterPointerConst(state->GetStateArg(0));
	emitters.AddParameterIntConst(state->GetIntArg(1));
}

static void CreateConsumeAmmoFunc(FunctionCallEmitter &emitters, int value1, int value2, MBFParamState* state)
{
	state->ValidateArgCount(1, "A_ConsumeAmmo");
	emitters.AddParameterIntConst(state->GetIntArg(0));

}

// This array must be in sync with the Aliases array in DEHSUPP.
static void (*MBFCodePointerFactories[])(FunctionCallEmitter&, int, int, MBFParamState*) =
{
	// Die and Detonate are not in this list because these codepointers have
	// no dehacked arguments and therefore do not need special handling.
	// NailBomb has no argument but is implemented as new parameters for A_Explode.
	CreateMushroomFunc,
	CreateSpawnFunc,
	CreateTurnFunc,
	CreateFaceFunc,
	CreateScratchFunc,
	CreatePlaySoundFunc,
	CreateRandomJumpFunc,
	CreateLineEffectFunc,
	CreateNailBombFunc,
	CreateSpawnObjectFunc,
	CreateMonsterProjectileFunc,
	CreateMonsterBulletAttackFunc,
	CreateMonsterMeleeAttackFunc,
	CreateRadiusDamageFunc,
	CreateHealChaseFunc,
	CreateSeekTracerFunc,
	CreateFindTracerFunc,
	CreateJumpIfHealthBelowFunc,
	CreateJumpIfFunc,
	CreateJumpIfFunc,
	CreateJumpIfFunc,
	CreateJumpIfFunc,
	CreateWeaponProjectileFunc,
	CreateWeaponBulletAttackFunc,
	CreateWeaponMeleeAttackFunc,
	CreateWeaponSoundFunc,
	CreateWeaponJumpFunc,
	CreateConsumeAmmoFunc,
	CreateWeaponJumpFunc,
	CreateWeaponJumpFunc,
	CreateWeaponJumpFunc,
	CreateJumpIfFlagSetFunc,
	CreateFlagSetFunc
};

// Creates new functions for the given state so as to convert MBF-args (misc1 and misc2) into real args.

static void SetDehParams(FState *state, int codepointer, VMDisassemblyDumper &disasmdump, MBFParamState* pstate)
{
	static const uint8_t regts[] = { REGT_POINTER, REGT_POINTER, REGT_POINTER };
	int value1 = state->GetMisc1();
	int value2 = state->GetMisc2();
	if (!(value1|value2)) return;
	
	bool returnsState = codepointer == 6;
	
	// Fakey fake script position thingamajig. Because NULL cannot be used instead.
	// Even if the lump was parsed by an FScanner, there would hardly be a way to
	// identify which line is troublesome.
	FScriptPosition *pos = new FScriptPosition(FString("DEHACKED"), 0);
	
	// Let's identify the codepointer we're dealing with.
	PFunction *sym;
	sym = dyn_cast<PFunction>(PClass::FindActor(NAME_Weapon)->FindSymbol(FName(MBFCodePointers[codepointer].name), true));
	if (sym == NULL ) return;

	if (codepointer < 0 || (unsigned)codepointer >= countof(MBFCodePointerFactories))
	{
		// This simply should not happen.
		Printf("Unmanaged dehacked codepointer alias num %i\n", codepointer);
	}
	else
	{
		MBFArgs scratchargs{};
		auto args = stateargs.CheckKey(pstate->state);
		if (!args) args = &scratchargs;
		pstate->args = args->args;
		pstate->argsused = args->argsused;

		int numargs = sym->GetImplicitArgs();
		auto funcsym = CreateAnonymousFunction(RUNTIME_CLASS(AActor)->VMType, returnsState? (PType*)TypeState : TypeVoid, numargs==3? SUF_ACTOR|SUF_WEAPON : SUF_ACTOR);
		VMFunctionBuilder buildit(numargs);
		// Allocate registers used to pass parameters in.
		// self, stateowner, state (all are pointers)
		buildit.Registers[REGT_POINTER].Get(numargs);
		// Emit code to pass the standard action function parameters.
		FunctionCallEmitter emitters(sym->Variants[0].Implementation);
		for (int i = 0; i < numargs; i++)
		{
			emitters.AddParameterPointer(i, false);
		}
		// Emit code for action parameters.
		MBFCodePointerFactories[codepointer](emitters, value1, value2, pstate);
		auto where = emitters.EmitCall(&buildit);
		if (!returnsState) buildit.Emit(OP_RET, RET_FINAL, REGT_NIL, 0);
		else buildit.Emit(OP_RET, RET_FINAL, EncodeRegType(where), where.RegNum);
		where.Free(&buildit);

		// Attach it to the state.
		VMScriptFunction *sfunc = new VMScriptFunction;
		funcsym->Variants[0].Implementation = sfunc;
		sfunc->Proto = funcsym->Variants[0].Proto;
		sfunc->RegTypes = regts;	// These functions are built after running the script compiler so they don't get this info.
		buildit.MakeFunction(sfunc);
		sfunc->NumArgs = numargs;
		sfunc->ImplicitArgs = numargs;
		state->SetAction(sfunc);
		sfunc->PrintableName.Format("Dehacked.%s.%d.%d", MBFCodePointers[codepointer].name.GetChars(), value1, value2);

		disasmdump.Write(sfunc, sfunc->PrintableName);

#ifdef HAVE_VM_JIT
		if (Args->CheckParm("-dumpjit"))
		{
			FILE *dump = fopen("dumpjit.txt", "a");
			if (dump != nullptr)
			{
				JitDumpLog(dump, sfunc);
			}
			fclose(dump);
		}
#endif // HAVE_VM_JIT
	}
}

struct DehFlags2
{
	const char* name;
	void (*setter)(AActor* defaults);
};

// not all of these map to real flags so this table needs handler callbacks.
static const struct DehFlags2 deh_mobjflags_mbf21[] = {
  {"LOGRAV",         [](AActor* defaults) { defaults->Gravity = 1. / 8.; }}, // low gravity
  {"SHORTMRANGE",    [](AActor* defaults) { defaults->maxtargetrange = 896.; }}, // short missile range
  {"DMGIGNORED",     [](AActor* defaults) { defaults->flags3 |= MF3_NOTARGET; }}, // other things ignore its attacks
  {"NORADIUSDMG",    [](AActor* defaults) { defaults->flags3 |= MF3_NORADIUSDMG; }}, // doesn't take splash damage
  {"FORCERADIUSDMG", [](AActor* defaults) { defaults->flags4 |= MF4_FORCERADIUSDMG; }}, // causes splash damage even if target immune
  {"HIGHERMPROB",    [](AActor* defaults) { defaults->MinMissileChance = 160; }}, // higher missile attack probability
  {"RANGEHALF",      [](AActor* defaults) { defaults->flags4 |= MF4_MISSILEMORE; }}, // use half distance for missile attack probability
  {"NOTHRESHOLD",    [](AActor* defaults) { defaults->flags4 |= MF4_QUICKTORETALIATE; }}, // no targeting threshold
  {"LONGMELEE",      [](AActor* defaults) { defaults->meleethreshold = 196; }}, // long melee range
  {"BOSS",           [](AActor* defaults) { defaults->flags2 |= MF2_BOSS; }}, // full volume see / death sound + splash immunity
  {"MAP07BOSS1",     [](AActor* defaults) { defaults->flags8 |= MF8_MAP07BOSS1; }}, // Tag 666 "boss" on doom 2 map 7
  {"MAP07BOSS2",     [](AActor* defaults) { defaults->flags8 |= MF8_MAP07BOSS2; }}, // Tag 667 "boss" on doom 2 map 7
  {"E1M8BOSS",       [](AActor* defaults) { defaults->flags8 |= MF8_E1M8BOSS; }}, // E1M8 boss
  {"E2M8BOSS",       [](AActor* defaults) { defaults->flags8 |= MF8_E2M8BOSS; }}, // E2M8 boss
  {"E3M8BOSS",       [](AActor* defaults) { defaults->flags8 |= MF8_E3M8BOSS; }}, // E3M8 boss
  {"E4M6BOSS",       [](AActor* defaults) { defaults->flags8 |= MF8_E4M6BOSS; }}, // E4M6 boss
  {"E4M8BOSS",       [](AActor* defaults) { defaults->flags8 |= MF8_E4M8BOSS; }}, // E4M8 boss
  {"RIP",            [](AActor* defaults) { defaults->flags2 |= MF2_RIP;  }}, // projectile rips through targets
  {"FULLVOLSOUNDS",  [](AActor* defaults) { defaults->flags8 |= MF8_FULLVOLSEE; defaults->flags3 |= MF3_FULLVOLDEATH; } }, // full volume see / death sound
};

static void ClearBits2Stuff(AActor* defaults)
{
	defaults->Gravity = 1.;
	defaults->maxtargetrange = 0;
	defaults->MinMissileChance = 200;
	defaults->meleethreshold = 0;
	defaults->flags2 &= ~(MF2_BOSS | MF2_RIP);
	defaults->flags3 &= ~(MF3_NOTARGET | MF3_NORADIUSDMG | MF3_FULLVOLDEATH);
	defaults->flags4 &= ~(MF4_MISSILEMORE | MF4_QUICKTORETALIATE | MF4_FORCERADIUSDMG);
	defaults->flags8 &= ~(MF8_E1M8BOSS | MF8_E2M8BOSS | MF8_E3M8BOSS | MF8_E4M8BOSS | MF8_E4M6BOSS | MF8_MAP07BOSS1 | MF8_MAP07BOSS2 | MF8_FULLVOLSEE);
}


static int PatchThing (int thingy)
{
	enum
	{
		// Boom flags
		MF_TRANSLATION	= 0x0c000000,	// if 0x4 0x8 or 0xc, use a translation
		MF_TRANSSHIFT	= 26,			// table for player colormaps
		// A couple of Boom flags that don't exist in ZDoom
		MF_SLIDE		= 0x00002000,	// Player: keep info about sliding along walls.
		MF_TRANSLUCENT	= 0x80000000,	// Translucent sprite?
		// MBF flags: TOUCHY is remapped to flags6, FRIEND is turned into FRIENDLY,
		// and finally BOUNCES is replaced by bouncetypes with the BOUNCES_MBF bit.
		MF_TOUCHY		= 0x10000000,	// killough 11/98: dies when solids touch it
		MF_BOUNCES		= 0x20000000,	// killough 7/11/98: for beta BFG fireballs
		MF_FRIEND		= 0x40000000,	// killough 7/18/98: friendly monsters
	};

	int result;
	AActor *info;
	uint8_t dummy[sizeof(AActor)];
	bool hadHeight = false;
	bool hadTranslucency = false;
	bool hadStyle = false;
	FStateDefinitions statedef;
	bool patchedStates = false;
	ActorFlags oldflags;
	PClassActor *type;
	int16_t *ednum, dummyed;

	type = NULL;
	info = (AActor *)&dummy;
	ednum = &dummyed;
	if (thingy > (int)InfoNames.Size() || thingy <= 0)
	{
		Printf ("Thing %d out of range.\n", thingy);
	}
	else
	{
		DPrintf (DMSG_SPAMMY, "Thing %d\n", thingy);
		if (thingy > 0)
		{
			type = InfoNames[thingy - 1];
			if (type == NULL)
			{
				info = (AActor *)&dummy;
				ednum = &dummyed;
				// An error for the name has already been printed while loading DEHSUPP.
				Printf ("Could not find thing %d\n", thingy);
			}
			else
			{
				info = GetDefaultByType (type);
				ednum = &type->ActorInfo()->DoomEdNum;
			}
		}
	}

	oldflags = info->flags;

	while ((result = GetLine ()) == 1)
	{
		char *endptr;
		uint32_t val = (uint32_t)strtoull (Line2, &endptr, 10);
		size_t linelen = strlen (Line1);

		if (linelen == 10 && stricmp (Line1, "Hit points") == 0)
		{
			info->health = val;
		}
		else if (linelen == 13 && stricmp (Line1, "Reaction time") == 0)
		{
			info->reactiontime = val;
		}
		else if (linelen == 11 && stricmp (Line1, "Pain chance") == 0)
		{
			info->PainChance = (int16_t)val;
		}
		else if (linelen == 12 && stricmp (Line1, "Translucency") == 0)
		{
			info->Alpha = DEHToDouble(val);
			info->RenderStyle = STYLE_Translucent;
			info->renderflags &= ~RF_ZDOOMTRANS;
			hadTranslucency = true;
			hadStyle = true;
		}
		else if (linelen == 6 && stricmp (Line1, "Height") == 0)
		{
			info->Height = DEHToDouble(val);
			info->projectilepassheight = 0;	// needs to be disabled
			hadHeight = true;
		}
		else if (linelen == 14 && stricmp (Line1, "Missile damage") == 0)
		{
			info->SetDamage(val);
		}
		else if (linelen == 5)
		{
			if (stricmp (Line1, "Speed") == 0)
			{
				info->Speed = (signed long)val;	// handle fixed point later.
			}
			else if (stricmp (Line1, "Width") == 0)
			{
				info->radius = DEHToDouble(val);
			}
			else if (stricmp (Line1, "Alpha") == 0)
			{
				info->Alpha = atof (Line2);
				hadTranslucency = true;
			}
			else if (stricmp (Line1, "Scale") == 0)
			{
				info->Scale.Y = info->Scale.X = clamp(atof (Line2), 1./65536, 256.);
			}
			else if (stricmp (Line1, "Decal") == 0)
			{
				stripwhite (Line2);
				const FDecalTemplate *decal = DecalLibrary.GetDecalByName (Line2);
				if (decal != NULL)
				{
					info->DecalGenerator = const_cast <FDecalTemplate *>(decal);
				}
				else
				{
					Printf ("Thing %d: Unknown decal %s\n", thingy, Line2);
				}
			}
		}
		else if (linelen == 12 && stricmp (Line1, "Render Style") == 0)
		{
			stripwhite (Line2);
			int style = FindStyle (Line2);
			if (style >= 0)
			{
				info->RenderStyle = ERenderStyle(style);
				hadStyle = true;
			}
		}
		else if (linelen == 12 && stricmp(Line1, "No Ice Death") == 0)
		{
			if (val)
			{
				info->flags4 |= MF4_NOICEDEATH;
			}
			else
			{
				info->flags4 &= ~MF4_NOICEDEATH;
			}
		}
		else if (linelen == 16 && stricmp(Line1, "infighting group") == 0)
		{
			if (val < 0)
			{
				Printf("Infighting groups must be >= 0 (check your dehacked)\n");
				val = 0;
			}
			type->ActorInfo()->infighting_group = val;
		}
		else if (linelen == 16 && stricmp(Line1, "projectile group") == 0)
		{
			if (val < 0) val = -1;
			type->ActorInfo()->projectile_group = val;
		}
		else if (linelen == 12 && stricmp(Line1, "splash group") == 0)
		{
			if (val < 0)
			{
				Printf("Splash groups must be >= 0 (check your dehacked)\n");
				val = 0;
			}
			type->ActorInfo()->splash_group = val;
		}
		else if (linelen == 10 && stricmp(Line1, "fast speed") == 0)
		{
			double fval = val >= 256 ? DEHToDouble(val) : val;
			info->FloatVar(NAME_FastSpeed) = fval;
		}
		else if (linelen == 11 && stricmp(Line1, "melee range") == 0)
		{
			info->meleerange = DEHToDouble(val) - 20;	// -20 is needed because DSDA subtracts it in P_CheckMeleeRange, while GZDoom does not.
		}
		else if (linelen == 10 && stricmp(Line1, "MBF21 Bits") == 0)
		{
			uint32_t value = 0;
			bool vchanged = false;

			char* strval;

			for (strval = Line2; (strval = strtok(strval, ",+| \t\f\r")); strval = NULL)
			{
				if (IsNum(strval))
				{
					value |= (unsigned long)strtoll(strval, NULL, 10);
					vchanged = true;
				}
				else
				{
					unsigned i;
					for (i = 0; i < countof(deh_mobjflags_mbf21); i++)
					{
						if (!stricmp(strval, deh_mobjflags_mbf21[i].name))
						{
							vchanged = true;
							value |= 1 << i;
							break;
						}
					}
					if (i == countof(deh_mobjflags_mbf21))
					{
						DPrintf(DMSG_ERROR, "Unknown bit mnemonic %s\n", strval);
					}
				}
			}
			if (vchanged)
			{
				ClearBits2Stuff(info);
				for (size_t i = 0; i < countof(deh_mobjflags_mbf21); i++)
				{
					if (value & (1 << i))
					{
						deh_mobjflags_mbf21[i].setter(info);
					}
				}
			}
			DPrintf(DMSG_SPAMMY, "MBF21 Bits: %d (0x%08x)\n", info->flags.GetValue(), info->flags.GetValue());
		}
		else if (linelen > 6)
		{
			if (stricmp (Line1 + linelen - 6, " frame") == 0)
			{
				FState *state = FindState (val);

				if (type != NULL && !patchedStates)
				{
					statedef.MakeStateDefines(type);
					patchedStates = true;
					changedStates = true;
				}

				if (!strnicmp (Line1, "Initial", 7))
					statedef.SetStateLabel("Spawn", state ? state : GetDefault<AActor>()->SpawnState);
				else if (!strnicmp (Line1, "First moving", 12))
					statedef.SetStateLabel("See", state);
				else if (!strnicmp (Line1, "Injury", 6))
					statedef.SetStateLabel("Pain", state);
				else if (!strnicmp (Line1, "Close attack", 12))
				{
					if (thingy != 1)	// Not for players!
					{
						statedef.SetStateLabel("Melee", state);
					}
				}
				else if (!strnicmp (Line1, "Far attack", 10))
				{
					if (thingy != 1)	// Not for players!
					{
						statedef.SetStateLabel("Missile", state);
					}
				}
				else if (!strnicmp (Line1, "Death", 5))
					statedef.SetStateLabel("Death", state);
				else if (!strnicmp (Line1, "Exploding", 9))
					statedef.SetStateLabel("XDeath", state);
				else if (!strnicmp (Line1, "Respawn", 7))
					statedef.SetStateLabel("Raise", state);
			}
			else if (stricmp (Line1 + linelen - 6, " sound") == 0)
			{
				FSoundID snd = 0;
				
				if (val == 0 || val >= SoundMap.Size())
				{
					if (endptr == Line2)
					{ // Sound was not a (valid) number,
						// so treat it as an actual sound name.
						stripwhite (Line2);
						snd = Line2;
					}
				}
				else
				{
					snd = SoundMap[val-1];
				}

				if (!strnicmp (Line1, "Alert", 5))
					info->SeeSound = snd;
				else if (!strnicmp (Line1, "Attack", 6))
					info->AttackSound = snd;
				else if (!strnicmp (Line1, "Pain", 4))
					info->PainSound = snd;
				else if (!strnicmp (Line1, "Death", 5))
					info->DeathSound = snd;
				else if (!strnicmp (Line1, "Action", 6))
					info->ActiveSound = snd;
				else if (!strnicmp(Line1, "Rip", 3))
					info->SoundVar(NAME_RipSound) = snd;

			}
		}
		else if (linelen == 4)
		{
			if (stricmp (Line1, "Mass") == 0)
			{
				info->Mass = val;
			}
			else if (stricmp (Line1, "Bits") == 0)
			{
				uint32_t value[4] = { 0, 0, 0 };
				bool vchanged[4] = { false, false, false };
				// ZDoom used to block the upper range of bits to force use of mnemonics for extra flags.
				// MBF also defined extra flags in the same range, but without forcing mnemonics. For MBF
				// compatibility, the upper bits are freed, but we have conflicts between the ZDoom bits
				// and the MBF bits. The only such flag exposed to DEHSUPP, though, is STEALTH -- the others
				// are not available through mnemonics, and aren't available either through their bit value.
				// So if we find the STEALTH keyword, it's a ZDoom mod, otherwise assume FRIEND.
				bool zdoomflags = false;
				char *strval;

				for (strval = Line2; (strval = strtok (strval, ",+| \t\f\r")); strval = NULL)
				{
					if (IsNum (strval))
					{
						value[0] |= (unsigned long)strtoll(strval, NULL, 10);
						vchanged[0] = true;
					}
					else
					{
						// STEALTH FRIEND HACK!
						if (!stricmp(strval, "STEALTH")) zdoomflags = true;
						unsigned i;
						for(i = 0; i < BitNames.Size(); i++)
						{
							if (!stricmp(strval, BitNames[i].Name))
							{
								vchanged[BitNames[i].WhichFlags] = true;
								value[BitNames[i].WhichFlags] |= 1 << BitNames[i].Bit;
								break;
							}
						}
						if (i == BitNames.Size())
						{
							DPrintf(DMSG_ERROR, "Unknown bit mnemonic %s\n", strval);
						}
					}
				}
				if (vchanged[0])
				{
					if (value[0] & MF_TRANSLATION)
					{
						info->Translation = TRANSLATION (TRANSLATION_Standard,
							((value[0] & MF_TRANSLATION) >> (MF_TRANSSHIFT))-1);
						value[0] &= ~MF_TRANSLATION;
					}
					if (value[0] & MF_TOUCHY)
					{
						// TOUCHY (which occupies in MBF what is the MF_UNMORPHED slot in ZDoom)
						value[0] &= ~MF_TOUCHY; // clean the slot
						info->flags6 |= MF6_TOUCHY; // remap the flag
					}
					if (value[0] & MF_BOUNCES)
					{
						// BOUNCES (which occupies in MBF the MF_NOLIFTDROP slot)
						// This flag is especially convoluted as what it does depend on what
						// other flags the actor also has, and whether it's "sentient" or not.
						value[0] &= ~MF_BOUNCES; // clean the slot

						// MBF considers that things that bounce can be damaged, even if not shootable.
						info->flags6 |= MF6_VULNERABLE;
						// MBF also considers that bouncers pass through blocking lines as projectiles.
						info->flags3 |= MF3_NOBLOCKMONST;
						// MBF also considers that bouncers that explode are grenades, and MBF grenades
						// are supposed to hurt everything, except cyberdemons if they're fired by cybies.
						// Let's translate that in a more generic way as grenades which hurt everything
						// except the class of their shooter. Yes, it does diverge a bit from MBF, as for
						// example a dehacked arachnotron that shoots grenade would kill itself quickly
						// in MBF and will not here. But class-specific checks are cumbersome and limiting.
						info->flags4 |= (MF4_FORCERADIUSDMG | MF4_DONTHARMCLASS);

						// MBF bouncing missiles rebound on floors and ceiling, but not on walls.
						// This is different from BOUNCE_Heretic behavior as in Heretic the missiles
						// die when they bounce, while in MBF they will continue to bounce until they
						// collide with a wall or a solid actor.
						if (value[0] & MF_MISSILE) info->BounceFlags = BOUNCE_Classic | BOUNCE_DEH;
						// MBF bouncing actors that do not have the missile flag will also rebound on
						// walls, and this does correspond roughly to the ZDoom bounce style.
						else info->BounceFlags = BOUNCE_Grenade | BOUNCE_DEH;

						// MBF grenades are dehacked rockets that gain the BOUNCES flag but
						// lose the MISSILE flag, so they can be identified here easily.
						if (!(value[0] & MF_MISSILE) && info->effects & FX_ROCKET)
						{
							info->effects &= ~FX_ROCKET;	// replace rocket trail
							info->effects |= FX_GRENADE;	// by grenade trail
						}

						// bounce factors are dynamic, we only set some defaults here.
						info->bouncefactor = 0.45;
						info->wallbouncefactor = 0.85;

						if (!info->IsSentient())
						{
							// Non sentient actors can be damaged but they shouldn't bleed.
							value[0] |= MF_NOBLOOD;
						}
					}
					if (zdoomflags && (value [0] & MF_STEALTH))
					{
						// STEALTH FRIEND HACK!
					}
					else if (value[0] & MF_FRIEND) 
					{
						// FRIEND (which occupies in MBF the MF_STEALTH slot)
						value[0] &= ~MF_FRIEND; // clean the slot
						value[0] |= MF_FRIENDLY; // remap the flag to its ZDoom equivalent
						// MBF friends are not blocked by monster blocking lines:
						info->flags3 |= MF3_NOBLOCKMONST;
					}
					if (value[0] & MF_TRANSLUCENT)
					{
						// TRANSLUCENT (which occupies in Boom the MF_ICECORPSE slot)
						value[0] &= ~MF_TRANSLUCENT; // clean the slot
						vchanged[2] = true; value[2] |= 2; // let the TRANSLUCxx code below handle it
					}
					if ((info->flags & MF_MISSILE) && (info->flags2 & MF2_NOTELEPORT)
						&& !(value[0] & MF_MISSILE))
					{
						// ZDoom gives missiles flags that did not exist in Doom: MF2_NOTELEPORT, 
						// MF2_IMPACT, and MF2_PCROSS. The NOTELEPORT one can be a problem since 
						// some projectile actors (those new to Doom II) were not excluded from 
						// triggering line effects and can teleport when the missile flag is removed.
						info->flags2 &= ~MF2_NOTELEPORT;
					}
					if (thingy == 1) // [SP] special handling for players - always be friendly!
					{
						value[0] |= MF_FRIENDLY;
					}
					info->flags = ActorFlags::FromInt (value[0]);
				}
				if (vchanged[1])
				{
					if (value[1] & 0x00000004)	// old BOUNCE1
					{ 	
						value[1] &= ~0x00000004;
						info->BounceFlags = BOUNCE_DoomCompat;
					}
					// Damage types that once were flags but now are not
					if (value[1] & 0x20000000)
					{
						info->DamageType = NAME_Ice;
						value[1] &= ~0x20000000;
					}
					if (value[1] & 0x10000000)
					{
						info->DamageType = NAME_Fire;
						value[1] &= ~0x10000000;
					}
					if (value[1] & 0x00000001)
					{
						info->Gravity = 1./4;
						value[1] &= ~0x00000001;
					}
					info->flags2 = ActorFlags2::FromInt (value[1]);
				}
				if (vchanged[2])
				{
					if (value[2] & 7)
					{
						hadTranslucency = true;
						if (value[2] & 1)
							info->Alpha = 0.25;
						else if (value[2] & 2)
							info->Alpha = 0.5;
						else if (value[2] & 4)
							info->Alpha = 0.75;
						info->RenderStyle = STYLE_Translucent;
						info->renderflags &= ~RF_ZDOOMTRANS;
					}
					if (value[2] & 8)
						info->renderflags |= RF_INVISIBLE;
					else
						info->renderflags &= ~RF_INVISIBLE;
				}
				DPrintf (DMSG_SPAMMY, "Bits: %d,%d (0x%08x,0x%08x)\n", info->flags.GetValue(), info->flags2.GetValue(),
													      info->flags.GetValue(), info->flags2.GetValue());
			}
			else if (stricmp (Line1, "ID #") == 0)
			{
				*ednum = (int16_t)val;
			}
		}
		else Printf (unknown_str, Line1, "Thing", thingy);
	}

	if (info != (AActor *)&dummy)
	{
		// Reset heights for things hanging from the ceiling that
		// don't specify a new height.
		if (info->flags & MF_SPAWNCEILING &&
			!hadHeight &&
			thingy <= (int)OrgHeights.Size() && thingy > 0)
		{
			info->Height = OrgHeights[thingy - 1];
			info->projectilepassheight = 0;
		}
		// If the thing's shadow changed, change its fuzziness if not already specified
		if ((info->flags ^ oldflags) & MF_SHADOW)
		{
			if (info->flags & MF_SHADOW)
			{ // changed to shadow
				if (!hadStyle)
					info->RenderStyle = STYLE_OptFuzzy;
				if (!hadTranslucency)
					info->Alpha = 0.5;
			}
			else
			{ // changed from shadow
				if (!hadStyle)
					info->RenderStyle = STYLE_Normal;
			}
		}
		// Speed could be either an int of fixed value, depending on its use
		// If this value is very large it needs to be rescaled.
		if (fabs(info->Speed) >= 256)
		{
			info->Speed /= 65536;
		}

		if (info->flags & MF_SPECIAL)
		{
			PushTouchedActor(const_cast<PClassActor *>(type));
		}

		// If MF_COUNTKILL is set, make sure the other standard monster flags are
		// set, too. And vice versa.
		if (thingy != 1) // don't mess with the player's flags
		{
			if (info->flags & MF_COUNTKILL)
			{
				info->flags2 |= MF2_PUSHWALL | MF2_MCROSS | MF2_PASSMOBJ;
				info->flags3 |= MF3_ISMONSTER;
			}
			else
			{
				info->flags2 &= ~(MF2_PUSHWALL | MF2_MCROSS);
				info->flags3 &= ~MF3_ISMONSTER;
			}
		}
		// Everything that's altered here gets the CANUSEWALLS flag, just in case
		// it calls P_Move().
		info->flags4 |= MF4_CANUSEWALLS;
		if (patchedStates)
		{
			statedef.InstallStates(type, info);
		}
	}

	return result;
}

// The only remotely useful thing Dehacked sound patches could do
// was change where the sound's name was stored. Since there is no
// real benefit to doing this, and it would be very difficult for
// me to emulate it, I have disabled them entirely.

static int PatchSound (int soundNum)
{
	int result;

	//DPrintf ("Sound %d (no longer supported)\n", soundNum);
/*
	sfxinfo_t *info, dummy;
	int offset = 0;
	if (soundNum >= 1 && soundNum <= NUMSFX) {
		info = &S_sfx[soundNum];
	} else {
		info = &dummy;
		Printf ("Sound %d out of range.\n");
	}
*/
	while ((result = GetLine ()) == 1) {
		/*
		if (!stricmp  ("Offset", Line1))
			offset = atoi (Line2);
		else CHECKKEY ("Zero/One",			info->singularity)
		else CHECKKEY ("Value",				info->priority)
		else CHECKKEY ("Zero 1",			info->link)
		else CHECKKEY ("Neg. One 1",		info->pitch)
		else CHECKKEY ("Neg. One 2",		info->volume)
		else CHECKKEY ("Zero 2",			info->data)
		else CHECKKEY ("Zero 3",			info->usefulness)
		else CHECKKEY ("Zero 4",			info->lumpnum)
		else Printf (unknown_str, Line1, "Sound", soundNum);
		*/
	}
/*
	if (offset) {
		// Calculate offset from start of sound names
		offset -= toff[dversion] + 21076;

		if (offset <= 64)			// pistol .. bfg
			offset >>= 3;
		else if (offset <= 260)		// sawup .. oof
			offset = (offset + 4) >> 3;
		else						// telept .. skeatk
			offset = (offset + 8) >> 3;

		if (offset >= 0 && offset < NUMSFX) {
			S_sfx[soundNum].name = OrgSfxNames[offset + 1];
		} else {
			Printf ("Sound name %d out of range.\n", offset + 1);
		}
	}
*/
	return result;
}

DehBits sbits[] = {
	{ "SKILL5FAST", STF_FAST }
};


static int PatchFrame (int frameNum)
{
	MBFArgs args{};
	int result;
	int tics, misc1, frame;
	FState *info, dummy;

	info = FindState (frameNum);
	if (info)
	{
		DPrintf (DMSG_SPAMMY, "Frame %d\n", frameNum);
		if (frameNum == 47)
		{ // Use original tics for S_DSGUNFLASH1
			tics = 5;
		}
		else if (frameNum == 48)
		{ // Ditto for S_DSGUNFLASH2
			tics = 4;
		}
		else
		{
			tics = info->GetTics ();
		}
		misc1 = info->GetMisc1 ();
		frame = info->GetFrame () | (info->GetFullbright() ? 0x8000 : 0);
	}
	else
	{
		info = &dummy;
		tics = misc1 = frame = 0;
		Printf ("Frame %d out of range\n", frameNum);
	}

	while ((result = GetLine ()) == 1)
	{
		int64_t val64 = atoll(Line2);
		int val = int(val64);
		size_t keylen = strlen (Line1);

		if (keylen == 8 && stricmp (Line1, "Duration") == 0)
		{
			tics = clamp (val, -1, SHRT_MAX);
		}
		else if (keylen == 9 && stricmp (Line1, "Unknown 1") == 0)
		{
			misc1 = val;
		}
		else if (keylen == 9 && stricmp (Line1, "Unknown 2") == 0)
		{
			info->Misc2 = val;
		}
		else if (keylen == 13 && stricmp (Line1, "Sprite number") == 0)
		{
			unsigned int i;

			if (val < (int)OrgSprNames.Size())
			{
				for (i = 0; i < sprites.Size(); i++)
				{
					if (memcmp (OrgSprNames[val].c, sprites[i].name, 4) == 0)
					{
						info->sprite = (int)i;
						break;
					}
				}
				if (i == sprites.Size ())
				{
					Printf ("Frame %d: Sprite %d (%s) is undefined\n",
						frameNum, val, OrgSprNames[val].c);
				}
			}
			else
			{
				Printf ("Frame %d: Sprite %d out of range\n", frameNum, val);
			}
		}
		else if (keylen == 10 && stricmp (Line1, "Next frame") == 0)
		{
			info->NextState = FindState (val);
			changedStates = true;
		}
		else if (keylen == 16 && stricmp (Line1, "Sprite subnumber") == 0)
		{
			frame = val;
		}
		else if (keylen == 5 && strnicmp(Line1, "Args", 4) == 0)
		{
			int arg = Line1[4] - '1';
			if (arg < 0 || arg >= 8)
			{
				Printf("Invalid frame arg %d\n", arg);
			}
			else
			{
				args.args[arg] = val64;
				args.argsused |= (1 << arg);
			}
		}
		else if (stricmp(Line1, "MBF21 Bits") == 0)
		{
			uint32_t value = 0;
			bool vchanged = false;

			char* strval;

			for (strval = Line2; (strval = strtok(strval, ",+| \t\f\r")); strval = NULL)
			{
				if (IsNum(strval))
				{
					value |= (unsigned long)strtoll(strval, NULL, 10);
					vchanged = true;
				}
				else
				{
					unsigned i;
					for (i = 0; i < countof(sbits); i++)
					{
						if (!stricmp(strval, sbits[i].name))
						{
							vchanged = true;
							value |= 1 << i;
							break;
						}
					}
					if (i == countof(sbits))
					{
						DPrintf(DMSG_ERROR, "Unknown bit mnemonic %s\n", strval);
					}
				}
			}
			if (vchanged)
			{
				int flags = 0;
				int mask = 0;
				for (size_t i = 0; i < countof(sbits); i++)
				{
					mask |= sbits[i].value;
					if (value & (1 << i))
					{
						flags |= sbits[i].value;
					}
				}
				info->StateFlags = (info->StateFlags & ~mask) | flags;
			}
			DPrintf(DMSG_SPAMMY, "MBF21 Bits: %d (0x%08x)\n", info->StateFlags, info->StateFlags);
		}

		else
		{
			Printf (unknown_str, Line1, "Frame", frameNum);
		}
	}

	if (info != &dummy)
	{
		info->StateFlags |= STF_DEHACKED;	// Signals the state has been modified by dehacked
		if ((unsigned)(frame & 0x7fff) > 63)
		{
			Printf("Frame %d: Subnumber must be in range [0,63]\n", frameNum);
		}
		info->Tics = tics;
		info->Misc1 = misc1;
		info->Frame = frame & 0x3f;
		if (frame & 0x8000) info->StateFlags |= STF_FULLBRIGHT;
		else info->StateFlags &= ~STF_FULLBRIGHT;
		stateargs.Insert(info, args);
	}

	return result;
}

static int PatchSprite (int sprNum)
{
	int result;
	int offset = 0;

	if ((unsigned)sprNum < OrgSprNames.Size())
	{
		DPrintf (DMSG_SPAMMY, "Sprite %d\n", sprNum);
	}
	else
	{
		Printf ("Sprite %d out of range.\n", sprNum);
		sprNum = -1;
	}

	while ((result = GetLine ()) == 1)
	{
		if (!stricmp ("Offset", Line1))
			offset = atoi (Line2);
		else Printf (unknown_str, Line1, "Sprite", sprNum);
	}

	if (offset > 0 && sprNum != -1)
	{
		// Calculate offset from beginning of sprite names.
		offset = (offset - toff[dversion] - 22044) / 8;
	
		if ((unsigned)offset < OrgSprNames.Size())
		{
			sprNum = FindSprite (OrgSprNames[sprNum].c);
			if (sprNum != -1)
				strncpy (sprites[sprNum].name, OrgSprNames[offset].c, 4);
		}
		else
		{
			Printf ("Sprite name %d out of range.\n", offset);
		}
	}

	return result;
}

static int PatchAmmo (int ammoNum)
{
	PClassActor *ammoType = NULL;
	AActor *defaultAmmo = NULL;
	int result;
	int oldclip;
	int dummy;
	int *max = &dummy;
	int *per = &dummy;

	if (ammoNum >= 0 && ammoNum < 4 && (unsigned)ammoNum <= AmmoNames.Size())
	{
		DPrintf (DMSG_SPAMMY, "Ammo %d.\n", ammoNum);
		ammoType = AmmoNames[ammoNum];
		if (ammoType != NULL)
		{
			defaultAmmo = GetDefaultByType (ammoType);
			if (defaultAmmo != NULL)
			{
				max = &defaultAmmo->IntVar(NAME_MaxAmount);
				per = &defaultAmmo->IntVar(NAME_Amount);
			}
		}
	}

	if (ammoType == NULL)
	{
		Printf ("Ammo %d out of range.\n", ammoNum);
	}

	oldclip = *per;

	while ((result = GetLine ()) == 1)
	{
			 CHECKKEY ("Max ammo", *max)
		else CHECKKEY ("Per ammo", *per)
		else Printf (unknown_str, Line1, "Ammo", ammoNum);
	}

	// Calculate the new backpack-given amounts for this ammo.
	if (ammoType != NULL)
	{
		defaultAmmo->IntVar("BackpackMaxAmount") = defaultAmmo->IntVar(NAME_MaxAmount) * 2;
		defaultAmmo->IntVar("BackpackAmount") = defaultAmmo->IntVar(NAME_Amount);
	}

	// Fix per-ammo/max-ammo amounts for descendants of the base ammo class
	if (oldclip != *per)
	{
		for (unsigned int i = 0; i < PClassActor::AllActorClasses.Size(); ++i)
		{
			PClassActor *type = PClassActor::AllActorClasses[i];

			if (type == ammoType)
				continue;

			if (type->IsDescendantOf (ammoType))
			{
				defaultAmmo = GetDefaultByType (type);
				defaultAmmo->IntVar(NAME_MaxAmount) = *max;
				defaultAmmo->IntVar(NAME_Amount) = Scale (defaultAmmo->IntVar(NAME_Amount), *per, oldclip);
			}
			else if (type->IsDescendantOf (NAME_Weapon))
			{
				auto defWeap = GetDefaultByType (type);
				if (defWeap->PointerVar<PClassActor>(NAME_AmmoType1) == ammoType)
				{
					auto &AmmoGive1 = defWeap->IntVar(NAME_AmmoGive1);
					AmmoGive1 = Scale (AmmoGive1, *per, oldclip);
				}
				if (defWeap->PointerVar<PClassActor>(NAME_AmmoType2) == ammoType)
				{
					auto &AmmoGive2 = defWeap->IntVar(NAME_AmmoGive2);
					AmmoGive2 = Scale (AmmoGive2, *per, oldclip);
				}
			}
		}
	}

	return result;
}

DehBits wbits[] = {
	{ "NOTHRUST", -1 },	// i.e. set kickback to 0
	{ "SILENT", WIF_NOALERT },
	{ "NOAUTOFIRE", WIF_NOAUTOFIRE },
	{ "FLEEMELEE", WIF_MELEEWEAPON },
	{ "AUTOSWITCHFROM", WIF_WIMPY_WEAPON },	// ugh, I wish I could change this stupid name...
	{ "NOAUTOSWITCHTO", WIF_NOAUTOSWITCHTO }
};

static int PatchWeapon (int weapNum)
{
	int result;
	PClassActor *type = nullptr;
	AActor *info = nullptr;
	bool patchedStates = false;
	FStateDefinitions statedef;

	if (weapNum >= 0 && weapNum < 9 && (unsigned)weapNum < WeaponNames.Size())
	{
		type = WeaponNames[weapNum];
		if (type != NULL)
		{
			info = GetDefaultByType (type);
			DPrintf (DMSG_SPAMMY, "Weapon %d\n", weapNum);
		}
	}

	if (type == NULL)
	{
		Printf ("Weapon %d out of range.\n", weapNum);
	}

	while ((result = GetLine ()) == 1)
	{
		int val = atoi (Line2);

		if (strlen (Line1) >= 9)
		{
			if (stricmp (Line1 + strlen (Line1) - 6, " frame") == 0)
			{
				FState *state = FindState (val);

				if (type != NULL && !patchedStates)
				{
					statedef.MakeStateDefines(type);
					patchedStates = true;
				}

				if (strnicmp (Line1, "Deselect", 8) == 0)
					statedef.SetStateLabel("Select", state);
				else if (strnicmp (Line1, "Select", 6) == 0)
					statedef.SetStateLabel("Deselect", state);
				else if (strnicmp (Line1, "Bobbing", 7) == 0)
					statedef.SetStateLabel("Ready", state);
				else if (strnicmp (Line1, "Shooting", 8) == 0)
					statedef.SetStateLabel("Fire", state);
				else if (strnicmp (Line1, "Firing", 6) == 0)
					statedef.SetStateLabel("Flash", state);
			}
			else if (stricmp (Line1, "Ammo type") == 0)
			{
				if (val < 0 || val >= 12 || (unsigned)val >= AmmoNames.Size())
				{
					val = 5;
				}
				if (info)
				{
					auto &AmmoType = info->PointerVar<PClassActor>(NAME_AmmoType1);
					AmmoType = AmmoNames[val];
					if (AmmoType != nullptr)
					{
						info->IntVar(NAME_AmmoGive1) = GetDefaultByType(AmmoType)->IntVar(NAME_Amount) * 2;
						auto &AmmoUse = info->IntVar(NAME_AmmoUse1);
						if (AmmoUse == 0)
						{
							AmmoUse = 1;
						}
					}
				}
			}
			else
			{
				Printf (unknown_str, Line1, "Weapon", weapNum);
			}
		}
		else if (stricmp (Line1, "Decal") == 0)
		{
			stripwhite (Line2);
			const FDecalTemplate *decal = DecalLibrary.GetDecalByName (Line2);
			if (decal != NULL)
			{
				if (info) info->DecalGenerator = const_cast <FDecalTemplate *>(decal);
			}
			else
			{
				Printf ("Weapon %d: Unknown decal %s\n", weapNum, Line2);
			}
		}
		else if (stricmp (Line1, "Ammo use") == 0 || stricmp (Line1, "Ammo per shot") == 0)
		{
			if (info)
			{
				info->IntVar(NAME_AmmoUse1) = val;
				info->flags6 |= MF6_INTRYMOVE;	// flag the weapon for postprocessing (reuse a flag that can't be set by external means)
			}
		}
		else if (stricmp (Line1, "Min ammo") == 0)
		{
			if (info) info->IntVar(NAME_MinSelAmmo1) = val;
		}
		else if (stricmp(Line1, "MBF21 Bits") == 0)
		{
			uint32_t value = 0;
			bool vchanged = false;

			char* strval;

			for (strval = Line2; (strval = strtok(strval, ",+| \t\f\r")); strval = NULL)
			{
				if (IsNum(strval))
				{
					value |= (unsigned long)strtoll(strval, NULL, 10);
					vchanged = true;
				}
				else
				{
					unsigned i;
					for (i = 0; i < countof(wbits); i++)
					{
						if (!stricmp(strval, wbits[i].name))
						{
							vchanged = true;
							value |= 1 << i;
							break;
						}
					}
					if (i == countof(wbits))
					{
						DPrintf(DMSG_ERROR, "Unknown bit mnemonic %s\n", strval);
					}
				}
			}
			if (vchanged)
			{
				int kickback = 100;
				int flags = 0;
				for (size_t i = 0; i < countof(wbits); i++)
				{
					if (value & (1 << i))
					{
						int val = wbits[i].value;
						if (val == -1) kickback = 0;
						else flags |= wbits[i].value;
					}
				}
				info->IntVar(NAME_Kickback) = kickback;
				info->IntVar(NAME_WeaponFlags) = flags;
			}
			DPrintf(DMSG_SPAMMY, "MBF21 Bits: %d (0x%08x)\n", info->flags.GetValue(), info->flags.GetValue());
		}

		else
		{
			Printf (unknown_str, Line1, "Weapon", weapNum);
		}
	}

	if (info)
	{
		if (info->PointerVar<PClassActor>(NAME_AmmoType1) == nullptr)
		{
			info->IntVar(NAME_AmmoUse1) = 0;
		}

		if (patchedStates)
		{
			statedef.InstallStates(type, info);
		}
	}

	return result;
}

static int SetPointer(FState *state, PFunction *sym, int frame = 0)
{
	if (sym == NULL)
	{
		state->ClearAction();
	}
	else
	{
		state->SetAction(sym->Variants[0].Implementation);

		for (unsigned int i = 0; i < MBFCodePointers.Size(); i++)
		{
			if (sym->SymbolName == MBFCodePointers[i].name)
			{
				MBFParamState newstate = { state, int(i) };
				return MBFParamStates.Push(newstate);
			}
		}
	}
	return -1;
}

static int PatchPointer (int ptrNum)
{
	int result;

	// Hack for some Boom dehacked patches that are of the form Pointer 0 (x statenumber)
	char * key;
	int indexnum;
	key=strchr(Line2, '(');
	if (key++) key=strchr(key, ' '); else key=NULL;
	if ((ptrNum == 0) && key++)
	{
		*strchr(key, ')') = '\0';
		indexnum = atoi(key);
		for (ptrNum = 0; (unsigned int) ptrNum < CodePConv.Size(); ++ptrNum)
		{
			if (CodePConv[ptrNum] == indexnum) break;
		}
		DPrintf(DMSG_SPAMMY, "Final ptrNum: %i\n", ptrNum);
	}
	// End of hack.

	// 448 Doom states with codepointers + 74 extra MBF states with codepointers = 522 total
	// Better to just use the size of the array rather than a hardcoded value.
	if (ptrNum >= 0 && (unsigned int) ptrNum < CodePConv.Size())
	{
		DPrintf (DMSG_SPAMMY, "Pointer %d\n", ptrNum);
	}
	else
	{
		Printf ("Pointer %d out of range.\n", ptrNum);
		ptrNum = -1;
	}

	while ((result = GetLine ()) == 1)
	{
		if ((unsigned)ptrNum < CodePConv.Size() && (!stricmp (Line1, "Codep Frame")))
		{
			FState *state = FindState (CodePConv[ptrNum]);
			if (state)
			{
				int index = atoi(Line2);
				if ((unsigned)(index) >= Actions.Size())
				{
					SetPointer(state, NULL);
				}
				else
				{
					SetPointer(state, Actions[index], CodePConv[ptrNum]);
				}
				DPrintf(DMSG_SPAMMY, "%s has a hacked state for pointer num %i with index %i\nLine1=%s, Line2=%s\n", 
					state->StaticFindStateOwner(state)->TypeName.GetChars(), ptrNum, index, Line1, Line2);
			}
			else
			{
				Printf ("Bad code pointer %d\n", ptrNum);
			}
		}
		else Printf (unknown_str, Line1, "Pointer", ptrNum);
	}
	return result;
}

static int PatchCheats (int dummy)
{
	int result;

	DPrintf (DMSG_NOTIFY, "Dehacked cheats support removed by request\n");

	while ((result = GetLine ()) == 1)
	{
	}
	return result;
}

static int PatchMisc (int dummy)
{
	static const struct Key keys[] = {
		{ "Initial Health",			static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,StartHealth)) },
		{ "Initial Bullets",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,StartBullets)) },
		{ "Max Health",				static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,MaxHealth)) },
		{ "Max Armor",				static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,MaxArmor)) },
		{ "Green Armor Class",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,GreenAC)) },
		{ "Blue Armor Class",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,BlueAC)) },
		{ "Max Soulsphere",			static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,MaxSoulsphere)) },
		{ "Soulsphere Health",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,SoulsphereHealth)) },
		{ "Megasphere Health",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,MegasphereHealth)) },
		{ "God Mode Health",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,GodHealth)) },
		{ "IDFA Armor",				static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,FAArmor)) },
		{ "IDFA Armor Class",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,FAAC)) },
		{ "IDKFA Armor",			static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,KFAArmor)) },
		{ "IDKFA Armor Class",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,KFAAC)) },
		{ "No Autofreeze",			static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,NoAutofreeze)) },
		{ NULL, 0 }
	};
	int result;

	DPrintf (DMSG_SPAMMY, "Misc\n");

	while ((result = GetLine()) == 1)
	{
		if (HandleKey (keys, &deh, Line1, atoi (Line2)))
		{
			if (stricmp (Line1, "BFG Cells/Shot") == 0)
			{
				deh.BFGCells = atoi (Line2);
			}
			else if (stricmp (Line1, "Rocket Explosion Style") == 0)
			{
				stripwhite (Line2);
				int style = FindStyle (Line2);
				if (style >= 0)
				{
					deh.ExplosionStyle = style;
				}
			}
			else if (stricmp (Line1, "Rocket Explosion Alpha") == 0)
			{
				deh.ExplosionAlpha = atof (Line2);
			}
			else if (stricmp (Line1, "Monsters Infight") == 0)
			{
				infighting = atoi (Line2);
			}
			else if (stricmp (Line1, "Monsters Ignore Each Other") == 0)
			{
				infighting = atoi (Line2) ? -1 : 0;
			}
			else if (strnicmp (Line1, "Powerup Color ", 14) == 0)
			{
				static const char * const names[] =
				{
					"Invulnerability",
					"Berserk",
					"Invisibility",
					"Radiation Suit",
					"Infrared",
					"Tome of Power",
					"Wings of Wrath",
					"Speed",
					"Minotaur",
					NULL
				};
				static const char *const types[] =
				{
					"PowerInvulnerable",
					"PowerStrength",
					"PowerInvisibility",
					"PowerIronFeet",
					"PowerLightAmp",
					"PowerWeaponLevel2",
					"PowerSpeed",
					"PowerMinotaur"
				};
				int i;

				for (i = 0; names[i] != NULL; ++i)
				{
					if (stricmp (Line1 + 14, names[i]) == 0)
					{
						break;
					}
				}
				if (names[i] == NULL)
				{
					Printf ("Unknown miscellaneous info %s.\n", Line1);
				}
				else
				{
					int r, g, b;
					float a;

					if (4 != sscanf (Line2, "%d %d %d %f", &r, &g, &b, &a))
					{
						Printf ("Bad powerup color description \"%s\" for %s\n", Line2, Line1);
					}
					else if (a > 0)
					{
						GetDefaultByName (types[i])->ColorVar(NAME_BlendColor) = PalEntry(
							uint8_t(clamp(a,0.f,1.f)*255.f),
							clamp(r,0,255),
							clamp(g,0,255),
							clamp(b,0,255));
					}
					else
					{
						GetDefaultByName (types[i])->ColorVar(NAME_BlendColor) = 0;
					}
				}
			}
			else
			{
				Printf ("Unknown miscellaneous info %s.\n", Line1);
			}
		}
	}

	// Update default item properties by patching the affected items
	// Note: This won't have any effect on DECORATE derivates of these items!

	auto armor = GetDefaultByName ("GreenArmor");
	if (armor!=NULL)
	{
		armor->IntVar(NAME_SaveAmount) = 100 * deh.GreenAC;
		armor->FloatVar(NAME_SavePercent) = deh.GreenAC == 1 ? 33.335 : 50;
	}
	armor = GetDefaultByName ("BlueArmor");
	if (armor!=NULL)
	{
		armor->IntVar(NAME_SaveAmount) = 100 * deh.BlueAC;
		armor->FloatVar(NAME_SavePercent) = deh.BlueAC == 1 ? 33.335 : 50;
	}

	auto barmor = GetDefaultByName ("ArmorBonus");
	if (barmor!=NULL)
	{
		barmor->IntVar("MaxSaveAmount") = deh.MaxArmor;
	}

	auto health = GetDefaultByName ("HealthBonus");
	if (health!=NULL) 
	{
		health->IntVar(NAME_MaxAmount) = 2 * deh.MaxHealth;
	}

	health = GetDefaultByName ("Soulsphere");
	if (health!=NULL)
	{
		health->IntVar(NAME_Amount) = deh.SoulsphereHealth;
		health->IntVar(NAME_MaxAmount) = deh.MaxSoulsphere;
	}

	health = GetDefaultByName ("MegasphereHealth");
	if (health!=NULL)
	{
		health->IntVar(NAME_Amount) = health->IntVar(NAME_MaxAmount) = deh.MegasphereHealth;
	}

	AActor *player = GetDefaultByName ("DoomPlayer");
	if (player != NULL)
	{
		player->health = deh.StartHealth;

		// Hm... I'm not sure that this is the right way to change this info...
		FDropItem *di = PClass::FindActor(NAME_DoomPlayer)->ActorInfo()->DropItems;
		while (di != NULL)
		{
			if (di->Name == NAME_Clip)
			{
				di->Amount = deh.StartBullets;
			}
			di = di->Next;
		}
	}


	// 0xDD means "enable infighting"
	if (infighting == 0xDD)
	{
		infighting = 1;
	}
	else if (infighting != -1)
	{
		infighting = 0;
	}

	return result;
}

static int PatchPars (int dummy)
{
	char *space, mapname[8], *moredata;
	level_info_t *info;
	int result, par;

	DPrintf (DMSG_SPAMMY, "[Pars]\n");

	while ( (result = GetLine()) ) {
		// Argh! .bex doesn't follow the same rules as .deh
		if (result == 1) {
			Printf ("Unknown key in [PARS] section: %s\n", Line1);
			continue;
		}
		if (stricmp ("par", Line1))
			return result;

		const auto FindSpace = [](char* str) -> char*
		{
			while ('\0' != *str)
			{
				if (*str != '\r' && isspace((unsigned char)*str))
				{
					return str;
				}
				++str;
			}
			return nullptr;
		};

		space = FindSpace (Line2);

		if (!space) {
			Printf ("Need data after par.\n");
			continue;
		}

		*space++ = '\0';

		while (*space && isspace(*space))
			space++;

		moredata = FindSpace (space);

		if (moredata) {
			// At least 3 items on this line, must be E?M? format
			mysnprintf (mapname, countof(mapname), "E%cM%c", *Line2, *space);
			par = atoi (moredata + 1);
		} else {
			// Only 2 items, must be MAP?? format
			mysnprintf (mapname, countof(mapname), "MAP%02d", atoi(Line2) % 100);
			par = atoi (space);
		}

		if (!(info = FindLevelInfo (mapname)) ) {
			Printf ("No map %s\n", mapname);
			continue;
		}

		info->partime = par;
		DPrintf (DMSG_SPAMMY, "Par for %s changed to %d\n", mapname, par);
	}
	return result;
}

static int PatchCodePtrs (int dummy)
{
	int result;

	DPrintf (DMSG_SPAMMY, "[CodePtr]\n");

	while ((result = GetLine()) == 1)
	{
		if (!strnicmp ("Frame", Line1, 5) && isspace(Line1[5]))
		{
			int frame = atoi (Line1 + 5);
			FState *state = FindState (frame);

			stripwhite (Line2);
			if (state == NULL)
			{
				Printf ("Frame %d out of range\n", frame);
			}
			else if (!stricmp(Line2, "NULL"))
			{
				SetPointer(state, NULL);
			}
			else
			{
				FString symname;


				if ((Line2[0] == 'A' || Line2[0] == 'a') && Line2[1] == '_')
					symname = Line2;
				else
					symname.Format("A_%s", Line2);

				// Let's consider as aliases some redundant MBF pointer
				bool ismbfcp = false;
				for (unsigned int i = 0; i < MBFCodePointers.Size(); i++)
				{
					if (!symname.CompareNoCase(MBFCodePointers[i].alias))
					{
						symname = MBFCodePointers[i].name.GetChars();
						DPrintf(DMSG_SPAMMY, "%s --> %s\n", MBFCodePointers[i].alias.GetChars(), MBFCodePointers[i].name.GetChars());
						ismbfcp = true;
						break;
					}
				}

				// This skips the action table and goes directly to the internal symbol table
				// DEH compatible functions are easy to recognize.
				PFunction *sym = dyn_cast<PFunction>(PClass::FindActor(NAME_Weapon)->FindSymbol(symname, true));
				if (sym == NULL)
				{
					Printf(TEXTCOLOR_RED "Frame %d: Unknown code pointer '%s'\n", frame, Line2);
				}
				else if (!ismbfcp)	// MBF special code pointers will produce errors here because they will receive some args and won't match the conditions here.
				{
					TArray<uint32_t> &args = sym->Variants[0].ArgFlags;
					unsigned numargs = sym->GetImplicitArgs();
					if ((sym->Variants[0].Flags & VARF_Virtual || (args.Size() > numargs && !(args[numargs] & VARF_Optional))))
					{
						Printf(TEXTCOLOR_RED "Frame %d: Incompatible code pointer '%s'\n", frame, Line2);
						sym = NULL;
					}
				}
				SetPointer(state, sym, frame);
			}
		}
	}
	return result;
}

static int PatchMusic (int dummy)
{
	int result;

	DPrintf (DMSG_SPAMMY, "[Music]\n");

	while ((result = GetLine()) == 1)
	{
		FString newname = skipwhite (Line2);
		FString keystring;
		
		keystring << "MUSIC_" << Line1;

		TableElement te = { LumpFileNum, { newname, newname, newname, newname } };
		DehStrings.Insert(keystring, te);
		DPrintf (DMSG_SPAMMY, "Music %s set to:\n%s\n", keystring.GetChars(), newname.GetChars());
	}

	return result;
}

static int PatchText (int oldSize)
{
	int newSize;
	char *oldStr;
	char *newStr;
	char *temp;
	INTBOOL good;
	int result;
	int i;

	// Skip old size, since we already know it
	temp = Line2;
	while (*temp > ' ')
		temp++;
	while (*temp && *temp <= ' ')
		temp++;

	if (*temp == 0)
	{
		Printf ("Text chunk is missing size of new string.\n");
		return 2;
	}
	newSize = atoi (temp);

	FString oldStrData, newStrData;
	oldStr = oldStrData.LockNewBuffer(oldSize + 1);
	newStr = newStrData.LockNewBuffer(newSize + 1);
	
	good = ReadChars (&oldStr, oldSize);
	good += ReadChars (&newStr, newSize);

	oldStrData.UnlockBuffer();
	newStrData.UnlockBuffer();
	
	if (!good)
	{
		Printf ("Unexpected end-of-file.\n");
		return 0;
	}

	if (includenotext)
	{
		Printf ("Skipping text chunk in included patch.\n");
		goto donewithtext;
	}

	DPrintf (DMSG_SPAMMY, "Searching for text:\n%s\n", oldStr);
	good = false;

	// Search through sprite names; they are always 4 chars
	if (oldSize == 4)
	{
		i = FindSprite (oldStr);
		if (i != -1)
		{
			strncpy (sprites[i].name, newStr, 4);
			if (strncmp ("PLAY", oldStr, 4) == 0)
			{
				strncpy (deh.PlayerSprite, newStr, 4);
			}
			for (unsigned ii = 0; ii < OrgSprNames.Size(); ii++)
			{
				if (!stricmp(OrgSprNames[ii].c, oldStr))
				{
					strcpy(OrgSprNames[ii].c, newStr);
				}
			}
			// If this sprite is used by a pickup, then the DehackedPickup sprite map
			// needs to be updated too.
			for (i = 0; (size_t)i < countof(DehSpriteMappings); ++i)
			{
				if (strncmp (DehSpriteMappings[i].Sprite, oldStr, 4) == 0)
				{
					// Found a match, so change it.
					strncpy (DehSpriteMappings[i].Sprite, newStr, 4);

					// Now shift the map's entries around so that it stays sorted.
					// This must be done because the map is scanned using a binary search.
					while (i > 0 && strncmp (DehSpriteMappings[i-1].Sprite, newStr, 4) > 0)
					{
						std::swap (DehSpriteMappings[i-1], DehSpriteMappings[i]);
						--i;
					}
					while ((size_t)i < countof(DehSpriteMappings)-1 &&
						strncmp (DehSpriteMappings[i+1].Sprite, newStr, 4) < 0)
					{
						std::swap (DehSpriteMappings[i+1], DehSpriteMappings[i]);
						++i;
					}
					break;
				}
			}
			goto donewithtext;
		}
	}

	// Search through most other texts
	const char *str;
	do
	{
		oldStrData.MergeChars(' ');
		str = EnglishStrings.MatchString(oldStr);
		if (str != NULL)
		{
			TableElement te = { LumpFileNum, { newStrData, newStrData, newStrData, newStrData } };
			DehStrings.Insert(str, te);
			EnglishStrings.Remove(str);	// remove entry so that it won't get found again by the next iteration or  by another replacement later
			good = true;
		}
	} 
	while (str != NULL);	// repeat search until the text can no longer be found

	if (!good)
	{
		DPrintf (DMSG_SPAMMY, "   (Unmatched)\n");
	}
		
donewithtext:
	// Fetch next identifier for main loop
	while ((result = GetLine ()) == 1)
		;

	return result;
}

static int PatchStrings (int dummy)
{
	int result;

	DPrintf (DMSG_SPAMMY, "[Strings]\n");

	while ((result = GetLine()) == 1)
	{
		FString holdstring;
		do
		{
			holdstring += skipwhite (Line2);
			holdstring.StripRight();
			if (holdstring.Len() > 0 && holdstring[holdstring.Len()-1] == '\\')
			{
				holdstring.Truncate(holdstring.Len()-1);
				Line2 = igets ();
			}
			else
			{
				Line2 = NULL;
			}
		} while (Line2 && *Line2);

		ReplaceSpecialChars (holdstring.LockBuffer());
		holdstring.UnlockBuffer();
		// Account for a discrepancy between Boom's and ZDoom's name for the red skull key pickup message
		const char *ll = Line1;
		if (!stricmp(ll, "GOTREDSKULL")) ll = "GOTREDSKUL";
		TableElement te = { LumpFileNum, { holdstring, holdstring, holdstring, holdstring } };
		DehStrings.Insert(ll, te);
		DPrintf (DMSG_SPAMMY, "%s set to:\n%s\n", Line1, holdstring.GetChars());
	}

	return result;
}

static int DoInclude (int dummy)
{
	char *data;
	int savedversion, savepversion, savepatchsize;
	char* savepatchfile, * savepatchpt;
	FString savepatchname;

	if (including)
	{
		Printf ("Sorry, can't nest includes\n");
		return GetLine();
	}

	if (strnicmp (Line2, "notext", 6) == 0 && Line2[6] != 0 && isspace(Line2[6]))
	{
		includenotext = true;
		Line2 = skipwhite (Line2+7);
	}

	stripwhite (Line2);
	if (*Line2 == '\"')
	{
		data = ++Line2;
		while (*data && *data != '\"')
			data++;
		*data = 0;
	}

	if (*Line2 == 0)
	{
		Printf ("Include directive is missing filename\n");
	}
	else
	{
		data = Line2;
		DPrintf (DMSG_SPAMMY, "Including %s\n", data);
		savepatchname = PatchName;
		savepatchfile = PatchFile;
		savepatchpt = PatchPt;
		savepatchsize = PatchSize;
		savedversion = dversion;
		savepversion = pversion;
		including = true;

		// Try looking for the included file in the same directory
		// as the patch before looking in the current file.
		const char *lastSlash = strrchr(savepatchname, '/');
		char *path = data;

		if (lastSlash != NULL)
		{
			size_t pathlen = lastSlash - savepatchname + strlen (data) + 2;
			path = new char[pathlen];
			strncpy (path, savepatchname, (lastSlash - savepatchname) + 1);
			strcpy (path + (lastSlash - savepatchname) + 1, data);
			if (!FileExists (path))
			{
				delete[] path;
				path = data;
			}
		}

		D_LoadDehFile(path);

		if (data != path)
		{
			delete[] path;
		}

		DPrintf (DMSG_SPAMMY, "Done with include\n");
		PatchName = savepatchname;
		PatchFile = savepatchfile;
		PatchPt = savepatchpt;
		PatchSize = savepatchsize;
		dversion = savedversion;
		pversion = savepversion;
	}

	including = false;
	includenotext = false;
	return GetLine();
}

CVAR(Int, dehload, 0, CVAR_ARCHIVE)	// Autoloading of .DEH lumps is disabled by default.

// checks if lump is a .deh or .bex file. Only lumps in the root directory are considered valid.
static bool isDehFile(int lumpnum)
{
	const char* const fullName  = fileSystem.GetFileFullName(lumpnum);
	const char* const extension = strrchr(fullName, '.');

	return NULL != extension && strchr(fullName, '/') == NULL
		&& (0 == stricmp(extension, ".deh") || 0 == stricmp(extension, ".bex"));
}

int D_LoadDehLumps(DehLumpSource source)
{
	int lastlump = 0, lumpnum, count = 0;

	while ((lumpnum = fileSystem.FindLump("DEHACKED", &lastlump)) >= 0)
	{
		const int filenum = fileSystem.GetFileContainer(lumpnum);
		
		if (FromIWAD == source && filenum > fileSystem.GetMaxIwadNum())
		{
			// No more DEHACKED lumps in IWAD
			break;
		}
		else if (FromPWADs == source && filenum <= fileSystem.GetMaxIwadNum())
		{
			// Skip DEHACKED lumps from IWAD
			continue;
		}

		count += D_LoadDehLump(lumpnum);
	}

	if (FromPWADs == source && 0 == PatchSize && dehload > 0)
	{
		// No DEH/BEX patch is loaded yet, try to find lump(s) with specific extensions

		if (dehload == 1)	// load all .DEH lumps that are found.
		{
			for (lumpnum = 0, lastlump = fileSystem.GetNumEntries(); lumpnum < lastlump; ++lumpnum)
			{
				if (isDehFile(lumpnum))
				{
					count += D_LoadDehLump(lumpnum);
				}
			}
		}
		else 	// only load the last .DEH lump that is found.
		{
			for (lumpnum = fileSystem.GetNumEntries()-1; lumpnum >=0; --lumpnum)
			{
				if (isDehFile(lumpnum))
				{
					count += D_LoadDehLump(lumpnum);
					break;
				}
			}
		}
	}

	return count;
}

bool D_LoadDehLump(int lumpnum)
{
	auto ls = LumpFileNum;
	LumpFileNum = fileSystem.GetFileContainer(lumpnum);

	PatchSize = fileSystem.FileLength(lumpnum);

	PatchName = fileSystem.GetFileFullPath(lumpnum);
	PatchFile = new char[PatchSize + 1];
	fileSystem.ReadFile(lumpnum, PatchFile);
	PatchFile[PatchSize] = '\0';		// terminate with a '\0' character
	auto res = DoDehPatch();
	LumpFileNum = ls;

	return res;
}

bool D_LoadDehFile(const char *patchfile)
{
	FileReader fr;

	if (fr.OpenFile(patchfile))
	{
		PatchSize = (int)fr.GetLength();

		PatchName = patchfile;
		PatchFile = new char[PatchSize + 1];
		fr.Read(PatchFile, PatchSize);
		fr.Close();
		PatchFile[PatchSize] = '\0';		// terminate with a '\0' character
		return DoDehPatch();
	}
	else
	{
		// Couldn't find it in the filesystem; try from a lump instead.
		int lumpnum = fileSystem.CheckNumForFullName(patchfile, true);
		if (lumpnum < 0)
		{
			// Compatibility fallback. It's just here because
			// some WAD may need it. Should be deleted if it can
			// be confirmed that nothing uses this case.
			FString filebase(ExtractFileBase(patchfile));
			lumpnum = fileSystem.CheckNumForName(filebase);
		}
		if (lumpnum >= 0)
		{
			return D_LoadDehLump(lumpnum);
		}
	}
	Printf ("Could not open DeHackEd patch \"%s\"\n", patchfile);
	return false;
}

static bool DoDehPatch()
{
	if (!batchrun) Printf("Adding dehacked patch %s\n", PatchName.GetChars());

	int cont;

	dversion = pversion = -1;
	cont = 0;
	if (0 == strncmp (PatchFile, "Patch File for DeHackEd v", 25))
	{
		if (PatchFile[25] < '3' && (PatchFile[25] < '2' || PatchFile[27] < '3'))
		{
			Printf (PRINT_DEFAULT, PRINTF_BOLD, "\"%s\" is an old and unsupported DeHackEd patch\n", PatchName.GetChars());
			PatchName = "";
			delete[] PatchFile;
			return false;
		}
		// fix for broken WolfenDoom patches which contain \0 characters in some places.
		for (int i = 0; i < PatchSize; i++)
		{
			if (PatchFile[i] == 0) PatchFile[i] = ' ';	
		}

		PatchPt = strchr (PatchFile, '\n');
		while (PatchPt != nullptr && (cont = GetLine()) == 1)
		{
				 CHECKKEY ("Doom version", dversion)
			else CHECKKEY ("Patch format", pversion)
		}
		if (!cont || dversion == -1 || pversion == -1)
		{
			Printf (PRINT_DEFAULT, PRINTF_BOLD, "\"%s\" is not a DeHackEd patch file\n", PatchName.GetChars());
			PatchName = "";
			delete[] PatchFile;
			return false;
		}
	}
	else
	{
		DPrintf (DMSG_WARNING, "Patch does not have DeHackEd signature. Assuming .bex\n");
		dversion = 19;
		pversion = 6;
		PatchPt = PatchFile;
		while ((cont = GetLine()) == 1)
		{}
	}

	if (pversion != 5 && pversion != 6)
	{
		Printf ("DeHackEd patch version is %d.\nUnexpected results may occur.\n", pversion);
	}

	if (dversion == 16)
		dversion = 0;
	else if (dversion == 17)
		dversion = 2;
	else if (dversion == 19)
		dversion = 3;
	else if (dversion == 20)
		dversion = 1;
	else if (dversion == 21)
		dversion = 4;
	else
	{
		Printf ("Patch created with unknown DOOM version.\nAssuming version 1.9.\n");
		dversion = 3;
	}

	if (!LoadDehSupp ())
	{
		Printf ("Could not load DEH support data\n");
		UnloadDehSupp ();
		PatchName = "";
		delete[] PatchFile;
		return false;
	}

	do
	{
		if (cont == 1)
		{
			Printf ("Key %s encountered out of context\n", Line1);
			cont = 0;
		}
		else if (cont == 2)
		{
			cont = HandleMode (Line1, atoi (Line2));
		}
	} while (cont);

	UnloadDehSupp ();
	PatchName = "";
	delete[] PatchFile;
	if (!batchrun) Printf ("Patch installed\n");
	return true;
}

static inline bool CompareLabel (const char *want, const uint8_t *have)
{
	return *(uint32_t *)want == *(uint32_t *)have;
}

static int DehUseCount;

static void UnloadDehSupp ()
{
	if (--DehUseCount <= 0)
	{
		VMDisassemblyDumper disasmdump(VMDisassemblyDumper::Append);

		// Handle MBF params here, before the required arrays are cleared
		for (unsigned int i=0; i < MBFParamStates.Size(); i++)
		{
			SetDehParams(MBFParamStates[i].state, MBFParamStates[i].pointer, disasmdump, &MBFParamStates[i]);
		}
		stateargs.Clear();
		MBFParamStates.Clear();
		MBFParamStates.ShrinkToFit();
		MBFCodePointers.Clear();
		MBFCodePointers.ShrinkToFit();
		// StateMap is not freed here, because if you load a second
		// dehacked patch through some means other than including it
		// in the first patch, it won't see the state information
		// that was altered by the first. So we need to keep the
		// StateMap around until all patches have been applied.
		DehUseCount = 0;
		Actions.Reset();
		OrgHeights.Reset();
		CodePConv.Reset();
		OrgSprNames.Reset();
		SoundMap.Reset();
		InfoNames.Reset();
		BitNames.Reset();
		StyleNames.Reset();
		AmmoNames.Reset();
		UnchangedSpriteNames.Reset();
	}
}

static bool LoadDehSupp ()
{
	try
	{
		// Make sure we only get the DEHSUPP lump from zdoom.pk3
		// User modifications are not supported!
		int lump = fileSystem.CheckNumForName("DEHSUPP");

		if (lump == -1)
		{
			return false;
		}

		if (fileSystem.GetFileContainer(lump) > 0)
		{
			Printf("Warning: DEHSUPP no longer supported. DEHACKED patch disabled.\n");
			return false;
		}
		bool gotnames = false;


		if (++DehUseCount > 1)
		{
			return true;
		}

		if (EnglishStrings.CountUsed() == 0)
			EnglishStrings = GStrings.GetDefaultStrings();

		UnchangedSpriteNames.Resize(sprites.Size());
		for (unsigned i = 0; i < UnchangedSpriteNames.Size(); ++i)
		{
			memcpy (&UnchangedSpriteNames[i], &sprites[i].name, 4);
		}

		FScanner sc;

		sc.OpenLumpNum(lump);
		sc.SetCMode(true);

		auto wcls = PClass::FindActor(NAME_Weapon);
		while (sc.GetString())
		{
			if (sc.Compare("ActionList"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					if (sc.Compare("NULL"))
					{
						Actions.Push(NULL);
					}
					else
					{
						// all relevant code pointers are either defined in Weapon
						// or Actor so this will find all of them.
						FString name = "A_";
						name << sc.String;
						PFunction *sym = dyn_cast<PFunction>(wcls->FindSymbol(name, true));
						if (sym == NULL)
						{
							sc.ScriptError("Unknown code pointer '%s'", sc.String);
						}
						else
						{
							TArray<uint32_t> &args = sym->Variants[0].ArgFlags;
							unsigned numargs = sym->GetImplicitArgs();
							if ((sym->Variants[0].Flags & VARF_Virtual || (args.Size() > numargs && !(args[numargs] & VARF_Optional))))
							{
								sc.ScriptMessage("Incompatible code pointer '%s'", sc.String);
							}
						}
						Actions.Push(sym);
					}
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("OrgHeights"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetNumber();
					OrgHeights.Push(sc.Number);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("CodePConv"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetNumber();
					CodePConv.Push(sc.Number);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("OrgSprNames"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					DEHSprName s;
					// initialize with zeroes
					memset(&s, 0, sizeof(s));
					if (strlen(sc.String) ==4)
					{
						s.c[0] = sc.String[0];
						s.c[1] = sc.String[1];
						s.c[2] = sc.String[2];
						s.c[3] = sc.String[3];
						s.c[4] = 0;
					}
					else
					{
						sc.ScriptError("Invalid sprite name '%s' (must be 4 characters)", sc.String);
					}
					OrgSprNames.Push(s);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("StateMap"))
			{
				bool addit = StateMap.Size() == 0;

				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					StateMapper s;
					sc.MustGetString();

					PClass *type = PClass::FindClass (sc.String);
					if (type == NULL)
					{
						sc.ScriptError ("Can't find type %s", sc.String);
					}
					else if (!type->IsDescendantOf(RUNTIME_CLASS(AActor)))
					{
						sc.ScriptError ("%s is not an actor", sc.String);
					}

					sc.MustGetStringName(",");
					sc.MustGetString();
					PClassActor *actortype = static_cast<PClassActor *>(type);
					s.State = actortype->FindState(sc.String);
					if (s.State == NULL && addit)
					{
						sc.ScriptError("Invalid state '%s' in '%s'", sc.String, type->TypeName.GetChars());
					}

					sc.MustGetStringName(",");
					sc.MustGetNumber();
					if (addit && (s.State == NULL || sc.Number < 1 || !actortype->OwnsState(s.State + sc.Number - 1)))
					{
						sc.ScriptError("Invalid state range in '%s'", type->TypeName.GetChars());
					}
					AActor *def = GetDefaultByType(type);
					
					s.StateSpan = sc.Number;
					s.Owner = actortype;
					s.OwnerIsPickup = def != NULL && (def->flags & MF_SPECIAL) != 0;
					if (addit) StateMap.Push(s);

					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("SoundMap"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					SoundMap.Push(S_FindSound(sc.String));
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("InfoNames"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					PClassActor *cls = PClass::FindActor(sc.String);
					if (cls == NULL)
					{
						sc.ScriptError("Unknown actor type '%s'", sc.String);
					}
					InfoNames.Push(cls);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("ThingBits"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					BitName bit;
					sc.MustGetNumber();
					if (sc.Number < 0 || sc.Number > 31)
					{
						sc.ScriptError("Invalid bit value %d", sc.Number);
					}
					bit.Bit = sc.Number;
					sc.MustGetStringName(",");
					sc.MustGetNumber();
					if (sc.Number < 0 || sc.Number > 2)
					{
						sc.ScriptError("Invalid flag word %d", sc.Number);
					}
					bit.WhichFlags = sc.Number;
					sc.MustGetStringName(",");
					sc.MustGetString();
					strncpy(bit.Name, sc.String, 19);
					bit.Name[19]=0;
					BitNames.Push(bit);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("RenderStyles"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					StyleName style;
					sc.MustGetNumber();
					style.Num = sc.Number;
					sc.MustGetStringName(",");
					sc.MustGetString();
					strncpy(style.Name, sc.String, 19);
					style.Name[19]=0;
					StyleNames.Push(style);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("AmmoNames"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					if (sc.Compare("NULL"))
					{
						AmmoNames.Push(NULL);
					}
					else
					{
						auto cls = PClass::FindActor(sc.String);
						if (cls == NULL || !cls->IsDescendantOf(NAME_Ammo))
						{
							sc.ScriptError("Unknown ammo type '%s'", sc.String);
						}
						AmmoNames.Push(cls);
					}
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("WeaponNames"))
			{
				WeaponNames.Clear();	// This won't be cleared by UnloadDEHSupp so we need to do it here explicitly
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					PClass *cls = PClass::FindClass(sc.String);
					if (cls == NULL || !cls->IsDescendantOf(NAME_Weapon))
					{
						sc.ScriptError("Unknown weapon type '%s'", sc.String);
					}
					WeaponNames.Push(static_cast<PClassActor *>(cls));
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("Aliases"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					CodePointerAlias temp;
					sc.MustGetString();
					temp.alias = sc.String;
					sc.MustGetStringName(",");
					sc.MustGetString();
					temp.name = sc.String;
					MBFCodePointers.Push(temp);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else
			{
				sc.ScriptError("Unknown section '%s'", sc.String);
			}

			sc.MustGetStringName(";");
		}
		return true;
	}
	catch(CRecoverableError &err)
	{
		// Don't abort if DEHSUPP loading fails. 
		// Just print the message and continue.
		Printf("%s\n", err.GetMessage());
		return false;
	}
}

void FinishDehPatch ()
{
	unsigned int touchedIndex;
	unsigned int nameindex = 0;

	// For compatibility all potentially altered actors now using A_SkullFly need to be set to the original slamming behavior.
	// Since this flag does not affect anything else let's just set it for everything, it will just be ignored by non-charging things.
	for (auto cls : InfoNames)
	{
		GetDefaultByType(cls)->flags8 |= MF8_RETARGETAFTERSLAM;
	}

	for (touchedIndex = 0; touchedIndex < TouchedActors.Size(); ++touchedIndex)
	{
		PClassActor *subclass;
		PClassActor *type = TouchedActors[touchedIndex];
		AActor *defaults1 = GetDefaultByType (type);
		if (!(defaults1->flags & MF_SPECIAL))
		{ // We only need to do this for pickups
			continue;
		}

		// Create a new class that will serve as the actual pickup
		char typeNameBuilder[32];
		// 
		auto dehtype = PClass::FindActor(NAME_DehackedPickup);
		do
		{
			// Retry until we find a free name. This is unlikely to happen but not impossible.
			mysnprintf(typeNameBuilder, countof(typeNameBuilder), "DehackedPickup%d", nameindex++);
			bool newlycreated;
			subclass = static_cast<PClassActor *>(dehtype->CreateDerivedClass(typeNameBuilder, dehtype->Size, &newlycreated));
			if (newlycreated) subclass->InitializeDefaults();
		} 
		while (subclass == nullptr);
		NewClassType(subclass);	// This needs a VM type to work as intended.

		AActor *defaults2 = GetDefaultByType (subclass);
		memcpy ((void *)defaults2, (void *)defaults1, sizeof(AActor));

		// Make a copy of the replaced class's state labels 
		FStateDefinitions statedef;
		statedef.MakeStateDefines(type);

		if (!type->IsDescendantOf(NAME_Inventory))
		{
			// If this is a hacked non-inventory item we must also copy Inventory's special states
			statedef.AddStateDefines(PClass::FindActor(NAME_Inventory)->GetStateLabels());
		}
		statedef.InstallStates(subclass, defaults2);

		// Use the DECORATE replacement feature to redirect all spawns
		// of the original class to the new one.
		PClassActor *old_replacement = type->ActorInfo()->Replacement;

		type->ActorInfo()->Replacement = subclass;
		subclass->ActorInfo()->Replacee = type;
		// If this actor was already replaced by another actor, copy that
		// replacement over to this item.
		if (old_replacement != NULL)
		{
			subclass->ActorInfo()->Replacement = old_replacement;
		}

		DPrintf (DMSG_NOTIFY, "%s replaces %s\n", subclass->TypeName.GetChars(), type->TypeName.GetChars());
	}

	// To avoid errors, flag all potentially touched states for use in weapons.
	if (changedStates)
	{
		for (auto &s : StateMap)
		{
			for (auto i = 0; i < s.StateSpan; i++)
			{
				s.State[i].UseFlags |= SUF_WEAPON;
			}
		}
	}
	// Now that all Dehacked patches have been processed, it's okay to free StateMap.
	StateMap.Reset();
	TouchedActors.Reset();
	EnglishStrings.Clear();
	GStrings.SetOverrideStrings(std::move(DehStrings));

	// Now it gets nasty: We have to fiddle around with the weapons' ammo use info to make Doom's original
	// ammo consumption work as intended.

	auto wcls = PClass::FindActor(NAME_Weapon);
	for(unsigned i = 0; i < WeaponNames.Size(); i++)
	{
		auto weap = GetDefaultByType(WeaponNames[i]);
		bool found = false;
		if (weap->flags6 & MF6_INTRYMOVE)
		{
			// Weapon sets an explicit amount of ammo to use so we won't need any special processing here
			weap->flags6 &= ~MF6_INTRYMOVE;
		}
		else
		{
			weap->BoolVar(NAME_bDehAmmo) = true;
			weap->IntVar(NAME_AmmoUse1) = 0;
			// to allow proper checks in CheckAmmo we have to find the first attack pointer in the Fire sequence
			// and set its default ammo use as the weapon's AmmoUse1.

			TMap<FState*, bool> StateVisited;

			FState *state = WeaponNames[i]->FindState(NAME_Fire);
			while (state != NULL)
			{
				bool *check = StateVisited.CheckKey(state);
				if (check != NULL && *check)
				{
					break;	// State has already been checked so we reached a loop
				}
				StateVisited[state] = true;
				for(unsigned j = 0; AmmoPerAttacks[j].func != NAME_None; j++)
				{
					if (AmmoPerAttacks[j].ptr == nullptr)
					{
						auto p = dyn_cast<PFunction>(wcls->FindSymbol(AmmoPerAttacks[j].func, true));
						if (p != nullptr) AmmoPerAttacks[j].ptr = p->Variants[0].Implementation;
					}
					if (state->ActionFunc == AmmoPerAttacks[j].ptr)
					{
						found = true;
						int use = AmmoPerAttacks[j].ammocount;
						if (use < 0) use = deh.BFGCells;
						weap->IntVar(NAME_AmmoUse1) = use;
						break;
					}
				}
				if (found) break;
				state = state->GetNextState();
			}
		}
	}
	WeaponNames.Clear();
	WeaponNames.ShrinkToFit();
}

DEFINE_ACTION_FUNCTION(ADehackedPickup, DetermineType)
{
	PARAM_SELF_PROLOGUE(AActor);

	// Look at the actor's current sprite to determine what kind of
	// item to pretend to me.
	int min = 0;
	int max = countof(DehSpriteMappings) - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lex = memcmp (DehSpriteMappings[mid].Sprite, sprites[self->sprite].name, 4);
		if (lex == 0)
		{
			ACTION_RETURN_POINTER(PClass::FindActor(DehSpriteMappings[mid].ClassName));
		}
		else if (lex < 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	ACTION_RETURN_POINTER(nullptr);
}

// Handling the flags is not as trivial as it seems...

struct FlagHandler
{
	void (*setter)(AActor*);
	void (*clearer)(AActor*);
	bool (*checker)(AActor*);
};

// cut down on boilerplate
#define F(flag) { [](AActor* a) { a->flags |= flag; }, [](AActor* a) { a->flags &= ~flag; }, [](AActor* a)->bool { return a->flags & flag; } }
#define F2(flag) { [](AActor* a) { a->flags2 |= flag; }, [](AActor* a) { a->flags2 &= ~flag; }, [](AActor* a)->bool { return a->flags2 & flag; } }
#define F3(flag) { [](AActor* a) { a->flags3 |= flag; }, [](AActor* a) { a->flags3 &= ~flag; }, [](AActor* a)->bool { return a->flags3 & flag; } }
#define F4(flag) { [](AActor* a) { a->flags4 |= flag; }, [](AActor* a) { a->flags4 &= ~flag; }, [](AActor* a)->bool { return a->flags4 & flag; } }
#define F6(flag) { [](AActor* a) { a->flags6 |= flag; }, [](AActor* a) { a->flags6 &= ~flag; }, [](AActor* a)->bool { return a->flags6 & flag; } }
#define F8(flag) { [](AActor* a) { a->flags8 |= flag; }, [](AActor* a) { a->flags8 &= ~flag; }, [](AActor* a)->bool { return a->flags8 & flag; } }
#define DEPF(flag) { [](AActor* a) { HandleDeprecatedFlags(a, nullptr, true, flag); }, [](AActor* a) { HandleDeprecatedFlags(a, nullptr, false, flag); }, [](AActor* a)->bool { return CheckDeprecatedFlags(a, nullptr, flag); } }

void SetNoSector(AActor* a) 
{ 
	a->UnlinkFromWorld(nullptr); 
	a->flags |= MF_NOSECTOR; 
	a->LinkToWorld(nullptr); 
}

void ClearNoSector(AActor* a)
{
	a->UnlinkFromWorld(nullptr);
	a->flags &= ~MF_NOSECTOR;
	a->LinkToWorld(nullptr);
}

void SetNoBlockmap(AActor* a)
{
	a->UnlinkFromWorld(nullptr);
	a->flags |= MF_NOBLOCKMAP;
	a->LinkToWorld(nullptr);
}

void ClearNoBlockmap(AActor* a)
{
	a->UnlinkFromWorld(nullptr);
	a->flags &= ~MF_NOBLOCKMAP;
	a->LinkToWorld(nullptr);
}

void SetCountkill(AActor* a)
{
	if (a->CountsAsKill() && a->health > 0) a->Level->total_monsters--;
	a->flags |= MF_COUNTKILL;
	if (a->CountsAsKill() && a->health > 0) a->Level->total_monsters++;
}

void ClearCountkill(AActor* a)
{
	if (a->CountsAsKill() && a->health > 0) a->Level->total_monsters--;
	a->flags &= ~MF_COUNTKILL;
	if (a->CountsAsKill() && a->health > 0) a->Level->total_monsters++;
}

void SetCountitem(AActor* a)
{
	if (!(a->flags & MF_COUNTITEM))
	{
		a->flags |= MF_COUNTITEM;
		a->Level->total_items++;
	}
}

void ClearCountitem(AActor* a)
{
	if (a->flags & MF_COUNTITEM)
	{
		a->flags |= MF_COUNTITEM;
		a->Level->total_items--;
	}
}

void SetFriendly(AActor* a)
{
	if (a->CountsAsKill() && a->health > 0) a->Level->total_monsters--;
	a->flags |= MF_FRIENDLY;
	if (a->CountsAsKill() && a->health > 0) a->Level->total_monsters++;
}

void ClearFriendly(AActor* a)
{
	if (a->CountsAsKill() && a->health > 0) a->Level->total_monsters--;
	a->flags &= ~MF_FRIENDLY;
	if (a->CountsAsKill() && a->health > 0) a->Level->total_monsters++;
}

void SetFullVol(AActor* a)
{
	a->flags8 |= MF8_FULLVOLSEE; 
	a->flags3 |= MF3_FULLVOLDEATH;
}

void ClearFullVol(AActor* a)
{
	a->flags8 &= ~MF8_FULLVOLSEE;
	a->flags3 &= ~MF3_FULLVOLDEATH;
}

void SetTranslucent(AActor* a)
{
	a->RenderStyle = STYLE_Translucent;
	a->Alpha = 0.5;
	a->renderflags &= ~RF_ZDOOMTRANS;
}

void ClearTranslucent(AActor* a)
{
	a->RenderStyle = STYLE_Normal;
	a->Alpha = 1;
	a->renderflags &= ~RF_ZDOOMTRANS;
}

bool CheckTranslucent(AActor* a)
{
	// This is solely for MBF21. It will treat all objects with ZDOOMTRANS as non-translucent to ensure consistent results.
	// This also means that all translucency changes via Dehacked need to clear this flag.
	return !(a->renderflags & RF_ZDOOMTRANS) && a->Alpha < 1 - FLT_EPSILON;
}

constexpr int t0 = 0;
constexpr int t1 = TRANSLATION(TRANSLATION_Standard, 0);
constexpr int t2 = TRANSLATION(TRANSLATION_Standard, 1);
constexpr int t3 = TRANSLATION(TRANSLATION_Standard, 2);

void SetTranslation1(AActor* a)
{
	if (a->Translation == t2 || a->Translation == t3) a->Translation = t3;
	else a->Translation = t1;
}

void ClearTranslation1(AActor* a)
{
	if (a->Translation == t3 || a->Translation == t2) a->Translation = t2;
	else a->Translation = t0;
}

bool CheckTranslation1(AActor* a)
{
	return a->Translation == t1 || a->Translation == t3;
}

void SetTranslation2(AActor* a)
{
	if (a->Translation == t1 || a->Translation == t3) a->Translation = t3;
	else a->Translation = t2;
}

void ClearTranslation2(AActor* a)
{
	if (a->Translation == t3 || a->Translation == t1) a->Translation = t1;
	else a->Translation = t0;
}

bool CheckTranslation2(AActor* a)
{
	return a->Translation == t2 || a->Translation == t3;
}

// Bounces is very complex...
void SetBounces(AActor* info)
{
	info->flags6 |= MF6_VULNERABLE;
	info->flags3 |= MF3_NOBLOCKMONST;
	info->flags4 |= (MF4_FORCERADIUSDMG | MF4_DONTHARMCLASS);
	info->effects &= ~FX_ROCKET;	// disable rocket trail if set.

	if (info->flags & MF_MISSILE) info->BounceFlags = BOUNCE_Classic | BOUNCE_DEH;
	else info->BounceFlags = BOUNCE_Grenade | BOUNCE_DEH;

}

void ClearBounces(AActor* info)
{
	info->flags6 &= ~MF6_VULNERABLE;
	info->flags3 &= ~MF3_NOBLOCKMONST;
	info->flags4 &= ~(MF4_FORCERADIUSDMG | MF4_DONTHARMCLASS);
	info->BounceFlags = 0;
}

// The missile flag affects the bouncing mode.
void SetMissile(AActor* info)
{
	info->flags |= MF_MISSILE;
	if (info->BounceFlags & BOUNCE_DEH) info->BounceFlags = BOUNCE_Classic | BOUNCE_DEH;
}

void ClearMissile(AActor* info)
{
	info->flags &= ~MF_MISSILE;
	if (info->BounceFlags & BOUNCE_DEH) info->BounceFlags = BOUNCE_Grenade | BOUNCE_DEH;
}

static FlagHandler flag1handlers[32] = {
	F(MF_SPECIAL),
	F(MF_SOLID),
	F(MF_SHOOTABLE),
	{ SetNoSector, ClearNoSector, [](AActor* a)->bool { return a->flags & MF_NOSECTOR; } },
	{ SetNoBlockmap, ClearNoBlockmap, [](AActor* a)->bool { return a->flags & MF_NOBLOCKMAP; } },
	F(MF_AMBUSH),
	F(MF_JUSTHIT),
	F(MF_JUSTATTACKED),
	F(MF_SPAWNCEILING),
	F(MF_NOGRAVITY),
	F(MF_DROPOFF),
	F(MF_PICKUP),
	F(MF_NOCLIP),
	F(MF_SLIDE),
	F(MF_FLOAT),
	F(MF_TELEPORT),
	{ SetMissile, ClearMissile, [](AActor* a)->bool { return a->flags & MF_MISSILE; } },
	F(MF_DROPPED),
	F(MF_SHADOW),
	F(MF_NOBLOOD),
	F(MF_CORPSE),
	F(MF_INFLOAT),
	{ SetCountkill, ClearCountkill, [](AActor* a)->bool { return a->flags & MF_COUNTKILL; } },
	{ SetCountitem, ClearCountitem, [](AActor* a)->bool { return a->flags & MF_COUNTITEM; } },
	F(MF_SKULLFLY),
	F(MF_NOTDMATCH),
	{ SetTranslation1, ClearTranslation1, CheckTranslation1 },
	{ SetTranslation2, ClearTranslation2, CheckTranslation2 },
	F6(MF6_TOUCHY),
	{ SetBounces, ClearBounces, [](AActor* a)->bool { return a->BounceFlags & BOUNCE_DEH; } },
	F(MF_FRIENDLY),
	{ SetTranslucent, ClearTranslucent, CheckTranslucent }
};

static FlagHandler flag2handlers[32] = {
	DEPF(DEPF_LOWGRAVITY),
	DEPF(DEPF_SHORTMISSILERANGE),
	F3(MF3_NOTARGET),
	F3(MF3_NORADIUSDMG),
	F4(MF4_FORCERADIUSDMG),
	DEPF(DEPF_HIGHERMPROB),
	F4(MF4_MISSILEMORE),
	F4(MF4_QUICKTORETALIATE),
	DEPF(DEPF_LONGMELEERANGE),
	F2(MF2_BOSS),
	F8(MF8_MAP07BOSS1),
	F8(MF8_MAP07BOSS2),
	F8(MF8_E1M8BOSS),
	F8(MF8_E2M8BOSS),
	F8(MF8_E3M8BOSS),
	F8(MF8_E4M6BOSS),
	F8(MF8_E4M8BOSS),
	F2(MF2_RIP),
	{ SetFullVol, ClearFullVol, [](AActor* a)->bool { return a->flags8 & MF8_FULLVOLSEE; } }, // checking one of the two flags should suffice here.
};


//
// A_JumpIfFlagsSet
// Jumps to a state if caller has the specified thing flags set.
//   args[0]: State to jump to
//   args[1]: Standard Flag(s) to check
//   args[2]: MBF21 Flag(s) to check
//
DEFINE_ACTION_FUNCTION(AActor, MBF21_JumpIfFlagsSet)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_POINTER(tstate, FState);
	PARAM_INT(flags);
	PARAM_INT(flags2);

	for (int i = 0; i < 32; i++)
	{
		if (flags & (1 << i) && flag1handlers[i].checker)
		{
			if (!flag1handlers[i].checker(self)) return 0;
		}
		if (flags2 & (1 << i) && flag2handlers[i].checker)
		{
			if (!flag2handlers[i].checker(self)) return 0;
		}
	}
	self->SetState(tstate);
	return 0;
}

//
// A_AddFlags
// Adds the specified thing flags to the caller.
//   args[0]: Standard Flag(s) to add
//   args[1]: MBF21 Flag(s) to add
//
DEFINE_ACTION_FUNCTION(AActor, MBF21_AddFlags)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(flags);
	PARAM_INT(flags2);

	for (int i = 0; i < 32; i++)
	{
		if (flags & (1 << i) && flag1handlers[i].setter)
		{
			flag1handlers[i].setter(self);
		}
		if (flags2 & (1 << i) && flag2handlers[i].setter)
		{
			flag2handlers[i].setter(self);
		}
	}
	return 0;
}

//
// A_RemoveFlags
// Removes the specified thing flags from the caller.
//   args[0]: Flag(s) to remove
//   args[1]: MBF21 Flag(s) to remove
//
DEFINE_ACTION_FUNCTION(AActor, MBF21_RemoveFlags)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(flags);
	PARAM_INT(flags2);

	for (int i = 0; i < 32; i++)
	{
		if (flags & (1 << i) && flag1handlers[i].clearer)
		{
			flag1handlers[i].clearer(self);
		}
		if (flags2 & (1 << i) && flag2handlers[i].clearer)
		{
			flag2handlers[i].clearer(self);
		}
	}
	return 0;
}

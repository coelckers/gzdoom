//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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
// DESCRIPTION:
//		Player related stuff.
//		Bobbing POV/weapon, movement.
//		Pending weapon.
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

#include "templates.h"
#include "doomdef.h"
#include "d_event.h"
#include "p_local.h"
#include "doomstat.h"
#include "s_sound.h"
#include "gi.h"
#include "m_random.h"
#include "p_pspr.h"
#include "p_enemy.h"
#include "a_sharedglobal.h"
#include "a_keys.h"
#include "filesystem.h"
#include "cmdlib.h"
#include "sbar.h"
#include "intermission/intermission.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "d_net.h"
#include "serializer_doom.h"
#include "serialize_obj.h"
#include "d_player.h"
#include "r_utility.h"
#include "p_blockmap.h"
#include "a_morph.h"
#include "p_spec.h"
#include "vm.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "p_acs.h"
#include "events.h"
#include "g_game.h"
#include "v_video.h"
#include "gstrings.h"
#include "s_music.h"
#include "d_main.h"

static FRandom pr_skullpop ("SkullPop");

// [SP] Allows respawn in single player
CVAR(Bool, sv_singleplayerrespawn, false, CVAR_SERVERINFO | CVAR_CHEAT)

// Variables for prediction
CVAR (Bool, cl_noprediction, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, cl_predict_specials, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Float, cl_predict_lerpscale, 0.05f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	P_PredictionLerpReset();
}
CUSTOM_CVAR(Float, cl_predict_lerpthreshold, 2.00f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0.1f)
		self = 0.1f;
	P_PredictionLerpReset();
}

ColorSetList ColorSets;
PainFlashList PainFlashes;

// [Nash] FOV cvar setting
CUSTOM_CVAR(Float, fov, 90.f, CVAR_ARCHIVE | CVAR_USERINFO | CVAR_NOINITCALL)
{
	player_t *p = &players[consoleplayer];
	p->SetFOV(fov);
}

struct PredictPos
{
	int gametic;
	DVector3 pos;
	DRotator angles;
} static PredictionLerpFrom, PredictionLerpResult, PredictionLast;
static int PredictionLerptics;

static player_t PredictionPlayerBackup;
static AActor *PredictionActor;
static TArray<uint8_t> PredictionActorBackupArray;
static TArray<AActor *> PredictionSectorListBackup;

static TArray<sector_t *> PredictionTouchingSectorsBackup;
static TArray<msecnode_t *> PredictionTouchingSectors_sprev_Backup;

static TArray<sector_t *> PredictionRenderSectorsBackup;
static TArray<msecnode_t *> PredictionRenderSectors_sprev_Backup;

static TArray<sector_t *> PredictionPortalSectorsBackup;
static TArray<msecnode_t *> PredictionPortalSectors_sprev_Backup;

static TArray<FLinePortal *> PredictionPortalLinesBackup;
static TArray<portnode_t *> PredictionPortalLines_sprev_Backup;

// [GRB] Custom player classes
TArray<FPlayerClass> PlayerClasses;

FPlayerClass::FPlayerClass ()
{
	Type = NULL;
	Flags = 0;
}

FPlayerClass::FPlayerClass (const FPlayerClass &other)
{
	Type = other.Type;
	Flags = other.Flags;
	Skins = other.Skins;
}

FPlayerClass::~FPlayerClass ()
{
}

bool FPlayerClass::CheckSkin (int skin)
{
	for (unsigned int i = 0; i < Skins.Size (); i++)
	{
		if (Skins[i] == skin)
			return true;
	}

	return false;
}

DEFINE_ACTION_FUNCTION(FPlayerClass, CheckSkin)
{
	PARAM_SELF_STRUCT_PROLOGUE(FPlayerClass);
	PARAM_INT(skin);
	ACTION_RETURN_BOOL(self->CheckSkin(skin));
}

// Custom pronouns
bool FPronouns::FromString(FString str)
{
	TArray<FString> split;
	str.Split(split, "/", FString::TOK_SKIPEMPTY);

	split.Clamp(6);
	if (split.Size() == 0) split.Push(str);

	for (unsigned int i = 0; i < split.Size(); i++)
	{
		split[i].StripLeftRight();
		split[i].ToLower();
	}

	if (split.Size() == 6)
	{
		for (unsigned int i = 0; i < 6; i++) pronoun[i] = split[i];
		BuildString();
		return true;
	}

	// Incomplete - try to find matching default
	for (const struct FPronouns &defPron : DefaultPronouns)
	{
		if (defPron[0] != split[0]) continue;

		for (unsigned int i = 1; i < split.Size(); i++)
			if (defPron[i] != split[i]) return false;

		// Matches completely - copy it
		for (unsigned int i = 0; i < 6; i++) pronoun[i] = defPron[i];
		BuildString();
		return true;
	}

	return false;
}

void FPronouns::BuildString()
{
	string.Format("%s/%s/%s/%s/%s/%s",
		pronoun[0].GetChars(), pronoun[1].GetChars(), pronoun[2].GetChars(),
		pronoun[3].GetChars(), pronoun[4].GetChars(), pronoun[5].GetChars()
	);
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetPronouns)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_STRING(self->userinfo.GetPronouns());
}

//===========================================================================
//
// GetDisplayName
//
//===========================================================================

FString GetPrintableDisplayName(PClassActor *cls)
{ 
	// Fixme; This needs a decent way to access the string table without creating a mess.
	// [RH] ????
	return cls->GetDisplayName();
}

bool ValidatePlayerClass(PClassActor *ti, const char *name)
{
	if (ti == NULL)
	{
		Printf("Unknown player class '%s'\n", name);
		return false;
	}
	else if (!ti->IsDescendantOf(NAME_PlayerPawn))
	{
		Printf("Invalid player class '%s'\n", name);
		return false;
	}
	else if (ti->GetDisplayName().IsEmpty())
	{
		Printf ("Missing displayname for player class '%s'\n", name);
		return false;
	}
	return true;
}

void SetupPlayerClasses ()
{
	FPlayerClass newclass;

	PlayerClasses.Clear();
	for (unsigned i = 0; i < gameinfo.PlayerClasses.Size(); i++)
	{
		PClassActor *cls = PClass::FindActor(gameinfo.PlayerClasses[i]);
		if (ValidatePlayerClass(cls, gameinfo.PlayerClasses[i].GetChars()))
		{
			newclass.Flags = 0;
			newclass.Type = cls;
			if ((GetDefaultByType(cls)->flags6 & MF6_NOMENU))
			{
				newclass.Flags |= PCF_NOMENU;
			}
			PlayerClasses.Push(newclass);
		}
	}
}

CCMD (playerclasses)
{
	for (unsigned int i = 0; i < PlayerClasses.Size (); i++)
	{
		Printf ("%3d: Class = %s, Name = %s\n", i,
			PlayerClasses[i].Type->TypeName.GetChars(),
			PlayerClasses[i].Type->GetDisplayName().GetChars());
	}
}

//
// Movement.
//

player_t::~player_t()
{
	DestroyPSprites();
}

void player_t::CopyFrom(player_t &p, bool copyPSP)
{
	mo = p.mo;
	playerstate = p.playerstate;
	cmd = p.cmd;
	original_cmd = p.original_cmd;
	original_oldbuttons = p.original_oldbuttons;
	// Intentionally not copying userinfo!
	cls = p.cls;
	DesiredFOV = p.DesiredFOV;
	FOV = p.FOV;
	viewz = p.viewz;
	viewheight = p.viewheight;
	deltaviewheight = p.deltaviewheight;
	bob = p.bob;
	Vel = p.Vel;
	centering = p.centering;
	turnticks = p.turnticks;
	attackdown = p.attackdown;
	usedown = p.usedown;
	oldbuttons = p.oldbuttons;
	health = p.health;
	inventorytics = p.inventorytics;
	CurrentPlayerClass = p.CurrentPlayerClass;
	memcpy(frags, &p.frags, sizeof(frags));
	fragcount = p.fragcount;
	lastkilltime = p.lastkilltime;
	multicount = p.multicount;
	spreecount = p.spreecount;
	WeaponState = p.WeaponState;
	ReadyWeapon = p.ReadyWeapon;
	PendingWeapon = p.PendingWeapon;
	cheats = p.cheats;
	timefreezer = p.timefreezer;
	refire = p.refire;
	inconsistant = p.inconsistant;
	waiting = p.waiting;
	killcount = p.killcount;
	itemcount = p.itemcount;
	secretcount = p.secretcount;
	damagecount = p.damagecount;
	bonuscount = p.bonuscount;
	hazardcount = p.hazardcount;
	hazardtype = p.hazardtype;
	hazardinterval = p.hazardinterval;
	poisoncount = p.poisoncount;
	poisontype = p.poisontype;
	poisonpaintype = p.poisonpaintype;
	poisoner = p.poisoner;
	attacker = p.attacker;
	extralight = p.extralight;
	fixedcolormap = p.fixedcolormap;
	fixedlightlevel = p.fixedlightlevel;
	morphTics = p.morphTics;
	MorphedPlayerClass = p.MorphedPlayerClass;
	MorphStyle = p.MorphStyle;
	MorphExitFlash = p.MorphExitFlash;
	PremorphWeapon = p.PremorphWeapon;
	chickenPeck = p.chickenPeck;
	jumpTics = p.jumpTics;
	onground = p.onground;
	respawn_time = p.respawn_time;
	camera = p.camera;
	air_finished = p.air_finished;
	LastDamageType = p.LastDamageType;
	Bot = p.Bot;
	settings_controller = p.settings_controller;
	BlendR = p.BlendR;
	BlendG = p.BlendG;
	BlendB = p.BlendB;
	BlendA = p.BlendA;
	LogText = p.LogText;
	MinPitch = p.MinPitch;
	MaxPitch = p.MaxPitch;
	crouching = p.crouching;
	crouchdir = p.crouchdir;
	crouchfactor = p.crouchfactor;
	crouchoffset = p.crouchoffset;
	crouchviewdelta = p.crouchviewdelta;
	weapons = p.weapons;
	ConversationNPC = p.ConversationNPC;
	ConversationPC = p.ConversationPC;
	ConversationNPCAngle = p.ConversationNPCAngle;
	ConversationFaceTalker = p.ConversationFaceTalker;
	MUSINFOactor = p.MUSINFOactor;
	MUSINFOtics = p.MUSINFOtics;
	SoundClass = p.SoundClass;
	if (copyPSP)
	{
		// This needs to transfer ownership completely.
		psprites = p.psprites;
		p.psprites = nullptr;
	}
}

size_t player_t::PropagateMark()
{
	GC::Mark(mo);
	GC::Mark(poisoner);
	GC::Mark(attacker);
	GC::Mark(camera);
	GC::Mark(Bot);
	GC::Mark(ReadyWeapon);
	GC::Mark(ConversationNPC);
	GC::Mark(ConversationPC);
	GC::Mark(MUSINFOactor);
	GC::Mark(PremorphWeapon);
	GC::Mark(psprites);
	if (PendingWeapon != WP_NOCHANGE)
	{
		GC::Mark(PendingWeapon);
	}
	return sizeof(*this);
}

void player_t::SetLogNumber (int num)
{
	char lumpname[26];
	int lumpnum;

	// First look up TXT_LOGTEXT%d in the string table
	mysnprintf(lumpname, countof(lumpname), "$TXT_LOGTEXT%d", num);
	auto text = GStrings[lumpname+1];
	if (text)
	{
		SetLogText(lumpname);	// set the label, not the content, so that a language change can be picked up.
		return;
	}

	mysnprintf (lumpname, countof(lumpname), "LOG%d", num);
	lumpnum = fileSystem.CheckNumForName (lumpname);
	if (lumpnum != -1)
	{
		auto fn = fileSystem.GetFileContainer(lumpnum);
		auto wadname = fileSystem.GetResourceFileName(fn);
		if (!stricmp(wadname, "STRIFE0.WAD") || !stricmp(wadname, "STRIFE1.WAD") || !stricmp(wadname, "SVE.WAD"))
		{
			// If this is an original IWAD text, try looking up its lower priority string version first.

			mysnprintf(lumpname, countof(lumpname), "$TXT_ILOG%d", num);
			auto text = GStrings[lumpname + 1];
			if (text)
			{
				SetLogText(lumpname);	// set the label, not the content, so that a language change can be picked up.
				return;
			}
		}

		auto lump = fileSystem.ReadFile(lumpnum);
		SetLogText (lump.GetString());
	}
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, SetLogNumber)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_INT(log);
	self->SetLogNumber(log);
	return 0;
}

void player_t::SetLogText (const char *text)
{
	 LogText = text;

	if (mo && mo->CheckLocalView())
	{
		// Print log text to console
		Printf(PRINT_NONOTIFY, TEXTCOLOR_GOLD "%s\n", LogText[0] == '$' ? GStrings(text + 1) : text);
	}
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, SetLogText)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_STRING(log);
	self->SetLogText(log);
	return 0;
}

void player_t::SetSubtitle(int num, FSoundID soundid)
{
	char lumpname[36];

	if (gameinfo.flags & GI_SHAREWARE) return;	// Subtitles are only for the full game.

	// Do we have a subtitle for this log entry's voice file?
	mysnprintf(lumpname, countof(lumpname), "$TXT_SUB_LOG%d", num);
	auto text = GStrings.GetLanguageString(lumpname+1, FStringTable::default_table);
	if (text != nullptr)
	{
		SubtitleText = lumpname;
		int sl = soundid == 0 ? 7000 : std::max<int>(7000, S_GetMSLength(soundid));
		SubtitleCounter = sl * TICRATE / 1000;
	}
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, SetSubtitleNumber)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_INT(log);
	PARAM_SOUND(soundid);
	self->SetSubtitle(log, soundid);
	return 0;
}



int player_t::GetSpawnClass()
{
	const PClass * type = PlayerClasses[CurrentPlayerClass].Type;
	return GetDefaultByType(type)->IntVar(NAME_SpawnMask);
}

// [Nash] Set FOV
void player_t::SetFOV(float fov)
{
	player_t *p = &players[consoleplayer];
	if (p != nullptr && p->mo != nullptr)
	{
		if (dmflags & DF_NO_FOV)
		{
			if (consoleplayer == Net_Arbitrator)
			{
				Net_WriteByte(DEM_MYFOV);
			}
			else
			{
				Printf("A setting controller has disabled FOV changes.\n");
				return;
			}
		}
		else
		{
			Net_WriteByte(DEM_MYFOV);
		}
		Net_WriteFloat(clamp<float>(fov, 5.f, 179.f));
	}
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, SetFOV)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_FLOAT(fov);
	self->SetFOV((float)fov);
	return 0;
}

//===========================================================================
//
// EnumColorsets
//
// Only used by the menu so it doesn't really matter that it's a bit
// inefficient.
//
//===========================================================================

static int intcmp(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

void EnumColorSets(PClassActor *cls, TArray<int> *out)
{
	TArray<int> deleteds;

	out->Clear();
	for (int i = ColorSets.Size() - 1; i >= 0; i--)
	{
		if (std::get<0>(ColorSets[i])->IsAncestorOf(cls))
		{
			int v = std::get<1>(ColorSets[i]);
			if (out->Find(v) == out->Size() && deleteds.Find(v) == deleteds.Size())
			{
				if (std::get<2>(ColorSets[i]).Name == NAME_None) deleteds.Push(v);
				else out->Push(v);
			}
		}
	}
	qsort(&(*out)[0], out->Size(), sizeof(int), intcmp);
}

DEFINE_ACTION_FUNCTION(FPlayerClass, EnumColorSets)
{
	PARAM_SELF_STRUCT_PROLOGUE(FPlayerClass);
	PARAM_POINTER(out, TArray<int>);
	EnumColorSets(self->Type, out);
	return 0;
}

//==========================================================================
//
//
//==========================================================================

FPlayerColorSet *GetColorSet(PClassActor *cls, int setnum)
{
	for (int i = ColorSets.Size() - 1; i >= 0; i--)
	{
		if (std::get<1>(ColorSets[i]) == setnum &&
			std::get<0>(ColorSets[i])->IsAncestorOf(cls))
		{
			auto c = &std::get<2>(ColorSets[i]);
			return c->Name != NAME_None ? c : nullptr;
		}
	}
	return nullptr;
}

DEFINE_ACTION_FUNCTION(FPlayerClass, GetColorSetName)
{
	PARAM_SELF_STRUCT_PROLOGUE(FPlayerClass);
	PARAM_INT(setnum);
	auto p = GetColorSet(self->Type, setnum);
	ACTION_RETURN_INT(p ? p->Name.GetIndex() : 0);
}

//==========================================================================
//
//
//==========================================================================

static int GetPainFlash(AActor *info, int type)
{
	// go backwards through the list and return the first item with a 
	// matching damage type for an ancestor of our class. 
	// This will always return the best fit because any parent class
	// must be processed before its children.
	for (int i = PainFlashes.Size() - 1; i >= 0; i--)
	{
		if (std::get<1>(PainFlashes[i]) == ENamedName(type) &&
			std::get<0>(PainFlashes[i])->IsAncestorOf(info->GetClass()))
		{
			return std::get<2>(PainFlashes[i]);
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(APlayerPawn, GetPainFlashForType, GetPainFlash)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(type);
	ACTION_RETURN_INT(GetPainFlash(self, type));
}

//===========================================================================
//
// player_t :: SendPitchLimits
//
// Ask the local player's renderer what pitch restrictions should be imposed
// and let everybody know. Only sends data for the consoleplayer, since the
// local player is the only one our data is valid for.
//
//===========================================================================

EXTERN_CVAR(Float, maxviewpitch)
EXTERN_CVAR(Bool, cl_oldfreelooklimit);


static int GetSoftPitch(bool down)
{
	int MAX_DN_ANGLE = MIN(56, (int)maxviewpitch); // Max looking down angle
	int MAX_UP_ANGLE = MIN(32, (int)maxviewpitch); // Max looking up angle
	return (down ? MAX_DN_ANGLE : ((cl_oldfreelooklimit) ? MAX_UP_ANGLE : MAX_DN_ANGLE));
}

void player_t::SendPitchLimits() const
{
	if (this - players == consoleplayer)
	{
		int uppitch, downpitch;

		if (!V_IsHardwareRenderer())
		{
			uppitch = GetSoftPitch(false);
			downpitch = GetSoftPitch(true);
		}
		else
		{
			uppitch = downpitch = (int)maxviewpitch;
		}

		Net_WriteByte(DEM_SETPITCHLIMIT);
		Net_WriteByte(uppitch);
		Net_WriteByte(downpitch);
	}
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, SendPitchLimits)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	self->SendPitchLimits();
	return 0;
}


bool player_t::HasWeaponsInSlot(int slot) const
{
	for (int i = 0; i < weapons.SlotSize(slot); i++)
	{
		PClassActor *weap = weapons.GetWeapon(slot, i);
		if (weap != NULL && mo->FindInventory(weap)) return true;
	}
	return false;
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, HasWeaponsInSlot)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_INT(slot);
	ACTION_RETURN_BOOL(self->HasWeaponsInSlot(slot));
}


bool player_t::Resurrect()
{
	if (mo == nullptr || mo->IsKindOf(NAME_PlayerChunk)) return false;
	mo->Revive();
	playerstate = PST_LIVE;
	health = mo->health = mo->GetDefault()->health;
	viewheight = DefaultViewHeight();
	mo->renderflags &= ~RF_INVISIBLE;
	mo->Height = mo->GetDefault()->Height;
	mo->radius = mo->GetDefault()->radius;
	mo->special1 = 0;	// required for the Hexen fighter's fist attack. 
								// This gets set by AActor::Die as flag for the wimpy death and must be reset here.
	mo->SetState(mo->SpawnState);
	int pnum = mo->Level->PlayerNum(this);
	if (!(mo->flags2 & MF2_DONTTRANSLATE))
	{
		mo->Translation = TRANSLATION(TRANSLATION_Players, uint8_t(pnum));
	}
	if (ReadyWeapon != nullptr)
	{
		PendingWeapon = ReadyWeapon;
		P_BringUpWeapon(this);
	}

	if (morphTics)
	{
		P_UnmorphActor(mo, mo);
	}

	// player is now alive.
	// fire E_PlayerRespawned and start the ACS SCRIPT_Respawn.
	mo->Level->localEventManager->PlayerRespawned(pnum);
	mo->Level->Behaviors.StartTypedScripts(SCRIPT_Respawn, mo, true);
	return true;
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, Resurrect)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_BOOL(self->Resurrect());
}


DEFINE_ACTION_FUNCTION(_PlayerInfo, GetUserName)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_STRING(self->userinfo.GetName());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetNeverSwitch)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_BOOL(self->userinfo.GetNeverSwitch());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetClassicFlight)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_BOOL(self->userinfo.GetClassicFlight());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetColor)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_INT(self->userinfo.GetColor());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetColorSet)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_INT(self->userinfo.GetColorSet());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetPlayerClassNum)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_INT(self->userinfo.GetPlayerClassNum());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetSkin)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_INT(self->userinfo.GetSkin());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetGender)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_INT(self->userinfo.GetGender());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetAutoaim)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_FLOAT(self->userinfo.GetAutoaim());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetTeam)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_INT(self->userinfo.GetTeam());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetNoAutostartMap)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_INT(self->userinfo.GetNoAutostartMap());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetWBobSpeed)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_FLOAT(self->userinfo.GetWBobSpeed());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetWBobFire)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_FLOAT(self->userinfo.GetWBobFire());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetMoveBob)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_FLOAT(self->userinfo.GetMoveBob());
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetStillBob)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	ACTION_RETURN_FLOAT(self->userinfo.GetStillBob());
}


//===========================================================================
//
// 
//
//===========================================================================

static int SetupCrouchSprite(AActor *self, int crouchsprite)
{
	// Check whether a PWADs normal sprite is to be combined with the base WADs
	// crouch sprite. In such a case the sprites normally don't match and it is
	// best to disable the crouch sprite.
	if (crouchsprite > 0)
	{
		// This assumes that player sprites always exist in rotated form and
		// that the front view is always a separate sprite. So far this is
		// true for anything that exists.
		FString normspritename = sprites[self->SpawnState->sprite].name;
		FString crouchspritename = sprites[crouchsprite].name;

		int spritenorm = fileSystem.CheckNumForName(normspritename + "A1", ns_sprites);
		if (spritenorm == -1)
		{
			spritenorm = fileSystem.CheckNumForName(normspritename + "A0", ns_sprites);
		}

		int spritecrouch = fileSystem.CheckNumForName(crouchspritename + "A1", ns_sprites);
		if (spritecrouch == -1)
		{
			spritecrouch = fileSystem.CheckNumForName(crouchspritename + "A0", ns_sprites);
		}

		if (spritenorm == -1 || spritecrouch == -1)
		{
			// Sprites do not exist so it is best to disable the crouch sprite.
			return false;
		}

		int wadnorm = fileSystem.GetFileContainer(spritenorm);
		int wadcrouch = fileSystem.GetFileContainer(spritenorm);

		if (wadnorm > fileSystem.GetMaxIwadNum() && wadcrouch <= fileSystem.GetMaxIwadNum())
		{
			// Question: Add an option / disable crouching or do what?
			return false;
		}
	}
	return true;
}

DEFINE_ACTION_FUNCTION_NATIVE(APlayerPawn, SetupCrouchSprite, SetupCrouchSprite)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(crouchsprite);
	ACTION_RETURN_INT(SetupCrouchSprite(self, crouchsprite));
}

//===========================================================================
//
// Animations
//
//===========================================================================

void PlayIdle (AActor *player)
{
	IFVIRTUALPTRNAME(player, NAME_PlayerPawn, PlayIdle)
	{
		VMValue params[1] = { (DObject*)player };
		VMCall(func, params, 1, nullptr, 0);
	}
}

//===========================================================================
//
// A_PlayerScream
//
// try to find the appropriate death sound and use suitable 
// replacements if necessary
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PlayerScream)
{
	PARAM_SELF_PROLOGUE(AActor);

	int sound = 0;
	int chan = CHAN_VOICE;

	if (self->player == NULL || self->DeathSound != 0)
	{
		if (self->DeathSound != 0)
		{
			S_Sound (self, CHAN_VOICE, 0, self->DeathSound, 1, ATTN_NORM);
		}
		else
		{
			S_Sound (self, CHAN_VOICE, 0, "*death", 1, ATTN_NORM);
		}
		return 0;
	}

	// Handle the different player death screams
	if ((((self->Level->flags >> 15) | (dmflags)) &
		(DF_FORCE_FALLINGZD | DF_FORCE_FALLINGHX)) &&
		self->Vel.Z <= -39)
	{
		sound = S_FindSkinnedSound (self, "*splat");
		chan = CHAN_BODY;
	}

	if (!sound && self->special1<10)
	{ // Wimpy death sound
		sound = S_FindSkinnedSoundEx (self, "*wimpydeath", self->player->LastDamageType.GetChars());
	}
	if (!sound && self->health <= -50)
	{
		if (self->health > -100)
		{ // Crazy death sound
			sound = S_FindSkinnedSoundEx (self, "*crazydeath", self->player->LastDamageType.GetChars());
		}
		if (!sound)
		{ // Extreme death sound
			sound = S_FindSkinnedSoundEx (self, "*xdeath", self->player->LastDamageType.GetChars());
			if (!sound)
			{
				sound = S_FindSkinnedSoundEx (self, "*gibbed", self->player->LastDamageType.GetChars());
				chan = CHAN_BODY;
			}
		}
	}
	if (!sound)
	{ // Normal death sound
		sound = S_FindSkinnedSoundEx (self, "*death", self->player->LastDamageType.GetChars());
	}

	if (chan != CHAN_VOICE)
	{
		for (int i = 0; i < 8; ++i)
		{ // Stop most playing sounds from this player.
		  // This is mainly to stop *land from messing up *splat.
			if (i != CHAN_WEAPON && i != CHAN_VOICE)
			{
				S_StopSound (self, i);
			}
		}
	}
	S_Sound (self, chan, 0, sound, 1, ATTN_NORM);
	return 0;
}


//===========================================================================
//
// P_CheckPlayerSprites
//
// Here's the place where crouching sprites are handled.
// R_ProjectSprite() calls this for any players.
//
//===========================================================================

void P_CheckPlayerSprite(AActor *actor, int &spritenum, DVector2 &scale)
{
	player_t *player = actor->player;
	int crouchspriteno;

	if (player->userinfo.GetSkin() != 0 && !(actor->flags4 & MF4_NOSKIN))
	{
		// Convert from default scale to skin scale.
		DVector2 defscale = actor->GetDefault()->Scale;
		scale.X *= Skins[player->userinfo.GetSkin()].Scale.X / defscale.X;
		scale.Y *= Skins[player->userinfo.GetSkin()].Scale.Y / defscale.Y;
	}

	// Set the crouch sprite?
	if (player->mo == actor && player->crouchfactor < 0.75)
	{
		int crouchsprite = player->mo->IntVar(NAME_crouchsprite);
		if (spritenum == actor->SpawnState->sprite || spritenum == crouchsprite) 
		{
			crouchspriteno = crouchsprite;
		}
		else if (!(actor->flags4 & MF4_NOSKIN) &&
				(spritenum == Skins[player->userinfo.GetSkin()].sprite ||
				 spritenum == Skins[player->userinfo.GetSkin()].crouchsprite))
		{
			crouchspriteno = Skins[player->userinfo.GetSkin()].crouchsprite;
		}
		else
		{ // no sprite -> squash the existing one
			crouchspriteno = -1;
		}

		if (crouchspriteno > 0) 
		{
			spritenum = crouchspriteno;
		}
		else if (player->playerstate != PST_DEAD && player->crouchfactor < 0.75)
		{
			scale.Y *= 0.5;
		}
	}
}

CUSTOM_CVAR (Float, sv_aircontrol, 0.00390625f, CVAR_SERVERINFO|CVAR_NOSAVE|CVAR_NOINITCALL)
{
	primaryLevel->aircontrol = self;
	primaryLevel->AirControlChanged ();
}

//==========================================================================
//
// P_FallingDamage
//
//==========================================================================

void P_FallingDamage (AActor *actor)
{
	int damagestyle;
	int damage;
	double vel;

	damagestyle = ((actor->Level->flags >> 15) | (dmflags)) &
		(DF_FORCE_FALLINGZD | DF_FORCE_FALLINGHX);

	if (damagestyle == 0)
		return;
		
	if (actor->floorsector->Flags & SECF_NOFALLINGDAMAGE)
		return;

	vel = fabs(actor->Vel.Z);

	// Since Hexen falling damage is stronger than ZDoom's, it takes
	// precedence. ZDoom falling damage may not be as strong, but it
	// gets felt sooner.

	switch (damagestyle)
	{
	case DF_FORCE_FALLINGHX:		// Hexen falling damage
		if (vel <= 23)
		{ // Not fast enough to hurt
			return;
		}
		if (vel >= 63)
		{ // automatic death
			damage = TELEFRAG_DAMAGE;
		}
		else
		{
			vel *= (16. / 23);
			damage = int((vel * vel) / 10 - 24);
			if (actor->Vel.Z > -39 && damage > actor->health
				&& actor->health != 1)
			{ // No-death threshold
				damage = actor->health-1;
			}
		}
		break;
	
	case DF_FORCE_FALLINGZD:		// ZDoom falling damage
		if (vel <= 19)
		{ // Not fast enough to hurt
			return;
		}
		if (vel >= 84)
		{ // automatic death
			damage = TELEFRAG_DAMAGE;
		}
		else
		{
			damage = int((vel*vel*(11 / 128.) - 30) / 2);
			if (damage < 1)
			{
				damage = 1;
			}
		}
		break;

	case DF_FORCE_FALLINGST:		// Strife falling damage
		if (vel <= 20)
		{ // Not fast enough to hurt
			return;
		}
		// The minimum amount of damage you take from falling in Strife
		// is 52. Ouch!
		damage = int(vel / (25000./65536.));
		break;

	default:
		return;
	}

	if (actor->player)
	{
		S_Sound (actor, CHAN_AUTO, 0, "*land", 1, ATTN_NORM);
		P_NoiseAlert (actor, actor, true);
		if (damage >= TELEFRAG_DAMAGE && ((actor->player->cheats & (CF_GODMODE | CF_BUDDHA) ||
			(actor->FindInventory(PClass::FindActor(NAME_PowerBuddha), true) != nullptr))))
		{
			damage = TELEFRAG_DAMAGE - 1;
		}
	}
	P_DamageMobj (actor, NULL, NULL, damage, NAME_Falling);
}

//----------------------------------------------------------------------------
//
// PROC P_CheckMusicChange
//
//----------------------------------------------------------------------------

void P_CheckMusicChange(player_t *player)
{
	// MUSINFO stuff
	if (player->MUSINFOtics >= 0 && player->MUSINFOactor != NULL)
	{
		if (--player->MUSINFOtics < 0)
		{
			if (player == player->mo->Level->GetConsolePlayer())
			{
				if (player->MUSINFOactor->args[0] != 0)
				{
					FName *music = player->MUSINFOactor->Level->info->MusicMap.CheckKey(player->MUSINFOactor->args[0]);

					if (music != NULL)
					{
						S_ChangeMusic(music->GetChars(), player->MUSINFOactor->args[1]);
					}
				}
				else
				{
					S_ChangeMusic("*");
				}
			}
			DPrintf(DMSG_NOTIFY, "MUSINFO change for player %d to %d\n", (int)player->mo->Level->PlayerNum(player), player->MUSINFOactor->args[0]);
		}
	}
}

DEFINE_ACTION_FUNCTION(APlayerPawn, CheckMusicChange)
{
	PARAM_SELF_PROLOGUE(AActor);
	P_CheckMusicChange(self->player);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC P_CheckEnviroment
//
//----------------------------------------------------------------------------

void P_CheckEnvironment(player_t *player)
{
	P_PlayerOnSpecial3DFloor(player);
	P_PlayerInSpecialSector(player);

	if (!player->mo->isAbove(player->mo->Sector->floorplane.ZatPoint(player->mo)) ||
		player->mo->waterlevel)
	{
		// Player must be touching the floor
		P_PlayerOnSpecialFlat(player, P_GetThingFloorType(player->mo));
	}
	if (player->mo->Vel.Z <= -player->mo->FloatVar(NAME_FallingScreamMinSpeed) &&
		player->mo->Vel.Z >= -player->mo->FloatVar(NAME_FallingScreamMaxSpeed) && !player->morphTics &&
		player->mo->waterlevel == 0)
	{
		int id = S_FindSkinnedSound(player->mo, "*falling");
		if (id != 0 && !S_IsActorPlayingSomething(player->mo, CHAN_VOICE, id))
		{
			S_Sound(player->mo, CHAN_VOICE, 0, id, 1, ATTN_NORM);
		}
	}
}

DEFINE_ACTION_FUNCTION(APlayerPawn, CheckEnvironment)
{
	PARAM_SELF_PROLOGUE(AActor);
	P_CheckEnvironment(self->player);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC P_CheckUse
//
//----------------------------------------------------------------------------

void P_CheckUse(player_t *player)
{
	// check for use
	if (player->cmd.ucmd.buttons & BT_USE)
	{
		if (!player->usedown)
		{
			player->usedown = true;
			if (!P_TalkFacing(player->mo))
			{
				P_UseLines(player);
			}
		}
	}
	else
	{
		player->usedown = false;
	}
}


DEFINE_ACTION_FUNCTION(APlayerPawn, CheckUse)
{
	PARAM_SELF_PROLOGUE(AActor);
	P_CheckUse(self->player);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC P_PlayerThink
//
//----------------------------------------------------------------------------

void P_PlayerThink (player_t *player)
{
	ticcmd_t *cmd = &player->cmd;

	if (player->mo == NULL)
	{
		I_Error ("No player %td start\n", player - players + 1);
	}

	if (player->SubtitleCounter > 0)
	{
		player->SubtitleCounter--;
	}

	// Bots do not think in freeze mode.
	if (player->mo->Level->isFrozen() && player->Bot != nullptr)
	{
		return;
	}

	if (debugfile && !(player->cheats & CF_PREDICTING))
	{
		fprintf (debugfile, "tic %d for pl %d: (%f, %f, %f, %f) b:%02x p:%d y:%d f:%d s:%d u:%d\n",
			gametic, (int)(player-players), player->mo->X(), player->mo->Y(), player->mo->Z(),
			player->mo->Angles.Yaw.Degrees, player->cmd.ucmd.buttons,
			player->cmd.ucmd.pitch, player->cmd.ucmd.yaw, player->cmd.ucmd.forwardmove,
			player->cmd.ucmd.sidemove, player->cmd.ucmd.upmove);
	}

	// Make unmodified copies for ACS's GetPlayerInput.
	player->original_oldbuttons = player->original_cmd.buttons;
	player->original_cmd = cmd->ucmd;
	// Don't interpolate the view for more than one tic
	player->cheats &= ~CF_INTERPVIEW;
	player->mo->FloatVar("prevBob") = player->bob;

	IFVIRTUALPTRNAME(player->mo, NAME_PlayerPawn, PlayerThink)
	{
		VMValue param = player->mo;
		VMCall(func, &param, 1, nullptr, 0);
	}
}

void P_PredictionLerpReset()
{
	PredictionLerptics = PredictionLast.gametic = PredictionLerpFrom.gametic = PredictionLerpResult.gametic = 0;
}

bool P_LerpCalculate(AActor *pmo, PredictPos from, PredictPos to, PredictPos &result, float scale)
{
	DVector3 vecFrom = from.pos;
	DVector3 vecTo = to.pos;
	DVector3 vecResult;
	vecResult = vecTo - vecFrom;
	vecResult *= scale;
	vecResult = vecResult + vecFrom;
	DVector3 delta = vecResult - vecTo;

	result.pos = pmo->Vec3Offset(vecResult - to.pos);

	// As a fail safe, assume extrapolation is the threshold.
	return (delta.LengthSquared() > cl_predict_lerpthreshold && scale <= 1.00f);
}

template<class nodetype, class linktype>
void BackupNodeList(AActor *act, nodetype *head, nodetype *linktype::*otherlist, TArray<nodetype*, nodetype*> &prevbackup, TArray<linktype *, linktype *> &otherbackup)
{
	// The ordering of the touching_sectorlist needs to remain unchanged
	// Also store a copy of all previous sector_thinglist nodes
	prevbackup.Clear();
	otherbackup.Clear();

	for (auto mnode = head; mnode != nullptr; mnode = mnode->m_tnext)
	{
		otherbackup.Push(mnode->m_sector);

		for (auto snode = mnode->m_sector->*otherlist; snode; snode = snode->m_snext)
		{
			if (snode->m_thing == act)
			{
				prevbackup.Push(snode->m_sprev);
				break;
			}
		}
	}
}

template<class nodetype, class linktype>
nodetype *RestoreNodeList(AActor *act, nodetype *head, nodetype *linktype::*otherlist, TArray<nodetype*, nodetype*> &prevbackup, TArray<linktype *, linktype *> &otherbackup)
{
	// Destroy old refrences
	nodetype *node = head;
	while (node)
	{
		node->m_thing = NULL;
		node = node->m_tnext;
	}

	// Make the sector_list match the player's touching_sectorlist before it got predicted.
	P_DelSeclist(head, otherlist);
	head = NULL;
	for (auto i = otherbackup.Size(); i-- > 0;)
	{
		head = P_AddSecnode(otherbackup[i], act, head, otherbackup[i]->*otherlist);
	}
	//act->touching_sectorlist = ctx.sector_list;	// Attach to thing
	//ctx.sector_list = NULL;		// clear for next time

	// In the old code this block never executed because of the commented-out NULL assignment above. Needs to be checked
	node = head;
	while (node)
	{
		if (node->m_thing == NULL)
		{
			if (node == head)
				head = node->m_tnext;
			node = P_DelSecnode(node, otherlist);
		}
		else
		{
			node = node->m_tnext;
		}
	}

	nodetype *snode;

	// Restore sector thinglist order
	for (auto i = otherbackup.Size(); i-- > 0;)
	{
		// If we were already the head node, then nothing needs to change
		if (prevbackup[i] == NULL)
			continue;

		for (snode = otherbackup[i]->*otherlist; snode; snode = snode->m_snext)
		{
			if (snode->m_thing == act)
			{
				if (snode->m_sprev)
					snode->m_sprev->m_snext = snode->m_snext;
				else
					snode->m_sector->*otherlist = snode->m_snext;
				if (snode->m_snext)
					snode->m_snext->m_sprev = snode->m_sprev;

				snode->m_sprev = prevbackup[i];

				// At the moment, we don't exist in the list anymore, but we do know what our previous node is, so we set its current m_snext->m_sprev to us.
				if (snode->m_sprev->m_snext)
					snode->m_sprev->m_snext->m_sprev = snode;
				snode->m_snext = snode->m_sprev->m_snext;
				snode->m_sprev->m_snext = snode;
				break;
			}
		}
	}
	return head;
}

void P_PredictPlayer (player_t *player)
{
	int maxtic;

	if (cl_noprediction ||
		singletics ||
		demoplayback ||
		player->mo == NULL ||
		player != player->mo->Level->GetConsolePlayer() ||
		player->playerstate != PST_LIVE ||
		!netgame ||
		/*player->morphTics ||*/
		(player->cheats & CF_PREDICTING))
	{
		return;
	}

	maxtic = maketic;

	if (gametic == maxtic)
	{
		return;
	}

	// Save original values for restoration later
	PredictionPlayerBackup.CopyFrom(*player, false);

	auto act = player->mo;
	PredictionActor = player->mo;
	PredictionActorBackupArray.Resize(act->GetClass()->Size);
	memcpy(PredictionActorBackupArray.Data(), &act->snext, act->GetClass()->Size - ((uint8_t *)&act->snext - (uint8_t *)act));

	act->flags &= ~MF_PICKUP;
	act->flags2 &= ~MF2_PUSHWALL;
	act->renderflags &= ~RF_NOINTERPOLATEVIEW;
	player->cheats |= CF_PREDICTING;

	BackupNodeList(act, act->touching_sectorlist, &sector_t::touching_thinglist, PredictionTouchingSectors_sprev_Backup, PredictionTouchingSectorsBackup);
	BackupNodeList(act, act->touching_rendersectors, &sector_t::touching_renderthings, PredictionRenderSectors_sprev_Backup, PredictionRenderSectorsBackup);
	BackupNodeList(act, act->touching_sectorportallist, &sector_t::sectorportal_thinglist, PredictionPortalSectors_sprev_Backup, PredictionPortalSectorsBackup);
	BackupNodeList(act, act->touching_lineportallist, &FLinePortal::lineportal_thinglist, PredictionPortalLines_sprev_Backup, PredictionPortalLinesBackup);

	// Keep an ordered list off all actors in the linked sector.
	PredictionSectorListBackup.Clear();
	if (!(act->flags & MF_NOSECTOR))
	{
		AActor *link = act->Sector->thinglist;
		
		while (link != NULL)
		{
			PredictionSectorListBackup.Push(link);
			link = link->snext;
		}
	}

	// Blockmap ordering also needs to stay the same, so unlink the block nodes
	// without releasing them. (They will be used again in P_UnpredictPlayer).
	FBlockNode *block = act->BlockNode;

	while (block != NULL)
	{
		if (block->NextActor != NULL)
		{
			block->NextActor->PrevActor = block->PrevActor;
		}
		*(block->PrevActor) = block->NextActor;
		block = block->NextBlock;
	}
	act->BlockNode = NULL;

	// Values too small to be usable for lerping can be considered "off".
	bool CanLerp = (!(cl_predict_lerpscale < 0.01f) && (ticdup == 1)), DoLerp = false, NoInterpolateOld = R_GetViewInterpolationStatus();
	for (int i = gametic; i < maxtic; ++i)
	{
		if (!NoInterpolateOld)
			R_RebuildViewInterpolation(player);

		player->cmd = localcmds[i % LOCALCMDTICS];
		P_PlayerThink (player);
		player->mo->Tick ();

		if (CanLerp && PredictionLast.gametic > 0 && i == PredictionLast.gametic && !NoInterpolateOld)
		{
			// Z is not compared as lifts will alter this with no apparent change
			// Make lerping less picky by only testing whole units
			DoLerp = (int)PredictionLast.pos.X != (int)player->mo->X() || (int)PredictionLast.pos.Y != (int)player->mo->Y();

			// Aditional Debug information
			if (developer >= DMSG_NOTIFY && DoLerp)
			{
				DPrintf(DMSG_NOTIFY, "Lerp! Ltic (%d) && Ptic (%d) | Lx (%f) && Px (%f) | Ly (%f) && Py (%f)\n",
					PredictionLast.gametic, i,
					(PredictionLast.pos.X), (player->mo->X()),
					(PredictionLast.pos.Y), (player->mo->Y()));
			}
		}
	}

	if (CanLerp)
	{
		if (NoInterpolateOld)
			P_PredictionLerpReset();

		else if (DoLerp)
		{
			// If lerping is already in effect, use the previous camera postion so the view doesn't suddenly snap
			PredictionLerpFrom = (PredictionLerptics == 0) ? PredictionLast : PredictionLerpResult;
			PredictionLerptics = 1;
		}

		PredictionLast.gametic = maxtic - 1;
		PredictionLast.pos = player->mo->Pos();
		//PredictionLast.portalgroup = player->mo->Sector->PortalGroup;

		if (PredictionLerptics > 0)
		{
			if (PredictionLerpFrom.gametic > 0 &&
				P_LerpCalculate(player->mo, PredictionLerpFrom, PredictionLast, PredictionLerpResult, (float)PredictionLerptics * cl_predict_lerpscale))
			{
				PredictionLerptics++;
				player->mo->SetXYZ(PredictionLerpResult.pos);
			}
			else
			{
				PredictionLerptics = 0;
			}
		}
	}
}

void P_UnPredictPlayer ()
{
	player_t *player = &players[consoleplayer];

	if (player->cheats & CF_PREDICTING)
	{
		unsigned int i;
		AActor *act = player->mo;

		if (act != PredictionActor)
		{
			// Q: Can this happen? If yes, can we continue?
		}

		AActor *savedcamera = player->camera;

		auto &actInvSel = act->PointerVar<AActor*>(NAME_InvSel);
		auto InvSel = actInvSel;
		int inventorytics = player->inventorytics;
		const bool settings_controller = player->settings_controller;

		player->CopyFrom(PredictionPlayerBackup, false);

		player->settings_controller = settings_controller;
		// Restore the camera instead of using the backup's copy, because spynext/prev
		// could cause it to change during prediction.
		player->camera = savedcamera;

		FLinkContext ctx;
		// Unlink from all list, including those which are not being handled by UnlinkFromWorld.
		auto sectorportal_list = act->touching_sectorportallist;
		auto lineportal_list = act->touching_lineportallist;
		act->touching_sectorportallist = nullptr;
		act->touching_lineportallist = nullptr;

		act->UnlinkFromWorld(&ctx);
		memcpy(&act->snext, PredictionActorBackupArray.Data(), PredictionActorBackupArray.Size() - ((uint8_t *)&act->snext - (uint8_t *)act));

		// The blockmap ordering needs to remain unchanged, too.
		// Restore sector links and refrences.
		// [ED850] This is somewhat of a duplicate of LinkToWorld(), but we need to keep every thing the same,
		// otherwise we end up fixing bugs in blockmap logic (i.e undefined behaviour with polyobject collisions),
		// which we really don't want to do here.
		if (!(act->flags & MF_NOSECTOR))
		{
			sector_t *sec = act->Sector;
			AActor *me, *next;
			AActor **link;// , **prev;

			// The thinglist is just a pointer chain. We are restoring the exact same things, so we can NULL the head safely
			sec->thinglist = NULL;

			for (i = PredictionSectorListBackup.Size(); i-- > 0;)
			{
				me = PredictionSectorListBackup[i];
				link = &sec->thinglist;
				next = *link;
				if ((me->snext = next))
					next->sprev = &me->snext;
				me->sprev = link;
				*link = me;
			}

			act->touching_sectorlist = RestoreNodeList(act, ctx.sector_list, &sector_t::touching_thinglist, PredictionTouchingSectors_sprev_Backup, PredictionTouchingSectorsBackup);
			act->touching_rendersectors = RestoreNodeList(act, ctx.render_list, &sector_t::touching_renderthings, PredictionRenderSectors_sprev_Backup, PredictionRenderSectorsBackup);
			act->touching_sectorportallist = RestoreNodeList(act, sectorportal_list, &sector_t::sectorportal_thinglist, PredictionPortalSectors_sprev_Backup, PredictionPortalSectorsBackup);
			act->touching_lineportallist = RestoreNodeList(act, lineportal_list, &FLinePortal::lineportal_thinglist, PredictionPortalLines_sprev_Backup, PredictionPortalLinesBackup);
		}

		// Now fix the pointers in the blocknode chain
		FBlockNode *block = act->BlockNode;

		while (block != NULL)
		{
			*(block->PrevActor) = block;
			if (block->NextActor != NULL)
			{
				block->NextActor->PrevActor = &block->NextActor;
			}
			block = block->NextBlock;
		}

		actInvSel = InvSel;
		player->inventorytics = inventorytics;
	}
}

void player_t::Serialize(FSerializer &arc)
{
	FString skinname;

	arc("class", cls)
		("mo", mo)
		("camera", camera)
		("playerstate", playerstate)
		("cmd", cmd);

	if (arc.isReading())
	{
		ReadUserInfo(arc, userinfo, skinname);
	}
	else
	{
		WriteUserInfo(arc, userinfo);
	}

	arc("desiredfov", DesiredFOV)
		("fov", FOV)
		("viewz", viewz)
		("viewheight", viewheight)
		("deltaviewheight", deltaviewheight)
		("bob", bob)
		("vel", Vel)
		("centering", centering)
		("health", health)
		("inventorytics", inventorytics)
		("fragcount", fragcount)
		("spreecount", spreecount)
		("multicount", multicount)
		("lastkilltime", lastkilltime)
		("readyweapon", ReadyWeapon)
		("pendingweapon", PendingWeapon)
		("cheats", cheats)
		("refire", refire)
		("inconsistant", inconsistant)
		("killcount", killcount)
		("itemcount", itemcount)
		("secretcount", secretcount)
		("damagecount", damagecount)
		("bonuscount", bonuscount)
		("hazardcount", hazardcount)
		("poisoncount", poisoncount)
		("poisoner", poisoner)
		("attacker", attacker)
		("extralight", extralight)
		("fixedcolormap", fixedcolormap)
		("fixedlightlevel", fixedlightlevel)
		("morphTics", morphTics)
		("morphedplayerclass", MorphedPlayerClass)
		("morphstyle", MorphStyle)
		("morphexitflash", MorphExitFlash)
		("premorphweapon", PremorphWeapon)
		("chickenpeck", chickenPeck)
		("jumptics", jumpTics)
		("respawntime", respawn_time)
		("airfinished", air_finished)
		("turnticks", turnticks)
		("oldbuttons", oldbuttons)
		("hazardtype", hazardtype)
		("hazardinterval", hazardinterval)
		("bot", Bot)
		("blendr", BlendR)
		("blendg", BlendG)
		("blendb", BlendB)
		("blenda", BlendA)
		("weaponstate", WeaponState)
		("logtext", LogText)
		("subtitletext", SubtitleText)
		("subtitlecounter", SubtitleCounter)
		("conversionnpc", ConversationNPC)
		("conversionpc", ConversationPC)
		("conversionnpcangle", ConversationNPCAngle)
		("conversionfacetalker", ConversationFaceTalker)
		.Array("frags", frags, MAXPLAYERS)
		("psprites", psprites)
		("currentplayerclass", CurrentPlayerClass)
		("crouchfactor", crouchfactor)
		("crouching", crouching)
		("crouchdir", crouchdir)
		("crouchviewdelta", crouchviewdelta)
		("original_cmd", original_cmd)
		("original_oldbuttons", original_oldbuttons)
		("poisontype", poisontype)
		("poisonpaintype", poisonpaintype)
		("timefreezer", timefreezer)
		("settings_controller", settings_controller)
		("onground", onground)
		("musinfoactor", MUSINFOactor)
		("musinfotics", MUSINFOtics)
		("soundclass", SoundClass);

	if (arc.isWriting ())
	{
		// If the player reloaded because they pressed +use after dying, we
		// don't want +use to still be down after the game is loaded.
		oldbuttons = ~0;
		original_oldbuttons = ~0;
	}
	if (skinname.IsNotEmpty())
	{
		userinfo.SkinChanged(skinname, CurrentPlayerClass);
	}
}

bool P_IsPlayerTotallyFrozen(const player_t *player)
{
	return
		gamestate == GS_TITLELEVEL ||
		player->cheats & CF_TOTALLYFROZEN ||
		player->mo->isFrozen();
}


//==========================================================================
//
// native members
//
//==========================================================================

DEFINE_FIELD_X(PlayerInfo, player_t, mo)
DEFINE_FIELD_X(PlayerInfo, player_t, playerstate)
DEFINE_FIELD_X(PlayerInfo, player_t, original_oldbuttons)
DEFINE_FIELD_X(PlayerInfo, player_t, cls)
DEFINE_FIELD_X(PlayerInfo, player_t, DesiredFOV)
DEFINE_FIELD_X(PlayerInfo, player_t, FOV)
DEFINE_FIELD_X(PlayerInfo, player_t, viewz)
DEFINE_FIELD_X(PlayerInfo, player_t, viewheight)
DEFINE_FIELD_X(PlayerInfo, player_t, deltaviewheight)
DEFINE_FIELD_X(PlayerInfo, player_t, bob)
DEFINE_FIELD_X(PlayerInfo, player_t, Vel)
DEFINE_FIELD_X(PlayerInfo, player_t, centering)
DEFINE_FIELD_X(PlayerInfo, player_t, turnticks)
DEFINE_FIELD_X(PlayerInfo, player_t, attackdown)
DEFINE_FIELD_X(PlayerInfo, player_t, usedown)
DEFINE_FIELD_X(PlayerInfo, player_t, oldbuttons)
DEFINE_FIELD_X(PlayerInfo, player_t, health)
DEFINE_FIELD_X(PlayerInfo, player_t, inventorytics)
DEFINE_FIELD_X(PlayerInfo, player_t, CurrentPlayerClass)
DEFINE_FIELD_X(PlayerInfo, player_t, frags)
DEFINE_FIELD_X(PlayerInfo, player_t, fragcount)
DEFINE_FIELD_X(PlayerInfo, player_t, lastkilltime)
DEFINE_FIELD_X(PlayerInfo, player_t, multicount)
DEFINE_FIELD_X(PlayerInfo, player_t, spreecount)
DEFINE_FIELD_X(PlayerInfo, player_t, WeaponState)
DEFINE_FIELD_X(PlayerInfo, player_t, ReadyWeapon)
DEFINE_FIELD_X(PlayerInfo, player_t, PendingWeapon)
DEFINE_FIELD_X(PlayerInfo, player_t, psprites)
DEFINE_FIELD_X(PlayerInfo, player_t, cheats)
DEFINE_FIELD_X(PlayerInfo, player_t, timefreezer)
DEFINE_FIELD_X(PlayerInfo, player_t, refire)
DEFINE_FIELD_NAMED_X(PlayerInfo, player_t, inconsistant, inconsistent)
DEFINE_FIELD_X(PlayerInfo, player_t, waiting)
DEFINE_FIELD_X(PlayerInfo, player_t, killcount)
DEFINE_FIELD_X(PlayerInfo, player_t, itemcount)
DEFINE_FIELD_X(PlayerInfo, player_t, secretcount)
DEFINE_FIELD_X(PlayerInfo, player_t, damagecount)
DEFINE_FIELD_X(PlayerInfo, player_t, bonuscount)
DEFINE_FIELD_X(PlayerInfo, player_t, hazardcount)
DEFINE_FIELD_X(PlayerInfo, player_t, hazardinterval)
DEFINE_FIELD_X(PlayerInfo, player_t, hazardtype)
DEFINE_FIELD_X(PlayerInfo, player_t, poisoncount)
DEFINE_FIELD_X(PlayerInfo, player_t, poisontype)
DEFINE_FIELD_X(PlayerInfo, player_t, poisonpaintype)
DEFINE_FIELD_X(PlayerInfo, player_t, poisoner)
DEFINE_FIELD_X(PlayerInfo, player_t, attacker)
DEFINE_FIELD_X(PlayerInfo, player_t, extralight)
DEFINE_FIELD_X(PlayerInfo, player_t, fixedcolormap)
DEFINE_FIELD_X(PlayerInfo, player_t, fixedlightlevel)
DEFINE_FIELD_X(PlayerInfo, player_t, morphTics)
DEFINE_FIELD_X(PlayerInfo, player_t, MorphedPlayerClass)
DEFINE_FIELD_X(PlayerInfo, player_t, MorphStyle)
DEFINE_FIELD_X(PlayerInfo, player_t, MorphExitFlash)
DEFINE_FIELD_X(PlayerInfo, player_t, PremorphWeapon)
DEFINE_FIELD_X(PlayerInfo, player_t, chickenPeck)
DEFINE_FIELD_X(PlayerInfo, player_t, jumpTics)
DEFINE_FIELD_X(PlayerInfo, player_t, onground)
DEFINE_FIELD_X(PlayerInfo, player_t, respawn_time)
DEFINE_FIELD_X(PlayerInfo, player_t, camera)
DEFINE_FIELD_X(PlayerInfo, player_t, air_finished)
DEFINE_FIELD_X(PlayerInfo, player_t, LastDamageType)
DEFINE_FIELD_X(PlayerInfo, player_t, MUSINFOactor)
DEFINE_FIELD_X(PlayerInfo, player_t, MUSINFOtics)
DEFINE_FIELD_X(PlayerInfo, player_t, settings_controller)
DEFINE_FIELD_X(PlayerInfo, player_t, crouching)
DEFINE_FIELD_X(PlayerInfo, player_t, crouchdir)
DEFINE_FIELD_X(PlayerInfo, player_t, Bot)
DEFINE_FIELD_X(PlayerInfo, player_t, BlendR)
DEFINE_FIELD_X(PlayerInfo, player_t, BlendG)
DEFINE_FIELD_X(PlayerInfo, player_t, BlendB)
DEFINE_FIELD_X(PlayerInfo, player_t, BlendA)
DEFINE_FIELD_X(PlayerInfo, player_t, LogText)
DEFINE_FIELD_X(PlayerInfo, player_t, MinPitch)
DEFINE_FIELD_X(PlayerInfo, player_t, MaxPitch)
DEFINE_FIELD_X(PlayerInfo, player_t, crouchfactor)
DEFINE_FIELD_X(PlayerInfo, player_t, crouchoffset)
DEFINE_FIELD_X(PlayerInfo, player_t, crouchviewdelta)
DEFINE_FIELD_X(PlayerInfo, player_t, ConversationNPC)
DEFINE_FIELD_X(PlayerInfo, player_t, ConversationPC)
DEFINE_FIELD_X(PlayerInfo, player_t, ConversationNPCAngle)
DEFINE_FIELD_X(PlayerInfo, player_t, ConversationFaceTalker)
DEFINE_FIELD_NAMED_X(PlayerInfo, player_t, cmd.ucmd, cmd)
DEFINE_FIELD_X(PlayerInfo, player_t, original_cmd)
DEFINE_FIELD_X(PlayerInfo, player_t, userinfo)
DEFINE_FIELD_X(PlayerInfo, player_t, weapons)
DEFINE_FIELD_NAMED_X(PlayerInfo, player_t, cmd.ucmd.buttons, buttons)
DEFINE_FIELD_X(PlayerInfo, player_t, SoundClass)

DEFINE_FIELD_X(UserCmd, usercmd_t, buttons)
DEFINE_FIELD_X(UserCmd, usercmd_t, pitch)
DEFINE_FIELD_X(UserCmd, usercmd_t, yaw)
DEFINE_FIELD_X(UserCmd, usercmd_t, roll)
DEFINE_FIELD_X(UserCmd, usercmd_t, forwardmove)
DEFINE_FIELD_X(UserCmd, usercmd_t, sidemove)
DEFINE_FIELD_X(UserCmd, usercmd_t, upmove)

DEFINE_FIELD(FPlayerClass, Type)
DEFINE_FIELD(FPlayerClass, Flags)
DEFINE_FIELD(FPlayerClass, Skins)

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


#ifndef __D_PLAYER_H__
#define __D_PLAYER_H__

// Finally, for odd reasons, the player input
// is buffered within the player data struct,
// as commands per game tick.
#include "d_protocol.h"
#include "doomstat.h"

#include "a_weapons.h"

#include "d_netinf.h"

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "actor.h"

//Added by MC:
#include "b_bot.h"

class player_t;

// Standard pre-defined skin colors
struct FPlayerColorSet
{
	struct ExtraRange
	{
		uint8_t RangeStart, RangeEnd;	// colors to remap
		uint8_t FirstColor, LastColor;	// colors to map to
	};

	FName Name;			// Name of this color

	int Lump;			// Lump to read the translation from, otherwise use next 2 fields
	uint8_t FirstColor, LastColor;		// Describes the range of colors to use for the translation

	uint8_t RepresentativeColor;		// A palette entry representative of this translation,
									// for map arrows and status bar backgrounds and such
	uint8_t NumExtraRanges;
	ExtraRange Extra[6];
};

typedef TArray<std::tuple<PClass*, FName, PalEntry>> PainFlashList;
typedef TArray<std::tuple<PClass*, int, FPlayerColorSet>> ColorSetList;

extern PainFlashList PainFlashes;
extern ColorSetList ColorSets;

FString GetPrintableDisplayName(PClassActor *cls);

void PlayIdle(AActor *player);


//
// PlayerPawn flags
//
enum
{
	PPF_NOTHRUSTWHENINVUL = 1,	// Attacks do not thrust the player if they are invulnerable.
};

//
// Player states.
//
typedef enum
{
	PST_LIVE,	// Playing or camping.
	PST_DEAD,	// Dead on the ground, view follows killer.
	PST_REBORN,	// Ready to restart/respawn???
	PST_ENTER,	// [BC] Entered the game
	PST_GONE	// Player has left the game
} playerstate_t;


//
// Player internal flags, for cheats and debug.
//
typedef enum
{
	CF_NOCLIP			= 1 << 0,		// No clipping, walk through barriers.
	CF_GODMODE			= 1 << 1,		// No damage, no health loss.
	CF_NOVELOCITY		= 1 << 2,		// Not really a cheat, just a debug aid.
	CF_NOTARGET			= 1 << 3,		// [RH] Monsters don't target
	CF_FLY				= 1 << 4,		// [RH] Flying player
	CF_CHASECAM			= 1 << 5,		// [RH] Put camera behind player
	CF_FROZEN			= 1 << 6,		// [RH] Don't let the player move
	CF_REVERTPLEASE		= 1 << 7,		// [RH] Stick camera in player's head if (s)he moves
	CF_STEPLEFT			= 1 << 9,		// [RH] Play left footstep sound next time
	CF_FRIGHTENING		= 1 << 10,		// [RH] Scare monsters away
	CF_INSTANTWEAPSWITCH= 1 << 11,		// [RH] Switch weapons instantly
	CF_TOTALLYFROZEN	= 1 << 12,		// [RH] All players can do is press +use
	CF_PREDICTING		= 1 << 13,		// [RH] Player movement is being predicted
	CF_INTERPVIEW		= 1 << 14,		// [RH] view was changed outside of input, so interpolate one frame
	CF_EXTREMELYDEAD	= 1 << 22,		// [RH] Reliably let the status bar know about extreme deaths.
	CF_BUDDHA2			= 1 << 24,		// [MC] Absolute buddha. No voodoo can kill it either.
	CF_GODMODE2			= 1 << 25,		// [MC] Absolute godmode. No voodoo can kill it either.
	CF_BUDDHA			= 1 << 27,		// [SP] Buddha mode - take damage, but don't die
	CF_NOCLIP2			= 1 << 30,		// [RH] More Quake-like noclip
} cheat_t;

enum
{
	WF_WEAPONREADY		= 1 << 0,		// [RH] Weapon is in the ready state and can fire its primary attack
	WF_WEAPONBOBBING	= 1 << 1,		// [HW] Bob weapon while the player is moving
	WF_WEAPONREADYALT	= 1 << 2,		// Weapon can fire its secondary attack
	WF_WEAPONSWITCHOK	= 1 << 3,		// It is okay to switch away from this weapon
	WF_DISABLESWITCH	= 1 << 4,		// Disable weapon switching completely
	WF_WEAPONRELOADOK	= 1 << 5,		// [XA] Okay to reload this weapon.
	WF_WEAPONZOOMOK		= 1 << 6,		// [XA] Okay to use weapon zoom function.
	WF_REFIRESWITCHOK	= 1 << 7,		// Mirror WF_WEAPONSWITCHOK for A_ReFire
	WF_USER1OK			= 1 << 8,		// [MC] Allow pushing of custom state buttons 1-4
	WF_USER2OK			= 1 << 9,
	WF_USER3OK			= 1 << 10,
	WF_USER4OK			= 1 << 11,
};

// The VM cannot deal with this as an invalid pointer because it performs a read barrier on every object pointer read.
// This doesn't have to point to a valid weapon, though, because WP_NOCHANGE is never dereferenced, but it must point to a valid object
// and the class descriptor just works fine for that.
extern DObject *WP_NOCHANGE;


// [GRB] Custom player classes
enum
{
	PCF_NOMENU			= 1,	// Hide in new game menu
};

class FPlayerClass
{
public:
	FPlayerClass ();
	FPlayerClass (const FPlayerClass &other);
	~FPlayerClass ();

	bool CheckSkin (int skin);

	PClassActor *Type;
	uint32_t Flags;
	TArray<int> Skins;
};

extern TArray<FPlayerClass> PlayerClasses;

// Custom pronouns
struct FPronouns {
	FString pronoun[6];
	FString string;

	FPronouns() {}
	FPronouns(FString p1, FString p2, FString p3, FString p4, FString p5, FString p6)
		: pronoun{p1, p2, p3, p4, p5, p6} { BuildString(); }

	FString& operator[](uint8_t index) { return pronoun[index]; }
	const FString& operator[](uint8_t index) const { return pronoun[index]; }

	operator const char *()  const { return string; }
	operator const FString() const { return string; }

	void BuildString();
	bool FromString (FString str);
};

enum
{
	PRONOUN_THEY,
	PRONOUN_SHE,
	PRONOUN_HE,
	PRONOUN_IT,
	PRONOUN_MAX
};

const struct FPronouns DefaultPronouns[PRONOUN_MAX] = {
	{"they", "them", "their", "theirs", "themself", "they're"},
	{"she",  "her",  "her",   "hers",   "herself",  "she's"  },
	{"he",   "him",  "his",   "his",    "himself",  "he's"   },
	{"it",   "it",   "its",   "its",    "itself",   "it's"   }
};

// User info (per-player copies of each CVAR_USERINFO cvar)
struct userinfo_t : TMap<FName,FBaseCVar *>
{
	~userinfo_t();

	double GetAimDist() const
	{
		if (dmflags2 & DF2_NOAUTOAIM)
		{
			return 0;
		}

		float aim = *static_cast<FFloatCVar *>(*CheckKey(NAME_Autoaim));
		if (aim > 35 || aim < 0)
		{
			return 35.;
		}
		else
		{
			return aim;
		}
	}
	// Same but unfiltered.
	double GetAutoaim() const
	{
		return *static_cast<FFloatCVar *>(*CheckKey(NAME_Autoaim));
	}
	const char *GetName() const
	{
		return *static_cast<FStringCVar *>(*CheckKey(NAME_Name));
	}
	int GetTeam() const
	{
		return *static_cast<FIntCVar *>(*CheckKey(NAME_Team));
	}
	int GetColorSet() const
	{
		return *static_cast<FIntCVar *>(*CheckKey(NAME_ColorSet));
	}
	uint32_t GetColor() const
	{
		return *static_cast<FColorCVar *>(*CheckKey(NAME_Color));
	}
	bool GetNeverSwitch() const
	{
		return *static_cast<FBoolCVar *>(*CheckKey(NAME_NeverSwitchOnPickup));
	}
	double GetMoveBob() const
	{
		return *static_cast<FFloatCVar *>(*CheckKey(NAME_MoveBob));
	}
	double GetStillBob() const
	{
		return *static_cast<FFloatCVar *>(*CheckKey(NAME_StillBob));
	}
	float GetWBobSpeed() const
	{
		return *static_cast<FFloatCVar *>(*CheckKey(NAME_WBobSpeed));
	}
	double GetWBobFire() const
	{
		return *static_cast<FFloatCVar *>(*CheckKey(NAME_WBobFire));
	}
	int GetPlayerClassNum() const
	{
		return *static_cast<FIntCVar *>(*CheckKey(NAME_PlayerClass));
	}
	bool GetClassicFlight() const
	{
		return *static_cast<FBoolCVar *>(*CheckKey(NAME_ClassicFlight));
	}
	PClassActor *GetPlayerClassType() const
	{
		return PlayerClasses[GetPlayerClassNum()].Type;
	}
	int GetSkin() const
	{
		return *static_cast<FIntCVar *>(*CheckKey(NAME_Skin));
	}
	int GetGender() const
	{
		auto cvar = CheckKey(NAME_Gender);
		return cvar ? *static_cast<FIntCVar *>(*cvar) : 0;
	}
	const struct FPronouns GetPronouns() const
	{
		const char *str = *static_cast<FStringCVar *>(*CheckKey(NAME_Pronouns));

		FPronouns pronouns;
		if (pronouns.FromString(str))
		{
			return pronouns;
		}
		else return DefaultPronouns[PRONOUN_THEY];
	}
	bool GetNoAutostartMap() const
	{
		return *static_cast<FBoolCVar *>(*CheckKey(NAME_Wi_NoAutostartMap));
	}

	void Reset();
	int TeamChanged(int team);
	int SkinChanged(const char *skinname, int playerclass);
	int SkinNumChanged(int skinnum);
	int GenderChanged(const char *gendername);
	int PlayerClassChanged(const char *classname);
		uint32_t ColorChanged(const char *colorname);
	uint32_t ColorChanged(uint32_t colorval);
	int ColorSetChanged(int setnum);
};

void ReadUserInfo(FSerializer &arc, userinfo_t &info, FString &skin);
void WriteUserInfo(FSerializer &arc, userinfo_t &info);

//
// Extended player object info: player_t
//
class player_t
{
public:
	player_t() = default;
	~player_t();
	player_t &operator= (const player_t &p) = delete;
	void CopyFrom(player_t &src, bool copyPSP);

	void Serialize(FSerializer &arc);
	size_t PropagateMark();

	void SetLogNumber (int num);
	void SetLogText (const char *text);
	void SendPitchLimits() const;
	void SetSubtitle(int num, FSoundID soundid);

	AActor *mo = nullptr;
	uint8_t		playerstate = 0;
	ticcmd_t	cmd = {};
	usercmd_t	original_cmd;
	uint32_t		original_oldbuttons;

	userinfo_t	userinfo;				// [RH] who is this?
	
	PClassActor *cls = nullptr;				// class of associated PlayerPawn

	float		DesiredFOV = 0;				// desired field of vision
	float		FOV = 0;					// current field of vision
	double		viewz = 0;					// focal origin above r.z
	double		viewheight = 0;				// base height above floor for viewz
	double		deltaviewheight = 0;		// squat speed.
	double		bob = 0;					// bounded/scaled total velocity

	// killough 10/98: used for realistic bobbing (i.e. not simply overall speed)
	// mo->velx and mo->vely represent true velocity experienced by player.
	// This only represents the thrust that the player applies himself.
	// This avoids anomalies with such things as Boom ice and conveyors.
	DVector2 Vel = { 0,0 };

	bool		centering = false;
	uint8_t		turnticks = 0;


	bool		attackdown = false;
	bool		usedown = false;
	uint32_t	oldbuttons = false;
	int			health = 0;					// only used between levels, mo->health
										// is used during levels

	int			inventorytics = 0;
	uint8_t		CurrentPlayerClass = 0;		// class # for this player instance

	int			frags[MAXPLAYERS] = {};		// kills of other players
	int			fragcount = 0;				// [RH] Cumulative frags for this player
	int			lastkilltime = 0;			// [RH] For multikills
	uint8_t		multicount = 0;
	uint8_t		spreecount = 0;				// [RH] Keep track of killing sprees
	uint16_t	WeaponState = 0;

	AActor	   *ReadyWeapon = nullptr;
	AActor	   *PendingWeapon = nullptr;			// WP_NOCHANGE if not changing
	TObjPtr<DPSprite*> psprites = nullptr; // view sprites (gun, etc)

	int			cheats = 0;					// bit flags
	int			timefreezer = 0;			// Player has an active time freezer
	short		refire = 0;					// refired shots are less accurate
	short		inconsistant = 0;
	bool		waiting = 0;
	int			killcount = 0, itemcount = 0, secretcount = 0;		// for intermission
	uint32_t	damagecount = 0, bonuscount = 0;// for screen flashing
	int			hazardcount = 0;			// for delayed Strife damage
	int			hazardinterval = 0;			// Frequency of damage infliction
	FName		hazardtype = NAME_None;				// Damage type of last hazardous damage encounter.
	int			poisoncount = 0;			// screen flash for poison damage
	FName		poisontype = NAME_None;				// type of poison damage to apply
	FName		poisonpaintype = NAME_None;			// type of Pain state to enter for poison damage
	TObjPtr<AActor*>		poisoner = nullptr;		// NULL for non-player actors
	TObjPtr<AActor*>		attacker = nullptr;		// who did damage (NULL for floors)
	int			extralight = 0;				// so gun flashes light up areas
	short		fixedcolormap = 0;			// can be set to REDCOLORMAP, etc.
	short		fixedlightlevel = 0;
	int			morphTics = 0;				// player is a chicken/pig if > 0
	PClassActor *MorphedPlayerClass = nullptr;		// [MH] (for SBARINFO) class # for this player instance when morphed
	int			MorphStyle = 0;				// which effects to apply for this player instance when morphed
	PClassActor *MorphExitFlash = nullptr;		// flash to apply when demorphing (cache of value given to MorphPlayer)
	TObjPtr<AActor*>	PremorphWeapon = nullptr;		// ready weapon before morphing
	int			chickenPeck = 0;			// chicken peck countdown
	int			jumpTics = 0;				// delay the next jump for a moment
	bool		onground = 0;				// Identifies if this player is on the ground or other object

	int			respawn_time = 0;			// [RH] delay respawning until this tic
	TObjPtr<AActor*>	camera = nullptr;			// [RH] Whose eyes this player sees through

	int			air_finished = 0;			// [RH] Time when you start drowning

	FName		LastDamageType = NAME_None;			// [RH] For damage-specific pain and death sounds

	TObjPtr<AActor*> MUSINFOactor = nullptr;		// For MUSINFO purposes
	int8_t		MUSINFOtics = 0;

	bool		settings_controller = false;	// Player can control game settings.
	int8_t		crouching = 0;
	int8_t		crouchdir = 0;

	//Added by MC:
	TObjPtr<DBot*> Bot = nullptr;

	float		BlendR = 0;		// [RH] Final blending values
	float		BlendG = 0;
	float		BlendB = 0;
	float		BlendA = 0;

	FString		SoundClass;
	FString		LogText;	// [RH] Log for Strife
	FString		SubtitleText;
	int			SubtitleCounter;

	DAngle			MinPitch = 0.;	// Viewpitch limits (negative is up, positive is down)
	DAngle			MaxPitch = 0.;

	double crouchfactor = 0;
	double crouchoffset = 0;
	double crouchviewdelta = 0;

	FWeaponSlots weapons;

	// [CW] I moved these here for multiplayer conversation support.
	TObjPtr<AActor*> ConversationNPC = nullptr, ConversationPC = nullptr;
	DAngle ConversationNPCAngle = 0.;
	bool ConversationFaceTalker = false;

	double GetDeltaViewHeight() const
	{
		return (mo->FloatVar(NAME_ViewHeight) + crouchviewdelta - viewheight) / 8;
	}

	double DefaultViewHeight() const
	{
		return mo->FloatVar(NAME_ViewHeight);
	}

	void Uncrouch()
	{
		if (crouchfactor != 1)
		{
			crouchfactor = 1;
			crouchoffset = 0;
			crouchdir = 0;
			crouching = 0;
			crouchviewdelta = 0;
			viewheight = mo ? mo->FloatVar(NAME_ViewHeight) : 0;
		}
	}
	
	int GetSpawnClass();

	// PSprite layers
	void TickPSprites();
	void DestroyPSprites();
	DPSprite *FindPSprite(int layer);
	// Used ONLY for compatibility with the old hardcoded layers.
	// Make sure that a state is properly set after calling this unless
	// you are 100% sure the context already implies the layer exists.
	DPSprite *GetPSprite(PSPLayers layer);

	// [Nash] set player FOV
	void SetFOV(float fov);
	bool HasWeaponsInSlot(int slot) const;
	bool Resurrect();
};

// Bookkeeping on players - state.
extern player_t players[MAXPLAYERS];

void P_CheckPlayerSprite(AActor *mo, int &spritenum, DVector2 &scale);
void EnumColorSets(PClassActor *pc, TArray<int> *out);
FPlayerColorSet *GetColorSet(PClassActor *pc, int setnum);

inline void AActor::SetFriendPlayer(player_t *player)
{
	if (player == NULL)
	{
		FriendPlayer = 0;
	}
	else
	{
		FriendPlayer = int(player - players) + 1;
	}
}

inline bool AActor::IsNoClip2() const
{
	if (player != NULL && player->mo == this)
	{
		return (player->cheats & CF_NOCLIP2) != 0;
	}
	return false;
}

bool P_IsPlayerTotallyFrozen(const player_t *player);

#endif // __D_PLAYER_H__

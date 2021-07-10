/*
** p_acs.cpp
** General BEHAVIOR management and ACS execution environment
**
**---------------------------------------------------------------------------
** Copyright 1998-2012 Randy Heit
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
** This code at one time made lots of little-endian assumptions.
** I think it should be fine on big-endian machines now, but I have no
** real way to test it.
*/

#include <assert.h>

#include "templates.h"
#include "doomdef.h"
#include "p_local.h"
#include "d_player.h"
#include "p_spec.h"
#include "p_acs.h"
#include "p_saveg.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "s_sndseq.h"
#include "sbar.h"
#include "a_sharedglobal.h"
#include "filesystem.h"
#include "r_sky.h"
#include "gstrings.h"
#include "gi.h"
#include "g_game.h"
#include "c_bind.h"
#include "cmdlib.h"
#include "m_png.h"
#include "p_setup.h"
#include "po_man.h"
#include "actorptrselect.h"
#include "serializer_doom.h"
#include "serialize_obj.h"
#include "decallib.h"
#include "p_terrain.h"
#include "version.h"
#include "p_effect.h"
#include "r_utility.h"
#include "a_morph.h"
#include "thingdef.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "types.h"
#include "scriptutil.h"
#include "s_music.h"
#include "v_video.h"
#include "texturemanager.h"

	// P-codes for ACS scripts
	enum
	{
/*  0*/	PCD_NOP,
		PCD_TERMINATE,
		PCD_SUSPEND,
		PCD_PUSHNUMBER,
		PCD_LSPEC1,
		PCD_LSPEC2,
		PCD_LSPEC3,
		PCD_LSPEC4,
		PCD_LSPEC5,
		PCD_LSPEC1DIRECT,
/* 10*/	PCD_LSPEC2DIRECT,
		PCD_LSPEC3DIRECT,
		PCD_LSPEC4DIRECT,
		PCD_LSPEC5DIRECT,
		PCD_ADD,
		PCD_SUBTRACT,
		PCD_MULTIPLY,
		PCD_DIVIDE,
		PCD_MODULUS,
		PCD_EQ,
/* 20*/ PCD_NE,
		PCD_LT,
		PCD_GT,
		PCD_LE,
		PCD_GE,
		PCD_ASSIGNSCRIPTVAR,
		PCD_ASSIGNMAPVAR,
		PCD_ASSIGNWORLDVAR,
		PCD_PUSHSCRIPTVAR,
		PCD_PUSHMAPVAR,
/* 30*/	PCD_PUSHWORLDVAR,
		PCD_ADDSCRIPTVAR,
		PCD_ADDMAPVAR,
		PCD_ADDWORLDVAR,
		PCD_SUBSCRIPTVAR,
		PCD_SUBMAPVAR,
		PCD_SUBWORLDVAR,
		PCD_MULSCRIPTVAR,
		PCD_MULMAPVAR,
		PCD_MULWORLDVAR,
/* 40*/	PCD_DIVSCRIPTVAR,
		PCD_DIVMAPVAR,
		PCD_DIVWORLDVAR,
		PCD_MODSCRIPTVAR,
		PCD_MODMAPVAR,
		PCD_MODWORLDVAR,
		PCD_INCSCRIPTVAR,
		PCD_INCMAPVAR,
		PCD_INCWORLDVAR,
		PCD_DECSCRIPTVAR,
/* 50*/	PCD_DECMAPVAR,
		PCD_DECWORLDVAR,
		PCD_GOTO,
		PCD_IFGOTO,
		PCD_DROP,
		PCD_DELAY,
		PCD_DELAYDIRECT,
		PCD_RANDOM,
		PCD_RANDOMDIRECT,
		PCD_THINGCOUNT,
/* 60*/	PCD_THINGCOUNTDIRECT,
		PCD_TAGWAIT,
		PCD_TAGWAITDIRECT,
		PCD_POLYWAIT,
		PCD_POLYWAITDIRECT,
		PCD_CHANGEFLOOR,
		PCD_CHANGEFLOORDIRECT,
		PCD_CHANGECEILING,
		PCD_CHANGECEILINGDIRECT,
		PCD_RESTART,
/* 70*/	PCD_ANDLOGICAL,
		PCD_ORLOGICAL,
		PCD_ANDBITWISE,
		PCD_ORBITWISE,
		PCD_EORBITWISE,
		PCD_NEGATELOGICAL,
		PCD_LSHIFT,
		PCD_RSHIFT,
		PCD_UNARYMINUS,
		PCD_IFNOTGOTO,
/* 80*/	PCD_LINESIDE,
		PCD_SCRIPTWAIT,
		PCD_SCRIPTWAITDIRECT,
		PCD_CLEARLINESPECIAL,
		PCD_CASEGOTO,
		PCD_BEGINPRINT,
		PCD_ENDPRINT,
		PCD_PRINTSTRING,
		PCD_PRINTNUMBER,
		PCD_PRINTCHARACTER,
/* 90*/	PCD_PLAYERCOUNT,
		PCD_GAMETYPE,
		PCD_GAMESKILL,
		PCD_TIMER,
		PCD_SECTORSOUND,
		PCD_AMBIENTSOUND,
		PCD_SOUNDSEQUENCE,
		PCD_SETLINETEXTURE,
		PCD_SETLINEBLOCKING,
		PCD_SETLINESPECIAL,
/*100*/	PCD_THINGSOUND,
		PCD_ENDPRINTBOLD,		// [RH] End of Hexen p-codes
		PCD_ACTIVATORSOUND,
		PCD_LOCALAMBIENTSOUND,
		PCD_SETLINEMONSTERBLOCKING,
		PCD_PLAYERBLUESKULL,	// [BC] Start of new [Skull Tag] pcodes
		PCD_PLAYERREDSKULL,
		PCD_PLAYERYELLOWSKULL,
		PCD_PLAYERMASTERSKULL,
		PCD_PLAYERBLUECARD,
/*110*/	PCD_PLAYERREDCARD,
		PCD_PLAYERYELLOWCARD,
		PCD_PLAYERMASTERCARD,
		PCD_PLAYERBLACKSKULL,
		PCD_PLAYERSILVERSKULL,
		PCD_PLAYERGOLDSKULL,
		PCD_PLAYERBLACKCARD,
		PCD_PLAYERSILVERCARD,
		PCD_ISNETWORKGAME,
		PCD_PLAYERTEAM,
/*120*/	PCD_PLAYERHEALTH,
		PCD_PLAYERARMORPOINTS,
		PCD_PLAYERFRAGS,
		PCD_PLAYEREXPERT,
		PCD_BLUETEAMCOUNT,
		PCD_REDTEAMCOUNT,
		PCD_BLUETEAMSCORE,
		PCD_REDTEAMSCORE,
		PCD_ISONEFLAGCTF,
		PCD_LSPEC6,				// These are never used. They should probably
/*130*/	PCD_LSPEC6DIRECT,		// be given names like PCD_DUMMY.
		PCD_PRINTNAME,
		PCD_MUSICCHANGE,
		PCD_CONSOLECOMMANDDIRECT,
		PCD_CONSOLECOMMAND,
		PCD_SINGLEPLAYER,		// [RH] End of Skull Tag p-codes
		PCD_FIXEDMUL,
		PCD_FIXEDDIV,
		PCD_SETGRAVITY,
		PCD_SETGRAVITYDIRECT,
/*140*/	PCD_SETAIRCONTROL,
		PCD_SETAIRCONTROLDIRECT,
		PCD_CLEARINVENTORY,
		PCD_GIVEINVENTORY,
		PCD_GIVEINVENTORYDIRECT,
		PCD_TAKEINVENTORY,
		PCD_TAKEINVENTORYDIRECT,
		PCD_CHECKINVENTORY,
		PCD_CHECKINVENTORYDIRECT,
		PCD_SPAWN,
/*150*/	PCD_SPAWNDIRECT,
		PCD_SPAWNSPOT,
		PCD_SPAWNSPOTDIRECT,
		PCD_SETMUSIC,
		PCD_SETMUSICDIRECT,
		PCD_LOCALSETMUSIC,
		PCD_LOCALSETMUSICDIRECT,
		PCD_PRINTFIXED,
		PCD_PRINTLOCALIZED,
		PCD_MOREHUDMESSAGE,
/*160*/	PCD_OPTHUDMESSAGE,
		PCD_ENDHUDMESSAGE,
		PCD_ENDHUDMESSAGEBOLD,
		PCD_SETSTYLE,
		PCD_SETSTYLEDIRECT,
		PCD_SETFONT,
		PCD_SETFONTDIRECT,
		PCD_PUSHBYTE,
		PCD_LSPEC1DIRECTB,
		PCD_LSPEC2DIRECTB,
/*170*/	PCD_LSPEC3DIRECTB,
		PCD_LSPEC4DIRECTB,
		PCD_LSPEC5DIRECTB,
		PCD_DELAYDIRECTB,
		PCD_RANDOMDIRECTB,
		PCD_PUSHBYTES,
		PCD_PUSH2BYTES,
		PCD_PUSH3BYTES,
		PCD_PUSH4BYTES,
		PCD_PUSH5BYTES,
/*180*/	PCD_SETTHINGSPECIAL,
		PCD_ASSIGNGLOBALVAR,
		PCD_PUSHGLOBALVAR,
		PCD_ADDGLOBALVAR,
		PCD_SUBGLOBALVAR,
		PCD_MULGLOBALVAR,
		PCD_DIVGLOBALVAR,
		PCD_MODGLOBALVAR,
		PCD_INCGLOBALVAR,
		PCD_DECGLOBALVAR,
/*190*/	PCD_FADETO,
		PCD_FADERANGE,
		PCD_CANCELFADE,
		PCD_PLAYMOVIE,
		PCD_SETFLOORTRIGGER,
		PCD_SETCEILINGTRIGGER,
		PCD_GETACTORX,
		PCD_GETACTORY,
		PCD_GETACTORZ,
		PCD_STARTTRANSLATION,
/*200*/	PCD_TRANSLATIONRANGE1,
		PCD_TRANSLATIONRANGE2,
		PCD_ENDTRANSLATION,
		PCD_CALL,
		PCD_CALLDISCARD,
		PCD_RETURNVOID,
		PCD_RETURNVAL,
		PCD_PUSHMAPARRAY,
		PCD_ASSIGNMAPARRAY,
		PCD_ADDMAPARRAY,
/*210*/	PCD_SUBMAPARRAY,
		PCD_MULMAPARRAY,
		PCD_DIVMAPARRAY,
		PCD_MODMAPARRAY,
		PCD_INCMAPARRAY,
		PCD_DECMAPARRAY,
		PCD_DUP,
		PCD_SWAP,
		PCD_WRITETOINI,
		PCD_GETFROMINI,
/*220*/ PCD_SIN,
		PCD_COS,
		PCD_VECTORANGLE,
		PCD_CHECKWEAPON,
		PCD_SETWEAPON,
		PCD_TAGSTRING,
		PCD_PUSHWORLDARRAY,
		PCD_ASSIGNWORLDARRAY,
		PCD_ADDWORLDARRAY,
		PCD_SUBWORLDARRAY,
/*230*/	PCD_MULWORLDARRAY,
		PCD_DIVWORLDARRAY,
		PCD_MODWORLDARRAY,
		PCD_INCWORLDARRAY,
		PCD_DECWORLDARRAY,
		PCD_PUSHGLOBALARRAY,
		PCD_ASSIGNGLOBALARRAY,
		PCD_ADDGLOBALARRAY,
		PCD_SUBGLOBALARRAY,
		PCD_MULGLOBALARRAY,
/*240*/	PCD_DIVGLOBALARRAY,
		PCD_MODGLOBALARRAY,
		PCD_INCGLOBALARRAY,
		PCD_DECGLOBALARRAY,
		PCD_SETMARINEWEAPON,
		PCD_SETACTORPROPERTY,
		PCD_GETACTORPROPERTY,
		PCD_PLAYERNUMBER,
		PCD_ACTIVATORTID,
		PCD_SETMARINESPRITE,
/*250*/	PCD_GETSCREENWIDTH,
		PCD_GETSCREENHEIGHT,
		PCD_THING_PROJECTILE2,
		PCD_STRLEN,
		PCD_SETHUDSIZE,
		PCD_GETCVAR,
		PCD_CASEGOTOSORTED,
		PCD_SETRESULTVALUE,
		PCD_GETLINEROWOFFSET,
		PCD_GETACTORFLOORZ,
/*260*/	PCD_GETACTORANGLE,
		PCD_GETSECTORFLOORZ,
		PCD_GETSECTORCEILINGZ,
		PCD_LSPEC5RESULT,
		PCD_GETSIGILPIECES,
		PCD_GETLEVELINFO,
		PCD_CHANGESKY,
		PCD_PLAYERINGAME,
		PCD_PLAYERISBOT,
		PCD_SETCAMERATOTEXTURE,
/*270*/	PCD_ENDLOG,
		PCD_GETAMMOCAPACITY,
		PCD_SETAMMOCAPACITY,
		PCD_PRINTMAPCHARARRAY,		// [JB] start of new p-codes
		PCD_PRINTWORLDCHARARRAY,
		PCD_PRINTGLOBALCHARARRAY,	// [JB] end of new p-codes
		PCD_SETACTORANGLE,			// [GRB]
		PCD_GRABINPUT,				// Unused but acc defines them
		PCD_SETMOUSEPOINTER,		// "
		PCD_MOVEMOUSEPOINTER,		// "
/*280*/	PCD_SPAWNPROJECTILE,
		PCD_GETSECTORLIGHTLEVEL,
		PCD_GETACTORCEILINGZ,
		PCD_SETACTORPOSITION,
		PCD_CLEARACTORINVENTORY,
		PCD_GIVEACTORINVENTORY,
		PCD_TAKEACTORINVENTORY,
		PCD_CHECKACTORINVENTORY,
		PCD_THINGCOUNTNAME,
		PCD_SPAWNSPOTFACING,
/*290*/	PCD_PLAYERCLASS,			// [GRB]
		//[MW] start my p-codes
		PCD_ANDSCRIPTVAR,
		PCD_ANDMAPVAR, 
		PCD_ANDWORLDVAR, 
		PCD_ANDGLOBALVAR, 
		PCD_ANDMAPARRAY, 
		PCD_ANDWORLDARRAY, 
		PCD_ANDGLOBALARRAY,
		PCD_EORSCRIPTVAR, 
		PCD_EORMAPVAR, 
/*300*/	PCD_EORWORLDVAR, 
		PCD_EORGLOBALVAR, 
		PCD_EORMAPARRAY, 
		PCD_EORWORLDARRAY, 
		PCD_EORGLOBALARRAY,
		PCD_ORSCRIPTVAR, 
		PCD_ORMAPVAR, 
		PCD_ORWORLDVAR, 
		PCD_ORGLOBALVAR, 
		PCD_ORMAPARRAY, 
/*310*/	PCD_ORWORLDARRAY, 
		PCD_ORGLOBALARRAY,
		PCD_LSSCRIPTVAR, 
		PCD_LSMAPVAR, 
		PCD_LSWORLDVAR, 
		PCD_LSGLOBALVAR, 
		PCD_LSMAPARRAY, 
		PCD_LSWORLDARRAY, 
		PCD_LSGLOBALARRAY,
		PCD_RSSCRIPTVAR, 
/*320*/	PCD_RSMAPVAR, 
		PCD_RSWORLDVAR, 
		PCD_RSGLOBALVAR, 
		PCD_RSMAPARRAY, 
		PCD_RSWORLDARRAY, 
		PCD_RSGLOBALARRAY, 
		//[MW] end my p-codes
		PCD_GETPLAYERINFO,			// [GRB]
		PCD_CHANGELEVEL,
		PCD_SECTORDAMAGE,
		PCD_REPLACETEXTURES,
/*330*/	PCD_NEGATEBINARY,
		PCD_GETACTORPITCH,
		PCD_SETACTORPITCH,
		PCD_PRINTBIND,
		PCD_SETACTORSTATE,
		PCD_THINGDAMAGE2,
		PCD_USEINVENTORY,
		PCD_USEACTORINVENTORY,
		PCD_CHECKACTORCEILINGTEXTURE,
		PCD_CHECKACTORFLOORTEXTURE,
/*340*/	PCD_GETACTORLIGHTLEVEL,
		PCD_SETMUGSHOTSTATE,
		PCD_THINGCOUNTSECTOR,
		PCD_THINGCOUNTNAMESECTOR,
		PCD_CHECKPLAYERCAMERA,		// [TN]
		PCD_MORPHACTOR,				// [MH]
		PCD_UNMORPHACTOR,			// [MH]
		PCD_GETPLAYERINPUT,
		PCD_CLASSIFYACTOR,
		PCD_PRINTBINARY,
/*350*/	PCD_PRINTHEX,
		PCD_CALLFUNC,
		PCD_SAVESTRING,			// [FDARI] create string (temporary)
		PCD_PRINTMAPCHRANGE,	// [FDARI] output range (print part of array)
		PCD_PRINTWORLDCHRANGE,
		PCD_PRINTGLOBALCHRANGE,
		PCD_STRCPYTOMAPCHRANGE,	// [FDARI] input range (copy string to all/part of array)
		PCD_STRCPYTOWORLDCHRANGE,
		PCD_STRCPYTOGLOBALCHRANGE,
		PCD_PUSHFUNCTION,		// from Eternity
/*360*/	PCD_CALLSTACK,			// from Eternity
		PCD_SCRIPTWAITNAMED,
		PCD_TRANSLATIONRANGE3,
		PCD_GOTOSTACK,
		PCD_ASSIGNSCRIPTARRAY,
		PCD_PUSHSCRIPTARRAY,
		PCD_ADDSCRIPTARRAY,
		PCD_SUBSCRIPTARRAY,
		PCD_MULSCRIPTARRAY,
		PCD_DIVSCRIPTARRAY,
/*370*/	PCD_MODSCRIPTARRAY,
		PCD_INCSCRIPTARRAY,
		PCD_DECSCRIPTARRAY,
		PCD_ANDSCRIPTARRAY,
		PCD_EORSCRIPTARRAY,
		PCD_ORSCRIPTARRAY,
		PCD_LSSCRIPTARRAY,
		PCD_RSSCRIPTARRAY,
		PCD_PRINTSCRIPTCHARARRAY,
		PCD_PRINTSCRIPTCHRANGE,
/*380*/	PCD_STRCPYTOSCRIPTCHRANGE,
		PCD_LSPEC5EX,
		PCD_LSPEC5EXRESULT,
		PCD_TRANSLATIONRANGE4,
		PCD_TRANSLATIONRANGE5,

/*381*/	PCODE_COMMAND_COUNT
	};

	// Some constants used by ACS scripts
	enum {
		LINE_FRONT =			0,
		LINE_BACK =				1
	};
	enum {
		SIDE_FRONT =			0,
		SIDE_BACK =				1
	};
	enum {
		TEXTURE_TOP =			0,
		TEXTURE_MIDDLE =		1,
		TEXTURE_BOTTOM =		2
	};
	enum {
		GAME_SINGLE_PLAYER =	0,
		GAME_NET_COOPERATIVE =	1,
		GAME_NET_DEATHMATCH =	2,
		GAME_TITLE_MAP =		3
	};
	enum {
		CLASS_FIGHTER =			0,
		CLASS_CLERIC =			1,
		CLASS_MAGE =			2
	};
	enum {
		SKILL_VERY_EASY =		0,
		SKILL_EASY =			1,
		SKILL_NORMAL =			2,
		SKILL_HARD =			3,
		SKILL_VERY_HARD =		4
	};
	enum {
		BLOCK_NOTHING =			0,
		BLOCK_CREATURES =		1,
		BLOCK_EVERYTHING =		2,
		BLOCK_RAILING =			3,
		BLOCK_PLAYERS =			4
	};
	enum {
		LEVELINFO_PAR_TIME,
		LEVELINFO_CLUSTERNUM,
		LEVELINFO_LEVELNUM,
		LEVELINFO_TOTAL_SECRETS,
		LEVELINFO_FOUND_SECRETS,
		LEVELINFO_TOTAL_ITEMS,
		LEVELINFO_FOUND_ITEMS,
		LEVELINFO_TOTAL_MONSTERS,
		LEVELINFO_KILLED_MONSTERS,
		LEVELINFO_SUCK_TIME
	};
	enum {
		PLAYERINFO_TEAM,
		PLAYERINFO_AIMDIST,
		PLAYERINFO_COLOR,
		PLAYERINFO_GENDER,
		PLAYERINFO_NEVERSWITCH,
		PLAYERINFO_MOVEBOB,
		PLAYERINFO_STILLBOB,
		PLAYERINFO_PLAYERCLASS,
		PLAYERINFO_FOV,
		PLAYERINFO_DESIREDFOV,
	};



FRandom pr_acs ("ACS");

// I imagine this much stack space is probably overkill, but it could
// potentially get used with recursive functions.
#define STACK_SIZE 4096

// HUD message flags
#define HUDMSG_LOG					(0x80000000)
#define HUDMSG_COLORSTRING			(0x40000000)
#define HUDMSG_ADDBLEND				(0x20000000)
#define HUDMSG_ALPHA				(0x10000000)
#define HUDMSG_NOWRAP				(0x08000000)

// HUD message layers; these are not flags
#define HUDMSG_LAYER_SHIFT			12
#define HUDMSG_LAYER_MASK			(0x0000F000)
// See HUDMSGLayer enumerations in sbar.h

// HUD message visibility flags
#define HUDMSG_VISIBILITY_SHIFT		16
#define HUDMSG_VISIBILITY_MASK		(0x00070000)
// See HUDMSG visibility enumerations in sbar.h

// LineAttack flags
#define FHF_NORANDOMPUFFZ	1
#define FHF_NOIMPACTDECAL	2

// SpawnDecal flags
#define SDF_ABSANGLE		1
#define SDF_PERMANENT		2
#define SDF_FIXED_ZOFF		4
#define SDF_FIXED_DISTANCE	8

// GetArmorInfo
enum
{
	ARMORINFO_CLASSNAME,
	ARMORINFO_SAVEAMOUNT,
	ARMORINFO_SAVEPERCENT,
	ARMORINFO_MAXABSORB,
	ARMORINFO_MAXFULLABSORB,
	ARMORINFO_ACTUALSAVEAMOUNT,
};

// PickActor
// [JP] I've renamed these flags to something else to avoid confusion with the other PAF_ flags
enum
{
//	PAF_FORCETID,
//	PAF_RETURNTID
	PICKAF_FORCETID = 1,
	PICKAF_RETURNTID = 2,
};

// ACS specific conversion functions to and from fixed point.
// These should be used to convert from and to sctipt variables
// so that there is a clear distinction between leftover fixed point code
// and genuinely needed conversions.

inline double ACSToDouble(int acsval)
{
	return acsval / 65536.;
}

inline float ACSToFloat(int acsval)
{
	return acsval / 65536.f;
}

inline int DoubleToACS(double val)
{
	return xs_Fix<16>::ToFix(val);
}

inline DAngle ACSToAngle(int acsval)
{
	return acsval * (360. / 65536.);
}

inline int AngleToACS(DAngle ang)
{
	return ang.BAMs() >> 16;
}

inline int PitchToACS(DAngle ang)
{
	return int(ang.Normalized180().Degrees * (65536. / 360));
}

struct CallReturn
{
	CallReturn(int pc, ScriptFunction *func, FBehavior *module, const ACSLocalVariables &locals, ACSLocalArrays *arrays, bool discard, unsigned int runaway)
		: ReturnFunction(func),
		  ReturnModule(module),
		  ReturnLocals(locals),
		  ReturnArrays(arrays),
		  ReturnAddress(pc),
		  bDiscardResult(discard),
		  EntryInstrCount(runaway)
	{}

	ScriptFunction *ReturnFunction;
	FBehavior *ReturnModule;
	ACSLocalVariables ReturnLocals;
	ACSLocalArrays *ReturnArrays;
	int ReturnAddress;
	int bDiscardResult;
	unsigned int EntryInstrCount;
};


class DLevelScript : public DObject
{
	DECLARE_CLASS(DLevelScript, DObject)
	HAS_OBJECT_POINTERS
public:


	enum EScriptState
	{
		SCRIPT_Running,
		SCRIPT_Suspended,
		SCRIPT_Delayed,
		SCRIPT_TagWait,
		SCRIPT_PolyWait,
		SCRIPT_ScriptWaitPre,
		SCRIPT_ScriptWait,
		SCRIPT_PleaseRemove,
		SCRIPT_DivideBy0,
		SCRIPT_ModulusBy0,
	};

	DLevelScript(FLevelLocals *l, AActor *who, line_t *where, int num, const ScriptPtr *code, FBehavior *module,
		const int *args, int argcount, int flags);

	void Serialize(FSerializer &arc);
	int RunScript();
	PClass *GetClassForIndex(int index) const;


	inline void SetState(EScriptState newstate) { state = newstate; }
	inline EScriptState GetState() { return state; }

	DLevelScript *GetNext() const { return next; }

	void MarkLocalVarStrings() const
	{
		GlobalACSStrings.MarkStringArray(&Localvars[0], Localvars.Size());
	}
	void LockLocalVarStrings(int levelnum) const
	{
		GlobalACSStrings.LockStringArray(levelnum, &Localvars[0], Localvars.Size());
	}

	FLevelLocals	*Level;

protected:
	DLevelScript	*next, *prev;
	int				script;
	TArray<int32_t>	Localvars;
	int				*pc;
	EScriptState	state;
	int				statedata;
	TObjPtr<AActor*>	activator;
	line_t			*activationline;
	bool			backSide;
	FFont			*activefont = nullptr;
	int				hudwidth, hudheight;
	int				ClipRectLeft, ClipRectTop, ClipRectWidth, ClipRectHeight;
	int				WrapWidth;
	bool			HandleAspect;
	FBehavior	    *activeBehavior;
	int				InModuleScriptNumber;

	TArray<FString> ACS_StringBuilderStack;

	inline void STRINGBUILDER_START(FString &Builder)
	{
		if (Builder.IsNotEmpty() || ACS_StringBuilderStack.Size())
		{
			ACS_StringBuilderStack.Push(Builder);
			Builder = "";
		}
	}

	inline void STRINGBUILDER_FINISH(FString &Builder)
	{
		if (!ACS_StringBuilderStack.Pop(Builder))
		{
			Builder = "";
		}
	}

	void Link();
	void Unlink();
	void PutLast();
	void PutFirst();
	int Random(int min, int max);
	int ThingCount(int type, int stringid, int tid, int tag);
	void ChangeFlat(int tag, int name, bool floorOrCeiling);
	int CountPlayers();
	void SetLineTexture(int lineid, int side, int position, int name);
	int DoSpawn(int type, const DVector3 &pos, int tid, DAngle angle, bool force);
	int DoSpawn(int type, int x, int y, int z, int tid, int angle, bool force);
	bool DoCheckActorTexture(int tid, AActor *activator, int string, bool floor);
	int DoSpawnSpot(int type, int spot, int tid, int angle, bool forced);
	int DoSpawnSpotFacing(int type, int spot, int tid, bool forced);
	int DoClassifyActor(int tid);
	int CallFunction(int argCount, int funcIndex, int32_t *args);

	void DoFadeTo(int r, int g, int b, int a, int time);
	void DoFadeRange(int r1, int g1, int b1, int a1, int r2, int g2, int b2, int a2, int time);
	void DoSetFont(int fontnum);
	void SetActorProperty(int tid, int property, int value);
	void DoSetActorProperty(AActor *actor, int property, int value);
	int GetActorProperty(int tid, int property);
	int CheckActorProperty(int tid, int property, int value);
	int GetPlayerInput(int playernum, int inputnum);

	int LineFromID(int id);
	int SideFromID(int id, int side);
	int ScriptCall(AActor *activator, unsigned argc, int32_t *args);
	int DoSetMaster(AActor *self, AActor *master);
	void SetUserVariable(AActor *self, FName varname, int index, int value);
	void DoSetCVar(FBaseCVar *cvar, int value, bool is_string, bool force = false);
	int SetUserCVar(int playernum, const char *cvarname, int value, bool is_string);
	int SetCVar(AActor *activator, const char *cvarname, int value, bool is_string);
	void SetActorAngle(AActor *activator, int tid, int angle, bool interpolate);
	void SetActorPitch(AActor *activator, int tid, int angle, bool interpolate);
	void SetActorRoll(AActor *activator, int tid, int angle, bool interpolate);
	void SetActorTeleFog(AActor *activator, int tid, FString telefogsrc, FString telefogdest);
	int SwapActorTeleFog(AActor *activator, int tid);


private:
	DLevelScript() = default;

	friend class DACSThinker;
};

static DLevelScript *P_GetScriptGoing (FLevelLocals *Level, AActor *who, line_t *where, int num, const ScriptPtr *code, FBehavior *module,
	const int *args, int argcount, int flags);


struct FBehavior::ArrayInfo
{
	uint32_t ArraySize;
	int32_t *Elements;
};

//============================================================================
//
// uallong
//
// Read a possibly unaligned four-byte little endian integer from memory.
//
//============================================================================

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
inline int uallong(const int &foo)
{
	return foo;
}
#else
inline int uallong(const int &foo)
{
	const unsigned char *bar = (const unsigned char *)&foo;
	return bar[0] | (bar[1] << 8) | (bar[2] << 16) | (bar[3] << 24);
}
#endif

//============================================================================
//
// Global and world variables
//
//============================================================================

// ACS variables with world scope
static BoundsCheckingArray<int32_t, NUM_WORLDVARS> ACS_WorldVars;
static BoundsCheckingArray<FWorldGlobalArray, NUM_WORLDVARS> ACS_WorldArrays;

// ACS variables with global scope
BoundsCheckingArray<int32_t, NUM_GLOBALVARS> ACS_GlobalVars;
BoundsCheckingArray<FWorldGlobalArray, NUM_GLOBALVARS> ACS_GlobalArrays;

//----------------------------------------------------------------------------
//
// ACS stack manager
//
// This is needed so that the garbage collector has access to all active
// script stacks
//
//----------------------------------------------------------------------------

using FACSStackMemory = BoundsCheckingArray<int32_t, STACK_SIZE>;

struct FACSStack
{
	FACSStackMemory buffer;
	int sp;
	FACSStack *next;
	FACSStack *prev;
	static FACSStack *head;

	FACSStack();
	~FACSStack();
};

FACSStack *FACSStack::head;

FACSStack::FACSStack()
{
	sp = 0;
	next = head;
	prev = NULL;
	head = this;
}

FACSStack::~FACSStack()
{
	if (next != NULL) next->prev = prev;
	if (prev == NULL)
	{
		head = next;
	}
	else
	{
		prev->next = next;
	}
}

//----------------------------------------------------------------------------
//
// Global ACS strings (Formerly known as On the fly strings)
//
// This special string table is part of the global state. Programmatically
// generated strings (e.g. those returned by strparam) are stored here.
// PCD_TAGSTRING also now stores strings in this table instead of simply
// tagging strings with their library ID.
//
// Identical strings map to identical string identifiers.
//
// When the string table needs to grow to hold more strings, a garbage
// collection is first attempted to see if more room can be made to store
// strings without growing. A string is considered in use if any value
// in any of these variable blocks contains a valid ID in the global string
// table:
//   * The active area of the ACS stack
//   * All running scripts' local variables
//   * All map variables
//   * All world variables
//   * All global variables
// It's not important whether or not they are really used as strings, only
// that they might be. A string is also considered in use if its lock count
// is non-zero, even if none of the above variable blocks referenced it.
//
// To keep track of local and map variables for nonresident maps in a hub,
// when a map's state is archived, all strings found in its local and map
// variables are locked. When a map is revisited in a hub, all strings found
// in its local and map variables are unlocked. Locking and unlocking are
// cumulative operations.
//
// What this all means is that:
//   * Strings returned by strparam last indefinitely. No longer do they
//     disappear at the end of the tic they were generated.
//   * You can pass library strings around freely without having to worry
//     about always having the same libraries loaded in the same order on
//     every map that needs to use those strings.
//
//----------------------------------------------------------------------------

ACSStringPool GlobalACSStrings;

void ACSStringPool::PoolEntry::Lock(int levelnum)
{
	if (Locks.Find(levelnum) == Locks.Size())
	{
		Locks.Push(levelnum);
	}
}

void ACSStringPool::PoolEntry::Unlock(int levelnum)
{
	auto ndx = Locks.Find(levelnum);
	if (ndx < Locks.Size())
	{
		Locks.Delete(ndx);
	}
}

ACSStringPool::ACSStringPool()
{
	memset(PoolBuckets, 0xFF, sizeof(PoolBuckets));
	FirstFreeEntry = 0;
}

//============================================================================
//
// ACSStringPool :: Clear
//
// Remove all strings from the pool.
//
//============================================================================

void ACSStringPool::Clear()
{
	Pool.Clear();
	memset(PoolBuckets, 0xFF, sizeof(PoolBuckets));
	FirstFreeEntry = 0;
}

//============================================================================
//
// ACSStringPool :: AddString
//
// Returns a valid string identifier (including library ID) or -1 if we ran
// out of room. Identical strings will return identical values.
//
//============================================================================

int ACSStringPool::AddString(const char *str)
{
	if (str == nullptr) str = "";
	size_t len = strlen(str);
	unsigned int h = SuperFastHash(str, len);
	unsigned int bucketnum = h % NUM_BUCKETS;
	int i = FindString(str, len, h, bucketnum);
	if (i >= 0)
	{
		return i | STRPOOL_LIBRARYID_OR;
	}
	FString fstr(str);
	return InsertString(fstr, h, bucketnum);
}

int ACSStringPool::AddString(FString &str)
{
	unsigned int h = SuperFastHash(str.GetChars(), str.Len());
	unsigned int bucketnum = h % NUM_BUCKETS;
	int i = FindString(str, str.Len(), h, bucketnum);
	if (i >= 0)
	{
		return i | STRPOOL_LIBRARYID_OR;
	}
	return InsertString(str, h, bucketnum);
}

//============================================================================
//
// ACSStringPool :: GetString
//
//============================================================================

const char *ACSStringPool::GetString(int strnum)
{
	assert((strnum & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR);
	strnum &= ~LIBRARYID_MASK;
	if ((unsigned)strnum < Pool.Size() && Pool[strnum].Next != FREE_ENTRY)
	{
		return Pool[strnum].Str;
	}
	return NULL;
}

//============================================================================
//
// ACSStringPool :: LockString
//
// Prevents this string from being purged.
//
//============================================================================

void ACSStringPool::LockString(int levelnum, int strnum)
{
	assert((strnum & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR);
	strnum &= ~LIBRARYID_MASK;
	assert((unsigned)strnum < Pool.Size());
	Pool[strnum].Lock(levelnum);
}

//============================================================================
//
// ACSStringPool :: MarkString
//
// Prevent this string from being purged during the next call to PurgeStrings.
// This does not carry over to subsequent calls of PurgeStrings.
//
//============================================================================

void ACSStringPool::MarkString(int strnum)
{
	assert((strnum & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR);
	strnum &= ~LIBRARYID_MASK;
	assert((unsigned)strnum < Pool.Size());
	Pool[strnum].Mark = true;
}

//============================================================================
//
// ACSStringPool :: LockStringArray
//
// Prevents several strings from being purged. Entries not in this pool will
// be silently ignored. The idea here is to pass this function a block of
// ACS variables. Everything that looks like it might be a string in the pool
// is locked, even if it's not actually used as such. It's better to keep
// more strings than we need than to throw away ones we do need.
//
//============================================================================

void ACSStringPool::LockStringArray(int levelnum, const int *strnum, unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i)
	{
		int num = strnum[i];
		if ((num & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR)
		{
			num &= ~LIBRARYID_MASK;
			if ((unsigned)num < Pool.Size())
			{
				Pool[num].Lock(levelnum);
			}
		}
	}
}

//============================================================================
//
// ACSStringPool :: MarkStringArray
//
// Array version of MarkString.
//
//============================================================================

void ACSStringPool::MarkStringArray(const int *strnum, unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i)
	{
		int num = strnum[i];
		if ((num & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR)
		{
			num &= ~LIBRARYID_MASK;
			if ((unsigned)num < Pool.Size())
			{
				Pool[num].Mark = true;
			}
		}
	}
}

//============================================================================
//
// ACSStringPool :: MarkStringMap
//
// World/global variables version of MarkString.
//
//============================================================================

void ACSStringPool::MarkStringMap(const FWorldGlobalArray &aray)
{
	FWorldGlobalArray::ConstIterator it(aray);
	FWorldGlobalArray::ConstPair *pair;

	while (it.NextPair(pair))
	{
		int num = pair->Value;
		if ((num & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR)
		{
			num &= ~LIBRARYID_MASK;
			if ((unsigned)num < Pool.Size())
			{
				Pool[num].Mark |= true;
			}
		}
	}
}

//============================================================================
//
// ACSStringPool :: UnlockAll
//
// Resets every entry's lock count to 0. Used when doing a partial reset of
// ACS state such as travelling to a new hub.
//
//============================================================================

void ACSStringPool::UnlockAll()
{
	for (unsigned int i = 0; i < Pool.Size(); ++i)
	{
		Pool[i].Mark = false;
		Pool[i].Locks.Clear();
	}
}

//============================================================================
//
// ACSStringPool :: PurgeStrings
//
// Remove all unlocked strings from the pool.
//
//============================================================================

void ACSStringPool::PurgeStrings()
{
	// Clear the hash buckets. We'll rebuild them as we decide what strings
	// to keep and which to toss.
	memset(PoolBuckets, 0xFF, sizeof(PoolBuckets));
	size_t usedcount = 0, freedcount = 0;
	for (unsigned int i = 0; i < Pool.Size(); ++i)
	{
		PoolEntry *entry = &Pool[i];
		if (entry->Next != FREE_ENTRY)
		{
			if (entry->Locks.Size() == 0 && !entry->Mark)
			{
				freedcount++;
				// Mark this entry as free.
				entry->Next = FREE_ENTRY;
				if (i < FirstFreeEntry)
				{
					FirstFreeEntry = i;
				}
				// And free the string.
				entry->Str = "";
			}
			else
			{
				usedcount++;
				// Rehash this entry.
				unsigned int h = entry->Hash % NUM_BUCKETS;
				entry->Next = PoolBuckets[h];
				PoolBuckets[h] = i;
				// Remove MarkString's mark.
				entry->Mark = false;
			}
		}
	}
}

//============================================================================
//
// ACSStringPool :: FindString
//
// Finds a string in the pool. Does not include the library ID in the returned
// value. Returns -1 if the string does not exist in the pool.
//
//============================================================================

int ACSStringPool::FindString(const char *str, size_t len, unsigned int h, unsigned int bucketnum)
{
	unsigned int i = PoolBuckets[bucketnum];
	while (i != NO_ENTRY)
	{
		PoolEntry *entry = &Pool[i];
		assert(entry->Next != FREE_ENTRY);
		if (entry->Hash == h && entry->Str.Len() == len &&
			memcmp(entry->Str.GetChars(), str, len) == 0)
		{
			return i;
		}
		i = entry->Next;
	}
	return -1;
}

//============================================================================
//
// ACSStringPool :: InsertString
//
// Inserts a new string into the pool.
//
//============================================================================

int ACSStringPool::InsertString(FString &str, unsigned int h, unsigned int bucketnum)
{
	unsigned int index = FirstFreeEntry;
	if (index >= MIN_GC_SIZE && index == Pool.Max())
	{ // We will need to grow the array. Try a garbage collection first.
		P_CollectACSGlobalStrings();
		index = FirstFreeEntry;
	}
	if (FirstFreeEntry >= STRPOOL_LIBRARYID_OR)
	{ // If we go any higher, we'll collide with the library ID marker.
		return -1;
	}
	if (index == Pool.Size())
	{ // There were no free entries; make a new one.
		Pool.Reserve(MIN_GC_SIZE);
		FirstFreeEntry++;
	}
	else
	{ // Scan for the next free entry
		FindFirstFreeEntry(FirstFreeEntry + 1);
	}
	PoolEntry *entry = &Pool[index];
	entry->Str = str;
	entry->Hash = h;
	entry->Next = PoolBuckets[bucketnum];
	entry->Mark = false;
	entry->Locks.Clear();
	PoolBuckets[bucketnum] = index;
	return index | STRPOOL_LIBRARYID_OR;
}

//============================================================================
//
// ACSStringPool :: FindFirstFreeEntry
//
// Finds the first free entry, starting at base.
//
//============================================================================

void ACSStringPool::FindFirstFreeEntry(unsigned base)
{
	while (base < Pool.Size() && Pool[base].Next != FREE_ENTRY)
	{
		base++;
	}
	FirstFreeEntry = base;
}

//============================================================================
//
// ACSStringPool :: ReadStrings
//
// Reads strings from a PNG chunk.
//
//============================================================================

void ACSStringPool::ReadStrings(FSerializer &file, const char *key)
{
	Clear();

	if (file.BeginObject(key))
	{
		int poolsize = 0;

		file("poolsize", poolsize);
		Pool.Resize(poolsize);
		for (auto &p : Pool)
		{
			p.Next = FREE_ENTRY;
			p.Mark = false;
			p.Locks.Clear();
		}
		if (file.BeginArray("pool"))
		{
			int j = file.ArraySize();
			for (int i = 0; i < j; i++)
			{
				if (file.BeginObject(nullptr))
				{
					unsigned ii = UINT_MAX;
					file("index", ii);
					if (ii < Pool.Size())
					{
						file("string", Pool[ii].Str)
							("locks", Pool[ii].Locks);

						unsigned h = SuperFastHash(Pool[ii].Str, Pool[ii].Str.Len());
						unsigned bucketnum = h % NUM_BUCKETS;
						Pool[ii].Hash = h;
						Pool[ii].Next = PoolBuckets[bucketnum];
						PoolBuckets[bucketnum] = ii;
					}
					file.EndObject();
				}
			}
		}
	}

	FindFirstFreeEntry(FirstFreeEntry);
}

//============================================================================
//
// ACSStringPool :: WriteStrings
//
// Writes strings to a serializer
//
//============================================================================

void ACSStringPool::WriteStrings(FSerializer &file, const char *key) const
{
	int32_t i, poolsize = (int32_t)Pool.Size();
	
	if (poolsize == 0)
	{ // No need to write if we don't have anything.
		return;
	}
	if (file.BeginObject(key))
	{
		file("poolsize", poolsize);
		if (file.BeginArray("pool"))
		{
			for (i = 0; i < poolsize; ++i)
			{
				PoolEntry *entry = &Pool[i];
				if (entry->Next != FREE_ENTRY)
				{
					if (file.BeginObject(nullptr))
					{
						file("index", i)
							("string", entry->Str)
							("locks", entry->Locks)
							.EndObject();
					}
				}
			}
			file.EndArray();
		}
		file.EndObject();
	}
}

//============================================================================
//
// ACSStringPool :: Dump
//
// Lists all strings in the pool.
//
//============================================================================

void ACSStringPool::Dump() const
{
	for (unsigned int i = 0; i < Pool.Size(); ++i)
	{
		if (Pool[i].Next != FREE_ENTRY)
		{
			Printf("%4u. (%2d) \"%s\"\n", i, Pool[i].Locks.Size(), Pool[i].Str.GetChars());
		}
	}
	Printf("First free %u\n", FirstFreeEntry);
}


void ACSStringPool::UnlockForLevel(int lnum)
{
	for (unsigned int i = 0; i < Pool.Size(); ++i)
	{
		if (Pool[i].Next != FREE_ENTRY)
		{
			auto ndx = Pool[i].Locks.Find(lnum);
			if (ndx < Pool[i].Locks.Size())
			{
				Pool[i].Locks.Delete(ndx);
			}
		}
	}
}

//============================================================================
//
// P_MarkWorldVarStrings
//
//============================================================================

void P_MarkWorldVarStrings()
{
	GlobalACSStrings.MarkStringArray(ACS_WorldVars.Pointer(), ACS_WorldVars.Size());
	for (size_t i = 0; i < ACS_WorldArrays.Size(); ++i)
	{
		GlobalACSStrings.MarkStringMap(ACS_WorldArrays.Pointer()[i]);
	}
}

//============================================================================
//
// P_MarkGlobalVarStrings
//
//============================================================================

void P_MarkGlobalVarStrings()
{
	GlobalACSStrings.MarkStringArray(ACS_GlobalVars.Pointer(), ACS_GlobalVars.Size());
	for (size_t i = 0; i < ACS_GlobalArrays.Size(); ++i)
	{
		GlobalACSStrings.MarkStringMap(ACS_GlobalArrays.Pointer()[i]);
	}
}

//============================================================================
//
// P_CollectACSGlobalStrings
//
// Garbage collect ACS global strings.
//
//============================================================================

void P_CollectACSGlobalStrings()
{
	for (FACSStack *stack = FACSStack::head; stack != NULL; stack = stack->next)
	{
		const int32_t sp = stack->sp;

		if (0 == sp)
		{
			continue;
		}
		else if (sp < 0 && sp >= STACK_SIZE)
		{
			I_Error("Corrupted stack pointer in ACS VM");
		}
		else
		{
			GlobalACSStrings.MarkStringArray(&stack->buffer[0], sp);
		}
	}
	for(auto Level : AllLevels())
	{ 
		Level->Behaviors.MarkLevelVarStrings();
	}
	P_MarkWorldVarStrings();
	P_MarkGlobalVarStrings();
	GlobalACSStrings.PurgeStrings();
}

#ifdef _DEBUG
CCMD(acsgc)
{
	P_CollectACSGlobalStrings();
}
CCMD(globstr)
{
	GlobalACSStrings.Dump();
}
#endif

//============================================================================
//
// ScriptPresentation
//
// Returns a presentable version of the script number.
//
//============================================================================

static FString ScriptPresentation(int script)
{
	FString out = "script ";

	if (script < 0)
	{
		FName scrname = FName(ENamedName(-script));
		if (scrname.IsValidName())
		{
			out << '"' << scrname.GetChars() << '"';
			return out;
		}
	}
	out.AppendFormat("%d", script);
	return out;
}

//============================================================================
//
// P_ClearACSVars
//
//============================================================================

void P_ClearACSVars(bool alsoglobal)
{
	int i;

	ACS_WorldVars.Fill(0);
	for (i = 0; i < NUM_WORLDVARS; ++i)
	{
		ACS_WorldArrays[i].Clear ();
	}
	if (alsoglobal)
	{
		ACS_GlobalVars.Fill(0);
		for (i = 0; i < NUM_GLOBALVARS; ++i)
		{
			ACS_GlobalArrays[i].Clear ();
		}
		// Since we cleared all ACS variables, we know nothing refers to them
		// anymore.
		GlobalACSStrings.Clear();
	}
	else
	{
		// Purge any strings that aren't referenced by global variables, since
		// they're the only possible references left.
		P_MarkGlobalVarStrings();
		GlobalACSStrings.PurgeStrings();
	}
}

//============================================================================
//
// WriteVars
//
//============================================================================

static void WriteVars (FSerializer &file, int32_t *vars, size_t count, const char *key)
{
	size_t i, j;

	for (i = 0; i < count; ++i)
	{
		if (vars[i] != 0)
			break;
	}
	if (i < count)
	{
		// Find last non-zero var. Anything beyond the last stored variable
		// will be zeroed at load time.
		for (j = count-1; j > i; --j)
		{
			if (vars[j] != 0)
				break;
		}
		file.Array(key, vars, int(j+1));
	}
}

//============================================================================
//
//
//
//============================================================================

static void ReadVars (FSerializer &arc, int32_t *vars, size_t count, const char *key)
{
	memset(&vars[0], 0, count * 4);
	arc.Array(key, vars, (int)count);
}

//============================================================================
//
//
//
//============================================================================

static void WriteArrayVars (FSerializer &file, FWorldGlobalArray *vars, unsigned int count, const char *key)
{
	unsigned int i;

	// Find the first non-empty array.
	for (i = 0; i < count; ++i)
	{
		if (vars[i].CountUsed() != 0)
			break;
	}
	if (i < count)
	{
		if (file.BeginObject(key))
		{
			for(;i<count;i++)
			{
				if (vars[i].CountUsed())
				{
					FString arraykey;

					arraykey.Format("%d", i);
					if (file.BeginObject(arraykey))
					{
						FWorldGlobalArray::ConstIterator it(vars[i]);
						const FWorldGlobalArray::Pair *pair;

						while (it.NextPair(pair))
						{
							arraykey.Format("%d", pair->Key);
							int v = pair->Value;
							file(arraykey.GetChars(), v);
						}
						file.EndObject();
					}
				}
			}
			file.EndObject();
		}
	}
}

//============================================================================
//
//
//
//============================================================================

static void ReadArrayVars (FSerializer &file, FWorldGlobalArray *vars, size_t count, const char *key)
{
	for (size_t i = 0; i < count; ++i)
	{
		vars[i].Clear();
	}

	if (file.BeginObject(key))
	{
		const char *arraykey;
		while ((arraykey = file.GetKey()))
		{
			int i = (int)strtoll(arraykey, nullptr, 10);
			if (file.BeginObject(nullptr))
			{
				while ((arraykey = file.GetKey()))
				{
					int k = (int)strtoll(arraykey, nullptr, 10);
					int val;
					file(nullptr, val);
					vars[i].Insert(k, val);
				}
				file.EndObject();
			}
		}
		file.EndObject();
	}
}

//============================================================================
//
//
//
//============================================================================

void P_ReadACSVars(FSerializer &arc)
{
	ReadVars (arc, ACS_WorldVars.Pointer(), NUM_WORLDVARS, "acsworldvars");
	ReadVars (arc, ACS_GlobalVars.Pointer(), NUM_GLOBALVARS, "acsglobalvars");
	ReadArrayVars (arc, ACS_WorldArrays.Pointer(), NUM_WORLDVARS, "acsworldarrays");
	ReadArrayVars (arc, ACS_GlobalArrays.Pointer(), NUM_GLOBALVARS, "acsglobalarrays");
	GlobalACSStrings.ReadStrings(arc, "acsglobalstrings");
}

//============================================================================
//
//
//
//============================================================================

void P_WriteACSVars(FSerializer &arc)
{
	WriteVars (arc, ACS_WorldVars.Pointer(), NUM_WORLDVARS, "acsworldvars");
	WriteVars (arc, ACS_GlobalVars.Pointer(), NUM_GLOBALVARS, "acsglobalvars");
	WriteArrayVars (arc, ACS_WorldArrays.Pointer(), NUM_WORLDVARS, "acsworldarrays");
	WriteArrayVars (arc, ACS_GlobalArrays.Pointer(), NUM_GLOBALVARS, "acsglobalarrays");
	GlobalACSStrings.WriteStrings(arc, "acsglobalstrings");
}

//---- Inventory functions --------------------------------------//
//

//============================================================================
//
// DoUseInv
//
// Makes a single actor use an inventory item
//
//============================================================================

static bool DoUseInv (AActor *actor, PClassActor *info)
{
	auto item = actor->FindInventory (info);
	if (item != NULL)
	{
		player_t* const player = actor->player;

		if (nullptr == player)
		{
			return actor->UseInventory(item);
		}
		else
		{
			int cheats;
			bool res;

			// Bypass CF_TOTALLYFROZEN
			cheats = player->cheats;
			player->cheats &= ~CF_TOTALLYFROZEN;
			res = actor->UseInventory(item);
			player->cheats |= (cheats & CF_TOTALLYFROZEN);
			return res;
		}
	}
	return false;
}

//============================================================================
//
// UseInventory
//
// makes one or more actors use an inventory item.
//
//============================================================================

static int UseInventory (FLevelLocals *Level, AActor *activator, const char *type)
{
	PClassActor *info;
	int ret = 0;

	if (type == NULL)
	{
		return 0;
	}
	info = PClass::FindActor (type);
	if (info == NULL)
	{
		return 0;
	}
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (Level->PlayerInGame(i))
				ret += DoUseInv (Level->Players[i]->mo, info);
		}
	}
	else
	{
		ret = DoUseInv (activator, info);
	}
	return ret;
}

//============================================================================
//
// CheckInventory
//
// Returns how much of a particular item an actor has.
// This also gets called from FraggleScript.
//
//============================================================================

int CheckInventory (AActor *activator, const char *type, bool max)
{
	if (activator == NULL || type == NULL)
		return 0;

	if (stricmp (type, "Armor") == 0)
	{
		type = "BasicArmor";
	}
	else if (stricmp (type, "Health") == 0)
	{
		if (max)
		{
			return activator->GetMaxHealth();
		}
		return activator->health;
	}

	PClassActor *info = PClass::FindActor (type);

	if (info == NULL)
	{
		DPrintf (DMSG_ERROR, "ACS: '%s': Unknown actor class.\n", type);
		return 0;
	}
	else if (!info->IsDescendantOf(NAME_Inventory))
	{
		DPrintf(DMSG_ERROR, "ACS: '%s' is not an inventory item.\n", type);
		return 0;
	}

	auto item = activator->FindInventory (info);

	if (max)
	{
		if (item)
		{
			return item->IntVar(NAME_MaxAmount);
		}
		else if (info != nullptr && info->IsDescendantOf(NAME_Inventory))
		{
			return GetDefaultByType(info)->IntVar(NAME_MaxAmount);
		}
	}
	return item ? item->IntVar(NAME_Amount) : 0;
}

//---- Plane watchers ----//

class DPlaneWatcher : public DThinker
{
	DECLARE_CLASS (DPlaneWatcher, DThinker)
	HAS_OBJECT_POINTERS
public:
	void Construct(AActor *it, line_t *line, int lineSide, bool ceiling,
		sector_t *sec, int height, int special,
		int arg0, int arg1, int arg2, int arg3, int arg4);
	void Tick ();
	void Serialize(FSerializer &arc);
private:
	sector_t *Sector;
	double WatchD, LastD;
	int Special;
	int Args[5];
	TObjPtr<AActor*> Activator;
	line_t *Line;
	bool LineSide;
	bool bCeiling;

	DPlaneWatcher() = default;
};

IMPLEMENT_CLASS(DPlaneWatcher, false, true)

IMPLEMENT_POINTERS_START(DPlaneWatcher)
	IMPLEMENT_POINTER(Activator)
IMPLEMENT_POINTERS_END

void DPlaneWatcher::Construct(AActor *it, line_t *line, int lineSide, bool ceiling,
	sector_t *sec, int height, int special, int arg0, int arg1, int arg2, int arg3, int arg4)
{
	secplane_t plane;

	Special = special;
	Activator = it;
	Line = line;
	LineSide = !!lineSide;
	bCeiling = ceiling;

	Args[0] = arg0;
	Args[1] = arg1;
	Args[2] = arg2;
	Args[3] = arg3;
	Args[4] = arg4;

	Sector = sec;
	if (bCeiling)
	{
		plane = Sector->ceilingplane;
	}
	else
	{
		plane = Sector->floorplane;
	}
	LastD = plane.fD();
	plane.ChangeHeight (height);
	WatchD = plane.fD();
}

void DPlaneWatcher::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("special", Special)
		("sector", Sector)
		("ceiling", bCeiling)
		("watchd", WatchD)
		("lastd", LastD)
		("activator", Activator)
		("line", Line)
		("lineside", LineSide);

	SerializeArgs(arc, "args", Args, nullptr, Special);
}

void DPlaneWatcher::Tick ()
{
	if (Sector == NULL)
	{
		Destroy ();
		return;
	}

	double newd;

	if (bCeiling)
	{
		newd = Sector->ceilingplane.fD();
	}
	else
	{
		newd = Sector->floorplane.fD();
	}

	if ((LastD < WatchD && newd >= WatchD) ||
		(LastD > WatchD && newd <= WatchD))
	{
		P_ExecuteSpecial(Level, Special, Line, Activator, LineSide, Args[0], Args[1], Args[2], Args[3], Args[4]);
		Destroy ();
	}

}

//---- ACS lump manager ----//

// Load user-specified default modules. This must be called after the level's
// own behavior is loaded (if it has one).
void FBehaviorContainer::LoadDefaultModules ()
{
	// Scan each LOADACS lump and load the specified modules in order
	int lump, lastlump = 0;

	while ((lump = fileSystem.FindLump ("LOADACS", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetString())
		{
			int acslump = fileSystem.CheckNumForName (sc.String, ns_acslibrary);
			if (acslump >= 0)
			{
				LoadModule (acslump);
			}
			else
			{
				Printf (TEXTCOLOR_RED "Could not find autoloaded ACS library %s\n", sc.String);
			}
		}
	}
}

FBehavior *FBehaviorContainer::LoadModule (int lumpnum, FileReader *fr, int len, int reallumpnum)
{
	if (lumpnum == -1 && fr == NULL) return NULL;

	for (unsigned int i = 0; i < StaticModules.Size(); ++i)
	{
		if (StaticModules[i]->LumpNum == lumpnum)
		{
			return StaticModules[i];
		}
	}

	FBehavior * behavior = new FBehavior ();
	if (behavior->Init(Level, lumpnum, fr, len, reallumpnum))
	{
		return behavior;
	}
	else
	{
		delete behavior;
		Printf(TEXTCOLOR_RED "%s: invalid ACS module\n", fileSystem.GetFileFullName(lumpnum));
		return NULL;
	}
}

bool FBehaviorContainer::CheckAllGood ()
{
	for (unsigned int i = 0; i < StaticModules.Size(); ++i)
	{
		if (!StaticModules[i]->IsGood())
		{
			return false;
		}
	}
	return true;
}

void FBehaviorContainer::UnloadModules ()
{
	for (unsigned int i = StaticModules.Size(); i-- > 0; )
	{
		delete StaticModules[i];
	}
	StaticModules.Clear ();
}

FBehavior *FBehaviorContainer::GetModule (int lib)
{
	if ((size_t)lib >= StaticModules.Size())
	{
		return NULL;
	}
	return StaticModules[lib];
}

void FBehaviorContainer::MarkLevelVarStrings()
{
	// Mark map variables.
	for (uint32_t modnum = 0; modnum < StaticModules.Size(); ++modnum)
	{
		StaticModules[modnum]->MarkMapVarStrings();
	}
	// Mark running scripts' local variables.
	if (Level->ACSThinker != nullptr)
	{
		for (DLevelScript *script = Level->ACSThinker->Scripts; script != NULL; script = script->GetNext())
		{
			script->MarkLocalVarStrings();
		}
	}
}

void FBehaviorContainer::LockLevelVarStrings(int levelnum)
{
	// Lock map variables.
	for (uint32_t modnum = 0; modnum < StaticModules.Size(); ++modnum)
	{
		StaticModules[modnum]->LockMapVarStrings(levelnum);
	}
	// Lock running scripts' local variables.
	if (Level->ACSThinker != nullptr)
	{
		for (DLevelScript *script = Level->ACSThinker->Scripts; script != NULL; script = script->GetNext())
		{
			script->LockLocalVarStrings(levelnum);
		}
	}
}

void FBehaviorContainer::UnlockLevelVarStrings(int levelnum)
{
	GlobalACSStrings.UnlockForLevel(levelnum);
}

void FBehavior::MarkMapVarStrings() const
{
	GlobalACSStrings.MarkStringArray(MapVarStore, NUM_MAPVARS);
	for (int i = 0; i < NumArrays; ++i)
	{
		GlobalACSStrings.MarkStringArray(ArrayStore[i].Elements, ArrayStore[i].ArraySize);
	}
}

void FBehavior::LockMapVarStrings(int levelnum) const
{
	GlobalACSStrings.LockStringArray(levelnum, MapVarStore, NUM_MAPVARS);
	for (int i = 0; i < NumArrays; ++i)
	{
		GlobalACSStrings.LockStringArray(levelnum, ArrayStore[i].Elements, ArrayStore[i].ArraySize);
	}
}

void FBehaviorContainer::SerializeModuleStates (FSerializer &arc)
{
	auto modnum = StaticModules.Size();

	if (arc.BeginArray("acsmodules"))
	{
		if (arc.isReading())
		{
			auto modnum = arc.ArraySize();
			if (modnum != StaticModules.Size())
			{
				I_Error("Level was saved with a different number of ACS modules. (Have %d, save has %d)", StaticModules.Size(), modnum);
			}
		}

		for (modnum = 0; modnum < StaticModules.Size(); ++modnum)
		{
			FBehavior *module = StaticModules[modnum];
			const char *modname = module->ModuleName;
			int ModSize = module->GetDataSize();

			if (arc.BeginObject(nullptr))
			{
				arc.StringPtr("modname", modname)
					("modsize", ModSize);

				if (arc.isReading())
				{
					if (stricmp(modname, module->ModuleName) != 0)
					{
						I_Error("Level was saved with a different set or order of ACS modules. (Have %s, save has %s)", module->ModuleName, modname);
					}
					else if (ModSize != module->GetDataSize())
					{
						I_Error("ACS module %s has changed from what was saved. (Have %d bytes, save has %d bytes)", module->ModuleName, module->GetDataSize(), ModSize);
					}
				}
				module->SerializeVars(arc);
				arc.EndObject();
			}
		}
		arc.EndArray();
	}
}

void FBehavior::SerializeVars (FSerializer &arc)
{
	if (arc.BeginArray("variables"))
	{
		SerializeVarSet(arc, MapVarStore, NUM_MAPVARS);
		for (int i = 0; i < NumArrays; ++i)
		{
			SerializeVarSet(arc, ArrayStore[i].Elements, ArrayStore[i].ArraySize);
		}
		arc.EndArray();
	}
}

void FBehavior::SerializeVarSet (FSerializer &arc, int32_t *vars, int max)
{
	int32_t count;
	int32_t first, last;

	if (arc.BeginObject(nullptr))
	{
		if (arc.isWriting())
		{
			// Find first non-zero variable
			for (first = 0; first < max; ++first)
			{
				if (vars[first] != 0)
				{
					break;
				}
			}

			// Find last non-zero variable
			for (last = max - 1; last >= first; --last)
			{
				if (vars[last] != 0)
				{
					break;
				}
			}

			if (last < first)
			{ // no non-zero variables
				count = 0;
				arc("count", count);
			}
			else
			{
				count = last - first + 1;
				arc("count", count);
				arc("first", first);
				arc.Array("values", &vars[first], count);
			}
		}
		else
		{
			memset(vars, 0, max * sizeof(*vars));
			arc("count", count);

			if (count != 0)
			{
				arc("first", first);
				if (first + count > max) count = max - first;
				arc.Array("values", &vars[first], count);
			}
		}
		arc.EndObject();
	}
}

static int ParseLocalArrayChunk(void *chunk, ACSLocalArrays *arrays, int offset)
{
	unsigned count = LittleShort(static_cast<unsigned short>(((unsigned *)chunk)[1] - 2)) / 4;
	int *sizes = (int *)((uint8_t *)chunk + 10);
	arrays->Count = count;
	if (count > 0)
	{
		ACSLocalArrayInfo *info = new ACSLocalArrayInfo[count];
		arrays->Info = info;
		for (unsigned i = 0; i < count; ++i)
		{
			info[i].Size = LittleLong(sizes[i]);
			info[i].Offset = offset;
			offset += info[i].Size;
		}
	}
	// Return the new local variable size, with space for the arrays
	return offset;
}

FBehavior::FBehavior()
{
	NumScripts = 0;
	NumFunctions = 0;
	NumArrays = 0;
	NumTotalArrays = 0;
	Scripts = NULL;
	Functions = NULL;
	Arrays = NULL;
	ArrayStore = NULL;
	Chunks = NULL;
	Data = NULL;
	Format = ACS_Unknown;
	LumpNum = -1;
	memset (MapVarStore, 0, sizeof(MapVarStore));
	ModuleName[0] = 0;
	FunctionProfileData = NULL;

}
	
	
bool FBehavior::Init(FLevelLocals *Level, int lumpnum, FileReader * fr, int len, int reallumpnum)
{
	uint8_t *object;
	int i;

	this->Level = Level;
	LumpNum = lumpnum;

	// Now that everything is set up, record this module as being among the loaded modules.
	// We need to do this before resolving any imports, because an import might (indirectly)
	// need to resolve exports in this module. The only things that can be exported are
	// functions and map variables, which must already be present if they're exported, so
	// this is okay.

	// This must be done first for 2 reasons:
	// 1. If not, corrupt modules cause memory leaks
	// 2. Corrupt modules won't be reported when a level is being loaded if this function quits before
	//    adding it to the list.

	if (fr == NULL) len = fileSystem.FileLength (lumpnum);



	// Any behaviors smaller than 32 bytes cannot possibly contain anything useful.
	// (16 bytes for a completely empty behavior + 12 bytes for one script header
	//  + 4 bytes for PCD_TERMINATE for an old-style object. A new-style object
	// has 24 bytes if it is completely empty. An empty SPTR chunk adds 8 bytes.)
	if (len < 32)
	{
		return false;
	}

	object = new uint8_t[len];
	if (fr == NULL)
	{
		fileSystem.ReadFile (lumpnum, object);
	}
	else
	{
		fr->Read (object, len);
	}

	if (object[0] != 'A' || object[1] != 'C' || object[2] != 'S')
	{
		delete[] object;
		return false;
	}

	switch (object[3])
	{
	case 0:
		Format = ACS_Old;
		break;
	case 'E':
		Format = ACS_Enhanced;
		break;
	case 'e':
		Format = ACS_LittleEnhanced;
		break;
	default:
		delete[] object;
		return false;
	}
    LibraryID = Level->Behaviors.StaticModules.Push (this) << LIBRARYID_SHIFT;

	if (fr == NULL)
	{
		fileSystem.GetFileShortName (ModuleName, lumpnum);
		ModuleName[8] = 0;
	}
	else
	{
		strcpy(ModuleName, "BEHAVIOR");
	}

	Data = object;
	DataSize = len;

	if (Format == ACS_Old)
	{
		uint32_t dirofs = LittleLong(((uint32_t *)object)[1]);
		uint32_t pretag = ((uint32_t *)(object + dirofs))[-1];

		Chunks = object + len;
		// Check for redesigned ACSE/ACSe
		if (dirofs >= 6*4 &&
			(pretag == MAKE_ID('A','C','S','e') ||
			 pretag == MAKE_ID('A','C','S','E')))
		{
			Format = (pretag == MAKE_ID('A','C','S','e')) ? ACS_LittleEnhanced : ACS_Enhanced;
			Chunks = object + LittleLong(((uint32_t *)(object + dirofs))[-2]);
			// Forget about the compatibility cruft at the end of the lump
			DataSize = LittleLong(((uint32_t *)object)[1]) - 8;
		}

		ShouldLocalize = false;
	}
	else
	{
		Chunks = object + LittleLong(((uint32_t *)object)[1]);
	}

	LoadScriptsDirectory ();

	if (Format == ACS_Old)
	{
		StringTable = LittleLong(((uint32_t *)Data)[1]);
		StringTable += LittleLong(((uint32_t *)(Data + StringTable))[0]) * 12 + 4;
		UnescapeStringTable(Data + StringTable, Data, false);
		// If this is an original Hexen BEHAVIOR, set up some localization info for it. Original Hexen BEHAVIORs are always in the old format.
		if ((Level->flags2 & LEVEL2_HEXENHACK) && gameinfo.gametype == GAME_Hexen && lumpnum == -1 && reallumpnum > 0)
		{
			int fileno = fileSystem.GetFileContainer(reallumpnum);
			const char * filename = fileSystem.GetResourceFileName(fileno);
			if (!stricmp(filename, "HEXEN.WAD") || !stricmp(filename, "HEXDD.WAD"))
			{
				ShouldLocalize = true;
			}
		}

	}
	else
	{
		UnencryptStrings ();
		uint8_t *strings = FindChunk (MAKE_ID('S','T','R','L'));
		if (strings != NULL)
		{
			StringTable = uint32_t(strings - Data + 8);
			UnescapeStringTable(strings + 8, NULL, true);
		}
		else
		{
			StringTable = 0;
		}
	}

	if (Format == ACS_Old)
	{
		// Do initialization for old-style behavior lumps
		for (i = 0; i < NUM_MAPVARS; ++i)
		{
			MapVars[i] = &MapVarStore[i];
		}
		//LibraryID = StaticModules.Push (this) << LIBRARYID_SHIFT;
	}
	else
	{
		uint32_t *chunk;

		// Load functions
		uint8_t *funcs;
		Functions = NULL;
		funcs = FindChunk (MAKE_ID('F','U','N','C'));
		if (funcs != NULL)
		{
			NumFunctions = LittleLong(((uint32_t *)funcs)[1]) / 8;
			funcs += 8;
			FunctionProfileData = new ACSProfileInfo[NumFunctions];
			Functions = new ScriptFunction[NumFunctions];
			for (i = 0; i < NumFunctions; ++i)
			{
				ScriptFunctionInFile *funcf = &((ScriptFunctionInFile *)funcs)[i];
				ScriptFunction *funcm = &Functions[i];
				funcm->ArgCount = funcf->ArgCount;
				funcm->HasReturnValue = funcf->HasReturnValue;
				funcm->ImportNum = funcf->ImportNum;
				funcm->LocalCount = funcf->LocalCount;
				funcm->Address = LittleLong(funcf->Address);
			}
		}

		// Load local arrays for functions
		if (NumFunctions > 0)
		{
			for (chunk = (uint32_t *)FindChunk(MAKE_ID('F','A','R','Y')); chunk != NULL; chunk = (uint32_t *)NextChunk((uint8_t *)chunk))
			{
				int size = LittleLong(chunk[1]);
				if (size >= 6)
				{
					unsigned int func_num = LittleShort(((uint16_t *)chunk)[4]);
					if (func_num < (unsigned int)NumFunctions)
					{
						ScriptFunction *func = &Functions[func_num];
						// Unlike scripts, functions do not include their arg count in their local count.
						func->LocalCount = ParseLocalArrayChunk(chunk, &func->LocalArrays, func->LocalCount + func->ArgCount) - func->ArgCount;
					}
				}
			}
		}

		// Load JUMP points
		chunk = (uint32_t *)FindChunk (MAKE_ID('J','U','M','P'));
		if (chunk != NULL)
		{
			for (i = 0;i < (int)LittleLong(chunk[1]);i += 4)
				JumpPoints.Push(LittleLong(chunk[2 + i/4]));
		}

		// Initialize this object's map variables
		memset (MapVarStore, 0, sizeof(MapVarStore));
		chunk = (uint32_t *)FindChunk (MAKE_ID('M','I','N','I'));
		while (chunk != NULL)
		{
			int numvars = LittleLong(chunk[1])/4 - 1;
			int firstvar = LittleLong(chunk[2]);
			for (i = 0; i < numvars; ++i)
			{
				MapVarStore[i+firstvar] = LittleLong(chunk[3+i]);
			}
			chunk = (uint32_t *)NextChunk ((uint8_t *)chunk);
		}

		// Initialize this object's map variable pointers to defaults. They can be changed
		// later once the imported modules are loaded.
		for (i = 0; i < NUM_MAPVARS; ++i)
		{
			MapVars[i] = &MapVarStore[i];
		}

		// Create arrays for this module
		chunk = (uint32_t *)FindChunk (MAKE_ID('A','R','A','Y'));
		if (chunk != NULL)
		{
			NumArrays = LittleLong(chunk[1])/8;
			ArrayStore = new ArrayInfo[NumArrays];
			memset (ArrayStore, 0, sizeof(*Arrays)*NumArrays);
			for (i = 0; i < NumArrays; ++i)
			{
				MapVarStore[LittleLong(chunk[2+i*2])] = i;
				ArrayStore[i].ArraySize = LittleLong(chunk[3+i*2]);
				ArrayStore[i].Elements = new int32_t[ArrayStore[i].ArraySize];
				memset(ArrayStore[i].Elements, 0, ArrayStore[i].ArraySize*sizeof(uint32_t));
			}
		}

		// Initialize arrays for this module
		chunk = (uint32_t *)FindChunk (MAKE_ID('A','I','N','I'));
		while (chunk != NULL)
		{
			int arraynum = MapVarStore[LittleLong(chunk[2])];
			if ((unsigned)arraynum < (unsigned)NumArrays)
			{
				// Use unsigned iterator here to avoid issue with GCC 4.9/5.x
				// optimizer. Might be some undefined behavior in this code,
				// but I don't know what it is.
				unsigned int initsize = MIN<unsigned int> (ArrayStore[arraynum].ArraySize, (LittleLong(chunk[1])-4)/4);
				int32_t *elems = ArrayStore[arraynum].Elements;
				for (unsigned int j = 0; j < initsize; ++j)
				{
					elems[j] = LittleLong(chunk[3+j]);
				}
			}
			chunk = (uint32_t *)NextChunk((uint8_t *)chunk);
		}

		// Start setting up array pointers
		NumTotalArrays = NumArrays;
		chunk = (uint32_t *)FindChunk (MAKE_ID('A','I','M','P'));
		if (chunk != NULL)
		{
			NumTotalArrays += LittleLong(chunk[2]);
		}
		if (NumTotalArrays != 0)
		{
			Arrays = new ArrayInfo *[NumTotalArrays];
			for (i = 0; i < NumArrays; ++i)
			{
				Arrays[i] = &ArrayStore[i];
			}
		}

		// Tag the library ID to any map variables that are initialized with strings
		if (LibraryID != 0)
		{
			chunk = (uint32_t *)FindChunk (MAKE_ID('M','S','T','R'));
			if (chunk != NULL)
			{
				for (uint32_t i = 0; i < LittleLong(chunk[1])/4; ++i)
				{
					const char *str = LookupString(MapVarStore[LittleLong(chunk[i+2])]);
					if (str != NULL)
					{
						MapVarStore[LittleLong(chunk[i+2])] = GlobalACSStrings.AddString(str);
					}
				}
			}

			chunk = (uint32_t *)FindChunk (MAKE_ID('A','S','T','R'));
			if (chunk != NULL)
			{
				for (uint32_t i = 0; i < LittleLong(chunk[1])/4; ++i)
				{
					int arraynum = MapVarStore[LittleLong(chunk[i+2])];
					if ((unsigned)arraynum < (unsigned)NumArrays)
					{
						int32_t *elems = ArrayStore[arraynum].Elements;
						for (int j = ArrayStore[arraynum].ArraySize; j > 0; --j, ++elems)
						{
//							*elems |= LibraryID;
							const char *str = LookupString(*elems);
							if (str != NULL)
							{
								*elems = GlobalACSStrings.AddString(str);
							}
						}
					}
				}
			}

			// [BL] Newer version of ASTR for structure aware compilers although we only have one array per chunk
			chunk = (uint32_t *)FindChunk (MAKE_ID('A','T','A','G'));
			while (chunk != NULL)
			{
				const uint8_t* chunkData = (const uint8_t*)(chunk + 2);
				// First byte is version, it should be 0
				if(*chunkData++ == 0)
				{
					int arraynum = MapVarStore[uallong(LittleLong(*(const int*)(chunkData)))];
					chunkData += 4;
					if ((unsigned)arraynum < (unsigned)NumArrays)
					{
						int32_t *elems = ArrayStore[arraynum].Elements;
						// Ending zeros may be left out.
						for (int j = MIN(LittleLong(chunk[1])-5, ArrayStore[arraynum].ArraySize); j > 0; --j, ++elems, ++chunkData)
						{
							// For ATAG, a value of 0 = Integer, 1 = String, 2 = FunctionPtr
							// Our implementation uses the same tags for both String and FunctionPtr
							if (*chunkData == 2)
							{
								*elems |= LibraryID;
							}
							else if (*chunkData == 1)
							{
								const char *str = LookupString(*elems);
								if (str != NULL)
								{
									*elems = GlobalACSStrings.AddString(str);
								}
							}
						}
					}
				}

				chunk = (uint32_t *)NextChunk ((uint8_t *)chunk);
			}
		}

		// Load required libraries.
		if (NULL != (chunk = (uint32_t *)FindChunk (MAKE_ID('L','O','A','D'))))
		{
			const char *const parse = (char *)&chunk[2];
			uint32_t i;

			for (i = 0; i < LittleLong(chunk[1]); )
			{
				if (parse[i])
				{
					FBehavior *module = NULL;
					int lump = fileSystem.CheckNumForName (&parse[i], ns_acslibrary);
					if (lump < 0)
					{
						Printf (TEXTCOLOR_RED "Could not find ACS library %s.\n", &parse[i]);
					}
					else
					{
						module = Level->Behaviors.LoadModule (lump);
					}
					if (module != NULL) Imports.Push (module);
					do {;} while (parse[++i]);
				}
				++i;
			}

			// Go through each imported module in order and resolve all imported functions
			// and map variables.
			for (i = 0; i < Imports.Size(); ++i)
			{
				FBehavior *lib = Imports[i];
				int j;

				if (lib == NULL)
					continue;

				// Resolve functions
				chunk = (uint32_t *)FindChunk(MAKE_ID('F','N','A','M'));
				for (j = 0; j < NumFunctions; ++j)
				{
					ScriptFunction *func = &((ScriptFunction *)Functions)[j];
					if (func->Address == 0 && func->ImportNum == 0)
					{
						int libfunc = lib->FindFunctionName ((char *)(chunk + 2) + LittleLong(chunk[3+j]));
						if (libfunc >= 0)
						{
							ScriptFunction *realfunc = &((ScriptFunction *)lib->Functions)[libfunc];
							// Make sure that the library really defines this function. It might simply
							// be importing it itself.
							if (realfunc->Address != 0 && realfunc->ImportNum == 0)
							{
								func->Address = libfunc;
								func->ImportNum = i+1;
								if (realfunc->ArgCount != func->ArgCount)
								{
									Printf (TEXTCOLOR_ORANGE "Function %s in %s has %d arguments. %s expects it to have %d.\n",
										(char *)(chunk + 2) + LittleLong(chunk[3+j]), lib->ModuleName, realfunc->ArgCount,
										ModuleName, func->ArgCount);
									Format = ACS_Unknown;
								}
								// The next two properties do not affect code compatibility, so it is
								// okay for them to be different in the imported module than they are
								// in this one, as long as we make sure to use the real values.
								func->LocalCount = LittleLong(realfunc->LocalCount);
								func->HasReturnValue = realfunc->HasReturnValue;
							}
						}
					}
				}

				// Resolve map variables
				chunk = (uint32_t *)FindChunk(MAKE_ID('M','I','M','P'));
				if (chunk != NULL)
				{
					char *parse = (char *)&chunk[2];
					for (uint32_t j = 0; j < LittleLong(chunk[1]); )
					{
						uint32_t varNum = LittleLong(*(uint32_t *)&parse[j]);
						j += 4;
						int impNum = lib->FindMapVarName (&parse[j]);
						if (impNum >= 0)
						{
							MapVars[varNum] = &lib->MapVarStore[impNum];
						}
						do {;} while (parse[++j]);
						++j;
					}
				}

				// Resolve arrays
				if (NumTotalArrays > NumArrays)
				{
					chunk = (uint32_t *)FindChunk(MAKE_ID('A','I','M','P'));
					char *parse = (char *)&chunk[3];
					for (uint32_t j = 0; j < LittleLong(chunk[2]); ++j)
					{
						uint32_t varNum = LittleLong(*(uint32_t *)parse);
						parse += 4;
						uint32_t expectedSize = LittleLong(*(uint32_t *)parse);
						parse += 4;
						int impNum = lib->FindMapArray (parse);
						if (impNum >= 0)
						{
							Arrays[NumArrays + j] = &lib->ArrayStore[impNum];
							MapVarStore[varNum] = NumArrays + j;
							if (lib->ArrayStore[impNum].ArraySize != expectedSize)
							{
								Format = ACS_Unknown;
								Printf (TEXTCOLOR_ORANGE "The array %s in %s has %u elements, but %s expects it to only have %u.\n",
									parse, lib->ModuleName, lib->ArrayStore[impNum].ArraySize,
									ModuleName, expectedSize);
							}
						}
						do {;} while (*++parse);
						++parse;
					}
				}
			}
		}
	}

	DPrintf (DMSG_NOTIFY, "Loaded %d scripts, %d functions\n", NumScripts, NumFunctions);
	return true;
}

FBehavior::~FBehavior ()
{
	if (Scripts != NULL)
	{
		delete[] Scripts;
		Scripts = NULL;
	}
	if (Arrays != NULL)
	{
		delete[] Arrays;
		Arrays = NULL;
	}
	if (ArrayStore != NULL)
	{
		for (int i = 0; i < NumArrays; ++i)
		{
			if (ArrayStore[i].Elements != NULL)
			{
				delete[] ArrayStore[i].Elements;
				ArrayStore[i].Elements = NULL;
			}
		}
		delete[] ArrayStore;
		ArrayStore = NULL;
	}
	if (Functions != NULL)
	{
		delete[] Functions;
		Functions = NULL;
	}
	if (FunctionProfileData != NULL)
	{
		delete[] FunctionProfileData;
		FunctionProfileData = NULL;
	}
	if (Data != NULL)
	{
		delete[] Data;
		Data = NULL;
	}
}

void FBehavior::LoadScriptsDirectory ()
{
	union
	{
		uint8_t *b;
		uint32_t *dw;
		uint16_t *w;
		int16_t *sw;
		ScriptPtr2 *po;		// Old
		ScriptPtr1 *pi;		// Intermediate
		ScriptPtr3 *pe;		// LittleEnhanced
	} scripts;
	int i, max;

	NumScripts = 0;
	Scripts = NULL;

	// Load the main script directory
	switch (Format)
	{
	case ACS_Old:
		scripts.dw = (uint32_t *)(Data + LittleLong(((uint32_t *)Data)[1]));
		NumScripts = LittleLong(scripts.dw[0]);
		if (NumScripts != 0)
		{
			scripts.dw++;

			Scripts = new ScriptPtr[NumScripts];

			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr2 *ptr1 = &scripts.po[i];
				ScriptPtr  *ptr2 = &Scripts[i];

				ptr2->Number = LittleLong(ptr1->Number) % 1000;
				ptr2->Type = LittleLong(ptr1->Number) / 1000;
				ptr2->ArgCount = LittleLong(ptr1->ArgCount);
				ptr2->Address = LittleLong(ptr1->Address);
			}
		}
		break;

	case ACS_Enhanced:
	case ACS_LittleEnhanced:
		scripts.b = FindChunk (MAKE_ID('S','P','T','R'));
		if (scripts.b == NULL)
		{
			// There are no scripts!
		}
		else if (*(uint32_t *)Data != MAKE_ID('A','C','S',0))
		{
			NumScripts = LittleLong(scripts.dw[1]) / 12;
			Scripts = new ScriptPtr[NumScripts];
			scripts.dw += 2;

			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr1 *ptr1 = &scripts.pi[i];
				ScriptPtr  *ptr2 = &Scripts[i];

				ptr2->Number = LittleShort(ptr1->Number);
				ptr2->Type = uint8_t(LittleShort(ptr1->Type));
				ptr2->ArgCount = LittleLong(ptr1->ArgCount);
				ptr2->Address = LittleLong(ptr1->Address);
			}
		}
		else
		{
			NumScripts = LittleLong(scripts.dw[1]) / 8;
			Scripts = new ScriptPtr[NumScripts];
			scripts.dw += 2;

			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr3 *ptr1 = &scripts.pe[i];
				ScriptPtr  *ptr2 = &Scripts[i];

				ptr2->Number = LittleShort(ptr1->Number);
				ptr2->Type = ptr1->Type;
				ptr2->ArgCount = ptr1->ArgCount;
				ptr2->Address = LittleLong(ptr1->Address);
			}
		}
		break;

	default:
		break;
	}

// [EP] Clang 3.5.0 optimizer miscompiles this function and causes random
// crashes in the program. This is fixed in 3.5.1 onwards.
#if defined(__clang__) && __clang_major__ == 3 && __clang_minor__ == 5 && __clang_patchlevel__ == 0
	asm("" : "+g" (NumScripts));
#endif
	for (i = 0; i < NumScripts; ++i)
	{
		Scripts[i].Flags = 0;
		Scripts[i].VarCount = LOCAL_SIZE;
	}

	// Sort scripts, so we can use a binary search to find them
	if (NumScripts > 1)
	{
		qsort (Scripts, NumScripts, sizeof(ScriptPtr), SortScripts);
		// Check for duplicates because ACC originally did not enforce
		// script number uniqueness across different script types. We
		// only need to do this for old format lumps, because the ACCs
		// that produce new format lumps won't let you do this.
		if (Format == ACS_Old)
		{
			for (i = 0; i < NumScripts - 1; ++i)
			{
				if (Scripts[i].Number == Scripts[i+1].Number)
				{
					Printf(TEXTCOLOR_ORANGE "%s appears more than once.\n",
						ScriptPresentation(Scripts[i].Number).GetChars());
					// Make the closed version the first one.
					if (Scripts[i+1].Type == SCRIPT_Closed)
					{
						std::swap(Scripts[i], Scripts[i+1]);
					}
				}
			}
		}
	}

	if (Format == ACS_Old)
		return;

	// Load script flags
	scripts.b = FindChunk (MAKE_ID('S','F','L','G'));
	if (scripts.dw != NULL)
	{
		max = LittleLong(scripts.dw[1]) / 4;
		scripts.dw += 2;
		for (i = max; i > 0; --i, scripts.w += 2)
		{
			ScriptPtr *ptr = const_cast<ScriptPtr *>(FindScript (LittleShort(scripts.sw[0])));
			if (ptr != NULL)
			{
				ptr->Flags = LittleShort(scripts.w[1]);
			}
		}
	}

	// Load script var counts. (Only recorded for scripts that use more than LOCAL_SIZE variables.)
	scripts.b = FindChunk (MAKE_ID('S','V','C','T'));
	if (scripts.dw != NULL)
	{
		max = LittleLong(scripts.dw[1]) / 4;
		scripts.dw += 2;
		for (i = max; i > 0; --i, scripts.w += 2)
		{
			ScriptPtr *ptr = const_cast<ScriptPtr *>(FindScript (LittleShort(scripts.sw[0])));
			if (ptr != NULL)
			{
				ptr->VarCount = LittleShort(scripts.w[1]);
			}
		}
	}

	// Load script array sizes. (One chunk per script that uses arrays.)
	for (scripts.b = FindChunk(MAKE_ID('S','A','R','Y')); scripts.dw != NULL; scripts.b = NextChunk(scripts.b))
	{
		int size = LittleLong(scripts.dw[1]);
		if (size >= 6)
		{
			int script_num = LittleShort(scripts.sw[4]);
			ScriptPtr *ptr = const_cast<ScriptPtr *>(FindScript(script_num));
			if (ptr != NULL)
			{
				ptr->VarCount = ParseLocalArrayChunk(scripts.b, &ptr->LocalArrays, ptr->VarCount);
			}
		}
	}

	// Load script names (if any)
	scripts.b = FindChunk(MAKE_ID('S','N','A','M'));
	if (scripts.dw != NULL)
	{
		UnescapeStringTable(scripts.b + 8, NULL, false);
		for (i = 0; i < NumScripts; ++i)
		{
			// ACC stores script names as an index into the SNAM chunk, with the first index as
			// -1 and counting down from there. We convert this from an index into SNAM into
			// a negative index into the global name table.
			if (Scripts[i].Number < 0)
			{
				const char *str = (const char *)(scripts.b + 8 + scripts.dw[3 + (-Scripts[i].Number - 1)]);
				FName name(str);
				Scripts[i].Number = -name.GetIndex();
			}
		}
		// We need to resort scripts, because the new numbers for named scripts likely
		// do not match the order they were originally in.
		qsort (Scripts, NumScripts, sizeof(ScriptPtr), SortScripts);
	}
}

int FBehavior::SortScripts (const void *a, const void *b)
{
	ScriptPtr *ptr1 = (ScriptPtr *)a;
	ScriptPtr *ptr2 = (ScriptPtr *)b;
	return ptr1->Number - ptr2->Number;
}

//============================================================================
//
// FBehavior :: UnencryptStrings
//
// Descrambles strings in a STRE chunk to transform it into a STRL chunk.
//
//============================================================================

void FBehavior::UnencryptStrings ()
{
	uint32_t *prevchunk = NULL;
	uint32_t *chunk = (uint32_t *)FindChunk(MAKE_ID('S','T','R','E'));
	while (chunk != NULL)
	{
		for (uint32_t strnum = 0; strnum < LittleLong(chunk[3]); ++strnum)
		{
			int ofs = LittleLong(chunk[5+strnum]);
			uint8_t *data = (uint8_t *)chunk + ofs + 8, last;
			int p = (uint8_t)(ofs*157135);
			int i = 0;
			do
			{
				last = (data[i] ^= (uint8_t)(p+(i>>1)));
				++i;
			} while (last != 0);
		}
		prevchunk = chunk;
		chunk = (uint32_t *)NextChunk ((uint8_t *)chunk);
		*prevchunk = MAKE_ID('S','T','R','L');
	}
	if (prevchunk != NULL)
	{
		*prevchunk = MAKE_ID('S','T','R','L');
	}
}

//============================================================================
//
// FBehavior :: UnescapeStringTable
//
// Processes escape sequences for every string in a string table.
// Chunkstart points to the string table. Datastart points to the base address
// for offsets in the string table; if NULL, it will use chunkstart. If
// has_padding is true, then this is a STRL chunk with four bytes of padding
// on either side of the string count.
//
//============================================================================

void FBehavior::UnescapeStringTable(uint8_t *chunkstart, uint8_t *datastart, bool has_padding)
{
	assert(chunkstart != NULL);

	uint32_t *chunk = (uint32_t *)chunkstart;

	if (datastart == NULL)
	{
		datastart = chunkstart;
	}
	if (!has_padding)
	{
		chunk[0] = LittleLong(chunk[0]);
		for (uint32_t strnum = 0; strnum < chunk[0]; ++strnum)
		{
			int ofs = LittleLong(chunk[1 + strnum]);	// Byte swap offset, if needed.
			chunk[1 + strnum] = ofs;
			strbin((char *)datastart + ofs);
		}
	}
	else
	{
		chunk[1] = LittleLong(chunk[1]);
		for (uint32_t strnum = 0; strnum < chunk[1]; ++strnum)
		{
			int ofs = LittleLong(chunk[3 + strnum]);	// Byte swap offset, if needed.
			chunk[3 + strnum] = ofs;
			strbin((char *)datastart + ofs);
		}
	}
}

//============================================================================
//
// FBehavior :: IsGood
//
//============================================================================

bool FBehavior::IsGood ()
{
	bool bad;
	int i;

	// Check that the data format was understood
	if (Format == ACS_Unknown)
	{
		return false;
	}

	// Check that all functions are resolved
	bad = false;
	for (i = 0; i < NumFunctions; ++i)
	{
		ScriptFunction *funcdef = (ScriptFunction *)Functions + i;
		if (funcdef->Address == 0 && funcdef->ImportNum == 0)
		{
			uint32_t *chunk = (uint32_t *)FindChunk (MAKE_ID('F','N','A','M'));
			Printf (TEXTCOLOR_RED "Could not find ACS function %s for use in %s.\n",
				(char *)(chunk + 2) + chunk[3+i], ModuleName);
			bad = true;
		}
	}

	// Check that all imported modules were loaded
	for (i = Imports.Size() - 1; i >= 0; --i)
	{
		if (Imports[i] == NULL)
		{
			Printf (TEXTCOLOR_RED "Not all the libraries used by %s could be found.\n", ModuleName);
			return false;
		}
	}

	return !bad;
}

const ScriptPtr *FBehavior::FindScript (int script) const
{
	const ScriptPtr *ptr = BinarySearch<ScriptPtr, int>
		((ScriptPtr *)Scripts, NumScripts, &ScriptPtr::Number, script);

	// If the preceding script has the same number, return it instead.
	// See the note by the script sorting above for why.
	if (ptr > Scripts)
	{
		if (ptr[-1].Number == script)
		{
			ptr--;
		}
	}
	return ptr;
}

const ScriptPtr *FBehaviorContainer::FindScript (int script, FBehavior *&module)
{
	for (uint32_t i = 0; i < StaticModules.Size(); ++i)
	{
		const ScriptPtr *code = StaticModules[i]->FindScript (script);
		if (code != NULL)
		{
			module = StaticModules[i];
			return code;
		}
	}
	return NULL;
}

ScriptFunction *FBehavior::GetFunction (int funcnum, FBehavior *&module) const
{
	if ((unsigned)funcnum >= (unsigned)NumFunctions)
	{
		return NULL;
	}
	ScriptFunction *funcdef = (ScriptFunction *)Functions + funcnum;
	if (funcdef->ImportNum)
	{
		return Imports[funcdef->ImportNum - 1]->GetFunction (funcdef->Address, module);
	}
	// Should I just un-const this function instead of using a const_cast?
	module = const_cast<FBehavior *>(this);
	return funcdef;
}

int FBehavior::FindFunctionName (const char *funcname) const
{
	return FindStringInChunk ((uint32_t *)FindChunk (MAKE_ID('F','N','A','M')), funcname);
}

int FBehavior::FindMapVarName (const char *varname) const
{
	return FindStringInChunk ((uint32_t *)FindChunk (MAKE_ID('M','E','X','P')), varname);
}

int FBehavior::FindMapArray (const char *arrayname) const
{
	int var = FindMapVarName (arrayname);
	if (var >= 0)
	{
		return MapVarStore[var];
	}
	return -1;
}

int FBehavior::FindStringInChunk (uint32_t *names, const char *varname) const
{
	if (names != NULL)
	{
		uint32_t i;

		for (i = 0; i < LittleLong(names[2]); ++i)
		{
			if (stricmp (varname, (char *)(names + 2) + LittleLong(names[3+i])) == 0)
			{
				return (int)i;
			}
		}
	}
	return -1;
}

int FBehavior::GetArrayVal (int arraynum, int index) const
{
	if ((unsigned)arraynum >= (unsigned)NumTotalArrays)
		return 0;
	const ArrayInfo *array = Arrays[arraynum];
	if ((unsigned)index >= (unsigned)array->ArraySize)
		return 0;
	return array->Elements[index];
}

void FBehavior::SetArrayVal (int arraynum, int index, int value)
{
	if ((unsigned)arraynum >= (unsigned)NumTotalArrays)
		return;
	const ArrayInfo *array = Arrays[arraynum];
	if ((unsigned)index >= (unsigned)array->ArraySize)
		return;
	array->Elements[index] = value;
}

inline bool FBehavior::CopyStringToArray(int arraynum, int index, int maxLength, const char *string)
{
	 // false if the operation was incomplete or unsuccessful

	if ((unsigned)arraynum >= (unsigned)NumTotalArrays || index < 0)
		return false;
	const ArrayInfo *array = Arrays[arraynum];
	
	if ((signed)array->ArraySize - index < maxLength) maxLength = (signed)array->ArraySize - index;

	while (maxLength-- > 0)
	{
		array->Elements[index++] = *string;
		if (!(*string)) return true; // written terminating 0
		string++;
	}
	return !(*string); // return true if only terminating 0 was not written
}

uint8_t *FBehavior::FindChunk (uint32_t id) const
{
	uint8_t *chunk = Chunks;

	while (chunk != NULL && chunk < Data + DataSize)
	{
		if (((uint32_t *)chunk)[0] == id)
		{
			return chunk;
		}
		chunk += LittleLong(((uint32_t *)chunk)[1]) + 8;
	}
	return NULL;
}

uint8_t *FBehavior::NextChunk (uint8_t *chunk) const
{
	uint32_t id = *(uint32_t *)chunk;
	chunk += LittleLong(((uint32_t *)chunk)[1]) + 8;
	while (chunk != NULL && chunk < Data + DataSize)
	{
		if (((uint32_t *)chunk)[0] == id)
		{
			return chunk;
		}
		chunk += LittleLong(((uint32_t *)chunk)[1]) + 8;
	}
	return NULL;
}

const char *FBehaviorContainer::LookupString (uint32_t index, bool forprint)
{
	uint32_t lib = index >> LIBRARYID_SHIFT;

	if (lib == STRPOOL_LIBRARYID)
	{
		return GlobalACSStrings.GetString(index);
	}
	if (lib >= (uint32_t)StaticModules.Size())
	{
		return NULL;
	}
	return StaticModules[lib]->LookupString (index & 0xffff, forprint);
}

const char *FBehavior::LookupString (uint32_t index, bool forprint) const
{
	if (StringTable == 0)
	{
		return NULL;
	}
	if (Format == ACS_Old)
	{
		uint32_t *list = (uint32_t *)(Data + StringTable);

		if (index >= list[0])
			return NULL;	// Out of range for this list;

		const char *s = (const char *)(Data + list[1 + index]);
		// Allow translations for Hexen's original strings.
		// This synthesizes a string label and looks it up.
		// It will only do this for original Hexen maps and PCD_PRINTSTRING operations.
		// For localizing user content better solutions exist so this hack won't be available as an editing feature.
		if (ShouldLocalize && forprint)
		{
			FString token = s;
			token.ToUpper();
			token.ReplaceChars(".,-+!?", ' ');
			token.Substitute(" ", "");
			token.Truncate(5);

			FStringf label("TXT_ACS_%s_%d_%.5s", Level->MapName.GetChars(), index, token.GetChars());
			auto p = GStrings[label];
			if (p) return p;
		}

		return s;
	}
	else
	{
		uint32_t *list = (uint32_t *)(Data + StringTable);

		if (index >= list[1])
			return NULL;	// Out of range for this list
		return (const char *)(Data + StringTable + list[3+index]);
	}
}

void FBehaviorContainer::StartTypedScripts (uint16_t type, AActor *activator, bool always, int arg1, bool runNow)
{
	static const char *const TypeNames[] =
	{
		"Closed",
		"Open",
		"Respawn",
		"Death",
		"Enter",
		"Pickup",
		"BlueReturn",
		"RedReturn",
		"WhiteReturn",
		"Unknown", "Unknown", "Unknown",
		"Lightning",
		"Unloading",
		"Disconnect",
		"Return",
		"Event",
		"Kill",
		"Reopen"
	};
	DPrintf(DMSG_NOTIFY, "Starting all scripts of type %d (%s)\n", type,
		type < countof(TypeNames) ? TypeNames[type] : TypeNames[SCRIPT_Lightning - 1]);
	for (unsigned int i = 0; i < StaticModules.Size(); ++i)
	{
		StaticModules[i]->StartTypedScripts (type, activator, always, arg1, runNow);
	}
}

void FBehavior::StartTypedScripts (uint16_t type, AActor *activator, bool always, int arg1, bool runNow)
{
	const ScriptPtr *ptr;
	int i;

	for (i = 0; i < NumScripts; ++i)
	{
		ptr = &Scripts[i];
		if (ptr->Type == type)
		{
			DLevelScript *runningScript = P_GetScriptGoing (Level, activator, NULL, ptr->Number,
				ptr, this, &arg1, 1, always ? ACS_ALWAYS : 0);
			if (nullptr != runningScript && runNow)
			{
				runningScript->RunScript();
			}
		}
	}
}

// FBehavior :: StaticStopMyScripts
//
// Stops any scripts started by the specified actor. Used by the net code
// when a player disconnects. Should this be used in general whenever an
// actor is destroyed?

void FBehaviorContainer::StopMyScripts (AActor *actor)
{
	DACSThinker *controller = actor->Level->ACSThinker;

	if (controller != NULL)
	{
		controller->StopScriptsFor (actor);
	}
}


//---- The ACS Interpreter ----//

IMPLEMENT_CLASS(DACSThinker, false, true)

IMPLEMENT_POINTERS_START(DACSThinker)
	IMPLEMENT_POINTER(LastScript)
	IMPLEMENT_POINTER(Scripts)
IMPLEMENT_POINTERS_END

DACSThinker::~DACSThinker ()
{
	Scripts = nullptr;
}

//==========================================================================
//
// helper class for the runningscripts serializer
//
//==========================================================================

struct SavingRunningscript
{
	int scriptnum;
	DLevelScript *lscript;
};

FSerializer &Serialize(FSerializer &arc, const char *key, SavingRunningscript &rs, SavingRunningscript *def)
{
	if (arc.BeginObject(key))
	{
		arc.ScriptNum("num", rs.scriptnum)
			("script", rs.lscript)
			.EndObject();
	}
	return arc;
}

void DACSThinker::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("scripts", Scripts)
		("lastscript", LastScript);

	if (arc.isWriting())
	{
		if (RunningScripts.CountUsed())
		{
			ScriptMap::Iterator it(RunningScripts);
			ScriptMap::Pair *pair;

			arc.BeginArray("runningscripts");
			while (it.NextPair(pair))
			{
				assert(pair->Value != nullptr);
				SavingRunningscript srs = { pair->Key, pair->Value };
				arc(nullptr, srs);
			}
			arc.EndArray();
		}
	}
	else // Loading
	{
		DLevelScript *script = nullptr;
		RunningScripts.Clear();
		if (arc.BeginArray("runningscripts"))
		{
			auto cnt = arc.ArraySize();
			for (unsigned i = 0; i < cnt; i++)
			{
				SavingRunningscript srs;
				arc(nullptr, srs);
				RunningScripts[srs.scriptnum] = srs.lscript;
			}
			arc.EndArray();
		}
	}
}

cycle_t ACSTime;

void DACSThinker::Tick ()
{
	ACSTime.Reset();
	ACSTime.Clock();
	DLevelScript *script = Scripts;

	while (script)
	{
		DLevelScript *next = script->next;
		script->RunScript();
		script = next;
	}

//	GlobalACSStrings.Clear();

	ACSTime.Unclock();
}

void DACSThinker::StopScriptsFor (AActor *actor)
{
	DLevelScript *script = Scripts;

	while (script != NULL)
	{
		DLevelScript *next = script->next;
		if (script->activator == actor)
		{
			script->SetState (DLevelScript::SCRIPT_PleaseRemove);
		}
		script = next;
	}
}

IMPLEMENT_CLASS(DLevelScript, false, true)

IMPLEMENT_POINTERS_START(DLevelScript)
	IMPLEMENT_POINTER(next)
	IMPLEMENT_POINTER(prev)
	IMPLEMENT_POINTER(activator)
IMPLEMENT_POINTERS_END

//==========================================================================
//
// SerializeFFontPtr
//
//==========================================================================

void DLevelScript::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);

	uint32_t pcofs;
	uint16_t lib;

	if (arc.isWriting())
	{
		lib = activeBehavior->GetLibraryID() >> LIBRARYID_SHIFT;
		pcofs = activeBehavior->PC2Ofs(pc);
	}

	arc.ScriptNum("scriptnum", script)
		("next", next)
		("prev", prev)
		.Enum("state", state)
		("statedata", statedata)
		("activator", activator)
		("activationline", activationline)
		("backside", backSide)
		("localvars", Localvars)
		("lib", lib)
		("pc", pcofs)
		("activefont", activefont)
		("hudwidth", hudwidth)
		("hudheight", hudheight)
		("cliprectleft", ClipRectLeft)
		("cliprectop", ClipRectTop)
		("cliprectwidth", ClipRectWidth)
		("cliprectheight", ClipRectHeight)
		("wrapwidth", WrapWidth)
		("inmodulescriptnum", InModuleScriptNumber)
		("level", Level);

	if (arc.isReading())
	{
		activeBehavior = Level->Behaviors.GetModule(lib);

		if (nullptr == activeBehavior)
		{
			I_Error("Could not find ACS module");
		}

		pc = activeBehavior->Ofs2PC(pcofs);
	}
}

void DLevelScript::Unlink ()
{
	DACSThinker *controller = Level->ACSThinker;

	if (controller->LastScript == this)
	{
		controller->LastScript = prev;
		GC::WriteBarrier(controller, prev);
	}
	if (controller->Scripts == this)
	{
		controller->Scripts = next;
		GC::WriteBarrier(controller, next);
	}
	if (prev)
	{
		prev->next = next;
		GC::WriteBarrier(prev, next);
	}
	if (next)
	{
		next->prev = prev;
		GC::WriteBarrier(next, prev);
	}
}

void DLevelScript::Link ()
{
	DACSThinker *controller = Level->ACSThinker;

	next = controller->Scripts;
	GC::WriteBarrier(this, next);
	if (controller->Scripts)
	{
		controller->Scripts->prev = this;
		GC::WriteBarrier(controller->Scripts, this);
	}
	prev = NULL;
	controller->Scripts = this;
	GC::WriteBarrier(controller, this);
	if (controller->LastScript == NULL)
	{
		controller->LastScript = this;
	}
}

void DLevelScript::PutLast ()
{
	DACSThinker *controller = Level->ACSThinker;

	if (controller->LastScript == this)
		return;

	Unlink ();
	if (controller->Scripts == NULL)
	{
		Link ();
	}
	else
	{
		if (controller->LastScript)
			controller->LastScript->next = this;
		prev = controller->LastScript;
		next = NULL;
		controller->LastScript = this;
	}
}

void DLevelScript::PutFirst ()
{
	DACSThinker *controller = Level->ACSThinker;

	if (controller->Scripts == this)
		return;

	Unlink ();
	Link ();
}

int DLevelScript::Random (int min, int max)
{
	if (max < min)
	{
		std::swap (max, min);
	}

	return min + pr_acs(max - min + 1);
}

int DLevelScript::ThingCount (int type, int stringid, int tid, int tag)
{
	AActor *actor;
	PClassActor *kind;
	int count = 0;
	bool replacemented = false;

	if (type > 0)
	{
		kind = P_GetSpawnableType(type);
		if (kind == NULL)
			return 0;
	}
	else if (stringid >= 0)
	{
		const char *type_name = Level->Behaviors.LookupString (stringid);
		if (type_name == NULL)
			return 0;

		kind = PClass::FindActor(type_name);
		if (kind == NULL)
			return 0;
	}
	else
	{
		kind = NULL;
	}

do_count:
	if (tid)
	{
		auto iterator = Level->GetActorIterator(tid);
		while ( (actor = iterator.Next ()) )
		{
			if (actor->health > 0 &&
				(kind == NULL || actor->IsA (kind)))
			{
				if (tag == -1 || Level->SectorHasTag(actor->Sector, tag))
				{
					// Don't count items in somebody's inventory
					if (actor->IsMapActor())
					{
						count++;
					}
				}
			}
		}
	}
	else
	{
		auto iterator = Level->GetThinkerIterator<AActor>();
		while ( (actor = iterator.Next ()) )
		{
			if (actor->health > 0 &&
				(kind == NULL || actor->IsA (kind)))
			{
				if (tag == -1 || Level->SectorHasTag(actor->Sector, tag))
				{
					// Don't count items in somebody's inventory
					if (actor->IsMapActor())
					{
						count++;
					}
				}
			}
		}
	}
	if (!replacemented && kind != NULL)
	{
		// Again, with decorate replacements
		replacemented = true;
		PClassActor *newkind = kind->GetReplacement(Level);
		if (newkind != kind)
		{
			kind = newkind;
			goto do_count;
		}
	}
	return count;
}

void DLevelScript::ChangeFlat (int tag, int name, bool floorOrCeiling)
{
	FTextureID flat;
	int secnum = -1;
	const char *flatname = Level->Behaviors.LookupString (name);

	if (flatname == NULL)
		return;

	flat = TexMan.GetTextureID(flatname, ETextureType::Flat, FTextureManager::TEXMAN_Overridable);

	auto it = Level->GetSectorTagIterator(tag);
	while ((secnum = it.Next()) >= 0)
	{
		int pos = floorOrCeiling? sector_t::ceiling : sector_t::floor;
		Level->sectors[secnum].SetTexture(pos, flat);
	}
}

int DLevelScript::CountPlayers ()
{
	int count = 0, i;

	for (i = 0; i < MAXPLAYERS; i++)
		if (Level->PlayerInGame(i))
			count++;

	return count;
}

void DLevelScript::SetLineTexture (int lineid, int side, int position, int name)
{
	FTextureID texture;
	int linenum = -1;
	const char *texname = Level->Behaviors.LookupString (name);

	if (texname == nullptr)
		return;

	side = !!side;

	texture = TexMan.GetTextureID(texname, ETextureType::Wall, FTextureManager::TEXMAN_Overridable);

	auto itr = Level->GetLineIdIterator(lineid);
	while ((linenum = itr.Next()) >= 0)
	{
		side_t *sidedef;

		sidedef = Level->lines[linenum].sidedef[side];
		if (sidedef == nullptr)
			continue;

		switch (position)
		{
		case TEXTURE_TOP:
			sidedef->SetTexture(side_t::top, texture);
			break;
		case TEXTURE_MIDDLE:
			sidedef->SetTexture(side_t::mid, texture);
			break;
		case TEXTURE_BOTTOM:
			sidedef->SetTexture(side_t::bottom, texture);
			break;
		default:
			break;
		}

	}
}

int DLevelScript::DoSpawn (int type, const DVector3 &pos, int tid, DAngle angle, bool force)
{
	PClassActor *info = PClass::FindActor(Level->Behaviors.LookupString (type));
	AActor *actor = NULL;
	int spawncount = 0;

	if (info != NULL)
	{
		info = info->GetReplacement (Level);

		if ((GetDefaultByType (info)->flags3 & MF3_ISMONSTER) &&
			((dmflags & DF_NO_MONSTERS) || (Level->flags2 & LEVEL2_NOMONSTERS)))
		{
			return 0;
		}

		actor = Spawn (Level, info, pos, ALLOW_REPLACE);
		if (actor != NULL)
		{
			ActorFlags2 oldFlags2 = actor->flags2;
			actor->flags2 |= MF2_PASSMOBJ;
			if (force || P_TestMobjLocation (actor))
			{
				actor->Angles.Yaw = angle;
				actor->SetTID(tid);
				if (actor->flags & MF_SPECIAL)
					actor->flags |= MF_DROPPED;  // Don't respawn
				actor->flags2 = oldFlags2;
				spawncount++;
			}
			else
			{
				// If this is a monster, subtract it from the total monster
				// count, because it already added to it during spawning.
				actor->ClearCounters();
				actor->Destroy ();
				actor = NULL;
			}
		}
	}
	return spawncount;
}

int DLevelScript::DoSpawn(int type, int x, int y, int z, int tid, int angle, bool force)
{
	return DoSpawn(type, DVector3(ACSToDouble(x), ACSToDouble(y), ACSToDouble(z)), tid, angle * (360. / 256), force);
}


int DLevelScript::DoSpawnSpot (int type, int spot, int tid, int angle, bool force)
{
	int spawned = 0;

	if (spot != 0)
	{
		auto iterator = Level->GetActorIterator(spot);
		AActor *aspot;

		while ( (aspot = iterator.Next ()) )
		{
			spawned += DoSpawn (type, aspot->Pos(), tid, angle * (360. / 256), force);
		}
	}
	else if (activator != NULL)
	{
		spawned += DoSpawn (type, activator->Pos(), tid, angle * (360. / 256), force);
	}
	return spawned;
}

int DLevelScript::DoSpawnSpotFacing (int type, int spot, int tid, bool force)
{
	int spawned = 0;

	if (spot != 0)
	{
		auto iterator = Level->GetActorIterator(spot);
		AActor *aspot;

		while ( (aspot = iterator.Next ()) )
		{
			spawned += DoSpawn (type, aspot->Pos(), tid, aspot->Angles.Yaw, force);
		}
	}
	else if (activator != NULL)
	{
			spawned += DoSpawn (type, activator->Pos(), tid, activator->Angles.Yaw, force);
	}
	return spawned;
}

void DLevelScript::DoFadeTo (int r, int g, int b, int a, int time)
{
	DoFadeRange (0, 0, 0, -1, clamp(r, 0, 255), clamp(g, 0, 255), clamp(b, 0, 255), clamp(a, 0, 65536), time);
}

void DLevelScript::DoFadeRange (int r1, int g1, int b1, int a1,
								int r2, int g2, int b2, int a2, int time)
{
	player_t *viewer;
	float ftime = (float)time / 65536.f;
	bool fadingFrom = a1 >= 0;
	float fr1 = 0, fg1 = 0, fb1 = 0, fa1 = 0;
	float fr2, fg2, fb2, fa2;
	int i;

	fr2 = (float)r2 / 255.f;
	fg2 = (float)g2 / 255.f;
	fb2 = (float)b2 / 255.f;
	fa2 = (float)a2 / 65536.f;

	if (fadingFrom)
	{
		fr1 = (float)r1 / 255.f;
		fg1 = (float)g1 / 255.f;
		fb1 = (float)b1 / 255.f;
		fa1 = (float)a1 / 65536.f;
	}

	if (activator != NULL)
	{
		viewer = activator->player;
		if (viewer == NULL)
			return;
		i = MAXPLAYERS;
		goto showme;
	}
	else
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (Level->PlayerInGame(i))
			{
				viewer = Level->Players[i];
showme:
				if (ftime <= 0.f)
				{
					viewer->BlendR = fr2;
					viewer->BlendG = fg2;
					viewer->BlendB = fb2;
					viewer->BlendA = fa2;
				}
				else
				{
					if (!fadingFrom)
					{
						if (viewer->BlendA <= 0.f)
						{
							fr1 = fr2;
							fg1 = fg2;
							fb1 = fb2;
							fa1 = 0.f;
						}
						else
						{
							fr1 = viewer->BlendR;
							fg1 = viewer->BlendG;
							fb1 = viewer->BlendB;
							fa1 = viewer->BlendA;
						}
					}
					Level->CreateThinker<DFlashFader> (fr1, fg1, fb1, fa1, fr2, fg2, fb2, fa2, ftime, viewer->mo);
				}
			}
		}
	}
}

void DLevelScript::DoSetFont (int fontnum)
{
	const char *fontname = Level->Behaviors.LookupString (fontnum);
	activefont = V_GetFont (fontname);
}

int DLevelScript::DoSetMaster (AActor *self, AActor *master)
{
    AActor *defs;
    if (self->flags3&MF3_ISMONSTER)
    {
        if (master)
        {
            if (master->flags3&MF3_ISMONSTER)
            {
                self->FriendPlayer = 0;
                self->master = master;
                Level->total_monsters -= self->CountsAsKill();
                self->flags = (self->flags & ~MF_FRIENDLY) | (master->flags & MF_FRIENDLY);
                Level->total_monsters += self->CountsAsKill();
                // Don't attack your new master
                if (self->target == self->master) self->target = nullptr;
                if (self->lastenemy == self->master) self->lastenemy = nullptr;
                if (self->LastHeard == self->master) self->LastHeard = nullptr;
                return 1;
            }
            else if (master->player)
            {
                // [KS] Be friendly to this player
                self->master = nullptr;
                Level->total_monsters -= self->CountsAsKill();
                self->flags|=MF_FRIENDLY;
                self->SetFriendPlayer(master->player);

                AActor * attacker=master->player->attacker;
                if (attacker)
                {
                    if (!(attacker->flags&MF_FRIENDLY) || 
                        (deathmatch && attacker->FriendPlayer!=0 && attacker->FriendPlayer!=self->FriendPlayer))
                    {
                        self->LastHeard = self->target = attacker;
                    }
                }
                // And stop attacking him if necessary.
                if (self->target == master) self->target = nullptr;
                if (self->lastenemy == master) self->lastenemy = nullptr;
                if (self->LastHeard == master) self->LastHeard = nullptr;
                return 1;
            }
        }
        else
        {
            self->master = nullptr;
            self->FriendPlayer = 0;
            // Go back to whatever friendliness we usually have...
            defs = self->GetDefault();
            Level->total_monsters -= self->CountsAsKill();
            self->flags = (self->flags & ~MF_FRIENDLY) | (defs->flags & MF_FRIENDLY);
            Level->total_monsters += self->CountsAsKill();
            // ...And re-side with our friends.
            if (self->target && !self->IsHostile (self->target)) self->target = nullptr;
            if (self->lastenemy && !self->IsHostile (self->lastenemy)) self->lastenemy = nullptr;
            if (self->LastHeard && !self->IsHostile (self->LastHeard)) self->LastHeard = nullptr;
            return 1;
        }
    }
    return 0;
}

int DoGetMasterTID (AActor *self)
{
	if (self->master) return self->master->tid;
	else if (self->FriendPlayer)
	{
		player_t *player = self->Level->Players[(self->FriendPlayer)-1];
		return player->mo->tid;
	}
	else return 0;
}


enum
{
	APROP_Health		= 0,
	APROP_Speed			= 1,
	APROP_Damage		= 2,
	APROP_Alpha			= 3,
	APROP_RenderStyle	= 4,
	APROP_SeeSound		= 5,	// Sounds can only be set, not gotten
	APROP_AttackSound	= 6,
	APROP_PainSound		= 7,
	APROP_DeathSound	= 8,
	APROP_ActiveSound	= 9,
	APROP_Ambush		= 10,
	APROP_Invulnerable	= 11,
	APROP_JumpZ			= 12,	// [GRB]
	APROP_ChaseGoal		= 13,
	APROP_Frightened	= 14,
	APROP_Gravity		= 15,
	APROP_Friendly		= 16,
	APROP_SpawnHealth   = 17,
	APROP_Dropped		= 18,
	APROP_Notarget		= 19,
	APROP_Species		= 20,
	APROP_NameTag		= 21,
	APROP_Score			= 22,
	APROP_Notrigger		= 23,
	APROP_DamageFactor	= 24,
	APROP_MasterTID     = 25,
	APROP_TargetTID		= 26,
	APROP_TracerTID		= 27,
	APROP_WaterLevel	= 28,
	APROP_ScaleX        = 29,
	APROP_ScaleY        = 30,
	APROP_Dormant		= 31,
	APROP_Mass			= 32,
	APROP_Accuracy      = 33,
	APROP_Stamina       = 34,
	APROP_Height		= 35,
	APROP_Radius		= 36,
	APROP_ReactionTime  = 37,
	APROP_MeleeRange	= 38,
	APROP_ViewHeight	= 39,
	APROP_AttackZOffset	= 40,
	APROP_StencilColor	= 41,
	APROP_Friction		= 42,
	APROP_DamageMultiplier=43,
	APROP_MaxStepHeight	= 44,
	APROP_MaxDropOffHeight= 45,
	APROP_DamageType	= 46,
	APROP_SoundClass	= 47,
};

// These are needed for ACS's APROP_RenderStyle
static const int LegacyRenderStyleIndices[] =
{
	0,	// STYLE_None,
	1,  // STYLE_Normal,
	2,  // STYLE_Fuzzy,
	3,	// STYLE_SoulTrans,
	4,	// STYLE_OptFuzzy,
	5,	// STYLE_Stencil,
	64,	// STYLE_Translucent
	65,	// STYLE_Add,
	66,	// STYLE_Shaded,
	67,	// STYLE_TranslucentStencil,
	68,	// STYLE_Shadow,
	69,	// STYLE_Subtract,
	6,	// STYLE_AddStencil
	7,	// STYLE_AddShaded
	-1
};

void DLevelScript::SetActorProperty (int tid, int property, int value)
{
	if (tid == 0)
	{
		DoSetActorProperty (activator, property, value);
	}
	else
	{
		AActor *actor;
		auto iterator = Level->GetActorIterator(tid);

		while ((actor = iterator.Next()) != NULL)
		{
			DoSetActorProperty (actor, property, value);
		}
	}
}

void DLevelScript::DoSetActorProperty (AActor *actor, int property, int value)
{
	if (actor == NULL)
	{
		return;
	}
	switch (property)
	{
	case APROP_Health:
		// Don't alter the health of dead things.
		if (actor->health <= 0 || (actor->player != NULL && actor->player->playerstate == PST_DEAD))
		{
			break;
		}
		actor->health = value;
		if (actor->player != NULL)
		{
			actor->player->health = value;
		}
		// If the health is set to a non-positive value, properly kill the actor.
		if (value <= 0)
		{
			actor->Die(activator, activator);
		}
		break;

	case APROP_Speed:
		actor->Speed = ACSToDouble(value);
		break;

	case APROP_Damage:
		actor->SetDamage(value);
		break;

	case APROP_Alpha:
		actor->Alpha = ACSToDouble(value);
		break;

	case APROP_RenderStyle:
		for(int i=0; LegacyRenderStyleIndices[i] >= 0; i++)
		{
			if (LegacyRenderStyleIndices[i] == value)
			{
				actor->RenderStyle = ERenderStyle(i);
				break;
			}
		}
		break;

	case APROP_Ambush:
		if (value) actor->flags |= MF_AMBUSH; else actor->flags &= ~MF_AMBUSH;
		break;

	case APROP_Dropped:
		if (value) actor->flags |= MF_DROPPED; else actor->flags &= ~MF_DROPPED;
		break;

	case APROP_Invulnerable:
		if (value) actor->flags2 |= MF2_INVULNERABLE; else actor->flags2 &= ~MF2_INVULNERABLE;
		break;

	case APROP_Notarget:
		if (value) actor->flags3 |= MF3_NOTARGET; else actor->flags3 &= ~MF3_NOTARGET;
		break;

	case APROP_Notrigger:
		if (value) actor->flags6 |= MF6_NOTRIGGER; else actor->flags6 &= ~MF6_NOTRIGGER;
		break;

	case APROP_JumpZ:
		if (actor->IsKindOf(NAME_PlayerPawn))
			actor->FloatVar(NAME_JumpZ) = ACSToDouble(value);
		break; 	// [GRB]

	case APROP_ChaseGoal:
		if (value)
			actor->flags5 |= MF5_CHASEGOAL;
		else
			actor->flags5 &= ~MF5_CHASEGOAL;
		break;

	case APROP_Frightened:
		if (value)
			actor->flags4 |= MF4_FRIGHTENED;
		else
			actor->flags4 &= ~MF4_FRIGHTENED;
		break;

	case APROP_Friendly:
		if (actor->CountsAsKill()) Level->total_monsters--;
		if (value)
		{
			actor->flags |= MF_FRIENDLY;
		}
		else
		{
			actor->flags &= ~MF_FRIENDLY;
		}
		if (actor->CountsAsKill()) Level->total_monsters++;
		break;


	case APROP_SpawnHealth:
		if (actor->IsKindOf(NAME_PlayerPawn))
		{
			actor->IntVar(NAME_MaxHealth) = value;
		}
		break;

	case APROP_Gravity:
		actor->Gravity = ACSToDouble(value);
		break;

	case APROP_SeeSound:
		actor->SeeSound = Level->Behaviors.LookupString(value);
		break;

	case APROP_AttackSound:
		actor->AttackSound = Level->Behaviors.LookupString(value);
		break;

	case APROP_PainSound:
		actor->PainSound = Level->Behaviors.LookupString(value);
		break;

	case APROP_DeathSound:
		actor->DeathSound = Level->Behaviors.LookupString(value);
		break;

	case APROP_ActiveSound:
		actor->ActiveSound = Level->Behaviors.LookupString(value);
		break;

	case APROP_Species:
		actor->Species = Level->Behaviors.LookupString(value);
		break;

	case APROP_Score:
		actor->Score = value;
		break;

	case APROP_NameTag:
		actor->SetTag(Level->Behaviors.LookupString(value));
		break;

	case APROP_DamageFactor:
		actor->DamageFactor = ACSToDouble(value);
		break;

	case APROP_DamageMultiplier:
		actor->DamageMultiply = ACSToDouble(value);
		break;

	case APROP_MasterTID:
		AActor *other;
		other = Level->SingleActorFromTID (value, NULL);
		DoSetMaster (actor, other);
		break;

	case APROP_ScaleX:
		actor->Scale.X = ACSToDouble(value);
		break;

	case APROP_ScaleY:
		actor->Scale.Y = ACSToDouble(value);
		break;

	case APROP_Mass:
		actor->Mass = value;
		break;

	case APROP_Accuracy:
		actor->accuracy = value;
		break;

	case APROP_Stamina:
		actor->stamina = value;
		break;

	case APROP_ReactionTime:
		actor->reactiontime = value;
		break;

	case APROP_MeleeRange:
		actor->meleerange = ACSToDouble(value);
		break;

	case APROP_ViewHeight:
		if (actor->IsKindOf(NAME_PlayerPawn))
		{
			actor->FloatVar(NAME_ViewHeight) = ACSToDouble(value);
			if (actor->player != NULL)
			{
				actor->player->viewheight = ACSToDouble(value);
			}
		}
		break;

	case APROP_AttackZOffset:
		if (actor->IsKindOf(NAME_PlayerPawn))
			actor->FloatVar(NAME_AttackZOffset) = ACSToDouble(value);
		break;

	case APROP_StencilColor:
		actor->SetShade(value);
		break;

	case APROP_Friction:
		actor->Friction = ACSToDouble(value);
		break;

	case APROP_MaxStepHeight:
		actor->MaxStepHeight = ACSToDouble(value);
		break;

	case APROP_MaxDropOffHeight:
		actor->MaxDropOffHeight = ACSToDouble(value);
		break;

	case APROP_DamageType:
		actor->DamageType = Level->Behaviors.LookupString(value);
		break;

	case APROP_SoundClass:
		if (actor->IsKindOf(NAME_PlayerPawn))
		{
			if (actor->player != nullptr)
			{
				actor->player->SoundClass = Level->Behaviors.LookupString(value);
			}
		}	
		break;

	default:
		// do nothing.
		break;
	}
}

int DLevelScript::GetActorProperty (int tid, int property)
{
	AActor *actor = Level->SingleActorFromTID (tid, activator);

	if (actor == NULL)
	{
		return 0;
	}
	switch (property)
	{
	case APROP_Health:		return actor->health;
	case APROP_Speed:		return DoubleToACS(actor->Speed);
	case APROP_Damage:		return actor->GetMissileDamage(0,1);
	case APROP_DamageFactor:return DoubleToACS(actor->DamageFactor);
	case APROP_DamageMultiplier: return DoubleToACS(actor->DamageMultiply);
	case APROP_Alpha:		return DoubleToACS(actor->Alpha);
	case APROP_RenderStyle:	for (int style = STYLE_None; style < STYLE_Count; ++style)
							{ // Check for a legacy render style that matches.
								if (LegacyRenderStyles[style] == actor->RenderStyle)
								{
									return LegacyRenderStyleIndices[style];
								}
							}
							// The current render style isn't expressable as a legacy style,
							// so pretends it's normal.
							return STYLE_Normal;
	case APROP_Gravity:		return DoubleToACS(actor->Gravity);
	case APROP_Invulnerable:return !!(actor->flags2 & MF2_INVULNERABLE);
	case APROP_Ambush:		return !!(actor->flags & MF_AMBUSH);
	case APROP_Dropped:		return !!(actor->flags & MF_DROPPED);
	case APROP_ChaseGoal:	return !!(actor->flags5 & MF5_CHASEGOAL);
	case APROP_Frightened:	return !!(actor->flags4 & MF4_FRIGHTENED);
	case APROP_Friendly:	return !!(actor->flags & MF_FRIENDLY);
	case APROP_Notarget:	return !!(actor->flags3 & MF3_NOTARGET);
	case APROP_Notrigger:	return !!(actor->flags6 & MF6_NOTRIGGER);
	case APROP_Dormant:		return !!(actor->flags2 & MF2_DORMANT);
	case APROP_SpawnHealth: return actor->GetMaxHealth();

	case APROP_JumpZ:		if (actor->IsKindOf(NAME_PlayerPawn))
							{
								return DoubleToACS(actor->FloatVar(NAME_JumpZ));
							}
							else
							{
								return 0;
							}
	case APROP_Score:		return actor->Score;
	case APROP_MasterTID:	return DoGetMasterTID (actor);
	case APROP_TargetTID:	return (actor->target != NULL)? actor->target->tid : 0;
	case APROP_TracerTID:	return (actor->tracer != NULL)? actor->tracer->tid : 0;
	case APROP_WaterLevel:	return actor->waterlevel;
	case APROP_ScaleX: 		return DoubleToACS(actor->Scale.X);
	case APROP_ScaleY: 		return DoubleToACS(actor->Scale.Y);
	case APROP_Mass: 		return actor->Mass;
	case APROP_Accuracy:    return actor->accuracy;
	case APROP_Stamina:     return actor->stamina;
	case APROP_Height:		return DoubleToACS(actor->Height);
	case APROP_Radius:		return DoubleToACS(actor->radius);
	case APROP_ReactionTime:return actor->reactiontime;
	case APROP_MeleeRange:	return DoubleToACS(actor->meleerange);
	case APROP_ViewHeight:	if (actor->player)
							{
								return DoubleToACS(actor->player->DefaultViewHeight());
							}
							else
							{
								return 0;
							}
	case APROP_AttackZOffset:
							// Note that this is inconsistent with every other place, where the return for monsters is 8, not 0!
							if (actor->IsKindOf(NAME_PlayerPawn))
							{
								return DoubleToACS(actor->FloatVar(NAME_AttackZOffset));
							}
							else
							{
								return 0;
							}

	case APROP_SeeSound:	return GlobalACSStrings.AddString(S_GetSoundName(actor->SeeSound));
	case APROP_AttackSound:	return GlobalACSStrings.AddString(S_GetSoundName(actor->AttackSound));
	case APROP_PainSound:	return GlobalACSStrings.AddString(S_GetSoundName(actor->PainSound));
	case APROP_DeathSound:	return GlobalACSStrings.AddString(S_GetSoundName(actor->DeathSound));
	case APROP_ActiveSound:	return GlobalACSStrings.AddString(S_GetSoundName(actor->ActiveSound));
	case APROP_Species:		return GlobalACSStrings.AddString(actor->GetSpecies().GetChars());
	case APROP_NameTag:		return GlobalACSStrings.AddString(actor->GetTag());
	case APROP_StencilColor:return actor->fillcolor;
	case APROP_Friction:	return DoubleToACS(actor->Friction);
	case APROP_MaxStepHeight: return DoubleToACS(actor->MaxStepHeight);
	case APROP_MaxDropOffHeight: return DoubleToACS(actor->MaxDropOffHeight);
	case APROP_DamageType:	return GlobalACSStrings.AddString(actor->DamageType.GetChars());
	case APROP_SoundClass:	return GlobalACSStrings.AddString(S_GetSoundClass(actor));

	default:				return 0;
	}
}



int DLevelScript::CheckActorProperty (int tid, int property, int value)
{
	AActor *actor = Level->SingleActorFromTID (tid, activator);
	const char *string = NULL;
	if (actor == NULL)
	{
		return 0;
	}
	switch (property)
	{
		// Default
		default:				return 0;

		// Straightforward integer values:
		case APROP_Health:
		case APROP_Speed:
		case APROP_Damage:
		case APROP_DamageFactor:
		case APROP_Alpha:
		case APROP_RenderStyle:
		case APROP_Gravity:
		case APROP_SpawnHealth:
		case APROP_JumpZ:
		case APROP_Score:
		case APROP_MasterTID:
		case APROP_TargetTID:
		case APROP_TracerTID:
		case APROP_WaterLevel:
		case APROP_ScaleX:
		case APROP_ScaleY:
		case APROP_Mass:
		case APROP_Accuracy:
		case APROP_Stamina:
		case APROP_Height:
		case APROP_Radius:
		case APROP_ReactionTime:
		case APROP_MeleeRange:
		case APROP_ViewHeight:
		case APROP_AttackZOffset:
		case APROP_MaxStepHeight:
		case APROP_MaxDropOffHeight:
		case APROP_StencilColor:
			return (GetActorProperty(tid, property) == value);

		// Boolean values need to compare to a binary version of value
		case APROP_Ambush:
		case APROP_Invulnerable:
		case APROP_Dropped:
		case APROP_ChaseGoal:
		case APROP_Frightened:
		case APROP_Friendly:
		case APROP_Notarget:
		case APROP_Notrigger:
		case APROP_Dormant:
			return (GetActorProperty(tid, property) == (!!value));

		// Strings are covered by GetActorProperty, but they're fairly
		// heavy-duty, so make the check here.
		case APROP_SeeSound:	string = S_GetSoundName(actor->SeeSound); break;
		case APROP_AttackSound:	string = S_GetSoundName(actor->AttackSound); break;
		case APROP_PainSound:	string = S_GetSoundName(actor->PainSound); break;
		case APROP_DeathSound:	string = S_GetSoundName(actor->DeathSound); break;
		case APROP_ActiveSound:	string = S_GetSoundName(actor->ActiveSound); break; 
		case APROP_Species:		string = actor->GetSpecies().GetChars(); break;
		case APROP_NameTag:		string = actor->GetTag(); break;
		case APROP_DamageType:	string = actor->DamageType.GetChars(); break;
		case APROP_SoundClass:  string = S_GetSoundClass(actor); break;
	}
	if (string == NULL) string = "";
	return (!stricmp(string, Level->Behaviors.LookupString(value)));
}

bool DLevelScript::DoCheckActorTexture(int tid, AActor *activator, int string, bool floor)
{
	AActor *actor = Level->SingleActorFromTID(tid, activator);
	if (actor == NULL)
	{
		return 0;
	}
	FTextureID tex = TexMan.CheckForTexture(Level->Behaviors.LookupString(string), ETextureType::Flat,
			FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny|FTextureManager::TEXMAN_DontCreate);

	if (!tex.Exists())
	{ // If the texture we want to check against doesn't exist, then
	  // they're obviously not the same.
		return 0;
	}
	FTextureID secpic;
	sector_t *resultsec;
	F3DFloor *resffloor;

	if (floor)
	{
		NextLowestFloorAt(actor->Sector, actor->X(), actor->Y(), actor->Z(), 0, actor->MaxStepHeight, &resultsec, &resffloor);
		secpic = resffloor ? *resffloor->top.texture : resultsec->planes[sector_t::floor].Texture;
	}
	else
	{
		NextHighestCeilingAt(actor->Sector, actor->X(), actor->Y(), actor->Z(), actor->Top(), 0, &resultsec, &resffloor);
		secpic = resffloor ? *resffloor->bottom.texture : resultsec->planes[sector_t::ceiling].Texture;
	}
	return tex == secpic;
}


int DLevelScript::GetPlayerInput(int playernum, int inputnum)
{
	player_t *p;

	if (playernum < 0)
	{
		if (activator == NULL)
		{
			return 0;
		}
		p = activator->player;
	}
	else if (playernum >= MAXPLAYERS || !Level->PlayerInGame(playernum))
	{
		return 0;
	}
	else
	{
		p = Level->Players[playernum];
	}
	if (p == NULL)
	{
		return 0;
	}

	return P_Thing_CheckInputNum(p, inputnum);
}

enum
{
	ACTOR_NONE				= 0x00000000,
	ACTOR_WORLD				= 0x00000001,
	ACTOR_PLAYER			= 0x00000002,
	ACTOR_BOT				= 0x00000004,
	ACTOR_VOODOODOLL		= 0x00000008,
	ACTOR_MONSTER			= 0x00000010,
	ACTOR_ALIVE				= 0x00000020,
	ACTOR_DEAD				= 0x00000040,
	ACTOR_MISSILE			= 0x00000080,
	ACTOR_GENERIC			= 0x00000100
};

int DLevelScript::DoClassifyActor(int tid)
{
	AActor *actor;
	int classify;

	if (tid == 0)
	{
		actor = activator;
		if (actor == NULL)
		{
			return ACTOR_WORLD;
		}
	}
	else
	{
		auto it = Level->GetActorIterator(tid);
		actor = it.Next();
	}
	if (actor == NULL)
	{
		return ACTOR_NONE;
	}

	classify = 0;
	if (actor->player != NULL)
	{
		classify |= ACTOR_PLAYER;
		if (actor->player->playerstate == PST_DEAD)
		{
			classify |= ACTOR_DEAD;
		}
		else
		{
			classify |= ACTOR_ALIVE;
		}
		if (actor->player->mo != actor)
		{
			classify |= ACTOR_VOODOODOLL;
		}
		if (actor->player->Bot != NULL)
		{
			classify |= ACTOR_BOT;
		}
	}
	else if (actor->flags3 & MF3_ISMONSTER)
	{
		classify |= ACTOR_MONSTER;
		if (actor->health <= 0)
		{
			classify |= ACTOR_DEAD;
		}
		else
		{
			classify |= ACTOR_ALIVE;
		}
	}
	else if (actor->flags & MF_MISSILE)
	{
		classify |= ACTOR_MISSILE;
	}
	else
	{
		classify |= ACTOR_GENERIC;
	}
	return classify;
}

enum
{
	SOUND_See,
	SOUND_Attack,
	SOUND_Pain,
	SOUND_Death,
	SOUND_Active,
	SOUND_Use,
	SOUND_Bounce,
	SOUND_WallBounce,
	SOUND_CrushPain,
	SOUND_Howl,
};

static FSoundID GetActorSound(AActor *actor, int soundtype)
{
	switch (soundtype)
	{
	case SOUND_See:			return actor->SeeSound;
	case SOUND_Attack:		return actor->AttackSound;
	case SOUND_Pain:		return actor->PainSound;
	case SOUND_Death:		return actor->DeathSound;
	case SOUND_Active:		return actor->ActiveSound;
	case SOUND_Use:			return actor->UseSound;
	case SOUND_Bounce:		return actor->BounceSound;
	case SOUND_WallBounce:	return actor->WallBounceSound;
	case SOUND_CrushPain:	return actor->CrushPainSound;
	case SOUND_Howl:		return actor->SoundVar(NAME_HowlSound);
	default:				return 0;
	}
}

enum EACSFunctions
{
	ACSF_GetLineUDMFInt=1,
	ACSF_GetLineUDMFFixed,
	ACSF_GetThingUDMFInt,
	ACSF_GetThingUDMFFixed,
	ACSF_GetSectorUDMFInt,
	ACSF_GetSectorUDMFFixed,
	ACSF_GetSideUDMFInt,
	ACSF_GetSideUDMFFixed,
	ACSF_GetActorVelX,
	ACSF_GetActorVelY,
	ACSF_GetActorVelZ,
	ACSF_SetActivator,
	ACSF_SetActivatorToTarget,
	ACSF_GetActorViewHeight,
	ACSF_GetChar,
	ACSF_GetAirSupply,
	ACSF_SetAirSupply,
	ACSF_SetSkyScrollSpeed,
	ACSF_GetArmorType,
	ACSF_SpawnSpotForced,
	ACSF_SpawnSpotFacingForced,
	ACSF_CheckActorProperty,
    ACSF_SetActorVelocity,
	ACSF_SetUserVariable,
	ACSF_GetUserVariable,
	ACSF_Radius_Quake2,
	ACSF_CheckActorClass,
	ACSF_SetUserArray,
	ACSF_GetUserArray,
	ACSF_SoundSequenceOnActor,
	ACSF_SoundSequenceOnSector,
	ACSF_SoundSequenceOnPolyobj,
	ACSF_GetPolyobjX,
	ACSF_GetPolyobjY,
    ACSF_CheckSight,
	ACSF_SpawnForced,
	ACSF_AnnouncerSound,	// Skulltag
	ACSF_SetPointer,
	ACSF_ACS_NamedExecute,
	ACSF_ACS_NamedSuspend,
	ACSF_ACS_NamedTerminate,
	ACSF_ACS_NamedLockedExecute,
	ACSF_ACS_NamedLockedExecuteDoor,
	ACSF_ACS_NamedExecuteWithResult,
	ACSF_ACS_NamedExecuteAlways,
	ACSF_UniqueTID,
	ACSF_IsTIDUsed,
	ACSF_Sqrt,
	ACSF_FixedSqrt,
	ACSF_VectorLength,
	ACSF_SetHUDClipRect,
	ACSF_SetHUDWrapWidth,
	ACSF_SetCVar,
	ACSF_GetUserCVar,
	ACSF_SetUserCVar,
	ACSF_GetCVarString,
	ACSF_SetCVarString,
	ACSF_GetUserCVarString,
	ACSF_SetUserCVarString,
	ACSF_LineAttack,
	ACSF_PlaySound,
	ACSF_StopSound,
	ACSF_strcmp,
	ACSF_stricmp,
	ACSF_StrLeft,
	ACSF_StrRight,
	ACSF_StrMid,
	ACSF_GetActorClass,
	ACSF_GetWeapon,
	ACSF_SoundVolume,
	ACSF_PlayActorSound,
	ACSF_SpawnDecal,
	ACSF_CheckFont,
	ACSF_DropItem,
	ACSF_CheckFlag,
	ACSF_SetLineActivation,
	ACSF_GetLineActivation,
	ACSF_GetActorPowerupTics,
	ACSF_ChangeActorAngle,
	ACSF_ChangeActorPitch,		// 80
	ACSF_GetArmorInfo,
	ACSF_DropInventory,
	ACSF_PickActor,
	ACSF_IsPointerEqual,
	ACSF_CanRaiseActor,
	ACSF_SetActorTeleFog,		// 86
	ACSF_SwapActorTeleFog,
	ACSF_SetActorRoll,
	ACSF_ChangeActorRoll,
	ACSF_GetActorRoll,
	ACSF_QuakeEx,
	ACSF_Warp,					// 92
	ACSF_GetMaxInventory,
	ACSF_SetSectorDamage,
	ACSF_SetSectorTerrain,
	ACSF_SpawnParticle,
	ACSF_SetMusicVolume,
	ACSF_CheckProximity,
	ACSF_CheckActorState,		// 99
	/* Zandronum's - these must be skipped when we reach 99!
	-100:ResetMap(0),
	-101 : PlayerIsSpectator(1),
	-102 : ConsolePlayerNumber(0),
	-103 : GetTeamProperty(2),
	-104 : GetPlayerLivesLeft(1),
	-105 : SetPlayerLivesLeft(2),
	-106 : KickFromGame(2),
	*/

	ACSF_CheckClass = 200,
	ACSF_DamageActor, // [arookas]
	ACSF_SetActorFlag,
	ACSF_SetTranslation,
	ACSF_GetActorFloorTexture,
	ACSF_GetActorFloorTerrain,
	ACSF_StrArg,
	ACSF_Floor,
	ACSF_Round,
	ACSF_Ceil,
	ACSF_ScriptCall,
	ACSF_StartSlideshow,
	ACSF_GetSectorHealth,
	ACSF_GetLineHealth,
	ACSF_SetSubtitleNumber,

		// Eternity's
	ACSF_GetLineX = 300,
	ACSF_GetLineY,


	// OpenGL stuff
	ACSF_SetSectorGlow = 400,
	ACSF_SetFogDensity,

	// ZDaemon
	ACSF_GetTeamScore = 19620,	// (int team)
	ACSF_SetTeamScore,			// (int team, int value
};

int DLevelScript::SideFromID(int id, int side)
{
	if (side != 0 && side != 1) return -1;
	
	if (id == 0)
	{
		if (activationline == NULL) return -1;
		if (activationline->sidedef[side] == NULL) return -1;
		return activationline->sidedef[side]->UDMFIndex;
	}
	else
	{
		int line = Level->FindFirstLineFromID(id);
		if (line == -1) return -1;
		if (Level->lines[line].sidedef[side] == NULL) return -1;
		return Level->lines[line].sidedef[side]->UDMFIndex;
	}
}

int DLevelScript::LineFromID(int id)
{
	if (id == 0)
	{
		if (activationline == NULL) return -1;
		return activationline->Index();
	}
	else
	{
		return Level->FindFirstLineFromID(id);
	}
}

bool GetVarAddrType(AActor *self, FName varname, int index, void *&addr, PType *&type, bool readonly)
{
	PField *var = dyn_cast<PField>(self->GetClass()->FindSymbol(varname, true));

	if (var == NULL || (!readonly && (var->Flags & VARF_Native)))
	{
		return false;
	}
	type = var->Type;
	uint8_t *baddr = reinterpret_cast<uint8_t *>(self) + var->Offset;
	if (type->isArray())
	{
		PArray *arraytype = static_cast<PArray*>(type);
		// unwrap contained type
		type = arraytype->ElementType;
		// offset by index (if in bounds)
		if ((unsigned)index >= arraytype->ElementCount)
		{ // out of bounds
			return false;
		}
		baddr += arraytype->ElementSize * index;
	}
	else if (index != 0)
	{ // ignore attempts to set indexed values on non-arrays
		return false;
	}
	addr = baddr;
	// We don't want Int subclasses like Name or Color to be accessible here.
	if (!type->isInt() && !type->isFloat() && type != TypeBool && type != TypeString)
	{
		// For reading, we also support Name types.
		if (readonly && (type == TypeName))
		{
			return true;
		}
		return false;
	}
	return true;
}

void DLevelScript::SetUserVariable(AActor *self, FName varname, int index, int value)
{
	void *addr;
	PType *type;

	if (GetVarAddrType(self, varname, index, addr, type, false))
	{
		if (type == TypeString)
		{
			FString str = Level->Behaviors.LookupString(value);
			type->InitializeValue(addr, &str);
		}
		else if (type->isFloat())
		{
			type->SetValue(addr, ACSToDouble(value));
		}
		else
		{
			type->SetValue(addr, value);
		}
	}
}

static int GetUserVariable(AActor *self, FName varname, int index)
{
	void *addr;
	PType *type;

	if (GetVarAddrType(self, varname, index, addr, type, true))
	{
		if (type->isFloat())
		{
			return DoubleToACS(type->GetValueFloat(addr));
		}
		else if (type == TypeName)
		{
			return GlobalACSStrings.AddString(FName(ENamedName(type->GetValueInt(addr))).GetChars());
		}
		else if (type == TypeString)
		{
			return GlobalACSStrings.AddString(*(FString *)addr);
		}
		else
		{
			return type->GetValueInt(addr);
		}
	}
	return 0;
}

// Converts fixed- to floating-point as required.
void DLevelScript::DoSetCVar(FBaseCVar *cvar, int value, bool is_string, bool force)
{
	UCVarValue val;
	ECVarType type;

	// For serverinfo variables, only the arbitrator should set it.
	// The actual change to this cvar will not show up until it's
	// been replicated to all peers.
	if ((cvar->GetFlags() & CVAR_SERVERINFO) && consoleplayer != Net_Arbitrator)
	{
		return;
	}
	if (is_string)
	{
		val.String = Level->Behaviors.LookupString(value);
		type = CVAR_String;
	}
	else if (cvar->GetRealType() == CVAR_Float)
	{
		val.Float = ACSToFloat(value);
		type = CVAR_Float;
	}
	else
	{
		val.Int = value;
		type = CVAR_Int;
	}
	if (force)
	{
		cvar->ForceSet(val, type, true);
	}
	else
	{
		cvar->SetGenericRep(val, type);
	}
}

// Converts floating- to fixed-point as required.
static int DoGetCVar(FBaseCVar *cvar, bool is_string)
{
	UCVarValue val;

	if (cvar == nullptr)
	{
		return 0;
	}
	else if (is_string)
	{
		val = cvar->GetGenericRep(CVAR_String);
		return GlobalACSStrings.AddString(val.String);
	}
	else if (cvar->GetRealType() == CVAR_Float)
	{
		val = cvar->GetGenericRep(CVAR_Float);
		return DoubleToACS(val.Float);
	}
	else
	{
		val = cvar->GetGenericRep(CVAR_Int);
		return val.Int;
	}
}

int DLevelScript::SetUserCVar(int playernum, const char *cvarname, int value, bool is_string)
{
	if ((unsigned)playernum >= MAXPLAYERS || !Level->PlayerInGame(playernum))
	{
		return 0;
	}
	auto player = Level->Players[playernum];
	FBaseCVar **cvar_p = player->userinfo.CheckKey(FName(cvarname, true));
	FBaseCVar *cvar;
	// Only mod-created cvars may be set.
	if (cvar_p == NULL || (cvar = *cvar_p) == NULL || (cvar->GetFlags() & CVAR_IGNORE) || !(cvar->GetFlags() & CVAR_MOD))
	{
		return 0;
	}
	DoSetCVar(cvar, value, is_string);

	// If we are this player, then also reflect this change in the local version of this cvar.
	if (player && player == Level->GetConsolePlayer())
	{
		FBaseCVar *cvar = FindCVar(cvarname, NULL);
		// If we can find it in the userinfo, then we should also be able to find it in the normal cvar list,
		// but check just to be safe.
		if (cvar != NULL)
		{
			DoSetCVar(cvar, value, is_string, true);
		}
	}

	return 1;
}

int DLevelScript::SetCVar(AActor *activator, const char *cvarname, int value, bool is_string)
{
	FBaseCVar *cvar = FindCVar(cvarname, NULL);
	// Only mod-created cvars may be set.
	if (cvar == NULL || (cvar->GetFlags() & (CVAR_IGNORE|CVAR_NOSET)) || !(cvar->GetFlags() & CVAR_MOD))
	{
		return 0;
	}
	// For userinfo cvars, redirect to SetUserCVar
	if (cvar->GetFlags() & CVAR_USERINFO)
	{
		if (activator == NULL || activator->player == NULL)
		{
			return 0;
		}
		auto pnum = Level->PlayerNum(activator->player);
		if (pnum < 0) return 0;
		return SetUserCVar(pnum, cvarname, value, is_string);
	}
	DoSetCVar(cvar, value, is_string);
	return 1;
}

static bool DoSpawnDecal(AActor *actor, const FDecalTemplate *tpl, int flags, DAngle angle, double zofs, double distance)
{
	if (!(flags & SDF_ABSANGLE))
	{
		angle += actor->Angles.Yaw;
	}
	return nullptr != ShootDecal(actor->Level, tpl, actor->Sector, actor->X(), actor->Y(),
		actor->Center() - actor->Floorclip + actor->GetBobOffset() + zofs,
		angle, distance, !!(flags & SDF_PERMANENT));
}

void DLevelScript::SetActorAngle(AActor *activator, int tid, int angle, bool interpolate)
{
	DAngle an = ACSToAngle(angle);
	if (tid == 0)
	{
		if (activator != NULL)
		{
			activator->SetAngle(an, interpolate);
		}
	}
	else
	{
		auto iterator = Level->GetActorIterator(tid);
		AActor *actor;

		while ((actor = iterator.Next()))
		{
			actor->SetAngle(an, interpolate);
		}
	}
}

void DLevelScript::SetActorPitch(AActor *activator, int tid, int angle, bool interpolate)
{
	DAngle an = ACSToAngle(angle).Normalized180();
	if (tid == 0)
	{
		if (activator != NULL)
		{
			activator->SetPitch(an, interpolate);
		}
	}
	else
	{
		auto iterator = Level->GetActorIterator(tid);
		AActor *actor;

		while ((actor = iterator.Next()))
		{
			actor->SetPitch(an, interpolate);
		}
	}
}

void DLevelScript::SetActorRoll(AActor *activator, int tid, int angle, bool interpolate)
{
	DAngle an = ACSToAngle(angle);
	if (tid == 0)
	{
		if (activator != NULL)
		{
			activator->SetRoll(an, interpolate);
		}
	}
	else
	{
		auto iterator = Level->GetActorIterator(tid);
		AActor *actor;

		while ((actor = iterator.Next()))
		{
			actor->SetRoll(an, interpolate);
		}
	}
}

void DLevelScript::SetActorTeleFog(AActor *activator, int tid, FString telefogsrc, FString telefogdest)
{
	// Set the actor's telefog to the specified actor. Handle "" as "don't
	// change" since "None" should work just fine for disabling the fog (given
	// that it will resolve to NAME_None which is not a valid actor name).
	if (tid == 0)
	{
		if (activator != NULL)
		{
			if (telefogsrc.IsNotEmpty())
				activator->TeleFogSourceType = PClass::FindActor(telefogsrc);
			if (telefogdest.IsNotEmpty())
				activator->TeleFogDestType = PClass::FindActor(telefogdest);
		}
	}
	else
	{
		auto iterator = Level->GetActorIterator(tid);
		AActor *actor;

		PClassActor * src = PClass::FindActor(telefogsrc);
		PClassActor * dest = PClass::FindActor(telefogdest);
		while ((actor = iterator.Next()))
		{
			if (telefogsrc.IsNotEmpty())
				actor->TeleFogSourceType = src;
			if (telefogdest.IsNotEmpty())
				actor->TeleFogDestType = dest;
		}
	}
}

int DLevelScript::SwapActorTeleFog(AActor *activator, int tid)
{
	int count = 0;
	if (tid == 0)
	{
		if ((activator == NULL) || (activator->TeleFogSourceType == activator->TeleFogDestType)) 
			return 0; //Does nothing if they're the same.

		std::swap (activator->TeleFogSourceType, activator->TeleFogDestType);
		return 1;
	}
	else
	{
		auto iterator = Level->GetActorIterator(tid);
		AActor *actor;
		
		while ((actor = iterator.Next()))
		{
			if (actor->TeleFogSourceType == actor->TeleFogDestType) 
				continue; //They're the same. Save the effort.

			std::swap (actor->TeleFogSourceType, actor->TeleFogDestType);
			count++;
		}
	}
	return count;
}

 int DLevelScript::ScriptCall(AActor *activator, unsigned argc, int32_t *args)
{
	int retval = 0;
	if (argc >= 2)
	{
		auto clsname = Level->Behaviors.LookupString(args[0]);
		auto funcname = Level->Behaviors.LookupString(args[1]);

		auto cls = PClass::FindClass(clsname);
		if (!cls)
		{
			I_Error("ACS call to unknown class in script function %s.%s", clsname, funcname);
		}
		auto funcsym = dyn_cast<PFunction>(cls->FindSymbol(funcname, true));
		if (funcsym == nullptr)
		{
			I_Error("ACS call to unknown script function %s.%s", clsname, funcname);
		}
		auto func = funcsym->Variants[0].Implementation;
		if (func->ImplicitArgs > 0)
		{
			I_Error("ACS call to non-static script function %s.%s", clsname, funcname);
		}

		// Note that this array may not be reallocated so its initial size must be the maximum possible elements.
		TArray<FString> strings(argc);
		TArray<VMValue> params;
		int p = 1;
		if (func->Proto->ArgumentTypes.Size() > 0 && func->Proto->ArgumentTypes[0] == NewPointer(RUNTIME_CLASS(AActor)))
		{
			params.Push(activator);
			p = 0;
		}
		for (unsigned i = 2; i < argc; i++)
		{
			if (func->Proto->ArgumentTypes.Size() < i - p)
			{
				I_Error("Too many parameters in call to %s.%s", clsname, funcname);
			}
			auto argtype = func->Proto->ArgumentTypes[i - p - 1];
			// The only types allowed are int, bool, double, Name, Sound, Color and String
			if (argtype == TypeSInt32 || argtype == TypeColor)
			{
				params.Push(args[i]);
			}
			else if (argtype == TypeBool)
			{
				params.Push(!!args[i]);
			}
			else if (argtype == TypeFloat64)
			{
				params.Push(ACSToDouble(args[i]));
			}
			else if (argtype == TypeName)
			{
				params.Push(FName(Level->Behaviors.LookupString(args[i])).GetIndex());
			}
			else if (argtype == TypeString)
			{
				strings.Push(Level->Behaviors.LookupString(args[i]));
				params.Push(&strings.Last());
			}
			else if (argtype == TypeSound)
			{
				params.Push(int(FSoundID(Level->Behaviors.LookupString(args[i]))));
			}
			else
			{
				I_Error("Invalid type %s in call to %s.%s", argtype->DescriptiveName(), clsname, funcname);
			}
		}
		if (func->Proto->ArgumentTypes.Size() > params.Size())
		{
			// Check if we got enough parameters. That means we either cover the full argument list of the function or the next argument is optional.
			if (!(funcsym->Variants[0].ArgFlags[params.Size()] & VARF_Optional))
			{
				I_Error("Insufficient parameters in call to %s.%s", clsname, funcname);
			}
		}
		// The return value can be the same types as the parameter types, plus void
		if (func->Proto->ReturnTypes.Size() == 0)
		{
			VMCallWithDefaults(func, params, nullptr, 0);
		}
		else
		{
			auto rettype = func->Proto->ReturnTypes[0];
			if (rettype == TypeSInt32 || rettype == TypeBool || rettype == TypeColor || rettype == TypeName || rettype == TypeSound)
			{
				VMReturn ret(&retval);
				VMCallWithDefaults(func, params, &ret, 1);
				if (rettype == TypeName)
				{
					retval = GlobalACSStrings.AddString(FName(ENamedName(retval)).GetChars());
				}
				else if (rettype == TypeSound)
				{
					retval = GlobalACSStrings.AddString(S_GetSoundName(FSoundID(retval)));
				}
			}
			else if (rettype == TypeFloat64)
			{
				double d;
				VMReturn ret(&d);
				VMCallWithDefaults(func, params, &ret, 1);
				retval = DoubleToACS(d);
			}
			else if (rettype == TypeString)
			{
				FString d;
				VMReturn ret(&d);
				VMCallWithDefaults(func, params, &ret, 1);
				retval = GlobalACSStrings.AddString(d);
			}
			else
			{
				// All other return values can not be handled so ignore them.
				VMCallWithDefaults(func, params, nullptr, 0);
			}
		}
	}
	return retval;
}

int DLevelScript::CallFunction(int argCount, int funcIndex, int32_t *args)
{
	AActor *actor;
	switch(funcIndex)
	{
		case ACSF_GetLineUDMFInt:
			return GetUDMFInt(Level, UDMF_Line, LineFromID(args[0]), Level->Behaviors.LookupString(args[1]));

		case ACSF_GetLineUDMFFixed:
			return DoubleToACS(GetUDMFFloat(Level, UDMF_Line, LineFromID(args[0]), Level->Behaviors.LookupString(args[1])));

		case ACSF_GetThingUDMFInt:
		case ACSF_GetThingUDMFFixed:
			return 0;	// Not implemented yet

		case ACSF_GetSectorUDMFInt:
			return GetUDMFInt(Level, UDMF_Sector, Level->FindFirstSectorFromTag(args[0]), Level->Behaviors.LookupString(args[1]));

		case ACSF_GetSectorUDMFFixed:
			return DoubleToACS(GetUDMFFloat(Level, UDMF_Sector, Level->FindFirstSectorFromTag(args[0]), Level->Behaviors.LookupString(args[1])));

		case ACSF_GetSideUDMFInt:
			return GetUDMFInt(Level, UDMF_Side, SideFromID(args[0], args[1]), Level->Behaviors.LookupString(args[2]));

		case ACSF_GetSideUDMFFixed:
			return DoubleToACS(GetUDMFFloat(Level, UDMF_Side, SideFromID(args[0], args[1]), Level->Behaviors.LookupString(args[2])));

		case ACSF_GetActorVelX:
			actor = Level->SingleActorFromTID(args[0], activator);
			return actor != NULL? DoubleToACS(actor->Vel.X) : 0;

		case ACSF_GetActorVelY:
			actor = Level->SingleActorFromTID(args[0], activator);
			return actor != NULL? DoubleToACS(actor->Vel.Y) : 0;

		case ACSF_GetActorVelZ:
			actor = Level->SingleActorFromTID(args[0], activator);
			return actor != NULL? DoubleToACS(actor->Vel.Z) : 0;

		case ACSF_SetPointer:
			if (activator)
			{
				AActor *ptr = Level->SingleActorFromTID(args[1], activator);
				if (argCount > 2)
				{
					ptr = COPY_AAPTREX(Level, ptr, args[2]);
				}
				if (ptr == activator) ptr = NULL;
				ASSIGN_AAPTR(activator, args[0], ptr, (argCount > 3) ? args[3] : 0);
				return ptr != NULL;
			}
			return 0;

		case ACSF_SetActivator:
			if (argCount > 1 && args[1] != AAPTR_DEFAULT) // condition (x != AAPTR_DEFAULT) is essentially condition (x).
			{
				activator = COPY_AAPTREX(Level, Level->SingleActorFromTID(args[0], activator), args[1]);
			}
			else
			{
				activator = Level->SingleActorFromTID(args[0], NULL);
			}
			return activator != NULL;
		
		case ACSF_SetActivatorToTarget:
			// [KS] I revised this a little bit
			actor = Level->SingleActorFromTID(args[0], activator);
			if (actor != NULL)
			{
				if (actor->player != NULL && actor->player->playerstate == PST_LIVE)
				{
					FTranslatedLineTarget t;
					P_BulletSlope(actor, &t, ALF_PORTALRESTRICT);
					actor = t.linetarget;
				}
				else
				{
					actor = actor->target;
				}
				if (actor != NULL) // [FDARI] moved this (actor != NULL)-branch inside the other, so that it is only tried when it can be true
				{
					activator = actor;
					return 1;
				}
			}
			return 0;

		case ACSF_GetActorViewHeight:
			actor = Level->SingleActorFromTID(args[0], activator);
			if (actor != NULL)
			{
				if (actor->player != NULL)
				{
					return DoubleToACS(actor->player->DefaultViewHeight() + actor->player->crouchviewdelta);
				}
				else
				{
					return DoubleToACS(actor->GetCameraHeight());
				}
			}
			else return 0;

		case ACSF_GetChar:
		{
			const char *p = Level->Behaviors.LookupString(args[0]);
			if (p != NULL && args[1] >= 0 && args[1] < int(strlen(p)))
			{
				return p[args[1]];
			}
			else 
			{
				return 0;
			}
		}

		case ACSF_GetAirSupply:
		{
			if (args[0] < 0 || args[0] >= MAXPLAYERS || !Level->PlayerInGame(args[0]))
			{
				return 0;
			}
			else
			{
				return Level->Players[args[0]]->air_finished - Level->maptime;
			}
		}

		case ACSF_SetAirSupply:
		{
			if (args[0] < 0 || args[0] >= MAXPLAYERS || !Level->PlayerInGame(args[0]))
			{
				return 0;
			}
			else
			{
				Level->Players[args[0]]->air_finished = args[1] + Level->maptime;
				return 1;
			}
		}

		case ACSF_SetSkyScrollSpeed:
		{
			if (args[0] == 1) Level->skyspeed1 = ACSToFloat(args[1]);
			else if (args[0] == 2) Level->skyspeed2 = ACSToFloat(args[1]);
			return 1;
		}

		case ACSF_GetArmorType:
		{
			if (args[1] < 0 || args[1] >= MAXPLAYERS || !Level->PlayerInGame(args[1]))
			{
				return 0;
			}
			else
			{
				FName p(Level->Behaviors.LookupString(args[0]));
				auto armor = Level->Players[args[1]]->mo->FindInventory(NAME_BasicArmor);
				if (armor && armor->NameVar(NAME_ArmorType) == p) return armor->IntVar(NAME_Amount);
			}
			return 0;
		}

		case ACSF_GetArmorInfo:
		{
			if (activator == NULL || activator->player == NULL) return 0;

			auto equippedarmor = activator->FindInventory(NAME_BasicArmor);

			if (equippedarmor && equippedarmor->IntVar(NAME_Amount) != 0)
			{
				switch(args[0])
				{
					case ARMORINFO_CLASSNAME:
						return GlobalACSStrings.AddString(equippedarmor->NameVar(NAME_ArmorType).GetChars());

					case ARMORINFO_SAVEAMOUNT:
						return equippedarmor->IntVar(NAME_MaxAmount);

					case ARMORINFO_SAVEPERCENT:
						return DoubleToACS(equippedarmor->FloatVar(NAME_SavePercent));

					case ARMORINFO_MAXABSORB:
						return equippedarmor->IntVar(NAME_MaxAbsorb);

					case ARMORINFO_MAXFULLABSORB:
						return equippedarmor->IntVar(NAME_MaxFullAbsorb);

					case ARMORINFO_ACTUALSAVEAMOUNT:
						return equippedarmor->IntVar(NAME_ActualSaveAmount);

					default:
						return 0;
				}
			}
			return args[0] == ARMORINFO_CLASSNAME ? GlobalACSStrings.AddString("None") : 0;
		}

		case ACSF_SpawnSpotForced:
			return DoSpawnSpot(args[0], args[1], args[2], args[3], true);

		case ACSF_SpawnSpotFacingForced:
			return DoSpawnSpotFacing(args[0], args[1], args[2], true);

		case ACSF_CheckActorProperty:
			return (CheckActorProperty(args[0], args[1], args[2]));
        
        case ACSF_SetActorVelocity:
		{
			DVector3 vel(ACSToDouble(args[1]), ACSToDouble(args[2]), ACSToDouble(args[3]));
			if (args[0] == 0)
			{
				P_Thing_SetVelocity(activator, vel, !!args[4], !!args[5]);
			}
			else
			{
				auto iterator = Level->GetActorIterator(args[0]);

				while ((actor = iterator.Next()))
				{
					P_Thing_SetVelocity(actor, vel, !!args[4], !!args[5]);
				}
			}
			return 0; 
		}

		case ACSF_SetUserVariable:
		{
			int cnt = 0;
			FName varname(Level->Behaviors.LookupString(args[1]), true);
			if (varname != NAME_None)
			{
				if (args[0] == 0)
				{
					if (activator != NULL)
					{
						SetUserVariable(activator, varname, 0, args[2]);
					}
					cnt++;
				}
				else
				{
					auto iterator = Level->GetActorIterator(args[0]);
	                
					while ( (actor = iterator.Next()) )
					{
						SetUserVariable(actor, varname, 0, args[2]);
						cnt++;
					}
				}
			}
			return cnt;
		}
		
		case ACSF_GetUserVariable:
		{
			FName varname(Level->Behaviors.LookupString(args[1]), true);
			if (varname != NAME_None)
			{
				AActor *a = Level->SingleActorFromTID(args[0], activator); 
				return a != NULL ? GetUserVariable(a, varname, 0) : 0;
			}
			return 0;
		}

		case ACSF_SetUserArray:
		{
			int cnt = 0;
			FName varname(Level->Behaviors.LookupString(args[1]), true);
			if (varname != NAME_None)
			{
				if (args[0] == 0)
				{
					if (activator != NULL)
					{
						SetUserVariable(activator, varname, args[2], args[3]);
					}
					cnt++;
				}
				else
				{
					auto iterator = Level->GetActorIterator(args[0]);
	                
					while ( (actor = iterator.Next()) )
					{
						SetUserVariable(actor, varname, args[2], args[3]);
						cnt++;
					}
				}
			}
			return cnt;
		}
		
		case ACSF_GetUserArray:
		{
			FName varname(Level->Behaviors.LookupString(args[1]), true);
			if (varname != NAME_None)
			{
				AActor *a = Level->SingleActorFromTID(args[0], activator); 
				return a != NULL ? GetUserVariable(a, varname, args[2]) : 0;
			}
			return 0;
		}

		case ACSF_Radius_Quake2:
			P_StartQuake(Level, activator, args[0], args[1], args[2], args[3], args[4], Level->Behaviors.LookupString(args[5]));
			break;

		case ACSF_CheckActorClass:
		{
			AActor *a = Level->SingleActorFromTID(args[0], activator);
			return a == NULL ? false : a->GetClass()->TypeName == FName(Level->Behaviors.LookupString(args[1]));
		}

		case ACSF_GetActorClass:
		{
			AActor *a = Level->SingleActorFromTID(args[0], activator);
			return GlobalACSStrings.AddString(a == NULL ? "None" : a->GetClass()->TypeName.GetChars());
		}

		case ACSF_SoundSequenceOnActor:
			{
				const char *seqname = Level->Behaviors.LookupString(args[1]);
				if (seqname != NULL)
				{
					if (args[0] == 0)
					{
						if (activator != NULL)
						{
							SN_StartSequence(activator, seqname, 0);
						}
					}
					else
					{
						auto it = Level->GetActorIterator(args[0]);
						AActor *actor;

						while ( (actor = it.Next()) )
						{
							SN_StartSequence(actor, seqname, 0);
						}
					}
				}
			}
			break;

		case ACSF_SoundSequenceOnSector:
			{
				const char *seqname = Level->Behaviors.LookupString(args[1]);
				int space = args[2] < CHAN_FLOOR || args[2] > CHAN_INTERIOR ? CHAN_FULLHEIGHT : args[2];
				if (seqname != NULL)
				{
					auto it = Level->GetSectorTagIterator(args[0]);
					int s;
					while ((s = it.Next()) >= 0)
					{
						SN_StartSequence(&Level->sectors[s], args[2], seqname, 0);
					}
				}
			}
			break;

		case ACSF_SoundSequenceOnPolyobj:
			{
				const char *seqname = Level->Behaviors.LookupString(args[1]);
				if (seqname != NULL)
				{
					FPolyObj *poly = Level->GetPolyobj(args[0]);
					if (poly != NULL)
					{
						SN_StartSequence(poly, seqname, 0);
					}
				}
			}
			break;

		case ACSF_GetPolyobjX:
			{
				FPolyObj *poly = Level->GetPolyobj(args[0]);
				if (poly != NULL)
				{
					return DoubleToACS(poly->StartSpot.pos.X);
				}
			}
			return FIXED_MAX;

		case ACSF_GetPolyobjY:
			{
				FPolyObj *poly = Level->GetPolyobj(args[0]);
				if (poly != NULL)
				{
					return DoubleToACS(poly->StartSpot.pos.Y);
				}
			}
			return FIXED_MAX;
        
        case ACSF_CheckSight:
        {
			AActor *source;
			AActor *dest;

			int flags = SF_IGNOREVISIBILITY;

			if (args[2] & 1) flags |= SF_IGNOREWATERBOUNDARY;
			if (args[2] & 2) flags |= SF_SEEPASTBLOCKEVERYTHING | SF_SEEPASTSHOOTABLELINES;

			if (args[0] == 0) 
			{
				source = (AActor *) activator;

				if (args[1] == 0) return 1; // [KS] I'm sure the activator can see itself.

				auto dstiter = Level->GetActorIterator(args[1]);
				while ( (dest = dstiter.Next ()) )
				{
					if (P_CheckSight(source, dest, flags)) return 1;
				}
			}
			else
			{
				auto srciter = Level->GetActorIterator(args[0]);

				while ( (source = srciter.Next ()) )
				{
					if (args[1] != 0)
					{
						auto dstiter = Level->GetActorIterator(args[1]);
						while ( (dest = dstiter.Next ()) )
						{
							if (P_CheckSight(source, dest, flags)) return 1;
						}
					}
					else
					{
						if (P_CheckSight(source, activator, flags)) return 1;
					}
				}
			}
            return 0;
        }

		case ACSF_SpawnForced:
			return DoSpawn(args[0], args[1], args[2], args[3], args[4], args[5], true);

		case ACSF_ACS_NamedExecute:
		case ACSF_ACS_NamedSuspend:
		case ACSF_ACS_NamedTerminate:
		case ACSF_ACS_NamedLockedExecute:
		case ACSF_ACS_NamedLockedExecuteDoor:
		case ACSF_ACS_NamedExecuteWithResult:
		case ACSF_ACS_NamedExecuteAlways:
			{
				int scriptnum = -FName(Level->Behaviors.LookupString(args[0])).GetIndex();
				int arg1 = argCount > 1 ? args[1] : 0;
				int arg2 = argCount > 2 ? args[2] : 0;
				int arg3 = argCount > 3 ? args[3] : 0;
				int arg4 = argCount > 4 ? args[4] : 0;
				return P_ExecuteSpecial(Level, NamedACSToNormalACS[funcIndex - ACSF_ACS_NamedExecute],
					activationline, activator, backSide,
					scriptnum, arg1, arg2, arg3, arg4);
			}
			break;

		case ACSF_UniqueTID:
			return Level->FindUniqueTID(argCount > 0 ? args[0] : 0, (argCount > 1 && args[1] >= 0) ? args[1] : 0);

		case ACSF_IsTIDUsed:
			return Level->IsTIDUsed(args[0]);

		case ACSF_Sqrt:
			return xs_FloorToInt(g_sqrt(double(args[0])));

		case ACSF_FixedSqrt:
			return DoubleToACS(g_sqrt(ACSToDouble(args[0])));

		case ACSF_VectorLength:
			return DoubleToACS(DVector2(ACSToDouble(args[0]), ACSToDouble(args[1])).Length());

		case ACSF_SetHUDClipRect:
			ClipRectLeft = argCount > 0 ? args[0] : 0;
			ClipRectTop = argCount > 1 ? args[1] : 0;
			ClipRectWidth = argCount > 2 ? args[2] : 0;
			ClipRectHeight = argCount > 3 ? args[3] : 0;
			WrapWidth = argCount > 4 ? args[4] : 0;
			HandleAspect = argCount > 5 ? !!args[5] : true;
			break;

		case ACSF_SetHUDWrapWidth:
			WrapWidth = argCount > 0 ? args[0] : 0;
			break;

		case ACSF_GetCVarString:
			if (argCount == 1)
			{
				return DoGetCVar(GetCVar(activator && activator->player ? int(activator->player - players) : -1, Level->Behaviors.LookupString(args[0])), true);
			}
			break;

		case ACSF_SetCVar:
			if (argCount == 2)
			{
				return SetCVar(activator, Level->Behaviors.LookupString(args[0]), args[1], false);
			}
			break;

		case ACSF_SetCVarString:
			if (argCount == 2)
			{
				return SetCVar(activator, Level->Behaviors.LookupString(args[0]), args[1], true);
			}
			break;

		case ACSF_GetUserCVar:
			if (argCount == 2)
			{
				return DoGetCVar(G_GetUserCVar(args[0], Level->Behaviors.LookupString(args[1])), false);
			}
			break;

		case ACSF_GetUserCVarString:
			if (argCount == 2)
			{
				return DoGetCVar(G_GetUserCVar(args[0], Level->Behaviors.LookupString(args[1])), true);
			}
			break;

		case ACSF_SetUserCVar:
			if (argCount == 3)
			{
				return SetUserCVar(args[0], Level->Behaviors.LookupString(args[1]), args[2], false);
			}
			break;

		case ACSF_SetUserCVarString:
			if (argCount == 3)
			{
				return SetUserCVar(args[0], Level->Behaviors.LookupString(args[1]), args[2], true);
			}
			break;

		//[RC] A bullet firing function for ACS. Thanks to DavidPH.
		case ACSF_LineAttack:
			{
				DAngle angle		= ACSToAngle(args[1]);
				DAngle pitch		= ACSToAngle(args[2]);
				int	damage			= args[3];
				FName pufftype		= argCount > 4 && args[4]? FName(Level->Behaviors.LookupString(args[4])) : NAME_BulletPuff;
				FName damagetype	= argCount > 5 && args[5]? FName(Level->Behaviors.LookupString(args[5])) : NAME_None;
				double range		= argCount > 6 && args[6]? ACSToDouble(args[6]) : MISSILERANGE;
				int flags			= argCount > 7 && args[7]? args[7] : 0;
				int pufftid			= argCount > 8 && args[8]? args[8] : 0;

				int fhflags = 0;
				if (flags & FHF_NORANDOMPUFFZ) fhflags |= LAF_NORANDOMPUFFZ;
				if (flags & FHF_NOIMPACTDECAL) fhflags |= LAF_NOIMPACTDECAL;

				if (args[0] == 0)
				{
					AActor *puff = P_LineAttack(activator, angle, range, pitch, damage, damagetype, pufftype, fhflags);
					if (puff != NULL && pufftid != 0)
					{
						puff->SetTID(pufftid);
					}
				}
				else
				{
					AActor *source;
					auto it = Level->GetActorIterator(args[0]);

					while ((source = it.Next()) != NULL)
					{
						AActor *puff = P_LineAttack(source, angle, range, pitch, damage, damagetype, pufftype, fhflags);
						if (puff != NULL && pufftid != 0)
						{
							puff->SetTID(pufftid);
						}
					}
				}
			}
			break;

		case ACSF_PlaySound:
		case ACSF_PlayActorSound:
			// PlaySound(tid, "SoundName", channel, volume, looping, attenuation, local)
			{
				FSoundID sid = 0;

				if (funcIndex == ACSF_PlaySound)
				{
					const char *lookup = Level->Behaviors.LookupString(args[1]);
					if (lookup != NULL)
					{
						sid = lookup;
					}
				}
				if (sid != 0 || funcIndex == ACSF_PlayActorSound)
				{
					auto it = Level->GetActorIterator(args[0]);
					AActor *spot;

					int chan = argCount > 2 ? args[2] : CHAN_BODY;
					float vol = argCount > 3 ? ACSToFloat(args[3]) : 1.f;
					INTBOOL looping = argCount > 4 ? args[4] : false;
					float atten = argCount > 5 ? ACSToFloat(args[5]) : ATTN_NORM;
					INTBOOL local = argCount > 6 ? args[6] : false;

					if (args[0] == 0)
					{
						spot = activator;
						goto doplaysound;
					}
					while ((spot = it.Next()) != NULL)
					{
doplaysound:			if (funcIndex == ACSF_PlayActorSound)
						{
							sid = GetActorSound(spot, args[1]);
						}
						if (sid != 0)
						{
							// What a mess. I think it's a given that this was used with sound flags so it will forever be restricted to the original 8 channels.
							if (local) chan |= CHANF_LOCAL;
							if (looping) chan |= CHANF_LOOP | CHANF_NOSTOP;
							S_PlaySound(spot, chan&7, EChanFlags::FromInt(chan&~7), sid, vol, atten);
						}
					}
				}
			}
			break;

		case ACSF_StopSound:
			{
				int chan = argCount > 1 ? args[1] : CHAN_BODY;

				if (args[0] == 0)
				{
					S_StopSound(activator, chan);
				}
				else
				{
					auto it = Level->GetActorIterator(args[0]);
					AActor *spot;

					while ((spot = it.Next()) != NULL)
					{
						S_StopSound(spot, chan);
					}
				}
			}
			break;

		case ACSF_SoundVolume:
			// SoundVolume(int tid, int channel, fixed volume)
			{
				int chan = args[1];
				double volume = ACSToDouble(args[2]);

				if (args[0] == 0)
				{
					S_ChangeActorSoundVolume(activator, chan, volume);
				}
				else
				{
					auto it = Level->GetActorIterator(args[0]);
					AActor *spot;

					while ((spot = it.Next()) != NULL)
					{
						S_ChangeActorSoundVolume(spot, chan, volume);
					}
				}
			}
			break;

		case ACSF_strcmp:
		case ACSF_stricmp:
			if (argCount >= 2)
			{
				const char *a, *b;
				// If the string indicies are the same, then they are the same string.
				if (args[0] == args[1])
				{
					return 0;
				}
				a = Level->Behaviors.LookupString(args[0]);
				b = Level->Behaviors.LookupString(args[1]);

				// Don't crash on invalid strings.
				if (a == NULL) a = "";
				if (b == NULL) b = "";

				if (argCount > 2)
				{
					int n = args[2];
					return (funcIndex == ACSF_strcmp) ? strncmp(a, b, n) : strnicmp(a, b, n);
				}
				else
				{
					return (funcIndex == ACSF_strcmp) ? strcmp(a, b) : stricmp(a, b);
				}
			}
			break;

		case ACSF_StrLeft:
		case ACSF_StrRight:
			if (argCount >= 2)
			{
				const char *oldstr = Level->Behaviors.LookupString(args[0]);
				if (oldstr == NULL || *oldstr == '\0')
				{
					return GlobalACSStrings.AddString("");
				}
				size_t oldlen = strlen(oldstr);
				size_t newlen = args[1];

				if (oldlen < newlen)
				{
					newlen = oldlen;
				}
				FString newstr(funcIndex == ACSF_StrLeft ? oldstr : oldstr + oldlen - newlen, newlen);
				return GlobalACSStrings.AddString(newstr);
			}
			break;

		case ACSF_StrMid:
			if (argCount >= 3)
			{
				const char *oldstr = Level->Behaviors.LookupString(args[0]);
				if (oldstr == NULL || *oldstr == '\0')
				{
					return GlobalACSStrings.AddString("");
				}
				size_t oldlen = strlen(oldstr);
				size_t pos = args[1];
				size_t newlen = args[2];

				if (pos >= oldlen)
				{
					return GlobalACSStrings.AddString("");
				}
				if (pos + newlen > oldlen || pos + newlen < pos)
				{
					newlen = oldlen - pos;
				}
				return GlobalACSStrings.AddString(FString(oldstr + pos, newlen));
			}
			break;

		case ACSF_GetWeapon:
            if (activator == NULL || activator->player == NULL || // Non-players do not have weapons
                activator->player->ReadyWeapon == NULL)
            {
                return GlobalACSStrings.AddString("None");
            }
            else
            {
				return GlobalACSStrings.AddString(activator->player->ReadyWeapon->GetClass()->TypeName.GetChars());
            }

		case ACSF_SpawnDecal:
			// int SpawnDecal(int tid, str decalname, int flags, fixed angle, int|fixed zoffset, int|fixed distance)
			// Returns number of decals spawned (not including spreading)
			{
				int count = 0;
				const FDecalTemplate *tpl = DecalLibrary.GetDecalByName(Level->Behaviors.LookupString(args[1]));
				if (tpl != NULL)
				{
					int flags = (argCount > 2) ? args[2] : 0;
					DAngle angle = ACSToAngle((argCount > 3) ? args[3] : 0);
					double zoffset = (argCount > 4) ? ((flags & SDF_FIXED_ZOFF) ? ACSToDouble(args[4]) : args[4]) : 0;
					double distance = (argCount > 5) ? ((flags & SDF_FIXED_DISTANCE) ? ACSToDouble(args[5]) : args[5]) : 64;

					if (args[0] == 0)
					{
						if (activator != NULL)
						{
							count += DoSpawnDecal(activator, tpl, flags, angle, zoffset, distance);
						}
					}
					else
					{
						auto it = Level->GetActorIterator(args[0]);
						AActor *actor;

						while ((actor = it.Next()) != NULL)
						{
							count += DoSpawnDecal(actor, tpl, flags, angle, zoffset, distance);
						}
					}
				}
				return count;
			}
			break;

		case ACSF_CheckFont:
			// bool CheckFont(str fontname)
			return V_GetFont(Level->Behaviors.LookupString(args[0])) != NULL;

		case ACSF_DropItem:
		{
			const char *type = Level->Behaviors.LookupString(args[1]);
			int amount = argCount >= 3? args[2] : -1;
			int chance = argCount >= 4? args[3] : 256;
			PClassActor *cls = PClass::FindActor(type);
			int cnt = 0;
			if (cls != NULL)
			{
				if (args[0] == 0)
				{
					if (activator != NULL)
					{
						P_DropItem(activator, cls, amount, chance);
						cnt++;
					}
				}
				else
				{
					auto it = Level->GetActorIterator(args[0]);
					AActor *actor;

					while ((actor = it.Next()) != NULL)
					{
						P_DropItem(actor, cls, amount, chance);
						cnt++;
					}
				}
				return cnt;
			}
			break;
		}

		case ACSF_DropInventory:
		{
			const char *type = Level->Behaviors.LookupString(args[1]);
			AActor *inv;
			
			if (type != NULL)
			{
				if (args[0] == 0)
				{
					if (activator != NULL)
					{
						inv = activator->FindInventory(type);
						if (inv)
						{
							activator->DropInventory(inv);
						}
					}
				}
				else
				{
					auto it = Level->GetActorIterator(args[0]);
					AActor *actor;
					
					while ((actor = it.Next()) != NULL)
					{
						inv = actor->FindInventory(type);
						if (inv)
						{
							actor->DropInventory(inv);
						}
					}
				}
			}
		break;
		}

		case ACSF_CheckFlag:
		{
			AActor *actor = Level->SingleActorFromTID(args[0], activator);
			if (actor != NULL)
			{
				return !!CheckActorFlag(actor, Level->Behaviors.LookupString(args[1]));
			}
			break;
		}

		case ACSF_QuakeEx:
		{
			return P_StartQuakeXYZ(Level, activator, args[0], args[1], args[2], args[3], args[4], args[5], args[6], Level->Behaviors.LookupString(args[7]),
				argCount > 8 ? args[8] : 0,
				argCount > 9 ? ACSToDouble(args[9]) : 1.0,
				argCount > 10 ? ACSToDouble(args[10]) : 1.0,
				argCount > 11 ? ACSToDouble(args[11]) : 1.0,
				argCount > 12 ? args[12] : 0,
				argCount > 13 ? args[13] : 0,
				argCount > 14 ? ACSToDouble(args[14]) : 0,
				argCount > 15 ? ACSToDouble(args[15]) : 0);
		}

		case ACSF_SetLineActivation:
			if (argCount >= 2)
			{
				int line;
				auto itr = Level->GetLineIdIterator(args[0]);
				while ((line = itr.Next()) >= 0)
				{
					Level->lines[line].activation = args[1];
				}
			}
			break;

		case ACSF_GetLineActivation:
			if (argCount > 0)
			{
				int line = Level->FindFirstLineFromID(args[0]);
				return line >= 0 ? Level->lines[line].activation : 0;
			}
			break;

		case ACSF_GetActorPowerupTics:
			if (argCount >= 2)
			{
				PClassActor *powerupclass = PClass::FindActor(Level->Behaviors.LookupString(args[1]));
				if (powerupclass == NULL || !powerupclass->IsDescendantOf(NAME_Powerup))
				{
					Printf("'%s' is not a type of Powerup.\n", Level->Behaviors.LookupString(args[1]));
					return 0;
				}

				AActor *actor = Level->SingleActorFromTID(args[0], activator);
				if (actor != NULL)
				{
					auto powerup = actor->FindInventory(powerupclass);
					if (powerup != NULL)
						return powerup->IntVar(NAME_EffectTics);
				}
				return 0;
			}
			break;

		case ACSF_ChangeActorAngle:
			if (argCount >= 2)
			{
				SetActorAngle(activator, args[0], args[1], argCount > 2 ? !!args[2] : false);
			}
			break;

		case ACSF_ChangeActorPitch:
			if (argCount >= 2)
			{
				SetActorPitch(activator, args[0], args[1], argCount > 2 ? !!args[2] : false);
			}
			break;
		case ACSF_SetActorTeleFog:
			if (argCount >= 3)
			{
				SetActorTeleFog(activator, args[0], Level->Behaviors.LookupString(args[1]), Level->Behaviors.LookupString(args[2]));
			}
			break;
		case ACSF_SwapActorTeleFog:
			if (argCount >= 1)
			{
				return SwapActorTeleFog(activator, args[0]);
			}
			break;
		case ACSF_PickActor:
			if (argCount >= 5)
			{
				actor = Level->SingleActorFromTID(args[0], activator);
				if (actor == NULL)
				{
					return 0;
				}

				ActorFlags actorMask = MF_SHOOTABLE;
				if (argCount >= 6) {
					actorMask = ActorFlags::FromInt(args[5]);
				}

				uint32_t wallMask = ML_BLOCKEVERYTHING | ML_BLOCKHITSCAN;
				if (argCount >= 7) {
					wallMask = args[6];
				}

				int flags = 0;
				if (argCount >= 8)
				{
					flags = args[7];
				}

				AActor* pickedActor = P_LinePickActor(actor, ACSToAngle(args[1]), ACSToDouble(args[3]), ACSToAngle(args[2]), actorMask, wallMask);
				if (pickedActor == NULL) {
					return 0;
				}

				if (!(flags & PICKAF_FORCETID) && (args[4] == 0) && (pickedActor->tid == 0))
					return 0;

				if ((pickedActor->tid == 0) || (flags & PICKAF_FORCETID))
				{
					pickedActor->SetTID(args[4]);
				}
				if (flags & PICKAF_RETURNTID)
				{
					return pickedActor->tid;
				}
				return 1;
			}
			break;

		case ACSF_IsPointerEqual:
			{
				int tid1 = 0, tid2 = 0;
				switch (argCount)
				{
				case 4: tid2 = args[3]; [[fallthrough]];
				case 3: tid1 = args[2];
				}

				actor = Level->SingleActorFromTID(tid1, activator);
				AActor * actor2 = tid2 == tid1 ? actor : Level->SingleActorFromTID(tid2, activator);

				return COPY_AAPTREX(Level, actor, args[0]) == COPY_AAPTREX(Level, actor2, args[1]);
			}
			break;

		case ACSF_CanRaiseActor:
			if (argCount >= 1) {
				if (args[0] == 0) {
					actor = Level->SingleActorFromTID(args[0], activator);
					if (actor != NULL) {
						return P_Thing_CanRaise(actor);
					}
				}

				auto iterator = Level->GetActorIterator(args[0]);
				bool canraiseall = true;
				while ((actor = iterator.Next()))
				{
					canraiseall = P_Thing_CanRaise(actor) & canraiseall;
				}
				
				return canraiseall;
			}
			break;

		// [Nash] Actor roll functions. Let's roll!
		case ACSF_SetActorRoll:
			SetActorRoll(activator, args[0], args[1], false);
			return 0;

		case ACSF_ChangeActorRoll:
			if (argCount >= 2)
			{
				SetActorRoll(activator, args[0], args[1], argCount > 2 ? !!args[2] : false);
			}
			break;

		case ACSF_GetActorRoll:
			actor = Level->SingleActorFromTID(args[0], activator);
			return actor != NULL? AngleToACS(actor->Angles.Roll) : 0;
		
		// [ZK] A_Warp in ACS
		case ACSF_Warp:
		{
			if (nullptr == activator)
			{
				return false;
			}
			
			const int dest = args[0];
			const int flags = args[5];
			
			AActor* const reference = ((flags & WARPF_USEPTR) && (AAPTR_DEFAULT != dest))
				? COPY_AAPTREX(Level, activator, dest)
				: Level->SingleActorFromTID(dest, activator);

			if (nullptr == reference)
			{
				// there is no actor to warp to
				return false;
			}
			
			const double xofs = ACSToDouble(args[1]);
			const double yofs = ACSToDouble(args[2]);
			const double zofs = ACSToDouble(args[3]);
			const DAngle angle = ACSToAngle(args[4]);
			const double heightoffset = argCount > 8 ? ACSToDouble(args[8]) : 0.0;
			const double radiusoffset = argCount > 9 ? ACSToDouble(args[9]) : 0.0;
			const DAngle pitch = ACSToAngle(argCount > 10 ? args[10] : 0);

			if (!P_Thing_Warp(activator, reference, xofs, yofs, zofs, angle, flags, heightoffset, radiusoffset, pitch))
			{
				return false;
			}
			
			if (argCount > 6)
			{
				const char* const statename = Level->Behaviors.LookupString(args[6]);
				
				if (nullptr != statename)
				{
					const bool exact = argCount > 7 && !!args[7];
					FState* const state = activator->GetClass()->FindStateByString(statename, exact);
					
					if (nullptr != state)
					{
						activator->SetState(state);
					}
				}
			}
			
			return true;
		}
		case ACSF_GetMaxInventory:
			actor = Level->SingleActorFromTID(args[0], activator);
			if (actor != NULL)
			{
				return CheckInventory(actor, Level->Behaviors.LookupString(args[1]), true);
			}
			break;

		case ACSF_SetSectorDamage:
			if (argCount >= 2)
			{
				auto it = Level->GetSectorTagIterator(args[0]);
				int s;
				while ((s = it.Next()) >= 0)
				{
					sector_t *sec = &Level->sectors[s];

					sec->damageamount = args[1];
					sec->damagetype = argCount >= 3 ? FName(Level->Behaviors.LookupString(args[2])) : FName(NAME_None);
					sec->damageinterval = argCount >= 4 ? clamp(args[3], 1, INT_MAX) : 32;
					sec->leakydamage = argCount >= 5 ? args[4] : 0;
				}
			}
			break;

		case ACSF_SetSectorTerrain:
			if (argCount >= 3)
			{
				if (args[1] == sector_t::floor || args[1] == sector_t::ceiling)
				{
					int terrain = P_FindTerrain(Level->Behaviors.LookupString(args[2]));
					auto it = Level->GetSectorTagIterator(args[0]);
					int s;
					while ((s = it.Next()) >= 0)
					{
						Level->sectors[s].terrainnum[args[1]] = terrain;
					}
				}
			}
			break;
		
		case ACSF_SpawnParticle:
		{
			PalEntry color = args[0];
			bool fullbright = argCount > 1 ? !!args[1] : false;
			int lifetime = argCount > 2 ? args[2] : TICRATE;
			double size = argCount > 3 ? args[3] : 1.;
			int x = argCount > 4 ? args[4] : 0;
			int y = argCount > 5 ? args[5] : 0;
			int z = argCount > 6 ? args[6] : 0;
			int xvel = argCount > 7 ? args[7] : 0;
			int yvel = argCount > 8 ? args[8] : 0;
			int zvel = argCount > 9 ? args[9] : 0;
			int accelx = argCount > 10 ? args[10] : 0;
			int accely = argCount > 11 ? args[11] : 0;
			int accelz = argCount > 12 ? args[12] : 0;
			int startalpha = argCount > 13 ? args[13] : 0xFF; // Byte trans			
			int fadestep = argCount > 14 ? args[14] : -1;
			double endsize = argCount > 15 ? args[15] : -1.;

			startalpha = clamp<int>(startalpha, 0, 255); // Clamp to byte
			lifetime = clamp<int>(lifetime, 0, 255); // Clamp to byte
			fadestep = clamp<int>(fadestep, -1, 255); // Clamp to byte inc. -1 (indicating automatic)
			size = fabs(size);

			if (lifetime != 0)
				P_SpawnParticle(Level, DVector3(ACSToDouble(x), ACSToDouble(y), ACSToDouble(z)),
								DVector3(ACSToDouble(xvel), ACSToDouble(yvel), ACSToDouble(zvel)),
								DVector3(ACSToDouble(accelx), ACSToDouble(accely), ACSToDouble(accelz)),
								color, startalpha/255., lifetime, size, endsize, fadestep/255., fullbright);
		}
		break;

		case ACSF_SetMusicVolume:
			Level->SetMusicVolume(ACSToFloat(args[0]));
			break;

		case ACSF_CheckProximity:
		{
			// [zombie] ACS version of A_CheckProximity
			actor = Level->SingleActorFromTID(args[0], activator);
			PClass *classname = PClass::FindClass(Level->Behaviors.LookupString(args[1]));
			double distance = ACSToDouble(args[2]);
			int count = argCount >= 4 ? args[3] : 1;
			int flags = argCount >= 5 ? args[4] : 0;
			int ptr = argCount >= 6 ? args[5] : AAPTR_DEFAULT;
			return P_Thing_CheckProximity(Level, actor, classname, distance, count, flags, ptr);
		}

		case ACSF_CheckActorState:
		{
			actor = Level->SingleActorFromTID(args[0], activator);
			const char *statename = Level->Behaviors.LookupString(args[1]);
			bool exact = (argCount > 2) ? !!args[2] : false;
			if (actor && statename)
			{
				return (actor->GetClass()->FindStateByString(statename, exact) != nullptr);
			}
			return false;
		}

		case ACSF_CheckClass:
		{
			const char *clsname = Level->Behaviors.LookupString(args[0]);
			return !!PClass::FindActor(clsname);
		}
		
		case ACSF_DamageActor: // [arookas] wrapper around P_DamageMobj
		{
			// (target, ptr_select1, inflictor, ptr_select2, amount, damagetype)
			AActor* target = COPY_AAPTREX(Level, Level->SingleActorFromTID(args[0], activator), args[1]);
			AActor* inflictor = COPY_AAPTREX(Level, Level->SingleActorFromTID(args[2], activator), args[3]);
			FName damagetype(Level->Behaviors.LookupString(args[5]));
			return P_DamageMobj(target, inflictor, inflictor, args[4], damagetype);
		}

		case ACSF_SetActorFlag:
		{
			int tid = args[0];
			FString flagname = Level->Behaviors.LookupString(args[1]);
			bool flagvalue = !!args[2];
			int count = 0; // Return value; number of actors affected
			if (tid == 0)
			{
				if (ModActorFlag(activator, flagname, flagvalue))
				{
					++count;
				}
			}
			else
			{
				auto it = Level->GetActorIterator(tid);
				while ((actor = it.Next()) != nullptr)
				{
					// Don't log errors when affecting many actors because things might share a TID but not share the flag
					if (ModActorFlag(actor, flagname, flagvalue, false))
					{
						++count;
					}
				}
			}
			return count;
		}

		case ACSF_SetTranslation:
		{
			int tid = args[0];
			const char *trname = Level->Behaviors.LookupString(args[1]);
			if (tid == 0)
			{
				if (activator != nullptr)
					activator->SetTranslation(trname);
			}
			else
			{
				auto it = Level->GetActorIterator(tid);
				while ((actor = it.Next()) != nullptr)
				{
					actor->SetTranslation(trname);
				}
			}
			return 1;
		}

		// OpenGL exclusive functions
		case ACSF_SetSectorGlow:
		{
			int which = !!args[1];
			PalEntry color(args[2], args[3], args[4]);
			float height = float(args[5]);
			if (args[2] == -1) color = -1;

			auto it = Level->GetSectorTagIterator(args[0]);
			int s;
			while ((s = it.Next()) >= 0)
			{
				Level->sectors[s].planes[which].GlowColor = color;
				Level->sectors[s].planes[which].GlowHeight = height;
			}
			break;
		}

		case ACSF_SetFogDensity:
		{
			auto it = Level->GetSectorTagIterator(args[0]);
			int s;
			int d = clamp(args[1]/2, 0, 255);
			while ((s = it.Next()) >= 0)
			{
				Level->sectors[s].SetFogDensity(d);
			}
			break;
		}

		case ACSF_GetActorFloorTexture:
		{
			auto a = Level->SingleActorFromTID(args[0], activator);
			if (a != nullptr)
			{
				return GlobalACSStrings.AddString(TexMan.GetGameTexture(a->floorpic)->GetName());
			}
			else
			{
				return GlobalACSStrings.AddString("");
			}
			break;
		}

		case ACSF_GetActorFloorTerrain:
		{
			auto a = Level->SingleActorFromTID(args[0], activator);
			if (a != nullptr)
			{
				return GlobalACSStrings.AddString(Terrains[a->floorterrain].Name.GetChars());
			}
			else
			{
				return GlobalACSStrings.AddString("");
			}
			break;
		}

		case ACSF_StrArg:
			return -FName(Level->Behaviors.LookupString(args[0])).GetIndex();

		case ACSF_Floor:
			return args[0] & ~0xffff;

		case ACSF_Ceil:
			return (args[0] & ~0xffff) + 0x10000;

		case ACSF_Round:
			return (args[0] + 32768) & ~0xffff;

		case ACSF_ScriptCall:
			return ScriptCall(activator, argCount, args);

		case ACSF_StartSlideshow:
			G_StartSlideshow(Level, FName(Level->Behaviors.LookupString(args[0])));
			break;

		case ACSF_GetSectorHealth:
		{
			int part = args[1];
			auto it = Level->GetSectorTagIterator(args[0]);
			int s = it.Next();
			if (s < 0)
				return 0;
			sector_t* ss = &Level->sectors[s];
			FHealthGroup* grp;
			if (part == SECPART_Ceiling)
			{
				return (ss->healthceilinggroup && (grp = P_GetHealthGroup(Level, ss->healthceilinggroup)))
					? grp->health : ss->healthceiling;
			}
			else if (part == SECPART_Floor)
			{
				return (ss->healthfloorgroup && (grp = P_GetHealthGroup(Level, ss->healthfloorgroup)))
					? grp->health : ss->healthfloor;
			}
			else if (part == SECPART_3D)
			{
				return (ss->health3dgroup && (grp = P_GetHealthGroup(Level, ss->health3dgroup)))
					? grp->health : ss->health3d;
			}
			return 0;
		}

		case ACSF_GetLineHealth:
		{
			auto it = Level->GetLineIdIterator(args[0]);
			int l = it.Next();
			if (l < 0)
				return 0;
			line_t* ll = &Level->lines[l];
			if (ll->healthgroup > 0)
			{
				FHealthGroup* grp = P_GetHealthGroup(Level, ll->healthgroup);
				if (grp) return grp->health;
			}

			return ll->health;
		}

		case ACSF_GetLineX:
		case ACSF_GetLineY:
		{
			auto it = Level->GetLineIdIterator(args[0]);
			int lineno = it.Next();
			if (lineno < 0) return 0;
			DVector2 delta = Level->lines[lineno].Delta();
			double result = Level->lines[lineno].v1->fPos()[funcIndex - ACSF_GetLineX] + delta[funcIndex - ACSF_GetLineX] * ACSToDouble(args[1]);
			if (args[2])
			{
				DVector2 normal = DVector2(delta.Y, -delta.X).Unit();
				result += normal[funcIndex - ACSF_GetLineX] * ACSToDouble(args[2]);
			}
			return DoubleToACS(result);
		}

		case ACSF_SetSubtitleNumber:
			if (argCount >= 2)
			{
				// only players allowed as activator
				if (activator != nullptr && activator->player != nullptr)
				{
					int logNum = args[0];
					FSoundID sid = 0;

					const char* lookup = Level->Behaviors.LookupString(args[1]);
					if (lookup != nullptr)
					{
						sid = lookup;
					}

					activator->player->SetSubtitle(logNum, sid);
				}
			}
			break;

		default:
			break;
	}
	return 0;
}

enum
{
	PRINTNAME_LEVELNAME		= -1,
	PRINTNAME_LEVEL			= -2,
	PRINTNAME_SKILL			= -3,
	PRINTNAME_NEXTLEVEL		= -4,
	PRINTNAME_NEXTSECRET	= -5,
};


#define NEXTWORD	(LittleLong(*pc++))
#define NEXTBYTE	(fmt==ACS_LittleEnhanced?getbyte(pc):NEXTWORD)
#define NEXTSHORT	(fmt==ACS_LittleEnhanced?getshort(pc):NEXTWORD)
#define STACK(a)	(Stack[sp - (a)])
#define PushToStack(a)	(Stack[sp++] = (a))
// Direct instructions that take strings need to have the tag applied.
#define TAGSTR(a)	(a|activeBehavior->GetLibraryID())

inline int getbyte (int *&pc)
{
	int res = *(uint8_t *)pc;
	pc = (int *)((uint8_t *)pc+1);
	return res;
}

inline int getshort (int *&pc)
{
	int res = LittleShort( *(int16_t *)pc);
	pc = (int *)((uint8_t *)pc+2);
	return res;
}

static bool CharArrayParms(int &capacity, int &offset, int &a, FACSStackMemory& Stack, int &sp, bool ranged)
{
	if (ranged)
	{
		capacity = STACK(1);
		offset = STACK(2);
		if (capacity < 1 || offset < 0)
		{
			sp -= 4;
			return false;
		}
		sp -= 2;
	}
	else
	{
		capacity = INT_MAX;
		offset = 0;
	}
	a = STACK(1);
	offset += STACK(2);
	sp -= 2;
	return true;
}

PClass *DLevelScript::GetClassForIndex(int index) const
{
	return PClass::FindActor(Level->Behaviors.LookupString(index));
}

int DLevelScript::RunScript()
{
	DACSThinker *controller = Level->ACSThinker;
	ACSLocalVariables locals(Localvars);
	ACSLocalArrays noarrays;
	ACSLocalArrays *localarrays = &noarrays;
	ScriptFunction *activeFunction = NULL;
	FRemapTable *translation = 0;
	int resultValue = 1;
	int transi = -1;

	if (InModuleScriptNumber >= 0)
	{
		ScriptPtr *ptr = activeBehavior->GetScriptPtr(InModuleScriptNumber);
		assert(ptr != NULL);
		if (ptr != NULL)
		{
			localarrays = &ptr->LocalArrays;
		}
	}

	// Hexen truncates all special arguments to bytes (only when using an old MAPINFO and old ACS format
	const int specialargmask = ((Level->flags2 & LEVEL2_HEXENHACK) && activeBehavior->GetFormat() == ACS_Old) ? 255 : ~0;

	switch (state)
	{
	case SCRIPT_Delayed:
		// Decrement the delay counter and enter state running
		// if it hits 0
		if (--statedata == 0)
			state = SCRIPT_Running;
		break;

	case SCRIPT_TagWait:
		// Wait for tagged sector(s) to go inactive, then enter
		// state running
	{
		int secnum;
		auto it = Level->GetSectorTagIterator(statedata);
		while ((secnum = it.Next()) >= 0)
		{
			if (Level->sectors[secnum].floordata || Level->sectors[secnum].ceilingdata)
				return resultValue;
		}

		// If we got here, none of the tagged sectors were busy
		state = SCRIPT_Running;
	}
	break;

	case SCRIPT_PolyWait:
		// Wait for polyobj(s) to stop moving, then enter state running
		if (!PO_Busy (Level, statedata))
		{
			state = SCRIPT_Running;
		}
		break;

	case SCRIPT_ScriptWaitPre:
		// Wait for a script to start running, then enter state scriptwait
		if (controller->RunningScripts.CheckKey(statedata) != NULL)
			state = SCRIPT_ScriptWait;
		break;

	case SCRIPT_ScriptWait:
		// Wait for a script to stop running, then enter state running
		if (controller->RunningScripts.CheckKey(statedata) != NULL)
			return resultValue;

		state = SCRIPT_Running;
		PutFirst ();
		break;

	default:
		break;
	}

	FACSStack stackobj;
	FACSStackMemory& Stack = stackobj.buffer;
	int &sp = stackobj.sp;

	int *pc = this->pc;
	ACSFormat fmt = activeBehavior->GetFormat();
	FBehavior* const savedActiveBehavior = activeBehavior;
	unsigned int runaway = 0;	// used to prevent infinite loops
	int pcd;
	FString work;
	const char *lookup;
	int optstart = -1;
	int temp;

	while (state == SCRIPT_Running)
	{
		if (++runaway > 2000000)
		{
			Printf ("Runaway %s terminated\n", ScriptPresentation(script).GetChars());
			state = SCRIPT_PleaseRemove;
			break;
		}

		if (fmt == ACS_LittleEnhanced)
		{
			pcd = getbyte(pc);
			if (pcd >= 256-16)
			{
				pcd = (256-16) + ((pcd - (256-16)) << 8) + getbyte(pc);
			}
		}
		else
		{
			pcd = NEXTWORD;
		}

		switch (pcd)
		{
		default:
			Printf ("Unknown P-Code %d in %s\n", pcd, ScriptPresentation(script).GetChars());
			activeBehavior = savedActiveBehavior;
			// fall through
		case PCD_TERMINATE:
			DPrintf (DMSG_NOTIFY, "%s finished\n", ScriptPresentation(script).GetChars());
			state = SCRIPT_PleaseRemove;
			break;

		case PCD_NOP:
			break;

		case PCD_SUSPEND:
			state = SCRIPT_Suspended;
			break;

		case PCD_TAGSTRING:
			//Stack[sp-1] |= activeBehavior->GetLibraryID();
			Stack[sp-1] = GlobalACSStrings.AddString(activeBehavior->LookupString(Stack[sp-1]));
			break;

		case PCD_PUSHNUMBER:
			PushToStack (uallong(pc[0]));
			pc++;
			break;

		case PCD_PUSHBYTE:
			PushToStack (*(uint8_t *)pc);
			pc = (int *)((uint8_t *)pc + 1);
			break;

		case PCD_PUSH2BYTES:
			Stack[sp] = ((uint8_t *)pc)[0];
			Stack[sp+1] = ((uint8_t *)pc)[1];
			sp += 2;
			pc = (int *)((uint8_t *)pc + 2);
			break;

		case PCD_PUSH3BYTES:
			Stack[sp] = ((uint8_t *)pc)[0];
			Stack[sp+1] = ((uint8_t *)pc)[1];
			Stack[sp+2] = ((uint8_t *)pc)[2];
			sp += 3;
			pc = (int *)((uint8_t *)pc + 3);
			break;

		case PCD_PUSH4BYTES:
			Stack[sp] = ((uint8_t *)pc)[0];
			Stack[sp+1] = ((uint8_t *)pc)[1];
			Stack[sp+2] = ((uint8_t *)pc)[2];
			Stack[sp+3] = ((uint8_t *)pc)[3];
			sp += 4;
			pc = (int *)((uint8_t *)pc + 4);
			break;

		case PCD_PUSH5BYTES:
			Stack[sp] = ((uint8_t *)pc)[0];
			Stack[sp+1] = ((uint8_t *)pc)[1];
			Stack[sp+2] = ((uint8_t *)pc)[2];
			Stack[sp+3] = ((uint8_t *)pc)[3];
			Stack[sp+4] = ((uint8_t *)pc)[4];
			sp += 5;
			pc = (int *)((uint8_t *)pc + 5);
			break;

		case PCD_PUSHBYTES:
			temp = *(uint8_t *)pc;
			pc = (int *)((uint8_t *)pc + temp + 1);
			for (temp = -temp; temp; temp++)
			{
				PushToStack (*((uint8_t *)pc + temp));
			}
			break;

		case PCD_DUP:
			Stack[sp] = Stack[sp-1];
			sp++;
			break;

		case PCD_SWAP:
			std::swap(Stack[sp-2], Stack[sp-1]);
			break;

		case PCD_LSPEC1:
			P_ExecuteSpecial(Level, NEXTBYTE, activationline, activator, backSide,
									STACK(1) & specialargmask, 0, 0, 0, 0);
			sp -= 1;
			break;

		case PCD_LSPEC2:
			P_ExecuteSpecial(Level, NEXTBYTE, activationline, activator, backSide,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask, 0, 0, 0);
			sp -= 2;
			break;

		case PCD_LSPEC3:
			P_ExecuteSpecial(Level, NEXTBYTE, activationline, activator, backSide,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask, 0, 0);
			sp -= 3;
			break;

		case PCD_LSPEC4:
			P_ExecuteSpecial(Level, NEXTBYTE, activationline, activator, backSide,
									STACK(4) & specialargmask,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask, 0);
			sp -= 4;
			break;

		case PCD_LSPEC5:
			P_ExecuteSpecial(Level, NEXTBYTE, activationline, activator, backSide,
									STACK(5) & specialargmask,
									STACK(4) & specialargmask,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask);
			sp -= 5;
			break;

		case PCD_LSPEC5RESULT:
			STACK(5) = P_ExecuteSpecial(Level, NEXTBYTE, activationline, activator, backSide,
									STACK(5) & specialargmask,
									STACK(4) & specialargmask,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask);
			sp -= 4;
			break;

		case PCD_LSPEC5EX:
			P_ExecuteSpecial(Level, NEXTWORD, activationline, activator, backSide,
									STACK(5) & specialargmask,
									STACK(4) & specialargmask,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask);
			sp -= 5;
			break;

		case PCD_LSPEC5EXRESULT:
			STACK(5) = P_ExecuteSpecial(Level, NEXTWORD, activationline, activator, backSide,
									STACK(5) & specialargmask,
									STACK(4) & specialargmask,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask);
			sp -= 4;
			break;

		case PCD_LSPEC1DIRECT:
			temp = NEXTBYTE;
			P_ExecuteSpecial(Level, temp, activationline, activator, backSide,
								uallong(pc[0]) & specialargmask ,0, 0, 0, 0);
			pc += 1;
			break;

		case PCD_LSPEC2DIRECT:
			temp = NEXTBYTE;
			P_ExecuteSpecial(Level, temp, activationline, activator, backSide,
								uallong(pc[0]) & specialargmask,
								uallong(pc[1]) & specialargmask, 0, 0, 0);
			pc += 2;
			break;

		case PCD_LSPEC3DIRECT:
			temp = NEXTBYTE;
			P_ExecuteSpecial(Level, temp, activationline, activator, backSide,
								uallong(pc[0]) & specialargmask,
								uallong(pc[1]) & specialargmask,
								uallong(pc[2]) & specialargmask, 0, 0);
			pc += 3;
			break;

		case PCD_LSPEC4DIRECT:
			temp = NEXTBYTE;
			P_ExecuteSpecial(Level, temp, activationline, activator, backSide,
								uallong(pc[0]) & specialargmask,
								uallong(pc[1]) & specialargmask,
								uallong(pc[2]) & specialargmask,
								uallong(pc[3]) & specialargmask, 0);
			pc += 4;
			break;

		case PCD_LSPEC5DIRECT:
			temp = NEXTBYTE;
			P_ExecuteSpecial(Level, temp, activationline, activator, backSide,
								uallong(pc[0]) & specialargmask,
								uallong(pc[1]) & specialargmask,
								uallong(pc[2]) & specialargmask,
								uallong(pc[3]) & specialargmask,
								uallong(pc[4]) & specialargmask);
			pc += 5;
			break;

		// Parameters for PCD_LSPEC?DIRECTB are by definition bytes so never need and-ing.
		case PCD_LSPEC1DIRECTB:
			P_ExecuteSpecial(Level, ((uint8_t *)pc)[0], activationline, activator, backSide,
				((uint8_t *)pc)[1], 0, 0, 0, 0);
			pc = (int *)((uint8_t *)pc + 2);
			break;

		case PCD_LSPEC2DIRECTB:
			P_ExecuteSpecial(Level, ((uint8_t *)pc)[0], activationline, activator, backSide,
				((uint8_t *)pc)[1], ((uint8_t *)pc)[2], 0, 0, 0);
			pc = (int *)((uint8_t *)pc + 3);
			break;

		case PCD_LSPEC3DIRECTB:
			P_ExecuteSpecial(Level, ((uint8_t *)pc)[0], activationline, activator, backSide,
				((uint8_t *)pc)[1], ((uint8_t *)pc)[2], ((uint8_t *)pc)[3], 0, 0);
			pc = (int *)((uint8_t *)pc + 4);
			break;

		case PCD_LSPEC4DIRECTB:
			P_ExecuteSpecial(Level, ((uint8_t *)pc)[0], activationline, activator, backSide,
				((uint8_t *)pc)[1], ((uint8_t *)pc)[2], ((uint8_t *)pc)[3],
				((uint8_t *)pc)[4], 0);
			pc = (int *)((uint8_t *)pc + 5);
			break;

		case PCD_LSPEC5DIRECTB:
			P_ExecuteSpecial(Level, ((uint8_t *)pc)[0], activationline, activator, backSide,
				((uint8_t *)pc)[1], ((uint8_t *)pc)[2], ((uint8_t *)pc)[3],
				((uint8_t *)pc)[4], ((uint8_t *)pc)[5]);
			pc = (int *)((uint8_t *)pc + 6);
			break;

		case PCD_CALLFUNC:
			{
				int argCount = NEXTBYTE;
				int funcIndex = NEXTSHORT;

				int retval = CallFunction(argCount, funcIndex, &STACK(argCount));
				sp -= argCount-1;
				STACK(1) = retval;
			}
			break;

		case PCD_PUSHFUNCTION:
		{
			int funcnum = NEXTBYTE;
			// Not technically a string, but since we use the same tagging mechanism
			PushToStack(TAGSTR(funcnum));
			break;
		}
		case PCD_CALL:
		case PCD_CALLDISCARD:
		case PCD_CALLSTACK:
			{
				int funcnum;
				int i;
				ScriptFunction *func;
				FBehavior *module;

				if(pcd == PCD_CALLSTACK)
				{
					funcnum = STACK(1);
					module = Level->Behaviors.GetModule(funcnum>>LIBRARYID_SHIFT);
					--sp;

					funcnum &= 0xFFFF; // Clear out tag
				}
				else
				{
					module = activeBehavior;
					funcnum = NEXTBYTE;
				}
				func = module->GetFunction (funcnum, module);

				if (func == NULL)
				{
					Printf ("Function %d in %s out of range\n", funcnum, ScriptPresentation(script).GetChars());
					state = SCRIPT_PleaseRemove;
					break;
				}
				if (sp + func->LocalCount + 64 > STACK_SIZE)
				{ // 64 is the margin for the function's working space
					Printf ("Out of stack space in %s\n", ScriptPresentation(script).GetChars());
					state = SCRIPT_PleaseRemove;
					break;
				}
				const ACSLocalVariables mylocals = locals;
				// The function's first argument is also its first local variable.
				locals.Reset(&Stack[sp - func->ArgCount], func->ArgCount + func->LocalCount);
				// Make space on the stack for any other variables the function uses.
				for (i = 0; i < func->LocalCount; ++i)
				{
					Stack[sp+i] = 0;
				}
				sp += i;
				::new(&Stack[sp]) CallReturn(activeBehavior->PC2Ofs(pc), activeFunction,
					activeBehavior, mylocals, localarrays, pcd == PCD_CALLDISCARD, runaway);
				sp += (sizeof(CallReturn) + sizeof(int) - 1) / sizeof(int);
				pc = module->Ofs2PC (func->Address);
				localarrays = &func->LocalArrays;
				activeFunction = func;
				activeBehavior = module;
				fmt = module->GetFormat();
			}
			break;

		case PCD_RETURNVOID:
		case PCD_RETURNVAL:
			{
				int value;
				union
				{
					int32_t *retsp;
					CallReturn *ret;
				};

				if (pcd == PCD_RETURNVAL)
				{
					value = Stack[--sp];
				}
				else
				{
					value = 0;
				}
				sp -= sizeof(CallReturn)/sizeof(int);
				retsp = &Stack[sp];
				activeBehavior->GetFunctionProfileData(activeFunction)->AddRun(runaway - ret->EntryInstrCount);
				sp = int(locals.GetPointer() - &Stack[0]);
				pc = ret->ReturnModule->Ofs2PC(ret->ReturnAddress);
				activeFunction = ret->ReturnFunction;
				activeBehavior = ret->ReturnModule;
				fmt = activeBehavior->GetFormat();
				locals = ret->ReturnLocals;
				localarrays = ret->ReturnArrays;
				if (!ret->bDiscardResult)
				{
					Stack[sp++] = value;
				}
				ret->~CallReturn();
			}
			break;

		case PCD_ADD:
			STACK(2) = STACK(2) + STACK(1);
			sp--;
			break;

		case PCD_SUBTRACT:
			STACK(2) = STACK(2) - STACK(1);
			sp--;
			break;

		case PCD_MULTIPLY:
			STACK(2) = STACK(2) * STACK(1);
			sp--;
			break;

		case PCD_DIVIDE:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				STACK(2) = STACK(2) / STACK(1);
				sp--;
			}
			break;

		case PCD_MODULUS:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				STACK(2) = STACK(2) % STACK(1);
				sp--;
			}
			break;

		case PCD_EQ:
			STACK(2) = (STACK(2) == STACK(1));
			sp--;
			break;

		case PCD_NE:
			STACK(2) = (STACK(2) != STACK(1));
			sp--;
			break;

		case PCD_LT:
			STACK(2) = (STACK(2) < STACK(1));
			sp--;
			break;

		case PCD_GT:
			STACK(2) = (STACK(2) > STACK(1));
			sp--;
			break;

		case PCD_LE:
			STACK(2) = (STACK(2) <= STACK(1));
			sp--;
			break;

		case PCD_GE:
			STACK(2) = (STACK(2) >= STACK(1));
			sp--;
			break;

		case PCD_ASSIGNSCRIPTVAR:
			locals[NEXTBYTE] = STACK(1);
			sp--;
			break;


		case PCD_ASSIGNMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNWORLDVAR:
			ACS_WorldVars[NEXTBYTE] = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNSCRIPTARRAY:
			localarrays->Set(locals, NEXTBYTE, STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_ASSIGNMAPARRAY:
			activeBehavior->SetArrayVal (*(activeBehavior->MapVars[NEXTBYTE]), STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_ASSIGNWORLDARRAY:
			ACS_WorldArrays[NEXTBYTE][STACK(2)] = STACK(1);
			sp -= 2;
			break;

		case PCD_ASSIGNGLOBALARRAY:
			ACS_GlobalArrays[NEXTBYTE][STACK(2)] = STACK(1);
			sp -= 2;
			break;

		case PCD_PUSHSCRIPTVAR:
			PushToStack (locals[NEXTBYTE]);
			break;

		case PCD_PUSHMAPVAR:
			PushToStack (*(activeBehavior->MapVars[NEXTBYTE]));
			break;

		case PCD_PUSHWORLDVAR:
			PushToStack (ACS_WorldVars[NEXTBYTE]);
			break;

		case PCD_PUSHGLOBALVAR:
			PushToStack (ACS_GlobalVars[NEXTBYTE]);
			break;

		case PCD_PUSHSCRIPTARRAY:
			STACK(1) = localarrays->Get(locals, NEXTBYTE, STACK(1));
			break;

		case PCD_PUSHMAPARRAY:
			STACK(1) = activeBehavior->GetArrayVal (*(activeBehavior->MapVars[NEXTBYTE]), STACK(1));
			break;

		case PCD_PUSHWORLDARRAY:
			STACK(1) = ACS_WorldArrays[NEXTBYTE][STACK(1)];
			break;

		case PCD_PUSHGLOBALARRAY:
			STACK(1) = ACS_GlobalArrays[NEXTBYTE][STACK(1)];
			break;

		case PCD_ADDSCRIPTVAR:
			locals[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) += STACK(1);
			sp--;
			break;

		case PCD_ADDWORLDVAR:
			ACS_WorldVars[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) + STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ADDMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) + STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ADDWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] += STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ADDGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] += STACK(1);
				sp -= 2;
			}
			break;

		case PCD_SUBSCRIPTVAR:
			locals[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) -= STACK(1);
			sp--;
			break;

		case PCD_SUBWORLDVAR:
			ACS_WorldVars[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) - STACK(1));
				sp -= 2;
			}
			break;

		case PCD_SUBMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) - STACK(1));
				sp -= 2;
			}
			break;

		case PCD_SUBWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] -= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_SUBGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] -= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MULSCRIPTVAR:
			locals[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) *= STACK(1);
			sp--;
			break;

		case PCD_MULWORLDVAR:
			ACS_WorldVars[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) * STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MULMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) * STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MULWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] *= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MULGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] *= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_DIVSCRIPTVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				locals[NEXTBYTE] /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVMAPVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				*(activeBehavior->MapVars[NEXTBYTE]) /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVWORLDVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				ACS_WorldVars[NEXTBYTE] /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVGLOBALVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				ACS_GlobalVars[NEXTBYTE] /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVSCRIPTARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) / STACK(1));
				sp -= 2;
			}
			break;

		case PCD_DIVMAPARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) / STACK(1));
				sp -= 2;
			}
			break;

		case PCD_DIVWORLDARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] /= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_DIVGLOBALARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] /= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MODSCRIPTVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				locals[NEXTBYTE] %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODMAPVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				*(activeBehavior->MapVars[NEXTBYTE]) %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODWORLDVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				ACS_WorldVars[NEXTBYTE] %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODGLOBALVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				ACS_GlobalVars[NEXTBYTE] %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODSCRIPTARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) % STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MODMAPARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) % STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MODWORLDARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] %= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MODGLOBALARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] %= STACK(1);
				sp -= 2;
			}
			break;

		//[MW] start
		case PCD_ANDSCRIPTVAR:
			locals[NEXTBYTE] &= STACK(1);
			sp--;
			break;

		case PCD_ANDMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) &= STACK(1);
			sp--;
			break;

		case PCD_ANDWORLDVAR:
			ACS_WorldVars[NEXTBYTE] &= STACK(1);
			sp--;
			break;

		case PCD_ANDGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] &= STACK(1);
			sp--;
			break;

		case PCD_ANDSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) & STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ANDMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) & STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ANDWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] &= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ANDGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] &= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_EORSCRIPTVAR:
			locals[NEXTBYTE] ^= STACK(1);
			sp--;
			break;

		case PCD_EORMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) ^= STACK(1);
			sp--;
			break;

		case PCD_EORWORLDVAR:
			ACS_WorldVars[NEXTBYTE] ^= STACK(1);
			sp--;
			break;

		case PCD_EORGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] ^= STACK(1);
			sp--;
			break;

		case PCD_EORSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) ^ STACK(1));
				sp -= 2;
			}
			break;

		case PCD_EORMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) ^ STACK(1));
				sp -= 2;
			}
			break;

		case PCD_EORWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] ^= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_EORGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] ^= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ORSCRIPTVAR:
			locals[NEXTBYTE] |= STACK(1);
			sp--;
			break;

		case PCD_ORMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) |= STACK(1);
			sp--;
			break;

		case PCD_ORWORLDVAR:
			ACS_WorldVars[NEXTBYTE] |= STACK(1);
			sp--;
			break;

		case PCD_ORGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] |= STACK(1);
			sp--;
			break;

		case PCD_ORSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) | STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ORMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) | STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ORWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] |= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ORGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_GlobalArrays[a][STACK(2)] |= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_LSSCRIPTVAR:
			locals[NEXTBYTE] <<= STACK(1);
			sp--;
			break;

		case PCD_LSMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) <<= STACK(1);
			sp--;
			break;

		case PCD_LSWORLDVAR:
			ACS_WorldVars[NEXTBYTE] <<= STACK(1);
			sp--;
			break;

		case PCD_LSGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] <<= STACK(1);
			sp--;
			break;

		case PCD_LSSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) << STACK(1));
				sp -= 2;
			}
			break;

		case PCD_LSMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) << STACK(1));
				sp -= 2;
			}
			break;

		case PCD_LSWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] <<= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_LSGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] <<= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_RSSCRIPTVAR:
			locals[NEXTBYTE] >>= STACK(1);
			sp--;
			break;

		case PCD_RSMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) >>= STACK(1);
			sp--;
			break;

		case PCD_RSWORLDVAR:
			ACS_WorldVars[NEXTBYTE] >>= STACK(1);
			sp--;
			break;

		case PCD_RSGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] >>= STACK(1);
			sp--;
			break;

		case PCD_RSSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) >> STACK(1));
				sp -= 2;
			}
			break;

		case PCD_RSMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) >> STACK(1));
				sp -= 2;
			}
			break;

		case PCD_RSWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] >>= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_RSGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] >>= STACK(1);
				sp -= 2;
			}
			break;
		//[MW] end

		case PCD_INCSCRIPTVAR:
			++locals[NEXTBYTE];
			break;

		case PCD_INCMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) += 1;
			break;

		case PCD_INCWORLDVAR:
			++ACS_WorldVars[NEXTBYTE];
			break;

		case PCD_INCGLOBALVAR:
			++ACS_GlobalVars[NEXTBYTE];
			break;

		case PCD_INCSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(1);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) + 1);
				sp--;
			}
			break;

		case PCD_INCMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(1);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) + 1);
				sp--;
			}
			break;

		case PCD_INCWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(1)] += 1;
				sp--;
			}
			break;

		case PCD_INCGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(1)] += 1;
				sp--;
			}
			break;

		case PCD_DECSCRIPTVAR:
			--locals[NEXTBYTE];
			break;

		case PCD_DECMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) -= 1;
			break;

		case PCD_DECWORLDVAR:
			--ACS_WorldVars[NEXTBYTE];
			break;

		case PCD_DECGLOBALVAR:
			--ACS_GlobalVars[NEXTBYTE];
			break;

		case PCD_DECSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(1);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) - 1);
				sp--;
			}
			break;

		case PCD_DECMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(1);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) - 1);
				sp--;
			}
			break;

		case PCD_DECWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(1)] -= 1;
				sp--;
			}
			break;

		case PCD_DECGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(1);
				ACS_GlobalArrays[a][STACK(1)] -= 1;
				sp--;
			}
			break;

		case PCD_GOTO:
			pc = activeBehavior->Ofs2PC (LittleLong(*pc));
			break;

		case PCD_GOTOSTACK:
			pc = activeBehavior->Jump2PC (STACK(1));
			sp--;
			break;

		case PCD_IFGOTO:
			if (STACK(1))
				pc = activeBehavior->Ofs2PC (LittleLong(*pc));
			else
				pc++;
			sp--;
			break;

		case PCD_SETRESULTVALUE:
			resultValue = STACK(1);
			[[fallthrough]];
		case PCD_DROP: //fall through.
			sp--;
			break;

		case PCD_DELAY:
			statedata = STACK(1) + (fmt == ACS_Old && gameinfo.gametype == GAME_Hexen);
			if (statedata > 0)
			{
				state = SCRIPT_Delayed;
			}
			sp--;
			break;

		case PCD_DELAYDIRECT:
			statedata = uallong(pc[0]) + (fmt == ACS_Old && gameinfo.gametype == GAME_Hexen);
			pc++;
			if (statedata > 0)
			{
				state = SCRIPT_Delayed;
			}
			break;

		case PCD_DELAYDIRECTB:
			statedata = *(uint8_t *)pc + (fmt == ACS_Old && gameinfo.gametype == GAME_Hexen);
			if (statedata > 0)
			{
				state = SCRIPT_Delayed;
			}
			pc = (int *)((uint8_t *)pc + 1);
			break;

		case PCD_RANDOM:
			STACK(2) = Random (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_RANDOMDIRECT:
			PushToStack (Random (uallong(pc[0]), uallong(pc[1])));
			pc += 2;
			break;

		case PCD_RANDOMDIRECTB:
			PushToStack (Random (((uint8_t *)pc)[0], ((uint8_t *)pc)[1]));
			pc = (int *)((uint8_t *)pc + 2);
			break;

		case PCD_THINGCOUNT:
			STACK(2) = ThingCount (STACK(2), -1, STACK(1), -1);
			sp--;
			break;

		case PCD_THINGCOUNTDIRECT:
			PushToStack (ThingCount (uallong(pc[0]), -1, uallong(pc[1]), -1));
			pc += 2;
			break;

		case PCD_THINGCOUNTNAME:
			STACK(2) = ThingCount (-1, STACK(2), STACK(1), -1);
			sp--;
			break;

		case PCD_THINGCOUNTNAMESECTOR:
			STACK(3) = ThingCount (-1, STACK(3), STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_THINGCOUNTSECTOR:
			STACK(3) = ThingCount (STACK(3), -1, STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_TAGWAIT:
			state = SCRIPT_TagWait;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_TAGWAITDIRECT:
			state = SCRIPT_TagWait;
			statedata = uallong(pc[0]);
			pc++;
			break;

		case PCD_POLYWAIT:
			state = SCRIPT_PolyWait;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_POLYWAITDIRECT:
			state = SCRIPT_PolyWait;
			statedata = uallong(pc[0]);
			pc++;
			break;

		case PCD_CHANGEFLOOR:
			ChangeFlat (STACK(2), STACK(1), 0);
			sp -= 2;
			break;

		case PCD_CHANGEFLOORDIRECT:
			ChangeFlat (uallong(pc[0]), TAGSTR(uallong(pc[1])), 0);
			pc += 2;
			break;

		case PCD_CHANGECEILING:
			ChangeFlat (STACK(2), STACK(1), 1);
			sp -= 2;
			break;

		case PCD_CHANGECEILINGDIRECT:
			ChangeFlat (uallong(pc[0]), TAGSTR(uallong(pc[1])), 1);
			pc += 2;
			break;

		case PCD_RESTART:
			{
				const ScriptPtr *scriptp;

				scriptp = activeBehavior->FindScript (script);
				pc = activeBehavior->GetScriptAddress (scriptp);
			}
			break;

		case PCD_ANDLOGICAL:
			STACK(2) = (STACK(2) && STACK(1));
			sp--;
			break;

		case PCD_ORLOGICAL:
			STACK(2) = (STACK(2) || STACK(1));
			sp--;
			break;

		case PCD_ANDBITWISE:
			STACK(2) = (STACK(2) & STACK(1));
			sp--;
			break;

		case PCD_ORBITWISE:
			STACK(2) = (STACK(2) | STACK(1));
			sp--;
			break;

		case PCD_EORBITWISE:
			STACK(2) = (STACK(2) ^ STACK(1));
			sp--;
			break;

		case PCD_NEGATELOGICAL:
			STACK(1) = !STACK(1);
			break;




		case PCD_NEGATEBINARY:
			STACK(1) = ~STACK(1);
			break;

		case PCD_LSHIFT:
			STACK(2) = (STACK(2) << STACK(1));
			sp--;
			break;

		case PCD_RSHIFT:
			STACK(2) = (STACK(2) >> STACK(1));
			sp--;
			break;

		case PCD_UNARYMINUS:
			STACK(1) = -STACK(1);
			break;

		case PCD_IFNOTGOTO:
			if (!STACK(1))
				pc = activeBehavior->Ofs2PC (LittleLong(*pc));
			else
				pc++;
			sp--;
			break;

		case PCD_LINESIDE:
			PushToStack (backSide);
			break;

		case PCD_SCRIPTWAIT:
			statedata = STACK(1);
			sp--;
scriptwait:
			if (controller->RunningScripts.CheckKey(statedata) != NULL)
				state = SCRIPT_ScriptWait;
			else
				state = SCRIPT_ScriptWaitPre;
			PutLast ();
			break;

		case PCD_SCRIPTWAITDIRECT:
			if (!(Level->i_compatflags2 & COMPATF2_SCRIPTWAIT))
			{
				statedata = uallong(pc[0]);
				pc++;
				goto scriptwait;
			}
			else
			{
				// Old implementation for compatibility with Daedalus MAP19
				state = SCRIPT_ScriptWait;
				statedata = uallong(pc[0]);
				pc++;
				PutLast();
				break;
			}

		case PCD_SCRIPTWAITNAMED:
			statedata = -FName(Level->Behaviors.LookupString(STACK(1))).GetIndex();
			sp--;
			goto scriptwait;

		case PCD_CLEARLINESPECIAL:
			if (activationline != NULL)
			{
				activationline->special = 0;
				DPrintf(DMSG_SPAMMY, "Cleared line special on line %d\n", activationline->Index());
			}
			break;

		case PCD_CASEGOTO:
			if (STACK(1) == uallong(pc[0]))
			{
				pc = activeBehavior->Ofs2PC (uallong(pc[1]));
				sp--;
			}
			else
			{
				pc += 2;
			}
			break;

		case PCD_CASEGOTOSORTED:
			// The count and jump table are 4-byte aligned
			pc = (int *)(((size_t)pc + 3) & ~3);
			{
				int numcases = uallong(pc[0]); pc++;
				int min = 0, max = numcases-1;
				while (min <= max)
				{
					int mid = (min + max) / 2;
					int32_t caseval = LittleLong(pc[mid*2]);
					if (caseval == STACK(1))
					{
						pc = activeBehavior->Ofs2PC (LittleLong(pc[mid*2+1]));
						sp--;
						break;
					}
					else if (caseval < STACK(1))
					{
						min = mid + 1;
					}
					else
					{
						max = mid - 1;
					}
				}
				if (min > max)
				{
					// The case was not found, so go to the next instruction.
					pc += numcases * 2;
				}
			}
			break;

		case PCD_BEGINPRINT:
			STRINGBUILDER_START(work);
			break;

		case PCD_PRINTSTRING:
		case PCD_PRINTLOCALIZED:
			lookup = Level->Behaviors.LookupString (STACK(1), true);
			if (pcd == PCD_PRINTLOCALIZED)
			{
				lookup = GStrings(lookup);
			}
			if (lookup != NULL)
			{
				work += lookup;
			}
			--sp;
			break;

		case PCD_PRINTNUMBER:
			work.AppendFormat ("%d", STACK(1));
			--sp;
			break;

		case PCD_PRINTBINARY:
			IGNORE_FORMAT_PRE
			work.AppendFormat ("%B", STACK(1));
			IGNORE_FORMAT_POST
			--sp;
			break;

		case PCD_PRINTHEX:
			work.AppendFormat ("%X", STACK(1));
			--sp;
			break;

		case PCD_PRINTCHARACTER:
			work += (char)STACK(1);
			--sp;
			break;

		case PCD_PRINTFIXED:
			work.AppendFormat ("%g", ACSToDouble(STACK(1)));
			--sp;
			break;

		// [BC] Print activator's name
		// [RH] Fancied up a bit
		case PCD_PRINTNAME:
			{
				player_t *player = NULL;

				if (STACK(1) < 0)
				{
					switch (STACK(1))
					{
					case PRINTNAME_LEVELNAME:
						work += Level->LevelName;
						break;

					case PRINTNAME_LEVEL:
					{
						FString uppername = Level->MapName;
						uppername.ToUpper();
						work += uppername;
						break; 
					}

					case PRINTNAME_NEXTLEVEL:
					{
						FString uppername = Level->NextMap;
						uppername.ToUpper();
						work += uppername;
						break;
					}

					case PRINTNAME_NEXTSECRET:
					{
						FString uppername = Level->NextSecretMap;
						uppername.ToUpper();
						work += uppername;
						break;
					}

					case PRINTNAME_SKILL:
						work += G_SkillName();
						break;

					default:
						work += ' ';
						break;
					}
					sp--;
					break;

				}
				else if (STACK(1) == 0 || (unsigned)STACK(1) > MAXPLAYERS)
				{
					if (activator)
					{
						player = activator->player;
					}
				}
				else if (Level->PlayerInGame(STACK(1)-1))
				{
					player = Level->Players[STACK(1)-1];
				}
				else
				{
					work.AppendFormat ("Player %d", STACK(1));
					sp--;
					break;
				}
				if (player)
				{
					work += player->userinfo.GetName();
				}
				else if (activator)
				{
					work += activator->GetTag();
				}
				else
				{
					work += ' ';
				}
				sp--;
			}
			break;

		// Print script character array
		case PCD_PRINTSCRIPTCHARARRAY:
		case PCD_PRINTSCRIPTCHRANGE:
			{
				int capacity, offset, a, c;
				if (CharArrayParms(capacity, offset, a, Stack, sp, pcd == PCD_PRINTSCRIPTCHRANGE))
				{
					while (capacity-- && (c = localarrays->Get(locals, a, offset)) != '\0')
					{
						work += (char)c;
						offset++;
					}
				}
			}
			break;

		// [JB] Print map character array
		case PCD_PRINTMAPCHARARRAY:
		case PCD_PRINTMAPCHRANGE:
			{
				int capacity, offset, a, c;
				if (CharArrayParms(capacity, offset, a, Stack, sp, pcd == PCD_PRINTMAPCHRANGE))
				{
					while (capacity-- && (c = activeBehavior->GetArrayVal (a, offset)) != '\0')
					{
						work += (char)c;
						offset++;
					}
				}
			}
			break;

		// [JB] Print world character array
		case PCD_PRINTWORLDCHARARRAY:
		case PCD_PRINTWORLDCHRANGE:
			{
				int capacity, offset, a, c;
				if (CharArrayParms(capacity, offset, a, Stack, sp, pcd == PCD_PRINTWORLDCHRANGE))
				{
					while (capacity-- && (c = ACS_WorldArrays[a][offset]) != '\0')
					{
						work += (char)c;
						offset++;
					}
				}
			}
			break;

		// [JB] Print global character array
		case PCD_PRINTGLOBALCHARARRAY:
		case PCD_PRINTGLOBALCHRANGE:
			{
				int capacity, offset, a, c;
				if (CharArrayParms(capacity, offset, a, Stack, sp, pcd == PCD_PRINTGLOBALCHRANGE))
				{
					while (capacity-- && (c = ACS_GlobalArrays[a][offset]) != '\0')
					{
						work += (char)c;
						offset++;
					}
				}
			}
			break;

		// [GRB] Print key name(s) for a command
		case PCD_PRINTBIND:
			lookup = Level->Behaviors.LookupString (STACK(1));
			if (lookup != NULL)
			{
				int key1, key2;

				Bindings.GetKeysForCommand ((char *)lookup, &key1, &key2);

				if (key2)
					work << KeyNames[key1] << " or " << KeyNames[key2];
				else if (key1)
					work << KeyNames[key1];
				else
					work << "??? (" << (char *)lookup << ')';
			}
			--sp;
			break;

		case PCD_ENDPRINT:
		case PCD_ENDPRINTBOLD:
		case PCD_MOREHUDMESSAGE:
		case PCD_ENDLOG:
			if (pcd == PCD_ENDLOG)
			{
				Printf ("%s\n", work.GetChars());
				STRINGBUILDER_FINISH(work);
			}
			else if (pcd != PCD_MOREHUDMESSAGE)
			{
				AActor *screen = activator;
				// If a missile is the activator, make the thing that
				// launched the missile the target of the print command.
				if (screen != NULL &&
					screen->player == NULL &&
					(screen->flags & MF_MISSILE) &&
					screen->target != NULL)
				{
					screen = screen->target;
				}
				if (pcd == PCD_ENDPRINTBOLD || screen == NULL ||
					screen->CheckLocalView())
				{
					C_MidPrint (activefont, work, pcd == PCD_ENDPRINTBOLD && (gameinfo.correctprintbold || (Level->flags2 & LEVEL2_HEXENHACK)));
				}
				STRINGBUILDER_FINISH(work);
			}
			else
			{
				optstart = -1;
			}
			break;

		case PCD_OPTHUDMESSAGE:
			optstart = sp;
			break;

		case PCD_ENDHUDMESSAGE:
		case PCD_ENDHUDMESSAGEBOLD:
			if (optstart == -1)
			{
				optstart = sp;
			}
			if (Level->isPrimaryLevel())
			{
				AActor *screen = activator;
				if (screen != NULL &&
					screen->player == NULL &&
					(screen->flags & MF_MISSILE) &&
					screen->target != NULL)
				{
					screen = screen->target;
				}
				if (Level->isPrimaryLevel() && (pcd == PCD_ENDHUDMESSAGEBOLD || screen == NULL || Level->isConsolePlayer(screen)))
				{
					int type = Stack[optstart-6];
					int id = Stack[optstart-5];
					EColorRange color;
					float x = ACSToFloat(Stack[optstart-3]);
					float y = ACSToFloat(Stack[optstart-2]);
					float holdTime = ACSToFloat(Stack[optstart-1]);
					float alpha;
					DHUDMessage *msg;

					if (type & HUDMSG_COLORSTRING)
					{
						color = V_FindFontColor(Level->Behaviors.LookupString(Stack[optstart-4]));
					}
					else
					{
						auto c = Stack[optstart - 4];
						color = (EColorRange)((unsigned)(c) >= NUM_TEXT_COLORS ? CR_UNTRANSLATED : (c));
					}

					switch (type & 0xFF)
					{
					default:	// normal
						alpha = (optstart < sp) ? ACSToFloat(Stack[optstart]) : 1.f;
						msg = Create<DHUDMessage> (activefont, work, x, y, hudwidth, hudheight, color, holdTime);
						break;
					case 1:		// fade out
						{
							float fadeTime = (optstart < sp) ? ACSToFloat(Stack[optstart]) : 0.5f;
							alpha = (optstart < sp-1) ? ACSToFloat(Stack[optstart+1]) : 1.f;
							msg = Create<DHUDMessageFadeOut> (activefont, work, x, y, hudwidth, hudheight, color, holdTime, fadeTime);
						}
						break;
					case 2:		// type on, then fade out
						{
							float typeTime = (optstart < sp) ? ACSToFloat(Stack[optstart]) : 0.05f;
							float fadeTime = (optstart < sp-1) ? ACSToFloat(Stack[optstart+1]) : 0.5f;
							alpha = (optstart < sp-2) ? ACSToFloat(Stack[optstart+2]) : 1.f;
							msg = Create<DHUDMessageTypeOnFadeOut> (activefont, work, x, y, hudwidth, hudheight, color, typeTime, holdTime, fadeTime);
						}
						break;
					case 3:		// fade in, then fade out
						{
							float inTime = (optstart < sp) ? ACSToFloat(Stack[optstart]) : 0.5f;
							float outTime = (optstart < sp-1) ? ACSToFloat(Stack[optstart+1]) : 0.5f;
							alpha = (optstart < sp-2) ? ACSToFloat(Stack[optstart + 2]) : 1.f;
							msg = Create<DHUDMessageFadeInOut> (activefont, work, x, y, hudwidth, hudheight, color, holdTime, inTime, outTime);
						}
						break;
					}
					msg->SetClipRect(ClipRectLeft, ClipRectTop, ClipRectWidth, ClipRectHeight, HandleAspect);
					if (WrapWidth != 0)
					{
						msg->SetWrapWidth(WrapWidth);
					}
					msg->SetVisibility((type & HUDMSG_VISIBILITY_MASK) >> HUDMSG_VISIBILITY_SHIFT);
					if (type & HUDMSG_NOWRAP)
					{
						msg->SetNoWrap(true);
					}
					if (type & HUDMSG_ALPHA)
					{
						msg->SetAlpha(alpha);
					}
					if (type & HUDMSG_ADDBLEND)
					{
						msg->SetRenderStyle(STYLE_Add);
					}
					StatusBar->AttachMessage (msg, id ? 0xff000000|id : 0,
						(type & HUDMSG_LAYER_MASK) >> HUDMSG_LAYER_SHIFT);
					if (type & HUDMSG_LOG)
					{
						int consolecolor = color >= CR_BRICK && color <= CR_YELLOW ? color + 'A' : '-';
						Printf(PRINT_DEFAULT, PRINTF_NONOTIFY, "\n" TEXTCOLOR_ESCAPESTR "%c%s\n%s\n%s\n", consolecolor, console_bar, work.GetChars(), console_bar);
					}
				}
			}
			STRINGBUILDER_FINISH(work);
			sp = optstart-6;
			break;

		case PCD_SETFONT:
			DoSetFont (STACK(1));
			sp--;
			break;

		case PCD_SETFONTDIRECT:
			DoSetFont (TAGSTR(uallong(pc[0])));
			pc++;
			break;

		case PCD_PLAYERCOUNT:
			PushToStack (CountPlayers ());
			break;

		case PCD_GAMETYPE:
			if (gamestate == GS_TITLELEVEL)
				PushToStack (GAME_TITLE_MAP);
			else if (deathmatch)
				PushToStack (GAME_NET_DEATHMATCH);
			else if (multiplayer)
				PushToStack (GAME_NET_COOPERATIVE);
			else
				PushToStack (GAME_SINGLE_PLAYER);
			break;

		case PCD_GAMESKILL:
			PushToStack (G_SkillProperty(SKILLP_ACSReturn));
			break;

// [BC] Start ST PCD's
		case PCD_ISNETWORKGAME:
			PushToStack(netgame);
			break;

		case PCD_PLAYERTEAM:
			if ( activator && activator->player )
				PushToStack( activator->player->userinfo.GetTeam() );
			else
				PushToStack( 0 );
			break;

		case PCD_PLAYERHEALTH:
			if (activator)
				PushToStack (activator->health);
			else
				PushToStack (0);
			break;

		case PCD_PLAYERARMORPOINTS:
			if (activator)
			{
				auto armor = activator->FindInventory(NAME_BasicArmor);
				PushToStack (armor ? armor->IntVar(NAME_Amount) : 0);
			}
			else
			{
				PushToStack (0);
			}
			break;

		case PCD_PLAYERFRAGS:
			if (activator && activator->player)
				PushToStack (activator->player->fragcount);
			else
				PushToStack (0);
			break;

		case PCD_MUSICCHANGE:
			lookup = Level->Behaviors.LookupString (STACK(2));
			if (lookup != NULL)
			{
				S_ChangeMusic (lookup, STACK(1));
			}
			sp -= 2;
			break;

		case PCD_SINGLEPLAYER:
			PushToStack (!multiplayer);
			break;
// [BC] End ST PCD's

		case PCD_TIMER:
			PushToStack (Level->time);
			break;

		case PCD_SECTORSOUND:
			lookup = Level->Behaviors.LookupString (STACK(2));
			if (lookup != NULL)
			{
				if (activationline)
				{
					S_Sound (
						activationline->frontsector,
						CHAN_AUTO, 0,	// Not CHAN_AREA, because that'd probably break existing scripts.
						lookup,
						(float)(STACK(1)) / 127.f,
						ATTN_NORM);
				}
				else
				{
					S_Sound (
						CHAN_AUTO, 0,
						lookup,
						(float)(STACK(1)) / 127.f,
						ATTN_NORM);
				}
			}
			sp -= 2;
			break;

		case PCD_AMBIENTSOUND:
			lookup = Level->Behaviors.LookupString (STACK(2));
			if (lookup != NULL)
			{
				S_Sound (CHAN_AUTO, 0,
						 lookup,
						 (float)(STACK(1)) / 127.f, ATTN_NONE);
			}
			sp -= 2;
			break;

		case PCD_LOCALAMBIENTSOUND:
			lookup = Level->Behaviors.LookupString (STACK(2));
			if (lookup != NULL && activator && activator->CheckLocalView())
			{
				S_Sound (CHAN_AUTO, 0,
						 lookup,
						 (float)(STACK(1)) / 127.f, ATTN_NONE);
			}
			sp -= 2;
			break;

		case PCD_ACTIVATORSOUND:
			lookup = Level->Behaviors.LookupString (STACK(2));
			if (lookup != NULL)
			{
				if (activator != NULL)
				{
					S_Sound (activator, CHAN_AUTO, 0,
							 lookup,
							 (float)(STACK(1)) / 127.f, ATTN_NORM);
				}
				else
				{
					S_Sound (CHAN_AUTO, 0,
							 lookup,
							 (float)(STACK(1)) / 127.f, ATTN_NONE);
				}
			}
			sp -= 2;
			break;

		case PCD_SOUNDSEQUENCE:
			lookup = Level->Behaviors.LookupString (STACK(1));
			if (lookup != NULL)
			{
				if (activationline != NULL)
				{
					SN_StartSequence (activationline->frontsector, CHAN_FULLHEIGHT, lookup, 0);
				}
			}
			sp--;
			break;

		case PCD_SETLINETEXTURE:
			SetLineTexture (STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 4;
			break;

		case PCD_REPLACETEXTURES:
		{
			const char *fromname = Level->Behaviors.LookupString(STACK(3));
			const char *toname = Level->Behaviors.LookupString(STACK(2));

			Level->ReplaceTextures(fromname, toname, STACK(1));
			sp -= 3;
			break;
		}

		case PCD_SETLINEBLOCKING:
			{
				int lineno;

				auto itr = Level->GetLineIdIterator(STACK(2));
				while ((lineno = itr.Next()) >= 0)
				{
					auto &line = Level->lines[lineno];
					switch (STACK(1))
					{
					case BLOCK_NOTHING:
						line.flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING|ML_RAILING|ML_BLOCK_PLAYERS);
						break;
					case BLOCK_CREATURES:
					default:
						line.flags &= ~(ML_BLOCKEVERYTHING|ML_RAILING|ML_BLOCK_PLAYERS);
						line.flags |= ML_BLOCKING;
						break;
					case BLOCK_EVERYTHING:
						line.flags &= ~(ML_RAILING|ML_BLOCK_PLAYERS);
						line.flags |= ML_BLOCKING|ML_BLOCKEVERYTHING;
						break;
					case BLOCK_RAILING:
						line.flags &= ~(ML_BLOCKEVERYTHING|ML_BLOCK_PLAYERS);
						line.flags |= ML_RAILING|ML_BLOCKING;
						break;
					case BLOCK_PLAYERS:
						line.flags &= ~(ML_BLOCKEVERYTHING|ML_BLOCKING|ML_RAILING);
						line.flags |= ML_BLOCK_PLAYERS;
						break;
					}
				}

				sp -= 2;
			}
			break;

		case PCD_SETLINEMONSTERBLOCKING:
			{
				int line;

				auto itr = Level->GetLineIdIterator(STACK(2));
				while ((line = itr.Next()) >= 0)
				{
					if (STACK(1))
						Level->lines[line].flags |= ML_BLOCKMONSTERS;
					else
						Level->lines[line].flags &= ~ML_BLOCKMONSTERS;
				}

				sp -= 2;
			}
			break;

		case PCD_SETLINESPECIAL:
			{
				int linenum = -1;
				int specnum = STACK(6);
				int arg0 = STACK(5);

				// Convert named ACS "specials" into real specials.
				if (specnum >= -ACSF_ACS_NamedExecuteAlways && specnum <= -ACSF_ACS_NamedExecute)
				{
					specnum = NamedACSToNormalACS[-specnum - ACSF_ACS_NamedExecute];
					arg0 = -FName(Level->Behaviors.LookupString(arg0)).GetIndex();
				}

				auto itr = Level->GetLineIdIterator(STACK(7));
				while ((linenum = itr.Next()) >= 0)
				{
					line_t *line = &Level->lines[linenum];
					line->special = specnum;
					line->args[0] = arg0;
					line->args[1] = STACK(4);
					line->args[2] = STACK(3);
					line->args[3] = STACK(2);
					line->args[4] = STACK(1);
					DPrintf(DMSG_SPAMMY, "Set special on line %d (id %d) to %d(%d,%d,%d,%d,%d)\n",
						linenum, STACK(7), specnum, arg0, STACK(4), STACK(3), STACK(2), STACK(1));
				}
				sp -= 7;
			}
			break;

		case PCD_SETTHINGSPECIAL:
			{
				int specnum = STACK(6);
				int arg0 = STACK(5);

				// Convert named ACS "specials" into real specials.
				if (specnum >= -ACSF_ACS_NamedExecuteAlways && specnum <= -ACSF_ACS_NamedExecute)
				{
					specnum = NamedACSToNormalACS[-specnum - ACSF_ACS_NamedExecute];
					arg0 = -FName(Level->Behaviors.LookupString(arg0)).GetIndex();
				}

				if (STACK(7) != 0)
				{
					auto iterator = Level->GetActorIterator(STACK(7));
					AActor *actor;

					while ( (actor = iterator.Next ()) )
					{
						actor->special = specnum;
						actor->args[0] = arg0;
						actor->args[1] = STACK(4);
						actor->args[2] = STACK(3);
						actor->args[3] = STACK(2);
						actor->args[4] = STACK(1);
					}
				}
				else if (activator != NULL)
				{
					activator->special = specnum;
					activator->args[0] = arg0;
					activator->args[1] = STACK(4);
					activator->args[2] = STACK(3);
					activator->args[3] = STACK(2);
					activator->args[4] = STACK(1);
				}
				sp -= 7;
			}
			break;

		case PCD_THINGSOUND:
			lookup = Level->Behaviors.LookupString (STACK(2));
			if (lookup != NULL)
			{
				auto iterator = Level->GetActorIterator(STACK(3));
				AActor *spot;

				while ( (spot = iterator.Next ()) )
				{
					S_Sound (spot, CHAN_AUTO, 0,
							 lookup,
							 (float)(STACK(1))/127.f, ATTN_NORM);
				}
			}
			sp -= 3;
			break;

		case PCD_FIXEDMUL:
			STACK(2) = MulScale(STACK(2), STACK(1), 16);
			sp--;
			break;

		case PCD_FIXEDDIV:
		{
			int a = STACK(2), b = STACK(1);
			// Overflow check.
			if ((uint32_t)abs(a) >> (31 - 16) >= (uint32_t)abs(b)) STACK(2) = (a ^ b) < 0 ? FIXED_MIN : FIXED_MAX;
			else STACK(2) = DivScale(STACK(2), STACK(1), 16);
			sp--;
			break;
		}
		case PCD_SETGRAVITY:
			Level->gravity = ACSToDouble(STACK(1));
			sp--;
			break;

		case PCD_SETGRAVITYDIRECT:
			Level->gravity = ACSToDouble(uallong(pc[0]));
			pc++;
			break;

		case PCD_SETAIRCONTROL:
			Level->aircontrol = ACSToDouble(STACK(1));
			sp--;
			Level->AirControlChanged ();
			break;

		case PCD_SETAIRCONTROLDIRECT:
			Level->aircontrol = ACSToDouble(uallong(pc[0]));
			pc++;
			Level->AirControlChanged ();
			break;

		case PCD_SPAWN:
			STACK(6) = DoSpawn (STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1), false);
			sp -= 5;
			break;

		case PCD_SPAWNDIRECT:
			PushToStack (DoSpawn (TAGSTR(uallong(pc[0])), uallong(pc[1]), uallong(pc[2]), uallong(pc[3]), uallong(pc[4]), uallong(pc[5]), false));
			pc += 6;
			break;

		case PCD_SPAWNSPOT:
			STACK(4) = DoSpawnSpot (STACK(4), STACK(3), STACK(2), STACK(1), false);
			sp -= 3;
			break;

		case PCD_SPAWNSPOTDIRECT:
			PushToStack (DoSpawnSpot (TAGSTR(uallong(pc[0])), uallong(pc[1]), uallong(pc[2]), uallong(pc[3]), false));
			pc += 4;
			break;

		case PCD_SPAWNSPOTFACING:
			STACK(3) = DoSpawnSpotFacing (STACK(3), STACK(2), STACK(1), false);
			sp -= 2;
			break;

		case PCD_CLEARINVENTORY:
			ScriptUtil::Exec(NAME_ClearInventory, ScriptUtil::Pointer, activator.Get(), ScriptUtil::End);
			break;

		case PCD_CLEARACTORINVENTORY:
			if (STACK(1) == 0)
			{
				ScriptUtil::Exec(NAME_ClearInventory, ScriptUtil::Pointer, nullptr, ScriptUtil::End);
			}
			else
			{
				auto it = Level->GetActorIterator(STACK(1));
				AActor *actor;
				for (actor = it.Next(); actor != NULL; actor = it.Next())
				{
					ScriptUtil::Exec(NAME_ClearInventory, ScriptUtil::Pointer, actor , ScriptUtil::End);
				}
			}
			sp--;
			break;

		case PCD_GIVEINVENTORY:
		{
			int typeindex = FName(Level->Behaviors.LookupString(STACK(2))).GetIndex();
			ScriptUtil::Exec(NAME_GiveInventory, ScriptUtil::Pointer, activator.Get(), ScriptUtil::Int, typeindex, ScriptUtil::Int, STACK(1), ScriptUtil::End);
			sp -= 2;
			break;
		}

		case PCD_GIVEACTORINVENTORY:
		{
			int typeindex = FName(Level->Behaviors.LookupString(STACK(2))).GetIndex();
			FName type = FName(Level->Behaviors.LookupString(STACK(2)));
			if (STACK(3) == 0)
			{
				ScriptUtil::Exec(NAME_GiveInventory, ScriptUtil::Pointer, nullptr, ScriptUtil::Int, typeindex, ScriptUtil::Int, STACK(1), ScriptUtil::End);
			}
			else
			{
				auto it = Level->GetActorIterator(STACK(3));
				AActor *actor;
				for (actor = it.Next(); actor != NULL; actor = it.Next())
				{
					ScriptUtil::Exec(NAME_GiveInventory, ScriptUtil::Pointer, actor, ScriptUtil::Int, typeindex, ScriptUtil::Int, STACK(1), ScriptUtil::End);
				}
			}
			sp -= 3;
			break;
		}

		case PCD_GIVEINVENTORYDIRECT:
		{
			int typeindex = FName(Level->Behaviors.LookupString(TAGSTR(uallong(pc[0])))).GetIndex();
			ScriptUtil::Exec(NAME_GiveInventory, ScriptUtil::Pointer, activator.Get(), ScriptUtil::Int, typeindex, ScriptUtil::Int, uallong(pc[1]), ScriptUtil::End);
			pc += 2;
			break;
		}

		case PCD_TAKEINVENTORY:
		{
			int typeindex = FName(Level->Behaviors.LookupString(STACK(2))).GetIndex();
			ScriptUtil::Exec(NAME_TakeInventory, ScriptUtil::Pointer, activator.Get(), ScriptUtil::Int, typeindex, ScriptUtil::Int, STACK(1), ScriptUtil::End);
			sp -= 2;
			break;
		}

		case PCD_TAKEACTORINVENTORY:
		{
			int typeindex = FName(Level->Behaviors.LookupString(STACK(2))).GetIndex();
			FName type = FName(Level->Behaviors.LookupString(STACK(2)));
			if (STACK(3) == 0)
			{
				ScriptUtil::Exec(NAME_TakeInventory, ScriptUtil::Pointer, nullptr, ScriptUtil::Int, typeindex, ScriptUtil::Int, STACK(1), ScriptUtil::End);
			}
			else
			{
				auto it = Level->GetActorIterator(STACK(3));
				AActor *actor;
				for (actor = it.Next(); actor != NULL; actor = it.Next())
				{
					ScriptUtil::Exec(NAME_TakeInventory, ScriptUtil::Pointer, actor, ScriptUtil::Int, typeindex, ScriptUtil::Int, STACK(1), ScriptUtil::End);
				}
			}
			sp -= 3;
			break;
		}

		case PCD_TAKEINVENTORYDIRECT:
		{
			int typeindex = FName(Level->Behaviors.LookupString(TAGSTR(uallong(pc[0])))).GetIndex();
			ScriptUtil::Exec(NAME_TakeInventory, ScriptUtil::Pointer, activator.Get(), ScriptUtil::Int, typeindex, ScriptUtil::Int, uallong(pc[1]), ScriptUtil::End);
			pc += 2;
			break;
		}

		case PCD_CHECKINVENTORY:
			STACK(1) = CheckInventory (activator, Level->Behaviors.LookupString (STACK(1)), false);
			break;

		case PCD_CHECKACTORINVENTORY:
			STACK(2) = CheckInventory (Level->SingleActorFromTID(STACK(2), NULL),
										Level->Behaviors.LookupString (STACK(1)), false);
			sp--;
			break;

		case PCD_CHECKINVENTORYDIRECT:
			PushToStack (CheckInventory (activator, Level->Behaviors.LookupString (TAGSTR(uallong(pc[0]))), false));
			pc += 1;
			break;

		case PCD_USEINVENTORY:
			STACK(1) = UseInventory (Level, activator, Level->Behaviors.LookupString (STACK(1)));
			break;

		case PCD_USEACTORINVENTORY:
			{
				int ret = 0;
				const char *type = Level->Behaviors.LookupString(STACK(1));
				if (STACK(2) == 0)
				{
					ret = UseInventory(Level, NULL, type);
				}
				else
				{
					auto it = Level->GetActorIterator(STACK(2));
					AActor *actor;
					for (actor = it.Next(); actor != NULL; actor = it.Next())
					{
						ret += UseInventory(Level, actor, type);
					}
				}
				STACK(2) = ret;
				sp--;
			}
			break;

		case PCD_GETSIGILPIECES:
			{
				AActor *sigil;

				if (activator == NULL || (sigil = activator->FindInventory(NAME_Sigil)) == NULL)
				{
					PushToStack (0);
				}
				else
				{
					PushToStack (sigil->health);
				}
			}
			break;

		case PCD_GETAMMOCAPACITY:
			if (activator != NULL)
			{
				PClass *type = PClass::FindClass (Level->Behaviors.LookupString (STACK(1)));

				if (type != NULL && type->ParentClass == PClass::FindActor(NAME_Ammo))
				{
					auto item = activator->FindInventory (static_cast<PClassActor *>(type));
					if (item != NULL)
					{
						STACK(1) = item->IntVar(NAME_MaxAmount);
					}
					else
					{
						STACK(1) = GetDefaultByType (type)->IntVar(NAME_MaxAmount);
					}
				}
				else
				{
					STACK(1) = 0;
				}
			}
			else
			{
				STACK(1) = 0;
			}
			break;

		case PCD_SETAMMOCAPACITY:
			if (activator != NULL)
			{
				PClassActor *type = PClass::FindActor (Level->Behaviors.LookupString (STACK(2)));

				if (type != NULL && type->ParentClass == PClass::FindActor(NAME_Ammo))
				{
					auto item = activator->FindInventory (type);
					if (item != NULL)
					{
						item->IntVar(NAME_MaxAmount) = STACK(1);
					}
					else
					{
						item = activator->GiveInventoryType (type);
						if (item != NULL)
						{
							item->IntVar(NAME_MaxAmount) = STACK(1);
							item->IntVar(NAME_Amount) = 0;
						}
					}
				}
			}
			sp -= 2;
			break;

		case PCD_SETMUSIC:
			S_ChangeMusic (Level->Behaviors.LookupString (STACK(3)), STACK(2));
			sp -= 3;
			break;

		case PCD_SETMUSICDIRECT:
			S_ChangeMusic (Level->Behaviors.LookupString (TAGSTR(uallong(pc[0]))), uallong(pc[1]));
			pc += 3;
			break;

		case PCD_LOCALSETMUSIC:
			if (Level->isConsolePlayer(activator))
			{
				S_ChangeMusic (Level->Behaviors.LookupString (STACK(3)), STACK(2));
			}
			sp -= 3;
			break;

		case PCD_LOCALSETMUSICDIRECT:
			if (Level->isConsolePlayer(activator))
			{
				S_ChangeMusic (Level->Behaviors.LookupString (TAGSTR(uallong(pc[0]))), uallong(pc[1]));
			}
			pc += 3;
			break;

		case PCD_FADETO:
			DoFadeTo (STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 5;
			break;

		case PCD_FADERANGE:
			DoFadeRange (STACK(9), STACK(8), STACK(7), STACK(6),
						 STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 9;
			break;

		case PCD_CANCELFADE:
			{
				auto iterator = Level->GetThinkerIterator<DFlashFader>();
				DFlashFader *fader;

				while ( (fader = iterator.Next()) )
				{
					if (activator == NULL || fader->WhoFor() == activator)
					{
						fader->Cancel ();
					}
				}
			}
			break;

		case PCD_PLAYMOVIE:
			STACK(1) = -1;
			break;

		case PCD_SETACTORPOSITION:
			{
				bool result = false;
				AActor *actor = Level->SingleActorFromTID (STACK(5), activator);
				if (actor != NULL)
					result = P_MoveThing(actor, DVector3(ACSToDouble(STACK(4)), ACSToDouble(STACK(3)), ACSToDouble(STACK(2))), !!STACK(1));
				sp -= 4;
				STACK(1) = result;
			}
			break;

		case PCD_GETACTORX:
		case PCD_GETACTORY:
		case PCD_GETACTORZ:
			{
				AActor *actor = Level->SingleActorFromTID(STACK(1), activator);
				if (actor == NULL)
				{
					STACK(1) = 0;
				}
				else if (pcd == PCD_GETACTORZ)
				{
					STACK(1) = DoubleToACS(actor->Z() + actor->GetBobOffset());
				}
				else
				{
					STACK(1) = DoubleToACS(pcd == PCD_GETACTORX ? actor->X() : actor->Y());
				}
			}
			break;

		case PCD_GETACTORFLOORZ:
			{
				AActor *actor = Level->SingleActorFromTID(STACK(1), activator);
				STACK(1) = actor == NULL ? 0 : DoubleToACS(actor->floorz);
			}
			break;

		case PCD_GETACTORCEILINGZ:
			{
				AActor *actor = Level->SingleActorFromTID(STACK(1), activator);
				STACK(1) = actor == NULL ? 0 : DoubleToACS(actor->ceilingz);
			}
			break;

		case PCD_GETACTORANGLE:
			{
				AActor *actor = Level->SingleActorFromTID(STACK(1), activator);
				STACK(1) = actor == NULL ? 0 : AngleToACS(actor->Angles.Yaw);
			}
			break;

		case PCD_GETACTORPITCH:
			{
				AActor *actor = Level->SingleActorFromTID(STACK(1), activator);
				STACK(1) = actor == NULL ? 0 : PitchToACS(actor->Angles.Pitch);
			}
			break;

		case PCD_GETLINEROWOFFSET:
			if (activationline != NULL)
			{
				PushToStack (int(activationline->sidedef[0]->GetTextureYOffset(side_t::mid)));
			}
			else
			{
				PushToStack (0);
			}
			break;

		case PCD_GETSECTORFLOORZ:
		case PCD_GETSECTORCEILINGZ:
			// Arguments are (tag, x, y). If you don't use slopes, then (x, y) don't
			// really matter and can be left as (0, 0) if you like.
			// [Dusk] If tag = 0, then this returns the z height at whatever sector
			// is in x, y.
			{
				int tag = STACK(3);
				int secnum;
				double x = double(STACK(2));
				double y = double(STACK(1));
				double z = 0;

				if (tag != 0)
					secnum = Level->FindFirstSectorFromTag (tag);
				else
					secnum = Level->PointInSector (DVector2(x, y))->sectornum;

				if (secnum >= 0)
				{
					if (pcd == PCD_GETSECTORFLOORZ)
					{
						z = Level->sectors[secnum].floorplane.ZatPoint (x, y);
					}
					else
					{
						z = Level->sectors[secnum].ceilingplane.ZatPoint (x, y);
					}
				}
				sp -= 2;
				STACK(1) = DoubleToACS(z);
			}
			break;

		case PCD_GETSECTORLIGHTLEVEL:
			{
				int secnum = Level->FindFirstSectorFromTag (STACK(1));
				int z = -1;

				if (secnum >= 0)
				{
					z = Level->sectors[secnum].lightlevel;
				}
				STACK(1) = z;
			}
			break;

		case PCD_SETFLOORTRIGGER:
		case PCD_SETCEILINGTRIGGER:
		{
			int secnum = Level->FindFirstSectorFromTag(STACK(8));
			if (secnum >= 0)
			{
				Level->CreateThinker<DPlaneWatcher>(activator, activationline, backSide, pcd == PCD_SETCEILINGTRIGGER, &Level->sectors[secnum],
					STACK(7), STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			}

			sp -= 8;
			break;
		}

		case PCD_STARTTRANSLATION:
			{
				int i = STACK(1);
				sp--;
				if (i >= 1 && i <= MAX_ACS_TRANSLATIONS)
				{
					translation = new FRemapTable;
					translation->MakeIdentity();
					transi = i - 1;
				}
			}
			break;

		case PCD_TRANSLATIONRANGE1:
			{ // translation using palette shifting
				int start = STACK(4);
				int end = STACK(3);
				int pal1 = STACK(2);
				int pal2 = STACK(1);
				sp -= 4;

				if (translation != NULL)
					translation->AddIndexRange(start, end, pal1, pal2);
			}
			break;

		case PCD_TRANSLATIONRANGE2:
			{ // translation using RGB values
			  // (would HSV be a good idea too?)
				int start = STACK(8);
				int end = STACK(7);
				int r1 = STACK(6);
				int g1 = STACK(5);
				int b1 = STACK(4);
				int r2 = STACK(3);
				int g2 = STACK(2);
				int b2 = STACK(1);
				sp -= 8;

				if (translation != NULL)
					translation->AddColorRange(start, end, r1, g1, b1, r2, g2, b2);
			}
			break;

		case PCD_TRANSLATIONRANGE3:
			{ // translation using desaturation
				int start = STACK(8);
				int end = STACK(7);
				int r1 = STACK(6);
				int g1 = STACK(5);
				int b1 = STACK(4);
				int r2 = STACK(3);
				int g2 = STACK(2);
				int b2 = STACK(1);
				sp -= 8;

				if (translation != NULL)
					translation->AddDesaturation(start, end,
						ACSToDouble(r1), ACSToDouble(g1), ACSToDouble(b1),
						ACSToDouble(r2), ACSToDouble(g2), ACSToDouble(b2));
			}
			break;

		case PCD_TRANSLATIONRANGE4:
			{ // Colourise translation
				int start = STACK(5);
				int end = STACK(4);
				int r = STACK(3);
				int g = STACK(2);
				int b = STACK(1);
				sp -= 5;

				if (translation != NULL)
					translation->AddColourisation(start, end, r, g, b);
			}
			break;

		case PCD_TRANSLATIONRANGE5:
			{ // Tint translation
				int start = STACK(6);
				int end = STACK(5);
				int a = STACK(4);
				int r = STACK(3);
				int g = STACK(2);
				int b = STACK(1);
				sp -= 6;

				if (translation != NULL)
					translation->AddTint(start, end, r, g, b, a);
			}
			break;

		case PCD_ENDTRANSLATION:
			if (translation != NULL)
			{
				GPalette.UpdateTranslation(TRANSLATION(TRANSLATION_LevelScripted, transi), translation);
				delete translation;
				translation = NULL;
			}
			break;

		case PCD_SIN:
			STACK(1) = DoubleToACS(ACSToAngle(STACK(1)).Sin());
			break;

		case PCD_COS:
			STACK(1) = DoubleToACS(ACSToAngle(STACK(1)).Cos());
			break;

		case PCD_VECTORANGLE:
			STACK(2) = AngleToACS(VecToAngle(STACK(2), STACK(1)).Degrees);
			sp--;
			break;

        case PCD_CHECKWEAPON:
            if (activator == NULL || activator->player == NULL || // Non-players do not have weapons
                activator->player->ReadyWeapon == NULL)
            {
                STACK(1) = 0;
            }
            else
            {
				STACK(1) = activator->player->ReadyWeapon->GetClass()->TypeName == FName(Level->Behaviors.LookupString (STACK(1)), true);
            }
            break;

		case PCD_SETWEAPON:
			STACK(1) = ScriptUtil::Exec(NAME_SetWeapon, ScriptUtil::Pointer, activator.Get(), ScriptUtil::Class, GetClassForIndex(STACK(1)), ScriptUtil::End);
			break;

		case PCD_SETMARINEWEAPON:
			ScriptUtil::Exec(NAME_SetMarineWeapon, ScriptUtil::Pointer, Level, ScriptUtil::Pointer, activator.Get(), ScriptUtil::Int, STACK(2), ScriptUtil::Int, STACK(1), ScriptUtil::End);
			sp -= 2;
			break;

		case PCD_SETMARINESPRITE:
			ScriptUtil::Exec(NAME_SetMarineSprite, ScriptUtil::Pointer, Level, ScriptUtil::Pointer, activator.Get(), ScriptUtil::Int, STACK(2), ScriptUtil::Class, GetClassForIndex(STACK(1)), ScriptUtil::End);
			sp -= 2;
			break;

		case PCD_SETACTORPROPERTY:
			SetActorProperty (STACK(3), STACK(2), STACK(1));
			sp -= 3;
			break;

		case PCD_GETACTORPROPERTY:
			STACK(2) = GetActorProperty (STACK(2), STACK(1));
			sp -= 1;
			break;

		case PCD_GETPLAYERINPUT:
			STACK(2) = GetPlayerInput (STACK(2), STACK(1));
			sp -= 1;
			break;

		case PCD_PLAYERNUMBER:
			if (activator == NULL || activator->player == NULL)
			{
				PushToStack (-1);
			}
			else
			{
				PushToStack (Level->PlayerNum(activator->player));
			}
			break;

		case PCD_PLAYERINGAME:
			if (STACK(1) < 0 || STACK(1) >= MAXPLAYERS)
			{
				STACK(1) = false;
			}
			else
			{
				STACK(1) = Level->PlayerInGame(STACK(1));
			}
			break;

		case PCD_PLAYERISBOT:
			if (STACK(1) < 0 || STACK(1) >= MAXPLAYERS || !Level->PlayerInGame(STACK(1)))
			{
				STACK(1) = false;
			}
			else
			{
				STACK(1) = (Level->Players[STACK(1)]->Bot != nullptr);
			}
			break;

		case PCD_ACTIVATORTID:
			if (activator == NULL)
			{
				PushToStack (0);
			}
			else
			{
				PushToStack (activator->tid);
			}
			break;

		case PCD_GETSCREENWIDTH:
			PushToStack (SCREENWIDTH);
			break;

		case PCD_GETSCREENHEIGHT:
			PushToStack (SCREENHEIGHT);
			break;

		case PCD_THING_PROJECTILE2:
			// Like Thing_Projectile(Gravity) specials, but you can give the
			// projectile a TID.
			// Thing_Projectile2 (tid, type, angle, speed, vspeed, gravity, newtid);
			Level->EV_Thing_Projectile(STACK(7), activator, STACK(6), NULL, STACK(5) * (360. / 256.),
				STACK(4) / 8., STACK(3) / 8., 0, NULL, STACK(2), STACK(1), false);
			sp -= 7;
			break;

		case PCD_SPAWNPROJECTILE:
			// Same, but takes an actor name instead of a spawn ID.
			Level->EV_Thing_Projectile(STACK(7), activator, 0, Level->Behaviors.LookupString(STACK(6)), STACK(5) * (360. / 256.),
				STACK(4) / 8., STACK(3) / 8., 0, NULL, STACK(2), STACK(1), false);
			sp -= 7;
			break;

		case PCD_STRLEN:
			{
				const char *str = Level->Behaviors.LookupString(STACK(1));
				if (str != NULL)
				{
					STACK(1) = int32_t(strlen(str));
					break;
				}

				static bool StrlenInvalidPrintedAlready = false;
				if (!StrlenInvalidPrintedAlready)
				{
					Printf(PRINT_DEFAULT, PRINTF_BOLD, "Warning: ACS function strlen called with invalid string argument.\n");
					StrlenInvalidPrintedAlready = true;
				}
				STACK(1) = 0;
			}
			break;

		case PCD_GETCVAR:
			// This should not use Level->PlayerNum!
			STACK(1) = DoGetCVar(GetCVar(activator && activator->player? int(activator->player - players) : -1, Level->Behaviors.LookupString(STACK(1))), false);
			break;

		case PCD_SETHUDSIZE:
			hudwidth = abs (STACK(3));
			hudheight = abs (STACK(2));
			if (STACK(1) != 0)
			{ // Negative height means to cover the status bar
				hudheight = -hudheight;
			}
			sp -= 3;
			break;

		case PCD_GETLEVELINFO:
			switch (STACK(1))
			{
			case LEVELINFO_PAR_TIME:		STACK(1) = Level->partime;			break;
			case LEVELINFO_SUCK_TIME:		STACK(1) = Level->sucktime;			break;
			case LEVELINFO_CLUSTERNUM:		STACK(1) = Level->cluster;			break;
			case LEVELINFO_LEVELNUM:		STACK(1) = Level->levelnum;			break;
			case LEVELINFO_TOTAL_SECRETS:	STACK(1) = Level->total_secrets;		break;
			case LEVELINFO_FOUND_SECRETS:	STACK(1) = Level->found_secrets;		break;
			case LEVELINFO_TOTAL_ITEMS:		STACK(1) = Level->total_items;		break;
			case LEVELINFO_FOUND_ITEMS:		STACK(1) = Level->found_items;		break;
			case LEVELINFO_TOTAL_MONSTERS:	STACK(1) = Level->total_monsters;	break;
			case LEVELINFO_KILLED_MONSTERS:	STACK(1) = Level->killed_monsters;	break;
			default:						STACK(1) = 0;						break;
			}
			break;

		case PCD_CHANGESKY:
			{
				const char *sky1name, *sky2name;

				sky1name = Level->Behaviors.LookupString (STACK(2));
				sky2name = Level->Behaviors.LookupString (STACK(1));
				if (sky1name[0] != 0)
				{
					Level->skytexture1 = TexMan.GetTextureID(sky1name, ETextureType::Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);
				}
				if (sky2name[0] != 0)
				{
					Level->skytexture2 = TexMan.GetTextureID(sky2name, ETextureType::Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);
				}
				InitSkyMap (Level);
				sp -= 2;
			}
			break;

		case PCD_SETCAMERATOTEXTURE:
			{
				const char *picname = Level->Behaviors.LookupString (STACK(2));
				AActor *camera;

				if (STACK(3) == 0)
				{
					camera = activator;
				}
				else
				{
					auto it = Level->GetActorIterator(STACK(3));
					camera = it.Next ();
				}

				if (camera != NULL)
				{
					FTextureID picnum = TexMan.CheckForTexture (picname, ETextureType::Wall, FTextureManager::TEXMAN_Overridable);
					if (!picnum.Exists())
					{
						Printf ("SetCameraToTexture: %s is not a texture\n", picname);
					}
					else
					{
						Level->canvasTextureInfo.Add(camera, picnum, STACK(1));
					}
				}
				sp -= 3;
			}
			break;

		case PCD_SETACTORANGLE:		// [GRB]
			SetActorAngle(activator, STACK(2), STACK(1), false);
			sp -= 2;
			break;

		case PCD_SETACTORPITCH:
			SetActorPitch(activator, STACK(2), STACK(1), false);
			sp -= 2;
			break;

		case PCD_SETACTORSTATE:
			{
				const char *statename = Level->Behaviors.LookupString (STACK(2));
				FState *state;

				if (STACK(3) == 0)
				{
					if (activator != NULL)
					{
						state = activator->GetClass()->FindStateByString (statename, !!STACK(1));
						if (state != NULL)
						{
							activator->SetState (state);
							STACK(3) = 1;
						}
						else
						{
							STACK(3) = 0;
						}
					}
				}
				else
				{
					auto iterator = Level->GetActorIterator(STACK(3));
					AActor *actor;
					int count = 0;

					while ( (actor = iterator.Next ()) )
					{
						state = actor->GetClass()->FindStateByString (statename, !!STACK(1));
						if (state != NULL)
						{
							actor->SetState (state);
							count++;
						}
					}
					STACK(3) = count;
				}
				sp -= 2;
			}
			break;

		case PCD_PLAYERCLASS:		// [GRB]
			if (STACK(1) < 0 || STACK(1) >= MAXPLAYERS || !Level->PlayerInGame(STACK(1)))
			{
				STACK(1) = -1;
			}
			else
			{
				STACK(1) = Level->Players[STACK(1)]->CurrentPlayerClass;
			}
			break;

		case PCD_GETPLAYERINFO:		// [GRB]
			if (STACK(2) < 0 || STACK(2) >= MAXPLAYERS || !Level->PlayerInGame(STACK(2)))
			{
				STACK(2) = -1;
			}
			else
			{
				player_t *pl = Level->Players[STACK(2)];
				userinfo_t *userinfo = &pl->userinfo;
				switch (STACK(1))
				{
				case PLAYERINFO_TEAM:			STACK(2) = userinfo->GetTeam(); break;
				case PLAYERINFO_AIMDIST:		STACK(2) = (int32_t)(userinfo->GetAimDist() * (0x40000000/90.)); break;	// Yes, this has been returning a BAM since its creation.
				case PLAYERINFO_COLOR:			STACK(2) = userinfo->GetColor(); break;
				case PLAYERINFO_GENDER:			STACK(2) = userinfo->GetGender(); break;
				case PLAYERINFO_NEVERSWITCH:	STACK(2) = userinfo->GetNeverSwitch(); break;
				case PLAYERINFO_MOVEBOB:		STACK(2) = DoubleToACS(userinfo->GetMoveBob()); break;
				case PLAYERINFO_STILLBOB:		STACK(2) = DoubleToACS(userinfo->GetStillBob()); break;
				case PLAYERINFO_PLAYERCLASS:	STACK(2) = userinfo->GetPlayerClassNum(); break;
				case PLAYERINFO_DESIREDFOV:		STACK(2) = (int)pl->DesiredFOV; break;
				case PLAYERINFO_FOV:			STACK(2) = (int)pl->FOV; break;
				default:						STACK(2) = 0; break;
				}
			}
			sp -= 1;
			break;

		case PCD_CHANGELEVEL:
			{
				Level->ChangeLevel(Level->Behaviors.LookupString(STACK(4)), STACK(3), STACK(2), STACK(1));
				sp -= 4;
			}
			break;

		case PCD_SECTORDAMAGE:
			{
				int tag = STACK(5);
				int amount = STACK(4);
				FName type = Level->Behaviors.LookupString(STACK(3));
				FName protection = FName (Level->Behaviors.LookupString(STACK(2)), true);
				PClassActor *protectClass = PClass::FindActor (protection);
				int flags = STACK(1);
				sp -= 5;

				P_SectorDamage(Level, tag, amount, type, protectClass, flags);
			}
			break;

		case PCD_THINGDAMAGE2:
			STACK(3) = Level->EV_Thing_Damage (STACK(3), activator, STACK(2), FName(Level->Behaviors.LookupString(STACK(1))));
			sp -= 2;
			break;

		case PCD_CHECKACTORCEILINGTEXTURE:
			STACK(2) = DoCheckActorTexture(STACK(2), activator, STACK(1), false);
			sp--;
			break;

		case PCD_CHECKACTORFLOORTEXTURE:
			STACK(2) = DoCheckActorTexture(STACK(2), activator, STACK(1), true);
			sp--;
			break;

		case PCD_GETACTORLIGHTLEVEL:
		{
			AActor *actor = Level->SingleActorFromTID(STACK(1), activator);
			if (actor != NULL)
			{
				sector_t *sector = actor->Sector;
				if (sector->e->XFloor.lightlist.Size())
				{
					unsigned   i;
					TArray<lightlist_t> &lightlist = sector->e->XFloor.lightlist;

					STACK(1) = *lightlist.Last().p_lightlevel;
					for (i = 1; i < lightlist.Size(); i++)
					{
						if (lightlist[i].plane.ZatPoint(actor) <= actor->Z())
						{
							STACK(1) = *lightlist[i - 1].p_lightlevel;
							break;
						}
					}
				}
				else
				{
					STACK(1) = actor->Sector->lightlevel;
				}
			}
			else STACK(1) = 0;
			break;
		}

		case PCD_SETMUGSHOTSTATE:
			if (!multiplayer || (activator != nullptr && activator->CheckLocalView()))
			{
				StatusBar->SetMugShotState(Level->Behaviors.LookupString(STACK(1)));
			}
			sp--;
			break;

		case PCD_CHECKPLAYERCAMERA:
			{
				int playernum = STACK(1);

				if (playernum < 0 || playernum >= MAXPLAYERS || !Level->PlayerInGame(playernum) ||
					Level->Players[playernum]->camera == nullptr || Level->Players[playernum]->camera->player != nullptr)
				{
					STACK(1) = -1;
				}
				else
				{
					STACK(1) = Level->Players[playernum]->camera->tid;
				}
			}
			break;

		case PCD_CLASSIFYACTOR:
			STACK(1) = DoClassifyActor(STACK(1));
			break;

		case PCD_MORPHACTOR:
			{
				int tag = STACK(7);
				FName playerclass_name = Level->Behaviors.LookupString(STACK(6));
				auto playerclass = PClass::FindActor (playerclass_name);
				FName monsterclass_name = Level->Behaviors.LookupString(STACK(5));
				PClassActor *monsterclass = PClass::FindActor(monsterclass_name);
				int duration = STACK(4);
				int style = STACK(3);
				FName morphflash_name = Level->Behaviors.LookupString(STACK(2));
				PClassActor *morphflash = PClass::FindActor(morphflash_name);
				FName unmorphflash_name = Level->Behaviors.LookupString(STACK(1));
				PClassActor *unmorphflash = PClass::FindActor(unmorphflash_name);
				int changes = 0;

				if (tag == 0)
				{
					changes = P_MorphActor(activator, activator, playerclass, monsterclass, duration, style, morphflash, unmorphflash);
				}
				else
				{
					auto iterator = Level->GetActorIterator(tag);
					TArray<AActor*> actorsToMorph;

					while (AActor *actor = iterator.Next())
					{
						actorsToMorph.Push(actor);
					}

					for (AActor *actor : actorsToMorph)
					{
						changes += P_MorphActor(activator, actor, playerclass, monsterclass, duration, style, morphflash, unmorphflash);
					}
				}

				STACK(7) = changes;
				sp -= 6;
			}	
			break;

		case PCD_UNMORPHACTOR:
			{
				int tag = STACK(2);
				bool force = !!STACK(1);
				int changes = 0;

				if (tag == 0)
				{
					changes += P_UnmorphActor(activator, activator, 0, force);
				}
				else
				{
					auto iterator = Level->GetActorIterator(tag);
					TArray<AActor*> actorsToUnmorph;

					while (AActor *actor = iterator.Next())
					{
						actorsToUnmorph.Push(actor);
					}

					for (AActor *actor : actorsToUnmorph)
					{
						changes += P_UnmorphActor(activator, actor, 0, force);
					}
				}

				STACK(2) = changes;
				sp -= 1;
			}	
			break;

		case PCD_SAVESTRING:
			// Saves the string
			{
				const int str = GlobalACSStrings.AddString(work);
				PushToStack(str);
				STRINGBUILDER_FINISH(work);
			}		
			break;

		case PCD_STRCPYTOSCRIPTCHRANGE:
		case PCD_STRCPYTOMAPCHRANGE:
		case PCD_STRCPYTOWORLDCHRANGE:
		case PCD_STRCPYTOGLOBALCHRANGE:
			// source: stringid(2); stringoffset(1)
			// destination: capacity (3); stringoffset(4); arrayid (5); offset(6)

			{
				int index = STACK(4);
				int capacity = STACK(3);

				if (index < 0 || STACK(1) < 0)
				{
					// no writable destination, or negative offset to source string
					sp -= 5;
					Stack[sp-1] = 0; // false
					break;
				}

				index += STACK(6);
				
				lookup = Level->Behaviors.LookupString (STACK(2));
				
				if (!lookup) {
					// no data, operation complete
	STRCPYTORANGECOMPLETE:
					sp -= 5;
					Stack[sp-1] = 1; // true
					break;
				}

				for (int i = 0; i < STACK(1); i++)
				{
					if (! (*(lookup++)))
					{
						// no data, operation complete
						goto STRCPYTORANGECOMPLETE;
					}
				}

				switch (pcd)
				{
				case PCD_STRCPYTOSCRIPTCHRANGE:
					{
						int a = STACK(5);

						while (capacity-- > 0)
						{
							localarrays->Set(locals, a, index++, *lookup);
							if (! (*(lookup++))) goto STRCPYTORANGECOMPLETE; // complete with terminating 0
						}
						
						Stack[sp-6] = !(*lookup); // true/success if only terminating 0 was not copied
					}
					break;
				case PCD_STRCPYTOMAPCHRANGE:
					{
						int a = STACK(5);
						if (a < NUM_MAPVARS && a > 0 &&
							activeBehavior->MapVars[a])
						{
							Stack[sp-6] = activeBehavior->CopyStringToArray(*(activeBehavior->MapVars[a]), index, capacity, lookup);
						}
					}
					break;
				case PCD_STRCPYTOWORLDCHRANGE:
					{
						int a = STACK(5);

						while (capacity-- > 0)
						{
							ACS_WorldArrays[a][index++] = *lookup;
							if (! (*(lookup++))) goto STRCPYTORANGECOMPLETE; // complete with terminating 0
						}
						
						Stack[sp-6] = !(*lookup); // true/success if only terminating 0 was not copied
					}
					break;
				case PCD_STRCPYTOGLOBALCHRANGE:
					{
						int a = STACK(5);

						while (capacity-- > 0)
						{
							ACS_GlobalArrays[a][index++] = *lookup;
							if (! (*(lookup++))) goto STRCPYTORANGECOMPLETE; // complete with terminating 0
						}
						
						Stack[sp-6] = !(*lookup); // true/success if only terminating 0 was not copied
					}
					break;
				}
				sp -= 5;
			}
			break;

		case PCD_CONSOLECOMMAND:
		case PCD_CONSOLECOMMANDDIRECT:
			Printf (TEXTCOLOR_RED GAMENAME " doesn't support execution of console commands from scripts\n");
			if (pcd == PCD_CONSOLECOMMAND)
				sp -= 3;
			else
				pc += 3;
			break;
 		}
 	}

	if (runaway != 0 && InModuleScriptNumber >= 0)
	{
		auto scriptptr = activeBehavior->GetScriptPtr(InModuleScriptNumber);
		if (scriptptr != nullptr)
		{
			scriptptr->ProfileData.AddRun(runaway);
		}
		else
		{
			// It is pointless to continue execution. The script is broken and needs to be aborted.
			I_Error("Bad script definition encountered. Script %d is reported running but not present.\nThe most likely cause for this message is using 'delay' inside a function which is not supported.\nPlease check the ACS compiler used for compiling the script!", InModuleScriptNumber);
		}
	}

	if (state == SCRIPT_DivideBy0)
	{
		Printf ("Divide by zero in %s\n", ScriptPresentation(script).GetChars());
		state = SCRIPT_PleaseRemove;
	}
	else if (state == SCRIPT_ModulusBy0)
	{
		Printf ("Modulus by zero in %s\n", ScriptPresentation(script).GetChars());
		state = SCRIPT_PleaseRemove;
	}
	if (state == SCRIPT_PleaseRemove)
	{
		Unlink ();
		DLevelScript **running;
		if ((running = controller->RunningScripts.CheckKey(script)) != NULL &&
			*running == this)
		{
			controller->RunningScripts.Remove(script);
		}
	}
	else
	{
		this->pc = pc;
		assert (sp == 0);
	}
	return resultValue;
}

#undef PushtoStack

static DLevelScript *P_GetScriptGoing (FLevelLocals *l, AActor *who, line_t *where, int num, const ScriptPtr *code, FBehavior *module,
	const int *args, int argcount, int flags)
{
	DACSThinker *controller = l->ACSThinker;
	DLevelScript **running;

	if (controller && !(flags & ACS_ALWAYS) && (running = controller->RunningScripts.CheckKey(num)) != NULL)
	{
		if ((*running)->GetState() == DLevelScript::SCRIPT_Suspended)
		{
			(*running)->SetState(DLevelScript::SCRIPT_Running);
			return *running;
		}
		return NULL;
	}

	return Create<DLevelScript> (l, who, where, num, code, module, args, argcount, flags);
}

DLevelScript::DLevelScript (FLevelLocals *l, AActor *who, line_t *where, int num, const ScriptPtr *code, FBehavior *module,
	const int *args, int argcount, int flags)
	: activeBehavior (module)
{
	Level = l;
	if (Level->ACSThinker == nullptr)
		Level->ACSThinker = Level->CreateThinker<DACSThinker>();

	script = num;
	assert(code->VarCount >= code->ArgCount);
	Localvars.Resize(code->VarCount);
	memset(&Localvars[0], 0, code->VarCount * sizeof(int32_t));
	for (int i = 0; i < MIN<int>(argcount, code->ArgCount); ++i)
	{
		Localvars[i] = args[i];
	}
	pc = module->GetScriptAddress(code);
	InModuleScriptNumber = module->GetScriptIndex(code);
	activator = who;
	activationline = where;
	backSide = flags & ACS_BACKSIDE;
	hudwidth = hudheight = 0;
	ClipRectLeft = ClipRectTop = ClipRectWidth = ClipRectHeight = WrapWidth = 0;
	HandleAspect = true;
	state = SCRIPT_Running;

	// Hexen waited one second before executing any open scripts. I didn't realize
	// this when I wrote my ACS implementation. Now that I know, it's still best to
	// run them right away because there are several map properties that can't be
	// set in an editor. If an open script sets them, it looks dumb if a second
	// goes by while they're in their default state.

	if (!(flags & ACS_ALWAYS))
		Level->ACSThinker->RunningScripts[num] = this;

	Link();

	if (Level->flags2 & LEVEL2_HEXENHACK)
	{
		PutLast();
	}

	DPrintf(DMSG_SPAMMY, "%s started.\n", ScriptPresentation(num).GetChars());
}

void SetScriptState (DACSThinker *controller, int script, DLevelScript::EScriptState state)
{
	DLevelScript **running;

	if (controller != NULL && (running = controller->RunningScripts.CheckKey(script)) != NULL)
	{
		(*running)->SetState (state);
	}
}

void FLevelLocals::DoDeferedScripts ()
{
	const ScriptPtr *scriptdata;
	FBehavior *module;

	// Handle defered scripts in this step, too
	for(int i = info->deferred.Size()-1; i>=0; i--)
	{
		acsdefered_t *def = &info->deferred[i];
		switch (def->type)
		{
		case acsdefered_t::defexecute:
		case acsdefered_t::defexealways:
			scriptdata = Behaviors.FindScript (def->script, module);
			if (scriptdata)
			{
				P_GetScriptGoing (this, (unsigned)def->playernum < MAXPLAYERS &&
					PlayerInGame(def->playernum) ? Players[def->playernum]->mo : nullptr,
					nullptr, def->script,
					scriptdata, module,
					def->args, 3,
					def->type == acsdefered_t::defexealways ? ACS_ALWAYS : 0);
			}
			else
			{
				Printf ("P_DoDeferredScripts: Unknown %s\n", ScriptPresentation(def->script).GetChars());
			}
			break;

		case acsdefered_t::defsuspend:
			SetScriptState (ACSThinker, def->script, DLevelScript::SCRIPT_Suspended);
			DPrintf (DMSG_SPAMMY, "Deferred suspend of %s\n", ScriptPresentation(def->script).GetChars());
			break;

		case acsdefered_t::defterminate:
			SetScriptState (ACSThinker, def->script, DLevelScript::SCRIPT_PleaseRemove);
			DPrintf (DMSG_SPAMMY, "Deferred terminate of %s\n", ScriptPresentation(def->script).GetChars());
			break;
		}
	}
	info->deferred.Clear();
}

static void addDefered (level_info_t *i, acsdefered_t::EType type, int script, const int *args, int argcount, AActor *who)
{
	if (i)
	{
		acsdefered_t &def = i->deferred[i->deferred.Reserve(1)];
		int j;

		def.type = type;
		def.script = script;
		for (j = 0; (size_t)j < countof(def.args) && j < argcount; ++j)
		{
			def.args[j] = args[j];
		}
		while ((size_t)j < countof(def.args))
		{
			def.args[j++] = 0;
		}
		if (who != NULL && who->player != NULL)
		{
			def.playernum = who->Level->PlayerNum(who->player);
		}
		else
		{
			def.playernum = -1;
		}
		DPrintf (DMSG_SPAMMY, "%s on map %s deferred\n", ScriptPresentation(script).GetChars(), i->MapName.GetChars());
	}
}

EXTERN_CVAR (Bool, sv_cheats)

int P_StartScript (FLevelLocals *Level, AActor *who, line_t *where, int script, const char *map, const int *args, int argcount, int flags)
{
	if (map == NULL || 0 == strnicmp (Level->MapName, map, 8))
	{
		FBehavior *module = NULL;
		const ScriptPtr *scriptdata;

		if ((scriptdata = Level->Behaviors.FindScript (script, module)) != NULL)
		{
			if ((flags & ACS_NET) && netgame && !sv_cheats)
			{
				// If playing multiplayer and cheats are disallowed, check to
				// make sure only net scripts are run.
				if (!(scriptdata->Flags & SCRIPTF_Net))
				{
					Printf(PRINT_DEFAULT, PRINTF_BOLD, "%s tried to puke %s (\n",
						who->player->userinfo.GetName(), ScriptPresentation(script).GetChars());
					for (int i = 0; i < argcount; ++i)
					{
						Printf(PRINT_DEFAULT, PRINTF_BOLD, "%d%s", args[i], i == argcount-1 ? "" : ", ");
					}
					Printf(PRINT_DEFAULT, PRINTF_BOLD, ")\n");
					return false;
				}
			}
			DLevelScript *runningScript = P_GetScriptGoing (Level, who, where, script,
				scriptdata, module, args, argcount, flags);
			if (runningScript != NULL)
			{
				if (flags & ACS_WANTRESULT)
				{
					return runningScript->RunScript();
				}
				return true;
			}
			return false;
		}
		else
		{
			if (!(flags & ACS_NET) || (who && Level->isConsolePlayer(who->player->mo))) // The indirection is necessary here.
			{
				Printf("P_StartScript: Unknown %s\n", ScriptPresentation(script).GetChars());
			}
		}
	}
	else
	{
		addDefered (FindLevelInfo (map),
					(flags & ACS_ALWAYS) ? acsdefered_t::defexealways : acsdefered_t::defexecute,
					script, args, argcount, who);
		return true;
	}
	return false;
}

void P_SuspendScript (FLevelLocals *Level, int script, const char *map)
{
	if (strnicmp (Level->MapName, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defsuspend, script, NULL, 0, NULL);
	else
		SetScriptState (Level->ACSThinker, script, DLevelScript::SCRIPT_Suspended);
}

void P_TerminateScript (FLevelLocals *Level, int script, const char *map)
{
	if (strnicmp (Level->MapName, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defterminate, script, NULL, 0, NULL);
	else
		SetScriptState (Level->ACSThinker, script, DLevelScript::SCRIPT_PleaseRemove);
}

FSerializer &Serialize(FSerializer &arc, const char *key, acsdefered_t &defer, acsdefered_t *def)
{
	if (arc.BeginObject(key))
	{
		arc.Enum("type", defer.type)
			.ScriptNum("script", defer.script)
			.Array("args", defer.args, 3)
			("player", defer.playernum)
			.EndObject();
	}
	return arc;
}

CCMD (scriptstat)
{
	for (auto Level : AllLevels())
	{
		Printf("Script status for %s\n", Level->MapName.GetChars());
		if (Level->ACSThinker == nullptr)
		{
			Printf("No scripts are running.\n");
		}
		else
		{
			Level->ACSThinker->DumpScriptStatus();
		}
	}
}

void DACSThinker::DumpScriptStatus ()
{
	static const char *stateNames[] =
	{
		"Running",
		"Suspended",
		"Delayed",
		"TagWait",
		"PolyWait",
		"ScriptWaitPre",
		"ScriptWait",
		"PleaseRemove"
	};
	DLevelScript *script = Scripts;

	while (script != NULL)
	{
		Printf("%s: %s\n", ScriptPresentation(script->script).GetChars(), stateNames[script->state]);
		script = script->next;
	}
}

// Profiling support --------------------------------------------------------

ACSProfileInfo::ACSProfileInfo()
{
	Reset();
}

void ACSProfileInfo::Reset()
{
	TotalInstr = 0;
	NumRuns = 0;
	MinInstrPerRun = UINT_MAX;
	MaxInstrPerRun = 0;
}

void ACSProfileInfo::AddRun(unsigned int num_instr)
{
	TotalInstr += num_instr;
	NumRuns++;
	if (num_instr < MinInstrPerRun)
	{
		MinInstrPerRun = num_instr;
	}
	if (num_instr > MaxInstrPerRun)
	{
		MaxInstrPerRun = num_instr;
	}
}

void FBehaviorContainer::ArrangeScriptProfiles(TArray<ProfileCollector> &profiles)
{
	for (unsigned int mod_num = 0; mod_num < StaticModules.Size(); ++mod_num)
	{
		FBehavior *module = StaticModules[mod_num];
		ProfileCollector prof;
		prof.Module = module;
		for (int i = 0; i < module->NumScripts; ++i)
		{
			prof.Index = i;
			prof.ProfileData = &module->Scripts[i].ProfileData;
			profiles.Push(prof);
		}
	}
}

void FBehaviorContainer::ArrangeFunctionProfiles(TArray<ProfileCollector> &profiles)
{
	for (unsigned int mod_num = 0; mod_num < StaticModules.Size(); ++mod_num)
	{
		FBehavior *module = StaticModules[mod_num];
		ProfileCollector prof;
		prof.Module = module;
		for (int i = 0; i < module->NumFunctions; ++i)
		{
			ScriptFunction *func = (ScriptFunction *)module->Functions + i;
			if (func->ImportNum == 0)
			{
				prof.Index = i;
				prof.ProfileData = module->FunctionProfileData + i;
				profiles.Push(prof);
			}
		}
	}
}

void ClearProfiles(TArray<ProfileCollector> &profiles)
{
	for (unsigned int i = 0; i < profiles.Size(); ++i)
	{
		profiles[i].ProfileData->Reset();
	}
}

static int sort_by_total_instr(const void *a_, const void *b_)
{
	const ProfileCollector *a = (const ProfileCollector *)a_;
	const ProfileCollector *b = (const ProfileCollector *)b_;

	assert(a != NULL && a->ProfileData != NULL);
	assert(b != NULL && b->ProfileData != NULL);
	return (int)(b->ProfileData->TotalInstr - a->ProfileData->TotalInstr);
}

static int sort_by_min(const void *a_, const void *b_)
{
	const ProfileCollector *a = (const ProfileCollector *)a_;
	const ProfileCollector *b = (const ProfileCollector *)b_;

	return b->ProfileData->MinInstrPerRun - a->ProfileData->MinInstrPerRun;
}

static int sort_by_max(const void *a_, const void *b_)
{
	const ProfileCollector *a = (const ProfileCollector *)a_;
	const ProfileCollector *b = (const ProfileCollector *)b_;

	return b->ProfileData->MaxInstrPerRun - a->ProfileData->MaxInstrPerRun;
}

static int sort_by_avg(const void *a_, const void *b_)
{
	const ProfileCollector *a = (const ProfileCollector *)a_;
	const ProfileCollector *b = (const ProfileCollector *)b_;

	int a_avg = a->ProfileData->NumRuns == 0 ? 0 : int(a->ProfileData->TotalInstr / a->ProfileData->NumRuns);
	int b_avg = b->ProfileData->NumRuns == 0 ? 0 : int(b->ProfileData->TotalInstr / b->ProfileData->NumRuns);
	return b_avg - a_avg;
}

static int sort_by_runs(const void *a_, const void *b_)
{
	const ProfileCollector *a = (const ProfileCollector *)a_;
	const ProfileCollector *b = (const ProfileCollector *)b_;

	return b->ProfileData->NumRuns - a->ProfileData->NumRuns;
}

static void ShowProfileData(TArray<ProfileCollector> &profiles, long ilimit,
	int (*sorter)(const void *, const void *), bool functions)
{
	static const char *const typelabels[2] = { "script", "function" };

	if (profiles.Size() == 0)
	{
		return;
	}

	unsigned int limit;
	char modname[13];
	char scriptname[21];

	qsort(&profiles[0], profiles.Size(), sizeof(ProfileCollector), sorter);

	if (ilimit > 0)
	{
		Printf(TEXTCOLOR_ORANGE "Top %ld %ss:\n", ilimit, typelabels[functions]);
		limit = (unsigned int)ilimit;
	}
	else
	{
		Printf(TEXTCOLOR_ORANGE "All %ss:\n", typelabels[functions]);
		limit = UINT_MAX;
	}

	Printf(TEXTCOLOR_YELLOW "Module       %-20s      Total    Runs     Avg     Min     Max\n", typelabels[functions]);
	Printf(TEXTCOLOR_YELLOW "------------ -------------------- ---------- ------- ------- ------- -------\n");
	for (unsigned int i = 0; i < limit && i < profiles.Size(); ++i)
	{
		ProfileCollector *prof = &profiles[i];
		if (prof->ProfileData->NumRuns == 0)
		{ // Don't list ones that haven't run.
			continue;
		}

		// Module name
		mysnprintf(modname, sizeof(modname), "%s", prof->Module->GetModuleName());

		// Script/function name
		if (functions)
		{
			uint32_t *fnames = (uint32_t *)prof->Module->FindChunk(MAKE_ID('F','N','A','M'));
			if (prof->Index >= 0 && prof->Index < (int)LittleLong(fnames[2]))
			{
				mysnprintf(scriptname, sizeof(scriptname), "%s",
					(char *)(fnames + 2) + LittleLong(fnames[3+prof->Index]));
			}
			else
			{
				mysnprintf(scriptname, sizeof(scriptname), "Function %d", prof->Index);
			}
		}
		else
		{
			mysnprintf(scriptname, sizeof(scriptname), "%s",
				ScriptPresentation(prof->Module->GetScriptPtr(prof->Index)->Number).GetChars() + 7);
		}
		Printf("%-12s %-20s%11llu%8u%8u%8u%8u\n",
			modname, scriptname,
			prof->ProfileData->TotalInstr,
			prof->ProfileData->NumRuns,
			unsigned(prof->ProfileData->TotalInstr / prof->ProfileData->NumRuns),
			prof->ProfileData->MinInstrPerRun,
			prof->ProfileData->MaxInstrPerRun
			);
	}
}

void ACSProfile(FLevelLocals *Level, FCommandLine &argv)
{
	static int (*sort_funcs[])(const void*, const void *) =
	{
		sort_by_total_instr,
		sort_by_min,
		sort_by_max,
		sort_by_avg,
		sort_by_runs
	};
	static const char *sort_names[] = { "total", "min", "max", "avg", "runs" };
	static const uint8_t sort_match_len[] = {   1,     2,     2,     1,      1 };

		TArray<ProfileCollector> ScriptProfiles, FuncProfiles;
		long limit = 10;
	int (*sorter)(const void *, const void *) = sort_by_total_instr;

	assert(countof(sort_names) == countof(sort_match_len));

	Printf("ACS profile for %s\n", Level->MapName.GetChars());
	Level->Behaviors.ArrangeScriptProfiles(ScriptProfiles);
	Level->Behaviors.ArrangeFunctionProfiles(FuncProfiles);

	if (argv.argc() > 1)
	{
		// `acsprofile clear` will zero all profiling information collected so far.
		if (stricmp(argv[1], "clear") == 0)
		{
			ClearProfiles(ScriptProfiles);
			ClearProfiles(FuncProfiles);
			return;
		}
		for (int i = 1; i < argv.argc(); ++i)
		{
			// If it's a number, set the display limit.
			char *endptr;
			long num = strtol(argv[i], &endptr, 0);
			if (endptr != argv[i])
			{
				limit = num;
				continue;
			}
			// If it's a name, set the sort method. We accept partial matches for
			// options that are shorter than the sort name.
			size_t optlen = strlen(argv[i]);
			unsigned int j;
			for (j = 0; j < countof(sort_names); ++j)
			{
				if (optlen < sort_match_len[j] || optlen > strlen(sort_names[j]))
				{ // Too short or long to match.
					continue;
				}
				if (strnicmp(argv[i], sort_names[j], optlen) == 0)
				{
					sorter = sort_funcs[j];
					break;
				}
			}
			if (j == countof(sort_names))
			{
				Printf("Unknown option '%s'\n", argv[i]);
				Printf("acsprofile clear : Reset profiling information\n");
				Printf("acsprofile [total|min|max|avg|runs] [<limit>]\n");
				return;
			}
		}
	}

	ShowProfileData(ScriptProfiles, limit, sorter, false);
	ShowProfileData(FuncProfiles, limit, sorter, true);
}

CCMD(acsprofile)
{
	for (auto Level : AllLevels())
	{
		ACSProfile(Level, argv);
	}
}

ADD_STAT(ACS)
{
	return FStringf("ACS time: %f ms", ACSTime.TimeMS());
}

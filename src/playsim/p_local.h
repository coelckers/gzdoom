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
//		Play functions, animation, global header.
//
//-----------------------------------------------------------------------------


#ifndef __P_LOCAL__
#define __P_LOCAL__

#include <float.h>
#include "doomtype.h"
#include "vectors.h"
#include "dobject.h"

const double NO_VALUE = FLT_MAX;

class player_t;
class AActor;
struct FPlayerStart;
class PClassActor;
struct line_t;
struct sector_t;
struct msecnode_t;
struct portnode_t;
struct secplane_t;
struct FCheckPosition;
struct FTranslatedLineTarget;
struct FLinePortal;

#include <stdlib.h>

#define STEEPSLOPE		(46342/65536.)	// [RH] Minimum floorplane.c value for walking

#define MAXMOVE 		(30.)

#define TALKRANGE		(128.)
#define USERANGE		(64.)

#define DEFMELEERANGE		(64.)
#define SAWRANGE		(64.+(1./65536.))	// use meleerange + 1 so the puff doesn't skip the flash (i.e. plays all states)
#define MISSILERANGE	(32*64.)
#define PLAYERMISSILERANGE	(8192.)	// [RH] New MISSILERANGE for players

// follow a player exlusively for 3 seconds
// No longer used.
// #define BASETHRESHOLD	100


//
// P_PSPR
//
void P_SetupPsprites (player_t* curplayer, bool startweaponup);


//
// P_USER
//
void	P_FallingDamage (AActor *ent);
void	P_PlayerThink (player_t *player);
void	P_PredictPlayer (player_t *player);
void	P_UnPredictPlayer ();
void	P_PredictionLerpReset();

//
// P_MOBJ
//

#define SPF_TEMPPLAYER		1	// spawning a short-lived dummy player
#define SPF_WEAPONFULLYUP	2	// spawn with weapon already raised

int P_FaceMobj (AActor *source, AActor *target, DAngle *delta);
bool P_SeekerMissile (AActor *actor, double thresh, double turnMax, bool precise = false, bool usecurspeed=false);

enum EPuffFlags
{
	PF_HITTHING = 1,
	PF_MELEERANGE = 2,
	PF_TEMPORARY = 4,
	PF_HITTHINGBLEED = 8,
	PF_NORANDOMZ = 16,
	PF_HITSKY = 32
};

AActor *P_SpawnPuff(AActor *source, PClassActor *pufftype, const DVector3 &pos, DAngle hitdir, DAngle particledir, int updown, int flags = 0, AActor *vict = NULL);
void	P_SpawnBlood (const DVector3 &pos, DAngle angle, int damage, AActor *originator);
void	P_BloodSplatter (const DVector3 &pos, AActor *originator, DAngle hitangle);
void	P_BloodSplatter2 (const DVector3 &pos, AActor *originator, DAngle hitangle);
void	P_RipperBlood (AActor *mo, AActor *bleeder);
int		P_GetThingFloorType (AActor *thing);
void	P_ExplodeMissile (AActor *missile, line_t *explodeline, AActor *target, bool onsky = false, FName damageType = NAME_None);

AActor *P_OldSpawnMissile(AActor *source, AActor *owner, AActor *dest, PClassActor *type);
AActor *P_SpawnMissile (AActor* source, AActor* dest, PClassActor *type, AActor* owner = NULL);
AActor *P_SpawnMissileZ(AActor* source, double z, AActor* dest, PClassActor *type);
AActor *P_SpawnMissileXYZ(DVector3 pos, AActor *source, AActor *dest, PClassActor *type, bool checkspawn = true, AActor *owner = NULL);
AActor *P_SpawnMissileAngleZSpeed(AActor *source, double z, PClassActor *type, DAngle angle, double vz, double speed, AActor *owner = NULL, bool checkspawn = true);
AActor *P_SpawnMissileZAimed(AActor *source, double z, AActor *dest, PClassActor *type);


AActor *P_SpawnPlayerMissile (AActor *source, double x, double y, double z, PClassActor *type, DAngle angle, 
							  FTranslatedLineTarget *pLineTarget = NULL, AActor **MissileActor = NULL, bool nofreeaim = false, bool noautoaim = false, int aimflags = 0);


void P_CheckFakeFloorTriggers(AActor *mo, double oldz, bool oldz_has_viewheight = false);

AActor *P_SpawnSubMissile (AActor *source, PClassActor *type, AActor *target);	// Strife uses it


//
// [RH] P_THINGS
//
extern FClassMap SpawnableThings;
extern FClassMap StrifeTypes;

bool	P_MoveThing(AActor *source, const DVector3 &pos, bool fog);
void	P_Thing_SetVelocity(AActor *actor, const DVector3 &vec, bool add, bool setbob);
void P_RemoveThing(AActor * actor);
bool P_Thing_Raise(AActor *thing, AActor *raiser, int flags = 0);
bool P_Thing_CanRaise(AActor *thing);
bool P_CanResurrect(AActor *ththing, AActor *thing);
PClassActor *P_GetSpawnableType(int spawnnum);
void InitSpawnablesFromMapinfo();
int P_Thing_CheckInputNum(player_t *p, int inputnum);
int P_Thing_Warp(AActor *caller, AActor *reference, double xofs, double yofs, double zofs, DAngle angle, int flags, double heightoffset, double radiusoffset, DAngle pitch);
struct FLevelLocals;
int P_Thing_CheckProximity(FLevelLocals *Level, AActor *self, PClass *classname, double distance, int count, int flags, int ptr, bool counting = false);

enum
{
	// These are the original inputs sent by the player.
	INPUT_OLDBUTTONS,
	INPUT_BUTTONS,
	INPUT_PITCH,
	INPUT_YAW,
	INPUT_ROLL,
	INPUT_FORWARDMOVE,
	INPUT_SIDEMOVE,
	INPUT_UPMOVE,

	// These are the inputs, as modified by P_PlayerThink().
	// Most of the time, these will match the original inputs, but
	// they can be different if a player is frozen or using a
	// chainsaw.
	MODINPUT_OLDBUTTONS,
	MODINPUT_BUTTONS,
	MODINPUT_PITCH,
	MODINPUT_YAW,
	MODINPUT_ROLL,
	MODINPUT_FORWARDMOVE,
	MODINPUT_SIDEMOVE,
	MODINPUT_UPMOVE
};
enum CPXF
{
	CPXF_ANCESTOR = 1 << 0,
	CPXF_LESSOREQUAL = 1 << 1,
	CPXF_NOZ = 1 << 2,
	CPXF_COUNTDEAD = 1 << 3,
	CPXF_DEADONLY = 1 << 4,
	CPXF_EXACT = 1 << 5,
	CPXF_SETTARGET = 1 << 6,
	CPXF_SETMASTER = 1 << 7,
	CPXF_SETTRACER = 1 << 8,
	CPXF_FARTHEST = 1 << 9,
	CPXF_CLOSEST = 1 << 10,
	CPXF_SETONPTR = 1 << 11,
	CPXF_CHECKSIGHT = 1 << 12,
};

enum WARPF
{
	WARPF_ABSOLUTEOFFSET = 0x1,
	WARPF_ABSOLUTEANGLE  = 0x2,
	WARPF_USECALLERANGLE = 0x4,

	WARPF_NOCHECKPOSITION = 0x8,

	WARPF_INTERPOLATE       = 0x10,
	WARPF_WARPINTERPOLATION = 0x20,
	WARPF_COPYINTERPOLATION = 0x40,

	WARPF_STOP             = 0x80,
	WARPF_TOFLOOR          = 0x100,
	WARPF_TESTONLY         = 0x200,
	WARPF_ABSOLUTEPOSITION = 0x400,
	WARPF_BOB              = 0x800,
	WARPF_MOVEPTR          = 0x1000,
	WARPF_USEPTR           = 0x2000,
	WARPF_USETID           = 0x2000,
	WARPF_COPYVELOCITY		= 0x4000,
	WARPF_COPYPITCH			= 0x8000,
};

enum SPF
{
	SPF_FORCECLAMP = 1,	// players always clamp
	SPF_INTERPOLATE = 2,
};

enum PCM
{
	PCM_DROPOFF =		1,
	PCM_NOACTORS =		1 << 1,
	PCM_NOLINES =		1 << 2,
};


AActor *P_BlockmapSearch (AActor *mo, int distance, AActor *(*check)(AActor*, int, void *), void *params = NULL);
AActor *P_RoughMonsterSearch (AActor *mo, int distance, bool onlyseekable=false, bool frontonly = false);

//
// P_MAP
//

struct spechit_t
{
	line_t *line;
	DVector2 Oldrefpos;
	DVector2 Refpos;
};

extern TArray<spechit_t> spechit;
extern TArray<spechit_t> portalhit;


int	P_TestMobjLocation (AActor *mobj);
int	P_TestMobjZ (AActor *mobj, bool quick=true, AActor **pOnmobj = NULL);
bool P_CheckPosition(AActor *thing, const DVector2 &pos, bool actorsonly = false);
void P_DoMissileDamage(AActor* inflictor, AActor* target);
bool P_CheckPosition(AActor *thing, const DVector2 &pos, FCheckPosition &tm, bool actorsonly = false);
AActor	*P_CheckOnmobj (AActor *thing);
void	P_FakeZMovement (AActor *mo);
bool	P_TryMove(AActor* thing, const DVector2 &pos, int dropoff, const secplane_t * onfloor, FCheckPosition &tm, bool missileCheck = false);
bool	P_TryMove(AActor* thing, const DVector2 &pos, int dropoff, const secplane_t * onfloor = NULL, bool missilecheck = false);

bool P_CheckMove(AActor *thing, const DVector2 &pos, FCheckPosition& tm, int flags);
bool	P_CheckMove(AActor *thing, const DVector2 &pos, int flags = 0);
void	P_ApplyTorque(AActor *mo);

bool	P_TeleportMove(AActor* thing, const DVector3 &pos, bool telefrag, bool modifyactor = true);	// [RH] Added z and telefrag parameters

void	P_PlayerStartStomp (AActor *actor, bool mononly=false);		// [RH] Stomp on things for a newly spawned player
void	P_SlideMove (AActor* mo, const DVector2 &pos, int numsteps);
bool	P_BounceWall (AActor *mo);
bool	P_BounceActor (AActor *mo, AActor *BlockingMobj, bool ontop);
int	P_CheckSight (AActor *t1, AActor *t2, int flags=0);

enum ESightFlags
{
	SF_IGNOREVISIBILITY=1,
	SF_SEEPASTSHOOTABLELINES=2,
	SF_SEEPASTBLOCKEVERYTHING=4,
	SF_IGNOREWATERBOUNDARY=8
};

void	P_ResetSightCounters (bool full);
bool	P_TalkFacing (AActor *player);
void	P_UseLines (player_t* player);
int	P_UsePuzzleItem (AActor *actor, int itemType);

enum
{
	FFCF_ONLYSPAWNPOS = 1,
	FFCF_SAMESECTOR = 2,
	FFCF_ONLY3DFLOORS = 4,	// includes 3D midtexes
	FFCF_3DRESTRICT = 8,	// ignore 3D midtexes and floors whose floorz are above thing's z
	FFCF_NOPORTALS = 16,	// ignore portals (considers them impassable.)
	FFCF_NOFLOOR = 32,
	FFCF_NOCEILING = 64,
	FFCF_RESTRICTEDPORTAL = 128,	// current values in the iterator's return are through a restricted portal type (i.e. some features are blocked.)
	FFCF_NODROPOFF = 256,			// Caller does not need a dropoff (saves some time when checking portals)
};
void	P_FindFloorCeiling (AActor *actor, int flags=0);

bool	P_ChangeSector (sector_t* sector, int crunch, double amt, int floorOrCeil, bool isreset, bool instant = false);

DAngle P_AimLineAttack(AActor *t1, DAngle angle, double distance, FTranslatedLineTarget *pLineTarget = NULL, DAngle vrange = 0., int flags = 0, AActor *target = NULL, AActor *friender = NULL);

enum	// P_AimLineAttack flags
{
	ALF_FORCENOSMART = 1,
	ALF_CHECK3D = 2,
	ALF_CHECKNONSHOOTABLE = 4,
	ALF_CHECKCONVERSATION = 8,
	ALF_NOFRIENDS = 16,
	ALF_PORTALRESTRICT = 32,	// only work through portals with a global offset (to be used for stuff that cannot remember the calculated FTranslatedLineTarget info)
	ALF_NOWEAPONCHECK = 64,		// ignore NOAUTOAIM flag on a player's weapon.
};

enum	// P_LineAttack flags
{
	LAF_ISMELEEATTACK = 1,
	LAF_NORANDOMPUFFZ = 1 << 1,
	LAF_NOIMPACTDECAL = 1 << 2,
	LAF_NOINTERACT =    1 << 3,
	LAF_TARGETISSOURCE= 1 << 4,
	LAF_OVERRIDEZ =     1 << 5,
	LAF_ABSOFFSET =     1 << 6,
	LAF_ABSPOSITION =   1 << 7,
};

AActor *P_LineAttack(AActor *t1, DAngle angle, double distance, DAngle pitch, int damage, FName damageType, PClassActor *pufftype, int flags = 0, FTranslatedLineTarget *victim = NULL, int *actualdamage = NULL, double sz = 0.0, double offsetforward = 0.0, double offsetside = 0.0);
AActor *P_LineAttack(AActor *t1, DAngle angle, double distance, DAngle pitch, int damage, FName damageType, FName pufftype, int flags = 0, FTranslatedLineTarget *victim = NULL, int *actualdamage = NULL, double sz = 0.0, double offsetforward = 0.0, double offsetside = 0.0);

enum	// P_LineTrace flags
{
	TRF_ABSPOSITION = 1,
	TRF_ABSOFFSET = 2,
	TRF_THRUSPECIES = 4,
	TRF_THRUACTORS = 8,
	TRF_THRUBLOCK = 16,
	TRF_THRUHITSCAN = 32,
	TRF_NOSKY = 64,
	TRF_ALLACTORS = 128,
	TRF_SOLIDACTORS = 256,
	TRF_BLOCKUSE = 512,
	TRF_BLOCKSELF = 1024,
};

void	P_TraceBleed(int damage, const DVector3 &pos, AActor *target, DAngle angle, DAngle pitch);
void	P_TraceBleed(int damage, AActor *target, DAngle angle, DAngle pitch);

void	P_TraceBleed (int damage, AActor *target, AActor *missile);		// missile version
void	P_TraceBleed(int damage, FTranslatedLineTarget *t, AActor *puff);		// hitscan version
void	P_TraceBleed (int damage, AActor *target);		// random direction version
bool	P_HitFloor (AActor *thing);
bool	P_HitWater (AActor *thing, sector_t *sec, const DVector3 &pos, bool checkabove = false, bool alert = true, bool force = false);


struct FRailParams
{
	AActor *source = nullptr;
	int damage = 0;
	double offset_xy = 0;
	double offset_z = 0;
	int color1 = 0, color2 = 0;
	double maxdiff = 0;
	int flags = 0;
	PClassActor *puff = nullptr;
	DAngle angleoffset = 0.;
	DAngle pitchoffset = 0.;
	double distance = 8192;
	int duration = 0;
	double sparsity = 1.0;
	double drift = 1.0;
	PClassActor *spawnclass = nullptr;
	int SpiralOffset = 270;
	int limit = 0;
};	// [RH] Shoot a railgun

void P_RailAttack(FRailParams *params);

enum	// P_RailAttack / A_RailAttack / A_CustomRailgun / P_DrawRailTrail flags
{	
	RAF_SILENT = 1,
	RAF_NOPIERCE = 2,
	RAF_EXPLICITANGLE = 4,
	RAF_FULLBRIGHT = 8,
	RAF_CENTERZ = 16,
	RAF_NORANDOMPUFFZ = 32,
};


bool	P_CheckMissileSpawn(AActor *missile, double maxdist);

void	P_PlaySpawnSound(AActor *missile, AActor *spawner);

// [RH] Position the chasecam
void	P_AimCamera (AActor *t1, DVector3 &, DAngle &, sector_t *&sec, bool &unlinked);

// [RH] Means of death
enum
{
	RADF_HURTSOURCE = 1,
	RADF_NOIMPACTDAMAGE = 2,
	RADF_SOURCEISSPOT = 4,
	RADF_NODAMAGE = 8,
	RADF_THRUSTZ = 16,
	RADF_OLDRADIUSDAMAGE = 32
};
int P_GetRadiusDamage(AActor *self, AActor *thing, int damage, int distance, int fulldmgdistance, bool oldradiusdmg);
int	P_RadiusAttack (AActor *spot, AActor *source, int damage, int distance, 
						FName damageType, int flags, int fulldamagedistance=0, FName species = NAME_None);

void	P_DelSeclist(msecnode_t *, msecnode_t *sector_t::*seclisthead);
void	P_DelSeclist(portnode_t *, portnode_t *FLinePortal::*seclisthead);

template<class nodetype, class linktype>
nodetype *P_AddSecnode(linktype *s, AActor *thing, nodetype *nextnode, nodetype *&sec_thinglist);

template<class nodetype, class linktype>
nodetype* P_DelSecnode(nodetype *, nodetype *linktype::*head);

msecnode_t *P_CreateSecNodeList(AActor *thing, double radius, msecnode_t *sector_list, msecnode_t *sector_t::*seclisthead);
double	P_GetMoveFactor(const AActor *mo, double *frictionp);	// phares  3/6/98
double		P_GetFriction(const AActor *mo, double *frictionfactor);

// [RH] 
const secplane_t * P_CheckSlopeWalk(AActor *actor, DVector2 &move);

//
// P_INTER
//
void P_TouchSpecialThing (AActor *special, AActor *toucher);
int  P_DamageMobj (AActor *target, AActor *inflictor, AActor *source, int damage, FName mod, int flags=0, DAngle angle = 0.);
void P_PoisonMobj (AActor *target, AActor *inflictor, AActor *source, int damage, int duration, int period, FName type);
bool P_GiveBody (AActor *actor, int num, int max=0);
bool P_PoisonPlayer (player_t *player, AActor *poisoner, AActor *source, int poison);
void P_PoisonDamage (player_t *player, AActor *source, int damage, bool playPainSound);

enum EDmgFlags
{
	DMG_NO_ARMOR = 1,
	DMG_INFLICTOR_IS_PUFF = 2,
	DMG_THRUSTLESS = 4,
	DMG_FORCED = 8,
	DMG_NO_FACTOR = 16,
	DMG_PLAYERATTACK = 32,
	DMG_FOILINVUL = 64,
	DMG_FOILBUDDHA = 128,
	DMG_NO_PROTECT = 256,
	DMG_USEANGLE = 512,
	DMG_NO_PAIN = 1024,
	DMG_EXPLOSION = 2048,
	DMG_NO_ENHANCE = 4096,
	DMG_NO_CVAR = 8192,
	DMG_NO_SKILL = 16384,
	DMG_SECTOR = 32768,
};


//
// P_SPEC
//
bool P_AlignFlat (int linenum, int side, int fc);

enum ETexReplaceFlags
{
	NOT_BOTTOM			= 1,
	NOT_MIDDLE			= 2,
	NOT_TOP				= 4,
	NOT_FLOOR			= 8,
	NOT_CEILING			= 16
};

void P_ReplaceTextures(const char *fromname, const char *toname, int flags);

enum ERaise
{
	RF_TRANSFERFRIENDLINESS = 1,
	RF_NOCHECKPOSITION = 2
};

#endif	// __P_LOCAL__

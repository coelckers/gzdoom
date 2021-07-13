//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
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
//		Map Objects, MObj, definition and handling.
//
//-----------------------------------------------------------------------------


#ifndef __P_MOBJ_H__
#define __P_MOBJ_H__

// Basics.
#include "templates.h"

// We need the thinker_t stuff.
#include "dthinker.h"


// States are tied to finite states are tied to animation frames.
#include "info.h"

#include "doomdef.h"
#include "textures.h"
#include "renderstyle.h"
#include "s_sound.h"
#include "memarena.h"
#include "g_level.h"
#include "tflags.h"
#include "portal.h"

struct subsector_t;
struct FBlockNode;
struct FPortalGroupArray;
struct visstyle_t;
class FLightDefaults;
struct FSection;
struct FLevelLocals;
struct FDynamicLight;

//
// NOTES: AActor
//
// Actors are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which patch_t
// is used to draw the sprite if it is visible.
// The sprite and frame values are almost always set
// from state_t structures.
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for patch_ts grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and sometimes z fields
// to do stereo positioning of any sound emitted by the actor.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when AActors are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The AActor->flags element has various bit flags
// used by the simulation.
//
// Every actor is linked into a single sector
// based on its origin coordinates.
// The subsector_t is found with PointInSector(x,y).
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any actor that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MF_NOBLOCK flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every line_t that it contains a piece of, and every
// interactable actor that has its origin contained.  
//
// A valid actor is an actor that has the proper subsector_t
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MF_NOSECTOR flag set (the subsector_t needs to be valid
// even if MF_NOSECTOR is set), and is linked into a blockmap
// block or has the MF_NOBLOCKMAP flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MF_NO* flags while a thing is valid.
//
// Any questions?
//



// --- mobj.flags ---
enum ActorFlag
{
	MF_SPECIAL			= 0x00000001,	// call P_SpecialThing when touched
	MF_SOLID			= 0x00000002,
	MF_SHOOTABLE		= 0x00000004,
	MF_NOSECTOR			= 0x00000008,	// don't use the sector links
										// (invisible but touchable)
	MF_NOBLOCKMAP		= 0x00000010,	// don't use the blocklinks
										// (inert but displayable)
	MF_AMBUSH			= 0x00000020,	// not activated by sound; deaf monster
	MF_JUSTHIT			= 0x00000040,	// try to attack right back
	MF_JUSTATTACKED		= 0x00000080,	// take at least one step before attacking
	MF_SPAWNCEILING		= 0x00000100,	// hang from ceiling instead of floor
	MF_NOGRAVITY		= 0x00000200,	// don't apply gravity every tic

// movement flags
	MF_DROPOFF			= 0x00000400,	// allow jumps from high places
	MF_PICKUP			= 0x00000800,	// for players to pick up items
	MF_NOCLIP			= 0x00001000,	// player cheat
	MF_SLIDE			= 0x00002000,	// Not used anymore but needed for MBF21 flag checkers.
	MF_FLOAT			= 0x00004000,	// allow moves to any height, no gravity
	MF_TELEPORT			= 0x00008000,	// don't cross lines or look at heights
	MF_MISSILE			= 0x00010000,	// don't hit same species, explode on block

	MF_DROPPED			= 0x00020000,	// dropped by a demon, not level spawned
	MF_SHADOW			= 0x00040000,	// actor is hard for monsters to see
	MF_NOBLOOD			= 0x00080000,	// don't bleed when shot (use puff)
	MF_CORPSE			= 0x00100000,	// don't stop moving halfway off a step
	MF_INFLOAT			= 0x00200000,	// floating to a height for a move, don't
										// auto float to target's height
	MF_INBOUNCE			= 0x00200000,	// used by Heretic bouncing missiles 

	MF_COUNTKILL		= 0x00400000,	// count towards intermission kill total
	MF_COUNTITEM		= 0x00800000,	// count towards intermission item total

	MF_SKULLFLY			= 0x01000000,	// skull in flight
	MF_NOTDMATCH		= 0x02000000,	// don't spawn in death match (key cards)

	MF_SPAWNSOUNDSOURCE	= 0x04000000,	// Plays missile's see sound at spawning object.
	MF_FRIENDLY			= 0x08000000,	// [RH] Friendly monsters for Strife (and MBF)
	MF_UNMORPHED		= 0x10000000,	// [RH] Actor is the unmorphed version of something else
	MF_NOLIFTDROP		= 0x20000000,	// [RH] Used with MF_NOGRAVITY to avoid dropping with lifts
	MF_STEALTH			= 0x40000000,	// [RH] Andy Baker's stealth monsters
	MF_ICECORPSE		= 0x80000000,	// a frozen corpse (for blasting) [RH] was 0x800000
};

// --- mobj.flags2 ---
enum ActorFlag2
{
	MF2_DONTREFLECT		= 0x00000001,	// this projectile cannot be reflected
	MF2_WINDTHRUST		= 0x00000002,	// gets pushed around by the wind specials
	MF2_DONTSEEKINVISIBLE=0x00000004,	// For seeker missiles: Don't home in on invisible/shadow targets
	MF2_BLASTED			= 0x00000008,	// actor will temporarily take damage from impact
	MF2_FLY				= 0x00000010,	// fly mode is active
	MF2_FLOORCLIP		= 0x00000020,	// if feet are allowed to be clipped
	MF2_SPAWNFLOAT		= 0x00000040,	// spawn random float z
	MF2_NOTELEPORT		= 0x00000080,	// does not teleport
	MF2_RIP				= 0x00000100,	// missile rips through solid targets
	MF2_PUSHABLE		= 0x00000200,	// can be pushed by other moving actors
	MF2_SLIDE			= 0x00000400,	// slides against walls
	MF2_ONMOBJ			= 0x00000800,	// actor is resting on top of another actor
	MF2_PASSMOBJ		= 0x00001000,	// Enable z block checking. If on,
										// this flag will allow the actor to
										// pass over/under other actors.
	MF2_CANNOTPUSH		= 0x00002000,	// cannot push other pushable mobjs
	MF2_THRUGHOST		= 0x00004000,	// missile will pass through ghosts [RH] was 8
	MF2_BOSS			= 0x00008000,	// mobj is a major boss

	MF2_DONTTRANSLATE	= 0x00010000,	// Don't apply palette translations
	MF2_NODMGTHRUST		= 0x00020000,	// does not thrust target when damaging
	MF2_TELESTOMP		= 0x00040000,	// mobj can stomp another
	MF2_FLOATBOB		= 0x00080000,	// use float bobbing z movement
	MF2_THRUACTORS		= 0x00100000,	// performs no actor<->actor collision checks
	MF2_IMPACT			= 0x00200000, 	// an MF_MISSILE mobj can activate SPAC_IMPACT
	MF2_PUSHWALL		= 0x00400000, 	// mobj can push walls
	MF2_MCROSS			= 0x00800000,	// can activate monster cross lines
	MF2_PCROSS			= 0x01000000,	// can activate projectile cross lines
	MF2_CANTLEAVEFLOORPIC=0x02000000,	// stay within a certain floor type
	MF2_NONSHOOTABLE	= 0x04000000,	// mobj is totally non-shootable, 
										// but still considered solid
	MF2_INVULNERABLE	= 0x08000000,	// mobj is invulnerable
	MF2_DORMANT			= 0x10000000,	// thing is dormant
	MF2_ARGSDEFINED		= 0x20000000,	// Internal flag used by DECORATE to signal that the 
										// args should not be taken from the mapthing definition
	MF2_SEEKERMISSILE	= 0x40000000,	// is a seeker (for reflection)
	MF2_REFLECTIVE		= 0x80000000,	// reflects missiles
};

// --- mobj.flags3 ---
enum ActorFlag3
{
	MF3_FLOORHUGGER		= 0x00000001,	// Missile stays on floor
	MF3_CEILINGHUGGER	= 0x00000002,	// Missile stays on ceiling
	MF3_NORADIUSDMG		= 0x00000004,	// Actor does not take radius damage
	MF3_GHOST			= 0x00000008,	// Actor is a ghost
	MF3_ALWAYSPUFF		= 0x00000010,	// Puff always appears, even when hit nothing
	MF3_SPECIALFLOORCLIP= 0x00000020,	// Actor uses floorclip for special effect (e.g. Wraith)
	MF3_DONTSPLASH		= 0x00000040,	// Thing doesn't make a splash
	MF3_NOSIGHTCHECK	= 0x00000080,	// Go after first acceptable target without checking sight
	MF3_DONTOVERLAP		= 0x00000100,	// Don't pass over/under other things with this bit set
	MF3_DONTMORPH		= 0x00000200,	// Immune to arti_egg
	MF3_DONTSQUASH		= 0x00000400,	// Death ball can't squash this actor
	MF3_EXPLOCOUNT		= 0x00000800,	// Don't explode until special2 counts to special1
	MF3_FULLVOLACTIVE	= 0x00001000,	// Active sound is played at full volume
	MF3_ISMONSTER		= 0x00002000,	// Actor is a monster
	MF3_SKYEXPLODE		= 0x00004000,	// Explode missile when hitting sky
	MF3_STAYMORPHED		= 0x00008000,	// Monster cannot unmorph
	MF3_DONTBLAST		= 0x00010000,	// Actor cannot be pushed by blasting
	MF3_CANBLAST		= 0x00020000,	// Actor is not a monster but can be blasted
	MF3_NOTARGET		= 0x00040000,	// This actor not targetted when it hurts something else
	MF3_DONTGIB			= 0x00080000,	// Don't gib this corpse
	MF3_NOBLOCKMONST	= 0x00100000,	// Can cross ML_BLOCKMONSTERS lines
	MF3_CRASHED			= 0x00200000,	// Actor entered its crash state
	MF3_FULLVOLDEATH	= 0x00400000,	// DeathSound is played full volume (for missiles)
	MF3_AVOIDMELEE		= 0x00800000,	// Avoids melee attacks (same as MBF's monster_backing but must be explicitly set)
	MF3_SCREENSEEKER    = 0x01000000,	// Fails the IsOkayToAttack test if potential target is outside player FOV
	MF3_FOILINVUL		= 0x02000000,	// Actor can hurt MF2_INVULNERABLE things
	MF3_NOTELEOTHER		= 0x04000000,	// Monster is unaffected by teleport other artifact
	MF3_BLOODLESSIMPACT	= 0x08000000,	// Projectile does not leave blood
	MF3_NOEXPLODEFLOOR	= 0x10000000,	// Missile stops at floor instead of exploding
	MF3_WARNBOT			= 0x20000000,	// Missile warns bot
	MF3_PUFFONACTORS	= 0x40000000,	// Puff appears even when hit bleeding actors
	MF3_HUNTPLAYERS		= 0x80000000,	// Used with TIDtoHate, means to hate players too
};

// --- mobj.flags4 ---
enum ActorFlag4
{
	MF4_NOHATEPLAYERS	= 0x00000001,	// Ignore player attacks
	MF4_QUICKTORETALIATE= 0x00000002,	// Always switch targets when hurt
	MF4_NOICEDEATH		= 0x00000004,	// Actor never enters an ice death, not even the generic one
	MF4_BOSSDEATH		= 0x00000008,	// A_FreezeDeathChunks calls A_BossDeath
	MF4_RANDOMIZE		= 0x00000010,	// Missile has random initial tic count
	MF4_NOSKIN			= 0x00000020,	// Player cannot use skins
	MF4_FIXMAPTHINGPOS	= 0x00000040,	// Fix this actor's position when spawned as a map thing
	MF4_ACTLIKEBRIDGE	= 0x00000080,	// Pickups can "stand" on this actor / cannot be moved by any sector action.
	MF4_STRIFEDAMAGE	= 0x00000100,	// Strife projectiles only do up to 4x damage, not 8x

	MF4_CANUSEWALLS		= 0x00000200,	// Can activate 'use' specials
	MF4_MISSILEMORE		= 0x00000400,	// increases the chance of a missile attack
	MF4_MISSILEEVENMORE	= 0x00000800,	// significantly increases the chance of a missile attack
	MF4_FORCERADIUSDMG	= 0x00001000,	// if put on an object it will override MF3_NORADIUSDMG
	MF4_DONTFALL		= 0x00002000,	// Doesn't have NOGRAVITY disabled when dying.
	MF4_SEESDAGGERS		= 0x00004000,	// This actor can see you striking with a dagger
	MF4_INCOMBAT		= 0x00008000,	// Don't alert others when attacked by a dagger
	MF4_LOOKALLAROUND	= 0x00010000,	// Monster has eyes in the back of its head
	MF4_STANDSTILL		= 0x00020000,	// Monster should not chase targets unless attacked?
	MF4_SPECTRAL		= 0x00040000,
	MF4_SCROLLMOVE		= 0x00080000,	// velocity has been applied by a scroller
	MF4_NOSPLASHALERT	= 0x00100000,	// Splashes don't alert this monster
	MF4_SYNCHRONIZED	= 0x00200000,	// For actors spawned at load-time only: Do not randomize tics
	MF4_NOTARGETSWITCH	= 0x00400000,	// monster never switches target until current one is dead
	MF4_VFRICTION		= 0x00800000,	// Internal flag used by A_PainAttack to push a monster down
	MF4_DONTHARMCLASS	= 0x01000000,	// Don't hurt one's own kind with explosions (hitscans, too?)
	MF4_SHIELDREFLECT	= 0x02000000,
	MF4_DEFLECT			= 0x04000000,	// different projectile reflection styles
	MF4_ALLOWPARTICLES	= 0x08000000,	// this puff type can be replaced by particles
	MF4_NOEXTREMEDEATH	= 0x10000000,	// this projectile or weapon never gibs its victim
	MF4_EXTREMEDEATH	= 0x20000000,	// this projectile or weapon always gibs its victim
	MF4_FRIGHTENED		= 0x40000000,	// Monster runs away from player
	MF4_BOSSSPAWNED		= 0x80000000,	// Spawned by a boss spawn cube
};

// --- mobj.flags5 ---

enum ActorFlag5
{
	MF5_DONTDRAIN		= 0x00000001,	// cannot be drained health from.
	MF5_GETOWNER		= 0x00000002,
	MF5_NODROPOFF		= 0x00000004,	// cannot drop off under any circumstances.
	MF5_NOFORWARDFALL	= 0x00000008,	// Does not make any actor fall forward by being damaged by this
	MF5_COUNTSECRET		= 0x00000010,	// From Doom 64: actor acts like a secret
	MF5_AVOIDINGDROPOFF = 0x00000020,	// Used to move monsters away from dropoffs
	MF5_NODAMAGE		= 0x00000040,	// Actor can be shot and reacts to being shot but takes no damage
	MF5_CHASEGOAL		= 0x00000080,	// Walks to goal instead of target if a valid goal is set.
	MF5_BLOODSPLATTER	= 0x00000100,	// Blood splatter like in Raven's games.
	MF5_OLDRADIUSDMG	= 0x00000200,	// Use old radius damage code (for barrels and boss brain)
	MF5_DEHEXPLOSION	= 0x00000400,	// Use the DEHACKED explosion options when this projectile explodes
	MF5_PIERCEARMOR		= 0x00000800,	// Armor doesn't protect against damage from this actor
	MF5_NOBLOODDECALS	= 0x00001000,	// Actor bleeds but doesn't spawn blood decals
	MF5_USESPECIAL		= 0x00002000,	// Actor executes its special when being 'used'.
	MF5_NOPAIN			= 0x00004000,	// If set the pain state won't be entered
	MF5_ALWAYSFAST		= 0x00008000,	// always uses 'fast' attacking logic
	MF5_NEVERFAST		= 0x00010000,	// never uses 'fast' attacking logic
	MF5_ALWAYSRESPAWN	= 0x00020000,	// always respawns, regardless of skill setting
	MF5_NEVERRESPAWN	= 0x00040000,	// never respawns, regardless of skill setting
	MF5_DONTRIP			= 0x00080000,	// Ripping projectiles explode when hitting this actor
	MF5_NOINFIGHTING	= 0x00100000,	// This actor doesn't switch target when it's hurt 
	MF5_NOINTERACTION	= 0x00200000,	// Thing is completely excluded from any gameplay related checks
	MF5_NOTIMEFREEZE	= 0x00400000,	// Actor is not affected by time freezer
	MF5_PUFFGETSOWNER	= 0x00800000,	// [BB] Sets the owner of the puff to the player who fired it
	MF5_SPECIALFIREDAMAGE=0x01000000,	// Special treatment of PhoenixFX1 turned into a flag to remove
										// dependence of main engine code of specific actor types.
	MF5_SUMMONEDMONSTER	= 0x02000000,	// To mark the friendly Minotaur. Hopefully to be generalized later.
	MF5_NOVERTICALMELEERANGE=0x04000000,// Does not check vertical distance for melee range
	MF5_BRIGHT			= 0x08000000,	// Actor is always rendered fullbright
	MF5_CANTSEEK		= 0x10000000,	// seeker missiles cannot home in on this actor
	MF5_INCONVERSATION	= 0x20000000,	// Actor is having a conversation
	MF5_PAINLESS		= 0x40000000,	// Actor always inflicts painless damage.
	MF5_MOVEWITHSECTOR	= 0x80000000,	// P_ChangeSector() will still process this actor if it has MF_NOBLOCKMAP
};

// --- mobj.flags6 ---
enum ActorFlag6
{
	MF6_NOBOSSRIP		= 0x00000001,	// For rippermissiles: Don't rip through bosses.
	MF6_THRUSPECIES		= 0x00000002,	// Actors passes through other of the same species.
	MF6_MTHRUSPECIES	= 0x00000004,	// Missile passes through actors of its shooter's species.
	MF6_FORCEPAIN		= 0x00000008,	// forces target into painstate (unless it has the NOPAIN flag)
	MF6_NOFEAR			= 0x00000010,	// Not scared of frightening players
	MF6_BUMPSPECIAL		= 0x00000020,	// Actor executes its special when being collided (as the ST flag)
	MF6_DONTHARMSPECIES = 0x00000040,	// Don't hurt one's own species with explosions (hitscans, too?)
	MF6_STEPMISSILE		= 0x00000080,	// Missile can "walk" up steps
	MF6_NOTELEFRAG		= 0x00000100,	// [HW] Actor can't be telefragged
	MF6_TOUCHY			= 0x00000200,	// From MBF: killough 11/98: dies when solids touch it
	MF6_CANJUMP			= 0x00000400,	// From MBF: a dedicated flag instead of the BOUNCES+FLOAT+sentient combo
	MF6_JUMPDOWN		= 0x00000800,	// From MBF: generalization of dog behavior wrt. dropoffs.
	MF6_VULNERABLE		= 0x00001000,	// Actor can be damaged (even if not shootable).
	MF6_ARMED			= 0x00002000,	// From MBF: Object is armed (for MF6_TOUCHY objects)
	MF6_FALLING			= 0x00004000,	// From MBF: Object is falling (for pseudotorque simulation)
	MF6_LINEDONE		= 0x00008000,	// From MBF: Object has already run a line effect
	MF6_NOTRIGGER		= 0x00010000,	// actor cannot trigger any line actions
	MF6_SHATTERING		= 0x00020000,	// marks an ice corpse for forced shattering
	MF6_KILLED			= 0x00040000,	// Something that was killed (but not necessarily a corpse)
	MF6_BLOCKEDBYSOLIDACTORS = 0x00080000, // Blocked by solid actors, even if not solid itself
	MF6_ADDITIVEPOISONDAMAGE	= 0x00100000,
	MF6_ADDITIVEPOISONDURATION	= 0x00200000,
	MF6_NOMENU			= 0x00400000,	// Player class should not appear in the class selection menu.
	MF6_BOSSCUBE		= 0x00800000,	// Actor spawned by A_BrainSpit, flagged for timefreeze reasons.
	MF6_SEEINVISIBLE	= 0x01000000,	// Monsters can see invisible player.
	MF6_DONTCORPSE		= 0x02000000,	// [RC] Don't autoset MF_CORPSE upon death and don't force Crash state change.
	MF6_POISONALWAYS	= 0x04000000,	// Always apply poison, even when target can't take the damage.
	MF6_DOHARMSPECIES	= 0x08000000,	// Do hurt one's own species with projectiles.
	MF6_INTRYMOVE		= 0x10000000,	// Executing P_TryMove
	MF6_NOTAUTOAIMED	= 0x20000000,	// Do not subject actor to player autoaim.
	MF6_NOTONAUTOMAP	= 0x40000000,	// will not be shown on automap with the 'scanner' powerup.
	MF6_RELATIVETOFLOOR	= 0x80000000,	// [RC] Make flying actors be affected by lifts.
};

// --- mobj.flags7 ---
enum ActorFlag7
{
	MF7_NEVERTARGET		= 0x00000001,	// can not be targetted at all, even if monster friendliness is considered.
	MF7_NOTELESTOMP		= 0x00000002,	// cannot telefrag under any circumstances (even when set by MAPINFO)
	MF7_ALWAYSTELEFRAG	= 0x00000004,	// will unconditionally be telefragged when in the way. Overrides all other settings.
	MF7_HANDLENODELAY	= 0x00000008,	// respect NoDelay state flag
	MF7_WEAPONSPAWN		= 0x00000010,	// subject to DF_NO_COOP_WEAPON_SPAWN dmflag
	MF7_HARMFRIENDS		= 0x00000020,	// is allowed to harm friendly monsters.
	MF7_BUDDHA			= 0x00000040,	// Behaves just like the buddha cheat. 
	MF7_FOILBUDDHA		= 0x00000080,	// Similar to FOILINVUL, foils buddha mode.
	MF7_DONTTHRUST		= 0x00000100,	// Thrusting functions do not take, and do not give thrust (damage) to actors with this flag.
	MF7_ALLOWPAIN		= 0x00000200,	// Invulnerable or immune (via damagefactors) actors can still react to taking damage even if they don't.
	MF7_CAUSEPAIN		= 0x00000400,	// Damage sources with this flag can cause similar effects like ALLOWPAIN.
	MF7_THRUREFLECT		= 0x00000800,	// Actors who are reflective cause the missiles to not slow down or change angles.
	MF7_MIRRORREFLECT	= 0x00001000,	// Actor is turned directly 180 degrees around when reflected.
	MF7_AIMREFLECT		= 0x00002000,	// Actor is directly reflected straight back at the one who fired the projectile.
	MF7_HITTARGET		= 0x00004000,	// The actor the projectile dies on is set to target, provided it's targetable anyway.
	MF7_HITMASTER		= 0x00008000,	// Same as HITTARGET, except it's master instead of target.
	MF7_HITTRACER		= 0x00010000,	// Same as HITTARGET, but for tracer.
	MF7_FLYCHEAT		= 0x00020000,	// must be part of the actor so that it can be tracked properly
	MF7_NODECAL			= 0x00040000,	// [ZK] Forces puff to have no impact decal
	MF7_FORCEDECAL		= 0x00080000,	// [ZK] Forces puff's decal to override the weapon's.
	MF7_LAXTELEFRAGDMG	= 0x00100000,	// [MC] Telefrag damage can be reduced.
	MF7_ICESHATTER		= 0x00200000,	// [MC] Shatters ice corpses regardless of damagetype.
	MF7_ALLOWTHRUFLAGS	= 0x00400000,	// [MC] Allow THRUACTORS and the likes on puffs to prevent mod breakage.
	MF7_USEKILLSCRIPTS	= 0x00800000,	// [JM] Use "KILL" Script on death if not forced by GameInfo.
	MF7_NOKILLSCRIPTS	= 0x01000000,	// [JM] No "KILL" Script on death whatsoever, even if forced by GameInfo.
	MF7_SPRITEANGLE		= 0x02000000,	// [MC] Utilize the SpriteAngle property and lock the rotation to the degrees specified.
	MF7_SMASHABLE		= 0x04000000,	// dies if hitting the floor.
	MF7_NOSHIELDREFLECT = 0x08000000,	// will not be reflected by shields.
	MF7_FORCEZERORADIUSDMG = 0x10000000,	// passes zero radius damage on to P_DamageMobj, this is necessary in some cases where DoSpecialDamage gets overrideen.
	MF7_NOINFIGHTSPECIES = 0x20000000,	// don't start infights with one's own species.
	MF7_FORCEINFIGHTING	= 0x40000000,	// overrides a map setting of 'no infighting'.
	MF7_INCHASE			= 0x80000000,	// [RH] used by A_Chase and A_Look to avoid recursion
};
enum ActorFlag8
{
	MF8_FRIGHTENING		= 0x00000001,	// for those moments when halloween just won't do
	MF8_INSCROLLSEC		= 0x00000002,	// actor is partially inside a scrolling sector
	MF8_BLOCKASPLAYER	= 0x00000004,	// actor is blocked by player-blocking lines even if not a player
	MF8_DONTFACETALKER	= 0x00000008,	// don't alter the angle to face the player in conversations
	MF8_HITOWNER		= 0x00000010,	// projectile can hit the actor that fired it
	MF8_NOFRICTION		= 0x00000020,	// friction doesn't apply to the actor at all
	MF8_NOFRICTIONBOUNCE	= 0x00000040,	// don't bounce off walls when on icy floors
	MF8_RETARGETAFTERSLAM	= 0x00000080,	// Forces jumping to the idle state after slamming into something
	MF8_RECREATELIGHTS	= 0x00000100,	// Internal flag that signifies that the light attachments need to be recreated at the
	MF8_STOPRAILS		= 0x00000200,	// [MC] Prevent rails from going further if an actor has this flag.
	MF8_ABSVIEWANGLES	= 0x00000400,	// [MC] By default view angle/pitch/roll is an offset. This will make it absolute instead.
	MF8_FALLDAMAGE		= 0x00000800,	// Monster will take fall damage regardless of map settings.
	MF8_ALLOWTHRUBITS	= 0x00008000,	// [MC] Enable ThruBits property
	MF8_FULLVOLSEE		= 0x00010000,	// Play see sound at full volume
	MF8_E1M8BOSS		= 0x00020000,	// MBF21 boss death.
	MF8_E2M8BOSS		= 0x00040000,	// MBF21 boss death.
	MF8_E3M8BOSS		= 0x00080000,	// MBF21 boss death.
	MF8_E4M8BOSS		= 0x00100000,	// MBF21 boss death.
	MF8_E4M6BOSS		= 0x00200000,	// MBF21 boss death.
	MF8_MAP07BOSS1		= 0x00400000,	// MBF21 boss death.
	MF8_MAP07BOSS2		= 0x00800000,	// MBF21 boss death.
};

// --- mobj.renderflags ---
enum ActorRenderFlag
{
	RF_XFLIP			= 0x0001,	// Flip sprite horizontally
	RF_YFLIP			= 0x0002,	// Flip sprite vertically
	RF_ONESIDED			= 0x0004,	// Wall/floor sprite is visible from front only
	RF_FULLBRIGHT		= 0x0010,	// Sprite is drawn at full brightness

	RF_RELMASK			= 0x0300,	// ---Relative z-coord for bound actors (these obey texture pegging)
	RF_RELABSOLUTE		= 0x0000,	// Actor z is absolute
	RF_RELUPPER			= 0x0100,	// Actor z is relative to upper part of wall
	RF_RELLOWER			= 0x0200,	// Actor z is relative to lower part of wall
	RF_RELMID			= 0x0300,	// Actor z is relative to middle part of wall

	RF_CLIPMASK			= 0x0c00,	// ---Clipping for bound actors
	RF_CLIPFULL			= 0x0000,	// Clip sprite to full height of wall
	RF_CLIPUPPER		= 0x0400,	// Clip sprite to upper part of wall
	RF_CLIPMID			= 0x0800,	// Clip sprite to mid part of wall
	RF_CLIPLOWER		= 0x0c00,	// Clip sprite to lower part of wall

	RF_DECALMASK		= RF_RELMASK|RF_CLIPMASK,

	RF_SPRITETYPEMASK	= 0x7000,	// ---Different sprite types, not all implemented
	RF_FACESPRITE		= 0x0000,	// Face sprite
	RF_WALLSPRITE		= 0x1000,	// Wall sprite
	RF_FLATSPRITE		= 0x2000,	// Flat sprite
	RF_VOXELSPRITE		= 0x3000,	// Voxel object
	RF_INVISIBLE		= 0x8000,	// Don't bother drawing this actor
	RF_FORCEYBILLBOARD	= 0x10000,	// [BB] OpenGL only: draw with y axis billboard, i.e. anchored to the floor (overrides gl_billboard_mode setting)
	RF_FORCEXYBILLBOARD	= 0x20000,	// [BB] OpenGL only: draw with xy axis billboard, i.e. unanchored (overrides gl_billboard_mode setting)
	RF_ROLLSPRITE		= 0x40000,	//[marrub]roll the sprite billboard
	RF_DONTFLIP			= 0x80000,	// Don't flip it when viewed from behind.
	RF_ROLLCENTER		= 0x00100000, // Rotate from the center of sprite instead of offsets
	RF_MASKROTATION		= 0x00200000, // [MC] Only draw the actor when viewed from a certain angle range.
	RF_ABSMASKANGLE		= 0x00400000, // [MC] The mask rotation does not offset by the actor's angle.
	RF_ABSMASKPITCH		= 0x00800000, // [MC] The mask rotation does not offset by the actor's pitch.
	RF_INTERPOLATEANGLES		= 0x01000000, // [MC] Allow interpolation of the actor's angle, pitch and roll.
	RF_MAYBEINVISIBLE	= 0x02000000,
	RF_DONTINTERPOLATE	= 0x04000000,	// no render interpolation ever!

	RF_SPRITEFLIP		= 0x08000000,	// sprite flipped on x-axis
	RF_ZDOOMTRANS		= 0x10000000,	// is not normally transparent in Vanilla Doom
	RF_CASTSPRITESHADOW = 0x20000000,	// actor will cast a sprite shadow
	RF_NOINTERPOLATEVIEW = 0x40000000,	// don't interpolate the view next frame if this actor is a camera.
	RF_NOSPRITESHADOW = 0x80000000,		// actor will not cast a sprite shadow
};

// This translucency value produces the closest match to Heretic's TINTTAB.
// ~40% of the value of the overlaid image shows through.
const double HR_SHADOW = (0x6800 / 65536.);
// Hexen's TINTTAB is the same as Heretic's, just reversed.
const double HX_SHADOW = (0x9800 / 65536.);
const double HX_ALTSHADOW = (0x6800 / 65536.);

// This could easily be a bool but then it'd be much harder to find later. ;)
enum replace_t
{
	NO_REPLACE = 0,
	ALLOW_REPLACE = 1
};

enum ActorBounceFlag
{
	BOUNCE_Walls = 1<<0,		// bounces off of walls
	BOUNCE_Floors = 1<<1,		// bounces off of floors
	BOUNCE_Ceilings = 1<<2,		// bounces off of ceilings
	BOUNCE_Actors = 1<<3,		// bounces off of some actors
	BOUNCE_AllActors = 1<<4,	// bounces off of all actors (requires BOUNCE_Actors to be set, too)
	BOUNCE_AutoOff = 1<<5,		// when bouncing off a sector plane, if the new Z velocity is below 3.0, disable further bouncing
	BOUNCE_HereticType = 1<<6,	// goes into Death state when bouncing on floors or ceilings

	BOUNCE_UseSeeSound = 1<<7,	// compatibility fallback. This will only be set by
								// the compatibility handlers for the old bounce flags.
	BOUNCE_NoWallSound = 1<<8,	// don't make noise when bouncing off a wall
	BOUNCE_Quiet = 1<<9,		// Strife's grenades don't make a bouncing sound
	BOUNCE_ExplodeOnWater = 1<<10,	// explodes when hitting a water surface
	BOUNCE_CanBounceWater = 1<<11,	// can bounce on water
	// MBF bouncing is a bit different from other modes as Killough coded many special behavioral cases
	// for them that are not present in ZDoom, so it is necessary to identify it properly.
	BOUNCE_MBF = 1<<12,			// This in itself is not a valid mode, but replaces MBF's MF_BOUNCE flag.
	BOUNCE_AutoOffFloorOnly = 1<<13,		// like BOUNCE_AutoOff, but only on floors
	BOUNCE_UseBounceState = 1<<14,	// Use Bounce[.*] states
	BOUNCE_NotOnShootables = 1<<15,	// do not bounce off shootable actors if we are a projectile. Explode instead.
	BOUNCE_BounceOnUnrips = 1<<16,	// projectile bounces on actors with DONTRIP
	BOUNCE_NotOnSky = 1<<17,		// Don't bounce on sky floors / ceilings / walls
	BOUNCE_DEH = 1<<18,				// Flag was set through Dehacked.

	BOUNCE_TypeMask = BOUNCE_Walls | BOUNCE_Floors | BOUNCE_Ceilings | BOUNCE_Actors | BOUNCE_AutoOff | BOUNCE_HereticType | BOUNCE_MBF,

	// The three "standard" types of bounciness are:
	// HERETIC - Missile will only bounce off the floor once and then enter
	//			 its death state. It does not bounce off walls at all.
	// HEXEN -	 Missile bounces off of walls and floors indefinitely.
	// DOOM -	 Like Hexen, but the bounce turns off if its vertical velocity
	//			 is too low.
	BOUNCE_None = 0,
	BOUNCE_Heretic = BOUNCE_Floors | BOUNCE_Ceilings | BOUNCE_HereticType,
	BOUNCE_Doom = BOUNCE_Walls | BOUNCE_Floors | BOUNCE_Ceilings | BOUNCE_Actors | BOUNCE_AutoOff,
	BOUNCE_Hexen = BOUNCE_Walls | BOUNCE_Floors | BOUNCE_Ceilings | BOUNCE_Actors,
	BOUNCE_Grenade = BOUNCE_MBF | BOUNCE_Doom,		// Bounces on walls and flats like ZDoom bounce.
	BOUNCE_Classic = BOUNCE_MBF | BOUNCE_Floors | BOUNCE_Ceilings,	// Bounces on flats only, but 
																	// does not die when bouncing.

	// combined types
	BOUNCE_DoomCompat = BOUNCE_Doom | BOUNCE_UseSeeSound,
	BOUNCE_HereticCompat = BOUNCE_Heretic | BOUNCE_UseSeeSound,
	BOUNCE_HexenCompat = BOUNCE_Hexen | BOUNCE_UseSeeSound

	// The distinction between BOUNCE_Actors and BOUNCE_AllActors: A missile with
	// BOUNCE_Actors set will bounce off of reflective and "non-sentient" actors.
	// A missile that also has BOUNCE_AllActors set will bounce off of any actor.
	// For compatibility reasons when BOUNCE_Actors was implied by the bounce type
	// being "Doom" or "Hexen" and BOUNCE_AllActors was the separate
	// MF5_BOUNCEONACTORS, you must set BOUNCE_Actors for BOUNCE_AllActors to have
	// an effect.


};

// this is a special flag set that is exposed directly to DECORATE/ZScript
// these flags are for filtering actor visibility based on certain conditions of the renderer's feature support.
// currently, no renderer supports every single one of these features.
enum ActorRenderFeatureFlag
{
	RFF_FLATSPRITES		= 1<<0, // flat sprites
	RFF_MODELS			= 1<<1, // 3d models
	RFF_SLOPE3DFLOORS	= 1<<2, // sloped 3d floor support
	RFF_TILTPITCH		= 1<<3, // full free-look
	RFF_ROLLSPRITES		= 1<<4, // roll sprites
	RFF_UNCLIPPEDTEX	= 1<<5, // midtex and sprite can render "into" flats and walls
	RFF_MATSHADER		= 1<<6, // material shaders
	RFF_POSTSHADER		= 1<<7, // post-process shaders (renderbuffers)
	RFF_BRIGHTMAP		= 1<<8, // brightmaps
	RFF_COLORMAP		= 1<<9, // custom colormaps (incl. ability to fullbright certain ranges, ala Strife)
	RFF_POLYGONAL		= 1<<10, // uses polygons instead of wallscans/visplanes (i.e. softpoly and hardware opengl)
	RFF_TRUECOLOR		= 1<<11, // renderer is currently truecolor
	RFF_VOXELS		= 1<<12, // renderer is capable of voxels
};

// [TP] Flagset definitions
typedef TFlags<ActorFlag> ActorFlags;
typedef TFlags<ActorFlag2> ActorFlags2;
typedef TFlags<ActorFlag3> ActorFlags3;
typedef TFlags<ActorFlag4> ActorFlags4;
typedef TFlags<ActorFlag5> ActorFlags5;
typedef TFlags<ActorFlag6> ActorFlags6;
typedef TFlags<ActorFlag7> ActorFlags7;
typedef TFlags<ActorFlag8> ActorFlags8;
typedef TFlags<ActorRenderFlag> ActorRenderFlags;
typedef TFlags<ActorBounceFlag> ActorBounceFlags;
typedef TFlags<ActorRenderFeatureFlag> ActorRenderFeatureFlags;
DEFINE_TFLAGS_OPERATORS (ActorFlags)
DEFINE_TFLAGS_OPERATORS (ActorFlags2)
DEFINE_TFLAGS_OPERATORS (ActorFlags3)
DEFINE_TFLAGS_OPERATORS (ActorFlags4)
DEFINE_TFLAGS_OPERATORS (ActorFlags5)
DEFINE_TFLAGS_OPERATORS (ActorFlags6)
DEFINE_TFLAGS_OPERATORS (ActorFlags7)
DEFINE_TFLAGS_OPERATORS (ActorFlags8)
DEFINE_TFLAGS_OPERATORS (ActorRenderFlags)
DEFINE_TFLAGS_OPERATORS (ActorBounceFlags)
DEFINE_TFLAGS_OPERATORS (ActorRenderFeatureFlags)

// Used to affect the logic for thing activation through death, USESPECIAL and BUMPSPECIAL
// "thing" refers to what has the flag and the special, "trigger" refers to what used or bumped it
enum EThingSpecialActivationType
{
	THINGSPEC_Default			= 0,		// Normal behavior: a player must be the trigger, and is the activator
	THINGSPEC_ThingActs			= 1,		// The thing itself is the activator of the special
	THINGSPEC_ThingTargets		= 1<<1,		// The thing changes its target to the trigger
	THINGSPEC_TriggerTargets	= 1<<2,		// The trigger changes its target to the thing
	THINGSPEC_MonsterTrigger	= 1<<3,		// The thing can be triggered by a monster
	THINGSPEC_MissileTrigger	= 1<<4,		// The thing can be triggered by a projectile
	THINGSPEC_ClearSpecial		= 1<<5,		// Clears special after successful activation
	THINGSPEC_NoDeathSpecial	= 1<<6,		// Don't activate special on death
	THINGSPEC_TriggerActs		= 1<<7,		// The trigger is the activator of the special
											// (overrides LEVEL_ACTOWNSPECIAL Hexen hack)
	THINGSPEC_Activate			= 1<<8,		// The thing is activated when triggered
	THINGSPEC_Deactivate		= 1<<9,		// The thing is deactivated when triggered
	THINGSPEC_Switch			= 1<<10,	// The thing is alternatively activated and deactivated when triggered
};

#define ONFLOORZ		FIXED_MIN
#define ONCEILINGZ		FIXED_MAX
#define FLOATRANDZ		(FIXED_MAX-1)


class FDecalBase;

inline AActor *GetDefaultByName (const char *name)
{
	return (AActor *)(PClass::FindClass(name)->Defaults);
}

inline AActor* GetDefaultByName(FName name)
{
	return (AActor*)(PClass::FindClass(name)->Defaults);
}

inline AActor *GetDefaultByType (const PClass *type)
{
	return (AActor *)(type->Defaults);
}

template<class T>
inline T *GetDefault ()
{
	return (T *)(RUNTIME_CLASS_CASTLESS(T)->Defaults);
}

struct line_t;
struct secplane_t;
struct msecnode_t;
struct FStrifeDialogueNode;

struct FLinkContext
{
	msecnode_t *sector_list = nullptr;
	msecnode_t *render_list = nullptr;
};

struct FDropItem
{
	FDropItem *Next;
	FName Name;
	int Probability;
	int Amount;
};

const double MinVel = EQUAL_EPSILON;

// Map Object definition.
class AActor : public DThinker
{
	DECLARE_CLASS_WITH_META (AActor, DThinker, PClassActor)
	HAS_OBJECT_POINTERS
public:
	AActor() = default;
	AActor(const AActor &other) = delete;	// Calling this would be disastrous.
	AActor &operator= (const AActor &other);
	~AActor ();

	virtual void OnDestroy() override;
	virtual void Serialize(FSerializer &arc) override;
	virtual void PostSerialize() override;
	virtual void PostBeginPlay() override;		// Called immediately before the actor's first tick
	virtual void Tick() override;

	static AActor *StaticSpawn (FLevelLocals *Level, PClassActor *type, const DVector3 &pos, replace_t allowreplacement, bool SpawningMapThing = false);

	inline AActor *GetDefault () const
	{
		return (AActor *)(this->GetClass()->Defaults);
	}

	FActorInfo *GetInfo() const
	{
		return static_cast<PClassActor*>(GetClass())->ActorInfo();
	}


	FDropItem *GetDropItems() const;

	// Return true if the monster should use a missile attack, false for melee
	bool SuggestMissileAttack (double dist);

	// Adjusts the angle for deflection/reflection of incoming missiles
	// Returns true if the missile should be allowed to explode anyway
	bool AdjustReflectionAngle (AActor *thing, DAngle &angle);
	int AbsorbDamage(int damage, FName dmgtype, AActor *inflictor, AActor *source, int flags);
	void AlterWeaponSprite(visstyle_t *vis);

	// Returns true if this actor is within melee range of its target
	bool CheckMeleeRange(double range = -1);

	bool CheckNoDelay();

	virtual void BeginPlay();			// Called immediately after the actor is created
	void CallBeginPlay();

	// [ZZ] custom postbeginplay (calls E_WorldThingSpawned)
	void CallPostBeginPlay() override;

	void LevelSpawned();				// Called after BeginPlay if this actor was spawned by the world
	void HandleSpawnFlags();	// Translates SpawnFlags into in-game flags.

	virtual void Activate (AActor *activator);
	void CallActivate(AActor *activator);

	virtual void Deactivate(AActor *activator);
	void CallDeactivate(AActor *activator);

	void BotThink();

	void SlopedFloorHandling();
	//carry offset is additional velocity produced from standing in carrying sectors
	void FullAxisMovement(DVector2 carryoffset = DVector2(0.0, 0.0) );

	// Called when actor dies
	virtual void Die (AActor *source, AActor *inflictor, int dmgflags = 0, FName MeansOfDeath = NAME_None);
	void CallDie(AActor *source, AActor *inflictor, int dmgflags = 0, FName MeansOfDeath = NAME_None);

	// Perform some special damage action. Returns the amount of damage to do.
	// Returning -1 signals the damage routine to exit immediately
	virtual int DoSpecialDamage (AActor *target, int damage, FName damagetype);
	int CallDoSpecialDamage(AActor *target, int damage, FName damagetype);

	// Like DoSpecialDamage, but called on the actor receiving the damage.
	virtual int TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype);
	int CallTakeSpecialDamage(AActor *inflictor, AActor *source, int damage, FName damagetype);

	// Actor had MF_SKULLFLY set and rammed into something
	// Returns false to stop moving and true to keep moving
	virtual bool Slam(AActor *victim);
	bool CallSlam(AActor *victim);

	// Something just touched this actor.
	virtual void Touch(AActor *toucher);
	void CallTouch(AActor *toucher);

	// Centaurs and ettins squeal when electrocuted, poisoned, or "holy"-ed
	// Made a metadata property so no longer virtual
	void Howl ();

	// plays bouncing sound
	void PlayBounceSound(bool onfloor);

	// Called when an actor with MF_MISSILE and MF2_FLOORBOUNCE hits the floor
	bool FloorBounceMissile (secplane_t &plane);

	// Called by RoughBlockCheck
	bool IsOkayToAttack (AActor *target);

	// Plays the actor's ActiveSound if its voice isn't already making noise.
	void PlayActiveSound ();

	void RestoreSpecialPosition();

	// Called by PIT_CheckThing() and needed for some Hexen things.
	// Returns -1 for normal behavior, 0 to return false, and 1 to return true.
	// I'm not sure I like it this way, but it will do for now.
	// (virtual on the script side only)
	int SpecialMissileHit (AActor *victim);

	// Returns true if it's okay to switch target to "other" after being attacked by it.
	bool CallOkayToSwitchTarget(AActor *other);
	bool OkayToSwitchTarget (AActor *other);

	// Uses an item and removes it from the inventory.
	bool UseInventory (AActor *item);

	// Tosses an item out of the inventory.
	AActor *DropInventory (AActor *item, int amt = -1);

	void ApplyPowerups();

	// Removes all items from the inventory.
	void ClearInventory();

	// Returns true if this view is considered "local" for the player.
	bool CheckLocalView() const;

	// Finds the first item of a particular type.
	AActor *FindInventory (PClassActor *type, bool subclass=false);
	AActor *FindInventory (FName type, bool subclass = false);
	template<class T> T *FindInventory ()
	{
		return static_cast<T *> (FindInventory (RUNTIME_CLASS(T)));
	}

	// Adds one item of a particular type. Returns NULL if it could not be added.
	AActor *GiveInventoryType (PClassActor *type);

	// Destroys all the inventory the actor is holding.
	void DestroyAllInventory ();

	// Set the alphacolor field properly
	void SetShade (uint32_t rgb);
	void SetShade (int r, int g, int b);

	// Plays a conversation animation
	void ConversationAnimation (int animnum);

	// Make this actor hate the same things as another actor
	void CopyFriendliness (AActor *other, bool changeTarget, bool resetHealth=true);

	// Moves the other actor's inventory to this one
	void ObtainInventory (AActor *other);

	// Die. Now.
	bool Massacre ();

	// Transforms the actor into a finely-ground paste
	bool Grind(bool items);
	bool CallGrind(bool items);

	// Get this actor's team
	int GetTeam();

	// Is the other actor on my team?
	bool IsTeammate (AActor *other);

	// Is the other actor my friend?
	bool IsFriend (AActor *other);

	// Do I hate the other actor?
	bool IsHostile (AActor *other);

	inline bool IsNoClip2() const;
	void CheckPortalTransition(bool islinked);
	DVector3 GetPortalTransition(double byoffset, sector_t **pSec = NULL);

	// What species am I?
	FName GetSpecies();

	// set translation
	void SetTranslation(FName trname);

	double GetBobOffset(double ticfrac = 0) const;

	// Enter the crash state
	void Crash();

	// Return starting health adjusted by skill level
	double AttackOffset(double offset = 0);
	int SpawnHealth() const;
	virtual int GetMaxHealth(bool withupgrades = false) const;
	int GetGibHealth() const;
	double GetCameraHeight() const;

	inline bool isMissile(bool precise=true)
	{
		return (flags&MF_MISSILE) || (precise && GetDefault()->flags&MF_MISSILE);
	}

	// Check for monsters that count as kill but excludes all friendlies.
	bool CountsAsKill() const
	{
		return (flags & MF_COUNTKILL) && !(flags & MF_FRIENDLY);
	}

	// These also set CF_INTERPVIEW for players.
	DAngle ClampPitch(DAngle p);
	void SetPitch(DAngle p, int fflags);
	void SetAngle(DAngle ang, int fflags);
	void SetRoll(DAngle roll, int fflags);
	void SetViewPitch(DAngle p, int fflags);
	void SetViewAngle(DAngle ang, int fflags);
	void SetViewRoll(DAngle roll, int fflags);

	PClassActor *GetBloodType(int type = 0) const;

	double Distance2DSquared(AActor *other, bool absolute = false)
	{
		DVector2 otherpos = absolute ? other->Pos() : other->PosRelative(this);
		return (Pos().XY() - otherpos).LengthSquared();
	}

	double Distance2D(AActor *other, bool absolute = false) const
	{
		DVector2 otherpos = absolute ? other->Pos() : other->PosRelative(this);
		return (Pos().XY() - otherpos).Length();
	}

	double Distance2D(double x, double y) const
	{
		return DVector2(X() - x, Y() - y).Length();
	}

	double Distance2D(AActor *other, double xadd, double yadd, bool absolute = false) const
	{
		DVector3 otherpos = absolute ? other->Pos() : other->PosRelative(this);
		return DVector2(X() - otherpos.X + xadd, Y() - otherpos.Y + yadd).Length();
	}


	// a full 3D version of the above
	double Distance3DSquared(AActor *other, bool absolute = false)
	{
		DVector3 otherpos = absolute ? other->Pos() : other->PosRelative(this);
		return (Pos() - otherpos).LengthSquared();
	}

	double Distance3D(AActor *other, bool absolute = false)
	{
		DVector3 otherpos = absolute ? other->Pos() : other->PosRelative(this);
		return (Pos() - otherpos).Length();
	}

	DAngle AngleTo(AActor *other, bool absolute = false)
	{
		DVector2 otherpos = absolute ? other->Pos() : other->PosRelative(this);
		return VecToAngle(otherpos - Pos().XY());
	}

	DAngle AngleTo(AActor *other, double oxofs, double oyofs, bool absolute = false) const
	{
		DVector2 otherpos = absolute ? other->Pos() : other->PosRelative(this);
		return VecToAngle(otherpos - Pos() + DVector2(oxofs, oyofs));
	}

	DVector2 Vec2To(AActor *other) const
	{
		return other->PosRelative(this) - Pos();
	}

	DVector3 Vec3To(AActor *other) const
	{
		return other->PosRelative(this) - Pos();
	}

	DVector2 Vec2Offset(double dx, double dy, bool absolute = false);
	DVector3 Vec2OffsetZ(double dx, double dy, double atz, bool absolute = false);
	DVector2 Vec2Angle(double length, DAngle angle, bool absolute = false);
	DVector3 Vec3Offset(double dx, double dy, double dz, bool absolute = false);
	DVector3 Vec3Offset(const DVector3 &ofs, bool absolute = false);
	DVector3 Vec3Angle(double length, DAngle angle, double dz, bool absolute = false);

	void ClearInterpolation();

	void Move(const DVector3 &vel)
	{
		SetOrigin(Pos() + vel, true);
	}
	virtual void SetOrigin(double x, double y, double z, bool moving);
	void SetOrigin(const DVector3 & npos, bool moving)
	{
		SetOrigin(npos.X, npos.Y, npos.Z, moving);
	}

	inline void SetFriendPlayer(player_t *player);

	bool IsVisibleToPlayer() const;
	bool IsInsideVisibleAngles() const;

	// Calculate amount of missile damage
	int GetMissileDamage(int mask, int add);

	bool CanSeek(AActor *target) const;

	double GetGravity() const;
	bool IsSentient() const;
	const char *GetTag(const char *def = NULL) const;
	void SetTag(const char *def);
	const char *GetCharacterName() const;

	DVector2 CarryingSectorsHandling();
	void RespawnHandling();
	// Triggers SECSPAC_Exit/SECSPAC_Enter and related events if oldsec != current sector
	void CheckSectorTransition(sector_t *oldsec);
	void UpdateRenderSectorList();
	void ClearRenderSectorList();
	void ClearRenderLineList();

	void AttachLight(unsigned int count, const FLightDefaults *lightdef);
	void SetDynamicLights();

// info for drawing
// NOTE: The first member variable *must* be snext.
	AActor			*snext, **sprev;	// links in sector (if needed)
	DVector3		__Pos;		// double underscores so that it won't get used by accident. Access to this should be exclusively through the designated access functions.

	DAngle			SpriteAngle;
	DAngle			SpriteRotation;
	DRotator		Angles;
	DRotator		ViewAngles;			// Offsets for cameras
	DVector2		Scale;				// Scaling values; 1 is normal size
	double			Alpha;				// Since P_CheckSight makes an alpha check this can't be a float. It has to be a double.

	int				sprite;				// used to find patch_t and flip value
	uint8_t			frame;				// sprite frame to draw
	uint8_t			effects;			// [RH] see p_effect.h
	uint8_t			fountaincolor;		// Split out of 'effect' to have easier access.
	FRenderStyle	RenderStyle;		// Style to draw this actor with
	FTextureID		picnum;				// Draw this instead of sprite if valid
	uint32_t			fillcolor;			// Color to draw when STYLE_Shaded
	uint32_t			Translation;

	uint32_t			RenderRequired;		// current renderer must have this feature set
	uint32_t			RenderHidden;		// current renderer must *not* have any of these features

	ActorRenderFlags	renderflags;		// Different rendering flags
	ActorFlags		flags;
	ActorFlags2		flags2;			// Heretic flags
	ActorFlags3		flags3;			// [RH] Hexen/Heretic actor-dependant behavior made flaggable
	ActorFlags4		flags4;			// [RH] Even more flags!
	ActorFlags5		flags5;			// OMG! We need another one.
	ActorFlags6		flags6;			// Shit! Where did all the flags go?
	ActorFlags7		flags7;			// WHO WANTS TO BET ON 8!?
	ActorFlags8		flags8;			// I see your 8, and raise you a bet for 9.
	double			Floorclip;		// value to use for floor clipping
	double			radius, Height;		// for movement checking

	FAngle			VisibleStartAngle;
	FAngle			VisibleStartPitch;
	FAngle			VisibleEndAngle;
	FAngle			VisibleEndPitch;

	DVector3		OldRenderPos;
	DVector3		Vel;
	DVector2		SpriteOffset;
	double			Speed;
	double			FloatSpeed;

// interaction info
	FBlockNode		*BlockNode;			// links in blocks (if needed)
	struct sector_t	*Sector;
	subsector_t *		subsector;
	FSection *			section;
	double			floorz, ceilingz;	// closest together of contacted secs
	double			dropoffz;		// killough 11/98: the lowest floor over all contacted Sectors.

	uint32_t		ThruBits;
	FTextureID		floorpic;			// contacted sec floorpic
	int				floorterrain;
	FTextureID		ceilingpic;			// contacted sec ceilingpic

	struct sector_t	*floorsector;
	struct sector_t	*ceilingsector;
	double			renderradius;

	double			projectilepassheight;	// height for clipping projectile movement against this actor
	double			CameraHeight;	// Height of camera when used as such
	double			CameraFOV;

	double			RadiusDamageFactor;		// Radius damage factor
	double			SelfDamageFactor;
	double			StealthAlpha;	// Minmum alpha for MF_STEALTH.
	int				WoundHealth;		// Health needed to enter wound state

	int32_t			tics;				// state tic counter
	FState			*state;
	//VMFunction		*Damage;			// For missiles and monster railgun
	int				DamageVal;
	VMFunction		*DamageFunc;
	int				projectileKickback;

	// [BB] If 0, everybody can see the actor, if > 0, only members of team (VisibleToTeam-1) can see it.
	uint32_t			VisibleToTeam;

	int				special1;		// Special info
	int				special2;		// Special info
	double			specialf1;		// With floats we cannot use the int versions for storing position or angle data without reverting to fixed point (which we do not want.)
	double			specialf2;

	int				weaponspecial;	// Special info for weapons.
	int 			health;
	uint8_t			movedir;		// 0-7
	int8_t			visdir;
	int16_t			movecount;		// when 0, select a new dir
	int16_t			strafecount;	// for MF3_AVOIDMELEE
	uint16_t			SpawnAngle;
	TObjPtr<AActor*> target;			// thing being chased/attacked (or NULL)
									// also the originator for missiles
	TObjPtr<AActor*>	lastenemy;		// Last known enemy -- killough 2/15/98
	TObjPtr<AActor*> LastHeard;		// [RH] Last actor this one heard
	int32_t			reactiontime;	// if non 0, don't attack yet; used by
									// player to freeze a bit after teleporting
	int32_t			threshold;		// if > 0, the target will be chased
	int32_t			DefThreshold;	// [MC] Default threshold which the actor will reset its threshold to after switching targets
									// no matter what (even if shot)
	player_t		*player;		// only valid if type of PlayerPawn
	TObjPtr<AActor*>	LastLookActor;	// Actor last looked for (if TIDtoHate != 0)
	DVector3		SpawnPoint; 	// For nightmare respawn
	int				StartHealth;
	uint8_t			WeaveIndexXY;	// Separated from special2 because it's used by globally accessible functions.
	uint8_t			WeaveIndexZ;
	uint16_t		skillrespawncount;
	int				TIDtoHate;			// TID of things to hate (0 if none)
	FName		Species;		// For monster families
	TObjPtr<AActor*>	alternative;	// (Un)Morphed actors stored here. Those with the MF_UNMORPHED flag are the originals.
	TObjPtr<AActor*>	tracer;			// Thing being chased/attacked for tracers
	TObjPtr<AActor*>	master;			// Thing which spawned this one (prevents mutual attacks)

	int				tid;			// thing identifier
	int				special;		// special
	int				args[5];		// special arguments

	int		accuracy, stamina;		// [RH] Strife stats -- [XA] moved here for DECORATE/ACS access.

	AActor			*inext, **iprev;// Links to other mobjs in same bucket
	TObjPtr<AActor*> goal;			// Monster's goal if not chasing anything
	int				waterlevel;		// 0=none, 1=feet, 2=waist, 3=eyes
	uint8_t			boomwaterlevel;	// splash information for non-swimmable water sectors
	uint8_t			MinMissileChance;// [RH] If a random # is > than this, then missile attack.
	int8_t			LastLookPlayerNumber;// Player number last looked for (if TIDtoHate == 0)
	ActorBounceFlags	BounceFlags;	// which bouncing type?
	uint32_t			SpawnFlags;		// Increased to uint32_t because of Doom 64
	double			meleerange;		// specifies how far a melee attack reaches.
	double			meleethreshold;	// Distance below which a monster doesn't try to shoot missiles anynore
									// but instead tries to come closer for a melee attack.
									// This is not the same as meleerange
	double			maxtargetrange;	// any target farther away cannot be attacked
	double			bouncefactor;	// Strife's grenades use 50%, Hexen's Flechettes 70.
	double			wallbouncefactor;	// The bounce factor for walls can be different.
	double			Gravity;		// [GRB] Gravity factor
	double			Friction;
	double			pushfactor;
	int				bouncecount;	// Strife's grenades only bounce twice before exploding
	int 			FastChaseStrafeCount;
	int				lastpush;
	int				activationtype;	// How the thing behaves when activated with USESPECIAL or BUMPSPECIAL
	int				lastbump;		// Last time the actor was bumped, used to control BUMPSPECIAL
	int				Score;			// manipulated by score items, ACS or DECORATE. The engine doesn't use this itself for anything.
	FString *		Tag;			// Strife's tag name.
	int				DesignatedTeam;	// Allow for friendly fire cacluations to be done on non-players.
	int				friendlyseeblocks;	// allow to override friendly search distance calculation

	AActor			*BlockingMobj;	// Actor that blocked the last move
	line_t			*BlockingLine;	// Line that blocked the last move
	sector_t		*Blocking3DFloor;	// 3D floor that blocked the last move (if any)
	sector_t		*BlockingCeiling;	// Sector that blocked the last move (ceiling plane slope)
	sector_t		*BlockingFloor;		// Sector that blocked the last move (floor plane slope)

	int PoisonDamage; // Damage received per tic from poison.
	FName PoisonDamageType; // Damage type dealt by poison.
	int PoisonDuration; // Duration left for receiving poison damage.
	int PoisonPeriod; // How often poison damage is applied. (Every X tics.)

	int PoisonDamageReceived; // Damage received per tic from poison.
	FName PoisonDamageTypeReceived; // Damage type received by poison.
	int PoisonDurationReceived; // Duration left for receiving poison damage.
	int PoisonPeriodReceived; // How often poison damage is applied. (Every X tics.)
	TObjPtr<AActor*> Poisoner; // Last source of received poison damage.

	// a linked list of sectors where this object appears
	struct msecnode_t	*touching_sectorlist;				// phares 3/14/98
	struct msecnode_t	*touching_sectorportallist;		// same for cross-sectorportal rendering
	struct portnode_t	*touching_lineportallist;		// and for cross-lineportal
	struct msecnode_t	*touching_rendersectors; // this is the list of sectors that this thing interesects with it's max(radius, renderradius).
	int validcount;


	TObjPtr<AActor*>	Inventory;		// [RH] This actor's inventory
	uint32_t			InventoryID;	// A unique ID to keep track of inventory items

	uint8_t smokecounter;
	uint8_t FloatBobPhase;
	uint8_t FriendPlayer;				// [RH] Player # + 1 this friendly monster works for (so 0 is no player, 1 is player 0, etc)
	double FloatBobStrength;
	PalEntry BloodColor;
	uint32_t BloodTranslation;

	// [RH] Stuff that used to be part of an Actor Info
	FSoundIDNoInit SeeSound;
	FSoundIDNoInit AttackSound;
	FSoundIDNoInit PainSound;
	FSoundIDNoInit DeathSound;
	FSoundIDNoInit ActiveSound;
	FSoundIDNoInit UseSound;		// [RH] Sound to play when an actor is used.
	FSoundIDNoInit BounceSound;
	FSoundIDNoInit WallBounceSound;
	FSoundIDNoInit CrushPainSound;

	double MaxDropOffHeight;
	double MaxStepHeight;
	double MaxSlopeSteepness;

	int32_t Mass;
	int16_t PainChance;
	int PainThreshold;
	FName DamageType;
	FName DamageTypeReceived;
	double DamageFactor;
	double DamageMultiply;

	FName PainType;
	FName DeathType;
	PClassActor *TeleFogSourceType;
	PClassActor *TeleFogDestType;
	int RipperLevel;
	int RipLevelMin;
	int RipLevelMax;

	int ConversationRoot;				// THe root of the current dialogue
	FStrifeDialogueNode* Conversation;	// [RH] The dialogue to show when this actor is "used."

	FState *SpawnState;
	FState *SeeState;
	FState *MeleeState;
	FState *MissileState;


	// [RH] Decal(s) this weapon/projectile generates on impact.
	FDecalBase *DecalGenerator;

	// [RH] Used to interpolate the view to get >35 FPS
	DVector3 Prev;
	DRotator PrevAngles;
	int PrevPortalGroup;
	TArray<FDynamicLight *> AttachedLights;
	TDeletingArray<FLightDefaults *> UserLights;

	// When was this actor spawned?
	int SpawnTime;
	uint32_t SpawnOrder;


	// ThingIDs
	void SetTID (int newTID);

private:
	void AddToHash ();
	void RemoveFromHash ();

	static inline int TIDHASH (int key) { return key & 127; }

public:
	static FSharedStringArena mStringPropertyData;
private:
	friend class FActorIterator;

	bool FixMapthingPos();

public:
	void LinkToWorld (FLinkContext *ctx, bool spawningmapthing=false, sector_t *sector = NULL);
	void UnlinkFromWorld(FLinkContext *ctx);
	void AdjustFloorClip ();
	bool IsMapActor();
	int GetTics(FState * newstate);
	bool SetState (FState *newstate, bool nofunction=false);
	bool AdvanceState();
	virtual void SplashCheck();
	virtual bool UpdateWaterLevel (bool splash=true);
	bool isFast();
	bool isSlow();
	void SetIdle(bool nofunction=false);
	void ClearCounters();
	FState *GetRaiseState();
	void Revive();

	void SetDamage(int dmg)
	{
		DamageVal = dmg;
		DamageFunc = nullptr;
	}

	bool IsZeroDamage() const
	{
		return DamageVal == 0 && DamageFunc == nullptr;
	}

	FState *FindState (FName label) const
	{
		return GetClass()->FindState(1, &label);
	}

	FState *FindState (FName label, FName sublabel, bool exact = false) const
	{
		FName names[] = { label, sublabel };
		return GetClass()->FindState(2, names, exact);
	}

	FState *FindState(int numnames, FName *names, bool exact = false) const
	{
		return GetClass()->FindState(numnames, names, exact);
	}

	bool HasSpecialDeathStates () const;

	double X() const
	{
		return __Pos.X;
	}
	double Y() const
	{
		return __Pos.Y;
	}
	double Z() const
	{
		return __Pos.Z;
	}
	DVector3 Pos() const
	{
		return __Pos;
	}
	// Note: Never compare z directly with a plane height if you want to know if the actor is *on* the plane. Some very minor inaccuracies may creep in. Always use these inline functions!
	// Comparing with floorz is ok because those values come from the same calculations.
	bool isAbove(double checkz) const
	{
		return Z() > checkz + EQUAL_EPSILON;
	}
	bool isBelow(double checkz) const
	{
		return Z() < checkz - EQUAL_EPSILON;
	}
	bool isAtZ(double checkz) const
	{
		return fabs(Z() - checkz) < EQUAL_EPSILON;
	}

	double RenderRadius() const
	{
		return MAX(radius, renderradius);
	}

	DVector3 PosRelative(int grp) const;
	DVector3 PosRelative(const AActor *other) const;
	DVector3 PosRelative(sector_t *sec) const;
	DVector3 PosRelative(const line_t *line) const;

	FVector3 SoundPos() const
	{
		// the sound system switches y and z axes so this function must, too.
		// fixme: This still needs portal handling
		return{ float(X()), float(Z()), float(Y()) };
	}
	DVector3 InterpolatedPosition(double ticFrac) const
	{
		if (renderflags & RF_DONTINTERPOLATE) return Pos();
		else return Prev + (ticFrac * (Pos() - Prev));
	}
	DRotator InterpolatedAngles(double ticFrac) const
	{
		DRotator result;
		result.Yaw = PrevAngles.Yaw + deltaangle(PrevAngles.Yaw, Angles.Yaw) * ticFrac;
		result.Pitch = PrevAngles.Pitch + deltaangle(PrevAngles.Pitch, Angles.Pitch) * ticFrac;
		result.Roll = PrevAngles.Roll + deltaangle(PrevAngles.Roll, Angles.Roll) * ticFrac;
		return result;
	}
	DAngle GetSpriteAngle(DAngle viewangle, double ticFrac)
	{
		if (flags7 & MF7_SPRITEANGLE)
		{
			return SpriteAngle;
		}
		else
		{
			DAngle thisang;
			if (renderflags & RF_INTERPOLATEANGLES) thisang = PrevAngles.Yaw + deltaangle(PrevAngles.Yaw, Angles.Yaw) * ticFrac;
			else thisang = Angles.Yaw;
			return viewangle - (thisang + SpriteRotation);
		}
	}
	DVector3 PosPlusZ(double zadd) const
	{
		return { X(), Y(), Z() + zadd };
	}
	DVector3 PosAtZ(double zadd) const
	{
		return{ X(), Y(), zadd };
	}
	double Top() const
	{
		return Z() + Height;
	}
	double Center() const
	{
		return Z() + Height/2;
	}
	void SetZ(double newz, bool moving = true)
	{
		__Pos.Z = newz;
	}
	void AddZ(double newz, bool moving = true)
	{
		__Pos.Z += newz;
		if (!moving) Prev.Z = Z();
	}

	void SetXY(const DVector2 &npos)
	{
		__Pos.X = npos.X;
		__Pos.Y = npos.Y;
	}
	void SetXYZ(double xx, double yy, double zz)
	{
		__Pos = { xx,yy,zz };
	}
	void SetXYZ(const DVector3 &npos)
	{
		__Pos = npos;
	}

	double VelXYToSpeed() const
	{
		return DVector2(Vel.X, Vel.Y).Length();
	}

	double VelToSpeed() const
	{
		return Vel.Length();
	}

	void AngleFromVel()
	{
		Angles.Yaw = VecToAngle(Vel.X, Vel.Y);
	}

	void VelFromAngle()
	{
		Vel.X = Speed * Angles.Yaw.Cos();
		Vel.Y = Speed * Angles.Yaw.Sin();
	}

	void VelFromAngle(double speed)
	{
		Vel.X = speed * Angles.Yaw.Cos();
		Vel.Y = speed * Angles.Yaw.Sin();
	}

	void VelFromAngle(double speed, DAngle angle)
	{
		Vel.X = speed * angle.Cos();
		Vel.Y = speed * angle.Sin();
	}

	void Thrust()
	{
		Vel.X += Speed * Angles.Yaw.Cos();
		Vel.Y += Speed * Angles.Yaw.Sin();
	}

	void Thrust(double speed)
	{
		Vel.X += speed * Angles.Yaw.Cos();
		Vel.Y += speed * Angles.Yaw.Sin();
	}

	void Thrust(DAngle angle, double speed)
	{
		Vel.X += speed * angle.Cos();
		Vel.Y += speed * angle.Sin();
	}

	void Vel3DFromAngle(DAngle angle, DAngle pitch, double speed)
	{
		double cospitch = pitch.Cos();
		Vel.X = speed * cospitch * angle.Cos();
		Vel.Y = speed * cospitch * angle.Sin();
		Vel.Z = speed * -pitch.Sin();
	}

	void Vel3DFromAngle(DAngle pitch, double speed)
	{
		double cospitch = pitch.Cos();
		Vel.X = speed * cospitch * Angles.Yaw.Cos();
		Vel.Y = speed * cospitch * Angles.Yaw.Sin();
		Vel.Z = speed * -pitch.Sin();
	}

	// This is used by many vertical velocity calculations.
	// Better have it in one place, if something needs to be changed about the formula.
	double DistanceBySpeed(AActor *dest, double speed) const
	{
		return MAX(1., Distance2D(dest) / speed);
	}

	int ApplyDamageFactor(FName damagetype, int damage) const;
	int GetModifiedDamage(FName damagetype, int damage, bool passive, AActor *inflictor, AActor *source, int flags = 0);
	void DeleteAttachedLights();
	bool isFrozen() const;

	bool				hasmodel;
};

class FActorIterator
{
	friend struct FLevelLocals;
protected:
	FActorIterator (AActor **hash, int i) : TIDHash(hash), base (nullptr), id (i)
	{
	}
	FActorIterator (AActor **hash, int i, AActor *start) : TIDHash(hash), base (start), id (i)
	{
	}
public:
	AActor *Next ()
	{
		if (id == 0)
			return nullptr;
		if (!base)
			base = TIDHash[id & 127];
		else
			base = base->inext;

		while (base && base->tid != id)
			base = base->inext;

		return base;
	}
	void Reinit()
	{
		base = nullptr;
	}

private:
	AActor **TIDHash;
	AActor *base;
	int id;
};

class NActorIterator : public FActorIterator
{
	friend struct FLevelLocals;
	const PClass *type;
protected:
	NActorIterator (AActor **hash, const PClass *cls, int id) : FActorIterator (hash, id) { type = cls; }
	NActorIterator (AActor **hash, FName cls, int id) : FActorIterator (hash, id) { type = PClass::FindClass(cls); }
public:
	AActor *Next ()
	{
		AActor *actor;
		if (type == nullptr) return nullptr;
		do
		{
			actor = FActorIterator::Next ();
		} while (actor && !actor->IsKindOf (type));
		return actor;
	}
};

PClassActor *ClassForSpawn(FName classname);

inline AActor *Spawn(FLevelLocals *Level, PClassActor *type)
{
	return AActor::StaticSpawn(Level, type, DVector3(0, 0, 0), NO_REPLACE);
}

inline AActor *Spawn(FLevelLocals *Level, PClassActor *type, const DVector3 &pos, replace_t allowreplacement)
{
	return AActor::StaticSpawn(Level, type, pos, allowreplacement);
}

inline AActor *Spawn(FLevelLocals *Level, FName type)
{
	return AActor::StaticSpawn(Level, ClassForSpawn(type), DVector3(0, 0, 0), NO_REPLACE);
}

inline AActor *Spawn(FLevelLocals *Level, FName type, const DVector3 &pos, replace_t allowreplacement)
{
	return AActor::StaticSpawn(Level, ClassForSpawn(type), pos, allowreplacement);
}

template<class T> inline T *Spawn(FLevelLocals *Level, const DVector3 &pos, replace_t allowreplacement)
{
	return static_cast<T *>(AActor::StaticSpawn(Level, RUNTIME_CLASS(T), pos, allowreplacement));
}

template<class T> inline T *Spawn(FLevelLocals *Level)	// for inventory items we do not need coordinates and replacement info.
{
	return static_cast<T *>(AActor::StaticSpawn(Level, RUNTIME_CLASS(T), DVector3(0, 0, 0), NO_REPLACE));
}

inline PClassActor *PClass::FindActor(FName name)
{
	auto cls = FindClass(name);
	return cls && cls->IsDescendantOf(RUNTIME_CLASS(AActor)) ? static_cast<PClassActor*>(cls) : nullptr;
}

inline PClassActor *ValidateActor(PClass *cls)
{
	return cls && cls->IsDescendantOf(RUNTIME_CLASS(AActor)) ? static_cast<PClassActor*>(cls) : nullptr;
}

void PrintMiscActorInfo(AActor * query);
AActor *P_LinePickActor(AActor *t1, DAngle angle, double distance, DAngle pitch, ActorFlags actorMask, uint32_t wallMask);

// If we want to make P_AimLineAttack capable of handling arbitrary portals, it needs to pass a lot more info than just the linetarget actor.
struct FTranslatedLineTarget
{
	AActor *linetarget;
	DAngle angleFromSource;
	DAngle attackAngleFromSource;
	bool unlinked;	// found by a trace that went through an unlinked portal.
};


void StaticPointerSubstitution(AActor* old, AActor* notOld);

#define S_FREETARGMOBJ	1

#endif // __P_MOBJ_H__

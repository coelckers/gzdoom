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
//		Implements special effects:
//		Texture animation, height or lighting changes
//		 according to adjacent sectors, respective
//		 utility functions, etc.
//		Line Tag handling. Line and Sector triggers.
//		Implements donut linedef triggers
//		Initializes and implements BOOM linedef triggers for
//			Scrollers/Conveyors
//			Friction
//			Wind/Current
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

#include <stdlib.h>

#include "templates.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "g_level.h"
#include "gstrings.h"
#include "events.h"

#include "m_random.h"

#include "p_local.h"
#include "p_spec.h"
#include "p_blockmap.h"
#include "p_lnspec.h"
#include "p_terrain.h"
#include "p_acs.h"
#include "p_3dmidtex.h"

#include "g_game.h"

#include "a_sharedglobal.h"
#include "a_keys.h"
#include "c_dispatch.h"
#include "r_sky.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "vm.h"
#include "p_setup.h"

#include "c_console.h"
#include "p_spec_thinkers.h"

static FRandom pr_playerinspecialsector ("PlayerInSpecialSector");

EXTERN_CVAR(Bool, cl_predict_specials)
EXTERN_CVAR(Bool, forcewater)

// [RH] Check dmflags for noexit and respond accordingly
bool FLevelLocals::CheckIfExitIsGood (AActor *self, level_info_t *info)
{
	cluster_info_t *clusterdef;

	// The world can always exit itself.
	if (self == NULL)
		return true;

	// We must kill all monsters to exit the Level
	if ((dmflags2 & DF2_KILL_MONSTERS) && killed_monsters != total_monsters)
		return false;

	// Is this a deathmatch game and we're not allowed to exit?
	if ((deathmatch || alwaysapplydmflags) && (dmflags & DF_NO_EXIT))
	{
		P_DamageMobj (self, self, self, TELEFRAG_DAMAGE, NAME_Exit);
		return false;
	}
	// Is this a singleplayer game and the next map is part of the same hub and we're dead?
	if (self->health <= 0 &&
		!multiplayer &&
		info != NULL &&
		info->cluster == cluster &&
		(clusterdef = FindClusterInfo(cluster)) != NULL &&
		clusterdef->flags & CLUSTER_HUB)
	{
		return false;
	}
	if (deathmatch && gameaction != ga_completed)
	{
		Printf ("%s exited the level\n", self->player->userinfo.GetName());
	}
	return true;
}


//
// UTILITIES
//

//============================================================================
//
// P_ActivateLine
//
//============================================================================

bool P_ActivateLine (line_t *line, AActor *mo, int side, int activationType, DVector3 *optpos)
{
	int lineActivation;
	INTBOOL repeat;
	INTBOOL buttonSuccess;
	uint8_t special;

	if (!P_TestActivateLine (line, mo, side, activationType, optpos))
	{
		return false;
	}

	auto Level = line->GetLevel();

	// [MK] Use WorldLinePreActivated to decide if activation should continue
	bool shouldactivate = true;
	Level->localEventManager->WorldLinePreActivated(line, mo, activationType, &shouldactivate);
	if ( !shouldactivate ) return false;

	bool remote = (line->special != 7 && line->special != 8 && (line->special < 11 || line->special > 14));
	if (line->locknumber > 0 && !P_CheckKeys (mo, line->locknumber, remote)) return false;

	lineActivation = line->activation;
	repeat = line->flags & ML_REPEAT_SPECIAL;
	buttonSuccess = false;
	buttonSuccess = P_ExecuteSpecial(line->GetLevel(), line->special, line, mo, side == 1, line->args[0], line->args[1], line->args[2], line->args[3], line->args[4]);

	// [MK] Fire up WorldLineActivated
	if ( buttonSuccess ) Level->localEventManager->WorldLineActivated(line, mo, activationType);

	special = line->special;
	if (!repeat && buttonSuccess)
	{ // clear the special on non-retriggerable lines
		line->special = 0;
	}

	if (buttonSuccess)
	{
		if (activationType == SPAC_Use || activationType == SPAC_Impact || activationType == SPAC_Push)
		{
			P_ChangeSwitchTexture (line->sidedef[0], repeat, special);
		}
	}
	// some old WADs use this method to create walls that change the texture when shot.
	else if (activationType == SPAC_Impact &&					// only for shootable triggers
		(Level->flags2 & LEVEL2_DUMMYSWITCHES) &&				// this is only a compatibility setting for an old hack!
		!repeat &&												// only non-repeatable triggers
		(special<Generic_Floor || special>Generic_Crusher) &&	// not for Boom's generalized linedefs
		special &&												// not for lines without a special
		Level->LineHasId(line, line->args[0]) &&							// Safety check: exclude edited UDMF linedefs or ones that don't map the tag to args[0]
		line->args[0] &&										// only if there's a tag (which is stored in the first arg)
		Level->FindFirstSectorFromTag (line->args[0]) == -1)			// only if no sector is tagged to this linedef
	{
		P_ChangeSwitchTexture (line->sidedef[0], repeat, special);
		line->special = 0;
	}
// end of changed code
	if (developer >= DMSG_SPAMMY && buttonSuccess)
	{
		Printf ("Line special %d activated on line %i\n", special, line->Index());
	}
	return true;
}

DEFINE_ACTION_FUNCTION(_Line, Activate)
{
	PARAM_SELF_STRUCT_PROLOGUE(line_t);
	PARAM_POINTER(mo, AActor);
	PARAM_INT(side);
	PARAM_INT(activationType);
	ACTION_RETURN_BOOL(P_ActivateLine(self, mo, side, activationType, NULL));
}

DEFINE_ACTION_FUNCTION(_Line, RemoteActivate)
{
	PARAM_SELF_STRUCT_PROLOGUE(line_t);
	PARAM_POINTER(mo, AActor);
	PARAM_INT(side);
	PARAM_INT(activationType);
	PARAM_FLOAT(optx);
	PARAM_FLOAT(opty);
	PARAM_FLOAT(optz);
	DVector3 optpos = DVector3(optx, opty, optz);
	ACTION_RETURN_BOOL(P_ActivateLine(self, mo, side, activationType, &optpos));
}

//============================================================================
//
// P_TestActivateLine
//
//============================================================================

bool P_TestActivateLine (line_t *line, AActor *mo, int side, int activationType, DVector3 *optpos)
{
	auto Level = line->GetLevel();
 	int lineActivation = line->activation;

	if ((line->flags & ML_FIRSTSIDEONLY && side == 1) || line->special == 0)
	{
		return false;
	}

	if (lineActivation & SPAC_UseThrough)
	{
		lineActivation |= SPAC_Use;
	}
	else if (line->special == Teleport &&
		(lineActivation & SPAC_Cross) &&
		activationType == SPAC_PCross &&
		mo != NULL &&
		mo->flags & MF_MISSILE)
	{ // Let missiles use regular player teleports
		lineActivation |= SPAC_PCross;
	}
	// BOOM's generalized line types that allow monster use can actually be
	// activated by anything except projectiles.
	if (lineActivation & SPAC_AnyCross)
	{
		lineActivation |= SPAC_Cross|SPAC_MCross;
	}
	if (activationType == SPAC_Use || activationType == SPAC_UseBack)
	{
		if (!P_CheckSwitchRange(mo, line, side, optpos))
		{
			return false;
		}
	}

	if (activationType == SPAC_Use && (lineActivation & SPAC_MUse) && !mo->player && mo->flags4 & MF4_CANUSEWALLS)
	{
		return true;
	}
	if (activationType == SPAC_Push && (lineActivation & SPAC_MPush) && !mo->player && mo->flags2 & MF2_PUSHWALL)
	{
		return true;
	}
	if ((lineActivation & activationType) == 0)
	{
		if (activationType != SPAC_MCross || lineActivation != SPAC_Cross)
		{ 
			return false;
		}
	}
	if (activationType == SPAC_AnyCross && (lineActivation & activationType))
	{
		return true;
	}
	if (mo && !mo->player &&
		!(mo->flags & MF_MISSILE) &&
		!(line->flags & ML_MONSTERSCANACTIVATE) &&
		(activationType != SPAC_MCross || (!(lineActivation & SPAC_MCross))))
	{ 
		// [RH] monsters' ability to activate this line depends on its type
		// In Hexen, only MCROSS lines could be activated by monsters. With
		// lax activation checks, monsters can also activate certain lines
		// even without them being marked as monster activate-able. This is
		// the default for non-Hexen maps in Hexen format.
		if (!(Level->flags2 & LEVEL2_LAXMONSTERACTIVATION))
		{
			return false;
		}
		if ((activationType == SPAC_Use || activationType == SPAC_Push)
			&& (line->flags & ML_SECRET))
			return false;		// never open secret doors

		bool noway = true;

		switch (activationType)
		{
		case SPAC_Use:
		case SPAC_Push:
			switch (line->special)
			{
			case Door_Raise:
				if (line->args[0] == 0 && line->args[1] < 64)
					noway = false;
				break;
			case Teleport:
			case Teleport_NoFog:
				noway = false;
			}
			break;

		case SPAC_MCross:
			if (!(lineActivation & SPAC_MCross))
			{
				switch (line->special)
				{
				case Door_Raise:
					if (line->args[1] >= 64)
					{
						break;
					}
					[[fallthrough]];
				case Teleport:
				case Teleport_NoFog:
				case Teleport_Line:
				case Plat_DownWaitUpStayLip:
				case Plat_DownWaitUpStay:
					noway = false;
				}
			}
			else noway = false;
			break;

		default:
			noway = false;
		}
		return !noway;
	}
	if (activationType == SPAC_MCross && !(lineActivation & SPAC_MCross) &&
		!(line->flags & ML_MONSTERSCANACTIVATE))
	{
		return false;
	}
	return true;
}

//============================================================================
//
// P_PredictLine
//
//============================================================================

bool P_PredictLine(line_t *line, AActor *mo, int side, int activationType)
{
	int lineActivation;
	INTBOOL buttonSuccess;
	uint8_t special;

	// Only predict a very specifc section of specials
	if (line->special != Teleport_Line &&
		line->special != Teleport)
	{
		return false;
	}

	if (!P_TestActivateLine(line, mo, side, activationType) || !cl_predict_specials)
	{
		return false;
	}

	if (line->locknumber > 0) return false;
	lineActivation = line->activation;
	buttonSuccess = false;
	buttonSuccess = P_ExecuteSpecial(line->GetLevel(), line->special,line, mo, side == 1, line->args[0], line->args[1], line->args[2], line->args[3], line->args[4]);

	special = line->special;

	// end of changed code
	if (developer >= DMSG_SPAMMY && buttonSuccess)
	{
		Printf("Line special %d predicted on line %i\n", special, line->Index());
	}
	return true;
}

//
// P_PlayerInSpecialSector
// Called every tic frame
//	that the player origin is in a special sector
//
void P_PlayerInSpecialSector (player_t *player, sector_t * sector)
{
	if (sector == NULL)
	{
		// Falling, not all the way down yet?
		sector = player->mo->Sector;
		if (!player->mo->isAtZ(sector->LowestFloorAt(player->mo))
			&& !player->mo->waterlevel)
		{
			return;
		}
	}

	// Has hit ground.

	auto Level = sector->Level;

	// [RH] Apply any customizable damage

	if (sector->damageinterval <= 0)
		sector->damageinterval = 32; // repair invalid damageinterval values

	if (sector->Flags & (SECF_EXIT1 | SECF_EXIT2))
	{
		for (int i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i])
				P_DamageMobj(players[i].mo, nullptr, nullptr, TELEFRAG_DAMAGE, NAME_InstantDeath);
		if (sector->Flags & SECF_EXIT2)
			Level->SecretExitLevel(0);
		else 
			Level->ExitLevel(0, false);
		return;
	}

	if (sector->damageamount > 0)
	{
		// Allow subclasses. Better would be to implement it as armor and let that reduce
		// the damage as part of the normal damage procedure. Unfortunately, I don't have
		// different damage types yet, so that's not happening for now.
		// [MK] account for subclasses that may have "Full" protection (i.e.: prevent leaky damage)
		int ironfeet = 0;
		for (auto i = player->mo->Inventory; i != NULL; i = i->Inventory)
		{
			if (i->IsKindOf(NAME_PowerIronFeet))
			{
				FName mode = i->NameVar(NAME_Mode);
				if ( ironfeet < 2 && mode == NAME_Full )
					ironfeet = 2;
				else if ( ironfeet < 1 && mode == NAME_Normal )
					ironfeet = 1;
			}
		}

		if (sector->Flags & SECF_ENDGODMODE) player->cheats &= ~CF_GODMODE;
		if ((ironfeet == 0 || (ironfeet < 2 && pr_playerinspecialsector() < sector->leakydamage)))
		{
			if (sector->Flags & SECF_HAZARD)
			{
				player->hazardcount += sector->damageamount;
				player->hazardtype = sector->damagetype;
				player->hazardinterval = sector->damageinterval;
			}
			else if (Level->time % sector->damageinterval == 0)
			{
				if (!(player->cheats & (CF_GODMODE | CF_GODMODE2)))
				{
					P_DamageMobj(player->mo, NULL, NULL, sector->damageamount, sector->damagetype);
				}
				if ((sector->Flags & SECF_ENDLEVEL) && player->health <= 10 && (!deathmatch || !(dmflags & DF_NO_EXIT)))
				{
					Level->ExitLevel(0, false);
				}
				if (sector->Flags & SECF_DMGTERRAINFX)
				{
					P_HitWater(player->mo, player->mo->Sector, player->mo->Pos(), false, true, true);
				}
			}
		}
	}
	else if (sector->damageamount < 0)
	{
		if (Level->time % sector->damageinterval == 0)
		{
			P_GiveBody(player->mo, -sector->damageamount, 100);
		}
	}

	if (sector->isSecret())
	{
		sector->ClearSecret();
		P_GiveSecret(Level, player->mo, true, true, sector->Index());
	}
}

//============================================================================
//
// P_SectorDamage
//
//============================================================================

static void DoSectorDamage(AActor *actor, sector_t *sec, int amount, FName type, PClassActor *protectClass, int flags)
{
	if (!(actor->flags & MF_SHOOTABLE))
		return;

	if (!(flags & DAMAGE_NONPLAYERS) && actor->player == NULL)
		return;

	if (!(flags & DAMAGE_PLAYERS) && actor->player != NULL)
		return;

	if (!(flags & DAMAGE_IN_AIR) && !actor->isAtZ(sec->floorplane.ZatPoint(actor)) && !actor->waterlevel)
		return;

	if (protectClass != NULL)
	{
		if (actor->FindInventory(protectClass, !!(flags & DAMAGE_SUBCLASSES_PROTECT)))
			return;
	}

	int dflags = (flags & DAMAGE_NO_ARMOR) ? DMG_NO_ARMOR : 0;
	P_DamageMobj (actor, NULL, NULL, amount, type, dflags);
}

void P_SectorDamage(FLevelLocals *Level, int tag, int amount, FName type, PClassActor *protectClass, int flags)
{
	auto itr = Level->GetSectorTagIterator(tag);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		AActor *actor, *next;
		sector_t *sec = &Level->sectors[secnum];

		// Do for actors in this sector.
		for (actor = sec->thinglist; actor != NULL; actor = next)
		{
			next = actor->snext;
			DoSectorDamage(actor, sec, amount, type, protectClass, flags);
		}
		// If this is a 3D floor control sector, also do for anything in/on
		// those 3D floors.
		for (unsigned i = 0; i < sec->e->XFloor.attached.Size(); ++i)
		{
			sector_t *sec2 = sec->e->XFloor.attached[i];

			for (actor = sec2->thinglist; actor != NULL; actor = next)
			{
				next = actor->snext;
				// Only affect actors touching the 3D floor
				double z1 = sec->floorplane.ZatPoint(actor);
				double z2 = sec->ceilingplane.ZatPoint(actor);
				if (z2 < z1)
				{
					// Account for Vavoom-style 3D floors
					double zz = z1;
					z1 = z2;
					z2 = zz;
				}
				if (actor->Top() > z1)
				{
					// If DAMAGE_IN_AIR is used, anything not beneath the 3D floor will be
					// damaged (so, anything touching it or above it). Other 3D floors between
					// the actor and this one will not stop this effect.
					if ((flags & DAMAGE_IN_AIR) || !actor->isAbove(z2))
					{
						// Here we pass the DAMAGE_IN_AIR flag to disable the floor check, since it
						// only works with the real sector's floor. We did the appropriate height checks
						// for 3D floors already.
						DoSectorDamage(actor, NULL, amount, type, protectClass, flags | DAMAGE_IN_AIR);
					}
				}
			}
		}
	}
}

//============================================================================
//
// P_GiveSecret
//
//============================================================================

CVAR(Bool, showsecretsector, false, 0)
CVAR(Bool, cl_showsecretmessage, true, CVAR_ARCHIVE)

void P_GiveSecret(FLevelLocals *Level, AActor *actor, bool printmessage, bool playsound, int sectornum)
{
	if (actor != NULL)
	{
		if (actor->player != NULL)
		{
			actor->player->secretcount++;
		}
		int retval = 1;
		IFVIRTUALPTR(actor, AActor, OnGiveSecret)
		{
			VMValue params[] = { actor, printmessage, playsound };
			VMReturn ret;
			ret.IntAt(&retval);
			VMCall(func, params, countof(params), &ret, 1);
		}
		if (retval && cl_showsecretmessage && actor->CheckLocalView())
		{
			if (printmessage)
			{
				C_MidPrint(nullptr, GStrings["SECRETMESSAGE"]);
				if (showsecretsector && sectornum >= 0) 
				{
					Printf(PRINT_DEFAULT, PRINTF_NONOTIFY, "Secret found in sector %d\n", sectornum);
				}
			}
			if (playsound) S_Sound (CHAN_AUTO, CHANF_UI, "misc/secret", 1, ATTN_NORM);
		}
	}
	Level->found_secrets++;
}

DEFINE_ACTION_FUNCTION(FLevelLocals, GiveSecret)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_OBJECT(activator, AActor);
	PARAM_BOOL(printmessage);
	PARAM_BOOL(playsound);
	P_GiveSecret(self, activator, printmessage, playsound, -1);
	return 0;
}

//============================================================================
//
// P_PlayerOnSpecialFlat
//
//============================================================================

void P_PlayerOnSpecialFlat (player_t *player, int floorType)
{
	auto Level = player->mo->Level;

	if (Terrains[floorType].DamageAmount &&
		!(Level->time % (Terrains[floorType].DamageTimeMask+1)))
	{
		AActor *ironfeet = NULL;

		if (Terrains[floorType].AllowProtection)
		{
			auto pitype = PClass::FindActor(NAME_PowerIronFeet);
			for (ironfeet = player->mo->Inventory; ironfeet != NULL; ironfeet = ironfeet->Inventory)
			{
				if (ironfeet->IsKindOf (pitype))
					break;
			}
		}

		int damage = 0;
		if (ironfeet == NULL)
		{
			damage = P_DamageMobj (player->mo, NULL, NULL, Terrains[floorType].DamageAmount,
				Terrains[floorType].DamageMOD);
		}
		if (damage > 0 && Terrains[floorType].Splash != -1)
		{
			S_Sound (player->mo, CHAN_AUTO, 0,
				Splashes[Terrains[floorType].Splash].NormalSplashSound, 1,
				ATTN_IDLE);
		}
	}
}



//
// P_UpdateSpecials
// Animate planes, scroll walls, etc.
//
EXTERN_CVAR (Float, timelimit)

void P_UpdateSpecials (FLevelLocals *Level)
{
	// LEVEL TIMER
	if (deathmatch && timelimit)
	{
		if (Level->maptime >= (int)(timelimit * TICRATE * 60))
		{
			Printf ("%s\n", GStrings("TXT_TIMELIMIT"));
			Level->ExitLevel(0, false);
		}
	}
}

////////////////////////////////////////////////////////////////////////////
//
// FRICTION EFFECTS
//
// phares 3/12/98: Start of friction effects

// As the player moves, friction is applied by decreasing the x and y
// velocity values on each tic. By varying the percentage of decrease,
// we can simulate muddy or icy conditions. In mud, the player slows
// down faster. In ice, the player slows down more slowly.
//
// The amount of friction change is controlled by the length of a linedef
// with type 223. A length < 100 gives you mud. A length > 100 gives you ice.
//
// Also, each sector where these effects are to take place is given a
// new special type _______. Changing the type value at runtime allows
// these effects to be turned on or off.
//
// Sector boundaries present problems. The player should experience these
// friction changes only when his feet are touching the sector floor. At
// sector boundaries where floor height changes, the player can find
// himself still 'in' one sector, but with his feet at the floor level
// of the next sector (steps up or down). To handle this, Thinkers are used
// in icy/muddy sectors. These thinkers examine each object that is touching
// their sectors, looking for players whose feet are at the same level as
// their floors. Players satisfying this condition are given new friction
// values that are applied by the player movement code later.

//
// killough 8/28/98:
//
// Completely redid code, which did not need thinkers, and which put a heavy
// drag on CPU. Friction is now a property of sectors, NOT objects inside
// them. All objects, not just players, are affected by it, if they touch
// the sector's floor. Code simpler and faster, only calling on friction
// calculations when an object needs friction considered, instead of doing
// friction calculations on every sector during every tic.
//
// Although this -might- ruin Boom demo sync involving friction, it's the only
// way, short of code explosion, to fix the original design bug. Fixing the
// design bug in Boom's original friction code, while maintaining demo sync
// under every conceivable circumstance, would double or triple code size, and
// would require maintenance of buggy legacy code which is only useful for old
// demos. Doom demos, which are more important IMO, are not affected by this
// change.
//
// [RH] On the other hand, since I've given up on trying to maintain demo
//		sync between versions, these considerations aren't a big deal to me.
//
/////////////////////////////
//
// Initialize the sectors where friction is increased or decreased

void P_SetSectorFriction (FLevelLocals *Level, int tag, int amount, bool alterFlag)
{
	int s;
	double friction, movefactor;

	// An amount of 100 should result in a friction of
	// ORIG_FRICTION (0xE800)
	friction = ((0x1EB8 * amount) / 0x80 + 0xD001) / 65536.;

	// killough 8/28/98: prevent odd situations
	friction = clamp(friction, 0., 1.);

	// The following check might seem odd. At the time of movement,
	// the move distance is multiplied by 'friction/0x10000', so a
	// higher friction value actually means 'less friction'.
	movefactor = FrictionToMoveFactor(friction);

	auto itr = Level->GetSectorTagIterator(tag);
	while ((s = itr.Next()) >= 0)
	{
		// killough 8/28/98:
		//
		// Instead of spawning thinkers, which are slow and expensive,
		// modify the sector's own friction values. Friction should be
		// a property of sectors, not objects which reside inside them.
		// Original code scanned every object in every friction sector
		// on every tic, adjusting its friction, putting unnecessary
		// drag on CPU. New code adjusts friction of sector only once
		// at level startup, and then uses this friction value.

		Level->sectors[s].friction = friction;
		Level->sectors[s].movefactor = movefactor;
		if (alterFlag)
		{
			// When used inside a script, the sectors' friction flags
			// can be enabled and disabled at will.
			if (friction == ORIG_FRICTION)
			{
				Level->sectors[s].Flags &= ~SECF_FRICTION;
			}
			else
			{
				Level->sectors[s].Flags |= SECF_FRICTION;
			}
		}
	}
}

double FrictionToMoveFactor(double friction)
{
	double movefactor;

	// [RH] Twiddled these values so that velocity on ice (with
	//		friction 0xf900) is the same as in Heretic/Hexen.
	if (friction >= ORIG_FRICTION)	// ice
									//movefactor = ((0x10092 - friction)*(0x70))/0x158;
		movefactor = (((0x10092 - friction * 65536) * 1024) / 4352 + 568) / 65536.;
	else
		movefactor = (((friction*65536. - 0xDB34)*(0xA)) / 0x80) / 65536.;

	// killough 8/28/98: prevent odd situations
	if (movefactor < 1 / 2048.)
		movefactor = 1 / 2048.;

	return movefactor;
}

/*
** bbannouncer.cpp
** The announcer from Blood (The Voice).
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
** It's been so long since I played a bloodbath, I don't know when all
** these sounds are used, so much of this usage is me guessing. Some of
** it has also obviously been reused for events that were never present
** in bloodbaths.
**
** I should really have a base Announcer class and derive the Bloodbath
** announcer off of that. That way, multiple announcer styles could be
** supported easily.
*/

// HEADER FILES ------------------------------------------------------------

#include "actor.h"
#include "gstrings.h"
#include "d_player.h"
#include "g_levellocals.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct SoundAndString
{
	const char *Message;
	const char *Sound;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void PronounMessage (const char *from, char *to, FPronouns pronouns,
	const char *victim, const char *killer);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Bool, cl_bbannounce, false, CVAR_ARCHIVE)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *BeginSounds[] =
{
	"VO1.SFX",		// Let the bloodbath begin
	"VO2.SFX",		// The festival of blood continues
};

static const SoundAndString WorldKillSounds[] =
{
	{ "TXT_SELFOBIT1",	"VO7.SFX" },		// Excrement
	{ "TXT_SELFOBIT2",	"VO8.SFX" },		// Hamburger
	{ "TXT_SELFOBIT3",	"VO9.SFX" },		// Scrotum separation
};

static const SoundAndString SuicideSounds[] =
{
	{ "TXT_SELFOBIT5",	"VO13.SFX" },		// Unassisted death
	{ "TXT_SELFOBIT5",	"VO5.SFX" },		// Kevorkian approves
	{ "TXT_SELFOBIT4",	"VO12.SFX" },		// Population control
	{ "TXT_SELFOBIT6",	"VO16.SFX" }		// Darwin award
};

static const SoundAndString KillSounds[] =
{
	{ "TXT_OBITUARY1",	"BONED.SFX" },		// Boned
	{ "TXT_OBITUARY3",	"CREAMED.SFX" },	// Creamed
	{ "TXT_OBITUARY4",	"DECIMAT.SFX" },	// Decimated
	{ "TXT_OBITUARY5",	"DESTRO.SFX" },		// Destroyed
	{ "TXT_OBITUARY6",	"DICED.SFX" },		// Diced
	{ "TXT_OBITUARY7",	"DISEMBO.SFX" },	// Disembowled
	{ "TXT_OBITUARY8",	"FLATTE.SFX" },		// Flattened
	{ "TXT_OBITUARY9",	"JUSTICE.SFX" },	// Justice
	{ "TXT_OBITUARY10",	"MADNESS.SFX" },	// Madness
	{ "TXT_OBITUARY11",	"KILLED.SFX" },		// Killed
	{ "TXT_OBITUARY12",	"MINCMEAT.SFX" },	// Mincemeat
	{ "TXT_OBITUARY13",	"MASSACR.SFX" },	// Massacred
	{ "TXT_OBITUARY14",	"MUTILA.SFX" },		// Mutilated
	{ "TXT_OBITUARY15",	"REAMED.SFX" },		// Reamed
	{ "TXT_OBITUARY16",	"RIPPED.SFX" },		// Ripped
	{ "TXT_OBITUARY17",	"SLAUGHT.SFX" },	// Slaughtered
	{ "TXT_OBITUARY18",	"SMASHED.SFX" },	// Smashed
	{ "TXT_OBITUARY19",	"SODOMIZ.SFX" },	// Sodomized
	{ "TXT_OBITUARY20",	"SPLATT.SFX" },		// Splattered
	{ "TXT_OBITUARY21",	"SQUASH.SFX" },		// Squashed
	{ "TXT_OBITUARY22",	"THROTTL.SFX" },	// Throttled
	{ "TXT_OBITUARY23",	"WASTED.SFX" },		// Wasted
	{ "TXT_OBITUARY24",	"VO10.SFX" },		// Body bagged
	{ "TXT_OBITUARY28",	"VO25.SFX" },		// Hosed
	{ "TXT_OBITUARY26",	"VO27.SFX" },		// Toasted
	{ "TXT_OBITUARY25",	"VO28.SFX" },		// Sent to hell
	{ "TXT_OBITUARY29",	"VO35.SFX" },		// Sprayed
	{ "TXT_OBITUARY30",	"VO36.SFX" },		// Dog meat
	{ "TXT_OBITUARY31",	"VO39.SFX" },		// Beaten like a cur
	{ "TXT_OBITUARY27",	"VO41.SFX" },		// Snuffed
	{ "TXT_OBITUARY2",	"CASTRA.SFX" },		// Castrated
};

static const char *GoodJobSounds[] =
{
	"VO22.SFX",		// Fine work
	"VO23.SFX",		// Well done
	"VO44.SFX",		// Excellent
};

static const char *TooBadSounds[] =
{
	"VO17.SFX",		// Go play Mario
	"VO18.SFX",		// Need a tricycle?
	"VO37.SFX",		// Bye bye now
};

static const char *TelefragSounds[] =
{
	"VO29.SFX",		// Pass the jelly
	"VO34.SFX",		// Spillage
	"VO40.SFX",		// Whipped and creamed
	"VO42.SFX",		// Spleen vented
	"VO43.SFX",		// Vaporized
	"VO38.SFX",		// Ripped him loose
	"VO14.SFX",		// Shat upon
};

#if 0	// Sounds I don't know what to do with
	"VO6.SFX",		// Asshole
	"VO15.SFX",		// Finish him
	"VO19.SFX",		// Talented
	"VO20.SFX",		// Good one
	"VO21.SFX",		// Lunch meat
	"VO26.SFX",		// Humiliated
	"VO30.SFX",		// Punishment delivered
	"VO31.SFX",		// Bobbit-ized
	"VO32.SFX",		// Stiffed
	"VO33.SFX",		// He shoots... He scores
#endif

static int LastAnnounceTime;
static FRandom pr_bbannounce ("BBAnnounce");

// CODE --------------------------------------------------------------------

//==========================================================================
//
// DoVoiceAnnounce
//
//==========================================================================

void DoVoiceAnnounce (const char *sound)
{
	// Don't play announcements too close together
	if (LastAnnounceTime == 0 || LastAnnounceTime <= primaryLevel->time-5)
	{
		LastAnnounceTime = primaryLevel->time;
		S_Sound (CHAN_VOICE, 0, sound, 1, ATTN_NONE);
	}
}

//==========================================================================
//
// AnnounceGameStart
//
// Called when a new map is entered.
//
//==========================================================================

bool AnnounceGameStart ()
{
	LastAnnounceTime = 0;
	if (cl_bbannounce && deathmatch)
	{
		DoVoiceAnnounce (BeginSounds[pr_bbannounce() & 1]);
	}
	return false;
}

//==========================================================================
//
// AnnounceKill
//
// Called when somebody dies.
//
//==========================================================================

bool AnnounceKill (AActor *killer, AActor *killee)
{
	const char *killerName;
	const SoundAndString *choice;
	const char *message;
	int rannum = pr_bbannounce();

	if (cl_bbannounce && deathmatch)
	{
		bool playSound = killee->CheckLocalView();

		if (killer == NULL)
		{ // The world killed the player
			if (killee->player->userinfo.GetGender() == GENDER_MALE)
			{ // Only males have scrotums to separate
				choice = &WorldKillSounds[rannum % 3];
			}
			else
			{
				choice = &WorldKillSounds[rannum & 1];
			}
			killerName = NULL;
		}
		else if (killer == killee)
		{ // The player killed self
			choice = &SuicideSounds[rannum & 3];
			killerName = killer->player->userinfo.GetName();
		}
		else
		{ // Another player did the killing
			if (killee->player->userinfo.GetGender() == GENDER_MALE)
			{ // Only males can be castrated
				choice = &KillSounds[rannum % countof(KillSounds)];
			}
			else
			{
				choice = &KillSounds[rannum % (countof(KillSounds) - 1)];
			}
			killerName = killer->player->userinfo.GetName();

			// Blood only plays the announcement sound on the killer's
			// computer. I think it sounds neater to also hear it on
			// the killee's machine.
			playSound |= killer->CheckLocalView();
		}

		message = GStrings(choice->Message);
		if (message != NULL)
		{
			char assembled[1024];

			PronounMessage (message, assembled, killee->player->userinfo.GetPronouns(),
				killee->player->userinfo.GetName(), killerName);
			Printf (PRINT_MEDIUM, "%s\n", assembled);
		}
		if (playSound)
		{
			DoVoiceAnnounce (choice->Sound);
		}
		return message != NULL;
	}
	return false;
}

//==========================================================================
//
// AnnounceTelefrag
//
// Called when somebody dies by telefragging.
//
//==========================================================================

bool AnnounceTelefrag (AActor *killer, AActor *killee)
{
	int rannum = pr_bbannounce();

	if (cl_bbannounce && multiplayer)
	{
		const char *message = GStrings("OB_MPTELEFRAG");
		if (message != NULL)
		{
			char assembled[1024];

			PronounMessage (message, assembled, killee->player->userinfo.GetPronouns(),
				killee->player->userinfo.GetName(), killer->player->userinfo.GetName());
			Printf (PRINT_MEDIUM, "%s\n", assembled);
		}
		if (killee->CheckLocalView() ||
			killer->CheckLocalView())
		{
			DoVoiceAnnounce (TelefragSounds[rannum % 7]);
		}
		return message != NULL;
	}
	return false;
}

//==========================================================================
//
// AnnounceSpree
//
// Called when somebody is on a spree.
//
//==========================================================================

bool AnnounceSpree (AActor *who)
{
	return false;
}

//==========================================================================
//
// AnnounceSpreeLoss
//
// Called when somebody on a spree gets killed.
//
//==========================================================================

bool AnnounceSpreeLoss (AActor *who)
{
	if (cl_bbannounce)
	{
		if (who->CheckLocalView())
		{
			DoVoiceAnnounce (TooBadSounds[M_Random() % 3]);
		}
	}
	return false;
}

//==========================================================================
//
// AnnounceMultikill
//
// Called when somebody is quickly raking in kills.
//
//==========================================================================

bool AnnounceMultikill (AActor *who)
{
	if (cl_bbannounce)
	{
		if (who->CheckLocalView())
		{
			DoVoiceAnnounce (GoodJobSounds[M_Random() % 3]);
		}
	}
	return false;
}

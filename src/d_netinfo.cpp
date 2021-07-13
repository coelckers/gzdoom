/*
** d_netinfo.cpp
** Manages transport of user and "server" cvars across a network
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
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_netinf.h"
#include "d_net.h"
#include "d_player.h"
#include "c_dispatch.h"
#include "r_state.h"
#include "sbar.h"
#include "teaminfo.h"
#include "cmdlib.h"
#include "serializer.h"
#include "vm.h"
#include "gstrings.h"
#include "g_game.h"

static FRandom pr_pickteam ("PickRandomTeam");

CVAR (Float,	autoaim,				35.f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	name,					"Player",	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Color,	color,					0x40cf00,	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Int,		colorset,				0,			CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	skin,					"base",		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Int,		team,					TEAM_NONE,	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	gender,					"male",		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	pronouns,				"they",		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Bool,		neverswitchonpickup,	false,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Float,	movebob,				0.25f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Float,	stillbob,				0.f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Float,	wbobspeed,				1.f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Float,	wbobfire,				0.f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	playerclass,			"Fighter",	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Bool,		classicflight,			false,		CVAR_USERINFO | CVAR_ARCHIVE);

enum
{
	INFO_Name,
	INFO_Autoaim,
	INFO_Color,
	INFO_Skin,
	INFO_Team,
	INFO_Gender,
	INFO_Pronouns,
	INFO_NeverSwitchOnPickup,
	INFO_MoveBob,
	INFO_StillBob,
	INFO_WBobSpeed,
	INFO_WBobFire,
	INFO_PlayerClass,
	INFO_ColorSet,
	INFO_ClassicFlight,
};

const char *GenderNames[GENDER_MAX] = { "male", "female", "neutral", "other" };

// Replace \ with %/ and % with %%
FString D_EscapeUserInfo (const char *str)
{
	FString ret;

	for (; *str != '\0'; ++str)
	{
		if (*str == '\\')
		{
			ret << '%' << '/';
		}
		else if (*str == '%')
		{
			ret << '%' << '%';
		}
		else
		{
			ret << *str;
		}
	}
	return ret;
}

// Replace %/ with \ and %% with %
FString D_UnescapeUserInfo (const char *str, size_t len)
{
	const char *end = str + len;
	FString ret;

	while (*str != '\0' && str < end)
	{
		if (*str == '%')
		{
			if (*(str + 1) == '/')
			{
				ret << '\\';
				str += 2;
				continue;
			}
			else if (*(str + 1) == '%')
			{
				str++;
			}
		}
		ret << *str++;
	}
	return ret;
}

int D_GenderToInt (const char *gender)
{
	if (!stricmp(gender, "female")) return GENDER_FEMALE;
	if (!stricmp(gender, "neutral")) return GENDER_NEUTER;
	if (!stricmp(gender, "neuter")) return GENDER_NEUTER;
	if (!stricmp(gender, "other")) return GENDER_OBJECT;
	if (!stricmp(gender, "object")) return GENDER_OBJECT;
	if (!stricmp(gender, "cyborg")) return GENDER_OBJECT;
	return GENDER_MALE;
}

int D_PlayerClassToInt (const char *classname)
{
	if (PlayerClasses.Size () > 1)
	{
		for (unsigned int i = 0; i < PlayerClasses.Size (); ++i)
		{
			auto type = PlayerClasses[i].Type;

			if (type->GetDisplayName().IsNotEmpty() && stricmp(type->GetDisplayName(), classname) == 0)
			{
				return i;
			}
		}
		return -1;
	}
	else
	{
		return 0;
	}
}

void D_GetPlayerColor (int player, float *h, float *s, float *v, FPlayerColorSet **set)
{
	userinfo_t *info = &players[player].userinfo;
	FPlayerColorSet *colorset = NULL;
	uint32_t color;
	int team;

	if (players[player].mo != NULL)
	{
		colorset = GetColorSet(players[player].mo->GetClass(), info->GetColorSet());
	}
	if (colorset != NULL)
	{
		color = GPalette.BaseColors[GPalette.Remap[colorset->RepresentativeColor]];
	}
	else
	{
		color = info->GetColor();
	}

	RGBtoHSV (RPART(color)/255.f, GPART(color)/255.f, BPART(color)/255.f,
		h, s, v);

	if (teamplay && TeamLibrary.IsValidTeam((team = info->GetTeam())) && !Teams[team].GetAllowCustomPlayerColor())
	{
		// In team play, force the player to use the team's hue
		// and adjust the saturation and value so that the team
		// hue is visible in the final color.
		float ts, tv;
		int tcolor = Teams[team].GetPlayerColor ();

		RGBtoHSV (RPART(tcolor)/255.f, GPART(tcolor)/255.f, BPART(tcolor)/255.f,
			h, &ts, &tv);

		*s = clamp(ts + *s * 0.15f - 0.075f, 0.f, 1.f);
		*v = clamp(tv + *v * 0.5f - 0.25f, 0.f, 1.f);

		// Make sure not to pass back any colorset in teamplay.
		colorset = NULL;
	}
	if (set != NULL)
	{
		*set = colorset;
	}
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetDisplayColor)
{
	float h, s, v, r, g, b;
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	D_GetPlayerColor(int(self-players), &h, &s, &v, NULL);
	HSVtoRGB(&r, &g, &b, h, s, v);
	int c = MAKERGB(clamp(int(r*255.f), 0, 255),
		clamp(int(g*255.f), 0, 255),
		clamp(int(b*255.f), 0, 255));
	ACTION_RETURN_INT(c);
}

// Find out which teams are present. If there is only one,
// then another team should be chosen at random.
//
// Otherwise, join whichever team has fewest players. If
// teams are tied for fewest players, pick one of those
// at random.

void D_PickRandomTeam (int player)
{
	static char teamline[8] = "\\team\\X";

	uint8_t *foo = (uint8_t *)teamline;
	teamline[6] = (char)D_PickRandomTeam() + '0';
	D_ReadUserInfoStrings (player, &foo, teamplay);
}

int D_PickRandomTeam ()
{
	for (unsigned int i = 0; i < Teams.Size (); i++)
	{
		Teams[i].m_iPresent = 0;
		Teams[i].m_iTies = 0;
	}

	int numTeams = 0;
	int team;

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			team = players[i].userinfo.GetTeam();
			if (TeamLibrary.IsValidTeam(team))
			{
				if (Teams[team].m_iPresent++ == 0)
				{
					numTeams++;
				}
			}
		}
	}

	if (numTeams < 2)
	{
		do
		{
			team = pr_pickteam() % Teams.Size();
		} while (Teams[team].m_iPresent != 0);
	}
	else
	{
		int lowest = INT_MAX, lowestTie = 0;
		unsigned int i;

		for (i = 0; i < Teams.Size (); ++i)
		{
			if (Teams[i].m_iPresent > 0)
			{
				if (Teams[i].m_iPresent < lowest)
				{
					lowest = Teams[i].m_iPresent;
					lowestTie = 0;
					Teams[0].m_iTies = i;
				}
				else if (Teams[i].m_iPresent == lowest)
				{
					Teams[++lowestTie].m_iTies = i;
				}
			}
		}
		if (lowestTie == 0)
		{
			team = Teams[0].m_iTies;
		}
		else
		{
			team = Teams[pr_pickteam() % (lowestTie+1)].m_iTies;
		}
	}

	return team;
}

static void UpdateTeam (int pnum, int team, bool update)
{
	userinfo_t *info = &players[pnum].userinfo;

	if ((dmflags2 & DF2_NO_TEAM_SWITCH) && (alwaysapplydmflags || deathmatch) && TeamLibrary.IsValidTeam (info->GetTeam()))
	{
		Printf ("%s\n", GStrings("TXT_NO_TEAM_CHANGE"));
		return;
	}

	int oldteam;

	if (!TeamLibrary.IsValidTeam (team))
	{
		team = TEAM_NONE;
	}
	oldteam = info->GetTeam();
	team = info->TeamChanged(team);

	if (update && oldteam != team)
	{
		FString message;
		if (TeamLibrary.IsValidTeam (team))
		{
			message = GStrings("TXT_JOINED_TEAM");
			message.Substitute("%t", Teams[team].GetName());
		}
		else
		{
			message = GStrings("TXT_LONER");
		}
		message.Substitute("%s", info->GetName());
		Printf("%s\n", message.GetChars());
	}
	// Let the player take on the team's color
	R_BuildPlayerTranslation (pnum);
	if (StatusBar != NULL && StatusBar->GetPlayer() == pnum)
	{
		StatusBar->AttachToPlayer (&players[pnum]);
	}
	// Double-check
	if (!TeamLibrary.IsValidTeam (team))
	{
		*static_cast<FIntCVar *>((*info)[NAME_Team]) = TEAM_NONE;
	}
}

int D_GetFragCount (player_t *player)
{
	const int team = player->userinfo.GetTeam();
	if (!teamplay || !TeamLibrary.IsValidTeam(team))
	{
		return player->fragcount;
	}
	else
	{
		// Count total frags for this player's team
		int count = 0;

		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].userinfo.GetTeam() == team)
			{
				count += players[i].fragcount;
			}
		}
		return count;
	}
}

void D_SetupUserInfo ()
{
	int i;
	userinfo_t *coninfo;

	// Reset everybody's userinfo to a default state.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		players[i].userinfo.Reset();
	}
	// Initialize the console player's user info
	coninfo = &players[consoleplayer].userinfo;

	for (FBaseCVar *cvar = CVars; cvar != NULL; cvar = cvar->GetNext())
	{
		if ((cvar->GetFlags() & (CVAR_USERINFO|CVAR_IGNORE)) == CVAR_USERINFO)
		{
			FBaseCVar **newcvar;
			FName cvarname(cvar->GetName());

			switch (cvarname.GetIndex())
			{
			// Some cvars don't copy their original value directly.
			case NAME_Team:			coninfo->TeamChanged(team); break;
			case NAME_Skin:			coninfo->SkinChanged(skin, players[consoleplayer].CurrentPlayerClass); break;
			case NAME_Gender:		coninfo->GenderChanged(gender); break;
			case NAME_PlayerClass:	coninfo->PlayerClassChanged(playerclass); break;
			// The rest do.
			default:
				newcvar = coninfo->CheckKey(cvarname);
				(*newcvar)->SetGenericRep(cvar->GetGenericRep(CVAR_String), CVAR_String);
				break;
			}
		}
	}
	R_BuildPlayerTranslation(consoleplayer);
}

void userinfo_t::Reset()
{
	// Clear this player's userinfo.
	TMapIterator<FName, FBaseCVar *> it(*this);
	TMap<FName, FBaseCVar *>::Pair *pair;

	while (it.NextPair(pair))
	{
		delete pair->Value;
	}
	Clear();

	// Create userinfo vars for this player, initialized to their defaults.
	for (FBaseCVar *cvar = CVars; cvar != NULL; cvar = cvar->GetNext())
	{
		if ((cvar->GetFlags() & (CVAR_USERINFO|CVAR_IGNORE)) == CVAR_USERINFO)
		{
			ECVarType type;
			FName cvarname(cvar->GetName());
			FBaseCVar *newcvar;

			// Some cvars have different types for their shadow copies.
			switch (cvarname.GetIndex())
			{
			case NAME_Skin:			type = CVAR_Int; break;
			case NAME_Gender:		type = CVAR_Int; break;
			case NAME_PlayerClass:	type = CVAR_Int; break;
			default:				type = cvar->GetRealType(); break;
			}
			newcvar = C_CreateCVar(NULL, type, cvar->GetFlags() & CVAR_MOD);
			newcvar->SetGenericRepDefault(cvar->GetGenericRepDefault(CVAR_String), CVAR_String);
			Insert(cvarname, newcvar);
		}
	}
}

int userinfo_t::TeamChanged(int team)
{
	if (teamplay && !TeamLibrary.IsValidTeam(team))
	{ // Force players onto teams in teamplay mode
		team = D_PickRandomTeam();
	}
	*static_cast<FIntCVar *>((*this)[NAME_Team]) = team;
	return team;
}

int userinfo_t::SkinChanged(const char *skinname, int playerclass)
{
	int skinnum = R_FindSkin(skinname, playerclass);
	*static_cast<FIntCVar *>((*this)[NAME_Skin]) = skinnum;
	return skinnum;
}

int userinfo_t::SkinNumChanged(int skinnum)
{
	*static_cast<FIntCVar *>((*this)[NAME_Skin]) = skinnum;
	return skinnum;
}

int userinfo_t::GenderChanged(const char *gendername)
{
	int gendernum = D_GenderToInt(gendername);
	*static_cast<FIntCVar *>((*this)[NAME_Gender]) = gendernum;
	return gendernum;
}

int userinfo_t::PlayerClassChanged(const char *classname)
{
	int classnum = D_PlayerClassToInt(classname);
	*static_cast<FIntCVar *>((*this)[NAME_PlayerClass]) = classnum;
	return classnum;
}

int userinfo_t::ColorSetChanged(int setnum)
{
	*static_cast<FIntCVar *>((*this)[NAME_ColorSet]) = setnum;
	return setnum;
}

uint32_t userinfo_t::ColorChanged(const char *colorname)
{
	FColorCVar *color = static_cast<FColorCVar *>((*this)[NAME_Color]);
	assert(color != NULL);
	UCVarValue val;
	val.String = const_cast<char *>(colorname);
	color->SetGenericRep(val, CVAR_String);
	*static_cast<FIntCVar *>((*this)[NAME_ColorSet]) = -1;
	return *color;
}

uint32_t userinfo_t::ColorChanged(uint32_t colorval)
{
	FColorCVar *color = static_cast<FColorCVar *>((*this)[NAME_Color]);
	assert(color != NULL);
	UCVarValue val;
	val.Int = colorval;
	color->SetGenericRep(val, CVAR_Int);
	// This version is called by the menu code. Do not implicitly set colorset.
	return colorval;
}

void D_UserInfoChanged (FBaseCVar *cvar)
{
	UCVarValue val;
	FString escaped_val;
	char foo[256];

	if (cvar == &autoaim)
	{
		if (autoaim < 0.0f)
		{
			autoaim = 0.0f;
			return;
		}
		else if (autoaim > 35.0f)
		{
			autoaim = 35.f;
			return;
		}
	}

	val = cvar->GetGenericRep (CVAR_String);
	escaped_val = D_EscapeUserInfo(val.String);
	if (4 + strlen(cvar->GetName()) + escaped_val.Len() > 256)
		I_Error ("User info descriptor too big");

	mysnprintf (foo, countof(foo), "\\%s\\%s", cvar->GetName(), escaped_val.GetChars());

	Net_WriteByte (DEM_UINFCHANGED);
	Net_WriteString (foo);
}

static const char *SetServerVar (char *name, ECVarType type, uint8_t **stream, bool singlebit)
{
	FBaseCVar *var = FindCVar (name, NULL);
	UCVarValue value;

	if (singlebit)
	{
		if (var != NULL)
		{
			int bitdata;
			int mask;

			value = var->GetFavoriteRep (&type);
			if (type != CVAR_Int)
			{
				return NULL;
			}
			bitdata = ReadByte (stream);
			mask = 1 << (bitdata & 31);
			if (bitdata & 32)
			{
				value.Int |= mask;
			}
			else
			{
				value.Int &= ~mask;
			}
		}
	}
	else
	{
		switch (type)
		{
		case CVAR_Bool:		value.Bool = ReadByte (stream) ? 1 : 0;	break;
		case CVAR_Int:		value.Int = ReadLong (stream);			break;
		case CVAR_Float:	value.Float = ReadFloat (stream);		break;
		case CVAR_String:	value.String = ReadString (stream);		break;
		default: break;	// Silence GCC
		}
	}

	if (var)
	{
		var->ForceSet (value, type);
	}

	if (type == CVAR_String)
	{
		delete[] value.String;
	}

	if (var == &teamplay)
	{
		// Put players on teams if teamplay turned on
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				UpdateTeam (i, players[i].userinfo.GetTeam(), true);
			}
		}
	}

	if (var)
	{
		value = var->GetGenericRep (CVAR_String);
		return value.String;
	}

	return NULL;
}

EXTERN_CVAR (Float, sv_gravity)

bool D_SendServerInfoChange (FBaseCVar *cvar, UCVarValue value, ECVarType type)
{
	if (gamestate != GS_STARTUP && !demoplayback && !savegamerestore)
	{
		if (netgame && !players[consoleplayer].settings_controller)
		{
			Printf("Only setting controllers can change server CVAR %s\n", cvar->GetName());
			cvar->MarkSafe();
			return true;
		}
		size_t namelen;

		namelen = strlen(cvar->GetName());

		Net_WriteByte(DEM_SINFCHANGED);
		Net_WriteByte((uint8_t)(namelen | (type << 6)));
		Net_WriteBytes((uint8_t*)cvar->GetName(), (int)namelen);
		switch (type)
		{
		case CVAR_Bool:		Net_WriteByte(value.Bool);		break;
		case CVAR_Int:		Net_WriteLong(value.Int);		break;
		case CVAR_Float:	Net_WriteFloat(value.Float);	break;
		case CVAR_String:	Net_WriteString(value.String);	break;
		default: break; // Silence GCC
		}
		return true;
	}
	return false;
}

bool D_SendServerFlagChange (FBaseCVar *cvar, int bitnum, bool set, bool silent)
{
	if (gamestate != GS_STARTUP && !demoplayback && !savegamerestore)
	{
		if (netgame && !players[consoleplayer].settings_controller)
		{
			if (!silent)
			{
				Printf("Only setting controllers can change server CVAR %s\n", cvar->GetName());
			}
			return true;
		}

		int namelen = (int)strlen(cvar->GetName());

		Net_WriteByte(DEM_SINFCHANGEDXOR);
		Net_WriteByte((uint8_t)namelen);
		Net_WriteBytes((uint8_t*)cvar->GetName(), namelen);
		Net_WriteByte(uint8_t(bitnum | (set << 5)));
		return true;
	}
	return false;
}

void D_DoServerInfoChange (uint8_t **stream, bool singlebit)
{
	const char *value;
	char name[64];
	int len;
	int type;

	len = ReadByte (stream);
	type = len >> 6;
	len &= 0x3f;
	if (len == 0)
		return;
	memcpy (name, *stream, len);
	*stream += len;
	name[len] = 0;

	if ( (value = SetServerVar (name, (ECVarType)type, stream, singlebit)) && netgame)
	{
		Printf ("%s changed to %s\n", name, value);
	}
}

static int userinfosortfunc(const void *a, const void *b)
{
	TMap<FName, FBaseCVar *>::ConstPair *pair1 = *(TMap<FName, FBaseCVar *>::ConstPair **)a;
	TMap<FName, FBaseCVar *>::ConstPair *pair2 = *(TMap<FName, FBaseCVar *>::ConstPair **)b;
	return stricmp(pair1->Key.GetChars(), pair2->Key.GetChars());
}

static int namesortfunc(const void *a, const void *b)
{
	FName *name1 = (FName *)a;
	FName *name2 = (FName *)b;
	return stricmp(name1->GetChars(), name2->GetChars());
}

void D_WriteUserInfoStrings (int pnum, uint8_t **stream, bool compact)
{
	if (pnum >= MAXPLAYERS)
	{
		WriteByte (0, stream);
		return;
	}

	userinfo_t *info = &players[pnum].userinfo;
	TArray<TMap<FName, FBaseCVar *>::Pair *> userinfo_pairs(info->CountUsed());
	TMap<FName, FBaseCVar *>::Iterator it(*info);
	TMap<FName, FBaseCVar *>::Pair *pair;
	UCVarValue cval;

	// Create a simple array of all userinfo cvars
	while (it.NextPair(pair))
	{
		userinfo_pairs.Push(pair);
	}
	// For compact mode, these need to be sorted. Verbose mode doesn't matter.
	if (compact)
	{
		qsort(&userinfo_pairs[0], userinfo_pairs.Size(), sizeof(pair), userinfosortfunc);
		// Compact mode is signified by starting the string with two backslash characters.
		// We output one now. The second will be output as part of the first value.
		*(*stream)++ = '\\';
	}
	for (unsigned int i = 0; i < userinfo_pairs.Size(); ++i)
	{
		pair = userinfo_pairs[i];

		if (!compact)
		{ // In verbose mode, prepend the cvar's name
			*stream += sprintf(*((char **)stream), "\\%s", pair->Key.GetChars());
		}
		// A few of these need special handling for compatibility reasons.
		switch (pair->Key.GetIndex())
		{
		case NAME_Gender:
			*stream += sprintf(*((char **)stream), "\\%s",
				*static_cast<FIntCVar *>(pair->Value) == GENDER_FEMALE ? "female" :
				*static_cast<FIntCVar *>(pair->Value) == GENDER_NEUTER ? "neutral" :
				*static_cast<FIntCVar *>(pair->Value) == GENDER_OBJECT ? "other" : "male");
			break;

		case NAME_PlayerClass:
			*stream += sprintf(*((char **)stream), "\\%s", info->GetPlayerClassNum() == -1 ? "Random" :
				D_EscapeUserInfo(info->GetPlayerClassType()->GetDisplayName().GetChars()).GetChars());
			break;

		case NAME_Skin:
			*stream += sprintf(*((char **)stream), "\\%s", D_EscapeUserInfo(Skins[info->GetSkin()].Name).GetChars());
			break;

		default:
			cval = pair->Value->GetGenericRep(CVAR_String);
			*stream += sprintf(*((char **)stream), "\\%s", cval.String);
			break;
		}
	}
	*(*stream)++ = '\0';
}

void D_ReadUserInfoStrings (int pnum, uint8_t **stream, bool update)
{
	userinfo_t *info = &players[pnum].userinfo;
	TArray<FName> compact_names(info->CountUsed());
	FBaseCVar **cvar_ptr;
	const char *ptr = *((const char **)stream);
	const char *breakpt;
	FString value;
	bool compact;
	FName keyname = NAME_None;
	unsigned int infotype = 0;

	if (*ptr++ != '\\')
		return;

	compact = (*ptr == '\\') ? ptr++, true : false;

	// We need the cvar names in sorted order for compact mode
	if (compact)
	{
		TMap<FName, FBaseCVar *>::Iterator it(*info);
		TMap<FName, FBaseCVar *>::Pair *pair;

		while (it.NextPair(pair))
		{
			compact_names.Push(pair->Key);
		}
		qsort(&compact_names[0], compact_names.Size(), sizeof(FName), namesortfunc);
	}

	if (pnum < MAXPLAYERS)
	{
		for (breakpt = ptr; breakpt != NULL; ptr = breakpt + 1)
		{
			breakpt = strchr(ptr, '\\');

			if (compact)
			{
				// Compact has just the value.
				if (infotype >= compact_names.Size())
				{ // Too many entries! OMG!
					break;
				}
				keyname = compact_names[infotype++];
				value = D_UnescapeUserInfo(ptr, breakpt != NULL ? breakpt - ptr : strlen(ptr));
			}
			else
			{
				// Verbose has both the key name and its value.
				assert(breakpt != NULL);
				// A malicious remote machine could invalidate the above assert.
				if (breakpt == NULL)
				{
					break;
				}
				const char *valstart = breakpt + 1;
				if ( (breakpt = strchr (valstart, '\\')) != NULL )
				{
					value = D_UnescapeUserInfo(valstart, breakpt - valstart);
				}
				else
				{
					value = D_UnescapeUserInfo(valstart, strlen(valstart));
				}
				keyname = FName(ptr, valstart - ptr - 1, true);
			}
			
			// A few of these need special handling.
			switch (keyname.GetIndex())
			{
			case NAME_Gender:
				info->GenderChanged(value);
				break;

			case NAME_PlayerClass:
				info->PlayerClassChanged(value);
				break;

			case NAME_Skin:
				info->SkinChanged(value, players[pnum].CurrentPlayerClass);
				if (players[pnum].mo != NULL)
				{
					if (players[pnum].cls != NULL &&
						!(players[pnum].mo->flags4 & MF4_NOSKIN) &&
						players[pnum].mo->state->sprite ==
						GetDefaultByType (players[pnum].cls)->SpawnState->sprite)
					{ // Only change the sprite if the player is using a standard one
						players[pnum].mo->sprite = Skins[info->GetSkin()].sprite;
					}
				}
				// Rebuild translation in case the new skin uses a different range
				// than the old one.
				R_BuildPlayerTranslation(pnum);
				break;

			case NAME_Team:
				UpdateTeam(pnum, atoi(value), update);
				break;

			case NAME_Color:
				info->ColorChanged(value);
				break;

			default:
				cvar_ptr = info->CheckKey(keyname);
				if (cvar_ptr != NULL)
				{
					assert(*cvar_ptr != NULL);
					UCVarValue val;
					FString oldname;

					if (keyname == NAME_Name)
					{
						val = (*cvar_ptr)->GetGenericRep(CVAR_String);
						oldname = val.String;
					}
					val.String = CleanseString(value.LockBuffer());
					(*cvar_ptr)->SetGenericRep(val, CVAR_String);
					value.UnlockBuffer();
					if (keyname == NAME_Name && update && oldname.Compare (value))
					{
						Printf("%s is now known as %s\n", oldname.GetChars(), value.GetChars());
					}
				}
				break;
			}
			if (keyname == NAME_Color || keyname == NAME_ColorSet)
			{
				R_BuildPlayerTranslation(pnum);
				if (StatusBar != NULL && pnum == StatusBar->GetPlayer())
				{
					StatusBar->AttachToPlayer(&players[pnum]);
				}
			}
		}
	}
	*stream += strlen (*((char **)stream)) + 1;
}

void WriteUserInfo(FSerializer &arc, userinfo_t &info)
{
	if (arc.BeginObject("userinfo"))
	{
		TMapIterator<FName, FBaseCVar *> it(info);
		TMap<FName, FBaseCVar *>::Pair *pair;
		FString name;
		const char *string;
		UCVarValue val;
		int i;

		while (it.NextPair(pair))
		{
			name = pair->Key.GetChars();
			name.ToLower();
			switch (pair->Key.GetIndex())
			{
			case NAME_Skin:
				string = Skins[info.GetSkin()].Name;
				break;

			case NAME_PlayerClass:
				i = info.GetPlayerClassNum();
				string = (i == -1 ? "Random" : PlayerClasses[i].Type->GetDisplayName().GetChars());
				break;

			default:
				val = pair->Value->GetGenericRep(CVAR_String);
				string = val.String;
				break;
			}
			arc.StringPtr(name, string);
		}
		arc.EndObject();
	}
}

void ReadUserInfo(FSerializer &arc, userinfo_t &info, FString &skin)
{
	UCVarValue val;
	const char *key;
	const char *str;

	info.Reset();
	skin = "";
	if (arc.BeginObject("userinfo"))
	{
		while ((key = arc.GetKey()))
		{
			arc.StringPtr(nullptr, str);
			FName name = key;
			FBaseCVar **cvar = info.CheckKey(name);
			if (cvar != NULL && *cvar != NULL)
			{
				switch (name.GetIndex())
				{
				case NAME_Team:			info.TeamChanged(atoi(str)); break;
				case NAME_Skin:			skin = str; break;	// Caller must call SkinChanged() once current calss is known
				case NAME_PlayerClass:	info.PlayerClassChanged(str); break;
				default:
					val.String = str;
					(*cvar)->SetGenericRep(val, CVAR_String);
					break;
				}
			}
		}
		arc.EndObject();
	}
}

CCMD (playerinfo)
{
	if (argv.argc() < 2)
	{
		int i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i])
			{
				Printf("%d. %s\n", i, players[i].userinfo.GetName());
			}
		}
	}
	else
	{
		int i = atoi(argv[1]);

		if (i < 0 || i >= MAXPLAYERS)
		{
			Printf("Bad player number\n");
			return;
		}
		userinfo_t *ui = &players[i].userinfo;

		if (!playeringame[i])
		{
			Printf(TEXTCOLOR_ORANGE "Player %d is not in the game\n", i);
			return;
		}

		// Print special info
		Printf("%20s: %s\n",      "Name", ui->GetName());
		Printf("%20s: %s (%d)\n", "Team", ui->GetTeam() == TEAM_NONE ? "None" : Teams[ui->GetTeam()].GetName(), ui->GetTeam());
		Printf("%20s: %s (%d)\n", "Skin", Skins[ui->GetSkin()].Name.GetChars(), ui->GetSkin());
		Printf("%20s: %s (%d)\n", "Gender", GenderNames[ui->GetGender()], ui->GetGender());
		Printf("%20s: %s (%d)\n", "PlayerClass",
			ui->GetPlayerClassNum() == -1 ? "Random" : ui->GetPlayerClassType()->GetDisplayName().GetChars(),
			ui->GetPlayerClassNum());

		// Print generic info
		TMapIterator<FName, FBaseCVar *> it(*ui);
		TMap<FName, FBaseCVar *>::Pair *pair;

		while (it.NextPair(pair))
		{
			if (pair->Key != NAME_Name && pair->Key != NAME_Team && pair->Key != NAME_Skin &&
				pair->Key != NAME_Gender && pair->Key != NAME_PlayerClass)
			{
				Printf("%20s: %s\n", pair->Key.GetChars(), pair->Value->GetHumanString());
			}
		}
		if (argv.argc() > 2)
		{
			PrintMiscActorInfo(players[i].mo);
		}
	}
}

userinfo_t::~userinfo_t()
{
	TMapIterator<FName, FBaseCVar *> it(*this);
	TMap<FName, FBaseCVar *>::Pair *pair;

	while (it.NextPair(pair))
	{
		delete pair->Value;
	}
	this->Clear();
}

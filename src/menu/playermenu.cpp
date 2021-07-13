/*
** playermenu.cpp
** The player setup menu's setters. These are native for security purposes.
**
**---------------------------------------------------------------------------
** Copyright 2001-2010 Randy Heit
** Copyright 2010 Christoph Oelckers
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

#include "menu.h"
#include "gi.h"
#include "c_dispatch.h"
#include "teaminfo.h"
#include "r_state.h"
#include "vm.h"
#include "d_player.h"

EXTERN_CVAR(Int, team)
EXTERN_CVAR(Float, autoaim)
EXTERN_CVAR(Bool, neverswitchonpickup)
EXTERN_CVAR(Bool, cl_run)

//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(DPlayerMenu, ColorChanged)
{
	PARAM_PROLOGUE;
	PARAM_INT(r);
	PARAM_INT(g);
	PARAM_INT(b);
	// only allow if the menu is active to prevent abuse.
	if (DMenu::InMenu)
	{
		char command[24];
		players[consoleplayer].userinfo.ColorChanged(MAKERGB(r, g, b));
		mysnprintf(command, countof(command), "color \"%02x %02x %02x\"", r, g, b);
		C_DoCommand(command);
	}
	return 0;
}


//=============================================================================
//
// access to the player config is done natively, so that broader access
// functions do not need to be exported.
//
//=============================================================================

DEFINE_ACTION_FUNCTION(DPlayerMenu, PlayerNameChanged)
{
	PARAM_PROLOGUE;
	PARAM_STRING(s);
	const char *pp = s;
	FString command("name \"");

	if (DMenu::InMenu)
	{
		// Escape any backslashes or quotation marks before sending the name to the console.
		for (auto p = pp; *p != '\0'; ++p)
		{
			if (*p == '"' || *p == '\\')
			{
				command << '\\';
			}
			command << *p;
		}
		command << '"';
		C_DoCommand(command);
	}
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(DPlayerMenu, ColorSetChanged)
{
	PARAM_PROLOGUE;
	PARAM_INT(sel);
	if (DMenu::InMenu)
	{
		players[consoleplayer].userinfo.ColorSetChanged(sel);
		char command[24];
		mysnprintf(command, countof(command), "colorset %d", sel);
		C_DoCommand(command);
	}
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(DPlayerMenu, ClassChanged)
{
	PARAM_PROLOGUE;
	PARAM_INT(sel);
	PARAM_POINTER(cls, FPlayerClass);
	if (DMenu::InMenu)
	{
		const char *pclass = sel == -1 ? "Random" : GetPrintableDisplayName(cls->Type).GetChars();
		players[consoleplayer].userinfo.PlayerClassChanged(pclass);
		cvar_set("playerclass", pclass);
	}
	return 0;
}


//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(DPlayerMenu, SkinChanged)
{
	PARAM_PROLOGUE;
	PARAM_INT(sel);
	if (DMenu::InMenu)
	{
		players[consoleplayer].userinfo.SkinNumChanged(sel);
		cvar_set("skin", Skins[sel].Name);
	}
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(DPlayerMenu, AutoaimChanged)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(val);
	// only allow if the menu is active to prevent abuse.
	if (DMenu::InMenu)
	{
		autoaim = float(val);
	}
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(DPlayerMenu, TeamChanged)
{
	PARAM_PROLOGUE;
	PARAM_INT(val);
	// only allow if the menu is active to prevent abuse.
	if (DMenu::InMenu)
	{
		team = val == 0 ? TEAM_NONE : val - 1;
	}
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(DPlayerMenu, GenderChanged)
{
	PARAM_PROLOGUE;
	PARAM_INT(v);
	// only allow if the menu is active to prevent abuse.
	if (DMenu::InMenu)
	{
		switch(v)
		{
		case 0: cvar_set("gender", "male");    break;
		case 1: cvar_set("gender", "female");  break;
		case 2: cvar_set("gender", "neutral"); break;
		case 3: cvar_set("gender", "other");   break;
		}
	}
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(DPlayerMenu, PronounsChanged)
{
	PARAM_PROLOGUE;
	PARAM_STRING(str);
	// only allow if the menu is active to prevent abuse.
	if (DMenu::InMenu)
	{
		cvar_set("pronouns", str);
	}
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(DPlayerMenu, SwitchOnPickupChanged)
{
	PARAM_PROLOGUE;
	PARAM_INT(v);
	// only allow if the menu is active to prevent abuse.
	if (DMenu::InMenu)
	{
		neverswitchonpickup = !!v;
	}
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(DPlayerMenu, AlwaysRunChanged)
{
	PARAM_PROLOGUE;
	PARAM_INT(v);
	// only allow if the menu is active to prevent abuse.
	if (DMenu::InMenu)
	{
		cl_run = !!v;
	}
	return 0;
}

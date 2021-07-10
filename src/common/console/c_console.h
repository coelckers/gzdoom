/*
** c_console.h
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

#ifndef __C_CONSOLE__
#define __C_CONSOLE__

#include <stdarg.h>
#include "basics.h"
#include "c_tabcomplete.h"
#include "textureid.h"

struct event_t;

typedef enum cstate_t 
{
	c_up=0, c_down=1, c_falling=2, c_rising=3
} 
constate_e;

enum
{
	PRINTLEVELS = 5
};
extern int PrintColors[PRINTLEVELS + 2];

extern constate_e ConsoleState;

// Initialize the console
void C_InitConsole (int width, int height, bool ingame);
void C_DeinitConsole ();
void C_InitConback(FTextureID fallback, bool tile, double lightlevel = 1.);

// Adjust the console for a new screen mode
void C_NewModeAdjust (void);

void C_Ticker (void);

void AddToConsole (int printlevel, const char *string);
int PrintString (int printlevel, int printflags, const char *string);
int VPrintf (int printlevel, int printstrings, const char *format, va_list parms) GCCFORMAT(3);

void C_DrawConsole ();
void C_ToggleConsole (void);
void C_FullConsole (void);
void C_HideConsole (void);
void C_AdjustBottom (void);
void C_FlushDisplay (void);
class FNotifyBufferBase;
void C_SetNotifyBuffer(FNotifyBufferBase *nbb);


bool C_Responder (event_t *ev);

extern double NotifyFontScale;
void C_SetNotifyFontScale(double scale);

extern const char *console_bar;

#endif

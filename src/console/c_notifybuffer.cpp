/*
** c_notifybuffer.cpp
** Implements the buffer for the notification message
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** Copyright 2005-2020 Christoph Oelckers
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

#include "c_console.h"
#include "vm.h"
#include "gamestate.h"
#include "c_cvars.h"
#include "sbar.h"
#include "v_video.h"
#include "i_time.h"
#include "c_notifybufferbase.h"

struct FNotifyBuffer : public FNotifyBufferBase
{
public:
	void AddString(int printlevel, int printflags, FString source) override;
	void Clear() override;
	void Draw() override;

};

static FNotifyBuffer NotifyStrings;

EXTERN_CVAR(Bool, show_messages)
extern bool generic_ui;
CVAR(Float, con_notifytime, 3.f, CVAR_ARCHIVE)
CVAR(Bool, con_centernotify, false, CVAR_ARCHIVE)
CVAR(Bool, con_pulsetext, false, CVAR_ARCHIVE)

CUSTOM_CVAR(Int, con_scaletext, 0, CVAR_ARCHIVE)		// Scale notify text at high resolutions?
{
	if (self < 0) self = 0;
}

enum { NOTIFYFADETIME = 6 };

CUSTOM_CVAR(Int, con_notifylines, 4, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)
{
	NotifyStrings.Shift(self);
}

void FNotifyBuffer::Clear()
{
	FNotifyBufferBase::Clear();
	if (StatusBar == nullptr) return;
	IFVIRTUALPTR(StatusBar, DBaseStatusBar, FlushNotify)
	{
		VMValue params[] = { (DObject*)StatusBar };
		VMCall(func, params, countof(params), nullptr, 1);
	}

}

void FNotifyBuffer::AddString(int printlevel, int printflags, FString source)
{
	if (!show_messages ||
		source.IsEmpty() ||
		gamestate == GS_FULLCONSOLE ||
		gamestate == GS_DEMOSCREEN ||
		con_notifylines == 0)
		return;

	// [MK] allow the status bar to take over notify printing
	if (StatusBar != nullptr)
	{
		IFVIRTUALPTR(StatusBar, DBaseStatusBar, ProcessNotify)
		{
			VMValue params[] = { (DObject*)StatusBar, printlevel, &source, printflags};
			int rv;
			VMReturn ret(&rv);
			VMCall(func, params, countof(params), &ret, 1);
			if (!!rv) return;
		}
	}

	int width = DisplayWidth / active_con_scaletext(twod, generic_ui);
	FFont *font = generic_ui ? NewSmallFont : AlternativeSmallFont;
	FNotifyBufferBase::AddString(printlevel, font, source, width, con_notifytime, con_notifylines);
}

void FNotifyBuffer::Draw()
{
	bool center = (con_centernotify != 0.f);
	int line, lineadv, color, j;
	bool canskip;
	
	FFont* font = generic_ui ? NewSmallFont : AlternativeSmallFont;

	line = Top + font->GetDisplacement();
	canskip = true;

	lineadv = font->GetHeight ();

	for (unsigned i = 0; i < Text.Size(); ++ i)
	{
		FNotifyText &notify = Text[i];

		if (notify.TimeOut == 0)
			continue;

		j = notify.TimeOut - notify.Ticker;
		if (j > 0)
		{
			double alpha = (j < NOTIFYFADETIME) ? 1. * j / NOTIFYFADETIME : 1;
			if (con_pulsetext)
			{
				alpha *= 0.7 + 0.3 * sin(I_msTime() / 100.);
			}

			if (notify.PrintLevel >= PRINTLEVELS)
				color = CR_UNTRANSLATED;
			else
				color = PrintColors[notify.PrintLevel];

			int scale = active_con_scaletext(twod, generic_ui);
			if (!center)
				DrawText(twod, font, color, 0, line, notify.Text,
					DTA_VirtualWidth, twod->GetWidth() / scale,
					DTA_VirtualHeight, twod->GetHeight() / scale,
					DTA_KeepRatio, true,
					DTA_Alpha, alpha, TAG_DONE);
			else
				DrawText(twod, font, color, (twod->GetWidth() -
					font->StringWidth (notify.Text) * scale) / 2 / scale,
					line, notify.Text,
					DTA_VirtualWidth, twod->GetWidth() / scale,
					DTA_VirtualHeight, twod->GetHeight() / scale,
					DTA_KeepRatio, true,
					DTA_Alpha, alpha, TAG_DONE);
			line += lineadv;
			canskip = false;
		}
		else
		{
			notify.TimeOut = 0;
		}
	}
	if (canskip)
	{
		Top = TopGoal;
	}
}

void SetConsoleNotifyBuffer()
{
	C_SetNotifyBuffer(&NotifyStrings);
}

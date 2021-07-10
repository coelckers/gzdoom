/*
** hudmessages.cpp
** Neato messages for the HUD
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

#include "templates.h"
#include "doomdef.h"
#include "sbar.h"
#include "c_cvars.h"
#include "v_video.h"
#include "cmdlib.h"
#include "serializer_doom.h"
#include "serialize_obj.h"
#include "doomstat.h"
#include "vm.h"
#include "c_console.h"
#include "v_draw.h"

IMPLEMENT_CLASS(DHUDMessageBase, false, true)
IMPLEMENT_POINTERS_START(DHUDMessageBase)
IMPLEMENT_POINTER(Next)
IMPLEMENT_POINTERS_END


IMPLEMENT_CLASS(DHUDMessage, false, false)

IMPLEMENT_CLASS(DHUDMessageFadeOut, false, false)
IMPLEMENT_CLASS(DHUDMessageFadeInOut, false, false)
IMPLEMENT_CLASS(DHUDMessageTypeOnFadeOut, false, false)

/*************************************************************************
 * Basic HUD message. Appears and disappears without any special effects *
 *************************************************************************/

void DHUDMessageBase::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("next", Next)
		("sbarid", SBarID);
}

DEFINE_ACTION_FUNCTION(DHUDMessageBase, Tick)
{
	PARAM_SELF_PROLOGUE(DHUDMessageBase);
	ACTION_RETURN_BOOL(self->Tick());
}

DEFINE_ACTION_FUNCTION(DHUDMessageBase, ScreenSizeChanged)
{
	PARAM_SELF_PROLOGUE(DHUDMessageBase);
	self->ScreenSizeChanged();
	return 0;
}

DEFINE_ACTION_FUNCTION(DHUDMessageBase, Draw)
{
	PARAM_SELF_PROLOGUE(DHUDMessageBase);
	PARAM_INT(bottom);
	PARAM_INT(visibility);
	self->Draw(bottom, visibility);
	return 0;
}

bool DHUDMessageBase::CallTick()
{
	IFVIRTUAL(DHUDMessageBase, Tick)
	{
		VMValue params[] = { (DObject*)this };
		int retval;
		VMReturn ret; ret.IntAt(&retval);
		VMCall(func, params, countof(params), &ret, 1);
		return !!retval;
	}
	return Tick();
}

void DHUDMessageBase::CallScreenSizeChanged()
{
	IFVIRTUAL(DHUDMessageBase, ScreenSizeChanged)
	{
		VMValue params[] = { (DObject*)this };
		VMCall(func, params, countof(params), nullptr, 0);
	}
	else ScreenSizeChanged();
}

void DHUDMessageBase::CallDraw(int bottom, int visibility)
{
	IFVIRTUAL(DHUDMessageBase, Draw)
	{
		VMValue params[] = { (DObject*)this, bottom, visibility };
		VMCall(func, params, countof(params), nullptr, 0);
	}
	else Draw(bottom, visibility);
}

//============================================================================
//
// DHUDMessage Constructor
//
//============================================================================

DHUDMessage::DHUDMessage (FFont *font, const char *text, float x, float y, int hudwidth, int hudheight,
						  EColorRange textColor, float holdTime)
{
	if (hudwidth == 0 || hudheight == 0)
	{
		// for y range [-1.0, 0.0]: Positions top edge of box
		// for y range [0.0, 1.0]: Positions center of box
		// for x range [-2.0, -1.0]: Positions left edge of box, and centers text inside it
		// for x range [-1.0, 0.0]: Positions left edge of box
		// for x range [0.0, 1.0]: Positions center of box
		// for x range [1.0, 2.0]: Positions center of box, and centers text inside it
		HUDWidth = HUDHeight = 0;
		if (fabs (x) > 2.f)
		{
			CenterX = true;
			Left = 0.5f;
		}
		else
		{
			Left = x < -1.f ? x + 1.f : x > 1.f ? x - 1.f : x;
			if (fabs(x) > 1.f)
			{
				CenterX = true;
			}
			else
			{
				CenterX = false;
			}
		}
	}
	else
	{ // HUD size is specified, so coordinates are in pixels, but you can add
	  // some fractions to affect positioning:
	  // For y: .1 = positions top edge of box
	  //		.2 = positions bottom edge of box
	  // For x: .1 = positions left edge of box
	  //        .2 = positions right edge of box
	  //		.4 = centers text inside box
	  //			 .4 can also be added to either .1 or .2
		Top = y;
		HUDWidth = hudwidth;
		HUDHeight = hudheight;
		
		float intpart;
		int fracpart = (int)(fabsf (modff (x, &intpart)) * 10.f + 0.5f);
		if (fracpart & 4)
		{
			CenterX = true;
		}
		else
		{
			CenterX = false;
		}
		if (x > 0)
		{
			Left = intpart + (float)(fracpart & 3) / 10.f;
		}
		else
		{
			Left = intpart - (float)(fracpart & 3) / 10.f;
		}
	}
	NoWrap = false;
	ClipX = ClipY = ClipWidth = ClipHeight = 0;
	WrapWidth = 0;
	HandleAspect = true;
	Top = y;
	HoldTics = (int)(holdTime * TICRATE);
	Tics = -1;	// -1 to compensate for one additional Tick the message will receive.

	// Try to find the optimal font if none is specified. Prefer SmallFont, but if that cannot handle this text use the IWAD's SmallFont and if that doesn't work either, use the VGA font.
	if (font) Font = font;
	else if (generic_ui) Font = NewSmallFont;
	else if (SmallFont->CanPrint(text)) Font = SmallFont;
	else if (OriginalSmallFont->CanPrint(text)) Font = OriginalSmallFont;
	else Font = NewSmallFont;

	TextColor = textColor;
	State = 0;
	SourceText = copystring (text);
	VisibilityFlags = 0;
	Style = STYLE_Translucent;
	Alpha = 1.;
	ResetText (SourceText);
}

//============================================================================
//
// DHUDMessage Destructor
//
//============================================================================

void DHUDMessage::OnDestroy()
{
	if (SourceText != NULL)
	{
		delete[] SourceText;
		SourceText = nullptr;
	}
}

//============================================================================
//
// DHUDMessage :: Serialize
//
//============================================================================

void DHUDMessage::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("left", Left)
		("top", Top)
		("centerx", CenterX)
		("holdtics", HoldTics)
		("tics", Tics)
		("state", State)
		.Enum("textcolor", TextColor)
		("sourcetext", SourceText)
		("font", Font)
		("hudwidth", HUDWidth)
		("hudheight", HUDHeight)
		("nowrap", NoWrap)
		("clipx", ClipX)
		("clipy", ClipY)
		("clipwidth", ClipWidth)
		("clipheight", ClipHeight)
		("wrapwidth", WrapWidth)
		("handleaspect", HandleAspect)
		("visibilityflags", VisibilityFlags)
		("style", Style)
		("alpha", Alpha);

	if (arc.isReading())
	{
		ResetText(SourceText);
	}
}

//============================================================================
//
// DHUDMessage :: ScreenSizeChanged
//
//============================================================================

void DHUDMessage::ScreenSizeChanged ()
{
	if (HUDWidth == 0)
	{
		ResetText (SourceText);
	}
}

//============================================================================
//
// DHUDMessage :: CalcClipCoords
//
// Take the clip rectangle in HUD coordinates (set via SetHudSize in ACS)
// and convert them to screen coordinates.
//
//============================================================================

void DHUDMessage::CalcClipCoords(int hudheight)
{
	int x = ClipX, y = ClipY, w = ClipWidth, h = ClipHeight;

	if ((x | y | w | h) == 0)
	{ // No clipping rectangle set; use the full screen.
		ClipLeft = 0;
		ClipTop = 0;
		ClipRight = twod->GetWidth();
		ClipBot = twod->GetHeight();
	}
	else
	{
		VirtualToRealCoordsInt(twod, x, y, w, h,
			HUDWidth, hudheight, false, HandleAspect);
		ClipLeft = x;
		ClipTop = y;
		ClipRight = x + w;
		ClipBot = y + h;
	}
}

//============================================================================
//
// DHUDMessage :: ResetText
//
//============================================================================

void DHUDMessage::ResetText (const char *text)
{
	int width;

	if (HUDWidth != 0)
	{
		width = WrapWidth == 0 ? HUDWidth : WrapWidth;
	}
	else
	{
		width = twod->GetWidth() / active_con_scaletext(twod);
	}

	Lines = V_BreakLines (Font, NoWrap ? INT_MAX : width, (uint8_t *)text);

	NumLines = Lines.Size();
	Width = 0;
	Height = 0;

	for (auto &line : Lines)
	{
		Height += Font->GetHeight ();
		Width = MAX<int> (Width, line.Width);
	}
}

//============================================================================
//
// DHUDMessage :: Tick
//
//============================================================================

bool DHUDMessage::Tick ()
{
	Tics++;
	if (HoldTics != 0 && HoldTics <= Tics)
	{ // This message has expired
		return true;
	}
	return false;
}

//============================================================================
//
// DHUDMessage :: Draw
//
//============================================================================

void DHUDMessage::Draw (int bottom, int visibility)
{
	int xscale, yscale;
	int x, y;
	int ystep;
	int i;
	bool clean = false;
	int hudheight;

	// If any of the visibility flags match, do NOT draw this message.
	if (VisibilityFlags & visibility)
	{
		return;
	}

	DrawSetup ();

	int screen_width = twod->GetWidth();
	int screen_height = twod->GetHeight();
	xscale = yscale = 1;
	if (HUDWidth == 0)
	{
		int scale = active_con_scaletext(twod);
		screen_width /= scale;
		screen_height /= scale;
		bottom /= scale;
	}

	if (HUDWidth == 0)
	{
		if (Left > 0.f)
		{ // Position center
			x = (int)((float)(screen_width - Width * xscale) * Left);
		}
		else
		{ // Position edge
			x = (int)((float)screen_width * -Left);
		}
		if (Top > 0.f)
		{ // Position center
			y = (int)((float)(bottom - Height * yscale) * Top);
		}
		else
		{ // Position edge
			y = (int)((float)bottom * -Top);
		}
	}
	else
	{
		float intpart;
		int fracpart;

		fracpart = (int)(fabsf (modff (Left, &intpart)) * 10.f + 0.5f);
		x = (int)intpart;
		switch (fracpart & 3)
		{
		case 0: // Position center
			x -= Width / 2;
			break;

		case 2: // Position right
			x -= Width;
			break;
		}

		fracpart = (int)(fabsf (modff (Top, &intpart)) * 10.f + 0.5f);
		y = (int)intpart;
		switch (fracpart & 3)
		{
		case 0: // Position center
			y -= Height / 2;
			break;

		case 2: // Position bottom
			y -= Height;
			break;
		}
	}

	if (CenterX)
	{
		x += Width * xscale / 2;
	}

	ystep = Font->GetHeight() * yscale;

	if (HUDHeight < 0)
	{ // A negative height means the HUD size covers the status bar
		hudheight = -HUDHeight;
	}
	else
	{ // A positive height means the HUD size does not cover the status bar
		hudheight = Scale (HUDHeight, screen_height, bottom);
	}
	CalcClipCoords(hudheight);

	for (i = 0; i < NumLines; i++)
	{
		int drawx;

		drawx = CenterX ? x - Lines[i].Width*xscale/2 : x;
		DoDraw (i, drawx, y, clean, hudheight);
		y += ystep;
	}
}

//============================================================================
//
// DHUDMessage :: DrawSetup
//
//============================================================================

void DHUDMessage::DrawSetup ()
{
}

//============================================================================
//
// DHUDMessage :: DoDraw
//
//============================================================================

void DHUDMessage::DoDraw (int linenum, int x, int y, bool clean, int hudheight)
{
	if (hudheight == 0)
	{
		int scale = active_con_scaletext(twod);
		DrawText(twod, Font, TextColor, x, y, Lines[linenum].Text,
			DTA_VirtualWidth, twod->GetWidth() / scale,
			DTA_VirtualHeight, twod->GetHeight() / scale,
			DTA_Alpha, Alpha,
			DTA_RenderStyle, Style,
			DTA_KeepRatio, true,
			TAG_DONE);
	}
	else
	{
		DrawText(twod, Font, TextColor, x, y, Lines[linenum].Text,
			DTA_VirtualWidth, HUDWidth,
			DTA_VirtualHeight, hudheight,
			DTA_ClipLeft, ClipLeft,
			DTA_ClipRight, ClipRight,
			DTA_ClipTop, ClipTop,
			DTA_ClipBottom, ClipBot,
			DTA_Alpha, Alpha,
			DTA_RenderStyle, Style,
			TAG_DONE);
	}
}

/******************************
 * HUD message that fades out *
 ******************************/

//============================================================================
//
// DHUDMessageFadeOut Constructor
//
//============================================================================

DHUDMessageFadeOut::DHUDMessageFadeOut (FFont *font, const char *text, float x, float y,
	int hudwidth, int hudheight,									
	EColorRange textColor, float holdTime, float fadeOutTime)
	: DHUDMessage (font, text, x, y, hudwidth, hudheight, textColor, holdTime)
{
	FadeOutTics = (int)(fadeOutTime * TICRATE);
	State = 1;
}

//============================================================================
//
// DHUDMessageFadeOut :: Serialize
//
//============================================================================

void DHUDMessageFadeOut::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("fadeouttics", FadeOutTics);
}

//============================================================================
//
// DHUDMessageFadeOut :: Tick
//
//============================================================================

bool DHUDMessageFadeOut::Tick ()
{
	Tics++;
	if (State == 1 && HoldTics <= Tics)
	{
		State++;
		Tics -= HoldTics;
	}
	if (State == 2 && FadeOutTics <= Tics)
	{
		return true;
	}
	return false;
}

//============================================================================
//
// DHUDMessageFadeOut :: DoDraw
//
//============================================================================

void DHUDMessageFadeOut::DoDraw (int linenum, int x, int y, bool clean, int hudheight)
{
	if (State == 1)
	{
		DHUDMessage::DoDraw (linenum, x, y, clean, hudheight);
	}
	else
	{
		float trans = float(Alpha * -(Tics - FadeOutTics) / FadeOutTics);
		if (hudheight == 0)
		{
			int scale = active_con_scaletext(twod);
			DrawText(twod, Font, TextColor, x, y, Lines[linenum].Text,
				DTA_VirtualWidth, twod->GetWidth() / scale,
				DTA_VirtualHeight, twod->GetHeight() / scale,
				DTA_Alpha, trans,
				DTA_RenderStyle, Style,
				DTA_KeepRatio, true,
				TAG_DONE);
		}
		else
		{
			DrawText(twod, Font, TextColor, x, y, Lines[linenum].Text,
				DTA_VirtualWidth, HUDWidth,
				DTA_VirtualHeight, hudheight,
				DTA_ClipLeft, ClipLeft,
				DTA_ClipRight, ClipRight,
				DTA_ClipTop, ClipTop,
				DTA_ClipBottom, ClipBot,
				DTA_Alpha, trans,
				DTA_RenderStyle, Style,
				TAG_DONE);
		}
	}
}

/***************************************
 * HUD message that fades in, then out *
 ***************************************/

//============================================================================
//
// DHUDMessageFadeInOut Constructor
//
//============================================================================

DHUDMessageFadeInOut::DHUDMessageFadeInOut (FFont *font, const char *text, float x, float y,
	int hudwidth, int hudheight,									
	EColorRange textColor, float holdTime, float fadeInTime, float fadeOutTime)
	: DHUDMessageFadeOut (font, text, x, y, hudwidth, hudheight, textColor, holdTime, fadeOutTime)
{
	FadeInTics = (int)(fadeInTime * TICRATE);
	State = FadeInTics == 0;
}

//============================================================================
//
// DHUDMessageFadeInOut :: Serialize
//
//============================================================================

void DHUDMessageFadeInOut::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("fadeintics", FadeInTics);
}

//============================================================================
//
// DHUDMessageFadeInOut :: Tick
//
//============================================================================

bool DHUDMessageFadeInOut::Tick ()
{
	if (!Super::Tick ())
	{
		if (State == 0 && FadeInTics <= Tics)
		{
			State++;
			Tics -= FadeInTics;
		}
		return false;
	}
	return true;
}

//============================================================================
//
// DHUDMessageFadeInOut :: DoDraw
//
//============================================================================

void DHUDMessageFadeInOut::DoDraw (int linenum, int x, int y, bool clean, int hudheight)
{
	if (State == 0)
	{
		float trans = float(Alpha * Tics / FadeInTics);
		if (hudheight == 0)
		{
			int scale = active_con_scaletext(twod);
			DrawText(twod, Font, TextColor, x, y, Lines[linenum].Text,
				DTA_VirtualWidth, twod->GetWidth() / scale,
				DTA_VirtualHeight, twod->GetHeight() / scale,
				DTA_Alpha, trans,
				DTA_RenderStyle, Style,
				DTA_KeepRatio, true,
				TAG_DONE);
		}
		else
		{
			DrawText(twod, Font, TextColor, x, y, Lines[linenum].Text,
				DTA_VirtualWidth, HUDWidth,
				DTA_VirtualHeight, hudheight,
				DTA_ClipLeft, ClipLeft,
				DTA_ClipRight, ClipRight,
				DTA_ClipTop, ClipTop,
				DTA_ClipBottom, ClipBot,
				DTA_Alpha, trans,
				DTA_RenderStyle, Style,
				TAG_DONE);
		}
	}
	else
	{
		Super::DoDraw (linenum, x, y, clean, hudheight);
	}
}

/****************************************************
 * HUD message that gets "typed" on, then fades out *
 ****************************************************/

//============================================================================
//
// DHUDMessageTypeOnFadeOut Constructor
//
//============================================================================

DHUDMessageTypeOnFadeOut::DHUDMessageTypeOnFadeOut (FFont *font, const char *text, float x, float y,
	int hudwidth, int hudheight,
	EColorRange textColor, float typeTime, float holdTime, float fadeOutTime)
	: DHUDMessageFadeOut (font, text, x, y, hudwidth, hudheight, textColor, holdTime, fadeOutTime)
{
	TypeOnTime = typeTime * TICRATE;
	if (TypeOnTime == 0.f)
		TypeOnTime = 0.1f;
	CurrLine = 0;
	LineLen = Lines.Size() > 0? (int)Lines[0].Text.Len() : 0;
	LineVisible = 0;
	State = 3;
}

//============================================================================
//
// DHUDMessageTypeOnFadeOut :: Serialize
//
//============================================================================

void DHUDMessageTypeOnFadeOut::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("typeontime", TypeOnTime)
		("currline", CurrLine)
		("linevisible", LineVisible);

	if (arc.isReading())
	{
		if (CurrLine < NumLines)
		{
			LineLen = (int)Lines[CurrLine].Text.Len();
		}
		else
		{
			LineLen = CurrLine = 0;
		}

		clamp(LineVisible, 0, LineLen);
	}
}

//============================================================================
//
// DHUDMessageTypeOnFadeOut :: Tick
//
//============================================================================

bool DHUDMessageTypeOnFadeOut::Tick ()
{
	if (NumLines > 0 && !Super::Tick ())
	{
		if (State == 3)
		{
			int step = Tics == 0 ? 0 : int(Tics / TypeOnTime) - int((Tics - 1) / TypeOnTime);
			int linevis = LineVisible;
			int linedrawcount = linevis;
			FString text = Lines[CurrLine].Text;
			// Advance LineVisible by 'step' *visible* characters
			while (step > 0 && State == 3)
			{
				if (linevis > LineLen)
				{
					linevis = 0;
					linedrawcount = 0;
					CurrLine++;
					if (CurrLine >= NumLines)
					{
						Tics = 0;
						State = 1;
					}
					else
					{
						text = Lines[CurrLine].Text;
						LineLen = (int)text.Len();
					}
				}
				if (State == 3 && --step >= 0)
				{
					linedrawcount++;
					
					if (text.GetNextCharacter(linevis) == TEXTCOLOR_ESCAPE)
					{
						if (text[linevis] == '[')
						{ // named color
							while (text[linevis] != ']' && text[linevis] != '\0')
							{
								linevis++;
							}
						}
						linevis += 2;
					}
				}
			}
			LineVisible = linevis;
		}
		return false;
	}
	return true;
}

//============================================================================
//
// DHUDMessageTypeOnFadeOut :: ScreenSizeChanged
//
//============================================================================

void DHUDMessageTypeOnFadeOut::ScreenSizeChanged ()
{
	int charCount = 0, i;

	for (i = 0; i < CurrLine; ++i)
	{
		charCount += (int)Lines[i].Text.Len();
	}
	charCount += LineVisible;

	Super::ScreenSizeChanged ();
	if (State == 3)
	{
		CurrLine = 0;
		LineLen = NumLines > 0 ? (int)Lines[0].Text.Len() : 0;
		Tics = (int)(charCount * TypeOnTime) - 1;
		Tick ();
	}
}

//============================================================================
//
// DHUDMessageTypeOnFadeOut :: DoDraw
//
//============================================================================

void DHUDMessageTypeOnFadeOut::DoDraw (int linenum, int x, int y, bool clean, int hudheight)
{
	if (State == 3)
	{
		if (linenum < CurrLine)
		{
			DHUDMessage::DoDraw (linenum, x, y, clean, hudheight);
		}
		else if (linenum == CurrLine)
		{
			if (hudheight == 0)
			{
				int scale = active_con_scaletext(twod);
				DrawText(twod, Font, TextColor, x, y, Lines[linenum].Text,
					DTA_VirtualWidth, twod->GetWidth() / scale,
					DTA_VirtualHeight, twod->GetHeight() / scale,
					DTA_KeepRatio, true,
					DTA_TextLen, LineVisible,
					DTA_Alpha, Alpha,
					DTA_RenderStyle, Style,
					TAG_DONE);
			}
			else
			{
				DrawText(twod, Font, TextColor, x, y, Lines[linenum].Text,
					DTA_VirtualWidth, HUDWidth,
					DTA_VirtualHeight, hudheight,
					DTA_ClipLeft, ClipLeft,
					DTA_ClipRight, ClipRight,
					DTA_ClipTop, ClipTop,
					DTA_ClipBottom, ClipBot,
					DTA_Alpha, Alpha,
					DTA_TextLen, LineVisible,
					DTA_RenderStyle, Style,
					TAG_DONE);
			}
		}
	}
	else
	{
		DHUDMessageFadeOut::DoDraw (linenum, x, y, clean, hudheight);
	}
}

/* Printing in the middle of the screen */

CVAR(Float, con_midtime, 3.f, CVAR_ARCHIVE)

const char* console_bar = "----------------------------------------";

void C_MidPrint(FFont* font, const char* msg, bool bold)
{
	if (StatusBar == nullptr || screen == nullptr)
		return;

	// [MK] allow the status bar to take over MidPrint
	IFVIRTUALPTR(StatusBar, DBaseStatusBar, ProcessMidPrint)
	{
		FString msgstr = msg;
		VMValue params[] = { (DObject*)StatusBar, font, &msgstr, bold };
		int rv;
		VMReturn ret(&rv);
		VMCall(func, params, countof(params), &ret, 1);
		if (!!rv) return;
	}

	if (msg != nullptr)
	{
		auto color = (EColorRange)PrintColors[bold ? PRINTLEVELS + 1 : PRINTLEVELS];
		Printf(PRINT_HIGH, PRINTF_NONOTIFY, TEXTCOLOR_ESCAPESTR "%c%s\n%s\n%s\n", color, console_bar, msg, console_bar);

		StatusBar->AttachMessage(Create<DHUDMessage>(font, msg, 1.5f, 0.375f, 0, 0, color, con_midtime), MAKE_ID('C', 'N', 'T', 'R'));
	}
	else
	{
		StatusBar->DetachMessage(MAKE_ID('C', 'N', 'T', 'R'));
	}
}


/*
** vmnatives.cpp
**
** VM exports for engine backend classes
**
**---------------------------------------------------------------------------
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
**
*/ 


#include "texturemanager.h"
#include "filesystem.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_bind.h"
#include "c_dispatch.h"
#include "templates.h"
#include "menu.h"
#include "vm.h"
#include "gstrings.h"
#include "printf.h"
#include "s_music.h"
#include "i_interface.h"
#include "base_sbar.h"
#include "image.h"

//==========================================================================
//
// status bar exports
//
//==========================================================================

static double StatusbarToRealCoords(DStatusBarCore* self, double x, double y, double w, double h, double* py, double* pw, double* ph)
{
	self->StatusbarToRealCoords(x, y, w, h);
	*py = y;
	*pw = w;
	*ph = h;
	return x;
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, StatusbarToRealCoords, StatusbarToRealCoords)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(w);
	PARAM_FLOAT(h);
	self->StatusbarToRealCoords(x, y, w, h);
	if (numret > 0) ret[0].SetFloat(x);
	if (numret > 1) ret[1].SetFloat(y);
	if (numret > 2) ret[2].SetFloat(w);
	if (numret > 3) ret[3].SetFloat(h);
	return MIN(4, numret);
}

void SBar_DrawTexture(DStatusBarCore* self, int texid, double x, double y, int flags, double alpha, double w, double h, double scaleX, double scaleY, int style, int color, int translation, double clipwidth)
{
	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	self->DrawGraphic(FSetTextureID(texid), x, y, flags, alpha, w, h, scaleX, scaleY, ERenderStyle(style), color, translation, clipwidth);
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, DrawTexture, SBar_DrawTexture)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_INT(texid);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_INT(flags);
	PARAM_FLOAT(alpha);
	PARAM_FLOAT(w);
	PARAM_FLOAT(h);
	PARAM_FLOAT(scaleX);
	PARAM_FLOAT(scaleY);
	PARAM_INT(style);
	PARAM_INT(col);
	PARAM_INT(trans);
	PARAM_FLOAT(clipwidth);
	SBar_DrawTexture(self, texid, x, y, flags, alpha, w, h, scaleX, scaleY, style, col, trans, clipwidth);
	return 0;
}

void SBar_DrawImage(DStatusBarCore* self, const FString& texid, double x, double y, int flags, double alpha, double w, double h, double scaleX, double scaleY, int style, int color, int translation, double clipwidth)
{
	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	self->DrawGraphic(TexMan.CheckForTexture(texid, ETextureType::Any), x, y, flags, alpha, w, h, scaleX, scaleY, ERenderStyle(style), color, translation, clipwidth);
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, DrawImage, SBar_DrawImage)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_STRING(texid);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_INT(flags);
	PARAM_FLOAT(alpha);
	PARAM_FLOAT(w);
	PARAM_FLOAT(h);
	PARAM_FLOAT(scaleX);
	PARAM_FLOAT(scaleY);
	PARAM_INT(style);
	PARAM_INT(col);
	PARAM_INT(trans);
	PARAM_FLOAT(clipwidth);
	SBar_DrawImage(self, texid, x, y, flags, alpha, w, h, scaleX, scaleY, style, col, trans, clipwidth);
	return 0;
}

void SBar_DrawImageRotated(DStatusBarCore* self, const FString& texid, double x, double y, int flags, double angle, double alpha, double scaleX, double scaleY, int style, int color, int translation)
{
	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	self->DrawRotated(TexMan.CheckForTexture(texid, ETextureType::Any), x, y, flags, angle, alpha, scaleX, scaleY, color, translation, (ERenderStyle)style);
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, DrawImageRotated, SBar_DrawImageRotated)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_STRING(texid);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_INT(flags);
	PARAM_FLOAT(angle);
	PARAM_FLOAT(alpha);
	PARAM_FLOAT(scaleX);
	PARAM_FLOAT(scaleY);
	PARAM_INT(style);
	PARAM_INT(col);
	PARAM_INT(trans);
	SBar_DrawImageRotated(self, texid, x, y, flags, angle, alpha, scaleX, scaleY, style, col, trans);
	return 0;
}

void SBar_DrawTextureRotated(DStatusBarCore* self, int texid, double x, double y, int flags, double angle, double alpha, double scaleX, double scaleY, int style, int color, int translation)
{
	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	self->DrawRotated(FSetTextureID(texid), x, y, flags, alpha, scaleX, scaleY, color, translation, style);
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, DrawTextureRotated, SBar_DrawTextureRotated)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_INT(texid);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_INT(flags);
	PARAM_FLOAT(angle);
	PARAM_FLOAT(alpha);
	PARAM_FLOAT(scaleX);
	PARAM_FLOAT(scaleY);
	PARAM_INT(style);
	PARAM_INT(col);
	PARAM_INT(trans);
	SBar_DrawTextureRotated(self, texid, x, y, flags, angle, alpha, scaleX, scaleY, style, col, trans);
	return 0;
}


void SBar_DrawString(DStatusBarCore* self, DHUDFont* font, const FString& string, double x, double y, int flags, int trans, double alpha, int wrapwidth, int linespacing, double scaleX, double scaleY, int translation, int style);

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, DrawString, SBar_DrawString)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_POINTER_NOT_NULL(font, DHUDFont);
	PARAM_STRING(string);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_INT(flags);
	PARAM_INT(trans);
	PARAM_FLOAT(alpha);
	PARAM_INT(wrapwidth);
	PARAM_INT(linespacing);
	PARAM_FLOAT(scaleX);
	PARAM_FLOAT(scaleY);
	PARAM_INT(pt);
	PARAM_INT(style);
	SBar_DrawString(self, font, string, x, y, flags, trans, alpha, wrapwidth, linespacing, scaleX, scaleY, pt, style);
	return 0;
}

static double SBar_TransformRect(DStatusBarCore* self, double x, double y, double w, double h, int flags, double* py, double* pw, double* ph)
{
	self->TransformRect(x, y, w, h, flags);
	*py = y;
	*pw = w;
	*ph = h;
	return x;
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, TransformRect, SBar_TransformRect)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(w);
	PARAM_FLOAT(h);
	PARAM_INT(flags);
	self->TransformRect(x, y, w, h, flags);
	if (numret > 0) ret[0].SetFloat(x);
	if (numret > 1) ret[1].SetFloat(y);
	if (numret > 2) ret[2].SetFloat(w);
	if (numret > 3) ret[3].SetFloat(h);
	return MIN(4, numret);
}

static void SBar_Fill(DStatusBarCore* self, int color, double x, double y, double w, double h, int flags)
{
	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	self->Fill(color, x, y, w, h, flags);
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, Fill, SBar_Fill)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_COLOR(color);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(w);
	PARAM_FLOAT(h);
	PARAM_INT(flags);
	SBar_Fill(self, color, x, y, w, h, flags);
	return 0;
}

static void SBar_SetClipRect(DStatusBarCore* self, double x, double y, double w, double h, int flags)
{
	self->SetClipRect(x, y, w, h, flags);
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, SetClipRect, SBar_SetClipRect)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(w);
	PARAM_FLOAT(h);
	PARAM_INT(flags);
	self->SetClipRect(x, y, w, h, flags);
	return 0;
}

void FormatNumber(int number, int minsize, int maxsize, int flags, const FString& prefix, FString* result);

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, FormatNumber, FormatNumber)
{
	PARAM_PROLOGUE;
	PARAM_INT(number);
	PARAM_INT(minsize);
	PARAM_INT(maxsize);
	PARAM_INT(flags);
	PARAM_STRING(prefix);
	FString fmt;
	FormatNumber(number, minsize, maxsize, flags, prefix, &fmt);
	ACTION_RETURN_STRING(fmt);
}

static void SBar_SetSize(DStatusBarCore* self, int rt, int vw, int vh, int hvw, int hvh)
{
	self->SetSize(rt, vw, vh, hvw, hvh);
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, SetSize, SBar_SetSize)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_INT(rt);
	PARAM_INT(vw);
	PARAM_INT(vh);
	PARAM_INT(hvw);
	PARAM_INT(hvh);
	self->SetSize(rt, vw, vh, hvw, hvh);
	return 0;
}

static void SBar_GetHUDScale(DStatusBarCore* self, DVector2* result)
{
	*result = self->GetHUDScale();
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, GetHUDScale, SBar_GetHUDScale)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	ACTION_RETURN_VEC2(self->GetHUDScale());
}

static void BeginStatusBar(DStatusBarCore* self, bool fs, int w, int h, int r)
{
	self->BeginStatusBar(w, h, r, fs);
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, BeginStatusBar, BeginStatusBar)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_BOOL(fs);
	PARAM_INT(w);
	PARAM_INT(h);
	PARAM_INT(r);
	self->BeginStatusBar(w, h, r, fs);
	return 0;
}

static void BeginHUD(DStatusBarCore* self, double a, bool fs, int w, int h)
{
	self->BeginHUD(w, h, a, fs);
}

DEFINE_ACTION_FUNCTION_NATIVE(DStatusBarCore, BeginHUD, BeginHUD)
{
	PARAM_SELF_PROLOGUE(DStatusBarCore);
	PARAM_FLOAT(a);
	PARAM_BOOL(fs);
	PARAM_INT(w);
	PARAM_INT(h);
	self->BeginHUD(w, h, a, fs);
	return 0;
}


//=====================================================================================
//
// 
//
//=====================================================================================

DHUDFont* CreateHudFont(FFont* fnt, int spac, int mono, int sx, int sy)
{
	return Create<DHUDFont>(fnt, spac, EMonospacing(mono), sy, sy);
}

DEFINE_ACTION_FUNCTION_NATIVE(DHUDFont, Create, CreateHudFont)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(fnt, FFont);
	PARAM_INT(spac);
	PARAM_INT(mono);
	PARAM_INT(sx);
	PARAM_INT(sy);
	ACTION_RETURN_POINTER(CreateHudFont(fnt, spac, mono, sy, sy));
}




//==========================================================================
//
// texture manager exports
//
//==========================================================================

DEFINE_ACTION_FUNCTION(_TexMan, GetName)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	auto tex = TexMan.GameByIndex(texid);
	FString retval;

	if (tex != nullptr)
	{
		if (tex->GetName().IsNotEmpty()) retval = tex->GetName();
		else
		{
			// Textures for full path names do not have their own name, they merely link to the source lump.
			auto lump = tex->GetSourceLump();
			if (fileSystem.GetLinkedTexture(lump) == tex)
				retval = fileSystem.GetFileFullName(lump);
		}
	}
	ACTION_RETURN_STRING(retval);
}

static int CheckForTexture(const FString& name, int type, int flags)
{
	return TexMan.CheckForTexture(name, static_cast<ETextureType>(type), flags).GetIndex();
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, CheckForTexture, CheckForTexture)
{
	PARAM_PROLOGUE;
	PARAM_STRING(name);
	PARAM_INT(type);
	PARAM_INT(flags);
	ACTION_RETURN_INT(CheckForTexture(name, type, flags));
}

//==========================================================================
//
//
//
//==========================================================================

static int GetTextureSize(int texid, int* py)
{
	auto tex = TexMan.GameByIndex(texid);
	int x, y;
	if (tex != nullptr)
	{
		x = int(0.5 + tex->GetDisplayWidth());
		y = int(0.5 + tex->GetDisplayHeight());
	}
	else x = y = -1;
	if (py) *py = y;
	return x;
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, GetSize, GetTextureSize)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	int x, y;
	x = GetTextureSize(texid, &y);
	if (numret > 0) ret[0].SetInt(x);
	if (numret > 1) ret[1].SetInt(y);
	return MIN(numret, 2);
}

//==========================================================================
//
//
//
//==========================================================================
static void GetScaledSize(int texid, DVector2* pvec)
{
	auto tex = TexMan.GameByIndex(texid);
	double x, y;
	if (tex != nullptr)
	{
		x = tex->GetDisplayWidth();
		y = tex->GetDisplayHeight();
	}
	else x = y = -1;
	if (pvec)
	{
		pvec->X = x;
		pvec->Y = y;
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, GetScaledSize, GetScaledSize)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	DVector2 vec;
	GetScaledSize(texid, &vec);
	ACTION_RETURN_VEC2(vec);
}

//==========================================================================
//
//
//
//==========================================================================
static void GetScaledOffset(int texid, DVector2* pvec)
{
	auto tex = TexMan.GameByIndex(texid);
	double x, y;
	if (tex != nullptr)
	{
		x = tex->GetDisplayLeftOffset();
		y = tex->GetDisplayTopOffset();
	}
	else x = y = -1;
	if (pvec)
	{
		pvec->X = x;
		pvec->Y = y;
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, GetScaledOffset, GetScaledOffset)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	DVector2 vec;
	GetScaledOffset(texid, &vec);
	ACTION_RETURN_VEC2(vec);
}

//==========================================================================
//
//
//
//==========================================================================

static int CheckRealHeight(int texid)
{
	auto tex = TexMan.GameByIndex(texid);
	if (tex != nullptr) return tex->CheckRealHeight();
	else return -1;
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, CheckRealHeight, CheckRealHeight)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	ACTION_RETURN_INT(CheckRealHeight(texid));
}

bool OkForLocalization(FTextureID texnum, const char* substitute);

static int OkForLocalization_(int index, const FString& substitute)
{
	return OkForLocalization(FSetTextureID(index), substitute);
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, OkForLocalization, OkForLocalization_)
{
	PARAM_PROLOGUE;
	PARAM_INT(name);
	PARAM_STRING(subst)
	ACTION_RETURN_INT(OkForLocalization_(name, subst));
}

static int UseGamePalette(int index)
{
	auto tex = TexMan.GameByIndex(index, false);
	if (!tex) return false;
	auto image = tex->GetTexture()->GetImage();
	return image ? image->UseGamePalette() : false;
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, UseGamePalette, UseGamePalette)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	ACTION_RETURN_INT(UseGamePalette(texid));
}

//=====================================================================================
//
// FFont exports
//
//=====================================================================================

static FFont *GetFont(int name)
{
	return V_GetFont(FName(ENamedName(name)).GetChars());
}

DEFINE_ACTION_FUNCTION_NATIVE(FFont, GetFont, GetFont)
{
	PARAM_PROLOGUE;
	PARAM_INT(name);
	ACTION_RETURN_POINTER(GetFont(name));
}

static FFont *FindFont(int name)
{
	return FFont::FindFont(FName(ENamedName(name)));
}

DEFINE_ACTION_FUNCTION_NATIVE(FFont, FindFont, FindFont)
{
	PARAM_PROLOGUE;
	PARAM_NAME(name);
	ACTION_RETURN_POINTER(FFont::FindFont(name));
}

static int GetCharWidth(FFont *font, int code)
{
	return font->GetCharWidth(code);
}

DEFINE_ACTION_FUNCTION_NATIVE(FFont, GetCharWidth, GetCharWidth)
{
	PARAM_SELF_STRUCT_PROLOGUE(FFont);
	PARAM_INT(code);
	ACTION_RETURN_INT(self->GetCharWidth(code));
}

static int GetHeight(FFont *font)
{
	return font->GetHeight();
}

DEFINE_ACTION_FUNCTION_NATIVE(FFont, GetHeight, GetHeight)
{
	PARAM_SELF_STRUCT_PROLOGUE(FFont);
	ACTION_RETURN_INT(self->GetHeight());
}

static int GetDisplacement(FFont* font)
{
	return font->GetDisplacement();
}

DEFINE_ACTION_FUNCTION_NATIVE(FFont, GetDisplacement, GetDisplacement)
{
	PARAM_SELF_STRUCT_PROLOGUE(FFont);
	ACTION_RETURN_INT(self->GetDisplacement());
}

double GetBottomAlignOffset(FFont *font, int c);
DEFINE_ACTION_FUNCTION_NATIVE(FFont, GetBottomAlignOffset, GetBottomAlignOffset)
{
	PARAM_SELF_STRUCT_PROLOGUE(FFont);
	PARAM_INT(code);
	ACTION_RETURN_FLOAT(GetBottomAlignOffset(self, code));
}

static int StringWidth(FFont *font, const FString &str)
{
	const char *txt = str[0] == '$' ? GStrings(&str[1]) : str.GetChars();
	return font->StringWidth(txt);
}

DEFINE_ACTION_FUNCTION_NATIVE(FFont, StringWidth, StringWidth)
{
	PARAM_SELF_STRUCT_PROLOGUE(FFont);
	PARAM_STRING(str);
	ACTION_RETURN_INT(StringWidth(self, str));
}

static int GetMaxAscender(FFont* font, const FString& str)
{
	const char* txt = str[0] == '$' ? GStrings(&str[1]) : str.GetChars();
	return font->GetMaxAscender(txt);
}

DEFINE_ACTION_FUNCTION_NATIVE(FFont, GetMaxAscender, GetMaxAscender)
{
	PARAM_SELF_STRUCT_PROLOGUE(FFont);
	PARAM_STRING(str);
	ACTION_RETURN_INT(GetMaxAscender(self, str));
}

static int CanPrint(FFont *font, const FString &str)
{
	const char *txt = str[0] == '$' ? GStrings(&str[1]) : str.GetChars();
	return font->CanPrint(txt);
}

DEFINE_ACTION_FUNCTION_NATIVE(FFont, CanPrint, CanPrint)
{
	PARAM_SELF_STRUCT_PROLOGUE(FFont);
	PARAM_STRING(str);
	ACTION_RETURN_INT(CanPrint(self, str));
}

static int FindFontColor(int name)
{
	return V_FindFontColor(ENamedName(name));
}

DEFINE_ACTION_FUNCTION_NATIVE(FFont, FindFontColor, FindFontColor)
{
	PARAM_PROLOGUE;
	PARAM_NAME(code);
	ACTION_RETURN_INT((int)V_FindFontColor(code));
}

static void GetCursor(FFont *font, FString *result)
{
	*result = font->GetCursor();
}

DEFINE_ACTION_FUNCTION_NATIVE(FFont, GetCursor, GetCursor)
{
	PARAM_SELF_STRUCT_PROLOGUE(FFont);
	ACTION_RETURN_STRING(FString(self->GetCursor()));
}

static int GetGlyphHeight(FFont* fnt, int code)
{
	auto glyph = fnt->GetChar(code, CR_UNTRANSLATED, nullptr);
	return glyph ? (int)glyph->GetDisplayHeight() : 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(FFont, GetGlyphHeight, GetGlyphHeight)
{
	PARAM_SELF_STRUCT_PROLOGUE(FFont);
	PARAM_INT(code);
	ACTION_RETURN_INT(GetGlyphHeight(self, code));
}
//==========================================================================
//
// file system
//
//==========================================================================

DEFINE_ACTION_FUNCTION(_Wads, GetNumLumps)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(fileSystem.GetNumEntries());
}

DEFINE_ACTION_FUNCTION(_Wads, CheckNumForName)
{
	PARAM_PROLOGUE;
	PARAM_STRING(name);
	PARAM_INT(ns);
	PARAM_INT(wadnum);
	PARAM_BOOL(exact);
	ACTION_RETURN_INT(fileSystem.CheckNumForName(name, ns, wadnum, exact));
}

DEFINE_ACTION_FUNCTION(_Wads, CheckNumForFullName)
{
	PARAM_PROLOGUE;
	PARAM_STRING(name);
	ACTION_RETURN_INT(fileSystem.CheckNumForFullName(name));
}

DEFINE_ACTION_FUNCTION(_Wads, FindLump)
{
	PARAM_PROLOGUE;
	PARAM_STRING(name);
	PARAM_INT(startlump);
	PARAM_INT(ns);
	const bool isLumpValid = startlump >= 0 && startlump < fileSystem.GetNumEntries();
	ACTION_RETURN_INT(isLumpValid ? fileSystem.FindLump(name, &startlump, 0 != ns) : -1);
}

DEFINE_ACTION_FUNCTION(_Wads, GetLumpName)
{
	PARAM_PROLOGUE;
	PARAM_INT(lump);
	FString lumpname;
	fileSystem.GetFileShortName(lumpname, lump);
	ACTION_RETURN_STRING(lumpname);
}

DEFINE_ACTION_FUNCTION(_Wads, GetLumpFullName)
{
	PARAM_PROLOGUE;
	PARAM_INT(lump);
	ACTION_RETURN_STRING(fileSystem.GetFileFullName(lump));
}

DEFINE_ACTION_FUNCTION(_Wads, GetLumpNamespace)
{
	PARAM_PROLOGUE;
	PARAM_INT(lump);
	ACTION_RETURN_INT(fileSystem.GetFileNamespace(lump));
}

DEFINE_ACTION_FUNCTION(_Wads, ReadLump)
{
	PARAM_PROLOGUE;
	PARAM_INT(lump);
	const bool isLumpValid = lump >= 0 && lump < fileSystem.GetNumEntries();
	ACTION_RETURN_STRING(isLumpValid ? fileSystem.ReadFile(lump).GetString() : FString());
}

//==========================================================================
//
// CVARs
//
//==========================================================================

DEFINE_ACTION_FUNCTION(_CVar, GetInt)
{
	PARAM_SELF_STRUCT_PROLOGUE(FBaseCVar);
	auto v = self->GetGenericRep(CVAR_Int);
	ACTION_RETURN_INT(v.Int);
}

DEFINE_ACTION_FUNCTION(_CVar, GetFloat)
{
	PARAM_SELF_STRUCT_PROLOGUE(FBaseCVar);
	auto v = self->GetGenericRep(CVAR_Float);
	ACTION_RETURN_FLOAT(v.Float);
}

DEFINE_ACTION_FUNCTION(_CVar, GetString)
{
	PARAM_SELF_STRUCT_PROLOGUE(FBaseCVar);
	auto v = self->GetGenericRep(CVAR_String);
	ACTION_RETURN_STRING(v.String);
}

DEFINE_ACTION_FUNCTION(_CVar, SetInt)
{
	// Only menus are allowed to change CVARs.
	PARAM_SELF_STRUCT_PROLOGUE(FBaseCVar);
	if (!(self->GetFlags() & CVAR_MOD))
	{
		// Only menus are allowed to change non-mod CVARs.
		if (DMenu::InMenu == 0)
		{
			ThrowAbortException(X_OTHER, "Attempt to change CVAR '%s' outside of menu code", self->GetName());
		}
	}
	PARAM_INT(val);
	UCVarValue v;
	v.Int = val;
	self->SetGenericRep(v, CVAR_Int);
	return 0;
}

DEFINE_ACTION_FUNCTION(_CVar, SetFloat)
{
	PARAM_SELF_STRUCT_PROLOGUE(FBaseCVar);
	if (!(self->GetFlags() & CVAR_MOD))
	{
		// Only menus are allowed to change non-mod CVARs.
		if (DMenu::InMenu == 0)
		{
			ThrowAbortException(X_OTHER, "Attempt to change CVAR '%s' outside of menu code", self->GetName());
		}
	}
	PARAM_FLOAT(val);
	UCVarValue v;
	v.Float = (float)val;
	self->SetGenericRep(v, CVAR_Float);
	return 0;
}

DEFINE_ACTION_FUNCTION(_CVar, SetString)
{
	// Only menus are allowed to change CVARs.
	PARAM_SELF_STRUCT_PROLOGUE(FBaseCVar);
	if (!(self->GetFlags() & CVAR_MOD))
	{
		// Only menus are allowed to change non-mod CVARs.
		if (DMenu::InMenu == 0)
		{
			ThrowAbortException(X_OTHER, "Attempt to change CVAR '%s' outside of menu code", self->GetName());
		}
	}
	PARAM_STRING(val);
	UCVarValue v;
	v.String = val.GetChars();
	self->SetGenericRep(v, CVAR_String);
	return 0;
}

DEFINE_ACTION_FUNCTION(_CVar, GetRealType)
{
	PARAM_SELF_STRUCT_PROLOGUE(FBaseCVar);
	ACTION_RETURN_INT(self->GetRealType());
}

DEFINE_ACTION_FUNCTION(_CVar, ResetToDefault)
{
	PARAM_SELF_STRUCT_PROLOGUE(FBaseCVar);
	if (!(self->GetFlags() & CVAR_MOD))
	{
		// Only menus are allowed to change non-mod CVARs.
		if (DMenu::InMenu == 0)
		{
			ThrowAbortException(X_OTHER, "Attempt to change CVAR '%s' outside of menu code", self->GetName());
		}
	}

	self->ResetToDefault();
	return 0;
}

DEFINE_ACTION_FUNCTION(_CVar, FindCVar)
{
	PARAM_PROLOGUE;
	PARAM_NAME(name);
	ACTION_RETURN_POINTER(FindCVar(name.GetChars(), nullptr));
}

//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(FKeyBindings, SetBind)
{
	PARAM_SELF_STRUCT_PROLOGUE(FKeyBindings);
	PARAM_INT(k);
	PARAM_STRING(cmd);

	// Only menus are allowed to change bindings.
	if (DMenu::InMenu == 0)
	{
		I_FatalError("Attempt to change key bindings outside of menu code to '%s'", cmd.GetChars());
	}


	self->SetBind(k, cmd);
	return 0;
}

DEFINE_ACTION_FUNCTION(FKeyBindings, NameKeys)
{
	PARAM_PROLOGUE;
	PARAM_INT(k1);
	PARAM_INT(k2);
	char buffer[120];
	C_NameKeys(buffer, k1, k2);
	ACTION_RETURN_STRING(buffer);
}

DEFINE_ACTION_FUNCTION(FKeyBindings, NameAllKeys)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(array, TArray<int>);
	auto buffer = C_NameKeys(array->Data(), array->Size(), true);
	ACTION_RETURN_STRING(buffer);
}

DEFINE_ACTION_FUNCTION(FKeyBindings, GetKeysForCommand)
{
	PARAM_SELF_STRUCT_PROLOGUE(FKeyBindings);
	PARAM_STRING(cmd);
	int k1, k2;
	self->GetKeysForCommand(cmd.GetChars(), &k1, &k2);
	if (numret > 0) ret[0].SetInt(k1);
	if (numret > 1) ret[1].SetInt(k2);
	return MIN(numret, 2);
}

DEFINE_ACTION_FUNCTION(FKeyBindings, GetAllKeysForCommand)
{
	PARAM_SELF_STRUCT_PROLOGUE(FKeyBindings);
	PARAM_POINTER(array, TArray<int>);
	PARAM_STRING(cmd);
	*array = self->GetKeysForCommand(cmd);
	return 0;
}

DEFINE_ACTION_FUNCTION(FKeyBindings, GetBinding)
{
	PARAM_SELF_STRUCT_PROLOGUE(FKeyBindings);
	PARAM_INT(key);
	ACTION_RETURN_STRING(self->GetBinding(key));
}

DEFINE_ACTION_FUNCTION(FKeyBindings, UnbindACommand)
{
	PARAM_SELF_STRUCT_PROLOGUE(FKeyBindings);
	PARAM_STRING(cmd);

	// Only menus are allowed to change bindings.
	if (DMenu::InMenu == 0)
	{
		I_FatalError("Attempt to unbind key bindings for '%s' outside of menu code", cmd.GetChars());
	}

	self->UnbindACommand(cmd);
	return 0;
}

// This is only accessible to the special menu item to run CCMDs.
DEFINE_ACTION_FUNCTION(DOptionMenuItemCommand, DoCommand)
{
	PARAM_PROLOGUE;
	PARAM_STRING(cmd);
	PARAM_BOOL(unsafe);

	// Only menus are allowed to execute CCMDs.
	if (DMenu::InMenu == 0)
	{
		I_FatalError("Attempt to execute CCMD '%s' outside of menu code", cmd.GetChars());
	}

	UnsafeExecutionScope scope(unsafe);
	C_DoCommand(cmd);
	return 0;
}

DEFINE_ACTION_FUNCTION(_Console, HideConsole)
{
	C_HideConsole();
	return 0;
}

DEFINE_ACTION_FUNCTION(_Console, Printf)
{
	PARAM_PROLOGUE;
	PARAM_VA_POINTER(va_reginfo)	// Get the hidden type information array

	FString s = FStringFormat(VM_ARGS_NAMES);
	Printf("%s\n", s.GetChars());
	return 0;
}

DEFINE_ACTION_FUNCTION(_Console, PrintString)
{
	PARAM_PROLOGUE;
	PARAM_INT(printlevel);
	PARAM_INT(printflags);
	PARAM_VA_POINTER(va_reginfo)

	FString s = FStringFormat(VM_ARGS_NAMES, 2);
	Printf(printlevel, printflags, "%s\n", s.GetChars());
	return 0;
}

DEFINE_GLOBAL_NAMED(mus_playing, musplaying);
DEFINE_FIELD_X(MusPlayingInfo, MusPlayingInfo, name);
DEFINE_FIELD_X(MusPlayingInfo, MusPlayingInfo, baseorder);
DEFINE_FIELD_X(MusPlayingInfo, MusPlayingInfo, loop);
DEFINE_FIELD_X(MusPlayingInfo, MusPlayingInfo, handle);

DEFINE_GLOBAL_NAMED(PClass::AllClasses, AllClasses)
DEFINE_GLOBAL(Bindings)
DEFINE_GLOBAL(AutomapBindings)
DEFINE_GLOBAL(generic_ui)

DEFINE_FIELD(DStatusBarCore, RelTop);
DEFINE_FIELD(DStatusBarCore, HorizontalResolution);
DEFINE_FIELD(DStatusBarCore, VerticalResolution);
DEFINE_FIELD(DStatusBarCore, CompleteBorder);
DEFINE_FIELD(DStatusBarCore, Alpha);
DEFINE_FIELD(DStatusBarCore, drawOffset);
DEFINE_FIELD(DStatusBarCore, drawClip);
DEFINE_FIELD(DStatusBarCore, fullscreenOffsets);
DEFINE_FIELD(DStatusBarCore, defaultScale);
DEFINE_FIELD(DHUDFont, mFont);

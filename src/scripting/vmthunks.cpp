//-----------------------------------------------------------------------------
//
// Copyright 2016-2018 Christoph Oelckers
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
// VM thunks for internal functions.
//
// Important note about this file: Since everything in here is supposed to be called
// from JIT-compiled VM code it needs to be very careful about calling conventions.
// As a result none of the integer sized struct types may be used as function
// arguments, because current C++ calling conventions require them to be passed
// by reference. The JIT code, however will pass them by value so any direct native function
// taking such an argument needs to receive it as a naked int.
//
//-----------------------------------------------------------------------------

#include <time.h>
#include "vm.h"
#include "r_defs.h"
#include "g_levellocals.h"
#include "gamedata/g_mapinfo.h"
#include "s_sound.h"
#include "p_local.h"
#include "v_font.h"
#include "gstrings.h"
#include "a_keys.h"
#include "sbar.h"
#include "doomstat.h"
#include "p_acs.h"
#include "a_pickups.h"
#include "a_specialspot.h"
#include "actorptrselect.h"
#include "a_weapons.h"
#include "d_player.h"
#include "p_setup.h"
#include "am_map.h"
#include "v_video.h"
#include "gi.h"
#include "utf8.h"
#include "fontinternals.h"
#include "intermission/intermission.h"
#include "menu.h"
#include "c_cvars.h"
#include "c_bind.h"
#include "c_dispatch.h"
#include "s_music.h"
#include "texturemanager.h"
#include "v_draw.h"

DVector2 AM_GetPosition();
int Net_GetLatency(int *ld, int *ad);
void PrintPickupMessage(bool localview, const FString &str);


void SetCameraToTexture(AActor *viewpoint, const FString &texturename, double fov);

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, SetCameraToTexture, SetCameraToTexture)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(viewpoint, AActor);
	PARAM_STRING(texturename); // [ZZ] there is no point in having this as FTextureID because it's easier to refer to a cameratexture by name and it isn't executed too often to cache it.
	PARAM_FLOAT(fov);
	SetCameraToTexture(viewpoint, texturename, fov);
	return 0;
}

static void SetCameraTextureAspectRatio(const FString &texturename, double aspectScale, bool useTextureRatio)
{
	FTextureID textureid = TexMan.CheckForTexture(texturename, ETextureType::Wall, FTextureManager::TEXMAN_Overridable);
	if (textureid.isValid())
	{
		// Only proceed if the texture actually has a canvas.
		auto tex = TexMan.GetGameTexture(textureid);
		if (tex && tex->isHardwareCanvas())
		{
			static_cast<FCanvasTexture *>(tex->GetTexture())->SetAspectRatio(aspectScale, useTextureRatio);
		}
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, SetCameraTextureAspectRatio, SetCameraTextureAspectRatio)
{
	PARAM_PROLOGUE;
	PARAM_STRING(texturename);
	PARAM_FLOAT(aspect);
	PARAM_BOOL(useTextureRatio);
	SetCameraTextureAspectRatio(texturename, aspect, useTextureRatio);
	return 0;
}

//=====================================================================================
//
// sector_t exports
//
//=====================================================================================

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindLowestFloorSurrounding, FindLowestFloorSurrounding)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = FindLowestFloorSurrounding(self, &v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}


DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindHighestFloorSurrounding, FindHighestFloorSurrounding)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = FindHighestFloorSurrounding(self, &v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindNextHighestFloor, FindNextHighestFloor)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = FindNextHighestFloor(self, &v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindNextLowestFloor, FindNextLowestFloor)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = FindNextLowestFloor(self, &v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindNextLowestCeiling, FindNextLowestCeiling)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = FindNextLowestCeiling(self, &v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindNextHighestCeiling, FindNextHighestCeiling)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = FindNextHighestCeiling(self, &v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindLowestCeilingSurrounding, FindLowestCeilingSurrounding)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = FindLowestCeilingSurrounding(self, &v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindHighestCeilingSurrounding, FindHighestCeilingSurrounding)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = FindHighestCeilingSurrounding(self, &v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}


DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindMinSurroundingLight, FindMinSurroundingLight)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(min);
	auto h = FindMinSurroundingLight(self, min);
	ACTION_RETURN_INT(h);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindHighestFloorPoint, FindHighestFloorPoint)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = FindHighestFloorPoint(self, &v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}
DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindLowestCeilingPoint, FindLowestCeilingPoint)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = FindLowestCeilingPoint(self, &v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, HighestCeilingAt, HighestCeilingAt)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	sector_t *s;
	double h = HighestCeilingAt(self, x, y, &s);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(s);
	return numret;
}
DEFINE_ACTION_FUNCTION_NATIVE(_Sector, LowestFloorAt, LowestFloorAt)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	sector_t *s;
	double h = LowestFloorAt(self, x, y, &s);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(s);
	return numret;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, NextHighestCeilingAt, NextHighestCeilingAt)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(bottomz);
	PARAM_FLOAT(topz);
	PARAM_INT(flags);
	sector_t *resultsec;
	F3DFloor *resultff;
	double resultheight = NextHighestCeilingAt(self, x, y, bottomz, topz, flags, &resultsec, &resultff);

	if (numret > 2) ret[2].SetPointer(resultff);
	if (numret > 1) ret[1].SetPointer(resultsec);
	if (numret > 0) ret[0].SetFloat(resultheight);
	return numret;
}
DEFINE_ACTION_FUNCTION_NATIVE(_Sector, NextLowestFloorAt, NextLowestFloorAt)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_INT(flags);
	PARAM_FLOAT(steph);
	sector_t *resultsec;
	F3DFloor *resultff;
	double resultheight = NextLowestFloorAt(self, x, y, z, flags, steph, &resultsec, &resultff);

	if (numret > 2) ret[2].SetPointer(resultff);
	if (numret > 1) ret[1].SetPointer(resultsec);
	if (numret > 0) ret[0].SetFloat(resultheight);
	return numret;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetFriction, GetFriction)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(plane);
	double mf;
	double h = self->GetFriction(plane, &mf);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetFloat(mf);
	return numret;
}


static void GetPortalDisplacement(sector_t *sec, int plane, DVector2 *result)
{
	*result = sec->GetPortalDisplacement(plane);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetPortalDisplacement, GetPortalDisplacement)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(pos);
	ACTION_RETURN_VEC2(self->GetPortalDisplacement(pos));
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindShortestTextureAround, FindShortestTextureAround)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_FLOAT(FindShortestTextureAround(self));
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindShortestUpperAround, FindShortestUpperAround)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_FLOAT(FindShortestUpperAround(self));
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindModelFloorSector, FindModelFloorSector)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(fdh);
	auto h = FindModelFloorSector(self, fdh);
	ACTION_RETURN_POINTER(h);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindModelCeilingSector, FindModelCeilingSector)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(fdh);
	auto h = FindModelCeilingSector(self, fdh);
	ACTION_RETURN_POINTER(h);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetColor, SetColor)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_COLOR(color);
	PARAM_INT(desat);
	SetColor(self, color, desat);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetFade, SetFade)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_COLOR(fade);
	SetFade(self, fade);
	return 0;
}

static void SetSpecialColor(sector_t *self, int num, int color)
{
	if (num >= 0 && num < 5)
	{
		self->SetSpecialColor(num, color);
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetSpecialColor, SetSpecialColor)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(num);
	PARAM_COLOR(color);
	SetSpecialColor(self, num, color);
	return 0;
}

static void SetAdditiveColor(sector_t *self, int pos, int color)
{
	if (pos >= 0 && pos < 5)
	{
		self->SetAdditiveColor(pos, color);
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetAdditiveColor, SetAdditiveColor)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(pos);
	PARAM_COLOR(color);
	SetAdditiveColor(self, pos, color);
	return 0;
}

static void SetColorization(sector_t* self, int pos, int cname)
{
	if (pos >= 0 && pos < 2)
	{
		self->SetTextureFx(pos, TexMan.GetTextureManipulation(ENamedName(cname)));
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetColorization, SetColorization)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(pos);
	PARAM_INT(color);
	SetColorization(self, pos, color);
	return 0;
}


static void SetFogDensity(sector_t *self, int dens)
{
	self->Colormap.FogDensity = dens;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetFogDensity, SetFogDensity)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(dens);
	self->Colormap.FogDensity = dens;
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PlaneMoving, PlaneMoving)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(pos);
	ACTION_RETURN_BOOL(PlaneMoving(self, pos));
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetFloorLight, GetFloorLight)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_INT(self->GetFloorLight());
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetCeilingLight, GetCeilingLight)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_INT(self->GetCeilingLight());
}

static sector_t *GetHeightSec(sector_t *self)
{
	return self->GetHeightSec();
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetHeightSec, GetHeightSec)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_POINTER(self->GetHeightSec());
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetSpecial, GetSpecial)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_POINTER(spec, secspecial_t);
	GetSpecial(self, spec);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetSpecial, SetSpecial)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_POINTER(spec, secspecial_t);
	SetSpecial(self, spec);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, TransferSpecial, TransferSpecial)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_POINTER(spec, sector_t);
	TransferSpecial(self, spec);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetTerrain, GetTerrain)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(pos);
	ACTION_RETURN_INT(GetTerrain(self, pos));
}


DEFINE_ACTION_FUNCTION_NATIVE(_Sector, CheckPortalPlane, CheckPortalPlane)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(plane);
	self->CheckPortalPlane(plane);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, RemoveForceField, RemoveForceField)
 {
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	RemoveForceField(self);
	return 0;
 }

  DEFINE_ACTION_FUNCTION_NATIVE(_Sector, AdjustFloorClip, AdjustFloorClip)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 AdjustFloorClip(self);
	 return 0;
 }

static sector_t *PointInSectorXY(FLevelLocals *self, double x, double y)
{
	return self->PointInSector(x ,y);
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, PointInSector, PointInSectorXY)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	ACTION_RETURN_POINTER(PointInSectorXY(self, x, y));
}

static void SetXOffset(sector_t *self, int pos, double o)
{
	self->SetXOffset(pos, o);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetXOffset, SetXOffset)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetXOffset(pos, o);
	 return 0;
 }

 static void AddXOffset(sector_t *self, int pos, double o)
 {
	 self->AddXOffset(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, AddXOffset, AddXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->AddXOffset(pos, o);
	 return 0;
 }

 static double GetXOffset(sector_t *self, int pos)
 {
	 return self->GetXOffset(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetXOffset, GetXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetXOffset(pos));
 }

 static void SetYOffset(sector_t *self, int pos, double o)
 {
	 self->SetYOffset(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetYOffset, SetYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetXOffset(pos, o);
	 return 0;
 }

 static void AddYOffset(sector_t *self, int pos, double o)
 {
	 self->AddYOffset(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, AddYOffset, AddYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->AddYOffset(pos, o);
	 return 0;
 }

 static double GetYOffset(sector_t *self, int pos)
 {
	 return self->GetYOffset(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetYOffset, GetYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_BOOL(addbase);
	 ACTION_RETURN_FLOAT(self->GetYOffset(pos, addbase));
 }

 static void SetXScale(sector_t *self, int pos, double o)
 {
	 self->SetXScale(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetXScale, SetXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetXScale(pos, o);
	 return 0;
 }

 static double GetXScale(sector_t *self, int pos)
 {
	 return self->GetXScale(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetXScale, GetXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetXScale(pos));
 }

 static void SetYScale(sector_t *self, int pos, double o)
 {
	 self->SetYScale(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetYScale, SetYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetYScale(pos, o);
	 return 0;
 }

 static double GetYScale(sector_t *self, int pos)
 {
	 return self->GetYScale(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetYScale, GetYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetYScale(pos));
 }

 static void SetAngle(sector_t *self, int pos, double o)
 {
	 self->SetAngle(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetAngle, SetAngle)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_ANGLE(o);
	 self->SetAngle(pos, o);
	 return 0;
 }

 static double GetAngle(sector_t *self, int pos, bool addbase)
 {
	 return self->GetAngle(pos, addbase).Degrees;
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetAngle, GetAngle)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_BOOL(addbase);
	 ACTION_RETURN_FLOAT(self->GetAngle(pos, addbase).Degrees);
 }

 static void SetBase(sector_t *self, int pos, double o, double a)
 {
	 self->SetBase(pos, o, a);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetBase, SetBase)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 PARAM_ANGLE(a);
	 self->SetBase(pos, o, a);
	 return 0;
 }

 static void SetAlpha(sector_t *self, int pos, double o)
 {
	 self->SetAlpha(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetAlpha, SetAlpha)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetAlpha(pos, o);
	 return 0;
 }

 static double GetAlpha(sector_t *self, int pos)
 {
	 return self->GetAlpha(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetAlpha, GetAlpha)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetAlpha(pos));
 }

 static int GetFlags(sector_t *self, int pos)
 {
	 return self->GetFlags(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetFlags, GetFlags)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetFlags(pos));
 }

 static int GetVisFlags(sector_t *self, int pos)
 {
	 return self->GetVisFlags(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetVisFlags, GetVisFlags)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetVisFlags(pos));
 }

 static void ChangeFlags(sector_t *self, int pos, int a, int o)
 {
	 self->ChangeFlags(pos, a, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, ChangeFlags, ChangeFlags)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_INT(a);
	 PARAM_INT(o);
	 self->ChangeFlags(pos, a, o);
	 return 0;
 }

 static void SetPlaneLight(sector_t *self, int pos, int o)
 {
	 self->SetPlaneLight(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetPlaneLight, SetPlaneLight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_INT(o);
	 self->SetPlaneLight(pos, o);
	 return 0;
 }

 static int GetPlaneLight(sector_t *self, int pos)
 {
	 return self->GetPlaneLight(pos);
 }

  DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetPlaneLight, GetPlaneLight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetPlaneLight(pos));
 }

  static void SetTexture(sector_t *self, int pos, int o, bool adj)
  {
	  self->SetTexture(pos, FSetTextureID(o), adj);
  }

  DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetTexture, SetTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_INT(o);
	 PARAM_BOOL(adj);
	 self->SetTexture(pos, FSetTextureID(o), adj);
	 return 0;
 }

  static int GetSectorTexture(sector_t *self, int pos)
  {
	  return self->GetTexture(pos).GetIndex();
  }

  DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetTexture, GetSectorTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetTexture(pos).GetIndex());
 }

  static void SetPlaneTexZ(sector_t *self, int pos, double o, bool)
  {
	  self->SetPlaneTexZ(pos, o, true);	// not setting 'dirty' here is a guaranteed cause for problems.
  }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetPlaneTexZ, SetPlaneTexZ)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 PARAM_BOOL(dirty);
	 self->SetPlaneTexZ(pos, o, true);	// not setting 'dirty' here is a guaranteed cause for problems.
	 return 0;
 }

 static double GetPlaneTexZ(sector_t *self, int pos)
 {
	 return self->GetPlaneTexZ(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetPlaneTexZ, GetPlaneTexZ)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetPlaneTexZ(pos));
 }

 static void SetLightLevel(sector_t *self, int o)
 {
	 self->SetLightLevel(o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetLightLevel, SetLightLevel)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(o);
	 self->SetLightLevel(o);
	 return 0;
 }

 static void ChangeLightLevel(sector_t *self, int o)
 {
	 self->ChangeLightLevel(o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, ChangeLightLevel, ChangeLightLevel)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(o);
	 self->ChangeLightLevel(o);
	 return 0;
 }

 static int GetLightLevel(sector_t *self)
 {
	 return self->GetLightLevel();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetLightLevel, GetLightLevel)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 ACTION_RETURN_INT(self->GetLightLevel());
 }

 static int PortalBlocksView(sector_t *self, int pos)
 {
	 return self->PortalBlocksView(pos);
 }
 
 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PortalBlocksView, PortalBlocksView)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksView(pos));
 }

 static int PortalBlocksSight(sector_t *self, int pos)
 {
	 return self->PortalBlocksSight(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PortalBlocksSight, PortalBlocksSight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksSight(pos));
 }

 static int PortalBlocksMovement(sector_t *self, int pos)
 {
	 return self->PortalBlocksMovement(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PortalBlocksMovement, PortalBlocksMovement)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksMovement(pos));
 }

 static int PortalBlocksSound(sector_t *self, int pos)
 {
	 return self->PortalBlocksSound(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PortalBlocksSound, PortalBlocksSound)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksSound(pos));
 }

 static int PortalIsLinked(sector_t *self, int pos)
 {
	 return self->PortalIsLinked(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PortalIsLinked, PortalIsLinked)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalIsLinked(pos));
 }

 static void ClearPortal(sector_t *self, int pos)
 {
	 self->ClearPortal(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, ClearPortal, ClearPortal)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 self->ClearPortal(pos);
	 return 0;
 }

 static double GetPortalPlaneZ(sector_t *self, int pos)
 {
	 return self->GetPortalPlaneZ(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetPortalPlaneZ, GetPortalPlaneZ)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetPortalPlaneZ(pos));
 }

 static int GetPortalType(sector_t *self, int pos)
 {
	 return self->GetPortalType(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetPortalType, GetPortalType)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetPortalType(pos));
 }

 static int GetOppositePortalGroup(sector_t *self, int pos)
 {
	 return self->GetOppositePortalGroup(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetOppositePortalGroup, GetOppositePortalGroup)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetOppositePortalGroup(pos));
 }

 static double CenterFloor(sector_t *self)
 {
	 return self->CenterFloor();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, CenterFloor, CenterFloor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 ACTION_RETURN_FLOAT(self->CenterFloor());
 }

 static double CenterCeiling(sector_t *self)
 {
	 return self->CenterCeiling();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, CenterCeiling, CenterCeiling)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 ACTION_RETURN_FLOAT(self->CenterCeiling());
 }

 static int SectorIndex(sector_t *self)
 {
	 return self->Index();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, Index, SectorIndex)
 {
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_INT(SectorIndex(self));
 }

 static void SetEnvironmentID(sector_t *self, int envnum)
 {
	 self->Level->Zones[self->ZoneNumber].Environment = S_FindEnvironment(envnum);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetEnvironmentID, SetEnvironmentID)
 {
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(envnum);
	SetEnvironmentID(self, envnum);
	return 0;
 }

 static void SetEnvironment(sector_t *self, const FString &env)
 {
	 self->Level->Zones[self->ZoneNumber].Environment = S_FindEnvironment(env);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetEnvironment, SetEnvironment)
 {
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_STRING(env);
	SetEnvironment(self, env);
	return 0;
 }

 static double GetGlowHeight(sector_t *self, int pos)
 {
	 return self->GetGlowHeight(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetGlowHeight, GetGlowHeight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetGlowHeight(pos));
 }

 static double GetGlowColor(sector_t *self, int pos)
 {
	 return self->GetGlowColor(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetGlowColor, GetGlowColor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetGlowColor(pos));
 }

 static void SetGlowHeight(sector_t *self, int pos, double o)
 {
	 self->SetGlowHeight(pos, float(o));
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetGlowHeight, SetGlowHeight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetGlowHeight(pos, float(o));
	 return 0;
 }

 static void SetGlowColor(sector_t *self, int pos, int o)
 {
	 self->SetGlowColor(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetGlowColor, SetGlowColor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_COLOR(o);
	 self->SetGlowColor(pos, o);
	 return 0;
 }

 static F3DFloor* Get3DFloor(sector_t *self, unsigned int index)
 {
 	 if (index >= self->e->XFloor.ffloors.Size())
 	 	return nullptr;
	 return self->e->XFloor.ffloors[index];
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, Get3DFloor, Get3DFloor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(index);
	 ACTION_RETURN_POINTER(Get3DFloor(self,index));
 }

 static int Get3DFloorCount(sector_t *self)
 {
	 return self->e->XFloor.ffloors.Size();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, Get3DFloorCount, Get3DFloorCount)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 ACTION_RETURN_INT(self->e->XFloor.ffloors.Size());
 }

 static sector_t* GetAttached(sector_t *self, unsigned int index)
 {
 	 if (index >= self->e->XFloor.attached.Size())
 	 	return nullptr;
	 return self->e->XFloor.attached[index];
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetAttached, GetAttached)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(index);
	 ACTION_RETURN_POINTER(GetAttached(self,index));
 }

 static int GetAttachedCount(sector_t *self)
 {
	 return self->e->XFloor.attached.Size();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetAttachedCount, GetAttachedCount)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 ACTION_RETURN_INT(self->e->XFloor.attached.Size());
 }

 static int Get3DFloorTexture(F3DFloor *self, int pos)
 {
 	 if ( pos )
 		 return self->bottom.texture->GetIndex();
 	 return self->top.texture->GetIndex();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_F3DFloor, GetTexture, Get3DFloorTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(F3DFloor);
	 PARAM_INT(pos);
	 if ( pos )
		 ACTION_RETURN_INT(self->bottom.texture->GetIndex());
	 ACTION_RETURN_INT(self->top.texture->GetIndex());
 }

 //===========================================================================
 //
 //  line_t exports
 //
 //===========================================================================

 static int isLinePortal(line_t *self)
 {
	 return self->isLinePortal();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Line, isLinePortal, isLinePortal)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_BOOL(self->isLinePortal());
 }

 static int isVisualPortal(line_t *self)
 {
	 return self->isVisualPortal();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Line, isVisualPortal, isVisualPortal)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_BOOL(self->isVisualPortal());
 }

 static line_t *getPortalDestination(line_t *self)
 {
	 return self->getPortalDestination();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Line, getPortalDestination, getPortalDestination)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_POINTER(self->getPortalDestination());
 }

 static int getPortalAlignment(line_t *self)
 {
	 return self->getPortalAlignment();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Line, getPortalAlignment, getPortalAlignment)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_INT(self->getPortalAlignment());
 }

 static int LineIndex(line_t *self)
 {
	 return self->Index();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Line, Index, LineIndex)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_INT(LineIndex(self));
 }

 //===========================================================================
 //
 // side_t exports 
 //
 //===========================================================================

 static int GetSideTexture(side_t *self, int which)
 {
	return self->GetTexture(which).GetIndex();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetTexture, GetSideTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_INT(self->GetTexture(which).GetIndex());
 }

 static void SetSideTexture(side_t *self, int which, int tex)
 {
	 self->SetTexture(which, FSetTextureID(tex));
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetTexture, SetSideTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_INT(tex);
	 self->SetTexture(which, FSetTextureID(tex));
	 return 0;
 }

 static void SetTextureXOffset(side_t *self, int which, double ofs)
 {
	 self->SetTextureXOffset(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetTextureXOffset, SetTextureXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureXOffset(which, ofs);
	 return 0;
 }

 static void AddTextureXOffset(side_t *self, int which, double ofs)
 {
	 self->AddTextureXOffset(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, AddTextureXOffset, AddTextureXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->AddTextureXOffset(which, ofs);
	 return 0;
 }

 static double GetTextureXOffset(side_t *self, int which)
 {
	 return self->GetTextureXOffset(which);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetTextureXOffset, GetTextureXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureXOffset(which));
 }

 static void SetTextureYOffset(side_t *self, int which, double ofs)
 {
	 self->SetTextureYOffset(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetTextureYOffset, SetTextureYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureYOffset(which, ofs);
	 return 0;
 }

 static void AddTextureYOffset(side_t *self, int which, double ofs)
 {
	 self->AddTextureYOffset(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, AddTextureYOffset, AddTextureYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->AddTextureYOffset(which, ofs);
	 return 0;
 }

 static double GetTextureYOffset(side_t *self, int which)
 {
	 return self->GetTextureYOffset(which);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetTextureYOffset, GetTextureYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureYOffset(which));
 }

 static void SetTextureXScale(side_t *self, int which, double ofs)
 {
	 self->SetTextureXScale(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetTextureXScale, SetTextureXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureXScale(which, ofs);
	 return 0;
 }

 static void MultiplyTextureXScale(side_t *self, int which, double ofs)
 {
	 self->MultiplyTextureXScale(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, MultiplyTextureXScale, MultiplyTextureXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->MultiplyTextureXScale(which, ofs);
	 return 0;
 }

 static double GetTextureXScale(side_t *self, int which)
 {
	 return self->GetTextureXScale(which);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetTextureXScale, GetTextureXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureXScale(which));
 }

 static void SetTextureYScale(side_t *self, int which, double ofs)
 {
	 self->SetTextureYScale(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetTextureYScale, SetTextureYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureYScale(which, ofs);
	 return 0;
 }

 static void MultiplyTextureYScale(side_t *self, int which, double ofs)
 {
	 self->MultiplyTextureYScale(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, MultiplyTextureYScale, MultiplyTextureYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->MultiplyTextureYScale(which, ofs);
	 return 0;
 }

 static double GetTextureYScale(side_t *self, int which)
 {
	 return self->GetTextureYScale(which);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetTextureYScale, GetTextureYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureYScale(which));
 }

 static vertex_t *GetSideV1(side_t *self)
 {
	 return self->V1();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, V1, GetSideV1)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 ACTION_RETURN_POINTER(self->V1());
 }

 static vertex_t *GetSideV2(side_t *self)
 {
	 return self->V2();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, V2, GetSideV2)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 ACTION_RETURN_POINTER(self->V2());
 }

 static int GetTextureFlags(side_t* self, int tier)
 {
	 return self->GetTextureFlags(tier);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetTextureFlags, GetTextureFlags)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(tier);
	 ACTION_RETURN_INT(self->GetTextureFlags(tier));
}

 static void ChangeTextureFlags(side_t* self, int tier, int And, int Or)
 {
	 self->ChangeTextureFlags(tier, And, Or);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, ChangeTextureFlags, ChangeTextureFlags)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(tier);
	 PARAM_INT(a);
	 PARAM_INT(o);
	 ChangeTextureFlags(self, tier, a, o);
	 return 0;
 }

 static void SetSideSpecialColor(side_t *self, int tier, int position, int color, int useown)
 {
	 if (tier >= 0 && tier < 3 && position >= 0 && position < 2)
	 {
		 self->SetSpecialColor(tier, position, color, useown);
	 }
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetSpecialColor, SetSideSpecialColor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(tier);
	 PARAM_INT(position);
	 PARAM_COLOR(color);
	 PARAM_BOOL(useown)
	 SetSideSpecialColor(self, tier, position, color, useown);
	 return 0;
 }

 static int GetSideAdditiveColor(side_t *self, int tier)
 {
	 if (tier >= 0 && tier < 3)
	 {
		 return self->GetAdditiveColor(tier, self->sector);
	 }
	 return 0;
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetAdditiveColor, GetSideAdditiveColor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(tier);
	 ACTION_RETURN_INT(GetSideAdditiveColor(self, tier));
	 return 0;
 }

 static void SetSideAdditiveColor(side_t *self, int tier, int color)
 {
	 if (tier >= 0 && tier < 3)
	 {
		 self->SetAdditiveColor(tier, color);
	 }
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetAdditiveColor, SetSideAdditiveColor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(tier);
	 PARAM_COLOR(color);
	 SetSideAdditiveColor(self, tier, color);
	 return 0;
 }

 static void EnableSideAdditiveColor(side_t *self, int tier, bool enable)
 {
	 if (tier >= 0 && tier < 3)
	 {
		 self->EnableAdditiveColor(tier, enable);
	 }
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, EnableAdditiveColor, EnableSideAdditiveColor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(tier);
	 PARAM_BOOL(enable);
	 EnableSideAdditiveColor(self, tier, enable);
	 return 0;
 }

 static void SetWallColorization(side_t* self, int pos, int cname)
 {
	 if (pos >= 0 && pos < 3)
	 {
		 self->SetTextureFx(pos, TexMan.GetTextureManipulation(ENamedName(cname)));
	 }
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetColorization, SetWallColorization)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(pos);
	 PARAM_INT(color);
	 SetWallColorization(self, pos, color);
	 return 0;
 }



 static int SideIndex(side_t *self)
 {
	 return self->Index();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, Index, SideIndex)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 ACTION_RETURN_INT(SideIndex(self));
 }

 //=====================================================================================
//
 // vertex_t exports
//
//=====================================================================================

 static int VertexIndex(vertex_t *self)
 {
	 return self->Index();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Vertex, Index, VertexIndex)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(vertex_t);
	 ACTION_RETURN_INT(VertexIndex(self));
 }

 //=====================================================================================
//
// TexMan exports
//
//=====================================================================================

 static int IsJumpingAllowed(FLevelLocals *self)
 {
	 return self->IsJumpingAllowed();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, IsJumpingAllowed, IsJumpingAllowed)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	 ACTION_RETURN_BOOL(self->IsJumpingAllowed());
 }

 //==========================================================================
 //
 //
 //==========================================================================

 static int IsCrouchingAllowed(FLevelLocals *self)
 {
	 return self->IsCrouchingAllowed();
 }


 DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, IsCrouchingAllowed, IsCrouchingAllowed)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	 ACTION_RETURN_BOOL(self->IsCrouchingAllowed());
 }

 //==========================================================================
 //
 //
 //==========================================================================

 static int IsFreelookAllowed(FLevelLocals *self)
 {
	 return self->IsFreelookAllowed();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, IsFreelookAllowed, IsFreelookAllowed)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	 ACTION_RETURN_BOOL(self->IsFreelookAllowed());
 }
 
 //==========================================================================
//
// ZScript counterpart to ACS ChangeSky, uses TextureIDs
//
//==========================================================================
 DEFINE_ACTION_FUNCTION(FLevelLocals, ChangeSky)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	 PARAM_INT(sky1);
	 PARAM_INT(sky2);
	 self->skytexture1 = FSetTextureID(sky1);
	 self->skytexture2 = FSetTextureID(sky2);
	 InitSkyMap(self);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(FLevelLocals, StartIntermission)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	 PARAM_NAME(seq);
	 PARAM_INT(state);
	 F_StartIntermission(seq, (uint8_t)state);
	 return 0;
 }


 // This is needed to convert the strings to char pointers.
 static void ReplaceTextures(FLevelLocals *self, const FString &from, const FString &to, int flags)
 {
	 self->ReplaceTextures(from, to, flags);
 }

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, ReplaceTextures, ReplaceTextures)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_STRING(from);
	PARAM_STRING(to);
	PARAM_INT(flags);
	self->ReplaceTextures(from, to, flags);
	return 0;
}

//=====================================================================================
//
// secplane_t exports
//
//=====================================================================================

static int isSlope(secplane_t *self)
{
	return !self->normal.XY().isZero();
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, isSlope, isSlope)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	ACTION_RETURN_BOOL(!self->normal.XY().isZero());
}

static int PointOnSide(const secplane_t *self, double x, double y, double z)
{
	return self->PointOnSide(DVector3(x, y, z));
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, PointOnSide, PointOnSide)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	ACTION_RETURN_INT(self->PointOnSide(DVector3(x, y, z)));
}

static double ZatPoint(const secplane_t *self, double x, double y)
{
	return self->ZatPoint(x, y);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, ZatPoint, ZatPoint)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	ACTION_RETURN_FLOAT(self->ZatPoint(x, y));
}

static double ZatPointDist(const secplane_t *self, double x, double y, double d)
{
	return (d + self->normal.X*x + self->normal.Y*y) * self->negiC;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, ZatPointDist, ZatPointDist)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(d);
	ACTION_RETURN_FLOAT(ZatPointDist(self, x, y, d));
}

static int isPlaneEqual(const secplane_t *self, const secplane_t *other)
{
	return *self == *other;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, isEqual, isPlaneEqual)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_POINTER(other, secplane_t);
	ACTION_RETURN_BOOL(*self == *other);
}

static void ChangeHeight(secplane_t *self, double hdiff)
{
	self->ChangeHeight(hdiff);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, ChangeHeight, ChangeHeight)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(hdiff);
	self->ChangeHeight(hdiff);
	return 0;
}

static double GetChangedHeight(const secplane_t *self, double hdiff)
{
	return self->GetChangedHeight(hdiff);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, GetChangedHeight, GetChangedHeight)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(hdiff);
	ACTION_RETURN_FLOAT(self->GetChangedHeight(hdiff));
}

static double HeightDiff(const secplane_t *self, double oldd, double newd)
{
	if (newd != 1e37)
	{
		return self->HeightDiff(oldd);
	}
	else
	{
		return self->HeightDiff(oldd, newd);
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, HeightDiff, HeightDiff)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(oldd);
	PARAM_FLOAT(newd);
	ACTION_RETURN_FLOAT(HeightDiff(self, oldd, newd));
}

static double PointToDist(const secplane_t *self, double x, double y, double z)
{
	return self->PointToDist(DVector2(x, y), z);
}


DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, PointToDist, PointToDist)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	ACTION_RETURN_FLOAT(self->PointToDist(DVector2(x, y), z));
}

//=====================================================================================
//
// WeaponSlots exports
//
//=====================================================================================

static int LocateWeapon(FWeaponSlots *self, PClassActor *weap, int *pslot, int *pindex)
{
	return self->LocateWeapon(weap, pslot, pindex);
}

DEFINE_ACTION_FUNCTION_NATIVE(FWeaponSlots, LocateWeapon, LocateWeapon)
{
	PARAM_SELF_STRUCT_PROLOGUE(FWeaponSlots);
	PARAM_CLASS(weap, AActor);
	int slot = 0, index = 0;
	bool retv = self->LocateWeapon(weap, &slot, &index);
	if (numret >= 1) ret[0].SetInt(retv);
	if (numret >= 2) ret[1].SetInt(slot);
	if (numret >= 3) ret[2].SetInt(index);
	return MIN(numret, 3);
}

static PClassActor *GetWeapon(FWeaponSlots *self, int slot, int index)
{
	return self->GetWeapon(slot, index);
}

DEFINE_ACTION_FUNCTION_NATIVE(FWeaponSlots, GetWeapon, GetWeapon)
{
	PARAM_SELF_STRUCT_PROLOGUE(FWeaponSlots);
	PARAM_INT(slot);
	PARAM_INT(index);
	ACTION_RETURN_POINTER(self->GetWeapon(slot, index));
	return 1;
}

static int SlotSize(FWeaponSlots *self, int slot)
{
	return self->SlotSize(slot);
}

DEFINE_ACTION_FUNCTION_NATIVE(FWeaponSlots, SlotSize, SlotSize)
{
	PARAM_SELF_STRUCT_PROLOGUE(FWeaponSlots);
	PARAM_INT(slot);
	ACTION_RETURN_INT(self->SlotSize(slot));
	return 1;
}

DEFINE_ACTION_FUNCTION_NATIVE(FWeaponSlots, SetupWeaponSlots, FWeaponSlots::SetupWeaponSlots)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(pawn, AActor);
	FWeaponSlots::SetupWeaponSlots(pawn);
	return 0;
}

//=====================================================================================
//
// SpotState exports
//
//=====================================================================================


static void AddSpot(DSpotState *state, AActor *spot)
{
	state->AddSpot(spot);
}

DEFINE_ACTION_FUNCTION_NATIVE(DSpotState, AddSpot, AddSpot)
{
	PARAM_SELF_PROLOGUE(DSpotState);
	PARAM_OBJECT(spot, AActor);
	self->AddSpot(spot);
	return 0;
}

static void RemoveSpot(DSpotState *state, AActor *spot)
{
	state->RemoveSpot(spot);
}

DEFINE_ACTION_FUNCTION_NATIVE(DSpotState, RemoveSpot, RemoveSpot)
{
	PARAM_SELF_PROLOGUE(DSpotState);
	PARAM_OBJECT(spot, AActor);
	self->RemoveSpot(spot);
	return 0;
}

static AActor *GetNextInList(DSpotState *self, PClassActor *type, int skipcounter)
{
	return self->GetNextInList(type, skipcounter);
}

DEFINE_ACTION_FUNCTION_NATIVE(DSpotState, GetNextInList, GetNextInList)
{
	PARAM_SELF_PROLOGUE(DSpotState);
	PARAM_CLASS(type, AActor);
	PARAM_INT(skipcounter);
	ACTION_RETURN_OBJECT(self->GetNextInList(type, skipcounter));
}

static AActor *GetSpotWithMinMaxDistance(DSpotState *self, PClassActor *type, double x, double y, double mindist, double maxdist)
{
	return self->GetSpotWithMinMaxDistance(type, x, y, mindist, maxdist);
}

DEFINE_ACTION_FUNCTION_NATIVE(DSpotState, GetSpotWithMinMaxDistance, GetSpotWithMinMaxDistance)
{
	PARAM_SELF_PROLOGUE(DSpotState);
	PARAM_CLASS(type, AActor);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(mindist);
	PARAM_FLOAT(maxdist);
	ACTION_RETURN_OBJECT(self->GetSpotWithMinMaxDistance(type, x, y, mindist, maxdist));
}

static AActor *GetRandomSpot(DSpotState *self, PClassActor *type, bool onlyonce)
{
	return self->GetRandomSpot(type, onlyonce);
}

DEFINE_ACTION_FUNCTION_NATIVE(DSpotState, GetRandomSpot, GetRandomSpot)
{
	PARAM_SELF_PROLOGUE(DSpotState);
	PARAM_CLASS(type, AActor);
	PARAM_BOOL(onlyonce);
	ACTION_RETURN_POINTER(self->GetRandomSpot(type, onlyonce));
}

//=====================================================================================
//
// Statusbar exports
//
//=====================================================================================

static void UpdateScreenGeometry(DBaseStatusBar *)
{
	setsizeneeded = true;
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, UpdateScreenGeometry, UpdateScreenGeometry)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	setsizeneeded = true;
	return 0;
}

static void SBar_Tick(DBaseStatusBar *self)
{
	self->Tick();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, Tick, SBar_Tick)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	self->Tick();
	return 0;
}

static void SBar_AttachMessage(DBaseStatusBar *self, DHUDMessageBase *msg, unsigned id, int layer)
{
	self->AttachMessage(msg, id, layer);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, AttachMessage, SBar_AttachMessage)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_OBJECT(msg, DHUDMessageBase);
	PARAM_UINT(id);
	PARAM_INT(layer);
	self->AttachMessage(msg, id, layer);
	return 0;
}

static void SBar_DetachMessage(DBaseStatusBar *self, DHUDMessageBase *msg)
{
	self->DetachMessage(msg);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, DetachMessage, SBar_DetachMessage)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_OBJECT(msg, DHUDMessageBase);
	ACTION_RETURN_OBJECT(self->DetachMessage(msg));
}

static void SBar_DetachMessageID(DBaseStatusBar *self, unsigned id)
{
	self->DetachMessage(id);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, DetachMessageID, SBar_DetachMessageID)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_INT(id);
	ACTION_RETURN_OBJECT(self->DetachMessage(id));
}

static void SBar_DetachAllMessages(DBaseStatusBar *self)
{
	self->DetachAllMessages();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, DetachAllMessages, SBar_DetachAllMessages)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	self->DetachAllMessages();
	return 0;
}

static void SBar_Draw(DBaseStatusBar *self, int state, double ticFrac)
{
	self->Draw((EHudState)state, ticFrac);
}


DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, Draw, SBar_Draw)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_INT(state);
	PARAM_FLOAT(ticFrac);
	self->Draw((EHudState)state, ticFrac);
	return 0;
}

static void SetMugshotState(DBaseStatusBar *self, const FString &statename, bool wait, bool reset)
{
	self->mugshot.SetState(statename, wait, reset);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, SetMugshotState, SetMugshotState)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_STRING(statename);
	PARAM_BOOL(wait);
	PARAM_BOOL(reset);
	self->mugshot.SetState(statename, wait, reset);
	return 0;
}

static void SBar_ScreenSizeChanged(DBaseStatusBar *self)
{
	self->ScreenSizeChanged();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, ScreenSizeChanged, SBar_ScreenSizeChanged)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	self->ScreenSizeChanged();
	return 0;
}

static int GetTopOfStatusbar(DBaseStatusBar *self)
{
	return self->GetTopOfStatusbar();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, GetTopOfStatusbar, GetTopOfStatusbar)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	ACTION_RETURN_INT(self->GetTopOfStatusbar());
}

static void GetGlobalACSString(int index, FString *result)
{
	*result = primaryLevel->Behaviors.LookupString(ACS_GlobalVars[index]);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, GetGlobalACSString, GetGlobalACSString)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	FString res;
	GetGlobalACSString(index, &res);
	ACTION_RETURN_STRING(res);
}

static void GetGlobalACSArrayString(int arrayno, int index, FString *result)
{
	*result = primaryLevel->Behaviors.LookupString(ACS_GlobalVars[index]);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, GetGlobalACSArrayString, GetGlobalACSArrayString)
{
	PARAM_PROLOGUE;
	PARAM_INT(arrayno);
	PARAM_INT(index);
	FString res;
	GetGlobalACSArrayString(arrayno, index, &res);
	ACTION_RETURN_STRING(res);
}

static int GetGlobalACSValue(int index)
{
	return (ACS_GlobalVars[index]);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, GetGlobalACSValue, GetGlobalACSValue)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	ACTION_RETURN_INT(ACS_GlobalVars[index]);
}

static int GetGlobalACSArrayValue(int arrayno, int index)
{
	return (ACS_GlobalArrays[arrayno][index]);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, GetGlobalACSArrayValue, GetGlobalACSArrayValue)
{
	PARAM_PROLOGUE;
	PARAM_INT(arrayno);
	PARAM_INT(index);
	ACTION_RETURN_INT(ACS_GlobalArrays[arrayno][index]);
}

static void ReceivedWeapon(DBaseStatusBar *self)
{
	self->mugshot.Grin();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, ReceivedWeapon, ReceivedWeapon)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	ReceivedWeapon(self);
	return 0;
}

static int GetMugshot(DBaseStatusBar *self, int accuracy, int stateflags, const FString &def_face)
{
	auto tex = self->mugshot.GetFace(self->CPlayer, def_face, accuracy, (FMugShot::StateFlags)stateflags);
	return (tex ? tex->GetID().GetIndex() : -1);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, GetMugshot, GetMugshot)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_INT(accuracy);
	PARAM_INT(stateflags);
	PARAM_STRING(def_face);
	ACTION_RETURN_INT(GetMugshot(self, accuracy, stateflags, def_face));
}

DEFINE_ACTION_FUNCTION_NATIVE(DBaseStatusBar, GetInventoryIcon, GetInventoryIcon)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(item, AActor);
	PARAM_INT(flags);
	int applyscale;
	FTextureID icon = FSetTextureID(GetInventoryIcon(item, flags, &applyscale));
	if (numret >= 1) ret[0].SetInt(icon.GetIndex());
	if (numret >= 2) ret[1].SetInt(applyscale);
	return MIN(numret, 2);
}

//=====================================================================================
//
//
//
//=====================================================================================

DSpotState *GetSpotState(FLevelLocals *self, int create)
{
	if (create && self->SpotState == nullptr) self->SpotState = Create<DSpotState>();
	GC::WriteBarrier(self->SpotState);
	return self->SpotState;
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, GetSpotState, GetSpotState)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(create);
	ACTION_RETURN_POINTER(GetSpotState(self, create));
}


//---------------------------------------------------------------------------
//
// Format the map name, include the map label if wanted
//
//---------------------------------------------------------------------------

EXTERN_CVAR(Int, am_showmaplabel)

void FormatMapName(FLevelLocals *self, int cr, FString *result)
{
	char mapnamecolor[3] = { '\34', char(cr + 'A'), 0 };

	cluster_info_t *cluster = FindClusterInfo(self->cluster);
	bool ishub = (cluster != nullptr && (cluster->flags & CLUSTER_HUB));

	*result = "";
	if (am_showmaplabel == 1 || (am_showmaplabel == 2 && !ishub))
	{
		*result << self->MapName << ": ";
	}
	*result << mapnamecolor << self->LevelName;
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, FormatMapName, FormatMapName)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(cr);
	FString rets;
	FormatMapName(self, cr, &rets);
	ACTION_RETURN_STRING(rets);
}

static void GetAutomapPosition(FLevelLocals *self, DVector2 *pos)
{
 	*pos = self->automap->GetPosition();
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, GetAutomapPosition, GetAutomapPosition)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	DVector2 result;
	GetAutomapPosition(self, &result);
	ACTION_RETURN_VEC2(result);
}

static int ZGetUDMFInt(FLevelLocals *self, int type, int index, int key)
{
	return GetUDMFInt(self,type, index, ENamedName(key));
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, GetUDMFInt, ZGetUDMFInt)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(type);
	PARAM_INT(index);
	PARAM_NAME(key);
	ACTION_RETURN_INT(GetUDMFInt(self, type, index, key));
}

static double ZGetUDMFFloat(FLevelLocals *self, int type, int index, int key)
{
	return GetUDMFFloat(self, type, index, ENamedName(key));
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, GetUDMFFloat, ZGetUDMFFloat)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(type);
	PARAM_INT(index);
	PARAM_NAME(key);
	ACTION_RETURN_FLOAT(GetUDMFFloat(self, type, index, key));
}

static void ZGetUDMFString(FLevelLocals *self, int type, int index, int key, FString *result)
{
	*result = GetUDMFString(self, type, index, ENamedName(key));
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, GetUDMFString, ZGetUDMFString)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(type);
	PARAM_INT(index);
	PARAM_NAME(key);
	ACTION_RETURN_STRING(GetUDMFString(self, type, index, key));
}

DEFINE_ACTION_FUNCTION(FLevelLocals, GetChecksum)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	char md5string[33];

	for (int j = 0; j < 16; ++j)
	{
		sprintf(md5string + j * 2, "%02x", self->md5[j]);
	}

	ACTION_RETURN_STRING((const char*)md5string);
}

static void Vec2Offset(FLevelLocals *Level, double x, double y, double dx, double dy, bool absolute, DVector2 *result)
{
	if (absolute)
	{
		*result = (DVector2(x + dx, y + dy));
	}
	else
	{
		*result = Level->GetPortalOffsetPosition(x, y, dx, dy);
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, Vec2Offset, Vec2Offset)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(dx);
	PARAM_FLOAT(dy);
	PARAM_BOOL(absolute);
	DVector2 result;
	Vec2Offset(self, x, y, dx, dy, absolute, &result);
	ACTION_RETURN_VEC2(result);
}

static void Vec2OffsetZ(FLevelLocals *Level, double x, double y, double dx, double dy, double atz, bool absolute, DVector3 *result)
{
	if (absolute)
	{
		*result = (DVector3(x + dx, y + dy, atz));
	}
	else
	{
		DVector2 v = Level->GetPortalOffsetPosition(x, y, dx, dy);
		*result = (DVector3(v, atz));
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, Vec2OffsetZ, Vec2OffsetZ)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(dx);
	PARAM_FLOAT(dy);
	PARAM_FLOAT(atz);
	PARAM_BOOL(absolute);
	DVector3 result;
	Vec2OffsetZ(self, x, y, dx, dy, atz, absolute, &result);
	ACTION_RETURN_VEC3(result);
}

static void Vec3Offset(FLevelLocals *Level, double x, double y, double z, double dx, double dy, double dz, bool absolute, DVector3 *result)
{
	if (absolute)
	{
		*result = (DVector3(x + dx, y + dy, z + dz));
	}
	else
	{
		DVector2 v = Level->GetPortalOffsetPosition(x, y, dx, dy);
		*result = (DVector3(v, z + dz));
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, Vec3Offset, Vec3Offset)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_FLOAT(dx);
	PARAM_FLOAT(dy);
	PARAM_FLOAT(dz);
	PARAM_BOOL(absolute);
	DVector3 result;
	Vec3Offset(self, x, y, z, dx, dy, dz, absolute, &result);
	ACTION_RETURN_VEC3(result);
}

int IsPointInMap(FLevelLocals *Level, double x, double y, double z);

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, IsPointInLevel, IsPointInMap)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	ACTION_RETURN_BOOL(IsPointInMap(self, x, y, z));
}

template <typename T>
inline T VecDiff(FLevelLocals *Level, const T& v1, const T& v2)
{
	T result = v2 - v1;
	
	if (Level->subsectors.Size() > 0)
	{
		const sector_t * sec1 = Level->PointInSector(v1);
		const sector_t * sec2 = Level->PointInSector(v2);
		
		if (nullptr != sec1 && nullptr != sec2)
		{
			result += Level->Displacements.getOffset(sec2->PortalGroup, sec1->PortalGroup);
		}
	}
	
	return result;
}

void Vec2Diff(FLevelLocals *Level, double x1, double y1, double x2, double y2, DVector2 *result)
{
	*result = VecDiff(Level, DVector2(x1, y1), DVector2(x2, y2));
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, Vec2Diff, Vec2Diff)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x1);
	PARAM_FLOAT(y1);
	PARAM_FLOAT(x2);
	PARAM_FLOAT(y2);
	ACTION_RETURN_VEC2(VecDiff(self, DVector2(x1, y1), DVector2(x2, y2)));
}

void Vec3Diff(FLevelLocals *Level, double x1, double y1, double z1, double x2, double y2, double z2, DVector3 *result)
{
	*result = VecDiff(Level, DVector3(x1, y1, z1), DVector3(x2, y2, z2));
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, Vec3Diff, Vec3Diff)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x1);
	PARAM_FLOAT(y1);
	PARAM_FLOAT(z1);
	PARAM_FLOAT(x2);
	PARAM_FLOAT(y2);
	PARAM_FLOAT(z2);
	ACTION_RETURN_VEC3(VecDiff(self, DVector3(x1, y1, z1), DVector3(x2, y2, z2)));
}

void SphericalCoords(FLevelLocals *self, double vpX, double vpY, double vpZ, double tX, double tY, double tZ, double viewYaw, double viewPitch, int absolute, DVector3 *result)
{
	
	DVector3 viewpoint(vpX, vpY, vpZ);
	DVector3 target(tX, tY, tZ);
	auto vecTo = absolute ? target - viewpoint : VecDiff(self, viewpoint, target);
	
	*result = (DVector3(
								deltaangle(vecTo.Angle(), viewYaw).Degrees,
								deltaangle(vecTo.Pitch(), viewPitch).Degrees,
								vecTo.Length()
								));

}
DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, SphericalCoords, SphericalCoords)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(viewpointX);
	PARAM_FLOAT(viewpointY);
	PARAM_FLOAT(viewpointZ);
	PARAM_FLOAT(targetX);
	PARAM_FLOAT(targetY);
	PARAM_FLOAT(targetZ);
	PARAM_FLOAT(viewYaw);
	PARAM_FLOAT(viewPitch);
	PARAM_BOOL(absolute);
	DVector3 result;
	SphericalCoords(self, viewpointX, viewpointY, viewpointZ, targetX, targetY, targetZ, viewYaw, viewPitch, absolute, &result);
	ACTION_RETURN_VEC3(result);
}


static int isFrozen(FLevelLocals *self)
{
	return self->isFrozen();
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, isFrozen, isFrozen)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	ACTION_RETURN_INT(isFrozen(self));
}

void setFrozen(FLevelLocals *self, int on)
{
	self->frozenstate = (self->frozenstate & ~1) | !!on;
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, setFrozen, setFrozen)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_BOOL(on);
	setFrozen(self, on);
	return 0;
}

//=====================================================================================
//
//
//
//=====================================================================================

DEFINE_ACTION_FUNCTION_NATIVE(_AltHUD, GetLatency, Net_GetLatency)
{
	PARAM_PROLOGUE;
	int ld, ad;
	int severity = Net_GetLatency(&ld, &ad);
	if (numret > 0) ret[0].SetInt(severity);
	if (numret > 1) ret[1].SetInt(ld);
	if (numret > 2) ret[2].SetInt(ad);
	return numret;
}

DEFINE_ACTION_FUNCTION(_CVar, GetCVar)
{
	PARAM_PROLOGUE;
	PARAM_NAME(name);
	PARAM_POINTER(plyr, player_t);
	ACTION_RETURN_POINTER(GetCVar(plyr ? int(plyr - players) : -1, name.GetChars()));
}


DEFINE_ACTION_FUNCTION(DObject, S_ChangeMusic)
{
	PARAM_PROLOGUE;
	PARAM_STRING(music);
	PARAM_INT(order);
	PARAM_BOOL(looping);
	PARAM_BOOL(force);
	ACTION_RETURN_BOOL(S_ChangeMusic(music, order, looping, force));
}


DEFINE_ACTION_FUNCTION(_Screen, GetViewWindow)
{
	PARAM_PROLOGUE;
	if (numret > 0) ret[0].SetInt(viewwindowx);
	if (numret > 1) ret[1].SetInt(viewwindowy);
	if (numret > 2) ret[2].SetInt(viewwidth);
	if (numret > 3) ret[3].SetInt(viewheight);
	return MIN(numret, 4);
}

DEFINE_ACTION_FUNCTION(_Console, MidPrint)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(fnt, FFont);
	PARAM_STRING(text);
	PARAM_BOOL(bold);

	const char* txt = text[0] == '$' ? GStrings(&text[1]) : text.GetChars();
	C_MidPrint(fnt, txt, bold);
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

static int isValid( level_info_t *info )
{
	return info->isValid();
}

DEFINE_ACTION_FUNCTION_NATIVE(_LevelInfo, isValid, isValid)
{
	PARAM_SELF_STRUCT_PROLOGUE(level_info_t);
	ACTION_RETURN_BOOL(isValid(self));
}

static void LookupLevelName( level_info_t *info, FString *result )
{
	*result = info->LookupLevelName();
}

DEFINE_ACTION_FUNCTION_NATIVE(_LevelInfo, LookupLevelName, LookupLevelName)
{
	PARAM_SELF_STRUCT_PROLOGUE(level_info_t);
	FString rets;
	LookupLevelName(self,&rets);
	ACTION_RETURN_STRING(rets);
}

static int GetLevelInfoCount()
{
	return wadlevelinfos.Size();
}

DEFINE_ACTION_FUNCTION_NATIVE(_LevelInfo, GetLevelInfoCount, GetLevelInfoCount)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(GetLevelInfoCount());
}

static level_info_t* GetLevelInfo( unsigned int index )
{
	if ( index >= wadlevelinfos.Size() )
		return nullptr;
	return &wadlevelinfos[index];
}

DEFINE_ACTION_FUNCTION_NATIVE(_LevelInfo, GetLevelInfo, GetLevelInfo)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	ACTION_RETURN_POINTER(GetLevelInfo(index));
}

static level_info_t* ZFindLevelInfo( const FString &mapname )
{
	return FindLevelInfo(mapname.GetChars());
}

DEFINE_ACTION_FUNCTION_NATIVE(_LevelInfo, FindLevelInfo, ZFindLevelInfo)
{
	PARAM_PROLOGUE;
	PARAM_STRING(mapname);
	ACTION_RETURN_POINTER(ZFindLevelInfo(mapname));
}

DEFINE_ACTION_FUNCTION_NATIVE(_LevelInfo, FindLevelByNum, FindLevelByNum)
{
	PARAM_PROLOGUE;
	PARAM_INT(num);
	ACTION_RETURN_POINTER(FindLevelByNum(num));
}

static int MapExists( const FString &mapname )
{
	return P_CheckMapData(mapname.GetChars());
}

DEFINE_ACTION_FUNCTION_NATIVE(_LevelInfo, MapExists, MapExists)
{
	PARAM_PROLOGUE;
	PARAM_STRING(mapname);
	ACTION_RETURN_BOOL(MapExists(mapname));
}

DEFINE_ACTION_FUNCTION(_LevelInfo, MapChecksum)
{
	PARAM_PROLOGUE;
	PARAM_STRING(mapname);
	char md5string[33] = "";
	MapData *map = P_OpenMapData(mapname.GetChars(), true);
	if (map != nullptr)
	{
		uint8_t cksum[16];
		map->GetChecksum(cksum);
		for (int j = 0; j < 16; ++j)
		{
			sprintf(md5string + j * 2, "%02x", cksum[j]);
		}
		delete map;
	}
	ACTION_RETURN_STRING((const char *)md5string);
}

//==========================================================================
//
//
//
//==========================================================================
DEFINE_FIELD_X(LevelInfo, level_info_t, levelnum)
DEFINE_FIELD_X(LevelInfo, level_info_t, MapName)
DEFINE_FIELD_X(LevelInfo, level_info_t, NextMap)
DEFINE_FIELD_X(LevelInfo, level_info_t, NextSecretMap)
DEFINE_FIELD_X(LevelInfo, level_info_t, SkyPic1)
DEFINE_FIELD_X(LevelInfo, level_info_t, SkyPic2)
DEFINE_FIELD_X(LevelInfo, level_info_t, F1Pic)
DEFINE_FIELD_X(LevelInfo, level_info_t, cluster)
DEFINE_FIELD_X(LevelInfo, level_info_t, partime)
DEFINE_FIELD_X(LevelInfo, level_info_t, sucktime)
DEFINE_FIELD_X(LevelInfo, level_info_t, flags)
DEFINE_FIELD_X(LevelInfo, level_info_t, flags2)
DEFINE_FIELD_X(LevelInfo, level_info_t, flags3)
DEFINE_FIELD_X(LevelInfo, level_info_t, Music)
DEFINE_FIELD_X(LevelInfo, level_info_t, LevelName)
DEFINE_FIELD_X(LevelInfo, level_info_t, AuthorName)
DEFINE_FIELD_X(LevelInfo, level_info_t, musicorder)
DEFINE_FIELD_X(LevelInfo, level_info_t, skyspeed1)
DEFINE_FIELD_X(LevelInfo, level_info_t, skyspeed2)
DEFINE_FIELD_X(LevelInfo, level_info_t, cdtrack)
DEFINE_FIELD_X(LevelInfo, level_info_t, gravity)
DEFINE_FIELD_X(LevelInfo, level_info_t, aircontrol)
DEFINE_FIELD_X(LevelInfo, level_info_t, airsupply)
DEFINE_FIELD_X(LevelInfo, level_info_t, compatflags)
DEFINE_FIELD_X(LevelInfo, level_info_t, compatflags2)
DEFINE_FIELD_X(LevelInfo, level_info_t, deathsequence)
DEFINE_FIELD_X(LevelInfo, level_info_t, fogdensity)
DEFINE_FIELD_X(LevelInfo, level_info_t, outsidefogdensity)
DEFINE_FIELD_X(LevelInfo, level_info_t, skyfog)
DEFINE_FIELD_X(LevelInfo, level_info_t, pixelstretch)
DEFINE_FIELD_X(LevelInfo, level_info_t, RedirectType)
DEFINE_FIELD_X(LevelInfo, level_info_t, RedirectMapName)
DEFINE_FIELD_X(LevelInfo, level_info_t, teamdamage)

DEFINE_GLOBAL_NAMED(currentVMLevel, level)
DEFINE_FIELD(FLevelLocals, sectors)
DEFINE_FIELD(FLevelLocals, lines)
DEFINE_FIELD(FLevelLocals, sides)
DEFINE_FIELD(FLevelLocals, vertexes)
DEFINE_FIELD(FLevelLocals, sectorPortals)
DEFINE_FIELD(FLevelLocals, time)
DEFINE_FIELD(FLevelLocals, maptime)
DEFINE_FIELD(FLevelLocals, totaltime)
DEFINE_FIELD(FLevelLocals, starttime)
DEFINE_FIELD(FLevelLocals, partime)
DEFINE_FIELD(FLevelLocals, sucktime)
DEFINE_FIELD(FLevelLocals, cluster)
DEFINE_FIELD(FLevelLocals, clusterflags)
DEFINE_FIELD(FLevelLocals, levelnum)
DEFINE_FIELD(FLevelLocals, LevelName)
DEFINE_FIELD(FLevelLocals, MapName)
DEFINE_FIELD(FLevelLocals, NextMap)
DEFINE_FIELD(FLevelLocals, NextSecretMap)
DEFINE_FIELD(FLevelLocals, F1Pic)
DEFINE_FIELD(FLevelLocals, AuthorName)
DEFINE_FIELD(FLevelLocals, maptype)
DEFINE_FIELD(FLevelLocals, Music)
DEFINE_FIELD(FLevelLocals, musicorder)
DEFINE_FIELD(FLevelLocals, skytexture1)
DEFINE_FIELD(FLevelLocals, skytexture2)
DEFINE_FIELD(FLevelLocals, skyspeed1)
DEFINE_FIELD(FLevelLocals, skyspeed2)
DEFINE_FIELD(FLevelLocals, total_secrets)
DEFINE_FIELD(FLevelLocals, found_secrets)
DEFINE_FIELD(FLevelLocals, total_items)
DEFINE_FIELD(FLevelLocals, found_items)
DEFINE_FIELD(FLevelLocals, total_monsters)
DEFINE_FIELD(FLevelLocals, killed_monsters)
DEFINE_FIELD(FLevelLocals, gravity)
DEFINE_FIELD(FLevelLocals, aircontrol)
DEFINE_FIELD(FLevelLocals, airfriction)
DEFINE_FIELD(FLevelLocals, airsupply)
DEFINE_FIELD(FLevelLocals, teamdamage)
DEFINE_FIELD(FLevelLocals, fogdensity)
DEFINE_FIELD(FLevelLocals, outsidefogdensity)
DEFINE_FIELD(FLevelLocals, skyfog)
DEFINE_FIELD(FLevelLocals, pixelstretch)
DEFINE_FIELD(FLevelLocals, MusicVolume)
DEFINE_FIELD(FLevelLocals, deathsequence)
DEFINE_FIELD_BIT(FLevelLocals, frozenstate, frozen, 1)	// still needed for backwards compatibility.
DEFINE_FIELD_NAMED(FLevelLocals, i_compatflags, compatflags)
DEFINE_FIELD_NAMED(FLevelLocals, i_compatflags2, compatflags2)
DEFINE_FIELD(FLevelLocals, info);

DEFINE_FIELD_BIT(FLevelLocals, flags, noinventorybar, LEVEL_NOINVENTORYBAR)
DEFINE_FIELD_BIT(FLevelLocals, flags, monsterstelefrag, LEVEL_MONSTERSTELEFRAG)
DEFINE_FIELD_BIT(FLevelLocals, flags, actownspecial, LEVEL_ACTOWNSPECIAL)
DEFINE_FIELD_BIT(FLevelLocals, flags, sndseqtotalctrl, LEVEL_SNDSEQTOTALCTRL)
DEFINE_FIELD_BIT(FLevelLocals, flags2, allmap, LEVEL2_ALLMAP)
DEFINE_FIELD_BIT(FLevelLocals, flags2, missilesactivateimpact, LEVEL2_MISSILESACTIVATEIMPACT)
DEFINE_FIELD_BIT(FLevelLocals, flags2, monsterfallingdamage, LEVEL2_MONSTERFALLINGDAMAGE)
DEFINE_FIELD_BIT(FLevelLocals, flags2, checkswitchrange, LEVEL2_CHECKSWITCHRANGE)
DEFINE_FIELD_BIT(FLevelLocals, flags2, polygrind, LEVEL2_POLYGRIND)
DEFINE_FIELD_BIT(FLevelLocals, flags2, allowrespawn, LEVEL2_ALLOWRESPAWN)
DEFINE_FIELD_BIT(FLevelLocals, flags2, nomonsters, LEVEL2_NOMONSTERS)
DEFINE_FIELD_BIT(FLevelLocals, flags2, infinite_flight, LEVEL2_INFINITE_FLIGHT)
DEFINE_FIELD_BIT(FLevelLocals, flags2, no_dlg_freeze, LEVEL2_CONV_SINGLE_UNFREEZE)
DEFINE_FIELD_BIT(FLevelLocals, flags2, keepfullinventory, LEVEL2_KEEPFULLINVENTORY)
DEFINE_FIELD_BIT(FLevelLocals, flags3, removeitems, LEVEL3_REMOVEITEMS)

DEFINE_FIELD_X(Sector, sector_t, floorplane)
DEFINE_FIELD_X(Sector, sector_t, ceilingplane)
DEFINE_FIELD_X(Sector, sector_t, Colormap)
DEFINE_FIELD_X(Sector, sector_t, SpecialColors)
DEFINE_FIELD_X(Sector, sector_t, AdditiveColors)
DEFINE_FIELD_X(Sector, sector_t, SoundTarget)
DEFINE_FIELD_X(Sector, sector_t, special)
DEFINE_FIELD_X(Sector, sector_t, lightlevel)
DEFINE_FIELD_X(Sector, sector_t, seqType)
DEFINE_FIELD_X(Sector, sector_t, sky)
DEFINE_FIELD_X(Sector, sector_t, SeqName)
DEFINE_FIELD_X(Sector, sector_t, centerspot)
DEFINE_FIELD_X(Sector, sector_t, validcount)
DEFINE_FIELD_X(Sector, sector_t, thinglist)
DEFINE_FIELD_X(Sector, sector_t, friction)
DEFINE_FIELD_X(Sector, sector_t, movefactor)
DEFINE_FIELD_X(Sector, sector_t, terrainnum)
DEFINE_FIELD_X(Sector, sector_t, floordata)
DEFINE_FIELD_X(Sector, sector_t, ceilingdata)
DEFINE_FIELD_X(Sector, sector_t, lightingdata)
DEFINE_FIELD_X(Sector, sector_t, Level)
DEFINE_FIELD_X(Sector, sector_t, interpolations)
DEFINE_FIELD_X(Sector, sector_t, soundtraversed)
DEFINE_FIELD_X(Sector, sector_t, stairlock)
DEFINE_FIELD_X(Sector, sector_t, prevsec)
DEFINE_FIELD_X(Sector, sector_t, nextsec)
DEFINE_FIELD_UNSIZED(Sector, sector_t, Lines)
DEFINE_FIELD_X(Sector, sector_t, heightsec)
DEFINE_FIELD_X(Sector, sector_t, bottommap)
DEFINE_FIELD_X(Sector, sector_t, midmap)
DEFINE_FIELD_X(Sector, sector_t, topmap)
DEFINE_FIELD_X(Sector, sector_t, touching_thinglist)
DEFINE_FIELD_X(Sector, sector_t, sectorportal_thinglist)
DEFINE_FIELD_X(Sector, sector_t, gravity)
DEFINE_FIELD_X(Sector, sector_t, damagetype)
DEFINE_FIELD_X(Sector, sector_t, damageamount)
DEFINE_FIELD_X(Sector, sector_t, damageinterval)
DEFINE_FIELD_X(Sector, sector_t, leakydamage)
DEFINE_FIELD_X(Sector, sector_t, ZoneNumber)
DEFINE_FIELD_X(Sector, sector_t, healthceiling)
DEFINE_FIELD_X(Sector, sector_t, healthfloor)
DEFINE_FIELD_X(Sector, sector_t, healthceilinggroup)
DEFINE_FIELD_X(Sector, sector_t, healthfloorgroup)
DEFINE_FIELD_X(Sector, sector_t, MoreFlags)
DEFINE_FIELD_X(Sector, sector_t, Flags)
DEFINE_FIELD_X(Sector, sector_t, SecActTarget)
DEFINE_FIELD_X(Sector, sector_t, Portals)
DEFINE_FIELD_X(Sector, sector_t, PortalGroup)
DEFINE_FIELD_X(Sector, sector_t, sectornum)

DEFINE_FIELD_X(Line, line_t, v1)
DEFINE_FIELD_X(Line, line_t, v2)
DEFINE_FIELD_X(Line, line_t, delta)
DEFINE_FIELD_X(Line, line_t, flags)
DEFINE_FIELD_X(Line, line_t, flags2)
DEFINE_FIELD_X(Line, line_t, activation)
DEFINE_FIELD_X(Line, line_t, special)
DEFINE_FIELD_X(Line, line_t, args)
DEFINE_FIELD_X(Line, line_t, alpha)
DEFINE_FIELD_X(Line, line_t, sidedef)
DEFINE_FIELD_X(Line, line_t, bbox)
DEFINE_FIELD_X(Line, line_t, frontsector)
DEFINE_FIELD_X(Line, line_t, backsector)
DEFINE_FIELD_X(Line, line_t, validcount)
DEFINE_FIELD_X(Line, line_t, locknumber)
DEFINE_FIELD_X(Line, line_t, portalindex)
DEFINE_FIELD_X(Line, line_t, portaltransferred)
DEFINE_FIELD_X(Line, line_t, health)
DEFINE_FIELD_X(Line, line_t, healthgroup)

DEFINE_FIELD_X(Side, side_t, sector)
DEFINE_FIELD_X(Side, side_t, linedef)
DEFINE_FIELD_X(Side, side_t, Light)
DEFINE_FIELD_X(Side, side_t, Flags)

DEFINE_FIELD_X(Secplane, secplane_t, normal)
DEFINE_FIELD_X(Secplane, secplane_t, D)
DEFINE_FIELD_X(Secplane, secplane_t, negiC)

DEFINE_FIELD_NAMED_X(F3DFloor, F3DFloor, bottom.plane, bottom);
DEFINE_FIELD_NAMED_X(F3DFloor, F3DFloor, top.plane, top);
DEFINE_FIELD_X(F3DFloor, F3DFloor, flags);
DEFINE_FIELD_X(F3DFloor, F3DFloor, master);
DEFINE_FIELD_X(F3DFloor, F3DFloor, model);
DEFINE_FIELD_X(F3DFloor, F3DFloor, target);
DEFINE_FIELD_X(F3DFloor, F3DFloor, alpha);

DEFINE_FIELD_X(Vertex, vertex_t, p)

DEFINE_FIELD(DBaseStatusBar, Centering);
DEFINE_FIELD(DBaseStatusBar, FixedOrigin);
DEFINE_FIELD(DBaseStatusBar, CrosshairSize);
DEFINE_FIELD(DBaseStatusBar, Displacement);
DEFINE_FIELD(DBaseStatusBar, CPlayer);
DEFINE_FIELD(DBaseStatusBar, ShowLog);
DEFINE_FIELD(DBaseStatusBar, artiflashTick);
DEFINE_FIELD(DBaseStatusBar, itemflashFade);


DEFINE_GLOBAL(StatusBar);

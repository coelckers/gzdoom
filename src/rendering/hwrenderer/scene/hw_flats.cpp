// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_flat.cpp
** Flat processing
**
*/

#include "a_sharedglobal.h"
#include "a_dynlight.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_utility.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "p_lnspec.h"
#include "matrix.h"
#include "hw_dynlightdata.h"
#include "hw_cvars.h"
#include "hw_clock.h"
#include "hw_lighting.h"
#include "hw_material.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "flatvertices.h"
#include "hw_lightbuffer.h"
#include "hw_drawstructs.h"
#include "hw_renderstate.h"
#include "texturemanager.h"

#ifdef _DEBUG
CVAR(Int, gl_breaksec, -1, 0)
#endif
//==========================================================================
//
// Sets the texture matrix according to the plane's texture positioning
// information
//
//==========================================================================

bool hw_SetPlaneTextureRotation(const HWSectorPlane * secplane, FGameTexture * gltexture, VSMatrix &dest)
{
	// only manipulate the texture matrix if needed.
	if (!secplane->Offs.isZero() ||
		secplane->Scale.X != 1. || secplane->Scale.Y != 1 ||
		secplane->Angle != 0 ||
		gltexture->GetDisplayWidth() != 64 ||
		gltexture->GetDisplayHeight() != 64)
	{
		float uoffs = secplane->Offs.X / gltexture->GetDisplayWidth();
		float voffs = secplane->Offs.Y / gltexture->GetDisplayHeight();

		float xscale1 = secplane->Scale.X;
		float yscale1 = secplane->Scale.Y;
		if (gltexture->isHardwareCanvas())
		{
			yscale1 = 0 - yscale1;
		}
		float angle = -secplane->Angle;

		float xscale2 = 64.f / gltexture->GetDisplayWidth();
		float yscale2 = 64.f / gltexture->GetDisplayHeight();

		dest.loadIdentity();
		dest.scale(xscale1, yscale1, 1.0f);
		dest.translate(uoffs, voffs, 0.0f);
		dest.scale(xscale2, yscale2, 1.0f);
		dest.rotate(angle, 0.0f, 0.0f, 1.0f);
		return true;
	}
	return false;
}

void SetPlaneTextureRotation(FRenderState &state, HWSectorPlane* plane, FGameTexture* texture)
{
	if (hw_SetPlaneTextureRotation(plane, texture, state.mTextureMatrix))
	{
		state.EnableTextureMatrix(true);
	}
}



//==========================================================================
//
// special handling for skyboxes which need texture clamping.
// This will find the bounding rectangle of the sector and just
// draw one single polygon filling that rectangle with a clamped
// texture.
//
//==========================================================================

void HWFlat::CreateSkyboxVertices(FFlatVertex *vert)
{
	float minx = FLT_MAX, miny = FLT_MAX;
	float maxx = -FLT_MAX, maxy = -FLT_MAX;

	for (auto ln : sector->Lines)
	{
		float x = ln->v1->fX();
		float y = ln->v1->fY();
		if (x < minx) minx = x;
		if (y < miny) miny = y;
		if (x > maxx) maxx = x;
		if (y > maxy) maxy = y;
		x = ln->v2->fX();
		y = ln->v2->fY();
		if (x < minx) minx = x;
		if (y < miny) miny = y;
		if (x > maxx) maxx = x;
		if (y > maxy) maxy = y;
	}

	static float uvals[] = { 0, 0, 1, 1 };
	static float vvals[] = { 1, 0, 0, 1 };
	int rot = -xs_FloorToInt(plane.Angle / 90.f);

	vert[0].Set(minx, z, miny, uvals[rot & 3], vvals[rot & 3]);
	vert[1].Set(minx, z, maxy, uvals[(rot + 1) & 3], vvals[(rot + 1) & 3]);
	vert[2].Set(maxx, z, miny, uvals[(rot + 3) & 3], vvals[(rot + 3) & 3]);
	vert[3].Set(maxx, z, maxy, uvals[(rot + 2) & 3], vvals[(rot + 2) & 3]);
}

//==========================================================================
//
//
//
//==========================================================================

void HWFlat::SetupLights(HWDrawInfo *di, FLightNode * node, FDynLightData &lightdata, int portalgroup)
{
	Plane p;

	lightdata.Clear();
	if (renderstyle == STYLE_Add && !di->Level->lightadditivesurfaces)
	{
		dynlightindex = -1;
		return;	// no lights on additively blended surfaces.
	}
	while (node)
	{
		FDynamicLight * light = node->lightsource;

		if (!light->IsActive() || light->DontLightMap())
		{
			node = node->nextLight;
			continue;
		}
		iter_dlightf++;

		// we must do the side check here because gl_GetLight needs the correct plane orientation
		// which we don't have for Legacy-style 3D-floors
		double planeh = plane.plane.ZatPoint(light->Pos);
		if ((planeh<light->Z() && ceiling) || (planeh>light->Z() && !ceiling))
		{
			node = node->nextLight;
			continue;
		}

		p.Set(plane.plane.Normal(), plane.plane.fD());
		draw_dlightf += GetLight(lightdata, portalgroup, p, light, false);
		node = node->nextLight;
	}

	dynlightindex = screen->mLights->UploadLights(lightdata);
}

//==========================================================================
//
//
//
//==========================================================================

void HWFlat::DrawSubsectors(HWDrawInfo *di, FRenderState &state)
{
	if (di->Level->HasDynamicLights && screen->BuffersArePersistent() && !di->isFullbrightScene())
	{
		SetupLights(di, section->lighthead, lightdata, sector->PortalGroup);
	}
	state.SetLightIndex(dynlightindex);


	state.DrawIndexed(DT_Triangles, iboindex + section->vertexindex, section->vertexcount);
	flatvertices += section->vertexcount;
	flatprimitives++;
}


//==========================================================================
//
// Drawer for render hacks
//
//==========================================================================

void HWFlat::DrawOtherPlanes(HWDrawInfo *di, FRenderState &state)
{
    state.SetMaterial(texture, UF_Texture, 0, CLAMP_NONE, 0, -1);
    
    // Draw the subsectors assigned to it due to missing textures
    auto pNode = (renderflags&SSRF_RENDERFLOOR) ?
        di->otherFloorPlanes.CheckKey(sector->sectornum) : di->otherCeilingPlanes.CheckKey(sector->sectornum);
    
    if (!pNode) return;
    auto node = *pNode;
    
    while (node)
    {
        state.SetLightIndex(node->lightindex);
        auto num = node->sub->numlines;
        flatvertices += num;
        flatprimitives++;
        state.Draw(DT_TriangleFan,node->vertexindex, num);
        node = node->next;
    }
}

//==========================================================================
//
// Drawer for render hacks
//
//==========================================================================

void HWFlat::DrawFloodPlanes(HWDrawInfo *di, FRenderState &state)
{
	// Flood gaps with the back side's ceiling/floor texture
	// This requires a stencil because the projected plane interferes with
	// the depth buffer

	state.SetMaterial(texture, UF_Texture, 0, CLAMP_NONE, 0, -1);

	// Draw the subsectors assigned to it due to missing textures
	auto pNode = (renderflags&SSRF_RENDERFLOOR) ?
		di->floodFloorSegs.CheckKey(sector->sectornum) : di->floodCeilingSegs.CheckKey(sector->sectornum);
	if (!pNode) return;

	auto fnode = *pNode;

	state.SetLightIndex(-1);
	while (fnode)
	{
		flatvertices += 12;
		flatprimitives += 3;

		// Push bleeding floor/ceiling textures back a little in the z-buffer
		// so they don't interfere with overlapping mid textures.
		state.SetDepthBias(1, 128);

		// Create stencil
		state.SetEffect(EFF_STENCIL);
		state.EnableTexture(false);
		state.SetStencil(0, SOP_Increment, SF_ColorMaskOff);
		state.Draw(DT_TriangleStrip, fnode->vertexindex, 4);

		// Draw projected plane into stencil
		state.EnableTexture(true);
		state.SetEffect(EFF_NONE);
		state.SetStencil(1, SOP_Keep, SF_DepthMaskOff);
		state.EnableDepthTest(false);
		state.Draw(DT_TriangleStrip, fnode->vertexindex + 4, 4);

		// clear stencil
		state.SetEffect(EFF_STENCIL);
		state.EnableTexture(false);
		state.SetStencil(1, SOP_Decrement, SF_ColorMaskOff | SF_DepthMaskOff);
		state.Draw(DT_TriangleStrip, fnode->vertexindex, 4);

		// restore old stencil op.
		state.EnableTexture(true);
		state.EnableDepthTest(true);
		state.SetEffect(EFF_NONE);
		state.SetDepthBias(0, 0);
		state.SetStencil(0, SOP_Keep, SF_AllOn);

		fnode = fnode->next;
	}

}


//==========================================================================
//
//
//
//==========================================================================
void HWFlat::DrawFlat(HWDrawInfo *di, FRenderState &state, bool translucent)
{
#ifdef _DEBUG
	if (sector->sectornum == gl_breaksec)
	{
		int a = 0;
	}
#endif

	int rel = getExtraLight();

	state.SetNormal(plane.plane.Normal().X, plane.plane.Normal().Z, plane.plane.Normal().Y);

	di->SetColor(state, lightlevel, rel, di->isFullbrightScene(), Colormap, alpha);
	di->SetFog(state, lightlevel, rel, di->isFullbrightScene(), &Colormap, false);
	state.SetObjectColor(FlatColor | 0xff000000);
	state.SetAddColor(AddColor | 0xff000000);
	state.ApplyTextureManipulation(TextureFx);


	if (hacktype & SSRF_PLANEHACK)
	{
		DrawOtherPlanes(di, state);
	}
	else if (hacktype & SSRF_FLOODHACK)
	{
		DrawFloodPlanes(di, state);
	}
	else if (!translucent)
	{
		if (sector->special != GLSector_Skybox)
		{
			state.SetMaterial(texture, UF_Texture, 0, CLAMP_NONE, 0, -1);
			SetPlaneTextureRotation(state, &plane, texture);
			DrawSubsectors(di, state);
			state.EnableTextureMatrix(false);
		}
		else if (!hacktype)
		{
			state.SetMaterial(texture, UF_Texture, 0, CLAMP_XY, 0, -1);
			state.SetLightIndex(dynlightindex);
			state.Draw(DT_TriangleStrip,iboindex, 4);
			flatvertices += 4;
			flatprimitives++;
		}
	}
	else
	{
		state.SetRenderStyle(renderstyle);
		if (!texture || !texture->isValid())
		{
			state.AlphaFunc(Alpha_GEqual, 0.f);
			state.EnableTexture(false);
			DrawSubsectors(di, state);
			state.EnableTexture(true);
		}
		else
		{
			if (!texture->GetTranslucency()) state.AlphaFunc(Alpha_GEqual, gl_mask_threshold);
			else state.AlphaFunc(Alpha_GEqual, 0.f);
			state.SetMaterial(texture, UF_Texture, 0, CLAMP_NONE, 0, -1);
			SetPlaneTextureRotation(state, &plane, texture);
			DrawSubsectors(di, state);
			state.EnableTextureMatrix(false);
		}
		state.SetRenderStyle(DefaultRenderStyle());
	}
	state.SetObjectColor(0xffffffff);
	state.SetAddColor(0);
	state.ApplyTextureManipulation(nullptr);
}

//==========================================================================
//
// HWFlat::PutFlat
//
// submit to the renderer
//
//==========================================================================

inline void HWFlat::PutFlat(HWDrawInfo *di, bool fog)
{
	if (di->isFullbrightScene())
	{
		Colormap.Clear();
	}
	else if (!screen->BuffersArePersistent())
	{
		if (di->Level->HasDynamicLights && texture != nullptr && !di->isFullbrightScene() && !(hacktype & (SSRF_PLANEHACK|SSRF_FLOODHACK)) )
		{
			SetupLights(di, section->lighthead, lightdata, sector->PortalGroup);
		}
	}
	di->AddFlat(this, fog);
}

//==========================================================================
//
// This draws one flat 
// The whichplane boolean indicates if the flat is a floor(false) or a ceiling(true)
//
//==========================================================================

void HWFlat::Process(HWDrawInfo *di, sector_t * model, int whichplane, bool fog)
{
	plane.GetFromSector(model, whichplane);
	if (whichplane != int(ceiling))
	{
		// Flip the normal if the source plane has a different orientation than what we are about to render.
		plane.plane.FlipVert();
	}

	if (!fog)
	{
		texture =  TexMan.GetGameTexture(plane.texture, true);
		if (!texture || !texture->isValid()) return;
		if (texture->isFullbright()) 
		{
			Colormap.MakeWhite();
			lightlevel=255;
		}
	}
	else 
	{
		texture = NULL;
		lightlevel = abs(lightlevel);
	}

	z = plane.plane.ZatPoint(0.f, 0.f);
	if (sector->special == GLSector_Skybox)
	{
		auto vert = screen->mVertexData->AllocVertices(4);
		CreateSkyboxVertices(vert.first);
		iboindex = vert.second;
	}

	// For hacks this won't go into a render list.
	PutFlat(di, fog);
	rendered_flats++;
}

//==========================================================================
//
// Sets 3D floor info. Common code for all 4 cases 
//
//==========================================================================

void HWFlat::SetFrom3DFloor(F3DFloor *rover, bool top, bool underside)
{
	F3DFloor::planeref & plane = top? rover->top : rover->bottom;

	// FF_FOG requires an inverted logic where to get the light from
	lightlist_t *light = P_GetPlaneLight(sector, plane.plane, underside);
	lightlevel = hw_ClampLight(*light->p_lightlevel);

	if (rover->flags & FF_FOG)
	{
		Colormap.LightColor = light->extra_colormap.FadeColor;
		FlatColor = 0xffffffff;
		AddColor = 0;
		TextureFx = nullptr;
	}
	else
	{
		CopyFrom3DLight(Colormap, light);
		FlatColor = plane.model->SpecialColors[plane.isceiling];
		AddColor = plane.model->AdditiveColors[plane.isceiling];
		TextureFx = &plane.model->planes[plane.isceiling].TextureFx;
	}


	alpha = rover->alpha/255.0f;
	renderstyle = rover->flags&FF_ADDITIVETRANS? STYLE_Add : STYLE_Translucent;
	iboindex = plane.vindex;
}

//==========================================================================
//
// Process a sector's flats for rendering
// This function is only called once per sector.
// Subsequent subsectors are just quickly added to the ss_renderflags array
//
//==========================================================================

void HWFlat::ProcessSector(HWDrawInfo *di, sector_t * frontsector, int which)
{
	lightlist_t * light;
	FSectorPortal *port;

#ifdef _DEBUG
	if (frontsector->sectornum == gl_breaksec)
	{
		int a = 0;
	}
#endif

	// Get the real sector for this one.
	sector = &di->Level->sectors[frontsector->sectornum];
	extsector_t::xfloor &x = sector->e->XFloor;
	dynlightindex = -1;
    hacktype = (which & (SSRF_PLANEHACK|SSRF_FLOODHACK));

	uint8_t sink;
	uint8_t &srf = hacktype? sink : di->section_renderflags[di->Level->sections.SectionIndex(section)];
    const auto &vp = di->Viewpoint;

	//
	//
	//
	// do floors
	//
	//
	//
	if ((which & SSRF_RENDERFLOOR) && frontsector->floorplane.ZatPoint(vp.Pos) <= vp.Pos.Z && (!section || !(section->flags & FSection::DONTRENDERFLOOR)))
	{
		// process the original floor first.

		srf |= SSRF_RENDERFLOOR;

		lightlevel = hw_ClampLight(frontsector->GetFloorLight());
		Colormap = frontsector->Colormap;
		FlatColor = frontsector->SpecialColors[sector_t::floor];
		AddColor = frontsector->AdditiveColors[sector_t::floor];
		TextureFx = &frontsector->planes[sector_t::floor].TextureFx;

		port = frontsector->ValidatePortal(sector_t::floor);
		if ((stack = (port != NULL)))
		{
            /* to be redone in a less invasive manner
			if (port->mType == PORTS_STACKEDSECTORTHING)
			{
				di->AddFloorStack(sector);	// stacked sector things require visplane merging.
			}
             */
			alpha = frontsector->GetAlpha(sector_t::floor);
		}
		else
		{
			alpha = 1.0f - frontsector->GetReflect(sector_t::floor);
		}

		if (alpha != 0.f && frontsector->GetTexture(sector_t::floor) != skyflatnum)
		{
			iboindex = frontsector->iboindex[sector_t::floor];

			ceiling = false;
			renderflags = SSRF_RENDERFLOOR;

			if (x.ffloors.Size())
			{
				light = P_GetPlaneLight(sector, &frontsector->floorplane, false);
				if ((!(sector->GetFlags(sector_t::floor)&PLANEF_ABSLIGHTING) || light->lightsource == NULL)
					&& (light->p_lightlevel != &frontsector->lightlevel))
				{
					lightlevel = hw_ClampLight(*light->p_lightlevel);
				}

				CopyFrom3DLight(Colormap, light);
			}
			renderstyle = STYLE_Translucent;
			Process(di, frontsector, sector_t::floor, false);
		}
	}

	//
	//
	//
	// do ceilings
	//
	// 
	//
	if ((which & SSRF_RENDERCEILING) && frontsector->ceilingplane.ZatPoint(vp.Pos) >= vp.Pos.Z && (!section || !(section->flags & FSection::DONTRENDERCEILING)))
	{
		// process the original ceiling first.

		srf |= SSRF_RENDERCEILING;

		lightlevel = hw_ClampLight(frontsector->GetCeilingLight());
		Colormap = frontsector->Colormap;
		FlatColor = frontsector->SpecialColors[sector_t::ceiling];
		AddColor = frontsector->AdditiveColors[sector_t::ceiling];
		TextureFx = &frontsector->planes[sector_t::ceiling].TextureFx;
		port = frontsector->ValidatePortal(sector_t::ceiling);
		if ((stack = (port != NULL)))
		{
            /* as above for floors
			if (port->mType == PORTS_STACKEDSECTORTHING)
			{
				di->AddCeilingStack(sector);
			}
             */
			alpha = frontsector->GetAlpha(sector_t::ceiling);
		}
		else
		{
			alpha = 1.0f - frontsector->GetReflect(sector_t::ceiling);
		}

		if (alpha != 0.f && frontsector->GetTexture(sector_t::ceiling) != skyflatnum)
		{
			iboindex = frontsector->iboindex[sector_t::ceiling];
			ceiling = true;
			renderflags = SSRF_RENDERCEILING;

			if (x.ffloors.Size())
			{
				light = P_GetPlaneLight(sector, &sector->ceilingplane, true);

				if ((!(sector->GetFlags(sector_t::ceiling)&PLANEF_ABSLIGHTING))
					&& (light->p_lightlevel != &frontsector->lightlevel))
				{
					lightlevel = hw_ClampLight(*light->p_lightlevel);
				}
				CopyFrom3DLight(Colormap, light);
			}
			renderstyle = STYLE_Translucent;
			Process(di, frontsector, sector_t::ceiling, false);
		}
	}

	//
	//
	//
	// do 3D floors
	//
	//
	//

	stack = false;
	if ((which & SSRF_RENDER3DPLANES) && x.ffloors.Size())
	{
		renderflags = SSRF_RENDER3DPLANES;
		srf |= SSRF_RENDER3DPLANES;
		// 3d-floors must not overlap!
		double lastceilingheight = sector->CenterCeiling();	// render only in the range of the
		double lastfloorheight = sector->CenterFloor();		// current sector part (if applicable)
		F3DFloor * rover;
		int k;

		// floors are ordered now top to bottom so scanning the list for the best match
		// is no longer necessary.

		ceiling = true;
		Colormap = frontsector->Colormap;
		for (k = 0; k < (int)x.ffloors.Size(); k++)
		{
			rover = x.ffloors[k];

			if ((rover->flags&(FF_EXISTS | FF_RENDERPLANES | FF_THISINSIDE)) == (FF_EXISTS | FF_RENDERPLANES))
			{
				if (rover->flags&FF_FOG && di->isFullbrightScene()) continue;
				if (!rover->top.copied && rover->flags&(FF_INVERTPLANES | FF_BOTHPLANES))
				{
					double ff_top = rover->top.plane->ZatPoint(sector->centerspot);
					if (ff_top < lastceilingheight)
					{
						if (vp.Pos.Z <= rover->top.plane->ZatPoint(vp.Pos))
						{
							SetFrom3DFloor(rover, true, !!(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;
							Process(di, rover->top.model, rover->top.isceiling, !!(rover->flags&FF_FOG));
						}
						lastceilingheight = ff_top;
					}
				}
				if (!rover->bottom.copied && !(rover->flags&FF_INVERTPLANES))
				{
					double ff_bottom = rover->bottom.plane->ZatPoint(sector->centerspot);
					if (ff_bottom < lastceilingheight)
					{
						if (vp.Pos.Z <= rover->bottom.plane->ZatPoint(vp.Pos))
						{
							SetFrom3DFloor(rover, false, !(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;
							Process(di, rover->bottom.model, rover->bottom.isceiling, !!(rover->flags&FF_FOG));
						}
						lastceilingheight = ff_bottom;
						if (rover->alpha < 255) lastceilingheight += EQUAL_EPSILON;
					}
				}
			}
		}

		ceiling = false;
		for (k = x.ffloors.Size() - 1; k >= 0; k--)
		{
			rover = x.ffloors[k];

			if ((rover->flags&(FF_EXISTS | FF_RENDERPLANES | FF_THISINSIDE)) == (FF_EXISTS | FF_RENDERPLANES))
			{
				if (rover->flags&FF_FOG && di->isFullbrightScene()) continue;
				if (!rover->bottom.copied && rover->flags&(FF_INVERTPLANES | FF_BOTHPLANES))
				{
					double ff_bottom = rover->bottom.plane->ZatPoint(sector->centerspot);
					if (ff_bottom > lastfloorheight || (rover->flags&FF_FIX))
					{
						if (vp.Pos.Z >= rover->bottom.plane->ZatPoint(vp.Pos))
						{
							SetFrom3DFloor(rover, false, !(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;

							if (rover->flags&FF_FIX)
							{
								lightlevel = hw_ClampLight(rover->model->lightlevel);
								Colormap = rover->GetColormap();
							}

							Process(di, rover->bottom.model, rover->bottom.isceiling, !!(rover->flags&FF_FOG));
						}
						lastfloorheight = ff_bottom;
					}
				}
				if (!rover->top.copied && !(rover->flags&FF_INVERTPLANES))
				{
					double ff_top = rover->top.plane->ZatPoint(sector->centerspot);
					if (ff_top > lastfloorheight)
					{
						if (vp.Pos.Z >= rover->top.plane->ZatPoint(vp.Pos))
						{
							SetFrom3DFloor(rover, true, !!(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;
							Process(di, rover->top.model, rover->top.isceiling, !!(rover->flags&FF_FOG));
						}
						lastfloorheight = ff_top;
						if (rover->alpha < 255) lastfloorheight -= EQUAL_EPSILON;
					}
				}
			}
		}
	}
}


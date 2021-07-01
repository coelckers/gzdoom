//-----------------------------------------------------------------------------
//
// Copyright(c) 1993 - 1997 Ken Silverman
// Copyright(c) 1998 - 2016 Randy Heit
// All rights reserved.
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
//---------------------------------------------------------------------------
//
// Voxel rendering
//
// Draw a voxel slab. This function is taken from the Build Engine.
//
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
//
// Permission has been obtained to use this code under the terms of
// the GNU General Public License v3.0.

#include <stdlib.h>
#if __has_include(<alloca.h>)
#include <alloca.h>
#endif
#include "templates.h"
#include "doomdef.h"
#include "sbar.h"
#include "r_data/r_translate.h"
#include "r_data/colormaps.h"
#include "voxels.h"
#include "r_data/sprites.h"
#include "d_net.h"
#include "po_man.h"
#include "r_utility.h"
#include "i_time.h"
#include "swrenderer/drawers/r_draw.h"
#include "r_thread.h"
#include "swrenderer/things/r_visiblesprite.h"
#include "swrenderer/things/r_voxel.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_translucent_pass.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/viewport/r_spritedrawer.h"
#include "r_memory.h"
#include "swrenderer/r_renderthread.h"

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)

namespace swrenderer
{
	void RenderVoxel::Project(RenderThread *thread, AActor *thing, DVector3 pos, FVoxelDef *voxel, const DVector2 &spriteScale, int renderflags, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector, int lightlevel, bool foggy, FDynamicColormap *basecolormap)
	{
		// transform the origin point
		double tr_x = pos.X - thread->Viewport->viewpoint.Pos.X;
		double tr_y = pos.Y - thread->Viewport->viewpoint.Pos.Y;

		double tz = tr_x * thread->Viewport->viewpoint.TanCos + tr_y * thread->Viewport->viewpoint.TanSin;
		double tx = tr_x * thread->Viewport->viewpoint.Sin - tr_y * thread->Viewport->viewpoint.Cos;

		// [RH] Flip for mirrors
		RenderPortal *renderportal = thread->Portal.get();
		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			tx = -tx;
		}
		//tx2 = tx >> 4;

		// too far off the side?
		if (fabs(tx / 128) > fabs(tz))
		{
			return;
		}

		double xscale = spriteScale.X * voxel->Scale;
		double yscale = spriteScale.Y * voxel->Scale;
		double piv = voxel->Voxel->Mips[0].Pivot.Z;
		double gzt = pos.Z + yscale * piv - thing->Floorclip;
		double gzb = pos.Z + yscale * (piv - voxel->Voxel->Mips[0].SizeZ);
		if (gzt <= gzb)
			return;

		// killough 3/27/98: exclude things totally separated
		// from the viewer, by either water or fake ceilings
		// killough 4/11/98: improve sprite clipping for underwater/fake ceilings

		sector_t *heightsec = thing->Sector->GetHeightSec();

		if (heightsec != nullptr)	// only clip things which are in special sectors
		{
			if (fakeside == WaterFakeSide::AboveCeiling)
			{
				if (gzt < heightsec->ceilingplane.ZatPoint(pos))
					return;
			}
			else if (fakeside == WaterFakeSide::BelowFloor)
			{
				if (gzb >= heightsec->floorplane.ZatPoint(pos))
					return;
			}
			else
			{
				if (gzt < heightsec->floorplane.ZatPoint(pos))
					return;
				if (!(heightsec->MoreFlags & SECMF_FAKEFLOORONLY) && gzb >= heightsec->ceilingplane.ZatPoint(pos))
					return;
			}
		}

		RenderVoxel *vis = thread->FrameMemory->NewObject<RenderVoxel>();

		vis->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		vis->xscale = FLOAT2FIXED(xscale);
		vis->yscale = (float)yscale;
		vis->x1 = renderportal->WindowLeft;
		vis->x2 = renderportal->WindowRight;
		vis->idepth = float(1 / tz);
		vis->floorclip = thing->Floorclip;

		pos.Z -= thing->Floorclip;

		vis->Angle = thing->Angles.Yaw + voxel->AngleOffset;

		int voxelspin = (thing->flags & MF_DROPPED) ? voxel->DroppedSpin : voxel->PlacedSpin;
		if (voxelspin != 0)
		{
			DAngle ang = double(screen->FrameTime) * voxelspin / 1000;
			vis->Angle -= ang;
		}

		vis->pa.vpos = { (float)thread->Viewport->viewpoint.Pos.X, (float)thread->Viewport->viewpoint.Pos.Y, (float)thread->Viewport->viewpoint.Pos.Z };
		vis->pa.vang = FAngle((float)thread->Viewport->viewpoint.Angles.Yaw.Degrees);

		// killough 3/27/98: save sector for special clipping later
		vis->heightsec = heightsec;
		vis->sector = thing->Sector;

		vis->depth = (float)tz;
		vis->gpos = { (float)pos.X, (float)pos.Y, (float)pos.Z };
		vis->gzb = (float)gzb;		// [RH] use gzb, not thing->z
		vis->gzt = (float)gzt;		// killough 3/27/98
		vis->deltax = float(pos.X - thread->Viewport->viewpoint.Pos.X);
		vis->deltay = float(pos.Y - thread->Viewport->viewpoint.Pos.Y);
		vis->renderflags = renderflags;
		if (thing->flags5 & MF5_BRIGHT)
			vis->renderflags |= RF_FULLBRIGHT; // kg3D
		vis->RenderStyle = thing->RenderStyle;
		vis->FillColor = thing->fillcolor;
		vis->Translation = thing->Translation;		// [RH] thing translation table
		vis->FakeFlatStat = fakeside;
		vis->Alpha = float(thing->Alpha);
		vis->fakefloor = fakefloor;
		vis->fakeceiling = fakeceiling;
		vis->bInMirror = renderportal->MirrorFlags & RF_XFLIP;
		//vis->bSplitSprite = false;

		vis->voxel = voxel->Voxel;
		vis->foggy = foggy;

		// The software renderer cannot invert the source without inverting the overlay
		// too. That means if the source is inverted, we need to do the reverse of what
		// the invert overlay flag says to do.
		bool invertcolormap = (vis->RenderStyle.Flags & STYLEF_InvertOverlay) != 0;

		if (vis->RenderStyle.Flags & STYLEF_InvertSource)
		{
			invertcolormap = !invertcolormap;
		}

		// Sprites that are added to the scene must fade to black.
		if (vis->RenderStyle == LegacyRenderStyles[STYLE_Add] && basecolormap->Fade != 0)
		{
			basecolormap = GetSpecialLights(basecolormap->Color, 0, basecolormap->Desaturate);
		}

		bool fullbright = !vis->foggy && ((renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT));
		bool fadeToBlack = (vis->RenderStyle.Flags & STYLEF_FadeToBlack) != 0;

		vis->Light.SetColormap(thread, tz, lightlevel, foggy, basecolormap, fullbright, invertcolormap, fadeToBlack, false, false);

		// Fake a voxel drawing to find its extents..
		SpriteDrawerArgs drawerargs;
		bool visible = drawerargs.SetStyle(thread->Viewport.get(), vis->RenderStyle, vis->Alpha, vis->Translation, vis->FillColor, vis->Light);
		if (!visible)
			return;
		int flags = vis->bInMirror ? (DVF_MIRRORED | DVF_FIND_X1X2) : DVF_FIND_X1X2;
		vis->DrawVoxel(thread, drawerargs, vis->pa.vpos, vis->pa.vang, vis->gpos, vis->Angle, vis->xscale, FLOAT2FIXED(vis->yscale), vis->voxel, nullptr, nullptr, 0, 0, flags);
		if (vis->x1 >= vis->x2)
			return;

		thread->SpriteList->Push(vis);
	}

	void RenderVoxel::Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ, Fake3DTranslucent clip3DFloor)
	{
		auto spr = this;
		auto viewport = thread->Viewport.get();

		SpriteDrawerArgs drawerargs;
		bool visible = drawerargs.SetStyle(viewport, spr->RenderStyle, spr->Alpha, spr->Translation, spr->FillColor, spr->Light);
		if (!visible)
			return;

		int flags = 0;

		/*
		if (colfunc == fuzzcolfunc || colfunc == R_FillColumn)
		{
			flags = DVF_OFFSCREEN | DVF_SPANSONLY;
		}
		else if (colfunc != basecolfunc)
		{
			flags = DVF_OFFSCREEN;
		}
		if (flags != 0)
		{
			CheckOffscreenBuffer(viewport->RenderTarget->GetWidth(), viewport->RenderTarget->GetHeight(), !!(flags & DVF_SPANSONLY));
		}
		*/

		if (spr->bInMirror)
		{
			flags |= DVF_MIRRORED;
		}

		// Render the voxel, either directly to the screen or offscreen.
		DrawVoxel(thread, drawerargs, spr->pa.vpos, spr->pa.vang, spr->gpos, spr->Angle,
			spr->xscale, FLOAT2FIXED(spr->yscale), spr->voxel, cliptop, clipbottom,
			minZ, maxZ, flags);

		/*
		// Blend the voxel, if that's what we need to do.
		if ((flags & ~DVF_MIRRORED) != 0)
		{
			int pixelsize = viewport->RenderTarget->IsBgra() ? 4 : 1;
			for (int x = 0; x < viewwidth; ++x)
			{
				if (!(flags & DVF_SPANSONLY) && (x & 3) == 0)
				{
					rt_initcols(OffscreenColorBuffer + x * OffscreenBufferHeight);
				}
				for (FCoverageBuffer::Span *span = OffscreenCoverageBuffer->Spans[x]; span != NULL; span = span->NextSpan)
				{
					if (flags & DVF_SPANSONLY)
					{
						dc_x = x;
						dc_yl = span->Start;
						dc_yh = span->Stop - 1;
						dc_count = span->Stop - span->Start;
						dc_dest = (ylookup[span->Start] + x) * pixelsize + dc_destorg;
						colfunc();
					}
					else
					{
						rt_span_coverage(x, span->Start, span->Stop - 1);
					}
				}
				if (!(flags & DVF_SPANSONLY) && (x & 3) == 3)
				{
					rt_draw4cols(x - 3);
				}
			}
		}
		*/
	}

	void RenderVoxel::DrawVoxel(
		RenderThread *thread, SpriteDrawerArgs &drawerargs,
		const FVector3 &globalpos, FAngle viewangle, const FVector3 &dasprpos, DAngle dasprang, fixed_t daxscale, fixed_t dayscale, FVoxel *voxobj,
		short *daumost, short *dadmost, int minslabz, int maxslabz, int flags)
	{
		int i, j, k, x, y, syoff, ggxstart, ggystart, nxoff;
		fixed_t cosang, sinang, sprcosang, sprsinang;
		int backx, backy, gxinc, gyinc;
		int daxscalerecip, dayscalerecip, cnt, gxstart, gystart, dazscale;
		int lx, rx, nx, ny, x1 = 0, y1 = 0, x2 = 0, y2 = 0, yinc = 0;
		int yoff, xs = 0, ys = 0, xe, ye, xi = 0, yi = 0, cbackx, cbacky, dagxinc, dagyinc;
		kvxslab_t *voxptr, *voxend;
		FVoxelMipLevel *mip;
		int z1a[64], z2a[64], yplc[64];

		auto viewport = thread->Viewport.get();

		const int nytooclose = viewport->viewwindow.centerxwide * 2100, nytoofar = 32768 * 32768 - 1048576;
		const int xdimenscale = FLOAT2FIXED(viewport->viewwindow.centerxwide * viewport->YaspectMul / 160);
		const double centerxwide_f = viewport->viewwindow.centerxwide;
		const double centerxwidebig_f = centerxwide_f * 65536 * 65536 * 8;

		// Convert to Build's coordinate system.
		fixed_t globalposx = xs_Fix<4>::ToFix(globalpos.X);
		fixed_t globalposy = xs_Fix<4>::ToFix(-globalpos.Y);
		fixed_t globalposz = xs_Fix<8>::ToFix(-globalpos.Z);

		fixed_t dasprx = xs_Fix<4>::ToFix(dasprpos.X);
		fixed_t daspry = xs_Fix<4>::ToFix(-dasprpos.Y);
		fixed_t dasprz = xs_Fix<8>::ToFix(-dasprpos.Z);

		// Shift the scales from 16 bits of fractional precision to 6.
		// Also do some magic voodoo scaling to make them the right size.
		daxscale = daxscale / (0xC000 >> 6);
		dayscale = dayscale / (0xC000 >> 6);
		if (daxscale <= 0 || dayscale <= 0)
		{
			// won't be visible.
			return;
		}

		angle_t viewang = viewangle.BAMs();
		cosang = FLOAT2FIXED(viewangle.Cos()) >> 2;
		sinang = FLOAT2FIXED(-viewangle.Sin()) >> 2;
		sprcosang = FLOAT2FIXED(dasprang.Cos()) >> 2;
		sprsinang = FLOAT2FIXED(-dasprang.Sin()) >> 2;

		// Select mip level
		i = abs(DMulScale(dasprx - globalposx, cosang, daspry - globalposy, sinang, 6));
		i = DivScale(i, MIN(daxscale, dayscale), 6);
		j = xs_Fix<13>::ToFix(viewport->FocalLengthX);
		for (k = 0; i >= j && k < voxobj->NumMips; ++k)
		{
			i >>= 1;
		}
		if (k >= voxobj->NumMips) k = voxobj->NumMips - 1;

		mip = &voxobj->Mips[k];		if (mip->GetSlabData(false) == NULL) return;

		minslabz >>= k;
		maxslabz >>= k;

		daxscale <<= (k + 8); dayscale <<= (k + 8);
		dazscale = DivScale(dayscale, FLOAT2FIXED(viewport->BaseYaspectMul), 16);
		daxscale = fixed_t(daxscale / viewport->YaspectMul);
		daxscale = Scale(daxscale, xdimenscale, viewport->viewwindow.centerxwide << 9);
		dayscale = Scale(dayscale, MulScale(xdimenscale, viewport->viewingrangerecip, 16), viewport->viewwindow.centerxwide << 9);

		daxscalerecip = (1 << 30) / daxscale;
		dayscalerecip = (1 << 30) / dayscale;

		fixed_t piv_x = fixed_t(mip->Pivot.X*256.);
		fixed_t piv_y = fixed_t(mip->Pivot.Y*256.);
		fixed_t piv_z = fixed_t(mip->Pivot.Z*256.);

		x = MulScale(globalposx - dasprx, daxscalerecip, 16);
		y = MulScale(globalposy - daspry, daxscalerecip, 16);
		backx = (DMulScale(x, sprcosang, y, sprsinang, 10) + piv_x) >> 8;
		backy = (DMulScale(y, sprcosang, x, -sprsinang, 10) + piv_y) >> 8;
		cbackx = clamp(backx, 0, mip->SizeX - 1);
		cbacky = clamp(backy, 0, mip->SizeY - 1);

		sprcosang = MulScale(daxscale, sprcosang, 14);
		sprsinang = MulScale(daxscale, sprsinang, 14);

		x = (dasprx - globalposx) - DMulScale(piv_x, sprcosang, piv_y, -sprsinang, 18);
		y = (daspry - globalposy) - DMulScale(piv_y, sprcosang, piv_x, sprsinang, 18);

		cosang = MulScale(cosang, dayscalerecip, 16);
		sinang = MulScale(sinang, dayscalerecip, 16);

		gxstart = y*cosang - x*sinang;
		gystart = x*cosang + y*sinang;
		gxinc = DMulScale(sprsinang, cosang, sprcosang, -sinang, 10);
		gyinc = DMulScale(sprcosang, cosang, sprsinang, sinang, 10);
		if ((abs(globalposz - dasprz) >> 10) >= abs(dazscale)) return;

		x = 0; y = 0; j = MAX(mip->SizeX, mip->SizeY);
		fixed_t *ggxinc = (fixed_t *)alloca((j + 1) * sizeof(fixed_t) * 2);
		fixed_t *ggyinc = ggxinc + (j + 1);
		for (i = 0; i <= j; i++)
		{
			ggxinc[i] = x; x += gxinc;
			ggyinc[i] = y; y += gyinc;
		}

		syoff = DivScale(globalposz - dasprz, MulScale(dazscale, 0xE800, 16), 21) + (piv_z << 7);
		yoff = (abs(gxinc) + abs(gyinc)) >> 1;

		bool useSlabDataBgra = !drawerargs.DrawerNeedsPalInput() && viewport->RenderTarget->IsBgra();
		if (!useSlabDataBgra) voxobj->Remap();
		else voxobj->CreateBgraSlabData();

		int coverageX1 = this->x2;
		int coverageX2 = this->x1;

		const int maxoutblocks = 100;
		VoxelBlock *outblocks = nullptr;
		if ((flags & DVF_FIND_X1X2) == 0)
			outblocks = thread->FrameMemory->AllocMemory<VoxelBlock>(maxoutblocks);
		int nextoutblock = 0;

		for (cnt = 0; cnt < 8; cnt++)
		{
			switch (cnt)
			{
			case 0: xs = 0;				ys = 0;				xi = 1; yi = 1; break;
			case 1: xs = mip->SizeX - 1;	ys = 0;				xi = -1; yi = 1; break;
			case 2: xs = 0;				ys = mip->SizeY - 1;	xi = 1; yi = -1; break;
			case 3: xs = mip->SizeX - 1;	ys = mip->SizeY - 1;	xi = -1; yi = -1; break;
			case 4: xs = 0;				ys = cbacky;		xi = 1; yi = 2; break;
			case 5: xs = mip->SizeX - 1;	ys = cbacky;		xi = -1; yi = 2; break;
			case 6: xs = cbackx;		ys = 0;				xi = 2; yi = 1; break;
			case 7: xs = cbackx;		ys = mip->SizeY - 1;	xi = 2; yi = -1; break;
			}
			xe = cbackx; ye = cbacky;
			if (cnt < 4)
			{
				if ((xi < 0) && (xe >= xs)) continue;
				if ((xi > 0) && (xe <= xs)) continue;
				if ((yi < 0) && (ye >= ys)) continue;
				if ((yi > 0) && (ye <= ys)) continue;
			}
			else
			{
				if ((xi < 0) && (xe > xs)) continue;
				if ((xi > 0) && (xe < xs)) continue;
				if ((yi < 0) && (ye > ys)) continue;
				if ((yi > 0) && (ye < ys)) continue;
				xe += xi; ye += yi;
			}

			i = sgn(ys - backy) + sgn(xs - backx) * 3 + 4;
			switch (i)
			{
			case 6: case 7: x1 = 0;				y1 = 0;				break;
			case 8: case 5: x1 = gxinc;			y1 = gyinc;			break;
			case 0: case 3: x1 = gyinc;			y1 = -gxinc;		break;
			case 2: case 1: x1 = gxinc + gyinc;	y1 = gyinc - gxinc;	break;
			}
			switch (i)
			{
			case 2: case 5: x2 = 0;				y2 = 0;				break;
			case 0: case 1: x2 = gxinc;			y2 = gyinc;			break;
			case 8: case 7: x2 = gyinc;			y2 = -gxinc;		break;
			case 6: case 3: x2 = gxinc + gyinc;	y2 = gyinc - gxinc;	break;
			}
			uint8_t oand = (1 << int(xs < backx)) + (1 << (int(ys < backy) + 2));
			uint8_t oand16 = oand + 16;
			uint8_t oand32 = oand + 32;

			if (yi > 0) { dagxinc = gxinc; dagyinc = MulScale(gyinc, viewport->viewingrangerecip, 16); }
			else { dagxinc = -gxinc; dagyinc = -MulScale(gyinc, viewport->viewingrangerecip, 16); }

			/* Fix for non 90 degree viewing ranges */
			nxoff = MulScale(x2 - x1, viewport->viewingrangerecip, 16);
			x1 = MulScale(x1, viewport->viewingrangerecip, 16);

			ggxstart = gxstart + ggyinc[ys];
			ggystart = gystart - ggxinc[ys];

			for (x = xs; x != xe; x += xi)
			{
				auto SlabData = mip->GetSlabData(true);
				uint8_t *slabxoffs = &SlabData[mip->OffsetX[x]];
				short *xyoffs = &mip->OffsetXY[x * (mip->SizeY + 1)];

				nx = MulScale(ggxstart + ggxinc[x], viewport->viewingrangerecip, 16) + x1;
				ny = ggystart + ggyinc[x];
				for (y = ys; y != ye; y += yi, nx += dagyinc, ny -= dagxinc)
				{
					if ((ny <= nytooclose) || (ny >= nytoofar)) continue;
					voxptr = (kvxslab_t *)(slabxoffs + xyoffs[y]);
					voxend = (kvxslab_t *)(slabxoffs + xyoffs[y + 1]);
					if (voxptr >= voxend) continue;

					lx = xs_RoundToInt(nx * centerxwide_f / (ny + y1)) + viewport->viewwindow.centerx;
					rx = xs_RoundToInt((nx + nxoff) * centerxwide_f / (ny + y2)) + viewport->viewwindow.centerx;

					if (flags & DVF_MIRRORED)
					{
						int t = viewwidth - lx;
						lx = viewwidth - rx;
						rx = t;
					}

					if (lx < this->x1) lx = this->x1;
					if (rx > this->x2) rx = this->x2;
					if (rx <= lx) continue;

					if (flags & DVF_FIND_X1X2)
					{
						coverageX1 = MIN(coverageX1, lx);
						coverageX2 = MAX(coverageX2, rx);
						continue;
					}

					fixed_t l1 = xs_RoundToInt(centerxwidebig_f / (ny - yoff));
					fixed_t l2 = xs_RoundToInt(centerxwidebig_f / (ny + yoff));
					for (; voxptr < voxend; voxptr = (kvxslab_t *)((uint8_t *)voxptr + voxptr->zleng + 3))
					{
						const uint8_t *col = voxptr->col;
						int zleng = voxptr->zleng;
						int ztop = voxptr->ztop;
						fixed_t z1, z2;

						if (ztop < minslabz)
						{
							int diff = minslabz - ztop;
							ztop = minslabz;
							col += diff;
							zleng -= diff;
						}
						if (ztop + zleng > maxslabz)
						{
							int diff = ztop + zleng - maxslabz;
							zleng -= diff;
						}
						if (zleng <= 0) continue;

						j = (ztop << 15) - syoff;
						if (j < 0)
						{
							k = j + (zleng << 15);
							if (k < 0)
							{
								if ((voxptr->backfacecull & oand32) == 0) continue;
								z2 = MulScale(l2, k, 32) + viewport->viewwindow.centery;					/* Below slab */
							}
							else
							{
								if ((voxptr->backfacecull & oand) == 0) continue;	/* Middle of slab */
								z2 = MulScale(l1, k, 32) + viewport->viewwindow.centery;
							}
							z1 = MulScale(l1, j, 32) + viewport->viewwindow.centery;
						}
						else
						{
							if ((voxptr->backfacecull & oand16) == 0) continue;
							z1 = MulScale(l2, j, 32) + viewport->viewwindow.centery;						/* Above slab */
							z2 = MulScale(l1, j + (zleng << 15), 32) + viewport->viewwindow.centery;
						}

						if (z2 <= z1) continue;

						if (zleng == 1)
						{
							yinc = 0;
						}
						else
						{
							if (z2 - z1 >= 1024) yinc = DivScale(zleng, z2 - z1, 16);
							else yinc = (((1 << 24) - 1) / (z2 - z1)) * zleng >> 8;
						}
						// [RH] Clip each column separately, not just by the first one.
						for (int stripwidth = MIN<int>(countof(z1a), rx - lx), lxt = lx;
							lxt < rx;
							(lxt += countof(z1a)), stripwidth = MIN<int>(countof(z1a), rx - lxt))
						{
							// Calculate top and bottom pixels locations
							for (int xxx = 0; xxx < stripwidth; ++xxx)
							{
								if (zleng == 1)
								{
									yplc[xxx] = 0;
									z1a[xxx] = MAX<int>(z1, daumost[lxt + xxx]);
								}
								else
								{
									if (z1 < daumost[lxt + xxx])
									{
										yplc[xxx] = yinc * (daumost[lxt + xxx] - z1);
										z1a[xxx] = daumost[lxt + xxx];
									}
									else
									{
										yplc[xxx] = 0;
										z1a[xxx] = z1;
									}
								}
								z2a[xxx] = MIN<int>(z2, dadmost[lxt + xxx]);
							}

							const uint8_t *columnColors = col;
							if (useSlabDataBgra)
							{
								// The true color slab data array is identical, except its using uint32_t instead of uint8.
								//
								// We can find the same slab column by calculating the offset from the start of SlabData
								// and use that to offset into the BGRA version of the same data.
								columnColors = (const uint8_t *)(&mip->SlabDataBgra[0] + (ptrdiff_t)(col - SlabData));
							}

							// Find top and bottom pixels that match and draw them as one strip
							for (int xxl = 0, xxr; xxl < stripwidth; )
							{
								if (z1a[xxl] >= z2a[xxl])
								{ // No column here
									xxl++;
									continue;
								}
								int z1 = z1a[xxl];
								int z2 = z2a[xxl];
								// How many columns share the same extents?
								for (xxr = xxl + 1; xxr < stripwidth; ++xxr)
								{
									if (z1a[xxr] != z1 || z2a[xxr] != z2)
										break;
								}

								outblocks[nextoutblock].x = lxt + xxl;
								outblocks[nextoutblock].y = z1;
								outblocks[nextoutblock].width = xxr - xxl;
								outblocks[nextoutblock].height = z2 - z1;
								outblocks[nextoutblock].vPos = yplc[xxl];
								outblocks[nextoutblock].vStep = yinc;
								outblocks[nextoutblock].voxels = columnColors;
								outblocks[nextoutblock].voxelsCount = zleng;
								nextoutblock++;

								if (nextoutblock == maxoutblocks)
								{
									drawerargs.DrawVoxelBlocks(thread, outblocks, maxoutblocks);
									outblocks = thread->FrameMemory->AllocMemory<VoxelBlock>(maxoutblocks);
									nextoutblock = 0;
								}

								/*
								for (int x = xxl; x < xxr; ++x)
								{
									drawerargs.SetDest(viewport, lxt + x, z1);
									drawerargs.SetCount(z2 - z1);
									drawerargs.DrawVoxelColumn(thread, yplc[xxl], yinc, columnColors, zleng);
								}
								*/

								/*
								if (!(flags & DVF_OFFSCREEN))
								{
									// Draw directly to the screen.
									R_DrawSlab(xxr - xxl, yplc[xxl], z2 - z1, yinc, col, (ylookup[z1] + lxt + xxl) * pixelsize + dc_destorg);
								}
								else
								{
									// Record the area covered and possibly draw to an offscreen buffer.
									dc_yl = z1;
									dc_yh = z2 - 1;
									dc_count = z2 - z1;
									dc_iscale = yinc;
									for (int x = xxl; x < xxr; ++x)
									{
										OffscreenCoverageBuffer->InsertSpan(lxt + x, z1, z2);
										if (!(flags & DVF_SPANSONLY))
										{
											dc_x = lxt + x;
											rt_initcols(OffscreenColorBuffer + (dc_x & ~3) * OffscreenBufferHeight);
											dc_source = col;
											dc_source2 = nullptr;
											dc_texturefrac = yplc[xxl];
											hcolfunc_pre();
										}
									}
								}
								*/

								xxl = xxr;
							}
						}
					}
				}
			}
		}

		if (flags & DVF_FIND_X1X2)
		{
			this->x1 = coverageX1;
			this->x2 = coverageX2;
		}
		else
		{
			if (nextoutblock != 0)
			{
				drawerargs.DrawVoxelBlocks(thread, outblocks, nextoutblock);
			}
		}
	}

	kvxslab_t *RenderVoxel::GetSlabStart(const FVoxelMipLevel &mip, int x, int y)
	{
		return (kvxslab_t *)&mip.GetSlabData(true)[mip.OffsetX[x] + (int)mip.OffsetXY[x * (mip.SizeY + 1) + y]];
	}

	kvxslab_t *RenderVoxel::GetSlabEnd(const FVoxelMipLevel &mip, int x, int y)
	{
		return GetSlabStart(mip, x, y + 1);
	}
	
	kvxslab_t *RenderVoxel::NextSlab(kvxslab_t *slab)
	{
		return (kvxslab_t*)(((uint8_t*)slab) + 3 + slab->zleng);
	}

	void RenderVoxel::Deinit()
	{
		// Free offscreen buffer
		if (OffscreenColorBuffer != nullptr)
		{
			delete[] OffscreenColorBuffer;
			OffscreenColorBuffer = nullptr;
		}
		if (OffscreenCoverageBuffer != nullptr)
		{
			delete OffscreenCoverageBuffer;
			OffscreenCoverageBuffer = nullptr;
		}
		OffscreenBufferHeight = OffscreenBufferWidth = 0;
	}

	void RenderVoxel::CheckOffscreenBuffer(int width, int height, bool spansonly)
	{
		// Allocates the offscreen coverage buffer and optionally the offscreen
		// color buffer. If they already exist but are the wrong size, they will
		// be reallocated.

		if (OffscreenCoverageBuffer == nullptr)
		{
			assert(OffscreenColorBuffer == nullptr && "The color buffer cannot exist without the coverage buffer");
			OffscreenCoverageBuffer = new FCoverageBuffer(width);
		}
		else if (OffscreenCoverageBuffer->NumLists != (unsigned)width)
		{
			delete OffscreenCoverageBuffer;
			OffscreenCoverageBuffer = new FCoverageBuffer(width);
			if (OffscreenColorBuffer != nullptr)
			{
				delete[] OffscreenColorBuffer;
				OffscreenColorBuffer = nullptr;
			}
		}
		else
		{
			OffscreenCoverageBuffer->Clear();
		}

		if (!spansonly)
		{
			if (OffscreenColorBuffer == nullptr)
			{
				OffscreenColorBuffer = new uint8_t[width * height * 4];
			}
			else if (OffscreenBufferWidth != width || OffscreenBufferHeight != height)
			{
				delete[] OffscreenColorBuffer;
				OffscreenColorBuffer = new uint8_t[width * height * 4];
			}
		}
		OffscreenBufferWidth = width;
		OffscreenBufferHeight = height;
	}

	FCoverageBuffer *RenderVoxel::OffscreenCoverageBuffer;
	int RenderVoxel::OffscreenBufferWidth;
	int RenderVoxel::OffscreenBufferHeight;
	uint8_t *RenderVoxel::OffscreenColorBuffer;

	////////////////////////////////////////////////////////////////////////////

	FCoverageBuffer::FCoverageBuffer(int lists)
		: Spans(nullptr), FreeSpans(nullptr)
	{
		NumLists = lists;
		Spans = new Span *[lists];
		memset(Spans, 0, sizeof(Span*)*lists);
	}

	FCoverageBuffer::~FCoverageBuffer()
	{
		if (Spans != nullptr)
		{
			delete[] Spans;
		}
	}

	void FCoverageBuffer::Clear()
	{
		SpanArena.FreeAll();
		memset(Spans, 0, sizeof(Span*)*NumLists);
		FreeSpans = nullptr;
	}

	void FCoverageBuffer::InsertSpan(int listnum, int start, int stop)
	{
		// start is inclusive.
		// stop is exclusive.

		assert(unsigned(listnum) < NumLists);
		assert(start < stop);

		Span **span_p = &Spans[listnum];
		Span *span;

		if (*span_p == nullptr || (*span_p)->Start > stop)
		{ // This list is empty or the first entry is after this one, so we can just insert the span.
			goto addspan;
		}

		// Insert the new span in order, merging with existing ones.
		while (*span_p != nullptr)
		{
			if ((*span_p)->Stop < start)							// =====		(existing span)
			{ // Span ends before this one starts.					//		  ++++	(new span)
				span_p = &(*span_p)->NextSpan;
				continue;
			}

			// Does the new span overlap or abut the existing one?
			if ((*span_p)->Start <= start)
			{
				if ((*span_p)->Stop >= stop)						// =============
				{ // The existing span completely covers this one.	//     +++++
					return;
				}
			extend:		// Extend the existing span with the new one.		// ======
				span = *span_p;										//     +++++++
				span->Stop = stop;									// (or)  +++++

																	// Free up any spans we just covered up.
				span_p = &(*span_p)->NextSpan;
				while (*span_p != nullptr && (*span_p)->Start <= stop && (*span_p)->Stop <= stop)
				{
					Span *span = *span_p;							// ======  ======
					*span_p = span->NextSpan;						//     +++++++++++++
					span->NextSpan = FreeSpans;
					FreeSpans = span;
				}
				if (*span_p != nullptr && (*span_p)->Start <= stop)	// =======         ========
				{ // Our new span connects two existing spans.		//     ++++++++++++++
				  // They should all be collapsed into a single span.
					span->Stop = (*span_p)->Stop;
					span = *span_p;
					*span_p = span->NextSpan;
					span->NextSpan = FreeSpans;
					FreeSpans = span;
				}
				goto check;
			}
			else if ((*span_p)->Start <= stop)						//        =====
			{ // The new span extends the existing span from		//    ++++
			  // the beginning.										// (or) ++++
				(*span_p)->Start = start;
				if ((*span_p)->Stop < stop)
				{ // The new span also extends the existing span	//     ======
				  // at the bottom									// ++++++++++++++
					goto extend;
				}
				goto check;
			}
			else													//         ======
			{ // No overlap, so insert a new span.					// +++++
				goto addspan;
			}
		}
		// Append a new span to the end of the list.
	addspan:
		span = AllocSpan();
		span->NextSpan = *span_p;
		span->Start = start;
		span->Stop = stop;
		*span_p = span;
	check:
#ifdef _DEBUG
		// Validate the span list: Spans must be in order, and there must be
		// at least one pixel between spans.
		for (span = Spans[listnum]; span != nullptr; span = span->NextSpan)
		{
			assert(span->Start < span->Stop);
			if (span->NextSpan != nullptr)
			{
				assert(span->Stop < span->NextSpan->Start);
			}
		}
#endif
		;
	}

	FCoverageBuffer::Span *FCoverageBuffer::AllocSpan()
	{
		Span *span;

		if (FreeSpans != nullptr)
		{
			span = FreeSpans;
			FreeSpans = span->NextSpan;
		}
		else
		{
			span = (Span *)SpanArena.Alloc(sizeof(Span));
		}
		return span;
	}

#if 0
	void RenderVoxel::Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ)
	{
		auto sprite = this;
		auto viewport = RenderViewport::Instance();

		SpriteDrawerArgs drawerargs;
		bool visible = drawerargs.SetStyle(sprite->RenderStyle, sprite->Alpha, sprite->Translation, sprite->FillColor, sprite->Light);
		if (!visible)
			return;

		DVector3 view_origin = { sprite->pa.vpos.X, sprite->pa.vpos.Y, sprite->pa.vpos.Z };
		FAngle view_angle = sprite->pa.vang;
		DVector3 sprite_origin = { sprite->gpos.X, sprite->gpos.Y, sprite->gpos.Z };
		DAngle sprite_angle = sprite->Angle;
		double sprite_xscale = FIXED2DBL(sprite->xscale);
		double sprite_yscale = sprite->yscale;
		FVoxel *voxel = sprite->voxel;
		
		// Select mipmap level:
		
		double viewSin = view_angle.Cos();
		double viewCos = view_angle.Sin();
		double logmip = fabs((view_origin.X - sprite_origin.X) * viewCos - (view_origin.Y - sprite_origin.Y) * viewSin);
		int miplevel = 0;
		while (miplevel < voxel->NumMips - 1 && logmip >= viewport->FocalLengthX)
		{
			logmip *= 0.5;
			miplevel++;
		}
		
		const FVoxelMipLevel &mip = voxel->Mips[miplevel];
		if (mip.SlabData == nullptr)
			return;

		minZ >>= miplevel;
		maxZ >>= miplevel;
		sprite_xscale *= (1 << miplevel);
		sprite_yscale *= (1 << miplevel);
		
		// Find voxel cube eigenvectors and origin in world space:

		double spriteSin = sprite_angle.Sin();
		double spriteCos = sprite_angle.Cos();
		
		DVector2 dirX(spriteSin * sprite_xscale, -spriteCos * sprite_xscale);
		DVector2 dirY(spriteCos * sprite_xscale, spriteSin * sprite_xscale);
		double dirZ = -sprite_yscale;
		
		DVector3 voxel_origin = sprite_origin;
		voxel_origin.X -= dirX.X * mip.Pivot.X + dirX.Y * mip.Pivot.Y;
		voxel_origin.Y -= dirY.X * mip.Pivot.X + dirY.Y * mip.Pivot.Y;
		voxel_origin.Z -= dirZ * mip.Pivot.Z;
		
		// Voxel cube walking directions:
		
		int startX[4] = { 0, mip.SizeX - 1,             0, mip.SizeX - 1 };
		int startY[4] = { 0,             0, mip.SizeY - 1, mip.SizeY - 1 };
		int stepX[4] = { 1, -1,  1, -1 };
		int stepY[4] = { 1,  1, -1, -1 };
		
		// The point in cube mipmap local space where voxel sides change from front to backfacing:
		
		double dx = (view_origin.X - sprite_origin.X) / sprite_xscale;
		double dy = (view_origin.Y - sprite_origin.Y) / sprite_xscale;
		int backX = (int)(dx * spriteCos - dy * spriteSin + mip.Pivot.X);
		int backY = (int)(dy * spriteCos + dx * spriteSin + mip.Pivot.Y);
		//int endX = clamp(backX, 0, mip.SizeX - 1);
		//int endY = clamp(backY, 0, mip.SizeY - 1);
		int endX = mip.SizeX - 1;// clamp(backX, 0, mip.SizeX - 1);
		int endY = mip.SizeY - 1;// clamp(backY, 0, mip.SizeY - 1);

		// Draw the voxel cube:
		
		for (int index = 0; index < 1; index++)
		{
			/*if ((stepX[index] < 0 && endX >= startX[index]) ||
			    (stepX[index] > 0 && endX <= startX[index]) ||
			    (stepY[index] < 0 && endY >= startY[index]) ||
			    (stepY[index] > 0 && endY <= startY[index])) continue;*/
		
			for (int x = startX[index]; x != endX; x += stepX[index])
			{
				for (int y = startY[index]; y != endY; y += stepY[index])
				{
					kvxslab_t *slab_start = GetSlabStart(mip, x, y);
					kvxslab_t *slab_end = GetSlabEnd(mip, x, y);
				
					for (kvxslab_t *slab = slab_start; slab != slab_end; slab = NextSlab(slab))
					{
						// To do: check slab->backfacecull
						
						int ztop = slab->ztop;
						int zbottom = ztop + slab->zleng;
						
						//ztop = MAX(ztop, minZ);
						//zbottom = MIN(zbottom, maxZ);
						
						for (int z = ztop; z < zbottom; z++)
						{
							uint8_t color = slab->col[z - slab->ztop];
						
							DVector3 voxel_pos = voxel_origin;
							voxel_pos.X += dirX.X * x + dirX.Y * y;
							voxel_pos.Y += dirY.X * x + dirY.Y * y;
							voxel_pos.Z += dirZ * z;
						
							FillBox(thread, drawerargs, voxel_pos, sprite_xscale, sprite_yscale, color, cliptop, clipbottom, false, false);
						}
					}
				}
			}
		}
	}

	void RenderVoxel::FillBox(RenderThread *thread, SpriteDrawerArgs &drawerargs, DVector3 origin, double extentX, double extentY, int color, short *cliptop, short *clipbottom, bool viewspace, bool pixelstretch)
	{
		auto viewport = RenderViewport::Instance();

		DVector3 viewPos = viewport->PointWorldToView(origin);

		if (viewPos.Z < 0.01f)
			return;

		DVector3 screenPos = viewport->PointViewToScreen(viewPos);
		DVector2 screenExtent = viewport->ScaleViewToScreen({ extentX, extentY }, viewPos.Z, pixelstretch);

		int x1 = MAX((int)(screenPos.X - screenExtent.X), 0);
		int x2 = MIN((int)(screenPos.X + screenExtent.X + 0.5f), viewwidth - 1);
		int y1 = MAX((int)(screenPos.Y - screenExtent.Y), 0);
		int y2 = MIN((int)(screenPos.Y + screenExtent.Y + 0.5f), viewheight - 1);

		int pixelsize = viewport->RenderTarget->IsBgra() ? 4 : 1;

		if (y1 < y2)
		{
			for (int x = x1; x < x2; x++)
			{
				int columnY1 = MAX(y1, (int)cliptop[x]);
				int columnY2 = MIN(y2, (int)clipbottom[x]);
				if (columnY1 < columnY2)
				{
					drawerargs.SetDest(x, columnY1);
					drawerargs.SetSolidColor(color);
					drawerargs.SetCount(columnY2 - columnY1);
					drawerargs.FillColumn(thread);
				}
			}
		}
	}
#endif
}

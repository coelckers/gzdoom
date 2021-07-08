//
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
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
** gl_models.cpp
**
** General model handling code
**
**/

#include "filesystem.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "m_crc32.h"
#include "c_console.h"
#include "g_game.h"
#include "doomstat.h"
#include "g_level.h"
#include "r_state.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "r_utility.h"
#include "models.h"
#include "model_kvx.h"
#include "i_time.h"
#include "texturemanager.h"
#include "modelrenderer.h"


#ifdef _MSC_VER
#pragma warning(disable:4244) // warning C4244: conversion from 'double' to 'float', possible loss of data
#endif

CVAR(Bool, gl_interpolate_model_frames, true, CVAR_ARCHIVE)
EXTERN_CVAR (Bool, r_drawvoxels)

extern TDeletingArray<FVoxel *> Voxels;
extern TDeletingArray<FVoxelDef *> VoxelDefs;

void RenderFrameModels(FModelRenderer* renderer, FLevelLocals* Level, const FSpriteModelFrame* smf, const FState* curState, const int curTics, const PClass* ti, int translation);


void RenderModel(FModelRenderer *renderer, float x, float y, float z, FSpriteModelFrame *smf, AActor *actor, double ticFrac)
{
	// Setup transformation.

	int translation = 0;
	if (!(smf->flags & MDL_IGNORETRANSLATION))
		translation = actor->Translation;

	// y scale for a sprite means height, i.e. z in the world!
	float scaleFactorX = actor->Scale.X * smf->xscale;
	float scaleFactorY = actor->Scale.X * smf->yscale;
	float scaleFactorZ = actor->Scale.Y * smf->zscale;
	float pitch = 0;
	float roll = 0;
	double rotateOffset = 0;
	DRotator angles;
	if (actor->renderflags & RF_INTERPOLATEANGLES) // [Nash] use interpolated angles
		angles = actor->InterpolatedAngles(ticFrac);
	else
		angles = actor->Angles;
	float angle = angles.Yaw.Degrees;

	// [BB] Workaround for the missing pitch information.
	if ((smf->flags & MDL_PITCHFROMMOMENTUM))
	{
		const double x = actor->Vel.X;
		const double y = actor->Vel.Y;
		const double z = actor->Vel.Z;

		if (actor->Vel.LengthSquared() > EQUAL_EPSILON)
		{
			// [BB] Calculate the pitch using spherical coordinates.
			if (z || x || y) pitch = float(atan(z / sqrt(x*x + y*y)) / M_PI * 180);

			// Correcting pitch if model is moving backwards
			if (fabs(x) > EQUAL_EPSILON || fabs(y) > EQUAL_EPSILON)
			{
				if ((x * cos(angle * M_PI / 180) + y * sin(angle * M_PI / 180)) / sqrt(x * x + y * y) < 0) pitch *= -1;
			}
			else pitch = fabs(pitch);
		}
	}

	if (smf->flags & MDL_ROTATING)
	{
		if (smf->rotationSpeed > 0.0000000001 || smf->rotationSpeed < -0.0000000001)
		{
			double turns = (I_GetTime() + I_GetTimeFrac()) / (200.0 / smf->rotationSpeed);
			turns -= floor(turns);
			rotateOffset = turns * 360.0;
		}
		else
		{
			rotateOffset = 0.0;
		}
	}

	// Added MDL_USEACTORPITCH and MDL_USEACTORROLL flags processing.
	// If both flags MDL_USEACTORPITCH and MDL_PITCHFROMMOMENTUM are set, the pitch sums up the actor pitch and the velocity vector pitch.
	if (smf->flags & MDL_USEACTORPITCH)
	{
		double d = angles.Pitch.Degrees;
		if (smf->flags & MDL_BADROTATION) pitch += d;
		else pitch -= d;
	}
	if (smf->flags & MDL_USEACTORROLL) roll += angles.Roll.Degrees;

	VSMatrix objectToWorldMatrix;
	objectToWorldMatrix.loadIdentity();

	// Model space => World space
	objectToWorldMatrix.translate(x, z, y);

	// [Nash] take SpriteRotation into account
	angle += actor->SpriteRotation.Degrees;

	// Applying model transformations:
	// 1) Applying actor angle, pitch and roll to the model
	if (smf->flags & MDL_USEROTATIONCENTER)
	{
		objectToWorldMatrix.translate(smf->rotationCenterX, smf->rotationCenterZ, smf->rotationCenterY);
	}
	objectToWorldMatrix.rotate(-angle, 0, 1, 0);
	objectToWorldMatrix.rotate(pitch, 0, 0, 1);
	objectToWorldMatrix.rotate(-roll, 1, 0, 0);
	if (smf->flags & MDL_USEROTATIONCENTER)
	{
		objectToWorldMatrix.translate(-smf->rotationCenterX, -smf->rotationCenterZ, -smf->rotationCenterY);
	}

	// 2) Applying Doomsday like rotation of the weapon pickup models
	// The rotation angle is based on the elapsed time.

	if (smf->flags & MDL_ROTATING)
	{
		objectToWorldMatrix.translate(smf->rotationCenterX, smf->rotationCenterY, smf->rotationCenterZ);
		objectToWorldMatrix.rotate(rotateOffset, smf->xrotate, smf->yrotate, smf->zrotate);
		objectToWorldMatrix.translate(-smf->rotationCenterX, -smf->rotationCenterY, -smf->rotationCenterZ);
	}

	// 3) Scaling model.
	objectToWorldMatrix.scale(scaleFactorX, scaleFactorZ, scaleFactorY);

	// 4) Aplying model offsets (model offsets do not depend on model scalings).
	objectToWorldMatrix.translate(smf->xoffset / smf->xscale, smf->zoffset / smf->zscale, smf->yoffset / smf->yscale);

	// 5) Applying model rotations.
	objectToWorldMatrix.rotate(-smf->angleoffset, 0, 1, 0);
	objectToWorldMatrix.rotate(smf->pitchoffset, 0, 0, 1);
	objectToWorldMatrix.rotate(-smf->rolloffset, 1, 0, 0);

	// consider the pixel stretching. For non-voxels this must be factored out here
	float stretch = (smf->modelIDs[0] != -1 ? Models[smf->modelIDs[0]]->getAspectFactor(actor->Level->info->pixelstretch) : 1.f) / actor->Level->info->pixelstretch;
	objectToWorldMatrix.scale(1, stretch, 1);

	float orientation = scaleFactorX * scaleFactorY * scaleFactorZ;

	renderer->BeginDrawModel(actor->RenderStyle, smf, objectToWorldMatrix, orientation < 0);
	RenderFrameModels(renderer, actor->Level, smf, actor->state, actor->tics, actor->GetClass(), translation);
	renderer->EndDrawModel(actor->RenderStyle, smf);
}

void RenderHUDModel(FModelRenderer *renderer, DPSprite *psp, float ofsX, float ofsY)
{
	AActor * playermo = players[consoleplayer].camera;
	FSpriteModelFrame *smf = FindModelFrame(psp->Caller->GetClass(), psp->GetSprite(), psp->GetFrame(), false);

	// [BB] No model found for this sprite, so we can't render anything.
	if (smf == nullptr)
		return;

	// The model position and orientation has to be drawn independently from the position of the player,
	// but we need to position it correctly in the world for light to work properly.
	VSMatrix objectToWorldMatrix = renderer->GetViewToWorldMatrix();

	// Scaling model (y scale for a sprite means height, i.e. z in the world!).
	objectToWorldMatrix.scale(smf->xscale, smf->zscale, smf->yscale);

	// Aplying model offsets (model offsets do not depend on model scalings).
	objectToWorldMatrix.translate(smf->xoffset / smf->xscale, smf->zoffset / smf->zscale, smf->yoffset / smf->yscale);

	// [BB] Weapon bob, very similar to the normal Doom weapon bob.
	objectToWorldMatrix.rotate(ofsX / 4, 0, 1, 0);
	objectToWorldMatrix.rotate((ofsY - WEAPONTOP) / -4., 1, 0, 0);

	// [BB] For some reason the jDoom models need to be rotated.
	objectToWorldMatrix.rotate(90.f, 0, 1, 0);

	// Applying angleoffset, pitchoffset, rolloffset.
	objectToWorldMatrix.rotate(-smf->angleoffset, 0, 1, 0);
	objectToWorldMatrix.rotate(smf->pitchoffset, 0, 0, 1);
	objectToWorldMatrix.rotate(-smf->rolloffset, 1, 0, 0);

	float orientation = smf->xscale * smf->yscale * smf->zscale;

	renderer->BeginDrawHUDModel(playermo->RenderStyle, objectToWorldMatrix, orientation < 0);
	uint32_t trans = psp->GetTranslation() != 0 ? psp->GetTranslation() : 0;
	if ((psp->Flags & PSPF_PLAYERTRANSLATED)) trans = psp->Owner->mo->Translation;
	RenderFrameModels(renderer, playermo->Level, smf, psp->GetState(), psp->GetTics(), psp->Caller->GetClass(), trans);
	renderer->EndDrawHUDModel(playermo->RenderStyle);
}

void RenderFrameModels(FModelRenderer *renderer, FLevelLocals *Level, const FSpriteModelFrame *smf, const FState *curState, const int curTics, const PClass *ti, int translation)
{
	// [BB] Frame interpolation: Find the FSpriteModelFrame smfNext which follows after smf in the animation
	// and the scalar value inter ( element of [0,1) ), both necessary to determine the interpolated frame.
	FSpriteModelFrame * smfNext = nullptr;
	double inter = 0.;
	if (gl_interpolate_model_frames && !(smf->flags & MDL_NOINTERPOLATION))
	{
		FState *nextState = curState->GetNextState();
		if (curState != nextState && nextState)
		{
			// [BB] To interpolate at more than 35 fps we take tic fractions into account.
			float ticFraction = 0.;
			// [BB] In case the tic counter is frozen we have to leave ticFraction at zero.
			if (ConsoleState == c_up && menuactive != MENU_On && !Level->isFrozen())
			{
				ticFraction = I_GetTimeFrac();
			}
			inter = static_cast<double>(curState->Tics - curTics + ticFraction) / static_cast<double>(curState->Tics);

			// [BB] For some actors (e.g. ZPoisonShroom) spr->actor->tics can be bigger than curState->Tics.
			// In this case inter is negative and we need to set it to zero.
			if (curState->Tics < curTics)
				inter = 0.;
			else
			{
				// [BB] Workaround for actors that use the same frame twice in a row.
				// Most of the standard Doom monsters do this in their see state.
				if ((smf->flags & MDL_INTERPOLATEDOUBLEDFRAMES))
				{
					const FState *prevState = curState - 1;
					if ((curState->sprite == prevState->sprite) && (curState->Frame == prevState->Frame))
					{
						inter /= 2.;
						inter += 0.5;
					}
					if (nextState && ((curState->sprite == nextState->sprite) && (curState->Frame == nextState->Frame)))
					{
						inter /= 2.;
						nextState = nextState->GetNextState();
					}
				}
				if (nextState && inter != 0.0)
					smfNext = FindModelFrame(ti, nextState->sprite, nextState->Frame, false);
			}
		}
	}

	for (int i = 0; i < smf->modelsAmount; i++)
	{
		if (smf->modelIDs[i] != -1)
		{
			FModel * mdl = Models[smf->modelIDs[i]];
			auto tex = smf->skinIDs[i].isValid() ? TexMan.GetGameTexture(smf->skinIDs[i], true) : nullptr;
			mdl->BuildVertexBuffer(renderer);

			mdl->PushSpriteMDLFrame(smf, i);

			if (smfNext && smf->modelframes[i] != smfNext->modelframes[i])
				mdl->RenderFrame(renderer, tex, smf->modelframes[i], smfNext->modelframes[i], inter, translation);
			else
				mdl->RenderFrame(renderer, tex, smf->modelframes[i], smf->modelframes[i], 0.f, translation);
		}
	}
}


static TArray<int> SpriteModelHash;
//TArray<FStateModelFrame> StateModelFrames;

//===========================================================================
//
// InitModels
//
//===========================================================================

static void ParseModelDefLump(int Lump);

void InitModels()
{
	Models.DeleteAndClear();
	SpriteModelFrames.Clear();
	SpriteModelHash.Clear();

	// First, create models for each voxel
	for (unsigned i = 0; i < Voxels.Size(); i++)
	{
		FVoxelModel *md = new FVoxelModel(Voxels[i], false);
		Voxels[i]->VoxelIndex = Models.Push(md);
	}
	// now create GL model frames for the voxeldefs
	for (unsigned i = 0; i < VoxelDefs.Size(); i++)
	{
		FVoxelModel *md = (FVoxelModel*)Models[VoxelDefs[i]->Voxel->VoxelIndex];
		FSpriteModelFrame smf;
		memset(&smf, 0, sizeof(smf));
		smf.isVoxel = true;
		smf.modelsAmount = 1;
		smf.modelframes.Alloc(1);
		smf.modelframes[0] = -1;
		smf.modelIDs.Alloc(1);
		smf.modelIDs[0] = VoxelDefs[i]->Voxel->VoxelIndex;
		smf.skinIDs.Alloc(1);
		smf.skinIDs[0] = md->GetPaletteTexture();
		smf.xscale = smf.yscale = smf.zscale = VoxelDefs[i]->Scale;
		smf.angleoffset = VoxelDefs[i]->AngleOffset.Degrees;
		if (VoxelDefs[i]->PlacedSpin != 0)
		{
			smf.yrotate = 1.f;
			smf.rotationSpeed = VoxelDefs[i]->PlacedSpin / 55.55f;
			smf.flags |= MDL_ROTATING;
		}
		VoxelDefs[i]->VoxeldefIndex = SpriteModelFrames.Push(smf);
		if (VoxelDefs[i]->PlacedSpin != VoxelDefs[i]->DroppedSpin)
		{
			if (VoxelDefs[i]->DroppedSpin != 0)
			{
				smf.yrotate = 1.f;
				smf.rotationSpeed = VoxelDefs[i]->DroppedSpin / 55.55f;
				smf.flags |= MDL_ROTATING;
			}
			else
			{
				smf.yrotate = 0;
				smf.rotationSpeed = 0;
				smf.flags &= ~MDL_ROTATING;
			}
			SpriteModelFrames.Push(smf);
		}
	}

	int Lump;
	int lastLump = 0;
	while ((Lump = fileSystem.FindLump("MODELDEF", &lastLump)) != -1)
	{
		ParseModelDefLump(Lump);
	}

	// create a hash table for quick access
	SpriteModelHash.Resize(SpriteModelFrames.Size ());
	memset(SpriteModelHash.Data(), 0xff, SpriteModelFrames.Size () * sizeof(int));

	for (unsigned int i = 0; i < SpriteModelFrames.Size (); i++)
	{
		int j = ModelFrameHash(&SpriteModelFrames[i]) % SpriteModelFrames.Size ();

		SpriteModelFrames[i].hashnext = SpriteModelHash[j];
		SpriteModelHash[j]=i;
	}
}

static void ParseModelDefLump(int Lump)
{
	FScanner sc(Lump);
	while (sc.GetString())
	{
		if (sc.Compare("model"))
		{
			int index, surface;
			FString path = "";
			sc.MustGetString();

			FSpriteModelFrame smf;
			memset(&smf, 0, sizeof(smf));
			smf.xscale=smf.yscale=smf.zscale=1.f;

			auto type = PClass::FindClass(sc.String);
			if (!type || type->Defaults == nullptr)
			{
				sc.ScriptError("MODELDEF: Unknown actor type '%s'\n", sc.String);
			}
			smf.type = type;
			FScanner::SavedPos scPos = sc.SavePos();
			sc.MustGetStringName("{");
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				if (sc.Compare("model"))
				{
					sc.MustGetNumber();
					index = sc.Number;
					if (index < 0)
					{
						sc.ScriptError("Model index must be 0 or greater in %s", type->TypeName.GetChars());
					}
					smf.modelsAmount = index + 1;
				}
			}
			//Make sure modelsAmount is at least equal to MIN_MODELS(4) to ensure compatibility with old mods
			if (smf.modelsAmount < MIN_MODELS)
			{
				smf.modelsAmount = MIN_MODELS;
			}

			const auto initArray = [](auto& array, const unsigned count, const auto value)
			{
				array.Alloc(count);
				std::fill(array.begin(), array.end(), value);
			};

			initArray(smf.modelIDs, smf.modelsAmount, -1);
			initArray(smf.skinIDs, smf.modelsAmount, FNullTextureID());
			initArray(smf.surfaceskinIDs, smf.modelsAmount * MD3_MAX_SURFACES, FNullTextureID());
			initArray(smf.modelframes, smf.modelsAmount, 0);

			sc.RestorePos(scPos);
			sc.MustGetStringName("{");
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				if (sc.Compare("path"))
				{
					sc.MustGetString();
					FixPathSeperator(sc.String);
					path = sc.String;
					if (path[(int)path.Len()-1]!='/') path+='/';
				}
				else if (sc.Compare("model"))
				{
					sc.MustGetNumber();
					index = sc.Number;
					if (index < 0)
					{
						sc.ScriptError("Model index must be 0 or greater in %s", type->TypeName.GetChars());
					}
					else if (index >= smf.modelsAmount)
					{
						sc.ScriptError("Too many models in %s", type->TypeName.GetChars());
					}
					sc.MustGetString();
					FixPathSeperator(sc.String);
					smf.modelIDs[index] = FindModel(path.GetChars(), sc.String);
					if (smf.modelIDs[index] == -1)
					{
						Printf("%s: model not found in %s\n", sc.String, path.GetChars());
					}
				}
				else if (sc.Compare("scale"))
				{
					sc.MustGetFloat();
					smf.xscale = sc.Float;
					sc.MustGetFloat();
					smf.yscale = sc.Float;
					sc.MustGetFloat();
					smf.zscale = sc.Float;
				}
				// [BB] Added zoffset reading.
				// Now it must be considered deprecated.
				else if (sc.Compare("zoffset"))
				{
					sc.MustGetFloat();
					smf.zoffset=sc.Float;
				}
				// Offset reading.
				else if (sc.Compare("offset"))
				{
					sc.MustGetFloat();
					smf.xoffset = sc.Float;
					sc.MustGetFloat();
					smf.yoffset = sc.Float;
					sc.MustGetFloat();
					smf.zoffset = sc.Float;
				}
				// angleoffset, pitchoffset and rolloffset reading.
				else if (sc.Compare("angleoffset"))
				{
					sc.MustGetFloat();
					smf.angleoffset = sc.Float;
				}
				else if (sc.Compare("pitchoffset"))
				{
					sc.MustGetFloat();
					smf.pitchoffset = sc.Float;
				}
				else if (sc.Compare("rolloffset"))
				{
					sc.MustGetFloat();
					smf.rolloffset = sc.Float;
				}
				// [BB] Added model flags reading.
				else if (sc.Compare("ignoretranslation"))
				{
					smf.flags |= MDL_IGNORETRANSLATION;
				}
				else if (sc.Compare("pitchfrommomentum"))
				{
					smf.flags |= MDL_PITCHFROMMOMENTUM;
				}
				else if (sc.Compare("inheritactorpitch"))
				{
					smf.flags |= MDL_USEACTORPITCH | MDL_BADROTATION;
				}
				else if (sc.Compare("inheritactorroll"))
				{
					smf.flags |= MDL_USEACTORROLL;
				}
				else if (sc.Compare("useactorpitch"))
				{
					smf.flags |= MDL_USEACTORPITCH;
				}
				else if (sc.Compare("useactorroll"))
				{
					smf.flags |= MDL_USEACTORROLL;
				}
				else if (sc.Compare("rotating"))
				{
					smf.flags |= MDL_ROTATING;
					smf.xrotate = 0.;
					smf.yrotate = 1.;
					smf.zrotate = 0.;
					smf.rotationCenterX = 0.;
					smf.rotationCenterY = 0.;
					smf.rotationCenterZ = 0.;
					smf.rotationSpeed = 1.;
				}
				else if (sc.Compare("rotation-speed"))
				{
					sc.MustGetFloat();
					smf.rotationSpeed = sc.Float;
				}
				else if (sc.Compare("rotation-vector"))
				{
					sc.MustGetFloat();
					smf.xrotate = sc.Float;
					sc.MustGetFloat();
					smf.yrotate = sc.Float;
					sc.MustGetFloat();
					smf.zrotate = sc.Float;
				}
				else if (sc.Compare("rotation-center"))
				{
					sc.MustGetFloat();
					smf.rotationCenterX = sc.Float;
					sc.MustGetFloat();
					smf.rotationCenterY = sc.Float;
					sc.MustGetFloat();
					smf.rotationCenterZ = sc.Float;
				}
				else if (sc.Compare("interpolatedoubledframes"))
				{
					smf.flags |= MDL_INTERPOLATEDOUBLEDFRAMES;
				}
				else if (sc.Compare("nointerpolation"))
				{
					smf.flags |= MDL_NOINTERPOLATION;
				}
				else if (sc.Compare("skin"))
				{
					sc.MustGetNumber();
					index=sc.Number;
					if (index<0 || index>= smf.modelsAmount)
					{
						sc.ScriptError("Too many models in %s", type->TypeName.GetChars());
					}
					sc.MustGetString();
					FixPathSeperator(sc.String);
					if (sc.Compare(""))
					{
						smf.skinIDs[index]=FNullTextureID();
					}
					else
					{
						smf.skinIDs[index] = LoadSkin(path.GetChars(), sc.String);
						if (!smf.skinIDs[index].isValid())
						{
							Printf("Skin '%s' not found in '%s'\n",
								sc.String, type->TypeName.GetChars());
						}
					}
				}
				else if (sc.Compare("surfaceskin"))
				{
					sc.MustGetNumber();
					index = sc.Number;
					sc.MustGetNumber();
					surface = sc.Number;

					if (index<0 || index >= smf.modelsAmount)
					{
						sc.ScriptError("Too many models in %s", type->TypeName.GetChars());
					}

					if (surface<0 || surface >= MD3_MAX_SURFACES)
					{
						sc.ScriptError("Invalid MD3 Surface %d in %s", MD3_MAX_SURFACES, type->TypeName.GetChars());
					}

					sc.MustGetString();
					FixPathSeperator(sc.String);
					int ssIndex = surface + index * MD3_MAX_SURFACES;
					if (sc.Compare(""))
					{
						smf.surfaceskinIDs[ssIndex] = FNullTextureID();
					}
					else
					{
						smf.surfaceskinIDs[ssIndex] = LoadSkin(path.GetChars(), sc.String);
						if (!smf.surfaceskinIDs[ssIndex].isValid())
						{
							Printf("Surface Skin '%s' not found in '%s'\n",
								sc.String, type->TypeName.GetChars());
						}
					}
				}
				else if (sc.Compare("frameindex") || sc.Compare("frame"))
				{
					bool isframe=!!sc.Compare("frame");

					sc.MustGetString();
					smf.sprite = -1;
					for (int i = 0; i < (int)sprites.Size (); ++i)
					{
						if (strnicmp (sprites[i].name, sc.String, 4) == 0)
						{
							if (sprites[i].numframes==0)
							{
								//sc.ScriptError("Sprite %s has no frames", sc.String);
							}
							smf.sprite = i;
							break;
						}
					}
					if (smf.sprite==-1)
					{
						sc.ScriptError("Unknown sprite %s in model definition for %s", sc.String, type->TypeName.GetChars());
					}

					sc.MustGetString();
					FString framechars = sc.String;

					sc.MustGetNumber();
					index=sc.Number;
					if (index<0 || index>= smf.modelsAmount)
					{
						sc.ScriptError("Too many models in %s", type->TypeName.GetChars());
					}
					if (isframe)
					{
						sc.MustGetString();
						if (smf.modelIDs[index] != -1)
						{
							FModel *model = Models[smf.modelIDs[index]];
							smf.modelframes[index] = model->FindFrame(sc.String);
							if (smf.modelframes[index]==-1) sc.ScriptError("Unknown frame '%s' in %s", sc.String, type->TypeName.GetChars());
						}
						else smf.modelframes[index] = -1;
					}
					else
					{
						sc.MustGetNumber();
						smf.modelframes[index] = sc.Number;
					}

					for(int i=0; framechars[i]>0; i++)
					{
						char map[29]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
						int c = toupper(framechars[i])-'A';

						if (c<0 || c>=29)
						{
							sc.ScriptError("Invalid frame character %c found", c+'A');
						}
						if (map[c]) continue;
						smf.frame=c;
						SpriteModelFrames.Push(smf);
						GetDefaultByType(type)->hasmodel = true;
						map[c]=1;
					}
				}
				else if (sc.Compare("dontcullbackfaces"))
				{
					smf.flags |= MDL_DONTCULLBACKFACES;
				}
				else if (sc.Compare("userotationcenter"))
				{
					smf.flags |= MDL_USEROTATIONCENTER;
					smf.rotationCenterX = 0.;
					smf.rotationCenterY = 0.;
					smf.rotationCenterZ = 0.;
				}
				else
				{
					sc.ScriptMessage("Unrecognized string \"%s\"", sc.String);
				}
			}
		}
		else if (sc.Compare("#include"))
		{
			sc.MustGetString();
			// This is not using sc.Open because it can print a more useful error message when done here
			int includelump = fileSystem.CheckNumForFullName(sc.String, true);
			if (includelump == -1)
			{
				if (strcmp(sc.String, "sentinel.modl") != 0) // Gene Tech mod has a broken #include statement
					sc.ScriptError("Lump '%s' not found", sc.String);
			}
			else
			{
				ParseModelDefLump(includelump);
			}
		}
	}
}

//===========================================================================
//
// FindModelFrame
//
//===========================================================================

FSpriteModelFrame * FindModelFrame(const PClass * ti, int sprite, int frame, bool dropped)
{
	if (GetDefaultByType(ti)->hasmodel)
	{
		FSpriteModelFrame smf;

		memset(&smf, 0, sizeof(smf));
		smf.type=ti;
		smf.sprite=sprite;
		smf.frame=frame;

		int hash = SpriteModelHash[ModelFrameHash(&smf) % SpriteModelFrames.Size()];

		while (hash>=0)
		{
			FSpriteModelFrame * smff = &SpriteModelFrames[hash];
			if (smff->type==ti && smff->sprite==sprite && smff->frame==frame) return smff;
			hash=smff->hashnext;
		}
	}

	// Check for voxel replacements
	if (r_drawvoxels)
	{
		spritedef_t *sprdef = &sprites[sprite];
		if (frame < sprdef->numframes)
		{
			spriteframe_t *sprframe = &SpriteFrames[sprdef->spriteframes + frame];
			if (sprframe->Voxel != nullptr)
			{
				int index = sprframe->Voxel->VoxeldefIndex;
				if (dropped && sprframe->Voxel->DroppedSpin !=sprframe->Voxel->PlacedSpin) index++;
				return &SpriteModelFrames[index];
			}
		}
	}
	return nullptr;
}

//===========================================================================
//
// IsHUDModelForPlayerAvailable
//
//===========================================================================

bool IsHUDModelForPlayerAvailable (player_t * player)
{
	if (player == nullptr || player->psprites == nullptr)
		return false;

	// [MK] check that at least one psprite uses models
	for (DPSprite *psp = player->psprites; psp != nullptr && psp->GetID() < PSP_TARGETCENTER; psp = psp->GetNext())
	{
		FSpriteModelFrame *smf = FindModelFrame(psp->Caller->GetClass(), psp->GetSprite(), psp->GetFrame(), false);
		if ( smf != nullptr ) return true;
	}
	return false;
}

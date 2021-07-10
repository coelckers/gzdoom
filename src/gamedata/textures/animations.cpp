/*
** r_anim.cpp
** Routines for handling texture animation.
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

#include "doomtype.h"
#include "cmdlib.h"
#include "engineerrors.h"
#include "r_sky.h"
#include "m_random.h"
#include "d_player.h"
#include "p_spec.h"
#include "filesystem.h"
#include "serializer.h"
#include "animations.h"
#include "texturemanager.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------
FTextureAnimator TexAnim;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FRandom pr_animatepictures ("AnimatePics");

// CODE --------------------------------------------------------------------

void FTextureAnimator::DeleteAll()
{
	for (unsigned i = 0; i < mAnimations.Size(); i++)
	{
		if (mAnimations[i] != NULL)
		{
			M_Free(mAnimations[i]);
			mAnimations[i] = NULL;
		}
	}
	mAnimations.Clear();

	for (unsigned i = 0; i < mSwitchDefs.Size(); i++)
	{
		if (mSwitchDefs[i] != NULL)
		{
			M_Free(mSwitchDefs[i]);
			mSwitchDefs[i] = NULL;
		}
	}
	mSwitchDefs.Clear();

	for (unsigned i = 0; i < mAnimatedDoors.Size(); i++)
	{
		if (mAnimatedDoors[i].TextureFrames != NULL)
		{
			delete[] mAnimatedDoors[i].TextureFrames;
			mAnimatedDoors[i].TextureFrames = NULL;
		}
	}
	mAnimatedDoors.Clear();
}

//==========================================================================
//
// FTextureAnimator :: AddAnim
//
// Adds a new animation to the array. If one with the same basepic as the
// new one already exists, it is replaced.
//
//==========================================================================

FAnimDef *FTextureAnimator::AddAnim (FAnimDef *anim)
{
	// Search for existing duplicate.
	for (unsigned int i = 0; i < mAnimations.Size(); ++i)
	{
		if (mAnimations[i]->BasePic == anim->BasePic)
		{
			// Found one!
			M_Free (mAnimations[i]);
			mAnimations[i] = anim;
			return anim;
		}
	}
	// Didn't find one, so add it at the end.
	mAnimations.Push (anim);
	return anim;
}

//==========================================================================
//
// FTextureAnimator :: AddSimpleAnim
//
// Creates an animation with simple characteristics. This is used for
// original Doom (non-ANIMDEFS-style) animations and Build animations.
//
//==========================================================================

FAnimDef *FTextureAnimator::AddSimpleAnim (FTextureID picnum, int animcount, uint32_t speedmin, uint32_t speedrange)
{
	if (TexMan.AreTexturesCompatible(picnum, picnum + (animcount - 1)))
	{
		FAnimDef *anim = (FAnimDef *)M_Malloc (sizeof(FAnimDef));
		anim->CurFrame = 0;
		anim->BasePic = picnum;
		anim->NumFrames = animcount;
		anim->AnimType = FAnimDef::ANIM_Forward;
		anim->bDiscrete = false;
		anim->SwitchTime = 0;
		anim->Frames[0].SpeedMin = speedmin;
		anim->Frames[0].SpeedRange = speedrange;
		anim->Frames[0].FramePic = anim->BasePic;
		return AddAnim (anim);
	}
	return NULL;
}

//==========================================================================
//
// FTextureAnimator :: AddComplexAnim
//
// Creates an animation with individually defined frames.
//
//==========================================================================

FAnimDef *FTextureAnimator::AddComplexAnim (FTextureID picnum, const TArray<FAnimDef::FAnimFrame> &frames)
{
	FAnimDef *anim = (FAnimDef *)M_Malloc (sizeof(FAnimDef) + (frames.Size()-1) * sizeof(frames[0]));
	anim->BasePic = picnum;
	anim->NumFrames = frames.Size();
	anim->CurFrame = 0;
	anim->AnimType = FAnimDef::ANIM_Forward;
	anim->bDiscrete = true;
	anim->SwitchTime = 0;
	memcpy (&anim->Frames[0], &frames[0], frames.Size() * sizeof(frames[0]));
	return AddAnim (anim);
}

//==========================================================================
//
// FTextureAnimator :: Initanimated
//
// [description copied from BOOM]
// Load the table of animation definitions, checking for existence of
// the start and end of each frame. If the start doesn't exist the sequence
// is skipped, if the last doesn't exist, BOOM exits.
//
// Wall/Flat animation sequences, defined by name of first and last frame,
// The full animation sequence is given using all lumps between the start
// and end entry, in the order found in the WAD file.
//
// This routine modified to read its data from a predefined lump or
// PWAD lump called ANIMATED rather than a static table in this module to
// allow wad designers to insert or modify animation sequences.
//
// Lump format is an array of byte packed animdef_t structures, terminated
// by a structure with istexture == -1. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
// [RH] Rewritten to support BOOM ANIMATED lump but also make absolutely
//		no assumptions about how the compiler packs the animdefs array.
//
// Since animdef_t no longer exists in ZDoom, here is some documentation
// of the format:
//
//   This is an array of <n> entries, terminated by a 0xFF byte. Each entry
//   is 23 bytes long and consists of the following fields:
//     Byte      0: Bit 1 set for wall texture, clear for flat texture.
//     Bytes   1-9: '\0'-terminated name of first texture.
//     Bytes 10-18: '\0'-terminated name of last texture.
//     Bytes 19-22: Tics per frame (stored in little endian order).
//
//==========================================================================

CVAR(Bool, debuganimated, false, 0)

void FTextureAnimator::InitAnimated (void)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable;
		// I think better not! This is only for old ANIMATED definitions that
		// don't know about ZDoom's more flexible texture system.
		// | FTextureAnimator::FTextureManager::TEXMAN_TryAny;

	int lumpnum = fileSystem.CheckNumForName ("ANIMATED");
	if (lumpnum != -1)
	{
		FileData animatedlump = fileSystem.ReadFile (lumpnum);
		int animatedlen = fileSystem.FileLength(lumpnum);
		const uint8_t *animdefs = (const uint8_t *)animatedlump.GetMem();
		const uint8_t *anim_p;
		FTextureID pic1, pic2;
		int animtype;
		uint32_t animspeed;

		// Init animation
		animtype = FAnimDef::ANIM_Forward;

		for (anim_p = animdefs; *anim_p != 0xFF; anim_p += 23)
		{
			// make sure the current chunk of data is inside the lump boundaries.
			if (anim_p + 22 >= animdefs + animatedlen)
			{
				I_Error("Tried to read past end of ANIMATED lump.");
			}
			if (*anim_p /* .istexture */ & 1)
			{
				// different episode ?
				if (!(pic1 = TexMan.CheckForTexture ((const char*)(anim_p + 10) /* .startname */, ETextureType::Wall, texflags)).Exists() ||
					!(pic2 = TexMan.CheckForTexture ((const char*)(anim_p + 1) /* .endname */, ETextureType::Wall, texflags)).Exists())
					continue;		

				// [RH] Bit 1 set means allow decals on walls with this texture
				bool nodecals = !(*anim_p & 2);
				TexMan.GameTexture(pic2)->SetNoDecals(nodecals);
				TexMan.GameTexture(pic1)->SetNoDecals(nodecals);
			}
			else
			{
				if (!(pic1 = TexMan.CheckForTexture ((const char*)(anim_p + 10) /* .startname */, ETextureType::Flat, texflags)).Exists() ||
					!(pic2 = TexMan.CheckForTexture ((const char*)(anim_p + 1) /* .startname */, ETextureType::Flat, texflags)).Exists())
					continue;
			}

			auto tex1 = TexMan.GameTexture(pic1);
			auto tex2 = TexMan.GameTexture(pic2);

			animspeed = (anim_p[19] << 0)  | (anim_p[20] << 8) |
						(anim_p[21] << 16) | (anim_p[22] << 24);

			// SMMU-style swirly hack? Don't apply on already-warping texture
			if (animspeed > 65535 && tex1 != NULL && !tex1->isWarped())
			{
				tex1->SetWarpStyle(2);
			}
			// These tests were not really relevant for swirling textures, or even potentially
			// harmful, so they have been moved to the else block.
			else
			{
				if (tex1->GetUseType() != tex2->GetUseType())
				{
					// not the same type - 
					continue;
				}

				if (debuganimated)
				{
					Printf("Defining animation '%s' (texture %d, lump %d, file %d) to '%s' (texture %d, lump %d, file %d)\n",
						tex1->GetName().GetChars(), pic1.GetIndex(), tex1->GetSourceLump(), fileSystem.GetFileContainer(tex1->GetSourceLump()),
						tex2->GetName().GetChars(), pic2.GetIndex(), tex2->GetSourceLump(), fileSystem.GetFileContainer(tex2->GetSourceLump()));
				}

				if (pic1 == pic2)
				{
					// This animation only has one frame. Skip it. (Doom aborted instead.)
					Printf ("Animation %s in ANIMATED has only one frame\n", (const char*)(anim_p + 10));
					continue;
				}
				// [RH] Allow for backward animations as well as forward.
				else if (pic1 > pic2)
				{
					std::swap (pic1, pic2);
					animtype = FAnimDef::ANIM_Backward;
				}

				// Speed is stored as tics, but we want ms so scale accordingly.
				FAnimDef *adef = AddSimpleAnim (pic1, pic2 - pic1 + 1, Scale (animspeed, 1000, TICRATE));
				if (adef != NULL) adef->AnimType = animtype;
			}
		}
	}
}

//==========================================================================
//
// FTextureAnimator :: InitAnimDefs
//
// This uses a Hexen ANIMDEFS lump to define the animation sequences
//
//==========================================================================

void FTextureAnimator::InitAnimDefs ()
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	int lump, lastlump = 0;
	
	while ((lump = fileSystem.FindLump ("ANIMDEFS", &lastlump)) != -1)
	{
		FScanner sc(lump);

		while (sc.GetString ())
		{
			if (sc.Compare ("flat"))
			{
				ParseAnim (sc, ETextureType::Flat);
			}
			else if (sc.Compare ("texture"))
			{
				ParseAnim (sc, ETextureType::Wall);
			}
			else if (sc.Compare ("switch"))
			{
				ProcessSwitchDef (sc);
			}
			// [GRB] Added warping type 2
			else if (sc.Compare ("warp") || sc.Compare ("warp2"))
			{
				ParseWarp(sc);
			}
			else if (sc.Compare ("cameratexture"))
			{
				ParseCameraTexture(sc);
			}
			else if (sc.Compare ("animatedDoor"))
			{
				ParseAnimatedDoor (sc);
			}
			else if (sc.Compare("skyoffset"))
			{
				sc.MustGetString ();
				FTextureID picnum = TexMan.CheckForTexture (sc.String, ETextureType::Wall, texflags);
				sc.MustGetNumber();
				if (picnum.Exists())
				{
					TexMan.GameTexture(picnum)->SetSkyOffset(sc.Number);
				}
			}
			else
			{
				sc.ScriptError (NULL);
			}
		}
	}
}

//==========================================================================
//
// FTextureAnimator :: ParseAnim
//
// Parse a single animation definition out of an ANIMDEFS lump and
// create the corresponding animation structure.
//
//==========================================================================

void FTextureAnimator::ParseAnim (FScanner &sc, ETextureType usetype)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	TArray<FAnimDef::FAnimFrame> frames (32);
	FTextureID picnum;
	int defined = 0;
	bool optional = false, missing = false;
	FAnimDef *ani = NULL;
	uint8_t type = FAnimDef::ANIM_Forward;

	sc.MustGetString ();
	if (sc.Compare ("optional"))
	{
		optional = true;
		sc.MustGetString ();
	}
	picnum = TexMan.CheckForTexture (sc.String, usetype, texflags);

	if (!picnum.Exists())
	{
		if (optional)
		{
			missing = true;
		}
		else
		{
			Printf (PRINT_DEFAULT, PRINTF_BOLD, "ANIMDEFS: Can't find %s\n", sc.String);
		}
	}

	// no decals on animating textures, by default
	if (picnum.isValid())
	{
		TexMan.GameTexture(picnum)->SetNoDecals(true);
	}

	while (sc.GetString ())
	{
		if (sc.Compare ("allowdecals"))
		{
			if (picnum.isValid())
			{
				TexMan.GameTexture(picnum)->SetNoDecals(false);
			}
			continue;
		}
		else if (sc.Compare ("Oscillate"))
		{
			if (type == FAnimDef::ANIM_Random)
			{
				sc.ScriptError ("You cannot use \"random\" and \"oscillate\" together in a single animation.");
			}
			type = FAnimDef::ANIM_OscillateUp;
		}
		else if (sc.Compare("Random"))
		{
			if (type == FAnimDef::ANIM_OscillateUp)
			{
				sc.ScriptError ("You cannot use \"random\" and \"oscillate\" together in a single animation.");
			}
			type = FAnimDef::ANIM_Random;
		}
		else if (sc.Compare ("range"))
		{
			if (picnum.Exists() && TexMan.GameTexture(picnum)->GetName().IsEmpty())
			{
				// long texture name: We cannot do ranged anims on these because they have no defined order
				sc.ScriptError ("You cannot use \"range\" for long texture names.");
			}
			if (defined == 2)
			{
				sc.ScriptError ("You cannot use \"pic\" and \"range\" together in a single animation.");
			}
			if (defined == 1)
			{
				sc.ScriptError ("You can only use one \"range\" per animation.");
			}
			defined = 1;
			ani = ParseRangeAnim (sc, picnum, usetype, missing);
		}
		else if (sc.Compare ("pic"))
		{
			if (defined == 1)
			{
				sc.ScriptError ("You cannot use \"pic\" and \"range\" together in a single animation.");
			}
			defined = 2;
			ParsePicAnim (sc, picnum, usetype, missing, frames);
		}
		else
		{
			sc.UnGet ();
			break;
		}
	}

	// If base pic is not present, don't add this anim
	// ParseRangeAnim adds the anim itself, but ParsePicAnim does not.
	if (picnum.isValid() && defined == 2)
	{
		if (frames.Size() < 2)
		{
			sc.ScriptError ("Animation needs at least 2 frames");
		}
		ani = AddComplexAnim (picnum, frames);
	}
	if (ani != NULL && type != FAnimDef::ANIM_Forward)
	{
		if (ani->AnimType == FAnimDef::ANIM_Backward && type == FAnimDef::ANIM_OscillateUp) ani->AnimType = FAnimDef::ANIM_OscillateDown;
		else ani->AnimType = type;
	}
}

//==========================================================================
//
// FTextureAnimator :: ParseRangeAnim
//
// Parse an animation defined using "range". Not that one range entry is
// enough to define a complete animation, unlike "pic".
//
//==========================================================================

FAnimDef *FTextureAnimator::ParseRangeAnim (FScanner &sc, FTextureID picnum, ETextureType usetype, bool missing)
{
	int type;
	FTextureID framenum;
	uint32_t min, max;

	type = FAnimDef::ANIM_Forward;
	framenum = ParseFramenum (sc, picnum, usetype, missing);

	ParseTime (sc, min, max);

	if (framenum == picnum || !picnum.Exists() || !framenum.Exists())
	{
		return NULL;		// Animation is only one frame or does not exist
	}

	if (TexMan.GameTexture(framenum)->GetName().IsEmpty())
	{
		// long texture name: We cannot do ranged anims on these because they have no defined order
		sc.ScriptError ("You cannot use \"range\" for long texture names.");
	}

	if (framenum < picnum)
	{
		type = FAnimDef::ANIM_Backward;
		TexMan.GameTexture(framenum)->SetNoDecals(TexMan.GameTexture(picnum)->allowNoDecals());
		std::swap (framenum, picnum);
	}
	FAnimDef *ani = AddSimpleAnim (picnum, framenum - picnum + 1, min, max - min);
	if (ani != NULL) ani->AnimType = type;
	return ani;
}

//==========================================================================
//
// FTextureAnimator :: ParsePicAnim
//
// Parse a single frame from ANIMDEFS defined using "pic".
//
//==========================================================================

void FTextureAnimator::ParsePicAnim (FScanner &sc, FTextureID picnum, ETextureType usetype, bool missing, TArray<FAnimDef::FAnimFrame> &frames)
{
	FTextureID framenum;
	uint32_t min = 1, max = 1;

	framenum = ParseFramenum (sc, picnum, usetype, missing);
	ParseTime (sc, min, max);

	if (picnum.isValid())
	{
		FAnimDef::FAnimFrame frame;

		frame.SpeedMin = min;
		frame.SpeedRange = max - min;
		frame.FramePic = framenum;
		frames.Push (frame);
	}
}

//==========================================================================
//
// FTextureAnimator :: ParseFramenum
//
// Reads a frame's texture from ANIMDEFS. It can either be an integral
// offset from basepicnum or a specific texture name.
//
//==========================================================================

FTextureID FTextureAnimator::ParseFramenum (FScanner &sc, FTextureID basepicnum, ETextureType usetype, bool allowMissing)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	FTextureID framenum;

	sc.MustGetString ();
	if (IsNum (sc.String))
	{
		framenum = basepicnum + (atoi(sc.String) - 1);
	}
	else
	{
		framenum = TexMan.CheckForTexture (sc.String, usetype, texflags);
		if (!framenum.Exists() && !allowMissing)
		{
			sc.ScriptError ("Unknown texture %s", sc.String);
		}
	}
	return framenum;
}

//==========================================================================
//
// FTextureAnimator :: ParseTime
//
// Reads a tics or rand time definition from ANIMDEFS.
//
//==========================================================================

void FTextureAnimator::ParseTime (FScanner &sc, uint32_t &min, uint32_t &max)
{
	sc.MustGetString ();
	if (sc.Compare ("tics"))
	{
		sc.MustGetFloat ();
		min = max = uint32_t(sc.Float * 1000 / TICRATE);
	}
	else if (sc.Compare ("rand"))
	{
		sc.MustGetFloat ();
		min = uint32_t(sc.Float * 1000 / TICRATE);
		sc.MustGetFloat ();
		max = uint32_t(sc.Float * 1000 / TICRATE);
	}
	else
	{
		min = max = 1;
		sc.ScriptError ("Must specify a duration for animation frame");
	}
}

//==========================================================================
//
// FTextureAnimator :: ParseWarp
//
// Parses a warping texture definition
//
//==========================================================================

void FTextureAnimator::ParseWarp(FScanner &sc)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	bool isflat = false;
	bool type2 = sc.Compare ("warp2");	// [GRB]
	sc.MustGetString ();
	if (sc.Compare ("flat"))
	{
		isflat = true;
		sc.MustGetString ();
	}
	else if (sc.Compare ("texture"))
	{
		isflat = false;
		sc.MustGetString ();
	}
	else
	{
		sc.ScriptError (NULL);
	}
	FTextureID picnum = TexMan.CheckForTexture (sc.String, isflat ? ETextureType::Flat : ETextureType::Wall, texflags);
	if (picnum.isValid())
	{

		auto warper = TexMan.GameTexture(picnum);

		if (warper->GetName().IsEmpty())
		{
			// long texture name: We cannot do warps on these due to the way the texture manager implements warping as a texture replacement.
			sc.ScriptError ("You cannot use \"warp\" for long texture names.");
		}


		// don't warp a texture more than once
		if (!warper->isWarped())
		{
			warper->SetWarpStyle(type2 ? 2 : 1);
		}

		if (sc.CheckFloat())
		{
			warper->SetShaderSpeed(float(sc.Float));
		}

		// No decals on warping textures, by default.
		// Warping information is taken from the last warp 
		// definition for this texture.
		warper->SetNoDecals(true);
		if (sc.GetString ())
		{
			if (sc.Compare ("allowdecals"))
			{
				warper->SetNoDecals(false);
			}
			else
			{
				sc.UnGet ();
			}
		}
	}
}

//==========================================================================
//
// ParseCameraTexture
//
// Parses a camera texture definition
//
//==========================================================================

void FTextureAnimator::ParseCameraTexture(FScanner &sc)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ShortNameOnly;
	int width, height;
	double fitwidth, fitheight;
	FString picname;

	sc.MustGetString ();
	picname = sc.String;
	sc.MustGetNumber ();
	width = sc.Number;
	sc.MustGetNumber ();
	height = sc.Number;
	FTextureID picnum = TexMan.CheckForTexture (picname, ETextureType::Flat, texflags);
	auto canvas = new FCanvasTexture(width, height);
	FGameTexture *viewer = MakeGameTexture(canvas, picname, ETextureType::Wall);
	if (picnum.Exists())
	{
		auto oldtex = TexMan.GameTexture(picnum);
		fitwidth = oldtex->GetDisplayWidth ();
		fitheight = oldtex->GetDisplayHeight ();
		viewer->SetUseType(oldtex->GetUseType());
		TexMan.ReplaceTexture (picnum, viewer, true);
	}
	else
	{
		fitwidth = width;
		fitheight = height;
		// [GRB] No need for oldtex
		TexMan.AddGameTexture (viewer);
	}
	if (sc.GetString())
	{
		if (sc.Compare ("fit"))
		{
			sc.MustGetNumber ();
			fitwidth = sc.Number;
			sc.MustGetNumber ();
			fitheight = sc.Number;
		}
		else
		{
			sc.UnGet ();
		}
	}
	if (sc.GetString())
	{
		if (sc.Compare("WorldPanning"))
		{
			viewer->SetWorldPanning(true);
		}
		else
		{
			sc.UnGet();
		}
	}
	canvas->aspectRatio = (float)fitwidth / (float)fitheight;
	viewer->SetDisplaySize((float)fitwidth, (float)fitheight);
}

//==========================================================================
//
// FTextureAnimator :: FixAnimations
//
// Copy the "front sky" flag from an animated texture to the rest
// of the textures in the animation, and make every texture in an
// animation range use the same setting for bNoDecals.
//
//==========================================================================

void FTextureAnimator::FixAnimations ()
{
	unsigned int i;
	int j;

	for (i = 0; i < mAnimations.Size(); ++i)
	{
		FAnimDef *anim = mAnimations[i];
		if (!anim->bDiscrete)
		{
			bool nodecals;
			bool noremap = false;
			const char *name;

			name = TexMan.GameTexture(anim->BasePic)->GetName();
			nodecals = TexMan.GameTexture(anim->BasePic)->allowNoDecals();
			for (j = 0; j < anim->NumFrames; ++j)
			{
				auto tex = TexMan.GameTexture(anim->BasePic + j);
				tex->SetNoDecals(nodecals);
			}
		}
	}
}

//==========================================================================
//
// ParseAnimatedDoor
//
// Parses an animated door definition
//
//==========================================================================

void FTextureAnimator::ParseAnimatedDoor(FScanner &sc)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	FDoorAnimation anim;
	TArray<FTextureID> frames;
	bool error = false;
	FTextureID v;

	sc.MustGetString();
	anim.BaseTexture = TexMan.CheckForTexture (sc.String, ETextureType::Wall, texflags);
	anim.OpenSound = anim.CloseSound = NAME_None;

	if (!anim.BaseTexture.Exists())
	{
		error = true;
	}
	else
	{
		TexMan.GameTexture(anim.BaseTexture)->SetNoDecals(true);
	}
	while (sc.GetString())
	{
		if (sc.Compare ("opensound"))
		{
			sc.MustGetString ();
			anim.OpenSound = sc.String;
		}
		else if (sc.Compare ("closesound"))
		{
			sc.MustGetString ();
			anim.CloseSound = sc.String;
		}
		else if (sc.Compare ("pic"))
		{
			sc.MustGetString ();
			if (IsNum (sc.String))
			{
				v = anim.BaseTexture + (atoi(sc.String) - 1);
			}
			else
			{
				v = TexMan.CheckForTexture (sc.String, ETextureType::Wall, texflags);
				if (!v.Exists() && anim.BaseTexture.Exists() && !error)
				{
					sc.ScriptError ("Unknown texture %s", sc.String);
				}
			}
			frames.Push(v);
		}
		else if (sc.Compare("allowdecals"))
		{
			if (anim.BaseTexture.Exists()) TexMan.GameTexture(anim.BaseTexture)->SetNoDecals(false);
		}
		else
		{
			sc.UnGet ();
			break;
		}
	}
	if (!error)
	{
		anim.TextureFrames = new FTextureID[frames.Size()];
		memcpy (anim.TextureFrames, &frames[0], sizeof(FTextureID) * frames.Size());
		anim.NumTextureFrames = frames.Size();
		mAnimatedDoors.Push (anim);
	}
}

//==========================================================================
//
// Return index into "DoorAnimations" array for which door type to use
//
//==========================================================================

FDoorAnimation *FTextureAnimator::FindAnimatedDoor (FTextureID picnum)
{
	unsigned int i;

	for (i = 0; i < mAnimatedDoors.Size(); ++i)
	{
		if (picnum == mAnimatedDoors[i].BaseTexture)
			return &mAnimatedDoors[i];
	}

	return NULL;
}

//==========================================================================
//
// FAnimDef :: SetSwitchTime
//
// Determines when to switch to the next frame.
//
//==========================================================================

void FAnimDef::SetSwitchTime (uint64_t mstime)
{
	int speedframe = bDiscrete ? CurFrame : 0;

	SwitchTime = mstime + Frames[speedframe].SpeedMin;
	if (Frames[speedframe].SpeedRange != 0)
	{
		SwitchTime += pr_animatepictures(Frames[speedframe].SpeedRange);
	}
}


//==========================================================================
//
// FTextureAnimator :: UpdateAnimations
//
// Updates texture translations for each animation and scrolls the skies.
//
//==========================================================================

void FTextureAnimator::UpdateAnimations (uint64_t mstime)
{
	for (unsigned int j = 0; j < mAnimations.Size(); ++j)
	{
		FAnimDef *anim = mAnimations[j];

		// If this is the first time through R_UpdateAnimations, just
		// initialize the anim's switch time without actually animating.
		if (anim->SwitchTime == 0)
		{
			anim->SetSwitchTime (mstime);
		}
		else while (anim->SwitchTime <= mstime)
		{ // Multiple frames may have passed since the last time calling
		  // R_UpdateAnimations, so be sure to loop through them all.

			switch (anim->AnimType)
			{
			default:
			case FAnimDef::ANIM_Forward:
				anim->CurFrame = (anim->CurFrame + 1) % anim->NumFrames;
				break;

			case FAnimDef::ANIM_Backward:
				if (anim->CurFrame == 0)
				{
					anim->CurFrame = anim->NumFrames - 1;
				}
				else
				{
					anim->CurFrame -= 1;
				}
				break;

			case FAnimDef::ANIM_Random:
				// select a random frame other than the current one
				if (anim->NumFrames > 1)
				{
					uint16_t rndFrame = (uint16_t)pr_animatepictures(anim->NumFrames - 1);
					if (rndFrame >= anim->CurFrame) rndFrame++;
					anim->CurFrame = rndFrame;
				}
				break;

			case FAnimDef::ANIM_OscillateUp:
				anim->CurFrame = anim->CurFrame + 1;
				if (anim->CurFrame >= anim->NumFrames - 1)
				{
					anim->AnimType = FAnimDef::ANIM_OscillateDown;
				}
				break;

			case FAnimDef::ANIM_OscillateDown:
				anim->CurFrame = anim->CurFrame - 1;
				if (anim->CurFrame == 0)
				{
					anim->AnimType = FAnimDef::ANIM_OscillateUp;
				}
				break;
			}
			anim->SetSwitchTime (mstime);
		}

		if (anim->bDiscrete)
		{
			TexMan.SetTranslation (anim->BasePic, anim->Frames[anim->CurFrame].FramePic);
		}
		else
		{
			for (unsigned int i = 0; i < anim->NumFrames; i++)
			{
				TexMan.SetTranslation (anim->BasePic + i, anim->BasePic + (i + anim->CurFrame) % anim->NumFrames);
			}
		}
	}
}

//==========================================================================
//
// operator<<
//
//==========================================================================

template<> FSerializer &Serialize(FSerializer &arc, const char *key, FDoorAnimation *&p, FDoorAnimation **def)
{
	FTextureID tex = p? p->BaseTexture : FNullTextureID();
	Serialize(arc, key, tex, def ? &(*def)->BaseTexture : nullptr);
	if (arc.isReading())
	{
		p = TexAnim.FindAnimatedDoor(tex);
	}
	return arc;
}


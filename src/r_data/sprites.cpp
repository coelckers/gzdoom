//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2005-2016 Christoph Oelckers
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


#include "doomtype.h"
#include "filesystem.h"

#include "s_sound.h"
#include "d_player.h"
#include "d_netinf.h"
#include "gi.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "r_data/sprites.h"
#include "voxels.h"
#include "vm.h"
#include "texturemanager.h"

void InitModels();
void R_InitVoxels();

// variables used to look up
//	and range check thing_t sprites patches
TArray<spritedef_t> sprites;
TArray<spriteframe_t> SpriteFrames;
uint32_t			NumStdSprites;		// The first x sprites that don't belong to skins.

struct spriteframewithrotate : public spriteframe_t
{
	int rotate;
};


// [RH] skin globals
TArray<FPlayerSkin> Skins;
uint8_t			OtherGameSkinRemap[256];
PalEntry		OtherGameSkinPalette[256];


//===========================================================================
//
//  Gets the texture index for a sprite frame
//
//===========================================================================

FTextureID spritedef_t::GetSpriteFrame(int frame, int rot, DAngle ang, bool *mirror, bool flipagain)
{
	if ((unsigned)frame >= numframes)
	{
		// If there are no frames at all for this sprite, don't draw it.
		return FNullTextureID();
	}
	else
	{
		// choose a different rotation based on player view
		spriteframe_t *sprframe = &SpriteFrames[spriteframes + frame];
		if (rot == -1)
		{
			if ((sprframe->Texture[0] == sprframe->Texture[1]) && flipagain)
			{
				rot = (360.0 - ang + 45.0 / 2 * 9).BAMs() >> 28;
			}
			else if (sprframe->Texture[0] == sprframe->Texture[1])
			{
				rot = (ang + 45.0 / 2 * 9).BAMs() >> 28;
			}
			else if (flipagain)
			{
				rot = (360.0 - ang + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
			}
			else
			{
				rot = (ang + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
			}
		}
		if (mirror) *mirror = !!(sprframe->Flip&(1 << rot));
		return sprframe->Texture[rot];
	}
}


//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
// [RH] Removed checks for coexistance of rotation 0 with other
//		rotations and made it look more like BOOM's version.
//
static bool R_InstallSpriteLump (FTextureID lump, unsigned frame, char rot, bool flipped, spriteframewithrotate *sprtemp, int &maxframe)
{
	unsigned rotation;

	if (rot >= '0' && rot <= '9')
	{
		rotation = rot - '0';
	}
	else if (rot >= 'A')
	{
		rotation = rot - 'A' + 10;
	}
	else
	{
		rotation = 17;
	}

	if (frame >= MAX_SPRITE_FRAMES || rotation > 16)
	{
		Printf (TEXTCOLOR_RED "R_InstallSpriteLump: Bad frame characters in lump %s\n", TexMan.GetGameTexture(lump)->GetName().GetChars());
		return false;
	}

	if ((int)frame > maxframe)
		maxframe = frame;

	if (rotation == 0)
	{
		// the lump should be used for all rotations
        // false=0, true=1, but array initialised to -1
        // allows doom to have a "no value set yet" boolean value!
		int r;

		for (r = 14; r >= 0; r -= 2)
		{
			if (!sprtemp[frame].Texture[r].isValid())
			{
				sprtemp[frame].Texture[r] = lump;
				if (flipped)
				{
					sprtemp[frame].Flip |= 1 << r;
				}
				sprtemp[frame].rotate = false;
			}
		}
	}
	else
	{
		if (rotation <= 8)
		{
			rotation = (rotation - 1) * 2;
		}
		else
		{
			rotation = (rotation - 9) * 2 + 1;
		}

		if (!sprtemp[frame].Texture[rotation].isValid())
		{
			// the lump is only used for one rotation
			sprtemp[frame].Texture[rotation] = lump;
			if (flipped)
			{
				sprtemp[frame].Flip |= 1 << rotation;
			}
			sprtemp[frame].rotate = true;
		}
	}
	return true;
}


// [RH] Seperated out of R_InitSpriteDefs()
void R_InstallSprite (int num, spriteframewithrotate *sprtemp, int &maxframe)
{
	int frame;
	int framestart;
	int rot;
//	int undefinedFix;

	if (maxframe == -1)
	{
		sprites[num].numframes = 0;
		return;
	}

	maxframe++;

	// [RH] If any frames are undefined, but there are some defined frames, map
	// them to the first defined frame. This is a fix for Doom Raider, which actually
	// worked with ZDoom 2.0.47, because of a bug here. It does not define frames A,
	// B, or C for the sprite PSBG, but because I had sprtemp[].rotate defined as a
	// bool, this code never detected that it was not actually present. After switching
	// to the unified texture system, this caused it to crash while loading the wad.

// [RH] Let undefined frames actually be blank because LWM uses this in at least
// one of her wads.
//	for (frame = 0; frame < maxframe && sprtemp[frame].rotate == -1; ++frame)
//	{ }
//
//	undefinedFix = frame;

	for (frame = 0; frame < maxframe; ++frame)
	{
		switch (sprtemp[frame].rotate)
		{
		case -1:
			// no rotations were found for that frame at all
			//I_FatalError ("R_InstallSprite: No patches found for %s frame %c", sprites[num].name, frame+'A');
			break;
			
		case 0:
			// only the first rotation is needed
			for (rot = 1; rot < 16; ++rot)
			{
				sprtemp[frame].Texture[rot] = sprtemp[frame].Texture[0];
			}
			// If the frame is flipped, they all should be
			if (sprtemp[frame].Flip & 1)
			{
				sprtemp[frame].Flip = 0xFFFF;
			}
			break;
					
		case 1:
			// must have all 8 frame pairs
			for (rot = 0; rot < 8; ++rot)
			{
				if (!sprtemp[frame].Texture[rot*2+1].isValid())
				{
					sprtemp[frame].Texture[rot*2+1] = sprtemp[frame].Texture[rot*2];
					if (sprtemp[frame].Flip & (1 << (rot*2)))
					{
						sprtemp[frame].Flip |= 1 << (rot*2+1);
					}
				}
				if (!sprtemp[frame].Texture[rot*2].isValid())
				{
					sprtemp[frame].Texture[rot*2] = sprtemp[frame].Texture[rot*2+1];
					if (sprtemp[frame].Flip & (1 << (rot*2+1)))
					{
						sprtemp[frame].Flip |= 1 << (rot*2);
					}
				}

			}
			for (rot = 0; rot < 16; ++rot)
			{
				if (!sprtemp[frame].Texture[rot].isValid())
					I_FatalError ("R_InstallSprite: Sprite %s frame %c is missing rotations",
									sprites[num].name, frame+'A');
			}
			break;
		}
	}

	for (frame = 0; frame < maxframe; ++frame)
	{
		if (sprtemp[frame].rotate == -1)
		{
			memset (&sprtemp[frame].Texture, 0, sizeof(sprtemp[0].Texture));
			sprtemp[frame].Flip = 0;
			sprtemp[frame].rotate = 0;
		}
	}
	
	// allocate space for the frames present and copy sprtemp to it
	sprites[num].numframes = maxframe;
	sprites[num].spriteframes = uint16_t(framestart = SpriteFrames.Reserve (maxframe));
	for (frame = 0; frame < maxframe; ++frame)
	{
		memcpy (SpriteFrames[framestart+frame].Texture, sprtemp[frame].Texture, sizeof(sprtemp[frame].Texture));
		SpriteFrames[framestart+frame].Flip = sprtemp[frame].Flip;
		SpriteFrames[framestart+frame].Voxel = sprtemp[frame].Voxel;
	}

	// Let the textures know about the rotations
	for (frame = 0; frame < maxframe; ++frame)
	{
		if (sprtemp[frame].rotate == 1)
		{
			for (int rot = 0; rot < 16; ++rot)
			{
				TexMan.GetGameTexture(sprtemp[frame].Texture[rot])->SetRotations(framestart + frame);
			}
		}
	}
}


//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
//	(4 chars exactly) to be used.
// Builds the sprite rotation matrices to account
//	for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant. 
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//	a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//	letter/number appended.
// The rotation character can be 0 to signify no rotations.
//
#define TEX_DWNAME(tex) MAKE_ID(tex->GetName()[0], tex->GetName()[1], tex->GetName()[2], tex->GetName()[3])

void R_InitSpriteDefs () 
{
	struct Hasher
	{
		int Head, Next;
	};
	struct VHasher
	{
		int Head, Next, Name, Spin;
		char Frame;
	};
	unsigned int i, j, smax, vmax;
	uint32_t intname;

	spriteframewithrotate sprtemp[MAX_SPRITE_FRAMES];

	// Create a hash table to speed up the process
	smax = TexMan.NumTextures();
	TArray<Hasher> hashes(smax, true);
	memset(hashes.Data(), -1, sizeof(Hasher)*smax);
	for (i = 0; i < smax; ++i)
	{
		auto tex = TexMan.GameByIndex(i);
		if (tex->GetUseType() == ETextureType::Sprite && strlen(tex->GetName()) >= 6)
		{
			size_t bucket = TEX_DWNAME(tex) % smax;
			hashes[i].Next = hashes[bucket].Head;
			hashes[bucket].Head = i;
		}
	}

	// Repeat, for voxels
	vmax = fileSystem.GetNumEntries();
	TArray<VHasher> vhashes(vmax, true);
	memset(vhashes.Data(), -1, sizeof(VHasher)*vmax);
	for (i = 0; i < vmax; ++i)
	{
		if (fileSystem.GetFileNamespace(i) == ns_voxels)
		{
			char name[9];
			size_t namelen;
			int spin;
			int sign;

			fileSystem.GetFileShortName(name, i);
			name[8] = 0;
			namelen = strlen(name);
			if (namelen < 4)
			{ // name is too short
				continue;
			}
			if (name[4] != '\0' && name[4] != ' ' && (name[4] < 'A' || name[4] >= 'A' + MAX_SPRITE_FRAMES))
			{ // frame char is invalid
				continue;
			}
			spin = 0;
			sign = 2;	// 2 to convert from deg/halfsec to deg/sec
			j = 5;
			if (j < namelen && name[j] == '-')
			{ // a minus sign is okay, but only before any digits
				j++;
				sign = -2;
			}
			for (; j < namelen; ++j)
			{ // the remainder to the end of the name must be digits
				if (name[j] >= '0' && name[j] <= '9')
				{
					spin = spin * 10 + name[j] - '0';
				}
				else
				{
					break;
				}
			}
			if (j < namelen)
			{ // the spin part is invalid
				continue;
			}
			memcpy(&vhashes[i].Name, name, 4);
			vhashes[i].Frame = name[4];
			vhashes[i].Spin = spin * sign;
			size_t bucket = vhashes[i].Name % vmax;
			vhashes[i].Next = vhashes[bucket].Head;
			vhashes[bucket].Head = i;
		}
	}

	// scan all the lump names for each of the names, noting the highest frame letter.
	for (i = 0; i < sprites.Size(); ++i)
	{
		memset (sprtemp, 0xFF, sizeof(sprtemp));
		for (j = 0; j < MAX_SPRITE_FRAMES; ++j)
		{
			sprtemp[j].Flip = 0;
			sprtemp[j].Voxel = NULL;
		}
				
		int maxframe = -1;
		intname = sprites[i].dwName;

		// scan the lumps, filling in the frames for whatever is found
		int hash = hashes[intname % smax].Head;
		while (hash != -1)
		{
			auto tex = TexMan.GetGameTexture(hash);
			if (TEX_DWNAME(tex) == intname)
			{
				bool res = R_InstallSpriteLump (FTextureID(hash), tex->GetName()[4] - 'A', tex->GetName()[5], false, sprtemp, maxframe);

				if (tex->GetName()[6] && res)
					R_InstallSpriteLump (FTextureID(hash), tex->GetName()[6] - 'A', tex->GetName()[7], true, sprtemp, maxframe);
			}
			hash = hashes[hash].Next;
		}

		// repeat, for voxels
		hash = vhashes[intname % vmax].Head;
		while (hash != -1)
		{
			VHasher *vh = &vhashes[hash];
			if (vh->Name == (int)intname)
			{
				FVoxelDef *voxdef = R_LoadVoxelDef(hash, vh->Spin);
				if (voxdef != NULL)
				{
					if (vh->Frame == ' ' || vh->Frame == '\0')
					{ // voxel applies to every sprite frame
						for (j = 0; j < MAX_SPRITE_FRAMES; ++j)
						{
							if (sprtemp[j].Voxel == NULL)
							{
								sprtemp[j].Voxel = voxdef;
							}
						}
						maxframe = MAX_SPRITE_FRAMES-1;
					}
					else
					{ // voxel applies to a specific frame
						j = vh->Frame - 'A';
						sprtemp[j].Voxel = voxdef;
						maxframe = MAX<int>(maxframe, j);
					}
				}
			}
			hash = vh->Next;
		}
		
		R_InstallSprite ((int)i, sprtemp, maxframe);
	}
}

//==========================================================================
//
// R_ExtendSpriteFrames
//
// Extends a sprite so that it can hold the desired frame.
//
//==========================================================================

static void R_ExtendSpriteFrames(spritedef_t &spr, int frame)
{
	unsigned int i, newstart;

	if (spr.numframes >= ++frame)
	{ // The sprite already has enough frames, so do nothing.
		return;
	}

	if (spr.numframes == 0 || (spr.spriteframes + spr.numframes == SpriteFrames.Size()))
	{ // Sprite's frames are at the end of the array, or it has no frames
	  // at all, so we can tack the new frames directly on to the end
	  // of the SpriteFrames array.
		newstart = SpriteFrames.Reserve(frame - spr.numframes);
		if (spr.numframes == 0)
		{
			spr.spriteframes = uint16_t(newstart);
		}
	}
	else
	{ // We need to allocate space for all the sprite's frames and copy
	  // the existing ones over to the new space. The old space will be
	  // lost.
		newstart = SpriteFrames.Reserve(frame);
		for (i = 0; i < spr.numframes; ++i)
		{
			SpriteFrames[newstart + i] = SpriteFrames[spr.spriteframes + i];
		}
		spr.spriteframes = uint16_t(newstart);
		newstart += i;
	}
	// Initialize all new frames to 0.
	memset(&SpriteFrames[newstart], 0, sizeof(spriteframe_t)*(frame - spr.numframes));
	spr.numframes = frame;
}

//==========================================================================
//
// VOX_AddVoxel
//
// Sets a voxel for a single sprite frame.
//
//==========================================================================

void VOX_AddVoxel(int sprnum, int frame, FVoxelDef *def)
{
	R_ExtendSpriteFrames(sprites[sprnum], frame);
	SpriteFrames[sprites[sprnum].spriteframes + frame].Voxel = def;
}



// [RH]
// R_InitSkins
// Reads in everything applicable to a skin. The skins should have already
// been counted and had their identifiers assigned to namespaces.
//
#define NUMSKINSOUNDS 17
static const char *skinsoundnames[NUMSKINSOUNDS][2] =
{ // The *painXXX sounds must be the first four
	{ "dsplpain",	"*pain100" },
	{ "dsplpain",	"*pain75" },
	{ "dsplpain",	"*pain50" },
	{ "dsplpain",	"*pain25" },
	{ "dsplpain",	"*poison" },

	{ "dsoof",		"*grunt" },
	{ "dsoof",		"*land" },

	{ "dspldeth",	"*death" },
	{ "dspldeth",	"*wimpydeath" },

	{ "dspdiehi",	"*xdeath" },
	{ "dspdiehi",	"*crazydeath" },

	{ "dsnoway",	"*usefail" },
	{ "dsnoway",	"*puzzfail" },

	{ "dsslop",		"*gibbed" },
	{ "dsslop",		"*splat" },

	{ "dspunch",	"*fist" },
	{ "dsjump",		"*jump" }
};

/*
static int skinsorter (const void *a, const void *b)
{
	return stricmp (((FPlayerSkin *)a)->name, ((FPlayerSkin *)b)->name);
}
*/

void R_InitSkins (void)
{
	FSoundID playersoundrefs[NUMSKINSOUNDS];
	spritedef_t temp;
	int sndlumps[NUMSKINSOUNDS];
	char key[65];
	uint32_t intname, crouchname;
	unsigned i;
	int j, k, base;
	int lastlump;
	int aliasid;
	bool remove;
	PClassActor *basetype, *transtype;

	key[sizeof(key)-1] = 0;
	i = PlayerClasses.Size () - 1;
	lastlump = 0;

	for (j = 0; j < NUMSKINSOUNDS; ++j)
	{
		playersoundrefs[j] = skinsoundnames[j][1];
	}

	while ((base = fileSystem.FindLump ("S_SKIN", &lastlump, true)) != -1)
	{
		// The player sprite has 23 frames. This means that the S_SKIN
		// marker needs a minimum of 23 lumps after it.
		if (base >= fileSystem.GetNumEntries() - 23 || base == -1)
			continue;

		i++;
		for (j = 0; j < NUMSKINSOUNDS; j++)
			sndlumps[j] = -1;
		Skins[i].namespc = fileSystem.GetFileNamespace (base);

		FScanner sc(base);
		intname = 0;
		crouchname = 0;

		remove = false;
		basetype = NULL;
		transtype = NULL;

		// Data is stored as "key = data".
		while (sc.GetString ())
		{
			strncpy (key, sc.String, sizeof(key)-1);
			if (!sc.GetString() || sc.String[0] != '=')
			{
				Printf (PRINT_DEFAULT, PRINTF_BOLD, "Bad format for skin %d: %s\n", (int)i, key);
				break;
			}
			sc.GetString ();
			if (0 == stricmp (key, "name"))
			{
				Skins[i].Name = sc.String;
				for (j = 0; (unsigned)j < i; j++)
				{
					if (Skins[i].Name.CompareNoCase(Skins[j].Name) == 0)
					{
						Skins[i].Name.Format("skin%u", i);
						Printf (PRINT_DEFAULT, PRINTF_BOLD, "Skin %s duplicated as %s\n", Skins[j].Name.GetChars(), Skins[i].Name.GetChars());
						break;
					}
				}
			}
			else if (0 == stricmp (key, "sprite"))
			{
				for (j = 3; j >= 0; j--)
					sc.String[j] = toupper (sc.String[j]);
				intname = *((uint32_t *)sc.String);
			}
			else if (0 == stricmp (key, "crouchsprite"))
			{
				for (j = 3; j >= 0; j--)
					sc.String[j] = toupper (sc.String[j]);
				crouchname = *((uint32_t *)sc.String);
			}
			else if (0 == stricmp (key, "face"))
			{
				Skins[i].Face = FString(sc.String, 3);
			}
			else if (0 == stricmp (key, "gender"))
			{
				Skins[i].gender = D_GenderToInt (sc.String);
			}
			else if (0 == stricmp (key, "scale"))
			{
				Skins[i].Scale.X = clamp(atof (sc.String), 1./65536, 256.);
				Skins[i].Scale.Y = Skins[i].Scale.X;
			}
			else if (0 == stricmp (key, "game"))
			{
				if (gameinfo.gametype == GAME_Heretic)
					basetype = PClass::FindActor(NAME_HereticPlayer);
				else if (gameinfo.gametype == GAME_Strife)
					basetype = PClass::FindActor(NAME_StrifePlayer);
				else
					basetype = PClass::FindActor(NAME_DoomPlayer);

				transtype = basetype;

				if (stricmp (sc.String, "heretic") == 0)
				{
					if (gameinfo.gametype & GAME_DoomChex)
					{
						transtype = PClass::FindActor(NAME_HereticPlayer);
						Skins[i].othergame = true;
					}
					else if (gameinfo.gametype != GAME_Heretic)
					{
						remove = true;
					}
				}
				else if (stricmp (sc.String, "strife") == 0)
				{
					if (gameinfo.gametype != GAME_Strife)
					{
						remove = true;
					}
				}
				else
				{
					if (gameinfo.gametype == GAME_Heretic)
					{
						transtype = PClass::FindActor(NAME_DoomPlayer);
						Skins[i].othergame = true;
					}
					else if (!(gameinfo.gametype & GAME_DoomChex))
					{
						remove = true;
					}
				}

				if (remove)
					break;
			}
			else if (0 == stricmp (key, "class"))
			{ // [GRB] Define the skin for a specific player class
				int pclass = D_PlayerClassToInt (sc.String);

				if (pclass < 0)
				{
					remove = true;
					break;
				}

				basetype = transtype = PlayerClasses[pclass].Type;
			}
			else if (key[0] == '*')
			{ // Player sound replacment (ZDoom extension)
				int lump = fileSystem.CheckNumForName (sc.String, Skins[i].namespc);
				if (lump == -1)
				{
					lump = fileSystem.CheckNumForFullName (sc.String, true, ns_sounds);
				}
				if (lump != -1)
				{
					if (stricmp (key, "*pain") == 0)
					{ // Replace all pain sounds in one go
						aliasid = S_AddPlayerSound (Skins[i].Name, Skins[i].gender,
							playersoundrefs[0], lump, true);
						for (int l = 3; l > 0; --l)
						{
							S_AddPlayerSoundExisting (Skins[i].Name, Skins[i].gender,
								playersoundrefs[l], aliasid, true);
						}
					}
					else
					{
						int sndref = soundEngine->FindSoundNoHash (key);
						if (sndref != 0)
						{
							S_AddPlayerSound (Skins[i].Name, Skins[i].gender, sndref, lump, true);
						}
					}
				}
			}
			else
			{
				for (j = 0; j < NUMSKINSOUNDS; j++)
				{
					if (stricmp (key, skinsoundnames[j][0]) == 0)
					{
						sndlumps[j] = fileSystem.CheckNumForName (sc.String, Skins[i].namespc);
						if (sndlumps[j] == -1)
						{ // Replacement not found, try finding it in the global namespace
							sndlumps[j] = fileSystem.CheckNumForFullName (sc.String, true, ns_sounds);
						}
					}
				}
				//if (j == 8)
				//	Printf ("Funny info for skin %i: %s = %s\n", i, key, sc.String);
			}
		}

		// [GRB] Assume Doom skin by default
		if (!remove && basetype == NULL)
		{
			if (gameinfo.gametype & GAME_DoomChex)
			{
				basetype = transtype = PClass::FindActor(NAME_DoomPlayer);
			}
			else if (gameinfo.gametype == GAME_Heretic)
			{
				basetype = PClass::FindActor(NAME_HereticPlayer);
				transtype = PClass::FindActor(NAME_DoomPlayer);
				Skins[i].othergame = true;
			}
			else
			{
				remove = true;
			}
		}

		if (!remove)
		{
			auto transdef = GetDefaultByType(transtype);
			auto basedef = GetDefaultByType(basetype);

			Skins[i].range0start = transdef->IntVar(NAME_ColorRangeStart);
			Skins[i].range0end = transdef->IntVar(NAME_ColorRangeEnd);

			remove = true;
			for (j = 0; j < (int)PlayerClasses.Size (); j++)
			{
				auto type = PlayerClasses[j].Type;
				auto type_def = GetDefaultByType(type);

				if (type->IsDescendantOf (basetype) &&
					GetDefaultByType(type)->SpawnState->sprite == GetDefaultByType(basetype)->SpawnState->sprite &&
					type_def->IntVar(NAME_ColorRangeStart) == basedef->IntVar(NAME_ColorRangeStart) &&
					type_def->IntVar(NAME_ColorRangeEnd) == basedef->IntVar(NAME_ColorRangeEnd))
				{
					PlayerClasses[j].Skins.Push ((int)i);
					remove = false;
				}
			}
		}

		if (!remove)
		{
			if (Skins[i].Name.IsEmpty())
				Skins[i].Name.Format("skin%u", i);

			// Now collect the sprite frames for this skin. If the sprite name was not
			// specified, use whatever immediately follows the specifier lump.
			if (intname == 0)
			{
				char name[9];
				fileSystem.GetFileShortName (name, base+1);
				memcpy(&intname, name, 4);
			}

			int basens = fileSystem.GetFileNamespace(base);

			for(int spr = 0; spr<2; spr++)
			{
				spriteframewithrotate sprtemp[MAX_SPRITE_FRAMES];
				memset (sprtemp, 0xFFFF, sizeof(sprtemp));
				for (k = 0; k < MAX_SPRITE_FRAMES; ++k)
				{
					sprtemp[k].Flip = 0;
					sprtemp[k].Voxel = NULL;
				}
				int maxframe = -1;

				if (spr == 1)
				{
					if (crouchname !=0 && crouchname != intname)
					{
						intname = crouchname;
					}
					else
					{
						Skins[i].crouchsprite = -1;
						break;
					}
				}

				for (k = base + 1; fileSystem.GetFileNamespace(k) == basens; k++)
				{
					char lname[9];
					uint32_t lnameint;
					fileSystem.GetFileShortName (lname, k);
					memcpy(&lnameint, lname, 4);
					if (lnameint == intname)
					{
						FTextureID picnum = TexMan.CreateTexture(k, ETextureType::SkinSprite);
						bool res = R_InstallSpriteLump (picnum, lname[4] - 'A', lname[5], false, sprtemp, maxframe);

						if (lname[6] && res)
							R_InstallSpriteLump (picnum, lname[6] - 'A', lname[7], true, sprtemp, maxframe);
					}
				}

				if (spr == 0 && maxframe <= 0)
				{
					Printf (PRINT_DEFAULT, PRINTF_BOLD, "Skin %s (#%u) has no frames. Removing.\n", Skins[i].Name.GetChars(), i);
					remove = true;
					break;
				}

				fileSystem.GetFileShortName (temp.name, base+1);
				temp.name[4] = 0;
				int sprno = (int)sprites.Push (temp);
				if (spr==0)	Skins[i].sprite = sprno;
				else Skins[i].crouchsprite = sprno;
				R_InstallSprite (sprno, sprtemp, maxframe);
			}
		}

		if (remove)
		{
			if (i < Skins.Size() - 1)
			{
				Skins.Delete(i);
			}
			i--;
			continue;
		}

		// Register any sounds this skin provides
		aliasid = 0;
		for (j = 0; j < NUMSKINSOUNDS; j++)
		{
			if (sndlumps[j] != -1)
			{
				if (j == 0 || sndlumps[j] != sndlumps[j-1])
				{
					aliasid = S_AddPlayerSound (Skins[i].Name, Skins[i].gender,
						playersoundrefs[j], sndlumps[j], true);
				}
				else
				{
					S_AddPlayerSoundExisting (Skins[i].Name, Skins[i].gender,
						playersoundrefs[j], aliasid, true);
				}
			}
		}

		// Make sure face prefix is a full 3 chars
		if (Skins[i].Face.Len() < 3)
		{
			Skins[i].Face = "";
		}
	}

	if (Skins.Size() > PlayerClasses.Size ())
	{ // The sound table may have changed, so rehash it.
		soundEngine->HashSounds ();
		S_ShrinkPlayerSoundLists ();
	}
}

// [RH] Find a skin by name
int R_FindSkin (const char *name, int pclass)
{
	if (stricmp ("base", name) == 0)
	{
		return pclass;
	}

	for (unsigned i = PlayerClasses.Size(); i < Skins.Size(); i++)
	{
		if (Skins[i].Name.CompareNoCase(name) == 0)
		{
			if (PlayerClasses[pclass].CheckSkin (i))
				return i;
			else
				return pclass;
		}
	}
	return pclass;
}

// [RH] List the names of all installed skins
CCMD (skins)
{
	int i;

	for (i = PlayerClasses.Size() - 1; i < (int)Skins.Size(); i++)
		Printf("% 3d %s\n", i - PlayerClasses.Size() + 1, Skins[i].Name.GetChars());
}


static void R_CreateSkinTranslation (const char *palname)
{
	FileData lump = fileSystem.ReadFile (palname);
	const uint8_t *otherPal = (uint8_t *)lump.GetMem();
 
	for (int i = 0; i < 256; ++i)
	{
		OtherGameSkinRemap[i] = ColorMatcher.Pick (otherPal[0], otherPal[1], otherPal[2]);
		OtherGameSkinPalette[i] = PalEntry(otherPal[0], otherPal[1], otherPal[2]);
		otherPal += 3;
	}
}


//
// R_InitSprites
// Called at program start.
//
void R_InitSprites ()
{
	int lump, lastlump;
	unsigned int i, j;
	unsigned numskins;

	// [RH] Create a standard translation to map skins between Heretic and Doom
	if (gameinfo.gametype == GAME_DoomChex)
	{
		R_CreateSkinTranslation ("SPALHTIC");
	}
	else
	{
		R_CreateSkinTranslation ("SPALDOOM");
	}

	// [RH] Count the number of skins.
	numskins = PlayerClasses.Size ();
	lastlump = 0;
	while ((lump = fileSystem.FindLump ("S_SKIN", &lastlump, true)) != -1)
	{
		numskins++;
	}

	// [RH] Do some preliminary setup
	Skins.Clear();
	Skins.Resize(numskins);

	for (i = 0; i < numskins; i++)
	{ // Assume Doom skin by default
		auto type = GetDefaultByType(PlayerClasses[0].Type);
		Skins[i].range0start = type->IntVar(NAME_ColorRangeStart);
		Skins[i].range0end = type->IntVar(NAME_ColorRangeEnd);
		Skins[i].Scale = type->Scale;
	}

	R_InitSpriteDefs ();
	R_InitVoxels();		// [RH] Parse VOXELDEF
	NumStdSprites = sprites.Size();
	R_InitSkins ();		// [RH] Finish loading skin data

	// [RH] Set up base skin
	// [GRB] Each player class has its own base skin
	for (i = 0; i < PlayerClasses.Size (); i++)
	{
		auto basetype = GetDefaultByType(PlayerClasses[i].Type);

		Skins[i].Name = "Base";
		auto face = basetype->NameVar(NAME_Face);
		Skins[i].Face = face == NAME_None? "STF" : face.GetChars();
		Skins[i].range0start = basetype->IntVar(NAME_ColorRangeStart);
		Skins[i].range0end = basetype->IntVar(NAME_ColorRangeEnd);
		Skins[i].Scale = basetype->Scale;
		Skins[i].sprite = basetype->SpawnState->sprite;
		Skins[i].namespc = ns_global;

		PlayerClasses[i].Skins.Push (i);

		if (memcmp (sprites[Skins[i].sprite].name, "PLAY", 4) == 0)
		{
			for (j = 0; j < sprites.Size (); j++)
			{
				if (memcmp (sprites[j].name, deh.PlayerSprite, 4) == 0)
				{
					Skins[i].sprite = (int)j;
					break;
				}
			}
		}
	}

	// [RH] Sort the skins, but leave base as skin 0
	//qsort (&skins[PlayerClasses.Size ()], numskins-PlayerClasses.Size (), sizeof(FPlayerSkin), skinsorter);

	InitModels();
}


DEFINE_FIELD_NAMED(FPlayerSkin, Name, SkinName);
DEFINE_FIELD(FPlayerSkin, Face);
DEFINE_FIELD(FPlayerSkin, gender);
DEFINE_FIELD(FPlayerSkin, range0start);
DEFINE_FIELD(FPlayerSkin, range0end);
DEFINE_FIELD(FPlayerSkin, othergame);
DEFINE_FIELD(FPlayerSkin, Scale);
DEFINE_FIELD(FPlayerSkin, sprite);
DEFINE_FIELD(FPlayerSkin, crouchsprite);
DEFINE_FIELD(FPlayerSkin, namespc);

/*
** textures.h
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
** Copyright 2005-2019 Christoph Oelckers
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

#ifndef __TEXTURES_H
#define __TEXTURES_H

#include "basics.h"
#include "vectors.h"
#include "colormatcher.h"
#include "renderstyle.h"
#include "textureid.h"
#include <vector>
#include "hw_texcontainer.h"
#include "floatrect.h"
#include "refcounted.h"

typedef TMap<int, bool> SpriteHits;
class FImageSource;
class FGameTexture;
class IHardwareTexture;


enum
{
	CLAMP_NONE = 0,
	CLAMP_X,
	CLAMP_Y,
	CLAMP_XY,
	CLAMP_XY_NOMIP,
	CLAMP_NOFILTER,
	CLAMP_NOFILTER_X,
	CLAMP_NOFILTER_Y,
	CLAMP_NOFILTER_XY,
	CLAMP_CAMTEX,
	NUMSAMPLERS
};

enum MaterialShaderIndex
{
	SHADER_Default,
	SHADER_Warp1,
	SHADER_Warp2,
	SHADER_Specular,
	SHADER_PBR,
	SHADER_Paletted,
	SHADER_NoTexture,
	SHADER_BasicFuzz,
	SHADER_SmoothFuzz,
	SHADER_SwirlyFuzz,
	SHADER_TranslucentFuzz,
	SHADER_JaggedFuzz,
	SHADER_NoiseFuzz,
	SHADER_SmoothNoiseFuzz,
	SHADER_SoftwareFuzz,
	FIRST_USER_SHADER
};

enum texflags
{
	// These get Or'ed into uTextureMode because it only uses its 3 lowermost bits.
	TEXF_Brightmap = 0x10000,
	TEXF_Detailmap = 0x20000,
	TEXF_Glowmap = 0x40000,
};



enum
{
	SFlag_Brightmap = 1,
	SFlag_Detailmap = 2,
	SFlag_Glowmap = 4,
};

struct UserShaderDesc
{
	FString shader;
	MaterialShaderIndex shaderType;
	FString defines;
	bool disablealphatest = false;
	uint8_t shaderFlags = 0;
};

extern TArray<UserShaderDesc> usershaders;

class FBitmap;
struct FRemapTable;
struct FCopyInfo;
class FScanner;

// Texture IDs
class FTextureManager;
class FTerrainTypeArray;
class IHardwareTexture;
class FMaterial;
class FMultipatchTextureBuilder;

extern int r_spriteadjustSW, r_spriteadjustHW;

enum FTextureFormat : uint32_t
{
	TEX_Pal,
	TEX_Gray,
	TEX_RGB,		// Actually ARGB

	TEX_Count
};

class ISoftwareTexture
{
public:
	virtual ~ISoftwareTexture() = default;
};

class FGLRenderState;

struct spriteframewithrotate;
class FSerializer;
namespace OpenGLRenderer
{
	class FGLRenderState;
	class FHardwareTexture;
}

union FContentIdBuilder
{
	uint64_t id;
	struct
	{
		unsigned imageID : 24;
		unsigned translation : 16;
		unsigned expand : 1;
		unsigned scaler : 4;
		unsigned scalefactor : 4;
	};
};

struct FTextureBuffer
{
	uint8_t *mBuffer = nullptr;
	int mWidth = 0;
	int mHeight = 0;
	uint64_t mContentId = 0;	// unique content identifier. (Two images created from the same image source with the same settings will return the same value.)

	FTextureBuffer() = default;

	~FTextureBuffer()
	{
		if (mBuffer) delete[] mBuffer;
	}

	FTextureBuffer(const FTextureBuffer &other) = delete;
	FTextureBuffer(FTextureBuffer &&other)
	{
		mBuffer = other.mBuffer;
		mWidth = other.mWidth;
		mHeight = other.mHeight;
		mContentId = other.mContentId;
		other.mBuffer = nullptr;
	}

	FTextureBuffer& operator=(FTextureBuffer &&other)
	{
		mBuffer = other.mBuffer;
		mWidth = other.mWidth;
		mHeight = other.mHeight;
		mContentId = other.mContentId;
		other.mBuffer = nullptr;
		return *this;
	}

};

// Base texture class
class FTexture : public RefCountedBase
{
	friend class FGameTexture;	// only for the porting work

public:
	FHardwareTextureContainer SystemTextures;
protected:
	FloatRect* areas = nullptr;
	int SourceLump;
	uint16_t Width = 0, Height = 0;

	bool Masked = false;			// Texture (might) have holes
	bool bHasCanvas = false;
	int8_t bTranslucent = -1;
	int8_t areacount = 0;			// this is capped at 4 sections.


public:

	IHardwareTexture* GetHardwareTexture(int translation, int scaleflags);
	virtual FImageSource *GetImage() const { return nullptr; }
	void CreateUpsampledTextureBuffer(FTextureBuffer &texbuffer, bool hasAlpha, bool checkonly);

	void CleanHardwareTextures()
	{
		SystemTextures.Clean();
	}

	void CleanPrecacheMarker()
	{
		SystemTextures.UnmarkAll();
	}

	void MarkForPrecache(int translation, int scaleflags)
	{
		SystemTextures.MarkForPrecache(translation, scaleflags);
	}

	void CleanUnused()
	{
		SystemTextures.CleanUnused();
	}

	int GetWidth() { return Width; }
	int GetHeight() { return Height; }
	
	bool isHardwareCanvas() const { return bHasCanvas; }	// There's two here so that this can deal with software canvases in the hardware renderer later.
	bool isCanvas() const { return bHasCanvas; }
	
	int GetSourceLump() { return SourceLump; }	// needed by the scripted GetName method.
	void SetSourceLump(int sl) { SourceLump  = sl; }
	bool FindHoles(const unsigned char * buffer, int w, int h);

	void CopySize(FTexture* BaseTexture)
	{
		Width = BaseTexture->GetWidth();
		Height = BaseTexture->GetHeight();
	}

	// This is only used for the null texture and for Heretic's skies.
	void SetSize(int w, int h)
	{
		Width = w;
		Height = h;
	}

	bool TrimBorders(uint16_t* rect);
	int GetAreas(FloatRect** pAreas) const;

	// Returns the whole texture, stored in column-major order
	virtual TArray<uint8_t> Get8BitPixels(bool alphatex);
	virtual FBitmap GetBgraBitmap(const PalEntry *remap, int *trans = nullptr);

	static bool SmoothEdges(unsigned char * buffer,int w, int h);


	virtual void ResolvePatches() {}


	FTexture (int lumpnum = -1);

public:
	FTextureBuffer CreateTexBuffer(int translation, int flags = 0);
	virtual bool DetermineTranslucency();
	bool GetTranslucency()
	{
		return bTranslucent != -1 ? bTranslucent : DetermineTranslucency();
	}

public:

	void CheckTrans(unsigned char * buffer, int size, int trans);
	bool ProcessData(unsigned char * buffer, int w, int h, bool ispatch);
	int CheckRealHeight();

	friend class FTextureManager;
};


// A texture that can be drawn to.

class FCanvasTexture : public FTexture
{
public:
	FCanvasTexture(int width, int height)
	{
		Width = width;
		Height = height;

		bHasCanvas = true;
		aspectRatio = (float)width / height;
	}

	void NeedUpdate() { bNeedsUpdate = true; }
	void SetUpdated(bool rendertype) { bNeedsUpdate = false; bFirstUpdate = false; bLastUpdateType = rendertype; }

	void SetAspectRatio(double aspectScale, bool useTextureRatio) { aspectRatio = aspectScale * (useTextureRatio? ((float)Width / Height) : 1); }

protected:

	bool bLastUpdateType = false;
	bool bNeedsUpdate = true;
public:
	bool bFirstUpdate = true;
	float aspectRatio;

	friend struct FCanvasTextureInfo;
};


// A wrapper around a hardware texture, to allow using it in the 2D drawing interface.
class FWrapperTexture : public FTexture
{
	int Format;
public:
	FWrapperTexture(int w, int h, int bits = 1);
	IHardwareTexture *GetSystemTexture()
	{
		return SystemTextures.GetHardwareTexture(0, 0);
	}

	int GetColorFormat() const
	{
		return Format;
	}
};


class FImageTexture : public FTexture
{
	FImageSource* mImage;
	bool bNoRemap0 = false;
protected:
	void SetFromImage();
public:
	FImageTexture(FImageSource* image) noexcept;
	~FImageTexture();
	TArray<uint8_t> Get8BitPixels(bool alphatex) override;

	void SetImage(FImageSource* img)
	{
		mImage = img;
		SetFromImage();
	}
	void SetNoRemap0() { bNoRemap0 = true; }

	FImageSource* GetImage() const override { return mImage; }
	FBitmap GetBgraBitmap(const PalEntry* p, int* trans) override;
	bool DetermineTranslucency() override;

};


#include "gametexture.h"
#endif



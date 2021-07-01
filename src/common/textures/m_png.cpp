/*
** m_png.cpp
** Routines for manipulating PNG files.
**
**---------------------------------------------------------------------------
** Copyright 2002-2006 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include <algorithm>
#include <stdlib.h>
#include <zlib.h>
#include <stdint.h>
#ifdef _MSC_VER
#include <malloc.h>		// for alloca()
#endif
#if __has_include(<alloca.h>)
#include <alloca.h>
#endif

#include "basics.h"
#include "m_crc32.h"
#include "m_swap.h"
#include "c_cvars.h"
#include "m_png.h"


// MACROS ------------------------------------------------------------------

// The maximum size of an IDAT chunk ZDoom will write. This is also the
// size of the compression buffer it allocates on the stack.
#define PNG_WRITE_SIZE	32768

// Set this to 1 to use a simple heuristic to select the filter to apply
// for each row of RGB image saves. As it turns out, it seems no filtering
// is the best for Doom screenshots, no matter what the heuristic might
// determine, so that's why this is 0 here.
#define USE_FILTER_HEURISTIC 0

// TYPES -------------------------------------------------------------------

struct IHDR
{
	uint32_t		Width;
	uint32_t		Height;
	uint8_t		BitDepth;
	uint8_t		ColorType;
	uint8_t		Compression;
	uint8_t		Filter;
	uint8_t		Interlace;
};

PNGHandle::PNGHandle (FileReader &file) : bDeleteFilePtr(true), ChunkPt(0)
{
	File = std::move(file);
}

PNGHandle::~PNGHandle ()
{
	for (unsigned int i = 0; i < TextChunks.Size(); ++i)
	{
		delete[] TextChunks[i];
	}
}

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static inline void MakeChunk (void *where, uint32_t type, size_t len);
static inline void StuffPalette (const PalEntry *from, uint8_t *to);
static bool WriteIDAT (FileWriter *file, const uint8_t *data, int len);
static void UnfilterRow (int width, uint8_t *dest, uint8_t *stream, uint8_t *prev, int bpp);
static void UnpackPixels (int width, int bytesPerRow, int bitdepth, const uint8_t *rowin, uint8_t *rowout, bool grayscale);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Int, png_level, 5, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 9)
		self = 9;
}
CVAR(Float, png_gamma, 0.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// M_CreatePNG
//
// Passed a newly-created file, writes the PNG signature and IHDR, gAMA, and
// PLTE chunks. Returns true if everything went as expected.
//
//==========================================================================

bool M_CreatePNG (FileWriter *file, const uint8_t *buffer, const PalEntry *palette,
				  ESSType color_type, int width, int height, int pitch, float gamma)
{
	uint8_t work[8 +				// signature
			  12+2*4+5 +		// IHDR
			  12+4 +			// gAMA
			  12+256*3];		// PLTE
	uint32_t *const sig = (uint32_t *)&work[0];
	IHDR *const ihdr = (IHDR *)&work[8 + 8];
	uint32_t *const gama = (uint32_t *)((uint8_t *)ihdr + 2*4+5 + 12);
	uint8_t *const plte = (uint8_t *)gama + 4 + 12;
	size_t work_len;

	sig[0] = MAKE_ID(137,'P','N','G');
	sig[1] = MAKE_ID(13,10,26,10);

	ihdr->Width = BigLong(width);
	ihdr->Height = BigLong(height);
	ihdr->BitDepth = 8;
	ihdr->ColorType = color_type == SS_PAL ? 3 : 2;
	ihdr->Compression = 0;
	ihdr->Filter = 0;
	ihdr->Interlace = 0;
	MakeChunk (ihdr, MAKE_ID('I','H','D','R'), 2*4+5);

	// Assume a display exponent of 2.2 (100000/2.2 ~= 45454.5)
	*gama = BigLong (int (45454.5f * (png_gamma == 0.f ? gamma : png_gamma)));
	MakeChunk (gama, MAKE_ID('g','A','M','A'), 4);

	if (color_type == SS_PAL)
	{
		StuffPalette (palette, plte);
		MakeChunk (plte, MAKE_ID('P','L','T','E'), 256*3);
		work_len = sizeof(work);
	}
	else
	{
		work_len = sizeof(work) - (12+256*3);
	}

	if (file->Write (work, work_len) != work_len)
		return false;

	return M_SaveBitmap (buffer, color_type, width, height, pitch, file);
}

//==========================================================================
//
// M_CreateDummyPNG
//
// Like M_CreatePNG, but the image is always a grayscale 1x1 black square.
//
//==========================================================================

bool M_CreateDummyPNG (FileWriter *file)
{
	static const uint8_t dummyPNG[] =
	{
		137,'P','N','G',13,10,26,10,
		0,0,0,13,'I','H','D','R',
		0,0,0,1,0,0,0,1,8,0,0,0,0,0x3a,0x7e,0x9b,0x55,
		0,0,0,10,'I','D','A','T',
		104,222,99,96,0,0,0,2,0,1,0x9f,0x65,0x0e,0x18
	};
	return file->Write (dummyPNG, sizeof(dummyPNG)) == sizeof(dummyPNG);
}


//==========================================================================
//
// M_FinishPNG
//
// Writes an IEND chunk to a PNG file. The file is left opened.
//
//==========================================================================

bool M_FinishPNG (FileWriter *file)
{
	static const uint8_t iend[12] = { 0,0,0,0,73,69,78,68,174,66,96,130 };
	return file->Write (iend, 12) == 12;
}

//==========================================================================
//
// M_AppendPNGChunk
//
// Writes a PNG-compliant chunk to the file.
//
//==========================================================================

bool M_AppendPNGChunk (FileWriter *file, uint32_t chunkID, const uint8_t *chunkData, uint32_t len)
{
	uint32_t head[2] = { BigLong((unsigned int)len), chunkID };
	uint32_t crc;

	if (file->Write (head, 8) == 8 &&
		(len == 0 || file->Write (chunkData, len) == len))
	{
		crc = CalcCRC32 ((uint8_t *)&head[1], 4);
		if (len != 0)
		{
			crc = AddCRC32 (crc, chunkData, len);
		}
		crc = BigLong((unsigned int)crc);
		return file->Write (&crc, 4) == 4;
	}
	return false;
}

//==========================================================================
//
// M_AppendPNGText
//
// Appends a PNG tEXt chunk to the file
//
//==========================================================================

bool M_AppendPNGText (FileWriter *file, const char *keyword, const char *text)
{
	struct { uint32_t len, id; char key[80]; } head;
	int len = (int)strlen (text);
	int keylen = std::min ((int)strlen (keyword), 79);
	uint32_t crc;

	head.len = BigLong(len + keylen + 1);
	head.id = MAKE_ID('t','E','X','t');
	memset (&head.key, 0, sizeof(head.key));
	strncpy (head.key, keyword, keylen);
	head.key[keylen] = 0;

	if ((int)file->Write (&head, keylen + 9) == keylen + 9 &&
		(int)file->Write (text, len) == len)
	{
		crc = CalcCRC32 ((uint8_t *)&head+4, keylen + 5);
		if (len != 0)
		{
			crc = AddCRC32 (crc, (uint8_t *)text, len);
		}
		crc = BigLong(crc);
		return file->Write (&crc, 4) == 4;
	}
	return false;
}

//==========================================================================
//
// M_FindPNGChunk
//
// Finds a chunk in a PNG file. The file pointer will be positioned at the
// beginning of the chunk data, and its length will be returned. A return
// value of 0 indicates the chunk was either not present or had 0 length.
// This means there is no way to conclusively determine if a chunk is not
// present in a PNG file with this function, but since we're only
// interested in chunks with content, that's okay. The file pointer will
// be left sitting at the start of the chunk's data if it was found.
//
//==========================================================================

unsigned int M_FindPNGChunk (PNGHandle *png, uint32_t id)
{
	png->ChunkPt = 0;
	return M_NextPNGChunk (png, id);
}

//==========================================================================
//
// M_NextPNGChunk
//
// Like M_FindPNGChunk, but it starts it search at the current chunk.
//
//==========================================================================

unsigned int M_NextPNGChunk (PNGHandle *png, uint32_t id)
{
	for ( ; png->ChunkPt < png->Chunks.Size(); ++png->ChunkPt)
	{
		if (png->Chunks[png->ChunkPt].ID == id)
		{ // Found the chunk
			png->File.Seek (png->Chunks[png->ChunkPt++].Offset, FileReader::SeekSet);
			return png->Chunks[png->ChunkPt - 1].Size;
		}
	}
	return 0;
}

//==========================================================================
//
// M_GetPNGText
//
// Finds a PNG text chunk with the given signature and returns a pointer
// to a NULL-terminated string if present. Returns NULL on failure.
//
//==========================================================================

char *M_GetPNGText (PNGHandle *png, const char *keyword)
{
	unsigned int i;
	size_t keylen, textlen;

	for (i = 0; i < png->TextChunks.Size(); ++i)
	{
		if (strncmp (keyword, png->TextChunks[i], 80) == 0)
		{
			// Woo! A match was found!
			keylen = std::min<size_t> (80, strlen (keyword) + 1);
			textlen = strlen (png->TextChunks[i] + keylen) + 1;
			char *str = new char[textlen];
			strcpy (str, png->TextChunks[i] + keylen);
			return str;
		}
	}
	return NULL;
}

// This version copies it to a supplied buffer instead of allocating a new one.

bool M_GetPNGText (PNGHandle *png, const char *keyword, char *buffer, size_t buffsize)
{
	unsigned int i;
	size_t keylen;

	for (i = 0; i < png->TextChunks.Size(); ++i)
	{
		if (strncmp (keyword, png->TextChunks[i], 80) == 0)
		{
			// Woo! A match was found!
			keylen = std::min<size_t> (80, strlen (keyword) + 1);
			strncpy (buffer, png->TextChunks[i] + keylen, buffsize);
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// M_VerifyPNG
//
// Returns a PNGHandle if the file is a PNG or NULL if not. CRC checking of
// chunks is not done in order to save time.
//
//==========================================================================

PNGHandle *M_VerifyPNG (FileReader &filer)
{
	PNGHandle::Chunk chunk;
	PNGHandle *png;
	uint32_t data[2];
	bool sawIDAT = false;

	if (filer.Read(&data, 8) != 8)
	{
		return NULL;
	}
	if (data[0] != MAKE_ID(137,'P','N','G') || data[1] != MAKE_ID(13,10,26,10))
	{ // Does not have PNG signature
		return NULL;
	}
	if (filer.Read (&data, 8) != 8)
	{
		return NULL;
	}
	if (data[1] != MAKE_ID('I','H','D','R'))
	{ // IHDR must be the first chunk
		return NULL;
	}

	// It looks like a PNG so far, so start creating a PNGHandle for it
	png = new PNGHandle (filer);
	// filer is no longer valid after the above line!
	chunk.ID = data[1];
	chunk.Offset = 16;
	chunk.Size = BigLong((unsigned int)data[0]);
	png->Chunks.Push (chunk);
	png->File.Seek (16, FileReader::SeekSet);

	while (png->File.Seek (chunk.Size + 4, FileReader::SeekCur) == 0)
	{
		// If the file ended before an IEND was encountered, it's not a PNG.
		if (png->File.Read (&data, 8) != 8)
		{
			break;
		}
		// An IEND chunk terminates the PNG and must be empty
		if (data[1] == MAKE_ID('I','E','N','D'))
		{
			if (data[0] == 0 && sawIDAT)
			{
				return png;
			}
			break;
		}
		// A PNG must include an IDAT chunk
		if (data[1] == MAKE_ID('I','D','A','T'))
		{
			sawIDAT = true;
		}
		chunk.ID = data[1];
		chunk.Offset = (uint32_t)png->File.Tell();
		chunk.Size = BigLong((unsigned int)data[0]);
		png->Chunks.Push (chunk);

		// If this is a text chunk, also record its contents.
		if (data[1] == MAKE_ID('t','E','X','t'))
		{
			char *str = new char[chunk.Size + 1];

			if (png->File.Read (str, chunk.Size) != chunk.Size)
			{
				delete[] str;
				break;
			}
			str[chunk.Size] = 0;
			png->TextChunks.Push (str);
			chunk.Size = 0;		// Don't try to seek past its contents again.
		}
	}

	filer = std::move(png->File);	// need to get the reader back if this function failed.
	delete png;
	return NULL;
}

//==========================================================================
//
// M_FreePNG
//
// Just deletes the PNGHandle. The file is not closed.
//
//==========================================================================

void M_FreePNG (PNGHandle *png)
{
	delete png;
}

//==========================================================================
//
// ReadIDAT
//
// Reads image data out of a PNG
//
//==========================================================================

bool M_ReadIDAT (FileReader &file, uint8_t *buffer, int width, int height, int pitch,
				 uint8_t bitdepth, uint8_t colortype, uint8_t interlace, unsigned int chunklen)
{
	// Uninterlaced images are treated as a conceptual eighth pass by these tables.
	static const uint8_t passwidthshift[8] =  { 3, 3, 2, 2, 1, 1, 0, 0 };
	static const uint8_t passheightshift[8] = { 3, 3, 3, 2, 2, 1, 1, 0 };
	static const uint8_t passrowoffset[8] =   { 0, 0, 4, 0, 2, 0, 1, 0 };
	static const uint8_t passcoloffset[8] =   { 0, 4, 0, 2, 0, 1, 0, 0 };

	Byte *inputLine, *prev, *curr, *adam7buff[3], *bufferend;
	Byte chunkbuffer[4096];
	z_stream stream;
	int err;
	int i, pass, passbuff, passpitch, passwidth;
	bool lastIDAT;
	int bytesPerRowIn, bytesPerRowOut;
	int bytesPerPixel;
	bool initpass;

	switch (colortype)
	{
	case 2:		bytesPerPixel = 3;		break;		// RGB
	case 4:		bytesPerPixel = 2;		break;		// LA
	case 6:		bytesPerPixel = 4;		break;		// RGBA
	default:	bytesPerPixel = 1;		break;
	}

	bytesPerRowOut = width * bytesPerPixel;
	i = 4 + bytesPerRowOut * 2;
	if (interlace)
	{
		i += bytesPerRowOut * 2;
	}
	inputLine = (Byte *)alloca (i);
	adam7buff[0] = inputLine + 4 + bytesPerRowOut;
	adam7buff[1] = adam7buff[0] + bytesPerRowOut;
	adam7buff[2] = adam7buff[1] + bytesPerRowOut;
	bufferend = buffer + pitch * height;

	stream.next_in = Z_NULL;
	stream.avail_in = 0;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	err = inflateInit (&stream);
	if (err != Z_OK)
	{
		return false;
	}
	lastIDAT = false;
	initpass = true;
	pass = interlace ? 0 : 7;

	// Silence GCC warnings. Due to initpass being true, these will be set
	// before they're used, but it doesn't know that.
	curr = prev = 0;
	passwidth = passpitch = bytesPerRowIn = 0;
	passbuff = 0;

	while (err != Z_STREAM_END && pass < 8 - interlace)
	{
		if (initpass)
		{
			int rowoffset, coloffset;

			initpass = false;
			pass--;
			do
			{
				pass++;
				rowoffset = passrowoffset[pass];
				coloffset = passcoloffset[pass];
			}
			while ((rowoffset >= height || coloffset >= width) && pass < 7);
			if (pass == 7 && interlace)
			{
				break;
			}
			passwidth = (width + (1 << passwidthshift[pass]) - 1 - coloffset) >> passwidthshift[pass];
			prev = adam7buff[0];
			passbuff = 1;
			memset (prev, 0, passwidth * bytesPerPixel);
			switch (bitdepth)
			{
			case 8:		bytesPerRowIn = passwidth * bytesPerPixel;	break;
			case 4:		bytesPerRowIn = (passwidth+1)/2;			break;
			case 2:		bytesPerRowIn = (passwidth+3)/4;			break;
			case 1:		bytesPerRowIn = (passwidth+7)/8;			break;
			default:	return false;
			}
			curr = buffer + rowoffset*pitch + coloffset*bytesPerPixel;
			passpitch = pitch << passheightshift[pass];
			stream.next_out = inputLine;
			stream.avail_out = bytesPerRowIn + 1;
		}
		if (stream.avail_in == 0 && chunklen > 0)
		{
			stream.next_in = chunkbuffer;
			stream.avail_in = (uInt)file.Read (chunkbuffer, std::min<uint32_t>(chunklen,sizeof(chunkbuffer)));
			chunklen -= stream.avail_in;
		}

		err = inflate (&stream, Z_SYNC_FLUSH);
		if (err != Z_OK && err != Z_STREAM_END)
		{ // something unexpected happened
			inflateEnd (&stream);
			return false;
		}

		if (stream.avail_out == 0)
		{
			if (pass >= 6)
			{
				// Store pixels directly into the output buffer
				UnfilterRow (bytesPerRowIn, curr, inputLine, prev, bytesPerPixel);
				prev = curr;
			}
			else
			{
				const uint8_t *in;
				uint8_t *out;
				int colstep, x;

				// Store pixels into a temporary buffer
				UnfilterRow (bytesPerRowIn, adam7buff[passbuff], inputLine, prev, bytesPerPixel);
				prev = adam7buff[passbuff];
				passbuff ^= 1;
				in = prev;
				if (bitdepth < 8)
				{
					UnpackPixels (passwidth, bytesPerRowIn, bitdepth, in, adam7buff[2], colortype == 0);
					in = adam7buff[2];
				}
				// Distribute pixels into the output buffer
				out = curr;
				colstep = bytesPerPixel << passwidthshift[pass];
				switch (bytesPerPixel)
				{
				case 1:
					for (x = passwidth; x > 0; --x)
					{
						*out = *in;
						out += colstep;
						in += 1;
					}
					break;

				case 2:
					for (x = passwidth; x > 0; --x)
					{
						*(uint16_t *)out = *(uint16_t *)in;
						out += colstep;
						in += 2;
					}
					break;

				case 3:
					for (x = passwidth; x > 0; --x)
					{
						out[0] = in[0];
						out[1] = in[1];
						out[2] = in[2];
						out += colstep;
						in += 3;
					}
					break;

				case 4:
					for (x = passwidth; x > 0; --x)
					{
						*(uint32_t *)out = *(uint32_t *)in;
						out += colstep;
						in += 4;
					}
					break;
				}
			}
			if ((curr += passpitch) >= bufferend)
			{
				++pass;
				initpass = true;
			}
			stream.next_out = inputLine;
			stream.avail_out = bytesPerRowIn + 1;
		}

		if (chunklen == 0 && !lastIDAT)
		{
			uint32_t x[3];

			if (file.Read (x, 12) != 12)
			{
				lastIDAT = true;
			}
			else if (x[2] != MAKE_ID('I','D','A','T'))
			{
				lastIDAT = true;
			}
			else
			{
				chunklen = BigLong((unsigned int)x[1]);
			}
		}
	}

	inflateEnd (&stream);

	if (bitdepth < 8)
	{
		// Noninterlaced images must be unpacked completely.
		// Interlaced images only need their final pass unpacked.
		passpitch = pitch << interlace;
		for (curr = buffer + pitch * interlace; curr <= prev; curr += passpitch)
		{
			UnpackPixels (width, bytesPerRowIn, bitdepth, curr, curr, colortype == 0);
		}
	}
	return true;
}

// PRIVATE CODE ------------------------------------------------------------


//==========================================================================
//
// MakeChunk
//
// Prepends the chunk length and type and appends the chunk's CRC32.
// There must be 8 bytes available before the chunk passed and 4 bytes
// after the chunk.
//
//==========================================================================

static inline void MakeChunk (void *where, uint32_t type, size_t len)
{
	uint8_t *const data = (uint8_t *)where;
	*(uint32_t *)(data - 8) = BigLong ((unsigned int)len);
	*(uint32_t *)(data - 4) = type;
	*(uint32_t *)(data + len) = BigLong ((unsigned int)CalcCRC32 (data-4, (unsigned int)(len+4)));
}

//==========================================================================
//
// StuffPalette
//
// Converts 256 4-byte palette entries to 3 bytes each.
//
//==========================================================================

static void StuffPalette (const PalEntry *from, uint8_t *to)
{
	for (int i = 256; i > 0; --i)
	{
		to[0] = from->r;
		to[1] = from->g;
		to[2] = from->b;
		from += 1;
		to += 3;
	}
}

//==========================================================================
//
// CalcSum
//
//
//==========================================================================

uint32_t CalcSum(Byte *row, int len)
{
	uint32_t sum = 0;

	while (len-- != 0)
	{
		sum += (char)*row++;
	}
	return sum;
}

//==========================================================================
//
// SelectFilter
//
// Performs the heuristic recommended by the PNG spec to decide the
// (hopefully) best filter to use for this row. To quate:
//
//    Select the filter that gives the smallest sum of absolute values of
//    outputs. (Consider the output bytes as signed differences for this
//    test.)
//
//==========================================================================

#if USE_FILTER_HEURISTIC
static int SelectFilter(Byte **row, Byte *prior, int width)
{
	// As it turns out, it seems no filtering is the best for Doom screenshots,
	// no matter what the heuristic might determine.
	return 0;
	uint32_t sum;
	uint32_t bestsum;
	int bestfilter;
	int x;

	width *= 3;

	// The first byte of each row holds the filter type, filled in by the caller.
	// However, the prior row does not contain a filter type, since it's always 0.

	bestsum = 0;
	bestfilter = 0;

	// None
	for (x = 1; x <= width; ++x)
	{
		bestsum += abs((char)row[0][x]);
	}

	// Sub
	row[1][1] = row[0][1];
	row[1][2] = row[0][2];
	row[1][3] = row[0][3];
	sum = abs((char)row[0][1]) + abs((char)row[0][2]) + abs((char)row[0][3]);
	for (x = 4; x <= width; ++x)
	{
		row[1][x] = row[0][x] - row[0][x - 3];
		sum += abs((char)row[1][x]);
		if (sum >= bestsum)
		{ // This isn't going to be any better.
			break;
		}
	}
	if (sum < bestsum)
	{
		bestsum = sum;
		bestfilter = 1;
	}

	// Up
	sum = 0;
	for (x = 1; x <= width; ++x)
	{
		row[2][x] = row[0][x] - prior[x - 1];
		sum += abs((char)row[2][x]);
		if (sum >= bestsum)
		{ // This isn't going to be any better.
			break;
		}
	}
	if (sum < bestsum)
	{
		bestsum = sum;
		bestfilter = 2;
	}

	// Average
	row[3][1] = row[0][1] - prior[0] / 2;
	row[3][2] = row[0][2] - prior[1] / 2;
	row[3][3] = row[0][3] - prior[2] / 2;
	sum = abs((char)row[3][1]) + abs((char)row[3][2]) + abs((char)row[3][3]);
	for (x = 4; x <= width; ++x)
	{
		row[3][x] = row[0][x] - (row[0][x - 3] + prior[x - 1]) / 2;
		sum += (char)row[3][x];
		if (sum >= bestsum)
		{ // This isn't going to be any better.
			break;
		}
	}
	if (sum < bestsum)
	{
		bestsum = sum;
		bestfilter = 3;
	}

	// Paeth
	row[4][1] = row[0][1] - prior[0];
	row[4][2] = row[0][2] - prior[1];
	row[4][3] = row[0][3] - prior[2];
	sum = abs((char)row[4][1]) + abs((char)row[4][2]) + abs((char)row[4][3]);
	for (x = 4; x <= width; ++x)
	{
		Byte a = row[0][x - 3];
		Byte b = prior[x - 1];
		Byte c = prior[x - 4];
		int p = a + b - c;
		int pa = abs(p - a);
		int pb = abs(p - b);
		int pc = abs(p - c);
		if (pa <= pb && pa <= pc)
		{
			row[4][x] = row[0][x] - a;
		}
		else if (pb <= pc)
		{
			row[4][x] = row[0][x] - b;
		}
		else
		{
			row[4][x] = row[0][x] - c;
		}
		sum += (char)row[4][x];
		if (sum >= bestsum)
		{ // This isn't going to be any better.
			break;
		}
	}
	if (sum < bestsum)
	{
		bestfilter = 4;
	}

	return bestfilter;
}
#else
#define SelectFilter(x,y,z)		0
#endif

//==========================================================================
//
// M_SaveBitmap
//
// Given a bitmap, creates one or more IDAT chunks in the given file.
// Returns true on success.
//
//==========================================================================

bool M_SaveBitmap(const uint8_t *from, ESSType color_type, int width, int height, int pitch, FileWriter *file)
{
	TArray<Byte> temprow_storage;

#if USE_FILTER_HEURISTIC
	static const unsigned temprow_count = 5;

	TArray<Byte> prior_storage(width * 3, true);
	Byte *prior = &prior_storage[0];
#else
	static const unsigned temprow_count = 1;
#endif

	const unsigned temprow_size = 1 + width * 3;
	temprow_storage.Resize(temprow_size * temprow_count);

	Byte* temprow[temprow_count];

	for (unsigned i = 0; i < temprow_count; ++i)
	{
		temprow[i] = &temprow_storage[temprow_size * i];
	}

	Byte buffer[PNG_WRITE_SIZE];
	z_stream stream;
	int err;
	int y;

	stream.next_in = Z_NULL;
	stream.avail_in = 0;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	err = deflateInit (&stream, png_level);

	if (err != Z_OK)
	{
		return false;
	}

	y = height;
	stream.next_out = buffer;
	stream.avail_out = sizeof(buffer);

	temprow[0][0] = 0;
#if USE_FILTER_HEURISTIC
	temprow[1][0] = 1;
	temprow[2][0] = 2;
	temprow[3][0] = 3;
	temprow[4][0] = 4;

	// Fill the prior row with 0 for RGB images. Paletted is always filter 0,
	// so it doesn't need this.
	if (color_type != SS_PAL)
	{
		memset(prior, 0, width * 3);
	}
#endif

	while (y-- > 0 && err == Z_OK)
	{
		switch (color_type)
		{
		case SS_PAL:
			memcpy(&temprow[0][1], from, width);
			// always use filter type 0 for paletted images
			stream.next_in = temprow[0];
			stream.avail_in = width + 1;
			break;

		case SS_RGB:
			memcpy(&temprow[0][1], from, width*3);
			stream.next_in = temprow[SelectFilter(temprow, prior, width)];
			stream.avail_in = width * 3 + 1;
			break;

		case SS_BGRA:
			for (int x = 0; x < width; ++x)
			{
				temprow[0][x*3 + 1] = from[x*4 + 2];
				temprow[0][x*3 + 2] = from[x*4 + 1];
				temprow[0][x*3 + 3] = from[x*4];
			}
			stream.next_in = temprow[SelectFilter(temprow, prior, width)];
			stream.avail_in = width * 3 + 1;
			break;
		}
#if USE_FILTER_HEURISTIC
		if (color_type != SS_PAL)
		{
			// Save this row for filter calculations on the next row.
			memcpy (prior, &temprow[0][1], stream.avail_in - 1);
		}
#endif

		from += pitch;

		err = deflate (&stream, (y == 0) ? Z_FINISH : 0);
		if (err != Z_OK)
		{
			break;
		}
		while (stream.avail_out == 0)
		{
			if (!WriteIDAT (file, buffer, sizeof(buffer)))
			{
				return false;
			}
			stream.next_out = buffer;
			stream.avail_out = sizeof(buffer);
			if (stream.avail_in != 0)
			{
				err = deflate (&stream, (y == 0) ? Z_FINISH : 0);
				if (err != Z_OK)
				{
					break;
				}
			}
		}
	}

	while (err == Z_OK)
	{
		err = deflate (&stream, Z_FINISH);
		if (err != Z_OK)
		{
			break;
		}
		if (stream.avail_out == 0)
		{
			if (!WriteIDAT (file, buffer, sizeof(buffer)))
			{
				return false;
			}
			stream.next_out = buffer;
			stream.avail_out = sizeof(buffer);
		}
	}

	deflateEnd (&stream);

	if (err != Z_STREAM_END)
	{
		return false;
	}
	return WriteIDAT (file, buffer, sizeof(buffer)-stream.avail_out);
}

//==========================================================================
//
// WriteIDAT
//
// Writes a single IDAT chunk to the file. Returns true on success.
//
//==========================================================================

static bool WriteIDAT (FileWriter *file, const uint8_t *data, int len)
{
	uint32_t foo[2], crc;

	foo[0] = BigLong (len);
	foo[1] = MAKE_ID('I','D','A','T');
	crc = CalcCRC32 ((uint8_t *)&foo[1], 4);
	crc = BigLong ((unsigned int)AddCRC32 (crc, data, len));

	if (file->Write (foo, 8) != 8 ||
		file->Write (data, len) != (size_t)len ||
		file->Write (&crc, 4) != 4)
	{
		return false;
	}
	return true;
}

//==========================================================================
//
// UnfilterRow
//
// Unfilters the given row. Unknown filter types are silently ignored.
// bpp is bytes per pixel, not bits per pixel.
// width is in bytes, not pixels.
//
//==========================================================================

void UnfilterRow (int width, uint8_t *dest, uint8_t *row, uint8_t *prev, int bpp)
{
	int x;

	switch (*row++)
	{
	case 1:		// Sub
		x = bpp;
		do
		{
			*dest++ = *row++;
		}
		while (--x);
		for (x = width - bpp; x > 0; --x)
		{
			*dest = *row++ + *(dest - bpp);
			dest++;
		}
		break;

	case 2:		// Up
		x = width;
		do
		{
			*dest++ = *row++ + *prev++;
		}
		while (--x);
		break;

	case 3:		// Average
		x = bpp;
		do
		{
			*dest++ = *row++ + (*prev++)/2;
		}
		while (--x);
		for (x = width - bpp; x > 0; --x)
		{
			*dest = *row++ + (uint8_t)((unsigned(*(dest - bpp)) + unsigned(*prev++)) >> 1);
			dest++;
		}
		break;

	case 4:		// Paeth
		x = bpp;
		do
		{
			*dest++ = *row++ + *prev++;
		}
		while (--x);
		for (x = width - bpp; x > 0; --x)
		{
			int a, b, c, pa, pb, pc;

			a = *(dest - bpp);
			b = *(prev);
			c = *(prev - bpp);
			pa = b - c;
			pb = a - c;
			pc = abs (pa + pb);
			pa = abs (pa);
			pb = abs (pb);
			*dest = *row + (uint8_t)((pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c);
			dest++;
			row++;
			prev++;
		}
		break;

	default:	// Treat everything else as filter type 0 (none)
		memcpy (dest, row, width);
		break;
	}
}

//==========================================================================
//
// UnpackPixels
//
// Unpacks a row of pixels whose depth is less than 8 so that each pixel
// occupies a single byte. The outrow must be "width" bytes long.
// "bytesPerRow" is the number of bytes for the packed row. The in and out
// rows may overlap, but only if rowin == rowout.
//
//==========================================================================

static void UnpackPixels (int width, int bytesPerRow, int bitdepth, const uint8_t *rowin, uint8_t *rowout, bool grayscale)
{
	const uint8_t *in;
	uint8_t *out;
	uint8_t pack;
	int lastbyte;

	assert(bitdepth == 1 || bitdepth == 2 || bitdepth == 4);

	out = rowout + width;
	in = rowin + bytesPerRow;

	switch (bitdepth)
	{
	case 1:

		lastbyte = width & 7;
		if (lastbyte != 0)
		{
			in--;
			pack = *in;
			out -= lastbyte;
			out[0] = (pack >> 7) & 1;
			if (lastbyte >= 2) out[1] = (pack >> 6) & 1;
			if (lastbyte >= 3) out[2] = (pack >> 5) & 1;
			if (lastbyte >= 4) out[3] = (pack >> 4) & 1;
			if (lastbyte >= 5) out[4] = (pack >> 3) & 1;
			if (lastbyte >= 6) out[5] = (pack >> 2) & 1;
			if (lastbyte == 7) out[6] = (pack >> 1) & 1;
		}

		while (in-- > rowin)
		{
			pack = *in;
			out -= 8;
			out[0] = (pack >> 7) & 1;
			out[1] = (pack >> 6) & 1;
			out[2] = (pack >> 5) & 1;
			out[3] = (pack >> 4) & 1;
			out[4] = (pack >> 3) & 1;
			out[5] = (pack >> 2) & 1;
			out[6] = (pack >> 1) & 1;
			out[7] = pack & 1;
		}
		break;

	case 2:

		lastbyte = width & 3;
		if (lastbyte != 0)
		{
			in--;
			pack = *in;
			out -= lastbyte;
			out[0] = pack >> 6;
			if (lastbyte >= 2) out[1] = (pack >> 4) & 3;
			if (lastbyte == 3) out[2] = (pack >> 2) & 3;
		}

		while (in-- > rowin)
		{
			pack = *in;
			out -= 4;
			out[0] = pack >> 6;
			out[1] = (pack >> 4) & 3;
			out[2] = (pack >> 2) & 3;
			out[3] = pack & 3;
		}
		break;

	case 4:
		lastbyte = width & 1;
		if (lastbyte != 0)
		{
			in--;
			pack = *in;
			out -= lastbyte;
			out[0] = pack >> 4;
		}

		while (in-- > rowin)
		{
			pack = *in;
			out -= 2;
			out[0] = pack >> 4;
			out[1] = pack & 15;
		}
		break;
	}

	// Expand grayscale to 8bpp
	if (grayscale)
	{
		// Put the 2-bit lookup table on the stack, since it's probably already
		// in a cache line.
		union
		{
			uint32_t bits2l;
			uint8_t bits2[4];
		};

		out = rowout + width;
		switch (bitdepth)
		{
		case 1:
			while (--out >= rowout)
			{
				// 1 becomes -1 (0xFF), and 0 remains untouched.
				*out = 0 - *out;
			}
			break;

		case 2:
			bits2l = MAKE_ID(0x00,0x55,0xAA,0xFF);
			while (--out >= rowout)
			{
				*out = bits2[*out];
			}
			break;

		case 4:
			while (--out >= rowout)
			{
				*out |= (*out << 4);
			}
			break;
		}
	}
}

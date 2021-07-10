/*
** consolebuffer.h
**
** manages the text for the console
**
**---------------------------------------------------------------------------
** Copyright 2014 Christoph Oelckers
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

#include <limits.h>
#include <stdio.h>
#include "zstring.h"
#include "tarray.h"
#include "v_text.h"

enum EAddType
{
	NEWLINE,
	APPENDLINE,
	REPLACELINE
};

class FConsoleBuffer
{
	TArray<FString> mConsoleText;
	TArray<TArray<FBrokenLines>> m_BrokenConsoleText;	// This holds the structures returned by V_BreakLines and is used for memory management.
	TArray<unsigned int> mBrokenStart;		
	TArray<FBrokenLines> mBrokenLines;		// This holds the single lines, indexed by mBrokenStart and is used for printing.
	FILE * mLogFile;
	EAddType mAddType;
	int mTextLines;
	bool mBufferWasCleared;
	
	FFont *mLastFont;
	int mLastDisplayWidth;
	bool mLastLineNeedsUpdate;

	
public:
	FConsoleBuffer();
	void AddText(int printlevel, int printflags, const char *string);
	void FormatText(FFont *formatfont, int displaywidth);
	void ResizeBuffer(unsigned newsize);
	void Clear()
	{
		mBufferWasCleared = true;
		mConsoleText.Clear();
	}
	int GetFormattedLineCount() { return mTextLines; }
	FBrokenLines *GetLines() { return &mBrokenLines[0]; }
};


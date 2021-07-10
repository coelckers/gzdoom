/*
** consolebuffer.cpp
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

#include "c_console.h"
#include "c_consolebuffer.h"
#include "printf.h"


//==========================================================================
//
//
//
//==========================================================================

FConsoleBuffer::FConsoleBuffer()
{
	mLogFile = NULL;
	mAddType = NEWLINE;
	mLastFont = NULL;
	mLastDisplayWidth = -1;
	mLastLineNeedsUpdate = false;
	mTextLines = 0;
	mBufferWasCleared = true;
	mBrokenStart.Push(0);
}

//==========================================================================
//
// Adds a new line of text to the console
// This is kept as simple as possible. This function does not:
// - remove old text if the buffer gets larger than the specified size
// - format the text for the current screen layout
//
// These tasks will only be be performed once per frame because they are
// relatively expensive. The old console did them each time text was added
// resulting in extremely bad performance with a high output rate.
//
//==========================================================================

void FConsoleBuffer::AddText(int printlevel, int printflags, const char *text)
{
	FString build = TEXTCOLOR_TAN;
	
	if (mAddType == REPLACELINE)
	{
		// Just wondering: Do we actually need this case? If so, it may need some work.
		mConsoleText.Pop();	// remove the line to be replaced
		mLastLineNeedsUpdate = true;
	}
	else if (mAddType == APPENDLINE)
	{
		mConsoleText.Pop(build);
		printlevel = -1;
		mLastLineNeedsUpdate = true;
	}
	
	if (printflags > 0 || (printlevel >= 0 && printlevel != PRINT_HIGH))
	{
		if (printflags & PRINTF_BOLD) build = TEXTCOLOR_GREEN;
		else if (printlevel < PRINTLEVELS) build.Format("%c%c", TEXTCOLOR_ESCAPE, PrintColors[printlevel]+'A');
	}
	
	size_t textsize = strlen(text);
	
	if (text[textsize-1] == '\r')
	{
		textsize--;
		mAddType = REPLACELINE;
	}
	else if (text[textsize-1] == '\n')
	{
		textsize--;
		mAddType = NEWLINE;
	}
	else
	{
		mAddType = APPENDLINE;
	}

	// don't bother with linefeeds etc. inside the text, we'll let the formatter sort this out later.
	build.AppendCStrPart(text, textsize);
	mConsoleText.Push(build);
}

//==========================================================================
//
// Format the text for output
//
//==========================================================================

void FConsoleBuffer::FormatText(FFont *formatfont, int displaywidth)
{
	if (formatfont != mLastFont || displaywidth != mLastDisplayWidth || mBufferWasCleared)
	{
		if (mBufferWasCleared)
			mLastLineNeedsUpdate = false;
		m_BrokenConsoleText.Clear();
		mBrokenStart.Clear();
		mBrokenStart.Push(0);
		mBrokenLines.Clear();
		mLastFont = formatfont;
		mLastDisplayWidth = displaywidth;
		mBufferWasCleared = false;
	}
	unsigned brokensize = m_BrokenConsoleText.Size();
	if (brokensize == mConsoleText.Size())
	{
		// The last line got text appended. 
		if (mLastLineNeedsUpdate)
		{
			brokensize--;
			m_BrokenConsoleText.Resize(brokensize);
		}
	}
	mBrokenLines.Resize(mBrokenStart[brokensize]);
	mBrokenStart.Resize(brokensize);
	for (unsigned i = brokensize; i < mConsoleText.Size(); i++)
	{
		auto bl = V_BreakLines(formatfont, displaywidth, mConsoleText[i], true);
		m_BrokenConsoleText.Push(bl);
		mBrokenStart.Push(mBrokenLines.Size());
		for(auto &bline : bl)
		{
			mBrokenLines.Push(bline);
		}
	}
	mTextLines = mBrokenLines.Size();
	mBrokenStart.Push(mTextLines);
	mLastLineNeedsUpdate = false;
}

//==========================================================================
//
// Delete old content if number of lines gets too large
//
//==========================================================================

void FConsoleBuffer::ResizeBuffer(unsigned newsize)
{
	if (mConsoleText.Size() > newsize)
	{
		unsigned todelete = mConsoleText.Size() - newsize;
		mConsoleText.Delete(0, todelete);
		mBufferWasCleared = true;
	}
}


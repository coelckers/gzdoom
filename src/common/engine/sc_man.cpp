/* For code that originates from ZDoom the following applies:
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
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

#include <string.h>
#include <stdlib.h>
#include "engineerrors.h"
#include "sc_man.h"
#include "cmdlib.h"
#include "templates.h"
#include "printf.h"
#include "name.h"
#include "v_text.h"
#include "templates.h"
#include "zstring.h"
#include "name.h"
#include <inttypes.h>
#include "filesystem.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void VersionInfo::operator=(const char *string)
{
	char *endp;
	major = (int16_t)clamp<unsigned long long>(strtoull(string, &endp, 10), 0, USHRT_MAX);
	if (*endp == '.')
	{
		minor = (int16_t)clamp<unsigned long long>(strtoull(endp + 1, &endp, 10), 0, USHRT_MAX);
		if (*endp == '.')
		{
			revision = (int16_t)clamp<unsigned long long>(strtoull(endp + 1, &endp, 10), 0, USHRT_MAX);
			if (*endp != 0) major = USHRT_MAX;
		}
		else if (*endp == 0)
		{
			revision = 0;
		}
		else major = USHRT_MAX;
	}
	else if (*endp == 0)
	{
		minor = revision = 0;
	}
	else major = USHRT_MAX;
}

//==========================================================================
//
// FScanner Constructor
//
//==========================================================================

FScanner::FScanner()
{
	ScriptOpen = false;
}

//==========================================================================
//
// FScanner Destructor
//
//==========================================================================

FScanner::~FScanner()
{
	// Humm... Nothing to do in here.
}

//==========================================================================
//
// FScanner Copy Constructor
//
//==========================================================================

FScanner::FScanner(const FScanner &other)
{
	ScriptOpen = false;
	*this = other;
}

//==========================================================================
//
// FScanner OpenLumpNum Constructor
//
//==========================================================================

FScanner::FScanner(int lumpnum)
{
	ScriptOpen = false;
	OpenLumpNum(lumpnum);
}

//==========================================================================
//
// FScanner :: operator =
//
//==========================================================================

FScanner &FScanner::operator=(const FScanner &other)
{
	if (this == &other)
	{
		return *this;
	}
	if (!other.ScriptOpen)
	{
		Close();
		return *this;
	}

	// Copy protected members
	ScriptOpen = true;
	ScriptName = other.ScriptName;
	ScriptBuffer = other.ScriptBuffer;
	ScriptPtr = other.ScriptPtr;
	ScriptEndPtr = other.ScriptEndPtr;
	AlreadyGot = other.AlreadyGot;
	AlreadyGotLine = other.AlreadyGotLine;
	LastGotToken = other.LastGotToken;
	LastGotPtr = other.LastGotPtr;
	LastGotLine = other.LastGotLine;
	CMode = other.CMode;
	Escape = other.Escape;
	StateMode = other.StateMode;
	StateOptions = other.StateOptions;

	// Copy public members
	if (other.String == other.StringBuffer)
	{
		memcpy(StringBuffer, other.StringBuffer, sizeof(StringBuffer));
		BigStringBuffer = "";
		String = StringBuffer;
	}
	else
	{
		// Past practice means the string buffer must be writeable, which
		// removes some of the benefit from using an FString to store
		// the big string buffer.
		BigStringBuffer = other.BigStringBuffer;
		String = BigStringBuffer.LockBuffer();
	}
	StringLen = other.StringLen;
	TokenType = other.TokenType;
	Number = other.Number;
	Float = other.Float;
	Line = other.Line;
	End = other.End;
	Crossed = other.Crossed;

	return *this;
}

//==========================================================================
//
// FScanner :: Open
//
//==========================================================================

void FScanner::Open (const char *name)
{
	int lump = fileSystem.CheckNumForFullName(name, true);
	if (lump == -1)
	{
		I_Error("Could not find script lump '%s'\n", name);
	}
	OpenLumpNum(lump);
}

//==========================================================================
//
// FScanner :: OpenFile
//
// Loads a script from a file. Uses new/delete for memory allocation.
//
//==========================================================================

bool FScanner::OpenFile (const char *name)
{
	Close ();

	FileReader fr;
	if (!fr.OpenFile(name)) return false;
	auto filesize = fr.GetLength();
	auto filebuff = fr.Read();
	if (filebuff.Size() == 0 && filesize > 0) return false;

	ScriptBuffer = FString((const char *)filebuff.Data(), filesize);
	ScriptName = name;	// This is used for error messages so the full file name is preferable
	LumpNum = -1;
	PrepareScript ();
	return true;
}

//==========================================================================
//
// FScanner :: OpenMem
//
// Prepares a script that is already in memory for parsing. The memory is
// copied, so you can do whatever you want with it after opening it.
//
//==========================================================================

void FScanner::OpenMem (const char *name, const char *buffer, int size)
{
	OpenString(name, FString(buffer, size));
}

//==========================================================================
//
// FScanner :: OpenString
//
// Like OpenMem, but takes a string directly.
//
//==========================================================================

void FScanner::OpenString (const char *name, FString buffer)
{
	Close ();
	ScriptBuffer = buffer;
	ScriptName = name;
	LumpNum = -1;
	PrepareScript ();
}

//==========================================================================
//
// FScanner :: OpenLumpNum
//
// Loads a script from the lump directory
//
//==========================================================================

void FScanner :: OpenLumpNum (int lump)
{
	Close ();
	{
		FileData mem = fileSystem.ReadFile(lump);
		ScriptBuffer = mem.GetString();
	}
	ScriptName = fileSystem.GetFileFullPath(lump);
	LumpNum = lump;
	PrepareScript ();
}

//==========================================================================
//
// FScanner :: PrepareScript
//
// Prepares a script for parsing.
//
//==========================================================================

void FScanner::PrepareScript ()
{
	// If the file got a UTF-8 byte order mark, remove that.
	if (ScriptBuffer.Len() > 3 && ScriptBuffer[0] == (char)0xEF && ScriptBuffer[1] == (char)0xBB && ScriptBuffer[2] == (char)0xBF)
	{
		ScriptBuffer = ScriptBuffer.Mid(3);
	}

	// The scanner requires the file to end with a '\n', so add one if
	// it doesn't already.
	if (ScriptBuffer.Len() == 0 || ScriptBuffer.Back() != '\n')
	{
		// If the last character in the buffer is a null character, change
		// it to a newline. Otherwise, append a newline to the end.
		if (ScriptBuffer.Len() > 0 && ScriptBuffer.Back() == '\0')
		{
			ScriptBuffer.LockBuffer()[ScriptBuffer.Len() - 1] = '\n';
			ScriptBuffer.UnlockBuffer();
		}
		else
		{
			ScriptBuffer += '\n';
		}
	}

	ScriptPtr = &ScriptBuffer[0];
	ScriptEndPtr = &ScriptBuffer[ScriptBuffer.Len()];
	Line = 1;
	End = false;
	ScriptOpen = true;
	String = StringBuffer;
	AlreadyGot = false;
	LastGotToken = false;
	LastGotPtr = NULL;
	LastGotLine = 1;
	CMode = false;
	Escape = true;
	StateMode = 0;
	StateOptions = false;
	StringBuffer[0] = '\0';
	BigStringBuffer = "";
}

//==========================================================================
//
// FScanner :: Close
//
//==========================================================================

void FScanner::Close ()
{
	ScriptOpen = false;
	ScriptBuffer = "";
	BigStringBuffer = "";
	StringBuffer[0] = '\0';
	String = StringBuffer;
}

//==========================================================================
//
// FScanner :: SavePos
//
// Saves the current script location for restoration later
//
//==========================================================================

const FScanner::SavedPos FScanner::SavePos ()
{
	SavedPos pos;

	CheckOpen ();
	if (End)
	{
		pos.SavedScriptPtr = NULL;
	}
	else
	{
		pos.SavedScriptPtr = ScriptPtr;
	}
	pos.SavedScriptLine = Line;
	return pos;
}

//==========================================================================
//
// FScanner :: RestorePos
//
// Restores the previously saved script location
//
//==========================================================================

void FScanner::RestorePos (const FScanner::SavedPos &pos)
{
	if (pos.SavedScriptPtr)
	{
		ScriptPtr = pos.SavedScriptPtr;
		Line = pos.SavedScriptLine;
		End = false;
	}
	else
	{
		End = true;
	}
	AlreadyGot = false;
	LastGotToken = false;
	Crossed = false;
}

long long FScanner::mystrtoll(const char* p, char** endp, int base)
{
	// Do not treat a leading 0 as an octal identifier if so desired.
	if (NoOctals && *p == '0' && p[1] != 'x' && p[1] != 'X' && base == 0) base = 10;
	return strtoll(p, endp, base);
}

//==========================================================================
//
// FScanner :: isText
//
// Checks if this is a text file.
//
//==========================================================================

bool FScanner::isText()
{
	for(unsigned int i=0;i<ScriptBuffer.Len();i++)
	{
		int c = ScriptBuffer[i];
		if (c < ' ' && c != '\n' && c != '\r' && c != '\t') return false;
	}
	return true;
}

//==========================================================================
//
// FScanner :: SetCMode
//
// Enables/disables C mode. In C mode, more characters are considered to
// be whole words than in non-C mode.
//
//==========================================================================

void FScanner::SetCMode (bool cmode)
{
	CMode = cmode;
}

//==========================================================================
//
// FScanner :: SetEscape
//
// Turns the escape sequence \" in strings on or off. If it's off, that
// means you can't include quotation marks inside strings.
//
//==========================================================================

void FScanner::SetEscape (bool esc)
{
	Escape = esc;
}

//==========================================================================
//
// FScanner :: SetStateMode
//
// Enters state mode. This mode is very permissive for identifiers, which
// it returns as TOK_NonWhitespace. The only character sequences that are
// not returned as such are these:
//
//   * stop
//   * wait
//   * fail
//   * loop
//   * goto - Automatically exits state mode after it's seen.
//   * :
//   * ;
//   * } - Automatically exits state mode after it's seen.
//
// Quoted strings are returned as TOK_NonWhitespace, minus the quotes. Once
// two consecutive sequences of TOK_NonWhitespace have been encountered
// (which would be the state's sprite and frame specifiers), nearly normal
// processing resumes, with the exception that various identifiers
// used for state options will be returned as tokens and not identifiers.
// This ends once a ';' or '{' character is encountered.
//
//==========================================================================

void FScanner::SetStateMode(bool stately)
{
	StateMode = stately ? 2 : 0;
	StateOptions = stately;
}

//==========================================================================
//
// FScanner::ScanString
//
// Set tokens true if you want TokenType to be set.
//
//==========================================================================

bool FScanner::ScanString (bool tokens)
{
	const char *marker, *tok;
	bool return_val;

	ParseError = false;
	CheckOpen();
	if (AlreadyGot)
	{
		AlreadyGot = false;
		if (!tokens || LastGotToken)
		{
			return true;
		}
		ScriptPtr = LastGotPtr;
		Line = LastGotLine;
	}

	Crossed = false;
	if (ScriptPtr >= ScriptEndPtr)
	{
		End = true;
		return false;
	}

	LastGotPtr = ScriptPtr;
	LastGotLine = Line;

	// In case the generated scanner does not use marker, avoid compiler warnings.
	// marker;
#include "sc_man_scanner.h"
	LastGotToken = tokens;
	return return_val;
}

//==========================================================================
//
// FScanner :: GetString
//
//==========================================================================

bool FScanner::GetString ()
{
	return ScanString (false);
}

//==========================================================================
//
// FScanner :: MustGetString
//
//==========================================================================

void FScanner::MustGetString (void)
{
	if (FScanner::GetString() == false)
	{
		ScriptError ("Missing string (unexpected end of file).");
	}
}

//==========================================================================
//
// FScanner :: MustGetStringName
//
//==========================================================================

void FScanner::MustGetStringName (const char *name)
{
	MustGetString ();
	if (Compare (name) == false)
	{
		ScriptError ("Expected '%s', got '%s'.", name, String);
	}
}

//==========================================================================
//
// FScanner :: CheckString
//
// Checks if the next token matches the specified string. Returns true if
// it does. If it doesn't, it ungets it and returns false.
//
//==========================================================================

bool FScanner::CheckString (const char *name)
{
	if (GetString ())
	{
		if (Compare (name))
		{
			return true;
		}
		UnGet ();
	}
	return false;
}

//==========================================================================
//
// FScanner :: GetToken
//
// Sets sc_Float, sc_Number, and sc_Name based on sc_TokenType.
//
//==========================================================================

bool FScanner::GetToken (bool evaluate)
{
	if (ScanString (true))
	{
		if (TokenType == TK_IntConst)
		{
			char *stopper;
			// Check for unsigned
			if (String[StringLen - 1] == 'u' || String[StringLen - 1] == 'U' ||
				String[StringLen - 2] == 'u' || String[StringLen - 2] == 'U')
			{
				TokenType = TK_UIntConst;
				BigNumber = (int64_t)strtoull(String, &stopper, 0);
				Number = (int)BigNumber;// clamp<int64_t>(BigNumber, 0, UINT_MAX);
				Float = (unsigned)Number;
			}
			else
			{
				BigNumber = mystrtoll(String, &stopper, 0);
				Number = (int)BigNumber;// clamp<int64_t>(BigNumber, 0, UINT_MAX);
				Float = Number;
			}
		}
		else if (TokenType == TK_FloatConst)
		{
			char *stopper;
			Float = strtod(String, &stopper);
		}
		else if (TokenType == TK_StringConst)
		{
			StringLen = strbin(const_cast<char*>(String));
		}
		else if (TokenType == TK_Identifier && evaluate && symbols.CountUsed() > 0)
		{
			auto sym = symbols.CheckKey(String);
			if (sym)
			{
				TokenType = sym->tokenType;
				BigNumber = sym->Number;
				Number = (int)sym->Number;
				Float = sym->Float;
				// String will retain the actual symbol name.
			}
		}
		return true;
	}
	return false;
}

//==========================================================================
//
// FScanner :: MustGetAnyToken
//
//==========================================================================

void FScanner::MustGetAnyToken (bool evaluate)
{
	if (GetToken (evaluate) == false)
	{
		ScriptError ("Missing token (unexpected end of file).");
	}
}

//==========================================================================
//
// FScanner :: TokenMustBe
//
//==========================================================================

void FScanner::TokenMustBe (int token)
{
	if (TokenType != token)
	{
		FString tok1 = TokenName(token);
		FString tok2 = TokenName(TokenType, String);
		ScriptError ("Expected %s but got %s instead.", tok1.GetChars(), tok2.GetChars());
	}
}

//==========================================================================
//
// FScanner :: MustGetToken
//
//==========================================================================

void FScanner::MustGetToken (int token, bool evaluate)
{
	MustGetAnyToken (evaluate);
	TokenMustBe(token);
}

//==========================================================================
//
// FScanner :: CheckToken
//
// Checks if the next token matches the specified token. Returns true if
// it does. If it doesn't, it ungets it and returns false.
//
//==========================================================================

bool FScanner::CheckToken (int token, bool evaluate)
{
	if (GetToken (evaluate))
	{
		if (TokenType == token)
		{
			return true;
		}
		UnGet ();
	}
	return false;
}

//==========================================================================
//
// FScanner :: GetNumber
//
//==========================================================================

bool FScanner::GetNumber (bool evaluate)
{
	char *stopper;

	CheckOpen();
	if (GetString())
	{
		if (strcmp (String, "MAXINT") == 0)
		{
			Number = INT_MAX;
		}
		else
		{
			BigNumber = mystrtoll(String, &stopper, 0);
			Number = (int)BigNumber;// clamp<int64_t>(BigNumber, 0, UINT_MAX);
			if (*stopper != 0)
			{
				if (evaluate && symbols.CountUsed())
				{
					auto sym = symbols.CheckKey(String);
					if (sym && sym->tokenType == TK_IntConst)
					{
						BigNumber = sym->Number;
						Number = (int)sym->Number;
						Float = sym->Float;
						// String will retain the actual symbol name.
						return true;
					}

				}
				ScriptError ("SC_GetNumber: Bad numeric constant \"%s\".", String);
				return false;
			}
		}
		Float = Number;
		return true;
	}
	else
	{
		return false;
	}
}

//==========================================================================
//
// FScanner :: MustGetNumber
//
//==========================================================================

void FScanner::MustGetNumber (bool evaluate)
{
	if (GetNumber(evaluate) == false)
	{
		ScriptError ("Missing integer (unexpected end of file).");
	}
}

//==========================================================================
//
// FScanner :: CheckNumber
//
// similar to GetNumber but ungets the token if it isn't a number 
// and does not print an error
//
//==========================================================================

bool FScanner::CheckNumber (bool evaluate)
{
	char *stopper;

	if (GetString())
	{
		if (String[0] == 0)
		{
			UnGet();
			return false;
		}
		else if (strcmp (String, "MAXINT") == 0)
		{
			BigNumber = INT64_MAX;
			Number = INT_MAX;
		}
		else
		{
			BigNumber = mystrtoll (String, &stopper, 0);
			Number = (int)BigNumber;// clamp<int64_t>(BigNumber, 0, UINT_MAX);
			if (*stopper != 0)
			{
				if (evaluate && symbols.CountUsed())
				{
					auto sym = symbols.CheckKey(String);
					if (sym && sym->tokenType == TK_IntConst)
					{
						BigNumber = sym->Number;
						Number = (int)sym->Number;
						Float = sym->Float;
						// String will retain the actual symbol name.
						return true;
					}

				}
				UnGet();
				return false;
			}
		}
		Float = Number;
		return true;
	}
	else
	{
		return false;
	}
}

//==========================================================================
//
// FScanner :: CheckFloat
//
// [GRB] Same as SC_CheckNumber, only for floats
//
//==========================================================================

bool FScanner::CheckFloat (bool evaluate)
{
	char *stopper;

	if (GetString())
	{
		if (String[0] == 0)
		{
			UnGet();
			return false;
		}
	
		Float = strtod (String, &stopper);
		if (*stopper != 0)
		{
			if (evaluate && symbols.CountUsed())
			{
				auto sym = symbols.CheckKey(String);
				if (sym && sym->tokenType == TK_IntConst && sym->tokenType != TK_FloatConst)
				{
					BigNumber = sym->Number;
					Number = (int)sym->Number;
					Float = sym->Float;
					// String will retain the actual symbol name.
					return true;
				}

			}

			UnGet();
			return false;
		}
		return true;
	}
	else
	{
		return false;
	}
}


//==========================================================================
//
// FScanner :: GetFloat
//
//==========================================================================

bool FScanner::GetFloat (bool evaluate)
{
	char *stopper;

	CheckOpen ();
	if (GetString())
	{
		Float = strtod (String, &stopper);
		if (*stopper != 0)
		{
			if (evaluate && symbols.CountUsed())
			{
				auto sym = symbols.CheckKey(String);
				if (sym && sym->tokenType == TK_IntConst && sym->tokenType != TK_FloatConst)
				{
					BigNumber = sym->Number;
					Number = (int)sym->Number;
					Float = sym->Float;
					// String will retain the actual symbol name.
					return true;
				}
			}

			ScriptError ("SC_GetFloat: Bad numeric constant \"%s\".", String);
			return false;
		}
		Number = (int)Float;
		return true;
	}
	else
	{
		return false;
	}
}

//==========================================================================
//
// FScanner :: MustGetFloat
//
//==========================================================================

void FScanner::MustGetFloat (bool evaluate)
{
	if (GetFloat(evaluate) == false)
	{
		ScriptError ("Missing floating-point number (unexpected end of file).");
	}
}

//==========================================================================
//
// FScanner :: UnGet
//
// Assumes there is a valid string in String.
//
//==========================================================================

void FScanner::UnGet ()
{
	AlreadyGot = true;
	AlreadyGotLine = LastGotLine;	// in case of an error we want the line of the last token.
}

//==========================================================================
//
// FScanner :: MatchString
//
// Returns the index of the first match to String from the passed
// array of strings, or -1 if not found.
//
//==========================================================================

int FScanner::MatchString (const char * const *strings, size_t stride)
{
	int i;

	assert(stride % sizeof(const char*) == 0);

	stride /= sizeof(const char*);

	for (i = 0; *strings != NULL; i++)
	{
		if (Compare (*strings))
		{
			return i;
		}
		strings += stride;
	}
	return -1;
}

//==========================================================================
//
// FScanner :: MustMatchString
//
//==========================================================================

int FScanner::MustMatchString (const char * const *strings, size_t stride)
{
	int i;

	i = MatchString (strings, stride);
	if (i == -1)
	{
		ScriptError ("Unknown keyword '%s'", String);
	}
	return i;
}

//==========================================================================
//
// FScanner :: Compare
//
//==========================================================================

bool FScanner::Compare (const char *text)
{
	return (stricmp (text, String) == 0);
}


//==========================================================================
//
// Convenience helpers that parse an entire number including a leading minus or plus sign
//
//==========================================================================

bool FScanner::ScanValue(bool allowfloat, bool evaluate)
{
	bool neg = false;
	if (!GetToken(evaluate)) 
	{
		return false;
	}
	if (TokenType == '-' || TokenType == '+')
	{
		neg = TokenType == '-';
		if (!GetToken(evaluate))
		{
			return false;
		}
	}

	if (TokenType == TK_FloatConst && !allowfloat)
		return false;

	if (TokenType != TK_IntConst && TokenType != TK_FloatConst) 
	{
		auto d = constants.CheckKey(String);
		if (!d) return false;
		if (!allowfloat && int64_t(*d) != *d) return false;
		BigNumber = int64_t(*d);
		Number = int(*d);
		Float = *d;
	}
	if (neg)
	{
		BigNumber = -BigNumber;
		Number = -Number;
		Float = -Float;
	}
	return true;
}

bool FScanner::CheckValue(bool allowfloat, bool evaluate) 
{ 
	auto savedstate = SavePos();
	bool res = ScanValue(allowfloat, evaluate);
	if (!res) RestorePos(savedstate);
	return res;
}

void FScanner::MustGetValue(bool allowfloat, bool evaluate)
{
	if (!ScanValue(allowfloat, evaluate)) ScriptError(allowfloat ? "Numeric constant expected" : "Integer constant expected");
}

bool FScanner::CheckBoolToken()
{
	if (CheckToken(TK_True))
	{
		Number = 1;
		Float = 1;
		return true;
	}
	if (CheckToken(TK_False))
	{
		Number = 0;
		Float = 0;
		return true;
	}
	return false;
}

void FScanner::MustGetBoolToken()
{
	if (!CheckBoolToken())
		ScriptError("Expected true or false");
}

//==========================================================================
//
// FScanner :: TokenName
//
// Returns the name of a token.
//
//==========================================================================

FString FScanner::TokenName (int token, const char *string)
{
	static const char *const names[] =
	{
#define xx(sym,str)		str,
#include "sc_man_tokens.h"
	};

	FString work;

	if (token > ' ' && token < 256)
	{
		work = '\'';
		work += token;
		work += '\'';
	}
	else if (token >= TK_Identifier && token < TK_LastToken)
	{
		work = names[token - TK_Identifier];
		if (string != NULL && token >= TK_Identifier && token <= TK_FloatConst)
		{
			work += ' ';
			char quote = (token == TK_StringConst) ? '"' : '\'';
			work += quote;
			work += string;
			work += quote;
		}
	}
	else
	{
		FString work;
		work.Format ("Unknown(%d)", token);
		return work;
	}
	return work;
}

//==========================================================================
//
// FScanner::GetMessageLine
//
//==========================================================================

int FScanner::GetMessageLine()
{
	return AlreadyGot? AlreadyGotLine : Line;
}

//==========================================================================
//
// FScanner::ScriptError
//
//==========================================================================

void FScanner::ScriptError (const char *message, ...)
{
	FString composed;

	if (message == NULL)
	{
		composed = "Bad syntax.";
	}
	else
	{
		va_list arglist;
		va_start (arglist, message);
		composed.VFormat (message, arglist);
		va_end (arglist);
	}

	if (NoFatalErrors)
	{
		Printf(TEXTCOLOR_RED "Script error, \"%s\"" TEXTCOLOR_RED " line %d:\n" TEXTCOLOR_RED "%s\n", ScriptName.GetChars(),
			AlreadyGot ? AlreadyGotLine : Line, composed.GetChars());
		return;
	}
	I_Error ("Script error, \"%s\" line %d:\n%s\n", ScriptName.GetChars(),
		AlreadyGot? AlreadyGotLine : Line, composed.GetChars());
}

//==========================================================================
//
// FScanner::ScriptMessage
//
//==========================================================================

void FScanner::ScriptMessage (const char *message, ...)
{
	FString composed;

	if (message == NULL)
	{
		composed = "Bad syntax.";
	}
	else
	{
		va_list arglist;
		va_start (arglist, message);
		composed.VFormat (message, arglist);
		va_end (arglist);
	}

	Printf (TEXTCOLOR_RED "Script error, \"%s\"" TEXTCOLOR_RED " line %d:\n" TEXTCOLOR_RED "%s\n", ScriptName.GetChars(),
		AlreadyGot? AlreadyGotLine : Line, composed.GetChars());
}

//==========================================================================
//
// FScanner :: CheckOpen
//
//==========================================================================

void FScanner::CheckOpen()
{
	if (ScriptOpen == false)
	{
		I_Error ("SC_ call before SC_Open().");
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FScanner::AddSymbol(const char *name, int64_t value)
{
	Symbol sym;
	sym.tokenType = TK_IntConst;
	sym.Number = int(value);
	sym.Float = double(value);
	symbols.Insert(name, sym);
}

//==========================================================================
//
// 
//
//==========================================================================

void FScanner::AddSymbol(const char* name, uint64_t value)
{
	Symbol sym;
	sym.tokenType = TK_UIntConst;
	sym.Number = value;
	sym.Float = (double)value;
	symbols.Insert(name, sym);
}

//==========================================================================
//
// 
//
//==========================================================================

void FScanner::SkipToEndOfBlock()
{
	int depth = 0;
	while (1)
	{
		MustGetString(); // this will abort if it reaches the end of the file
		if (Compare("{")) depth++;
		else if (Compare("}"))
		{
			depth--;
			if (depth < 0) return;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FScanner::AddSymbol(const char* name, double value)
{
	Symbol sym;
	sym.tokenType = TK_FloatConst;
	sym.Number = (int64_t)value;
	sym.Float = value;
	symbols.Insert(name, sym);
}

//==========================================================================
//
//
//
//==========================================================================

int FScanner::StartBraces(FScanner::SavedPos* braceend)
{
	if (CheckString("{"))
	{
		auto here = SavePos();
		SkipToEndOfBlock();
		*braceend = SavePos();
		RestorePos(here);
		return 0;
	}
	else
	{
		ScriptError("'{' expected");
		return -1;
	}
}

//==========================================================================
//
//
//
//==========================================================================

bool FScanner::FoundEndBrace(FScanner::SavedPos& braceend)
{
	auto here = SavePos();
	return here.SavedScriptPtr >= braceend.SavedScriptPtr;
}


//==========================================================================
//
// a class that remembers a parser position
//
//==========================================================================
int FScriptPosition::ErrorCounter;
int FScriptPosition::WarnCounter;
int FScriptPosition::Developer;
bool FScriptPosition::StrictErrors;	// makes all OPTERROR messages real errors.
bool FScriptPosition::errorout;		// call I_Error instead of printing the error itself.


FScriptPosition::FScriptPosition(FString fname, int line)
{
	FileName = fname;
	ScriptLine = line;
}

FScriptPosition::FScriptPosition(FScanner &sc)
{
	FileName = sc.ScriptName;
	ScriptLine = sc.GetMessageLine();
}

FScriptPosition &FScriptPosition::operator=(FScanner &sc)
{
	FileName = sc.ScriptName;
	ScriptLine = sc.GetMessageLine();
	return *this;
}

//==========================================================================
//
// FScriptPosition::Message
//
//==========================================================================

void FScriptPosition::Message (int severity, const char *message, ...) const
{
	FString composed;

	if (severity == MSG_DEBUGLOG && Developer < DMSG_NOTIFY) return;
	if (severity == MSG_DEBUGERROR && Developer < DMSG_ERROR) return;
	if (severity == MSG_DEBUGWARN && Developer < DMSG_WARNING) return;
	if (severity == MSG_DEBUGMSG && Developer < DMSG_NOTIFY) return;
	if (severity == MSG_OPTERROR)
	{
		severity = StrictErrors? MSG_ERROR : MSG_WARNING;
	}
	// This is mainly for catching the error with an exception handler.
	if (severity == MSG_ERROR && errorout) severity = MSG_FATAL;

	if (message == NULL)
	{
		composed = "Bad syntax.";
	}
	else
	{
		va_list arglist;
		va_start (arglist, message);
		composed.VFormat (message, arglist);
		va_end (arglist);
	}
	const char *type = "";
	const char *color;
	int level = PRINT_HIGH;
	int flags = PRINTF_DEFAULT;

	switch (severity)
	{
	default:
		return;

	case MSG_WARNING:
	case MSG_DEBUGWARN:
	case MSG_DEBUGERROR:	// This is intentionally not being printed as an 'error', the difference to MSG_DEBUGWARN is only the severity level at which it gets triggered.
		WarnCounter++;
		type = "warning";
		color = TEXTCOLOR_ORANGE;
		break;

	case MSG_ERROR:
		ErrorCounter++;
		type = "error";
		color = TEXTCOLOR_RED;
		break;

	case MSG_MESSAGE:
	case MSG_DEBUGMSG:
		type = "message";
		color = TEXTCOLOR_GREEN;
		break;

	case MSG_DEBUGLOG:
	case MSG_LOG:
		type = "message";
		flags = PRINTF_LOGONLY;
		color = "";
		break;

	case MSG_FATAL:
		I_Error ("Script error, \"%s\" line %d:\n%s\n",
			FileName.GetChars(), ScriptLine, composed.GetChars());
		return;
	}
	Printf (level, flags, "%sScript %s, \"%s\" line %d:\n%s%s\n",
		color, type, FileName.GetChars(), ScriptLine, color, composed.GetChars());
}

//==========================================================================
//
// ParseHex
//
//==========================================================================

int ParseHex(const char* hex, FScriptPosition* sc)
{
	const char* str;
	int num;

	num = 0;
	str = hex;

	while (*str)
	{
		num <<= 4;
		if (*str >= '0' && *str <= '9')
			num += *str - '0';
		else if (*str >= 'a' && *str <= 'f')
			num += 10 + *str - 'a';
		else if (*str >= 'A' && *str <= 'F')
			num += 10 + *str - 'A';
		else {
			if (sc) sc->Message(MSG_WARNING, "Bad hex number: %s", hex);
			else Printf("Bad hex number: %s\n", hex);
			return 0;
		}
		str++;
	}

	return num;
}



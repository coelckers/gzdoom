/*
** c_dispatch.cpp
** Functions for executing console commands and aliases
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "templates.h"
#include "cmdlib.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "m_argv.h"
#include "gamestate.h"
#include "configfile.h"
#include "printf.h"
#include "c_cvars.h"
#include "c_buttons.h"
#include "findfile.h"
#include "gstrings.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class FDelayedCommand
{
public:
	virtual ~FDelayedCommand() {}

protected:
	virtual bool Tick() = 0;

	friend class FDelayedCommandQueue;
};

class FWaitingCommand : public FDelayedCommand
{
public:
	FWaitingCommand(const char *cmd, int tics, bool unsafe)
		: Command(cmd), TicsLeft(tics+1), IsUnsafe(unsafe)
	{}

	bool Tick() override
	{
		if (--TicsLeft == 0)
		{
			UnsafeExecutionScope scope(IsUnsafe);
			AddCommandString(Command);
			return true;
		}
		return false;
	}

	FString Command;
	int TicsLeft;
	bool IsUnsafe;
};

class FStoredCommand : public FDelayedCommand
{
public:
	FStoredCommand(FConsoleCommand *com, const char *cmd)
		: Command(com), Text(cmd)
	{}

	bool Tick() override
	{
		if (Text.IsNotEmpty() && Command != nullptr)
		{
			FCommandLine args(Text);
			Command->Run(args, 0);
		}
		return true;
	}

private:

	FConsoleCommand *Command;
	FString Text;
};

class FDelayedCommandQueue
{
	TDeletingArray<FDelayedCommand *> delayedCommands;
public:
	void Run()
	{
		for (unsigned i = 0; i < delayedCommands.Size(); i++)
		{
			if (delayedCommands[i]->Tick())
			{
				delete delayedCommands[i];
				delayedCommands.Delete(i);
				i--;
			}
		}
	}

	void Clear()
	{
		delayedCommands.DeleteAndClear();
	}

	void AddCommand(FDelayedCommand * cmd)
	{
		delayedCommands.Push(cmd);
	}
};

static FDelayedCommandQueue delayedCommandQueue;

void C_RunDelayedCommands()
{
	delayedCommandQueue.Run();
}

void C_ClearDelayedCommands()
{
	delayedCommandQueue.Clear();
}



// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static FConsoleCommand *FindNameInHashTable (FConsoleCommand **table, const char *name, size_t namelen);
static FConsoleCommand *ScanChainForName (FConsoleCommand *start, const char *name, size_t namelen, FConsoleCommand **prev);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------
bool ParsingKeyConf, UnsafeExecutionContext;
FString StoredWarp;

FConsoleCommand* Commands[FConsoleCommand::HASH_SIZE];


// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *KeyConfCommands[] =
{
	"alias",
	"defaultbind",
	"addkeysection",
	"addmenukey",
	"addslotdefault",
	"weaponsection",
	"setslot",
	"addplayerclass",
	"clearplayerclasses"
};

// CODE --------------------------------------------------------------------

void C_DoCommand (const char *cmd, int keynum)
{
	FConsoleCommand *com;
	const char *end;
	const char *beg;

	// Skip any beginning whitespace
	while (*cmd > 0 && *cmd <= ' ')
		cmd++;

	// Find end of the command name
	if (*cmd == '\"')
	{
		for (end = beg = cmd+1; *end && *end != '\"'; ++end)
			;
	}
	else
	{
		beg = cmd;
		for (end = cmd+1; *end > ' ' || *end < 0; ++end)
			;
	}

	const size_t len = end - beg;

	if (ParsingKeyConf)
	{
		int i;

		for (i = countof(KeyConfCommands)-1; i >= 0; --i)
		{
			if (strnicmp (beg, KeyConfCommands[i], len) == 0 &&
				KeyConfCommands[i][len] == 0)
			{
				break;
			}
		}
		if (i < 0)
		{
			Printf ("Invalid command for KEYCONF: %s\n", beg);
			return;
		}
	}

	// Check if this is an action
	if (*beg == '+' || *beg == '-')
	{
		auto button = buttonMap.FindButton(beg + 1, int(end - beg - 1));
		if (button != nullptr)
		{
			if (*beg == '+')
			{
				button->PressKey (keynum);
				if (button->PressHandler) button->PressHandler();
			}
			else
			{
				button->ReleaseKey (keynum);
				if (button->ReleaseHandler) button->ReleaseHandler();
			}
			return;
		}
	}
	
	// Parse it as a normal command
	// Checking for matching commands follows this search order:
	//	1. Check the Commands[] hash table
	//	2. Check the CVars list

	if ( (com = FindNameInHashTable (Commands, beg, len)) )
	{
		if (gamestate != GS_STARTUP || ParsingKeyConf ||
			(len == 3 && strnicmp (beg, "set", 3) == 0) ||
			(len == 7 && strnicmp (beg, "logfile", 7) == 0) ||
			(len == 9 && strnicmp (beg, "unbindall", 9) == 0) ||
			(len == 4 && strnicmp (beg, "bind", 4) == 0) ||
			(len == 4 && strnicmp (beg, "exec", 4) == 0) ||
			(len ==10 && strnicmp (beg, "doublebind", 10) == 0) ||
			(len == 6 && strnicmp (beg, "pullin", 6) == 0)
			)
		{
			FCommandLine args (beg);
			com->Run (args, keynum);
		}
		else
		{
			if (len == 4 && strnicmp(beg, "warp", 4) == 0)
			{
				StoredWarp = beg;
			}
			else
			{
				auto cmd = new FStoredCommand(com, beg);
				delayedCommandQueue.AddCommand(cmd);
			}
		}
	}
	else
	{ // Check for any console vars that match the command
		FBaseCVar *var = FindCVarSub (beg, int(len));

		if (var != NULL)
		{
			FCommandLine args (beg);

			if (args.argc() >= 2)
			{ // Set the variable
				var->CmdSet (args[1]);
			}
			else
			{ // Get the variable's value
				if (var->GetDescription().Len()) Printf("%s\n", GStrings.localize(var->GetDescription()));
				Printf ("\"%s\" is \"%s\" ", var->GetName(), var->GetHumanString());
				Printf ("(default: \"%s\")\n", var->GetHumanStringDefault());
			}
		}
		else
		{ // We don't know how to handle this command
			Printf ("Unknown command \"%.*s\"\n", (int)len, beg);
		}
	}
}

void AddCommandString (const char *text, int keynum)
{
	// Operate on a local copy instead of messing around with the data that's being passed in here.
	TArray<char> buffer(strlen(text) + 1, true);
	memcpy(buffer.Data(), text, buffer.Size());
	char *cmd = buffer.Data();
	char *brkpt;
	int more;

	if (cmd)
	{
		while (*cmd)
		{
			brkpt = cmd;
			while (*brkpt != ';' && *brkpt != '\0')
			{
				if (*brkpt == '\"')
				{
					brkpt++;
					while (*brkpt != '\0' && (*brkpt != '\"' || *(brkpt-1) == '\\'))
						brkpt++;
				}
				if (*brkpt != '\0')
					brkpt++;
			}
			if (*brkpt == ';')
			{
				*brkpt = '\0';
				more = 1;
			}
			else
			{
				more = 0;
			}
			// Intercept wait commands here. Note: wait must be lowercase
			while (*cmd > 0 && *cmd <= ' ')
				cmd++;
			if (*cmd)
			{
				if (!ParsingKeyConf &&
					cmd[0] == 'w' && cmd[1] == 'a' && cmd[2] == 'i' && cmd[3] == 't' &&
					(cmd[4] == 0 || cmd[4] == ' '))
				{
					int tics;

					if (cmd[4] == ' ')
					{
						tics = (int)strtoll (cmd + 5, NULL, 0);
					}
					else
					{
						tics = 1;
					}
					if (tics > 0)
					{
						if (more)
						{ // The remainder of the command will be executed later
						  // Note that deferred commands lose track of which key
						  // (if any) they were pressed from.
							*brkpt = ';';
							auto cmd = new FWaitingCommand(brkpt, tics, UnsafeExecutionContext);
							delayedCommandQueue.AddCommand(cmd);
						}
						return;
					}
				}
				else
				{
					C_DoCommand (cmd, keynum);
				}
			}
			if (more)
			{
				*brkpt = ';';
			}
			cmd = brkpt + more;
		}
	}
}

static FConsoleCommand *ScanChainForName (FConsoleCommand *start, const char *name, size_t namelen, FConsoleCommand **prev)
{
	int comp;

	*prev = NULL;
	while (start)
	{
		comp = strnicmp (start->m_Name, name, namelen);
		if (comp > 0)
			return NULL;
		else if (comp == 0 && start->m_Name[namelen] == 0)
			return start;

		*prev = start;
		start = start->m_Next;
	}
	return NULL;
}

static FConsoleCommand *FindNameInHashTable (FConsoleCommand **table, const char *name, size_t namelen)
{
	FConsoleCommand *dummy;

	return ScanChainForName (table[MakeKey (name, namelen) % FConsoleCommand::HASH_SIZE], name, namelen, &dummy);
}

bool FConsoleCommand::AddToHash (FConsoleCommand **table)
{
	unsigned int key;
	FConsoleCommand *insert, **bucket;

	key = MakeKey (m_Name);
	bucket = &table[key % HASH_SIZE];

	if (ScanChainForName (*bucket, m_Name, strlen (m_Name), &insert))
	{
		return false;
	}
	else
	{
		if (insert)
		{
			m_Next = insert->m_Next;
			if (m_Next)
				m_Next->m_Prev = &m_Next;
			insert->m_Next = this;
			m_Prev = &insert->m_Next;
		}
		else
		{
			m_Next = *bucket;
			*bucket = this;
			m_Prev = bucket;
			if (m_Next)
				m_Next->m_Prev = &m_Next;
		}
	}
	return true;
}

FConsoleCommand* FConsoleCommand::FindByName (const char* name)
{
	return FindNameInHashTable (Commands, name, strlen (name));
}

FConsoleCommand::FConsoleCommand (const char *name, CCmdRun runFunc)
	: m_RunFunc (runFunc)
{
	int ag = strcmp (name, "kill");
	if (ag == 0)
		ag=0;
	m_Name = name;

	if (!AddToHash (Commands))
		Printf ("Adding CCMD %s twice.\n", name);
	else
		C_AddTabCommand (name);
}

FConsoleCommand::~FConsoleCommand ()
{
	*m_Prev = m_Next;
	if (m_Next)
		m_Next->m_Prev = m_Prev;
	C_RemoveTabCommand (m_Name);
}

void FConsoleCommand::Run(FCommandLine &argv, int key)
{
	m_RunFunc (argv, key);
}

void FUnsafeConsoleCommand::Run(FCommandLine &args, int key)
{
	if (UnsafeExecutionContext)
	{
		Printf(TEXTCOLOR_RED "Cannot execute unsafe command " TEXTCOLOR_GOLD "%s\n", m_Name.GetChars());
		return;
	}

	FConsoleCommand::Run (args, key);
}

FConsoleAlias::FConsoleAlias (const char *name, const char *command, bool noSave)
	: FConsoleCommand (name, NULL),
	  bRunning(false), bKill(false)
{
	m_Command[noSave] = command;
	m_Command[!noSave] = FString();
	// If the command contains % characters, assume they are parameter markers
	// for substitution when the command is executed.
	bDoSubstitution = (strchr (command, '%') != NULL);
}

FConsoleAlias::~FConsoleAlias ()
{
	m_Command[1] = m_Command[0] = FString();
}

// Given an argument vector, reconstitute the command line it could have been produced from.
FString BuildString (int argc, FString *argv)
{
	if (argc == 1)
	{
		return *argv;
	}
	else
	{
		FString buf;
		int arg;

		for (arg = 0; arg < argc; arg++)
		{
			if (argv[arg][0] == '\0')
			{ // It's an empty argument, we need to convert it to '""'
				buf << "\"\" ";
			}
			else if (strchr(argv[arg], '"'))
			{ // If it contains one or more quotes, we need to escape them.
				buf << '"';
				long substr_start = 0, quotepos;
				while ((quotepos = argv[arg].IndexOf('"', substr_start)) >= 0)
				{
					if (substr_start < quotepos)
					{
						buf << argv[arg].Mid(substr_start, quotepos - substr_start);
					}
					buf << "\\\"";
					substr_start = quotepos + 1;
				}
				buf << argv[arg].Mid(substr_start) << "\" ";
			}
			else if (strchr(argv[arg], ' '))
			{ // If it contains a space, it needs to be quoted.
				buf << '"' << argv[arg] << "\" ";
			}
			else
			{
				buf << argv[arg] << ' ';
			}
		}
		return buf;
	}
}

//===========================================================================
//
// SubstituteAliasParams
//
// Given an command line and a set of arguments, replace instances of
// %x or %{x} in the command line with argument x. If argument x does not
// exist, then the empty string is substituted in its place.
//
// Substitution is not done inside of quoted strings, unless that string is
// prepended with a % character.
//
// To avoid a substitution, use %%. The %% will be replaced by a single %.
//
//===========================================================================

void FConsoleCommand::PrintCommand()
{
	Printf("%s\n", m_Name.GetChars());
}

FString SubstituteAliasParams (FString &command, FCommandLine &args)
{
	// Do substitution by replacing %x with the argument x.
	// If there is no argument x, then %x is simply removed.

	// For some reason, strtoul's stop parameter is non-const.
	char *p = command.LockBuffer(), *start = p;
	unsigned long argnum;
	FString buf;
	bool inquote = false;

	while (*p != '\0')
	{
		if (p[0] == '%' && ((p[1] >= '0' && p[1] <= '9') || p[1] == '{'))
		{
			// Do a substitution. Output what came before this.
			buf.AppendCStrPart (start, p - start);

			// Extract the argument number and substitute the corresponding argument.
			argnum = strtoul (p + 1 + (p[1] == '{'), &start, 10);
			if ((p[1] != '{' || *start == '}') && argnum < (unsigned long)args.argc())
			{
				buf += args[argnum];
			}
			p = (start += (p[1] == '{' && *start == '}'));
		}
		else if (p[0] == '%' && p[1] == '%')
		{
			// Do not substitute. Just collapse to a single %.
			buf.AppendCStrPart (start, p - start + 1);
			start = p = p + 2;
			continue;
		}
		else if (p[0] == '%' && p[1] == '"')
		{
			// Collapse %" to " and remember that we're in a quote so when we
			// see a " character again, we don't start skipping below.
			if (!inquote)
			{
				inquote = true;
				buf.AppendCStrPart(start, p - start);
				start = p + 1;
			}
			else
			{
				inquote = false;
			}
			p += 2;
		}
		else if (p[0] == '\\' && p[1] == '"')
		{
			p += 2;
		}
		else if (p[0] == '"')
		{
			// Don't substitute inside quoted strings if it didn't start
			// with a %"
			if (!inquote)
			{
				p++;
				while (*p != '\0' && (*p != '"' || *(p-1) == '\\'))
					p++;
				if (*p != '\0')
					p++;
			}
			else
			{
				inquote = false;
				p++;
			}
		}
		else
		{
			p++;
		}
	}
	// Return whatever was after the final substitution.
	if (p > start)
	{
		buf.AppendCStrPart (start, p - start);
	}
	command.UnlockBuffer();

	return buf;
}

static int DumpHash (FConsoleCommand **table, bool aliases, const char *pattern=NULL)
{
	int bucket, count;
	FConsoleCommand *cmd;

	for (bucket = count = 0; bucket < FConsoleCommand::HASH_SIZE; bucket++)
	{
		cmd = table[bucket];
		while (cmd)
		{
			if (CheckWildcards (pattern, cmd->m_Name))
			{
				if (cmd->IsAlias())
				{
					if (aliases)
					{
						++count;
						static_cast<FConsoleAlias *>(cmd)->PrintAlias ();
					}
				}
				else if (!aliases)
				{
					++count;
					cmd->PrintCommand ();
				}
			}
			cmd = cmd->m_Next;
		}
	}
	return count;
}

void FConsoleAlias::PrintAlias ()
{
	if (m_Command[0].IsNotEmpty())
	{
		Printf (TEXTCOLOR_YELLOW "%s : %s\n", m_Name.GetChars(), m_Command[0].GetChars());
	}
	if (m_Command[1].IsNotEmpty())
	{
		Printf (TEXTCOLOR_ORANGE "%s : %s\n", m_Name.GetChars(), m_Command[1].GetChars());
	}
}

void FConsoleAlias::Archive (FConfigFile *f)
{
	if (f != NULL && !m_Command[0].IsEmpty())
	{
		f->SetValueForKey ("Name", m_Name, true);
		f->SetValueForKey ("Command", m_Command[0], true);
	}
}

void C_ArchiveAliases (FConfigFile *f)
{
	int bucket;
	FConsoleCommand *alias;

	for (bucket = 0; bucket < FConsoleCommand::HASH_SIZE; bucket++)
	{
		alias = Commands[bucket];
		while (alias)
		{
			if (alias->IsAlias())
				static_cast<FConsoleAlias *>(alias)->Archive (f);
			alias = alias->m_Next;
		}
	}
}

void C_ClearAliases ()
{
	int bucket;
	FConsoleCommand *alias;

	for (bucket = 0; bucket < FConsoleCommand::HASH_SIZE; bucket++)
	{
		alias = Commands[bucket];
		while (alias)
		{
			FConsoleCommand *next = alias->m_Next;
			if (alias->IsAlias())
				static_cast<FConsoleAlias *>(alias)->SafeDelete();
			alias = next;
		}
	}
}

CCMD(clearaliases)
{
	C_ClearAliases();
}


// This is called only by the ini parser.
void C_SetAlias (const char *name, const char *cmd)
{
	FConsoleCommand *prev, *alias, **chain;

	chain = &Commands[MakeKey (name) % FConsoleCommand::HASH_SIZE];
	alias = ScanChainForName (*chain, name, strlen (name), &prev);
	if (alias != NULL)
	{
		if (!alias->IsAlias ())
		{
			//Printf (PRINT_DEFAULT, PRINTF_BOLD, "%s is a command and cannot be an alias.\n", name);
			return;
		}
		delete alias;
	}
	new FConsoleAlias (name, cmd, false);
}

CCMD (alias)
{
	FConsoleCommand *prev, *alias, **chain;

	if (argv.argc() == 1)
	{
		Printf ("Current alias commands:\n");
		DumpHash (Commands, true);
	}
	else
	{
		chain = &Commands[MakeKey (argv[1]) % FConsoleCommand::HASH_SIZE];

		if (argv.argc() == 2)
		{ // Remove the alias

			if ( (alias = ScanChainForName (*chain, argv[1], strlen (argv[1]), &prev)))
			{
				if (alias->IsAlias ())
				{
					static_cast<FConsoleAlias *> (alias)->SafeDelete ();
				}
				else
				{
					Printf ("%s is a normal command\n", alias->m_Name.GetChars());
				}
			}
		}
		else
		{ // Add/change the alias

			alias = ScanChainForName (*chain, argv[1], strlen (argv[1]), &prev);
			if (alias != NULL)
			{
				if (alias->IsAlias ())
				{
					static_cast<FConsoleAlias *> (alias)->Realias (argv[2], ParsingKeyConf);
				}
				else
				{
					Printf ("%s is a normal command\n", alias->m_Name.GetChars());
					alias = NULL;
				}
			}
			else if (ParsingKeyConf)
			{
				new FUnsafeConsoleAlias (argv[1], argv[2]);
			}
			else
			{
				new FConsoleAlias (argv[1], argv[2], false);
			}
		}
	}
}

CCMD (cmdlist)
{
	int count;
	const char *filter = (argv.argc() == 1 ? NULL : argv[1]);

	count = buttonMap.ListActionCommands (filter);
	count += DumpHash (Commands, false, filter);
	Printf ("%d commands\n", count);
}

CCMD (key)
{
	if (argv.argc() > 1)
	{
		int i;

		for (i = 1; i < argv.argc(); ++i)
		{
			unsigned int key = MakeKey (argv[i]);
			Printf (" 0x%08x\n", key);
		}
	}
}

// Execute any console commands specified on the command line.
// These all begin with '+' as opposed to '-'.
FExecList *C_ParseCmdLineParams(FExecList *exec)
{
	for (int currArg = 1; currArg < Args->NumArgs(); )
	{
		if (*Args->GetArg (currArg++) == '+')
		{
			FString cmdString;
			int cmdlen = 1;
			int argstart = currArg - 1;

			while (currArg < Args->NumArgs())
			{
				if (*Args->GetArg (currArg) == '-' || *Args->GetArg (currArg) == '+')
					break;
				currArg++;
				cmdlen++;
			}

			cmdString = BuildString (cmdlen, Args->GetArgList (argstart));
			if (!cmdString.IsEmpty())
			{
				if (exec == NULL)
				{
					exec = new FExecList;
				}
				exec->AddCommand(&cmdString[1]);
			}
		}
	}
	return exec;
}

bool FConsoleCommand::IsAlias ()
{
	return false;
}

bool FConsoleAlias::IsAlias ()
{
	return true;
}

void FConsoleAlias::Run (FCommandLine &args, int key)
{
	if (bRunning)
	{
		Printf ("Alias %s tried to recurse.\n", m_Name.GetChars());
		return;
	}

	int index = !m_Command[1].IsEmpty();
	FString savedcommand = m_Command[index], mycommand;
	m_Command[index] = FString();

	if (bDoSubstitution)
	{
		mycommand = SubstituteAliasParams (savedcommand, args);
	}
	else
	{
		mycommand = savedcommand;
	}

	bRunning = true;
	AddCommandString (mycommand, key);
	bRunning = false;
	if (m_Command[index].IsEmpty())
	{ // The alias is unchanged, so put the command back so it can be used again.
	  // If the command had been non-empty, then that means that executing this
	  // alias caused it to realias itself, so the old command will be forgotten
	  // once this function returns.
		m_Command[index] = savedcommand;
	}
	if (bKill)
	{ // The alias wants to remove itself
		delete this;
	}
}

void FConsoleAlias::Realias (const char *command, bool noSave)
{
	if (!noSave && !m_Command[1].IsEmpty())
	{
		noSave = true;
	}
	m_Command[noSave] = command;

	// If the command contains % characters, assume they are parameter markers
	// for substitution when the command is executed.
	bDoSubstitution = (strchr (command, '%') != NULL);
	bKill = false;
}

void FConsoleAlias::SafeDelete ()
{
	if (!bRunning)
	{
		delete this;
	}
	else
	{
		bKill = true;
	}
}

void FUnsafeConsoleAlias::Run (FCommandLine &args, int key)
{
	UnsafeExecutionScope scope;
	FConsoleAlias::Run(args, key);
}

void FExecList::AddCommand(const char *cmd, const char *file)
{
	// Pullins are special and need to be separated from general commands.
	// They also turned out to be a really bad idea, since they make things
	// more complicated. :(
	if (file != NULL && strnicmp(cmd, "pullin", 6) == 0 && isspace(cmd[6]))
	{
		FCommandLine line(cmd);
		C_SearchForPullins(this, file, line);
	}
	// Recursive exec: Parse this file now.
	else if (strnicmp(cmd, "exec", 4) == 0 && isspace(cmd[4]))
	{
		FCommandLine argv(cmd);
		for (int i = 1; i < argv.argc(); ++i)
		{
			C_ParseExecFile(argv[i], this);
		}
	}
	else
	{
		Commands.Push(cmd);
	}
}

void FExecList::ExecCommands() const
{
	for (unsigned i = 0; i < Commands.Size(); ++i)
	{
		AddCommandString(Commands[i]);
	}
}

void FExecList::AddPullins(TArray<FString> &wads, FConfigFile *config) const
{
	for (unsigned i = 0; i < Pullins.Size(); ++i)
	{
		D_AddFile(wads, Pullins[i], true, -1, config);
	}
}

FExecList *C_ParseExecFile(const char *file, FExecList *exec)
{
	char cmd[4096];
	int retval = 0;

	FileReader fr;

	if ( (fr.OpenFile(file)) )
	{
		while (fr.Gets(cmd, countof(cmd)-1))
		{
			// Comments begin with //
			char *stop = cmd + strlen(cmd) - 1;
			char *comment = cmd;
			int inQuote = 0;

			if (*stop == '\n')
				*stop-- = 0;

			while (comment < stop)
			{
				if (*comment == '\"')
				{
					inQuote ^= 1;
				}
				else if (!inQuote && *comment == '/' && *(comment + 1) == '/')
				{
					break;
				}
				comment++;
			}
			if (comment == cmd)
			{ // Comment at line beginning
				continue;
			}
			else if (comment < stop)
			{ // Comment in middle of line
				*comment = 0;
			}
			if (exec == NULL)
			{
				exec = new FExecList;
			}
			exec->AddCommand(cmd, file);
		}
	}
	else
	{
		Printf ("Could not open \"%s\"\n", file);
	}
	return exec;
}

bool C_ExecFile (const char *file)
{
	FExecList *exec = C_ParseExecFile(file, NULL);
	if (exec != NULL)
	{
		exec->ExecCommands();
		if (exec->Pullins.Size() > 0)
		{
			Printf(TEXTCOLOR_BOLD "Notice: Pullin files were ignored.\n");
		}
		delete exec;
	}
	return exec != NULL;
}

void C_SearchForPullins(FExecList *exec, const char *file, FCommandLine &argv)
{
	const char *lastSlash;

	assert(exec != NULL);
	assert(file != NULL);
#ifdef __unix__
	lastSlash = strrchr(file, '/');
#else
	const char *lastSlash1, *lastSlash2;

	lastSlash1 = strrchr(file, '/');
	lastSlash2 = strrchr(file, '\\');
	lastSlash = MAX(lastSlash1, lastSlash2);
#endif

	for (int i = 1; i < argv.argc(); ++i)
	{
		// Try looking for the wad in the same directory as the .cfg
		// before looking for it in the current directory.
		if (lastSlash != NULL)
		{
			FString path(file, (lastSlash - file) + 1);
			path += argv[i];
			if (FileExists(path))
			{
				exec->Pullins.Push(path);
				continue;
			}
		}
		exec->Pullins.Push(argv[i]);
	}
}

static TArray<FConsoleCommand*> dynccmds; // This needs to be explicitly deleted before shutdown - the names in here may not be valid during the exit handler.
//
// C_RegisterFunction() -- dynamically register a CCMD.
//
int C_RegisterFunction(const char* pszName, const char* pszDesc, int (*func)(CCmdFuncPtr))
{
	FString nname = pszName;
	auto callback = [nname, pszDesc, func](FCommandLine& args, int key)
	{
		if (args.argc() > 0) args.operator[](0);
		CCmdFuncParm param = { args.argc() - 1, nname.GetChars(), (const char**)args._argv + 1, args.cmd };
		if (func(&param) != CCMD_OK && pszDesc)
		{
			Printf("%s\n", pszDesc);
		}
	};
	auto ccmd = new FConsoleCommand(pszName, callback);
	dynccmds.Push(ccmd);
	return 0;
}


void C_ClearDynCCmds()
{
	for (auto ccmd : dynccmds)
	{
		delete ccmd;
	}
	dynccmds.Clear();
}

CCMD (pullin)
{
	// Actual handling for pullin is now completely special-cased above
	Printf (TEXTCOLOR_BOLD "Pullin" TEXTCOLOR_NORMAL " is only valid from .cfg\n"
			"files and only when used at startup.\n");
}


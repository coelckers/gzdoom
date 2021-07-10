// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2000 Simon Howard
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// Preprocessor.
//
// The preprocessor must be called when the script is first loaded.
// It performs 2 functions:
//      1: blank out comments (which could be misinterpreted)
//      2: makes a list of all the sections held within {} braces
//      3: 'dry' runs the script: goes thru each statement and
//         sets the types of all the DFsSection's in the script
//      4: Saves locations of all goto() labels
//
// the system of DFsSection's is pretty horrible really, but it works
// and its probably the only way i can think of of saving scripts
// half-way thru running
//
// By Simon Howard
//
//---------------------------------------------------------------------------
//

/* includes ************************/

#include "t_script.h"
#include "filesystem.h"
#include "serializer.h"
#include "serialize_obj.h"
#include "g_levellocals.h"


//==========================================================================
//
// {} sections
//
// during preprocessing all of the {} sections
// are found. these are stored in a hash table
// according to their offset in the script. 
// functions here deal with creating new sections
// and finding them from a given offset.
//
//==========================================================================

IMPLEMENT_CLASS(DFsSection, false, true)

IMPLEMENT_POINTERS_START(DFsSection)
	IMPLEMENT_POINTER(next)
IMPLEMENT_POINTERS_END

//==========================================================================
//
//
//
//==========================================================================

void DFsSection::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("type", type)
		("start_index", start_index)
		("end_index", end_index)
		("loop_index", loop_index)
		("next", next);
}

//==========================================================================
//
//
//
//==========================================================================

char *DFsScript::SectionStart(const DFsSection *sec)
{
	return Data.Data() + sec->start_index;
}

//==========================================================================
//
//
//
//==========================================================================

char *DFsScript::SectionEnd(const DFsSection *sec)
{
	return Data.Data() + sec->end_index;
}

//==========================================================================
//
//
//
//==========================================================================

char *DFsScript::SectionLoop(const DFsSection *sec)
{
	return Data.Data() + sec->loop_index;
}

//==========================================================================
//
//
//
//==========================================================================

void DFsScript::ClearSections()
{
	for(int i=0;i<SECTIONSLOTS;i++)
	{
		DFsSection * var = sections[i];
		while(var)
		{
			DFsSection *next = var->next;
			var->Destroy();
			var = next;
		}
		sections[i] = nullptr;
	}
}

//==========================================================================
//
// create section
//
//==========================================================================

DFsSection *DFsScript::NewSection(const char *brace)
{
	int n = section_hash(brace);
	DFsSection *newsec = Create<DFsSection>();
	
	newsec->start_index = MakeIndex(brace);
	newsec->next = sections[n];
	sections[n] = newsec;
	GC::WriteBarrier(this, newsec);
	return newsec;
}

//==========================================================================
//
// find a Section from the location of the starting { brace
//
//==========================================================================

DFsSection *DFsScript::FindSectionStart(const char *brace)
{
	int n = section_hash(brace);
	DFsSection *current = sections[n];
	
	// use the hash table: check the appropriate hash chain
	
	while(current)
    {
		if(SectionStart(current) == brace) return current;
		current = current->next;
    }
	
	return NULL;    // not found
}


//==========================================================================
//
// find a Section from the location of the closing } brace
//
//==========================================================================

DFsSection *DFsScript::FindSectionEnd(const char *brace)
{
	int n;
	
	// hash table is no use, they are hashed according to
	// the offset of the starting brace
	
	// we have to go through every entry to find from the
	// ending brace
	
	for(n=0; n<SECTIONSLOTS; n++)      // check all sections in all chains
	{
		DFsSection *current = sections[n];
		
		while(current)
		{
			if(SectionEnd(current) == brace) return current;        // found it
			current = current->next;
		}
	}
	return NULL;    // not found
}

//==========================================================================
//
// preproocessor main loop
//
// This works by recursion. when a { opening
// brace is found, another instance of the
// function is called for the data inside
// the {} section.
// At the same time, the sections are noted
// down and hashed. Goto() labels are noted
// down, and comments are blanked out
//
//==========================================================================

char *DFsScript::ProcessFindChar(char *datap, char find)
{
	while(*datap)
    {
		if(*datap==find) return datap;
		if(*datap=='\"')       // found a quote: ignore stuff in it
		{
			datap++;
			while(*datap && *datap != '\"')
			{
				// escape sequence ?
				if(*datap=='\\') datap++;
				datap++;
			}
			// error: end of script in a constant
			if(!*datap) return NULL;
		}
		
		// comments: blank out
		
		if(*datap=='/' && *(datap+1)=='*')        // /* -- */ comment
		{
			while(*datap && (*datap != '*' || *(datap+1) != '/') )
			{
				*datap=' '; datap++;
			}
			if(*datap)
				*datap = *(datap+1) = ' ';   // blank the last bit
			else
			{
				// script terminated in comment
				script_error("script terminated inside comment\n");
			}
		}
		if(*datap=='/' && *(datap+1)=='/')        // // -- comment
		{
			while(*datap != '\n')
			{
				*datap=' '; datap++;       // blank out
			}
		}
		
		/********** labels ****************/

		// labels are also found during the
		// preprocessing. these are of the form
		//
		//      label_name:
		//
		// and are used for the goto function.
		// goto labels are stored as variables.

		if(*datap==':' && scriptnum != -1) // not in global scripts
		{
			char *labelptr = datap-1;
			
			while(!isop(*labelptr)) labelptr--;

			FString labelname(labelptr+1, strcspn(labelptr+1, ":"));
			
			if (labelname.Len() == 0)
			{
				Printf(PRINT_DEFAULT, PRINTF_BOLD,"Script %d: ':' encountrered in incorrect position!\n",scriptnum);
			}

			DFsVariable *newlabel = NewVariable(labelname, svt_label);
			newlabel->value.i = MakeIndex(labelptr);
		}
		
		if(*datap=='{')  // { -- } sections: add 'em
		{
			DFsSection *newsec = NewSection(datap);
			
			newsec->type = st_empty;
			// find the ending } and save
			char * theend = ProcessFindChar(datap+1, '}');
			if(!theend)
			{                // brace not found
				// This is fatal because it will cause a crash later
				// if the game isn't terminated.
				I_Error("Script %d: section error: no ending brace\n", scriptnum);
			}

			newsec->end_index = MakeIndex(theend);
			// continue from the end of the section
			datap = theend;
		}
		datap++;
    }
	return NULL;
}


//==========================================================================
//
// second stage parsing
//
// second stage preprocessing considers the script
// in terms of tokens rather than as plain data.
//
// we 'dry' run the script: go thru each statement and
// collect types for Sections
//
// this is an important thing to do, it cannot be done
// at runtime for 2 reasons:
//      1. gotos() jumping inside loops will pass thru
//         the end of the loop
//      2. savegames. loading a script saved inside a
//         loop will let it pass thru the loop
//
// this is basically a cut-down version of the normal
// parsing loop.
//
//==========================================================================

void DFsScript::DryRunScript(FLevelLocals *Level)
{
	char *end = Data.Data() + len;
	char *rover = Data.Data();
	
	// allocate space for the tokens
	FParser parse(Level, this);
	try
	{
		while(rover < end && *rover)
		{
			rover = parse.GetTokens(rover);
			
			if(!parse.NumTokens) continue;
			
			if(parse.Section && parse.TokenType[0] == function)
			{
				if(!strcmp(parse.Tokens[0], "if"))
				{
					parse.Section->type = st_if;
					continue;
				}
				else if(!strcmp(parse.Tokens[0], "elseif")) // haleyjd: SoM's else code
				{
					parse.Section->type = st_elseif;
					continue;
				}
				else if(!strcmp(parse.Tokens[0], "else"))
				{
					parse.Section->type = st_else;
					continue;
				}
				else if(!strcmp(parse.Tokens[0], "while") ||
					!strcmp(parse.Tokens[0], "for"))
				{
					parse.Section->type = st_loop;
					parse.Section->loop_index = MakeIndex(parse.LineStart);
					continue;
				}
			}
		}
	}
	catch (CFsError err)
	{
		parse.ErrorMessage(err.msg);
	}
}

//==========================================================================
//
// main preprocess function
//
//==========================================================================

void DFsScript::Preprocess(FLevelLocals *Level)
{
	len = (int)Data.Size() - 1;
	ProcessFindChar(Data.Data(), 0);  // fill in everything
	DryRunScript(Level);
}

//==========================================================================
//
// FraggleScript allows 'including' of other lumps.
// we divert input from the current script (normally
// levelscript) to a seperate lump. This of course
// first needs to be preprocessed to remove comments
// etc.
//
// parse an 'include' lump
//
//==========================================================================

void DFsScript::ParseInclude(FLevelLocals *Level, char *lumpname)
{
	int lumpnum;
	char *lump;
	
	if((lumpnum = fileSystem.CheckNumForName(lumpname)) == -1)
    {
		I_Error("include lump '%s' not found!\n", lumpname);
		return;
    }
	
	int lumplen=fileSystem.FileLength(lumpnum);
	lump=new char[lumplen+10];
	fileSystem.ReadFile(lumpnum,lump);
	
	lump[lumplen]=0;
	
	// preprocess the include
	// we assume that it does not include sections or labels or 
	// other nasty things
	ProcessFindChar(lump, 0);
	
	// now parse the lump
	FParser parse(Level, this);
	parse.Run(lump, lump, lump+lumplen);
	
	// free the lump
	delete[] lump;
}


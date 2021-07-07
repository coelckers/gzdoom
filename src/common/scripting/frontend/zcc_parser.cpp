/*
** zcc_expr.cpp
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
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

#include "dobject.h"
#include "sc_man.h"
#include "filesystem.h"
#include "cmdlib.h"
#include "m_argv.h"
#include "v_text.h"
#include "version.h"
#include "zcc_parser.h"
#include "zcc_compile.h"
#include "templates.h"

TArray<FString> Includes;
TArray<FScriptPosition> IncludeLocs;

static FString ZCCTokenName(int terminal);
void AddInclude(ZCC_ExprConstant *node)
{
	assert(node->Type == TypeString);
	if (Includes.Find(*node->StringVal) >= Includes.Size())
	{
		Includes.Push(*node->StringVal);
		IncludeLocs.Push(*node);
	}
}

#include "zcc-parse.h"
#include "zcc-parse.c"

struct TokenMapEntry
{
	int16_t TokenType;
	uint16_t TokenName;
	TokenMapEntry(int16_t a, uint16_t b)
	: TokenType(a), TokenName(b)
	{ }
};
static TMap<int16_t, TokenMapEntry> TokenMap;
static int16_t BackTokenMap[YYERRORSYMBOL];	// YYERRORSYMBOL immediately follows the terminals described by the grammar

#define TOKENDEF2(sc, zcc, name)	{ TokenMapEntry tme(zcc, name); TokenMap.Insert(sc, tme); } BackTokenMap[zcc] = sc
#define TOKENDEF(sc, zcc)			TOKENDEF2(sc, zcc, NAME_None)

static void InitTokenMap()
{
	TOKENDEF ('=',				ZCC_EQ);
	TOKENDEF (TK_MulEq,			ZCC_MULEQ);
	TOKENDEF (TK_DivEq,			ZCC_DIVEQ);
	TOKENDEF (TK_ModEq,			ZCC_MODEQ);
	TOKENDEF (TK_AddEq,			ZCC_ADDEQ);
	TOKENDEF (TK_SubEq,			ZCC_SUBEQ);
	TOKENDEF (TK_LShiftEq,		ZCC_LSHEQ);
	TOKENDEF (TK_RShiftEq,		ZCC_RSHEQ);
	TOKENDEF (TK_URShiftEq,		ZCC_URSHEQ);
	TOKENDEF (TK_AndEq,			ZCC_ANDEQ);
	TOKENDEF (TK_OrEq,			ZCC_OREQ);
	TOKENDEF (TK_XorEq,			ZCC_XOREQ);
	TOKENDEF ('?',				ZCC_QUESTION);
	TOKENDEF (':',				ZCC_COLON);
	TOKENDEF ('@',				ZCC_ATSIGN);
	TOKENDEF (TK_OrOr,			ZCC_OROR);
	TOKENDEF (TK_AndAnd,		ZCC_ANDAND);
	TOKENDEF (TK_Eq,			ZCC_EQEQ);
	TOKENDEF (TK_Neq,			ZCC_NEQ);
	TOKENDEF (TK_ApproxEq,		ZCC_APPROXEQ);
	TOKENDEF ('<',				ZCC_LT);
	TOKENDEF ('>',				ZCC_GT);
	TOKENDEF (TK_Leq,			ZCC_LTEQ);
	TOKENDEF (TK_Geq,			ZCC_GTEQ);
	TOKENDEF (TK_LtGtEq,		ZCC_LTGTEQ);
	TOKENDEF (TK_Is,			ZCC_IS);
	TOKENDEF (TK_DotDot,		ZCC_DOTDOT);
	TOKENDEF (TK_Ellipsis,		ZCC_ELLIPSIS);
	TOKENDEF ('|',				ZCC_OR);
	TOKENDEF ('^',				ZCC_XOR);
	TOKENDEF ('&',				ZCC_AND);
	TOKENDEF (TK_LShift,		ZCC_LSH);
	TOKENDEF (TK_RShift,		ZCC_RSH);
	TOKENDEF (TK_URShift,		ZCC_URSH);
	TOKENDEF ('-',				ZCC_SUB);
	TOKENDEF ('+',				ZCC_ADD);
	TOKENDEF ('*',				ZCC_MUL);
	TOKENDEF ('/',				ZCC_DIV);
	TOKENDEF ('%',				ZCC_MOD);
	TOKENDEF (TK_Cross,			ZCC_CROSSPROD);
	TOKENDEF (TK_Dot,			ZCC_DOTPROD);
	TOKENDEF (TK_MulMul,		ZCC_POW);
	TOKENDEF (TK_Incr,			ZCC_ADDADD);
	TOKENDEF (TK_Decr,			ZCC_SUBSUB);
	TOKENDEF ('.',				ZCC_DOT);
	TOKENDEF ('(',				ZCC_LPAREN);
	TOKENDEF (')',				ZCC_RPAREN);
	TOKENDEF (TK_ColonColon,	ZCC_SCOPE);
	TOKENDEF (';',				ZCC_SEMICOLON);
	TOKENDEF (',',				ZCC_COMMA);
	TOKENDEF (TK_Class,			ZCC_CLASS);
	TOKENDEF (TK_Abstract,		ZCC_ABSTRACT);
	TOKENDEF (TK_Native,		ZCC_NATIVE);
	TOKENDEF (TK_Action,		ZCC_ACTION);
	TOKENDEF (TK_Replaces,		ZCC_REPLACES);
	TOKENDEF (TK_Static,		ZCC_STATIC);
	TOKENDEF (TK_Private,		ZCC_PRIVATE);
	TOKENDEF (TK_Protected,		ZCC_PROTECTED);
	TOKENDEF (TK_Latent,		ZCC_LATENT);
	TOKENDEF (TK_Virtual,		ZCC_VIRTUAL);
	TOKENDEF (TK_VarArg,        ZCC_VARARG);
	TOKENDEF (TK_UI,			ZCC_UI);
	TOKENDEF (TK_Play,			ZCC_PLAY);
	TOKENDEF (TK_ClearScope,	ZCC_CLEARSCOPE);
	TOKENDEF (TK_VirtualScope,  ZCC_VIRTUALSCOPE);
	TOKENDEF (TK_Override,		ZCC_OVERRIDE);
	TOKENDEF (TK_Final,			ZCC_FINAL);
	TOKENDEF (TK_Meta,			ZCC_META);
	TOKENDEF (TK_Deprecated,	ZCC_DEPRECATED);
	TOKENDEF (TK_Version,		ZCC_VERSION);
	TOKENDEF (TK_ReadOnly,		ZCC_READONLY);
	TOKENDEF (TK_Internal,		ZCC_INTERNAL);
	TOKENDEF ('{',				ZCC_LBRACE);
	TOKENDEF ('}',				ZCC_RBRACE);
	TOKENDEF (TK_Struct,		ZCC_STRUCT);
	TOKENDEF (TK_Property,		ZCC_PROPERTY);
	TOKENDEF (TK_FlagDef,		ZCC_FLAGDEF);
	TOKENDEF (TK_Mixin,			ZCC_MIXIN);
	TOKENDEF (TK_Transient,		ZCC_TRANSIENT);
	TOKENDEF (TK_Enum,			ZCC_ENUM);
	TOKENDEF2(TK_SByte,			ZCC_SBYTE,		NAME_sByte);
	TOKENDEF2(TK_Byte,			ZCC_BYTE,		NAME_Byte);
	TOKENDEF2(TK_Short,			ZCC_SHORT,		NAME_Short);
	TOKENDEF2(TK_UShort,		ZCC_USHORT,		NAME_uShort);
	TOKENDEF2(TK_Int8,			ZCC_SBYTE,		NAME_int8);
	TOKENDEF2(TK_UInt8,			ZCC_BYTE,		NAME_uint8);
	TOKENDEF2(TK_Int16,			ZCC_SHORT,		NAME_int16);
	TOKENDEF2(TK_UInt16,		ZCC_USHORT,		NAME_uint16);
	TOKENDEF2(TK_Int,			ZCC_INT,		NAME_Int);
	TOKENDEF2(TK_UInt,			ZCC_UINT,		NAME_uInt);
	TOKENDEF2(TK_Bool,			ZCC_BOOL,		NAME_Bool);
	TOKENDEF2(TK_Float,			ZCC_FLOAT,		NAME_Float);
	TOKENDEF2(TK_Double,		ZCC_DOUBLE,		NAME_Double);
	TOKENDEF2(TK_String,		ZCC_STRING,		NAME_String);
	TOKENDEF2(TK_Vector2,		ZCC_VECTOR2,	NAME_Vector2);
	TOKENDEF2(TK_Vector3,		ZCC_VECTOR3,	NAME_Vector3);
	TOKENDEF2(TK_Name,			ZCC_NAME,		NAME_Name);
	TOKENDEF2(TK_Map,			ZCC_MAP,		NAME_Map);
	TOKENDEF2(TK_Array,			ZCC_ARRAY,		NAME_Array);
	TOKENDEF2(TK_Include,		ZCC_INCLUDE,	NAME_Include);
	TOKENDEF (TK_Void,			ZCC_VOID);
	TOKENDEF (TK_True,			ZCC_TRUE);
	TOKENDEF (TK_False,			ZCC_FALSE);
	TOKENDEF ('[',				ZCC_LBRACKET);
	TOKENDEF (']',				ZCC_RBRACKET);
	TOKENDEF (TK_In,			ZCC_IN);
	TOKENDEF (TK_Out,			ZCC_OUT);
	TOKENDEF (TK_Super,			ZCC_SUPER);
	TOKENDEF (TK_Null,			ZCC_NULLPTR);
	TOKENDEF ('~',				ZCC_TILDE);
	TOKENDEF ('!',				ZCC_BANG);
	TOKENDEF (TK_SizeOf,		ZCC_SIZEOF);
	TOKENDEF (TK_AlignOf,		ZCC_ALIGNOF);
	TOKENDEF (TK_Continue,		ZCC_CONTINUE);
	TOKENDEF (TK_Break,			ZCC_BREAK);
	TOKENDEF (TK_Return,		ZCC_RETURN);
	TOKENDEF (TK_Do,			ZCC_DO);
	TOKENDEF (TK_For,			ZCC_FOR);
	TOKENDEF (TK_While,			ZCC_WHILE);
	TOKENDEF (TK_Until,			ZCC_UNTIL);
	TOKENDEF (TK_If,			ZCC_IF);
	TOKENDEF (TK_Else,			ZCC_ELSE);
	TOKENDEF (TK_Switch,		ZCC_SWITCH);
	TOKENDEF (TK_Case,			ZCC_CASE);
	TOKENDEF2(TK_Default,		ZCC_DEFAULT,	NAME_Default);
	TOKENDEF (TK_Const,			ZCC_CONST);
	TOKENDEF (TK_Stop,			ZCC_STOP);
	TOKENDEF (TK_Wait,			ZCC_WAIT);
	TOKENDEF (TK_Fail,			ZCC_FAIL);
	TOKENDEF (TK_Loop,			ZCC_LOOP);
	TOKENDEF (TK_Goto,			ZCC_GOTO);
	TOKENDEF (TK_States,		ZCC_STATES);
	TOKENDEF2(TK_State,			ZCC_STATE,		NAME_State);
	TOKENDEF2(TK_Color,			ZCC_COLOR,		NAME_Color);
	TOKENDEF2(TK_Sound,			ZCC_SOUND,		NAME_Sound);
	TOKENDEF2(TK_Let,			ZCC_LET,		NAME_let);
	TOKENDEF2(TK_StaticConst,	ZCC_STATICCONST,NAME_Staticconst);

	TOKENDEF (TK_Identifier,	ZCC_IDENTIFIER);
	TOKENDEF (TK_StringConst,	ZCC_STRCONST);
	TOKENDEF (TK_NameConst,		ZCC_NAMECONST);
	TOKENDEF (TK_IntConst,		ZCC_INTCONST);
	TOKENDEF (TK_UIntConst,		ZCC_UINTCONST);
	TOKENDEF (TK_FloatConst,	ZCC_FLOATCONST);
	TOKENDEF (TK_NonWhitespace,	ZCC_NWS);

	TOKENDEF (TK_Bright,		ZCC_BRIGHT);
	TOKENDEF (TK_Slow,			ZCC_SLOW);
	TOKENDEF (TK_Fast,			ZCC_FAST);
	TOKENDEF (TK_NoDelay,		ZCC_NODELAY);
	TOKENDEF (TK_Offset,		ZCC_OFFSET);
	TOKENDEF (TK_CanRaise,		ZCC_CANRAISE);
	TOKENDEF (TK_Light,			ZCC_LIGHT);
	TOKENDEF (TK_Extend,		ZCC_EXTEND);
}
#undef TOKENDEF
#undef TOKENDEF2

//**--------------------------------------------------------------------------

static void ParseSingleFile(FScanner *pSC, const char *filename, int lump, void *parser, ZCCParseState &state)
{
	int tokentype;
	//bool failed;
	ZCCToken value;
	FScanner lsc;

	if (pSC == nullptr)
	{
		if (filename != nullptr)
		{
			lump = fileSystem.CheckNumForFullName(filename, true);
			if (lump >= 0)
			{
				lsc.OpenLumpNum(lump);
			}
			else
			{
				Printf("Could not find script lump '%s'\n", filename);
				return;
			}
		}
		else lsc.OpenLumpNum(lump);

		pSC = &lsc;
	}
	FScanner &sc = *pSC;
	sc.SetParseVersion(state.ParseVersion);
	state.sc = &sc;

	while (sc.GetToken())
	{
		value.Largest = 0;
		value.SourceLoc = sc.GetMessageLine();
		switch (sc.TokenType)
		{
		case TK_StringConst:
			value.String = state.Strings.Alloc(sc.String, sc.StringLen);
			tokentype = ZCC_STRCONST;
			break;

		case TK_NameConst:
			value.Int = FName(sc.String).GetIndex();
			tokentype = ZCC_NAMECONST;
			break;

		case TK_IntConst:
			value.Int = sc.Number;
			tokentype = ZCC_INTCONST;
			break;

		case TK_UIntConst:
			value.Int = sc.Number;
			tokentype = ZCC_UINTCONST;
			break;

		case TK_FloatConst:
			value.Float = sc.Float;
			tokentype = ZCC_FLOATCONST;
			break;

		case TK_None:	// 'NONE' is a token for SBARINFO but not here.
		case TK_Identifier:
			value.Int = FName(sc.String).GetIndex();
			tokentype = ZCC_IDENTIFIER;
			break;

		case TK_NonWhitespace:
			value.Int = FName(sc.String).GetIndex();
			tokentype = ZCC_NWS;
			break;

		case TK_Static:
			sc.MustGetAnyToken();
			// The oh so wonderful grammar has problems with the 'const' token thanks to the overly broad rule for constants,
			// which effectively prevents use of this token nearly anywhere else. So in order to get 'static const' through
			// on the class/struct level we have to muck around with the token type here so that both words get combined into 
			// a single token that doesn't make the grammar throw a fit.
			if (sc.TokenType == TK_Const)
			{
				tokentype = ZCC_STATICCONST;
				value.Int = NAME_Staticconst;
			}
			else
			{
				tokentype = ZCC_STATIC;
				value.Int = NAME_Static;
				sc.UnGet();
			}
			break;

		default:
			TokenMapEntry *zcctoken = TokenMap.CheckKey(sc.TokenType);
			if (zcctoken != nullptr)
			{
				tokentype = zcctoken->TokenType;
				value.Int = zcctoken->TokenName;
			}
			else
			{
				sc.ScriptMessage("Unexpected token %s.\n", sc.TokenName(sc.TokenType).GetChars());
				goto parse_end;
			}
			break;
		}
		ZCCParse(parser, tokentype, value, &state);
	}
parse_end:
	value.Int = -1;
	ZCCParse(parser, ZCC_EOF, value, &state);
	state.sc = nullptr;
}

//**--------------------------------------------------------------------------

PNamespace *ParseOneScript(const int baselump, ZCCParseState &state)
{
	FScanner sc;
	void *parser;
	ZCCToken value;
	int lumpnum = baselump;
	auto fileno = fileSystem.GetFileContainer(lumpnum);

	if (TokenMap.CountUsed() == 0)
	{
		InitTokenMap();
	}

	parser = ZCCParseAlloc(malloc);

#ifndef NDEBUG
	FILE *f = nullptr;
	const char *tracefile = Args->CheckValue("-tracefile");
	if (tracefile != nullptr)
	{
		f = fopen(tracefile, "w");
		char prompt = '\0';
		ZCCParseTrace(f, &prompt);
	}
#endif

	sc.OpenLumpNum(lumpnum);
	sc.SetParseVersion({ 2, 4 });	// To get 'version' we need parse version 2.4 for the initial test
	auto saved = sc.SavePos();

	if (sc.GetToken())
	{
		if (sc.TokenType == TK_Version)
		{
			char *endp;
			sc.MustGetString();
			state.ParseVersion.major = (int16_t)clamp<unsigned long long>(strtoull(sc.String, &endp, 10), 0, USHRT_MAX);
			if (*endp != '.')
			{
				sc.ScriptError("Bad version directive");
			}
			state.ParseVersion.minor = (int16_t)clamp<unsigned long long>(strtoll(endp + 1, &endp, 10), 0, USHRT_MAX);
			if (*endp == '.')
			{
				state.ParseVersion.revision = (int16_t)clamp<unsigned long long>(strtoll(endp + 1, &endp, 10), 0, USHRT_MAX);
			}
			else state.ParseVersion.revision = 0;
			if (*endp != 0)
			{
				sc.ScriptError("Bad version directive");
			}
			if (state.ParseVersion.major == USHRT_MAX || state.ParseVersion.minor == USHRT_MAX || state.ParseVersion.revision == USHRT_MAX)
			{
				sc.ScriptError("Bad version directive");
			}
			if (state.ParseVersion > MakeVersion(VER_MAJOR, VER_MINOR, VER_REVISION))
			{
				sc.ScriptError("The file you are attempting to run requires a newer version of " GAMENAME ".\n\nA version with ZScript version %d.%d.%d is required, but your copy of " GAMENAME " only supports %d.%d.%d. Please upgrade!", state.ParseVersion.major, state.ParseVersion.minor, state.ParseVersion.revision, VER_MAJOR, VER_MINOR, VER_REVISION);
			}
		}
		else
		{
			state.ParseVersion = MakeVersion(2, 3);	// 2.3 is the first version of ZScript.
			sc.RestorePos(saved);
		}
	}

	ParseSingleFile(&sc, nullptr, lumpnum, parser, state);
	for (unsigned i = 0; i < Includes.Size(); i++)
	{
		lumpnum = fileSystem.CheckNumForFullName(Includes[i], true);
		if (lumpnum == -1 && ( Includes[i].IndexOf("./") == 0 || Includes[i].IndexOf("../") == 0 ) ) // relative path resolving
		{
			FString fullPath = IncludeLocs[i].FileName.GetChars(); // get full path, format 'wad:filepath/filename'
			
			long start = fullPath.IndexOf(":"); // find first ':'

			long end = fullPath.LastIndexOf("/"); // find last '/'

			if (start!=-1&&end!=-1)
			{
				FString resolvedPath = fullPath.Mid(start + 1, end - start - 1); // extract filepath from string
				FString relativePath = Includes[i];
				if ( relativePath.IndexOf("./") == 0 ) // strip initial marker
				{
					relativePath = relativePath.Mid(2);
				}
				
				bool pathOk = true;

				while (relativePath.IndexOf("../") == 0) // go back one folder for each '..'
				{
					relativePath = relativePath.Mid(3);
					long slash_index = resolvedPath.LastIndexOf("/");
					if (slash_index != -1) {
						resolvedPath = resolvedPath.Mid(0,slash_index);
					} else {
						pathOk = false;
						break;
					}
				}
				if ( pathOk ) // if '..' parsing was successful
				{
					resolvedPath += "/" + relativePath; // add relative path

					lumpnum = fileSystem.CheckNumForFullName(resolvedPath, true); // check for relative include
				}
			}

		}
		if (lumpnum == -1)
		{
			IncludeLocs[i].Message(MSG_ERROR, "Include script lump %s not found", Includes[i].GetChars());
		}
		else
		{
			auto fileno2 = fileSystem.GetFileContainer(lumpnum);
			if (fileno == 0 && fileno2 != 0)
			{
				I_FatalError("File %s is overriding core lump %s.",
					fileSystem.GetResourceFileFullName(fileSystem.GetFileContainer(lumpnum)), Includes[i].GetChars());
			}

			ParseSingleFile(nullptr, nullptr, lumpnum, parser, state);
		}
	}
	Includes.Clear();
	Includes.ShrinkToFit();
	IncludeLocs.Clear();
	IncludeLocs.ShrinkToFit();

	value.Int = -1;
	value.SourceLoc = sc.GetMessageLine();
	ZCCParse(parser, 0, value, &state);
	ZCCParseFree(parser, free);

	// If the parser fails, there is no point starting the compiler, because it'd only flood the output with endless errors.
	if (FScriptPosition::ErrorCounter > 0)
	{
		I_Error("%d errors while parsing %s", FScriptPosition::ErrorCounter, fileSystem.GetFileFullPath(baselump).GetChars());
	}

#ifndef NDEBUG
	if (f != nullptr)
	{
		fclose(f);
	}
#endif

	// Make a dump of the AST before running the compiler for diagnostic purposes.
	if (Args->CheckParm("-dumpast"))
	{
		FString ast = ZCC_PrintAST(state.TopNode);
		FString filename = fileSystem.GetFileFullPath(baselump);
		filename.ReplaceChars(":\\/?|", '.');
		filename << ".ast";
		FileWriter *ff = FileWriter::Open(filename);
		if (ff != NULL)
		{
			ff->Write(ast.GetChars(), ast.Len());
			delete ff;
		}
	}

	auto newns = fileSystem.GetFileContainer(baselump) == 0 ? Namespaces.GlobalNamespace : Namespaces.NewNamespace(fileSystem.GetFileContainer(baselump));
	return newns;
}

static FString ZCCTokenName(int terminal)
{
	if (terminal == ZCC_EOF)
	{
		return "end of file";
	}
	int sc_token;
	if (terminal > 0 && terminal < (int)countof(BackTokenMap))
	{
		sc_token = BackTokenMap[terminal];
		if (sc_token == 0)
		{ // This token was not initialized. Whoops!
			sc_token = -terminal;
		}
	}
	else
	{ // This should never happen.
		sc_token = -terminal;
	}
	return FScanner::TokenName(sc_token);
}

ZCC_TreeNode *ZCC_AST::InitNode(size_t size, EZCCTreeNodeType type, ZCC_TreeNode *basis)
{
	ZCC_TreeNode *node = (ZCC_TreeNode *)SyntaxArena.Alloc(size);
	node->SiblingNext = node;
	node->SiblingPrev = node;
	node->NodeType = type;
	if (basis != NULL)
	{
		node->SourceName = basis->SourceName;
		node->SourceLump = basis->SourceLump;
		node->SourceLoc = basis->SourceLoc;
	}
	return node;
}

ZCC_TreeNode *ZCCParseState::InitNode(size_t size, EZCCTreeNodeType type)
{
	ZCC_TreeNode *node = ZCC_AST::InitNode(size, type, NULL);
	node->SourceName = Strings.Alloc(sc->ScriptName);
	node->SourceLump = sc->LumpNum;
	return node;
}

// Appends a sibling to this node's sibling list.
void AppendTreeNodeSibling(ZCC_TreeNode *thisnode, ZCC_TreeNode *sibling)
{
		if (thisnode == nullptr)
		{
			// Some bad syntax can actually get here, so better abort so that the user can see the error which caused this.
			I_FatalError("Internal script compiler error. Execution aborted.");
		}
		if (sibling == nullptr)
		{
			return;
		}

		ZCC_TreeNode *&SiblingPrev = thisnode->SiblingPrev;
		ZCC_TreeNode *&SiblingNext = thisnode->SiblingNext;

		// Check integrity of our sibling list.
		assert(SiblingPrev->SiblingNext == thisnode);
		assert(SiblingNext->SiblingPrev == thisnode);

		// Check integrity of new sibling list.
		assert(sibling->SiblingPrev->SiblingNext == sibling);
		assert(sibling->SiblingNext->SiblingPrev == sibling);

		ZCC_TreeNode *siblingend = sibling->SiblingPrev;
		SiblingPrev->SiblingNext = sibling;
		sibling->SiblingPrev = SiblingPrev;
		SiblingPrev = siblingend;
		siblingend->SiblingNext = thisnode;
}

//**--------------------------------------------------------------------------

const char *GetMixinTypeString(EZCCMixinType type) {
	switch (type) {
	case ZCC_Mixin_Class:
		return "class";

	default:
		assert(0 && "Unhandled mixin type");
		return "invalid type";
	}
}

//**--------------------------------------------------------------------------

ZCC_TreeNode *TreeNodeDeepCopy_Internal(ZCC_AST *ast, ZCC_TreeNode *orig, bool copySiblings, TMap<ZCC_TreeNode *, ZCC_TreeNode *> *copiedNodesList);
void TreeNodeDeepCopy_Base(ZCC_AST *ast, ZCC_TreeNode *orig, ZCC_TreeNode *copy, bool copySiblings, TMap<ZCC_TreeNode *, ZCC_TreeNode *> *copiedNodesList)
{
	copy->SourceName = orig->SourceName;
	copy->SourceLump = orig->SourceLump;
	copy->SourceLoc = orig->SourceLoc;
	copy->NodeType = orig->NodeType;

	if (copySiblings)
	{
		auto node = orig->SiblingNext;
		while (node != orig)
		{
			auto nextNode = TreeNodeDeepCopy_Internal(ast, node, false, copiedNodesList);
			auto newLast = nextNode->SiblingPrev;

			auto firstNode = copy;
			auto lastNode = firstNode->SiblingPrev;

			lastNode->SiblingNext = nextNode;
			firstNode->SiblingPrev = newLast;

			nextNode->SiblingPrev = lastNode;
			newLast->SiblingNext = firstNode;

			node = node->SiblingNext;
		}
	}
}

#define GetTreeNode(type) static_cast<ZCC_##type *>(ast->InitNode(sizeof(ZCC_##type), AST_##type, nullptr));
#define TreeNodeDeepCopy_Start(type) \
	auto copy = GetTreeNode(type); \
	auto origCasted = static_cast<ZCC_##type *>(orig); \
	ret = copy; \
	copiedNodesList->Insert(orig, ret);

ZCC_TreeNode *TreeNodeDeepCopy(ZCC_AST *ast, ZCC_TreeNode *orig, bool copySiblings)
{
	TMap<ZCC_TreeNode *, ZCC_TreeNode *> copiedNodesList;
	copiedNodesList.Clear();

	return TreeNodeDeepCopy_Internal(ast, orig, copySiblings, &copiedNodesList);
}

ZCC_TreeNode *TreeNodeDeepCopy_Internal(ZCC_AST *ast, ZCC_TreeNode *orig, bool copySiblings, TMap<ZCC_TreeNode *, ZCC_TreeNode *> *copiedNodesList)
{
	// [pbeta] This is a legitimate case as otherwise this function would need
	// an excessive amount of "if" statements, so just return null.
	if (orig == nullptr)
	{
		return nullptr;
	}

	// [pbeta] We need to keep and check a list of already copied nodes, because
	// some are supposed to be the same, and it can cause infinite loops.
	auto existingCopy = copiedNodesList->CheckKey(orig);
	if (existingCopy != nullptr)
	{
		return *existingCopy;
	}

	ZCC_TreeNode *ret = nullptr;

	switch (orig->NodeType)
	{
	case AST_Identifier:
	{
		TreeNodeDeepCopy_Start(Identifier);

		// ZCC_Identifier
		copy->Id = origCasted->Id;

		break;
	}

	case AST_Struct:
	{
		TreeNodeDeepCopy_Start(Struct);

		// ZCC_NamedNode
		copy->NodeName = origCasted->NodeName;
		copy->Symbol = origCasted->Symbol;
		// ZCC_Struct
		copy->Flags = origCasted->Flags;
		copy->Body = TreeNodeDeepCopy_Internal(ast, origCasted->Body, true, copiedNodesList);
		copy->Type = origCasted->Type;
		copy->Version = origCasted->Version;

		break;
	}

	case AST_Class:
	{
		TreeNodeDeepCopy_Start(Class);

		// ZCC_NamedNode
		copy->NodeName = origCasted->NodeName;
		copy->Symbol = origCasted->Symbol;
		// ZCC_Struct
		copy->Flags = origCasted->Flags;
		copy->Body = TreeNodeDeepCopy_Internal(ast, origCasted->Body, true, copiedNodesList);
		copy->Type = origCasted->Type;
		copy->Version = origCasted->Version;
		// ZCC_Class
		copy->ParentName = static_cast<ZCC_Identifier *>(TreeNodeDeepCopy_Internal(ast, origCasted->ParentName, true, copiedNodesList));
		copy->Replaces = static_cast<ZCC_Identifier *>(TreeNodeDeepCopy_Internal(ast, origCasted->Replaces, true, copiedNodesList));

		break;
	}

	case AST_Enum:
	{
		TreeNodeDeepCopy_Start(Enum);

		// ZCC_NamedNode
		copy->NodeName = origCasted->NodeName;
		copy->Symbol = origCasted->Symbol;
		// ZCC_Enum
		copy->EnumType = origCasted->EnumType;
		copy->Elements = static_cast<ZCC_ConstantDef *>(TreeNodeDeepCopy_Internal(ast, origCasted->Elements, false, copiedNodesList));

		break;
	}

	case AST_EnumTerminator:
	{
		TreeNodeDeepCopy_Start (EnumTerminator);
		break;
	}

	case AST_States:
	{
		TreeNodeDeepCopy_Start(States);

		// ZCC_States
		copy->Body = static_cast<ZCC_StatePart *>(TreeNodeDeepCopy_Internal(ast, origCasted->Body, true, copiedNodesList));
		copy->Flags = static_cast<ZCC_Identifier *>(TreeNodeDeepCopy_Internal(ast, origCasted->Flags, true, copiedNodesList));

		break;
	}

	case AST_StatePart:
	{
		TreeNodeDeepCopy_Start (StatePart);
		break;
	}

	case AST_StateLabel:
	{
		TreeNodeDeepCopy_Start(StateLabel);

		// ZCC_StateLabel
		copy->Label = origCasted->Label;

		break;
	}

	case AST_StateStop:
	{
		TreeNodeDeepCopy_Start (StateStop);
		break;
	}

	case AST_StateWait:
	{
		TreeNodeDeepCopy_Start (StateWait);
		break;
	}

	case AST_StateFail:
	{
		TreeNodeDeepCopy_Start (StateFail);
		break;
	}

	case AST_StateLoop:
	{
		TreeNodeDeepCopy_Start (StateLoop);
		break;
	}

	case AST_StateGoto:
	{
		TreeNodeDeepCopy_Start(StateGoto);

		// ZCC_StateGoto
		copy->Qualifier = static_cast<ZCC_Identifier *>(TreeNodeDeepCopy_Internal(ast, origCasted->Qualifier, true, copiedNodesList));
		copy->Label = static_cast<ZCC_Identifier *>(TreeNodeDeepCopy_Internal(ast, origCasted->Label, true, copiedNodesList));
		copy->Offset = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Offset, true, copiedNodesList));

		break;
	}

	case AST_StateLine:
	{
		TreeNodeDeepCopy_Start(StateLine);

		// ZCC_StateLine
		copy->Sprite = origCasted->Sprite;

		copy->bBright = origCasted->bBright;
		copy->bFast = origCasted->bFast;
		copy->bSlow = origCasted->bSlow;
		copy->bNoDelay = origCasted->bNoDelay;
		copy->bCanRaise = origCasted->bCanRaise;

		copy->Frames = origCasted->Frames;
		copy->Duration = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Duration, true, copiedNodesList));
		copy->Offset = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Offset, true, copiedNodesList));
		copy->Lights = static_cast<ZCC_ExprConstant *>(TreeNodeDeepCopy_Internal(ast, origCasted->Lights, true, copiedNodesList));
		copy->Action = TreeNodeDeepCopy_Internal(ast, origCasted->Action, true, copiedNodesList);

		break;
	}

	case AST_VarName:
	{
		TreeNodeDeepCopy_Start(VarName);

		// ZCC_VarName
		copy->Name = origCasted->Name;
		copy->ArraySize = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->ArraySize, true, copiedNodesList));

		break;
	}

	case AST_VarInit:
	{
		TreeNodeDeepCopy_Start(VarInit);

		// ZCC_VarName
		copy->Name = origCasted->Name;
		copy->ArraySize = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->ArraySize, true, copiedNodesList));
		// ZCC_VarInit
		copy->Init = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Init, true, copiedNodesList));
		copy->InitIsArray = origCasted->InitIsArray;

		break;
	}

	case AST_Type:
	{
		TreeNodeDeepCopy_Start(Type);

		// ZCC_Type
		copy->ArraySize = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->ArraySize, true, copiedNodesList));

		break;
	}

	case AST_BasicType:
	{
		TreeNodeDeepCopy_Start(BasicType);

		// ZCC_Type
		copy->ArraySize = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->ArraySize, true, copiedNodesList));
		// ZCC_BasicType
		copy->Type = origCasted->Type;
		copy->UserType = static_cast<ZCC_Identifier *>(TreeNodeDeepCopy_Internal(ast, origCasted->UserType, true, copiedNodesList));
		copy->isconst = origCasted->isconst;

		break;
	}

	case AST_MapType:
	{
		TreeNodeDeepCopy_Start(MapType);

		// ZCC_Type
		copy->ArraySize = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->ArraySize, true, copiedNodesList));
		// ZCC_MapType
		copy->KeyType = static_cast<ZCC_Type *>(TreeNodeDeepCopy_Internal(ast, origCasted->KeyType, true, copiedNodesList));
		copy->ValueType = static_cast<ZCC_Type *>(TreeNodeDeepCopy_Internal(ast, origCasted->ValueType, true, copiedNodesList));

		break;
	}

	case AST_DynArrayType:
	{
		TreeNodeDeepCopy_Start(DynArrayType);

		// ZCC_Type
		copy->ArraySize = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->ArraySize, true, copiedNodesList));
		// ZCC_DynArrayType
		copy->ElementType = static_cast<ZCC_Type *>(TreeNodeDeepCopy_Internal(ast, origCasted->ElementType, true, copiedNodesList));

		break;
	}

	case AST_ClassType:
	{
		TreeNodeDeepCopy_Start(ClassType);

		// ZCC_Type
		copy->ArraySize = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->ArraySize, true, copiedNodesList));
		// ZCC_ClassType
		copy->Restriction = static_cast<ZCC_Identifier *>(TreeNodeDeepCopy_Internal(ast, origCasted->Restriction, true, copiedNodesList));

		break;
	}

	case AST_Expression:
	{
		TreeNodeDeepCopy_Start(Expression);

		// ZCC_Expression
		copy->Operation = origCasted->Operation;
		copy->Type = origCasted->Type;

		break;
	}

	case AST_ExprID:
	{
		TreeNodeDeepCopy_Start(ExprID);

		// ZCC_Expression
		copy->Operation = origCasted->Operation;
		copy->Type = origCasted->Type;
		// ZCC_ExprID
		copy->Identifier = origCasted->Identifier;

		break;
	}

	case AST_ExprTypeRef:
	{
		TreeNodeDeepCopy_Start(ExprTypeRef);

		// ZCC_Expression
		copy->Operation = origCasted->Operation;
		copy->Type = origCasted->Type;
		// ZCC_ExprTypeRef
		copy->RefType = origCasted->RefType;

		break;
	}

	case AST_ExprConstant:
	{
		TreeNodeDeepCopy_Start(ExprConstant);

		// ZCC_Expression
		copy->Operation = origCasted->Operation;
		copy->Type = origCasted->Type;
		// ZCC_ExprConstant
		// Currently handled: StringVal, IntVal and DoubleVal. (UIntVal appears to be completely unused.)
		if (origCasted->Type == TypeString)
		{
			copy->StringVal = origCasted->StringVal;
		}
		else if (origCasted->Type == TypeFloat64 || origCasted->Type == TypeFloat32)
		{
			copy->DoubleVal = origCasted->DoubleVal;
		}
		else if (origCasted->Type == TypeName || origCasted->Type->isIntCompatible())
		{
			copy->IntVal = origCasted->IntVal;
		}

		break;
	}

	case AST_ExprFuncCall:
	{
		TreeNodeDeepCopy_Start(ExprFuncCall);

		// ZCC_Expression
		copy->Operation = origCasted->Operation;
		copy->Type = origCasted->Type;
		// ZCC_ExprFuncCall
		copy->Function = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Function, true, copiedNodesList));
		copy->Parameters = static_cast<ZCC_FuncParm *>(TreeNodeDeepCopy_Internal(ast, origCasted->Parameters, true, copiedNodesList));

		break;
	}

	case AST_ExprMemberAccess:
	{
		TreeNodeDeepCopy_Start(ExprMemberAccess);

		// ZCC_Expression
		copy->Operation = origCasted->Operation;
		copy->Type = origCasted->Type;
		// ZCC_ExprMemberAccess
		copy->Left = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Left, true, copiedNodesList));
		copy->Right = origCasted->Right;

		break;
	}

	case AST_ExprUnary:
	{
		TreeNodeDeepCopy_Start(ExprUnary);

		// ZCC_Expression
		copy->Operation = origCasted->Operation;
		copy->Type = origCasted->Type;
		// ZCC_ExprUnary
		copy->Operand = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Operand, true, copiedNodesList));

		break;
	}

	case AST_ExprBinary:
	{
		TreeNodeDeepCopy_Start(ExprBinary);

		// ZCC_Expression
		copy->Operation = origCasted->Operation;
		copy->Type = origCasted->Type;
		// ZCC_ExprBinary
		copy->Left = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Left, true, copiedNodesList));
		copy->Right = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Right, true, copiedNodesList));

		break;
	}

	case AST_ExprTrinary:
	{
		TreeNodeDeepCopy_Start(ExprTrinary);

		// ZCC_Expression
		copy->Operation = origCasted->Operation;
		copy->Type = origCasted->Type;
		// ZCC_ExprTrinary
		copy->Test = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Test, true, copiedNodesList));
		copy->Left = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Left, true, copiedNodesList));
		copy->Right = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Right, true, copiedNodesList));

		break;
	}

	case AST_FuncParm:
	{
		TreeNodeDeepCopy_Start(FuncParm);

		// ZCC_FuncParm
		copy->Value = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Value, true, copiedNodesList));
		copy->Label = origCasted->Label;

		break;
	}

	case AST_Statement:
	{
		TreeNodeDeepCopy_Start (Statement);
		break;
	}

	case AST_CompoundStmt:
	{
		TreeNodeDeepCopy_Start(CompoundStmt);

		// ZCC_CompoundStmt
		copy->Content = static_cast<ZCC_Statement *>(TreeNodeDeepCopy_Internal(ast, origCasted->Content, true, copiedNodesList));

		break;
	}

	case AST_ContinueStmt:
	{
		TreeNodeDeepCopy_Start (ContinueStmt);
		break;
	}

	case AST_BreakStmt:
	{
		TreeNodeDeepCopy_Start (BreakStmt);
		break;
	}

	case AST_ReturnStmt:
	{
		TreeNodeDeepCopy_Start(ReturnStmt);

		// ZCC_ReturnStmt
		copy->Values = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Values, true, copiedNodesList));

		break;
	}

	case AST_ExpressionStmt:
	{
		TreeNodeDeepCopy_Start(ExpressionStmt);

		// ZCC_ExpressionStmt
		copy->Expression = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Expression, true, copiedNodesList));

		break;
	}

	case AST_IterationStmt:
	{
		TreeNodeDeepCopy_Start(IterationStmt);

		// ZCC_IterationStmt
		copy->LoopCondition = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->LoopCondition, true, copiedNodesList));
		copy->LoopStatement = static_cast<ZCC_Statement *>(TreeNodeDeepCopy_Internal(ast, origCasted->LoopStatement, true, copiedNodesList));
		copy->LoopBumper = static_cast<ZCC_Statement *>(TreeNodeDeepCopy_Internal(ast, origCasted->LoopBumper, true, copiedNodesList));
		copy->CheckAt = origCasted->CheckAt;

		break;
	}

	case AST_IfStmt:
	{
		TreeNodeDeepCopy_Start(IfStmt);

		// ZCC_IfStmt
		copy->Condition = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Condition, true, copiedNodesList));
		copy->TruePath = static_cast<ZCC_Statement *>(TreeNodeDeepCopy_Internal(ast, origCasted->TruePath, true, copiedNodesList));
		copy->FalsePath = static_cast<ZCC_Statement *>(TreeNodeDeepCopy_Internal(ast, origCasted->FalsePath, true, copiedNodesList));

		break;
	}

	case AST_SwitchStmt:
	{
		TreeNodeDeepCopy_Start(SwitchStmt);

		// ZCC_SwitchStmt
		copy->Condition = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Condition, true, copiedNodesList));
		copy->Content = static_cast<ZCC_Statement *>(TreeNodeDeepCopy_Internal(ast, origCasted->Content, true, copiedNodesList));

		break;
	}

	case AST_CaseStmt:
	{
		TreeNodeDeepCopy_Start(CaseStmt);

		// ZCC_CaseStmt
		copy->Condition = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Condition, true, copiedNodesList));

		break;
	}

	case AST_AssignStmt:
	{
		TreeNodeDeepCopy_Start(AssignStmt);

		// ZCC_AssignStmt
		copy->Dests = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Dests, true, copiedNodesList));
		copy->Sources = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Sources, true, copiedNodesList));
		copy->AssignOp = origCasted->AssignOp;

		break;
	}

	case AST_LocalVarStmt:
	{
		TreeNodeDeepCopy_Start(LocalVarStmt);

		// ZCC_LocalVarStmt
		copy->Type = static_cast<ZCC_Type *>(TreeNodeDeepCopy_Internal(ast, origCasted->Type, true, copiedNodesList));
		copy->Vars = static_cast<ZCC_VarInit *>(TreeNodeDeepCopy_Internal(ast, origCasted->Vars, true, copiedNodesList));

		break;
	}

	case AST_FuncParamDecl:
	{
		TreeNodeDeepCopy_Start(FuncParamDecl);

		// ZCC_FuncParamDecl
		copy->Type = static_cast<ZCC_Type *>(TreeNodeDeepCopy_Internal(ast, origCasted->Type, true, copiedNodesList));
		copy->Default = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Default, true, copiedNodesList));
		copy->Name = origCasted->Name;
		copy->Flags = origCasted->Flags;

		break;
	}

	case AST_ConstantDef:
	{
		TreeNodeDeepCopy_Start(ConstantDef);

		// ZCC_NamedNode
		copy->NodeName = origCasted->NodeName;
		copy->Symbol = origCasted->Symbol;
		// ZCC_ConstantDef
		copy->Value = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Value, true, copiedNodesList));
		copy->Symbol = origCasted->Symbol;
		if (copy->Symbol != nullptr)
		{
			copy->Type = static_cast<ZCC_Enum *>(TreeNodeDeepCopy_Internal(ast, origCasted->Type, true, copiedNodesList));
		}

		break;
	}

	case AST_Declarator:
	{
		TreeNodeDeepCopy_Start(Declarator);

		// ZCC_Declarator
		copy->Type = static_cast<ZCC_Type *>(TreeNodeDeepCopy_Internal(ast, origCasted->Type, true, copiedNodesList));
		copy->Flags = origCasted->Flags;
		copy->Version = origCasted->Version;

		break;
	}

	case AST_VarDeclarator:
	{
		TreeNodeDeepCopy_Start(VarDeclarator);

		// ZCC_Declarator
		copy->Type = static_cast<ZCC_Type *>(TreeNodeDeepCopy_Internal(ast, origCasted->Type, true, copiedNodesList));
		copy->Flags = origCasted->Flags;
		copy->Version = origCasted->Version;
		// ZCC_VarDeclarator
		copy->Names = static_cast<ZCC_VarName *>(TreeNodeDeepCopy_Internal(ast, origCasted->Names, true, copiedNodesList));
		copy->DeprecationMessage = origCasted->DeprecationMessage;

		break;
	}

	case AST_FuncDeclarator:
	{
		TreeNodeDeepCopy_Start(FuncDeclarator);

		// ZCC_Declarator
		copy->Type = static_cast<ZCC_Type *>(TreeNodeDeepCopy_Internal(ast, origCasted->Type, true, copiedNodesList));
		copy->Flags = origCasted->Flags;
		copy->Version = origCasted->Version;
		// ZCC_FuncDeclarator
		copy->Params = static_cast<ZCC_FuncParamDecl *>(TreeNodeDeepCopy_Internal(ast, origCasted->Params, true, copiedNodesList));
		copy->Name = origCasted->Name;
		copy->Body = static_cast<ZCC_Statement *>(TreeNodeDeepCopy_Internal(ast, origCasted->Body, true, copiedNodesList));
		copy->UseFlags = static_cast<ZCC_Identifier *>(TreeNodeDeepCopy_Internal(ast, origCasted->UseFlags, true, copiedNodesList));
		copy->DeprecationMessage = origCasted->DeprecationMessage;

		break;
	}

	case AST_Default:
	{
		TreeNodeDeepCopy_Start(Default);

		// ZCC_CompoundStmt
		copy->Content = static_cast<ZCC_Statement *>(TreeNodeDeepCopy_Internal(ast, origCasted->Content, true, copiedNodesList));

		break;
	}

	case AST_FlagStmt:
	{
		TreeNodeDeepCopy_Start(FlagStmt);

		// ZCC_FlagStmt
		copy->name = static_cast<ZCC_Identifier *>(TreeNodeDeepCopy_Internal(ast, origCasted->name, true, copiedNodesList));
		copy->set = origCasted->set;

		break;
	}

	case AST_PropertyStmt:
	{
		TreeNodeDeepCopy_Start(PropertyStmt);

		// ZCC_PropertyStmt
		copy->Prop = static_cast<ZCC_Identifier *>(TreeNodeDeepCopy_Internal(ast, origCasted->Prop, true, copiedNodesList));
		copy->Values = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Values, true, copiedNodesList));

		break;
	}

	case AST_VectorValue:
	{
		TreeNodeDeepCopy_Start(VectorValue);

		// ZCC_Expression
		copy->Operation = origCasted->Operation;
		copy->Type = origCasted->Type;
		// ZCC_VectorValue
		copy->X = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->X, true, copiedNodesList));
		copy->Y = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Y, true, copiedNodesList));
		copy->Z = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Z, true, copiedNodesList));

		break;
	}

	case AST_DeclFlags:
	{
		TreeNodeDeepCopy_Start(DeclFlags);

		// ZCC_DeclFlags
		copy->Id = static_cast<ZCC_Identifier *>(TreeNodeDeepCopy_Internal(ast, origCasted->Id, true, copiedNodesList));
		copy->DeprecationMessage = origCasted->DeprecationMessage;
		copy->Version = origCasted->Version;
		copy->Flags = origCasted->Flags;

		break;
	}
		
	case AST_ClassCast:
	{
		TreeNodeDeepCopy_Start(ClassCast);

		// ZCC_Expression
		copy->Operation = origCasted->Operation;
		copy->Type = origCasted->Type;
		// ZCC_ClassCast
		copy->ClassName = origCasted->ClassName;
		copy->Parameters = static_cast<ZCC_FuncParm *>(TreeNodeDeepCopy_Internal(ast, origCasted->Parameters, true, copiedNodesList));

		break;
	}

	case AST_StaticArrayStatement:
	{
		TreeNodeDeepCopy_Start(StaticArrayStatement);

		// ZCC_StaticArrayStatement
		copy->Type = static_cast<ZCC_Type *>(TreeNodeDeepCopy_Internal(ast, origCasted->Type, true, copiedNodesList));
		copy->Id = origCasted->Id;
		copy->Values = static_cast<ZCC_Expression *>(TreeNodeDeepCopy_Internal(ast, origCasted->Values, true, copiedNodesList));

		break;
	}

	case AST_Property:
	{
		TreeNodeDeepCopy_Start(Property);

		// ZCC_NamedNode
		copy->NodeName = origCasted->NodeName;
		copy->Symbol = origCasted->Symbol;
		// ZCC_Property
		copy->Body = TreeNodeDeepCopy_Internal(ast, origCasted->Body, true, copiedNodesList);

		break;
	}

	case AST_FlagDef:
	{
		TreeNodeDeepCopy_Start(FlagDef);

		// ZCC_NamedNode
		copy->NodeName = origCasted->NodeName;
		copy->Symbol = origCasted->Symbol;
		// ZCC_FlagDef
		copy->RefName = origCasted->RefName;
		copy->BitValue = origCasted->BitValue;

		break;
	}

	case AST_MixinDef:
	{
		TreeNodeDeepCopy_Start(MixinDef);

		// ZCC_NamedNode
		copy->NodeName = origCasted->NodeName;
		copy->Symbol = origCasted->Symbol;
		// ZCC_MixinDef
		copy->Body = TreeNodeDeepCopy_Internal(ast, origCasted->Body, true, copiedNodesList);
		copy->MixinType = origCasted->MixinType;

		break;
	}

	case AST_MixinStmt:
	{
		TreeNodeDeepCopy_Start(MixinStmt);

		// ZCC_MixinStmt
		copy->MixinName = origCasted->MixinName;

		break;
	}

	default:
		assert(0 && "Unimplemented node type");
		break;
	}

	TreeNodeDeepCopy_Base(ast, orig, ret, copySiblings, copiedNodesList);

	return ret;
}
/*
** zcc_compile.cpp
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
** Copyright 2016 Christoph Oelckers
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
#include "filesystem.h"
#include "zcc_parser.h"
#include "zcc-parse.h"
#include "zcc_compile.h"
#include "printf.h"
#include "symbols.h"

FSharedStringArena VMStringConstants;


int GetIntConst(FxExpression *ex, FCompileContext &ctx)
{
	ex = new FxIntCast(ex, false);
	ex = ex->Resolve(ctx);
	return ex ? static_cast<FxConstant*>(ex)->GetValue().GetInt() : 0;
}

double GetFloatConst(FxExpression *ex, FCompileContext &ctx)
{
	ex = new FxFloatCast(ex);
	ex = ex->Resolve(ctx);
	return ex ? static_cast<FxConstant*>(ex)->GetValue().GetFloat() : 0;
}

const char * ZCCCompiler::GetStringConst(FxExpression *ex, FCompileContext &ctx)
{
	ex = new FxStringCast(ex);
	ex = ex->Resolve(ctx);
	if (!ex) return "";
	// The string here must be stored in a persistent place that lasts long enough to have it processed.
	return AST.Strings.Alloc(static_cast<FxConstant*>(ex)->GetValue().GetString())->GetChars();
}

int ZCCCompiler::IntConstFromNode(ZCC_TreeNode *node, PContainerType *cls)
{
	FCompileContext ctx(OutNamespace, cls, false);
	FxExpression *ex = new FxIntCast(ConvertNode(node), false);
	ex = ex->Resolve(ctx);
	if (ex == nullptr) return 0;
	if (!ex->isConstant())
	{
		ex->ScriptPosition.Message(MSG_ERROR, "Expression is not constant");
		return 0;
	}
	return static_cast<FxConstant*>(ex)->GetValue().GetInt();
}

FString ZCCCompiler::StringConstFromNode(ZCC_TreeNode *node, PContainerType *cls)
{
	FCompileContext ctx(OutNamespace, cls, false);
	FxExpression *ex = new FxStringCast(ConvertNode(node));
	ex = ex->Resolve(ctx);
	if (ex == nullptr) return "";
	if (!ex->isConstant())
	{
		ex->ScriptPosition.Message(MSG_ERROR, "Expression is not constant");
		return "";
	}
	return static_cast<FxConstant*>(ex)->GetValue().GetString();
}

ZCC_MixinDef *ZCCCompiler::ResolveMixinStmt(ZCC_MixinStmt *mixinStmt, EZCCMixinType type)
{
	for (auto mx : Mixins)
	{
		if (mx->mixin->NodeName == mixinStmt->MixinName)
		{
			if (mx->mixin->MixinType != type)
			{
				Error(mixinStmt, "Mixin %s is a %s mixin cannot be used here.", FName(mixinStmt->MixinName).GetChars(), GetMixinTypeString(type));
				return nullptr;
			}

			return mx->mixin;
		}
	}

	Error(mixinStmt, "Mixin %s does not exist.", FName(mixinStmt->MixinName).GetChars());

	return nullptr;
}


//==========================================================================
//
// ZCCCompiler :: ProcessClass
//
//==========================================================================

void ZCCCompiler::ProcessClass(ZCC_Class *cnode, PSymbolTreeNode *treenode)
{
	ZCC_ClassWork *cls = nullptr;
	// If this is a class extension, put the new node directly into the existing class.
	if (cnode->Flags == ZCC_Extension)
	{
		for (auto clss : Classes)
		{
			if (clss->NodeName() == cnode->NodeName)
			{
				cls = clss;
				break;
			}
		}
		if (cls == nullptr)
		{
			Error(cnode, "Class %s cannot be found in the current translation unit.", FName(cnode->NodeName).GetChars());
			return;
		}
	}
	else
	{
		Classes.Push(new ZCC_ClassWork(static_cast<ZCC_Class *>(cnode), treenode));
		cls = Classes.Last();
	}

	auto node = cnode->Body;
	PSymbolTreeNode *childnode;
	ZCC_Enum *enumType = nullptr;

	// [pbeta] Handle mixins here for the sake of simplifying things.
	if (node != nullptr)
	{
		bool mixinError = false;
		TArray<ZCC_MixinStmt *> mixinStmts;
		mixinStmts.Clear();

		// Gather all mixin statement nodes.
		do
		{
			if (node->NodeType == AST_MixinStmt)
			{
				mixinStmts.Push(static_cast<ZCC_MixinStmt *>(node));
			}

			node = node->SiblingNext;
		}
		while (node != cnode->Body);

		for (auto mixinStmt : mixinStmts)
		{
			ZCC_MixinDef *mixinDef = ResolveMixinStmt(mixinStmt, ZCC_Mixin_Class);

			if (mixinDef == nullptr)
			{
				mixinError = true;
				continue;
			}

			// Insert the mixin if there's a body. If not, just remove this node.
			if (mixinDef->Body != nullptr)
			{
				auto newNode = TreeNodeDeepCopy(&AST, mixinDef->Body, true);

				if (mixinStmt->SiblingNext != mixinStmt && mixinStmt->SiblingPrev != mixinStmt)
				{
					auto prevSibling = mixinStmt->SiblingPrev;
					auto nextSibling = mixinStmt->SiblingNext;

					auto newFirst = newNode;
					auto newLast = newNode->SiblingPrev;

					newFirst->SiblingPrev = prevSibling;
					newLast->SiblingNext = nextSibling;

					prevSibling->SiblingNext = newFirst;
					nextSibling->SiblingPrev = newLast;
				}

				if (cnode->Body == mixinStmt)
				{
					cnode->Body = newNode;
				}
			}
			else
			{
				if (mixinStmt->SiblingNext != mixinStmt && mixinStmt->SiblingPrev != mixinStmt)
				{
					auto prevSibling = mixinStmt->SiblingPrev;
					auto nextSibling = mixinStmt->SiblingNext;

					prevSibling->SiblingNext = nextSibling;
					nextSibling->SiblingPrev = prevSibling;

					if (cnode->Body == mixinStmt)
					{
						cnode->Body = nextSibling;
					}
				}
				else if (cnode->Body == mixinStmt)
				{
					cnode->Body = nullptr;
				}
			}
		}

		mixinStmts.Clear();

		if (mixinError)
		{
			return;
		}
	}

	node = cnode->Body;

	// Need to check if the class actually has a body.
	if (node != nullptr) do
	{
		switch (node->NodeType)
		{
		case AST_MixinStmt:
			assert(0 && "Unhandled mixin statement in class parsing loop. If this has been reached, something is seriously wrong");
			Error(node, "Internal mixin error.");
			break;

		case AST_Struct:
		case AST_ConstantDef:
		case AST_Enum:
			if ((childnode = AddTreeNode(static_cast<ZCC_NamedNode *>(node)->NodeName, node, &cls->TreeNodes)))
			{
				switch (node->NodeType)
				{
				case AST_Enum:			
					enumType = static_cast<ZCC_Enum *>(node);
					cls->Enums.Push(enumType);
					break;

				case AST_Struct:	
					if (static_cast<ZCC_Struct *>(node)->Flags & VARF_Native)
					{
						Error(node, "Cannot define native structs inside classes");
						static_cast<ZCC_Struct *>(node)->Flags &= ~VARF_Native;
					}
					ProcessStruct(static_cast<ZCC_Struct *>(node), childnode, cls->cls);	
					break;

				case AST_ConstantDef:	
					cls->Constants.Push(static_cast<ZCC_ConstantDef *>(node));	
					cls->Constants.Last()->Type = enumType;
					break;

				default: 
					assert(0 && "Default case is just here to make GCC happy. It should never be reached");
				}
			}
			break;

		case AST_Property:
			cls->Properties.Push(static_cast<ZCC_Property *>(node));
			break;

		case AST_FlagDef:
			cls->FlagDefs.Push(static_cast<ZCC_FlagDef*>(node));
			break;

		case AST_VarDeclarator: 
			cls->Fields.Push(static_cast<ZCC_VarDeclarator *>(node)); 
			break;

		case AST_EnumTerminator:
			enumType = nullptr;
			break;

		case AST_States:
			cls->States.Push(static_cast<ZCC_States *>(node));
			break;

		case AST_FuncDeclarator:
			cls->Functions.Push(static_cast<ZCC_FuncDeclarator *>(node));
			break;

		case AST_Default:
			cls->Defaults.Push(static_cast<ZCC_Default *>(node));
			break;

		case AST_StaticArrayStatement:
			if (AddTreeNode(static_cast<ZCC_StaticArrayStatement *>(node)->Id, node, &cls->TreeNodes))
			{
				cls->Arrays.Push(static_cast<ZCC_StaticArrayStatement *>(node));
			}
			break;

		default:
			assert(0 && "Unhandled AST node type");
			break;
		}

		node = node->SiblingNext;
	}
	while (node != cnode->Body);
}

//==========================================================================
//
// ZCCCompiler :: ProcessMixin
//
//==========================================================================

void ZCCCompiler::ProcessMixin(ZCC_MixinDef *cnode, PSymbolTreeNode *treenode)
{
	ZCC_MixinWork *cls = new ZCC_MixinWork(cnode, treenode);

	auto node = cnode->Body;

	// Need to check if the mixin actually has a body.
	if (node != nullptr) do
	{
		if (cnode->MixinType == ZCC_Mixin_Class)
		{
			switch (node->NodeType)
			{
			case AST_Struct:
			case AST_ConstantDef:
			case AST_Enum:
			case AST_Property:
			case AST_FlagDef:
			case AST_VarDeclarator:
			case AST_EnumTerminator:
			case AST_States:
			case AST_FuncDeclarator:
			case AST_Default:
			case AST_StaticArrayStatement:
				break;

			default:
				assert(0 && "Unhandled AST node type");
				break;
			}
		}

		node = node->SiblingNext;
	} while (node != cnode->Body);

	Mixins.Push(cls);
}

//==========================================================================
//
// ZCCCompiler :: ProcessStruct
//
//==========================================================================

void ZCCCompiler::ProcessStruct(ZCC_Struct *cnode, PSymbolTreeNode *treenode, ZCC_Class *outer)
{
	ZCC_StructWork* cls = nullptr;

	// If this is a struct extension, put the new node directly into the existing class.
	if (cnode->Flags == ZCC_Extension)
	{
		for (auto strct : Structs)
		{
			if (strct->NodeName() == cnode->NodeName)
			{
				cls = strct;
				break;
			}
		}
		if (cls == nullptr)
		{
			Error(cnode, "Struct %s cannot be found in the current translation unit.", FName(cnode->NodeName).GetChars());
			return;
		}
	}
	else
	{
		Structs.Push(new ZCC_StructWork(static_cast<ZCC_Struct*>(cnode), treenode, outer));
		cls = Structs.Last();
	}

	auto node = cnode->Body;
	PSymbolTreeNode *childnode;
	ZCC_Enum *enumType = nullptr;

	// Need to check if the struct actually has a body.
	if (node != nullptr) do
	{
		switch (node->NodeType)
		{
		case AST_ConstantDef:
		case AST_Enum:
			if ((childnode = AddTreeNode(static_cast<ZCC_NamedNode *>(node)->NodeName, node, &cls->TreeNodes)))
			{
				switch (node->NodeType)
				{
				case AST_Enum:
					enumType = static_cast<ZCC_Enum *>(node);
					cls->Enums.Push(enumType);
					break;

				case AST_ConstantDef:
					cls->Constants.Push(static_cast<ZCC_ConstantDef *>(node));
					cls->Constants.Last()->Type = enumType;
					break;

				default:
					assert(0 && "Default case is just here to make GCC happy. It should never be reached");
				}
			}
			break;

		case AST_VarDeclarator: 
			cls->Fields.Push(static_cast<ZCC_VarDeclarator *>(node)); 
			break;

		case AST_FuncDeclarator:
			cls->Functions.Push(static_cast<ZCC_FuncDeclarator *>(node));
			break;

		case AST_EnumTerminator:
			enumType = nullptr;
			break;

		case AST_StaticArrayStatement:
			if (AddTreeNode(static_cast<ZCC_StaticArrayStatement *>(node)->Id, node, &cls->TreeNodes))
			{
				cls->Arrays.Push(static_cast<ZCC_StaticArrayStatement *>(node));
			}
			break;

		default:
			assert(0 && "Unhandled AST node type");
			break;
		}
		node = node->SiblingNext;
	} 
	while (node != cnode->Body);
}

//==========================================================================
//
// ZCCCompiler Constructor
//
//==========================================================================

ZCCCompiler::ZCCCompiler(ZCC_AST &ast, DObject *_outer, PSymbolTable &_symbols, PNamespace *_outnamespc, int lumpnum, const VersionInfo &ver)
	: mVersion(ver), Outer(_outer), ConvertClass(nullptr), GlobalTreeNodes(&_symbols), OutNamespace(_outnamespc), AST(ast), Lump(lumpnum)
{
	FScriptPosition::ResetErrorCounter();
	// Group top-level nodes by type
	if (ast.TopNode != NULL)
	{
		ZCC_TreeNode *node = ast.TopNode;
		PSymbolTreeNode *tnode;

		// [pbeta] Anything that must be processed before classes, structs, etc. should go here.
		do
		{
			switch (node->NodeType)
			{
			// [pbeta] Mixins must be processed before everything else.
			case AST_MixinDef:
				if ((tnode = AddTreeNode(static_cast<ZCC_NamedNode *>(node)->NodeName, node, GlobalTreeNodes)))
				{
					ProcessMixin(static_cast<ZCC_MixinDef *>(node), tnode);
					break;
				}
				break;

			default:
				break; // Shut GCC up.
			}
			node = node->SiblingNext;
		} while (node != ast.TopNode);

		node = ast.TopNode;
		PType *enumType = nullptr;
		ZCC_Enum *zenumType = nullptr;

		do
		{
			switch (node->NodeType)
			{
			case AST_MixinDef:
				// [pbeta] We already processed mixins, ignore them here.
				break;

			case AST_Class:
				// a class extension should not check the tree node symbols.
				if (static_cast<ZCC_Class *>(node)->Flags == ZCC_Extension)
				{
					ProcessClass(static_cast<ZCC_Class *>(node), tnode);
					break;
				}
				goto common;
			case AST_Struct:
				if (static_cast<ZCC_Class*>(node)->Flags == ZCC_Extension)
				{
					ProcessStruct(static_cast<ZCC_Struct*>(node), tnode, nullptr);
					break;
				}
				goto common;

			common:
			case AST_ConstantDef:
			case AST_Enum:
				if ((tnode = AddTreeNode(static_cast<ZCC_NamedNode *>(node)->NodeName, node, GlobalTreeNodes)))
				{
					switch (node->NodeType)
					{
					case AST_Enum:
						zenumType = static_cast<ZCC_Enum *>(node);
						enumType = NewEnum(zenumType->NodeName, OutNamespace);
						OutNamespace->Symbols.AddSymbol(Create<PSymbolType>(zenumType->NodeName, enumType));
						break;

					case AST_Class:
						ProcessClass(static_cast<ZCC_Class *>(node), tnode);
						break;

					case AST_Struct:
						ProcessStruct(static_cast<ZCC_Struct *>(node), tnode, nullptr);
						break;

					case AST_ConstantDef:
						Constants.Push(static_cast<ZCC_ConstantDef *>(node));
						Constants.Last()->Type = zenumType;
						break;

					default:
						assert(0 && "Default case is just here to make GCC happy. It should never be reached");
					}
				}
				break;

			case AST_EnumTerminator:
				zenumType = nullptr;
				break;

			default:
				assert(0 && "Unhandled AST node type");
				break;
			}
			node = node->SiblingNext;
		} while (node != ast.TopNode);
	}
}

ZCCCompiler::~ZCCCompiler()
{
	for (auto s : Structs)
	{
		delete s;
	}
	for (auto c : Classes)
	{
		delete c;
	}
	Structs.Clear();
	Classes.Clear();
}

//==========================================================================
//
// ZCCCompiler :: AddTreeNode
//
// Keeps track of definition nodes by their names. Ensures that all names
// in this scope are unique.
//
//==========================================================================

PSymbolTreeNode *ZCCCompiler::AddTreeNode(FName name, ZCC_TreeNode *node, PSymbolTable *treenodes, bool searchparents)
{
	PSymbol *check = treenodes->FindSymbol(name, searchparents);
	if (check != NULL)
	{
		assert(check->IsA(RUNTIME_CLASS(PSymbolTreeNode)));
		Error(node, "Attempt to redefine '%s'", name.GetChars());
		Error(static_cast<PSymbolTreeNode *>(check)->Node, " Original definition is here");
		return nullptr;
	}
	else
	{
		auto sy = Create<PSymbolTreeNode>(name, node);
		FString name;
		treenodes->AddSymbol(sy);
		return sy;
	}
}

//==========================================================================
//
// ZCCCompiler :: Warn
//
// Prints a warning message, and increments WarnCount.
//
//==========================================================================

void ZCCCompiler::Warn(ZCC_TreeNode *node, const char *msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);
	MessageV(node, TEXTCOLOR_ORANGE, msg, argptr);
	va_end(argptr);

	FScriptPosition::WarnCounter++;
}

//==========================================================================
//
// ZCCCompiler :: Error
//
// Prints an error message, and increments ErrorCount.
//
//==========================================================================

void ZCCCompiler::Error(ZCC_TreeNode *node, const char *msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);
	MessageV(node, TEXTCOLOR_RED, msg, argptr);
	va_end(argptr);

	FScriptPosition::ErrorCounter++;
}

//==========================================================================
//
// ZCCCompiler :: MessageV
//
// Prints a message, annotated with the source location for the tree node.
//
//==========================================================================

void ZCCCompiler::MessageV(ZCC_TreeNode *node, const char *txtcolor, const char *msg, va_list argptr)
{
	FString composed;

	composed.Format("%s%s, line %d: ", txtcolor, node->SourceName->GetChars(), node->SourceLoc);
	composed.VAppendFormat(msg, argptr);
	composed += '\n';
	PrintString(PRINT_HIGH, PRINTF_DEFAULT, composed);
}

//==========================================================================
//
// ZCCCompiler :: Compile
//
// Compile everything defined at this level.
// This can be overridden to add custom content.
//
//==========================================================================

int ZCCCompiler::Compile()
{
	CreateClassTypes();
	CreateStructTypes();
	CompileAllConstants();
	CompileAllFields();
	InitDefaults();
	InitFunctions();
	return FScriptPosition::ErrorCounter;
}

//==========================================================================
//
// ZCCCompiler :: CreateStructTypes
//
// Creates a PStruct for every struct.
//
//==========================================================================

void ZCCCompiler::CreateStructTypes()
{
	for(auto s : Structs)
	{
		PTypeBase *outer;
		PSymbolTable *syms;

		s->Outer = s->OuterDef == nullptr? nullptr : s->OuterDef->CType();
		if (s->Outer)
		{
			outer = s->Outer->VMType;
			syms = &s->Outer->VMType->Symbols;
		}
		else
		{
			outer = OutNamespace;
			syms = &OutNamespace->Symbols;
		}

		if (s->NodeName() == NAME__ && fileSystem.GetFileContainer(Lump) == 0)
		{
			// This is just a container for syntactic purposes.
			s->strct->Type = nullptr;
			continue;
		}
		else if (s->strct->Flags & ZCC_Native)
		{
			s->strct->Type = NewStruct(s->NodeName(), outer, true);
		}
		else
		{
			s->strct->Type = NewStruct(s->NodeName(), outer);
		}
		if (s->strct->Flags & ZCC_Version)
		{
			s->strct->Type->mVersion = s->strct->Version;
		}

		auto &sf = s->Type()->ScopeFlags;
		if (mVersion >= MakeVersion(2, 4, 0))
		{
			if ((s->strct->Flags & (ZCC_UIFlag | ZCC_Play)) == (ZCC_UIFlag | ZCC_Play))
			{
				Error(s->strct, "Struct %s has incompatible flags", s->NodeName().GetChars());
			}

			if (outer != OutNamespace) sf = FScopeBarrier::ChangeSideInObjectFlags(sf, FScopeBarrier::SideFromObjectFlags(static_cast<PType*>(outer)->ScopeFlags));
			else if (s->strct->Flags & ZCC_ClearScope) Warn(s->strct, "Useless 'ClearScope' on struct %s not inside a class", s->NodeName().GetChars());
			if (s->strct->Flags & ZCC_UIFlag)
				sf = FScopeBarrier::ChangeSideInObjectFlags(sf, FScopeBarrier::Side_UI);
			if (s->strct->Flags & ZCC_Play)
				sf = FScopeBarrier::ChangeSideInObjectFlags(sf, FScopeBarrier::Side_Play);
			if (s->strct->Flags & ZCC_ClearScope)
				sf = FScopeBarrier::ChangeSideInObjectFlags(sf, FScopeBarrier::Side_PlainData); // don't inherit the scope from the outer class
		}
		else
		{
			// old versions force 'play'.
			sf = FScopeBarrier::ChangeSideInObjectFlags(sf, FScopeBarrier::Side_Play);
		}
		s->strct->Symbol = Create<PSymbolType>(s->NodeName(), s->Type());
		syms->AddSymbol(s->strct->Symbol);

		for (auto e : s->Enums)
		{
			auto etype = NewEnum(e->NodeName, s->Type());
			s->Type()->Symbols.AddSymbol(Create<PSymbolType>(e->NodeName, etype));
		}
	}
}

//==========================================================================
//
// ZCCCompiler :: CreateClassTypes
//
// Creates a PClass for every class so that we get access to the symbol table
// These will be created with unknown size because for that we need to
// process all fields first, but to do that we need the PClass and some
// other info depending on the PClass.
//
//==========================================================================

void ZCCCompiler::CreateClassTypes()
{
	// we are going to sort the classes array so that entries are sorted in order of inheritance.

	auto OrigClasses = std::move(Classes);
	Classes.Clear();
	bool donesomething = true;
	while (donesomething)
	{
		donesomething = false;
		for (unsigned i = 0; i<OrigClasses.Size(); i++)
		{
			auto c = OrigClasses[i];
			// Check if we got the parent already defined.
			PClass *parent;
			auto ParentName = c->cls->ParentName;

			if (ParentName != nullptr && ParentName->SiblingNext == ParentName) parent = PClass::FindClass(ParentName->Id);
			else if (ParentName == nullptr) parent = RUNTIME_CLASS(DObject);
			else
			{
				// The parent is a dotted name which the type system currently does not handle.
				// Once it does this needs to be implemented here.
				auto p = ParentName;
				FString build;

				do
				{
					if (build.IsNotEmpty()) build += '.';
					build += FName(p->Id).GetChars();
					p = static_cast<decltype(p)>(p->SiblingNext);
				} while (p != ParentName);
				Error(c->cls, "Qualified name '%s' for base class not supported in '%s'", build.GetChars(), FName(c->NodeName()).GetChars());
				parent = RUNTIME_CLASS(DObject);
			}

			if (parent != nullptr && (parent->VMType != nullptr || c->NodeName() == NAME_Object))
			{
				// The parent exists, we may create a type for this class
				if (c->cls->Flags & ZCC_Native)
				{
					// If this is a native class, its own type must also already exist and not be a runtime class.
					auto me = PClass::FindClass(c->NodeName());
					if (me == nullptr)
					{
						Error(c->cls, "Unknown native class %s", c->NodeName().GetChars());
						// Create a placeholder so that the compiler can continue looking for errors.
						me = parent->FindClassTentative(c->NodeName());
					}
					else if (me->bRuntimeClass)
					{
						Error(c->cls, "%s is not a native class", c->NodeName().GetChars());
					}
					else
					{
						DPrintf(DMSG_SPAMMY, "Registered %s as native with parent %s\n", me->TypeName.GetChars(), parent->TypeName.GetChars());
					}
					c->cls->Type = NewClassType(me);
					me->SourceLumpName = *c->cls->SourceName;
				}
				else
				{
					// We will never get here if the name is a duplicate, so we can just do the assignment.
					try
					{
						if (parent->VMType->mVersion > mVersion)
						{
							Error(c->cls, "Parent class %s of %s not accessible to ZScript version %d.%d.%d", parent->TypeName.GetChars(), c->NodeName().GetChars(), mVersion.major, mVersion.minor, mVersion.revision);
						}
						auto newclass = parent->CreateDerivedClass(c->NodeName(), TentativeClass);
						if (newclass == nullptr)
						{
							Error(c->cls, "Class name %s already exists", c->NodeName().GetChars());
						}
						else
						{
							c->cls->Type = NewClassType(newclass);
							DPrintf(DMSG_SPAMMY, "Created class %s with parent %s\n", c->Type()->TypeName.GetChars(), c->ClassType()->ParentClass->TypeName.GetChars());
						}
					}
					catch (CRecoverableError &err)
					{
						Error(c->cls, "%s", err.GetMessage());
						c->cls->Type = nullptr;
					}
				}
				if (c->Type() == nullptr)
				{
					// create a placeholder so that the compiler can continue looking for errors.
					c->cls->Type = NewClassType(parent->FindClassTentative(c->NodeName()));
				}

				if (c->cls->Flags & ZCC_Abstract)
					c->ClassType()->bAbstract = true;

				if (c->cls->Flags & ZCC_Version)
				{
					c->Type()->mVersion = c->cls->Version;
				}
				// 
				if (mVersion >= MakeVersion(2, 4, 0))
				{
					static int incompatible[] = { ZCC_UIFlag, ZCC_Play, ZCC_ClearScope };
					int incompatiblecnt = 0;
					for (size_t k = 0; k < countof(incompatible); k++)
						if (incompatible[k] & c->cls->Flags) incompatiblecnt++;

					if (incompatiblecnt > 1)
					{
						Error(c->cls, "Class %s has incompatible flags", c->NodeName().GetChars());
					}

					if (c->cls->Flags & ZCC_UIFlag)
						c->Type()->ScopeFlags = EScopeFlags((c->Type()->ScopeFlags&~Scope_Play) | Scope_UI);
					if (c->cls->Flags & ZCC_Play)
						c->Type()->ScopeFlags = EScopeFlags((c->Type()->ScopeFlags&~Scope_UI) | Scope_Play);
					if (parent->VMType->ScopeFlags & (Scope_UI | Scope_Play)) // parent is either ui or play
					{
						if (c->cls->Flags & (ZCC_UIFlag | ZCC_Play))
						{
							Error(c->cls, "Can't change class scope in class %s", c->NodeName().GetChars());
						}
						c->Type()->ScopeFlags = FScopeBarrier::ChangeSideInObjectFlags(c->Type()->ScopeFlags, FScopeBarrier::SideFromObjectFlags(parent->VMType->ScopeFlags));
					}
				}
				else
				{
					c->Type()->ScopeFlags = FScopeBarrier::ChangeSideInObjectFlags(c->Type()->ScopeFlags, FScopeBarrier::Side_Play);
				}

				c->cls->Symbol = Create<PSymbolType>(c->NodeName(), c->Type());
				OutNamespace->Symbols.AddSymbol(c->cls->Symbol);
				Classes.Push(c);
				OrigClasses.Delete(i--);
				donesomething = true;
			}
			else if (c->cls->ParentName != nullptr)
			{
				// No base class found. Now check if something in the unprocessed classes matches.
				// If not, print an error. If something is found let's retry again in the next iteration.
				bool found = false;
				for (auto d : OrigClasses)
				{
					if (d->NodeName() == c->cls->ParentName->Id)
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					Error(c->cls, "Class %s has unknown base class %s", c->NodeName().GetChars(), FName(c->cls->ParentName->Id).GetChars());
					// create a placeholder so that the compiler can continue looking for errors.
					c->cls->Type = NewClassType(RUNTIME_CLASS(DObject)->FindClassTentative(c->NodeName()));
					c->cls->Symbol = Create<PSymbolType>(c->NodeName(), c->Type());
					OutNamespace->Symbols.AddSymbol(c->cls->Symbol);
					Classes.Push(c);
					OrigClasses.Delete(i--);
					donesomething = true;
				}
			}
		}
	}

	// What's left refers to some other class in the list but could not be resolved.
	// This normally means a circular reference.
	for (auto c : OrigClasses)
	{
		Error(c->cls, "Class %s has circular inheritance", FName(c->NodeName()).GetChars());
		c->cls->Type = NewClassType(RUNTIME_CLASS(DObject)->FindClassTentative(c->NodeName()));
		c->cls->Symbol = Create<PSymbolType>(c->NodeName(), c->Type());
		OutNamespace->Symbols.AddSymbol(c->cls->Symbol);
		Classes.Push(c);
	}

	// Last but not least: Now that all classes have been created, we can create the symbols for the internal enums and link the treenode symbol tables.
	for (auto cd : Classes)
	{
		for (auto e : cd->Enums)
		{
			auto etype = NewEnum(e->NodeName, cd->Type());
			cd->Type()->Symbols.AddSymbol(Create<PSymbolType>(e->NodeName, etype));
		}
		// Link the tree node tables. We only can do this after we know the class relations.
		for (auto cc : Classes)
		{
			if (cc->ClassType() == cd->ClassType()->ParentClass)
			{
				cd->TreeNodes.SetParentTable(&cc->TreeNodes);
				break;
			}
		}
	}
}

//==========================================================================
//
// ZCCCompiler :: AddConstants
//
// Helper for CompileAllConstants
//
//==========================================================================

void ZCCCompiler::CopyConstants(TArray<ZCC_ConstantWork> &dest, TArray<ZCC_ConstantDef*> &Constants, PContainerType *cls, PSymbolTable *ot)
{
	for (auto c : Constants)
	{
		dest.Push({ c, cls, ot });
	}
}

//==========================================================================
//
// ZCCCompiler :: CompileAllConstants
//
// Make symbols from every constant defined at all levels.
// Since constants may only depend on other constants this can be done
// without any more involved processing of the AST as a first step.
//
//==========================================================================

void ZCCCompiler::CompileAllConstants()
{
	// put all constants in one list to make resolving this easier.
	TArray<ZCC_ConstantWork> constantwork;

	CopyConstants(constantwork, Constants, nullptr, &OutNamespace->Symbols);
	for (auto c : Classes)
	{
		CopyConstants(constantwork, c->Constants, c->Type(), &c->Type()->Symbols);
	}
	for (auto s : Structs)
	{
		if (s->Type() != nullptr)
			CopyConstants(constantwork, s->Constants, s->Type(), &s->Type()->Symbols);
	}

	// Before starting to resolve the list, let's create symbols for all already resolved ones first (i.e. all literal constants), to reduce work.
	for (unsigned i = 0; i<constantwork.Size(); i++)
	{
		if (constantwork[i].node->Value->NodeType == AST_ExprConstant)
		{
			AddConstant(constantwork[i]);
			// Remove the constant from the list
			constantwork.Delete(i);
			i--;
		}
	}
	bool donesomething = true;
	// Now go through this list until no more constants can be resolved. The remaining ones will be non-constant values.
	while (donesomething && constantwork.Size() > 0)
	{
		donesomething = false;
		for (unsigned i = 0; i < constantwork.Size(); i++)
		{
			if (CompileConstant(&constantwork[i]))
			{
				AddConstant(constantwork[i]);
				// Remove the constant from the list
				constantwork.Delete(i);
				i--;
				donesomething = true;
			}
		}
	}
	for (unsigned i = 0; i < constantwork.Size(); i++)
	{
		Error(constantwork[i].node, "%s is not a constant", FName(constantwork[i].node->NodeName).GetChars());
	}


	for (auto s : Structs)
	{
		CompileArrays(s);
	}
	for (auto c : Classes)
	{
		CompileArrays(c);
	}
}

//==========================================================================
//
// ZCCCompiler :: AddConstant
//
// Adds a constant to its assigned symbol table
//
//==========================================================================

void ZCCCompiler::AddConstant(ZCC_ConstantWork &constant)
{
	auto def = constant.node;
	auto val = def->Value;
	ExpVal &c = constant.constval;
	
	// This is for literal constants.
	if (val->NodeType == AST_ExprConstant)
	{
		ZCC_ExprConstant *cval = static_cast<ZCC_ExprConstant *>(val);
		if (cval->Type == TypeString)
		{
			def->Symbol = Create<PSymbolConstString>(def->NodeName, *(cval->StringVal));
		}
		else if (cval->Type->isInt())
		{
			// How do we get an Enum type in here without screwing everything up???
			//auto type = def->Type != nullptr ? def->Type : cval->Type;
			def->Symbol = Create<PSymbolConstNumeric>(def->NodeName, cval->Type, cval->IntVal);
		}
		else if (cval->Type->isFloat())
		{
			if (def->Type != nullptr)
			{
				Error(def, "Enum members must be integer values");
			}
			def->Symbol = Create<PSymbolConstNumeric>(def->NodeName, cval->Type, cval->DoubleVal);
		}
		else
		{
			Error(def->Value, "Bad type for constant definiton");
			def->Symbol = nullptr;
		}
	}
	else
	{
		if (c.Type == TypeString)
		{
			def->Symbol = Create<PSymbolConstString>(def->NodeName, c.GetString());
		}
		else if (c.Type->isInt())
		{
			// How do we get an Enum type in here without screwing everything up???
			//auto type = def->Type != nullptr ? def->Type : cval->Type;
			def->Symbol = Create<PSymbolConstNumeric>(def->NodeName, c.Type, c.GetInt());
		}
		else if (c.Type->isFloat())
		{
			if (def->Type != nullptr)
			{
				Error(def, "Enum members must be integer values");
			}
			def->Symbol = Create<PSymbolConstNumeric>(def->NodeName, c.Type, c.GetFloat());
		}
		else
		{
			Error(def->Value, "Bad type for constant definiton");
			def->Symbol = nullptr;
		}
	}

	if (def->Symbol == nullptr)
	{
		// Create a dummy constant so we don't make any undefined value warnings.
		def->Symbol = Create<PSymbolConstNumeric>(def->NodeName, TypeError, 0);
	}
	constant.Outputtable->ReplaceSymbol(def->Symbol);
}

//==========================================================================
//
// ZCCCompiler :: CompileConstant
//
// For every constant definition, evaluate its value (which should result
// in a constant), and create a symbol for it.
//
//==========================================================================

bool ZCCCompiler::CompileConstant(ZCC_ConstantWork *work)
{
	FCompileContext ctx(OutNamespace, work->cls, false);
	FxExpression *exp = ConvertNode(work->node->Value);
	try
	{
		FScriptPosition::errorout = true;
		exp = exp->Resolve(ctx);
		if (exp == nullptr) return false;
		FScriptPosition::errorout = false;
		if (!exp->isConstant())
		{
			delete exp;
			return false;
		}
		work->constval = static_cast<FxConstant*>(exp)->GetValue();
		delete exp;
		return true;
	}
	catch (...)
	{
		// eat the reported error and treat this as a temorary failure. All unresolved contants will be reported at the end.
		FScriptPosition::errorout = false;
		return false;
	}
}


void ZCCCompiler::CompileArrays(ZCC_StructWork *work)
{
	for(auto sas : work->Arrays)
	{
		PType *ztype = DetermineType(work->Type(), sas, sas->Id, sas->Type, false, true);
		PType *ctype = ztype;
		FArgumentList values;

		// Don't use narrow typea for casting.
		if (ctype->isInt()) ctype = static_cast<PInt*>(ztype)->Unsigned ? TypeUInt32 : TypeSInt32;
		else if (ctype == TypeFloat32) ctype = TypeFloat64;

		ConvertNodeList(values, sas->Values);

		bool fail = false;
		FCompileContext ctx(OutNamespace, work->Type(), false);

		char *destmem = (char *)ClassDataAllocator.Alloc(values.Size() * ztype->Align);
		memset(destmem, 0, values.Size() * ztype->Align);
		char *copyp = destmem;
		for (unsigned i = 0; i < values.Size(); i++)
		{
			values[i] = new FxTypeCast(values[i], ctype, false);
			values[i] = values[i]->Resolve(ctx);
			if (values[i] == nullptr) fail = true;
			else if (!values[i]->isConstant())
			{
				Error(sas, "Initializer must be constant");
				fail = true;
			}
			else
			{
				ExpVal val = static_cast<FxConstant*>(values[i])->GetValue();
				switch (ztype->GetRegType())
				{
				default:
					// should never happen
					Error(sas, "Non-integral type in constant array");
					return;

				case REGT_INT:
					ztype->SetValue(copyp, val.GetInt());
					break;

				case REGT_FLOAT:
					ztype->SetValue(copyp, val.GetFloat());
					break;

				case REGT_POINTER:
					*(void**)copyp = val.GetPointer();
					break;

				case REGT_STRING:
					::new(copyp) FString(val.GetString());
					break;
				}
				copyp += ztype->Align;
			}
		}
		work->Type()->Symbols.AddSymbol(Create<PField>(sas->Id, NewArray(ztype, values.Size()), VARF_Static | VARF_ReadOnly, (size_t)destmem));
	}
}

//==========================================================================
//
// ZCCCompiler :: NodeFromSymbol
//
//==========================================================================

ZCC_Expression *ZCCCompiler::NodeFromSymbol(PSymbol *sym, ZCC_Expression *source, PSymbolTable *table)
{
	assert(sym != nullptr);

	if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConst)))
	{
		return NodeFromSymbolConst(static_cast<PSymbolConst *>(sym), source);
	}
	else if (sym->IsKindOf(RUNTIME_CLASS(PSymbolType)))
	{
		return NodeFromSymbolType(static_cast<PSymbolType *>(sym), source);
	}
	return NULL;
}

//==========================================================================
//
// ZCCCompiler :: NodeFromSymbolConst
//
// Returns a new AST constant node with the symbol's content.
//
//==========================================================================

ZCC_ExprConstant *ZCCCompiler::NodeFromSymbolConst(PSymbolConst *sym, ZCC_Expression *idnode)
{
	ZCC_ExprConstant *val = static_cast<ZCC_ExprConstant *>(AST.InitNode(sizeof(*val), AST_ExprConstant, idnode));
	val->Operation = PEX_ConstValue;
	if (sym == NULL)
	{
		val->Type = TypeError;
		val->IntVal = 0;
	}
	else if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConstString)))
	{
		val->StringVal = AST.Strings.Alloc(static_cast<PSymbolConstString *>(sym)->Str);
		val->Type = TypeString;
	}
	else
	{
		val->Type = sym->ValueType;
		if (val->Type != TypeError)
		{
			assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolConstNumeric)));
			if (sym->ValueType->isIntCompatible())
			{
				val->IntVal = static_cast<PSymbolConstNumeric *>(sym)->Value;
			}
			else
			{
				assert(sym->ValueType->isFloat());
				val->DoubleVal = static_cast<PSymbolConstNumeric *>(sym)->Float;
			}
		}
	}
	return val;
}

//==========================================================================
//
// ZCCCompiler :: NodeFromSymbolType
//
// Returns a new AST type ref node with the symbol's content.
//
//==========================================================================

ZCC_ExprTypeRef *ZCCCompiler::NodeFromSymbolType(PSymbolType *sym, ZCC_Expression *idnode)
{
	ZCC_ExprTypeRef *ref = static_cast<ZCC_ExprTypeRef *>(AST.InitNode(sizeof(*ref), AST_ExprTypeRef, idnode));
	ref->Operation = PEX_TypeRef;
	ref->RefType = sym->Type;
	ref->Type = NewClassPointer(RUNTIME_CLASS(DObject));
	return ref;
}

//==========================================================================
//
// ZCCCompiler :: CompileAllFields
//
// builds the internal structure of all classes and structs
//
//==========================================================================

void ZCCCompiler::CompileAllFields()
{
	// Create copies of the arrays which can be altered
	auto Classes = this->Classes;
	auto Structs = this->Structs;
	TMap<FName, bool> HasNativeChildren;

	// first step: Look for native classes with native children.
	// These may not have any variables added to them because it'd clash with the native definitions.
	for (unsigned i = 0; i < Classes.Size(); i++)
	{
		auto c = Classes[i];

		if (c->Type()->Size != TentativeClass && c->Fields.Size() > 0)
		{
			// We need to search the global class table here because not all children may have a scripted definition attached.
			for (auto ac : PClass::AllClasses)
			{
				if (ac->ParentClass != nullptr && ac->ParentClass->VMType == c->Type() && ac->Size != TentativeClass)
				{
					// Only set a marker here, so that we can print a better message when the actual fields get added.
					HasNativeChildren.Insert(c->Type()->TypeName, true);
					break;
				}
			}
		}
	}
	bool donesomething = true;
	while (donesomething && (Structs.Size() > 0 || Classes.Size() > 0))
	{
		donesomething = false;
		for (unsigned i = 0; i < Structs.Size(); i++)
		{
			if (CompileFields(Structs[i]->Type(), Structs[i]->Fields, Structs[i]->Outer, Structs[i]->Type() == 0? GlobalTreeNodes : &Structs[i]->TreeNodes, true))
			{
				// Remove from the list if all fields got compiled.
				Structs.Delete(i--);
				donesomething = true;
			}
		}
		for (unsigned i = 0; i < Classes.Size(); i++)
		{
			auto type = Classes[i]->ClassType();
				
			if (type->Size == TentativeClass)
			{
				if (type->ParentClass->Size == TentativeClass)
				{
					// we do not know the parent class's size yet, so skip this class for now.
					continue;
				}
				else
				{
					// Inherit the size of the parent class
					type->Size = Classes[i]->ClassType()->ParentClass->Size;
				}
			}
			if (!PrepareMetaData(type))
			{
				if (Classes[i]->ClassType()->ParentClass)
					type->MetaSize = Classes[i]->ClassType()->ParentClass->MetaSize;
				else
					type->MetaSize = 0;
			}

			if (CompileFields(type->VMType, Classes[i]->Fields, nullptr, &Classes[i]->TreeNodes, false, !!HasNativeChildren.CheckKey(type->TypeName)))
			{
				// Remove from the list if all fields got compiled.
				Classes.Delete(i--);
				donesomething = true;
			}
		}
	}
	// This really should never happen, but if it does, let's better print an error.
	for (auto s : Structs)
	{
		Error(s->strct, "Unable to resolve all fields for struct %s", FName(s->NodeName()).GetChars());
	}
	for (auto s : Classes)
	{
		Error(s->cls, "Unable to resolve all fields for class %s", FName(s->NodeName()).GetChars());
	}
}

//==========================================================================
//
// ZCCCompiler :: CompileFields
//
// builds the internal structure of a single class or struct
//
//==========================================================================

bool ZCCCompiler::CompileFields(PContainerType *type, TArray<ZCC_VarDeclarator *> &Fields, PClass *Outer, PSymbolTable *TreeNodes, bool forstruct, bool hasnativechildren)
{
	while (Fields.Size() > 0)
	{
		auto field = Fields[0];
		FieldDesc *fd = nullptr;

		PType *fieldtype = DetermineType(type, field, field->Names->Name, field->Type, true, true);

		// For structs only allow 'deprecated', for classes exclude function qualifiers.
		int notallowed = forstruct? 
			ZCC_Latent | ZCC_Final | ZCC_Action | ZCC_Static | ZCC_FuncConst | ZCC_Abstract | ZCC_Virtual | ZCC_Override | ZCC_Meta | ZCC_Extension | ZCC_VirtualScope | ZCC_ClearScope :
			ZCC_Latent | ZCC_Final | ZCC_Action | ZCC_Static | ZCC_FuncConst | ZCC_Abstract | ZCC_Virtual | ZCC_Override | ZCC_Extension | ZCC_VirtualScope | ZCC_ClearScope;

		// Some internal fields need to be set to clearscope.
		if (fileSystem.GetFileContainer(Lump) == 0) notallowed &= ~ZCC_ClearScope;

		if (field->Flags & notallowed)
		{
			Error(field, "Invalid qualifiers for %s (%s not allowed)", FName(field->Names->Name).GetChars(), FlagsToString(field->Flags & notallowed).GetChars());
			field->Flags &= notallowed;
		}
		uint32_t varflags = 0;

		// These map directly to implementation flags.
		if (field->Flags & ZCC_Private) varflags |= VARF_Private;
		if (field->Flags & ZCC_Protected) varflags |= VARF_Protected;
		if (field->Flags & ZCC_Deprecated) varflags |= VARF_Deprecated;
		if (field->Flags & ZCC_ReadOnly) varflags |= VARF_ReadOnly;
		if (field->Flags & ZCC_Internal) varflags |= VARF_InternalAccess;
		if (field->Flags & ZCC_Transient) varflags |= VARF_Transient;
		if (mVersion >= MakeVersion(2, 4, 0))
		{
			if (type != nullptr)
			{
				if (type->ScopeFlags & Scope_UI)
					varflags |= VARF_UI;
				if (type->ScopeFlags & Scope_Play)
					varflags |= VARF_Play;
			}
			if (field->Flags & ZCC_UIFlag)
				varflags = FScopeBarrier::ChangeSideInFlags(varflags, FScopeBarrier::Side_UI);
			if (field->Flags & ZCC_Play)
				varflags = FScopeBarrier::ChangeSideInFlags(varflags, FScopeBarrier::Side_Play);
			if (field->Flags & ZCC_ClearScope)
				varflags = FScopeBarrier::ChangeSideInFlags(varflags, FScopeBarrier::Side_PlainData);
		}
		else
		{
			varflags |= VARF_Play;
		}

		if (field->Flags & ZCC_Native)
		{
			varflags |= VARF_Native | VARF_Transient;
		}

		static int excludescope[] = { ZCC_UIFlag, ZCC_Play, ZCC_ClearScope };
		int excludeflags = 0;
		int fc = 0;
		for (size_t i = 0; i < countof(excludescope); i++)
		{
			if (field->Flags & excludescope[i])
			{
				fc++;
				excludeflags |= excludescope[i];
			}
		}
		if (fc > 1)
		{
			Error(field, "Invalid combination of scope qualifiers %s on field %s", FlagsToString(excludeflags).GetChars(), FName(field->Names->Name).GetChars());
			varflags &= ~(VARF_UI | VARF_Play); // make plain data
		}

		if (field->Flags & ZCC_Meta)
		{
			varflags |= VARF_Meta | VARF_Static | VARF_ReadOnly;	// metadata implies readonly
		}

		if (field->Type->ArraySize != nullptr)
		{
			bool nosize;
			fieldtype = ResolveArraySize(fieldtype, field->Type->ArraySize, type, &nosize);

			if (nosize)
			{
				Error(field, "Must specify array size");
			}
		}

		auto name = field->Names;
		do
		{
			if ((fieldtype->Size == 0 || !fieldtype->SizeKnown) && !(varflags & VARF_Native))	// Size not known yet.
			{
				if (type != nullptr)
				{
					type->SizeKnown = false;
				}
				return false;
			}

			if (AddTreeNode(name->Name, name, TreeNodes, !forstruct))
			{
				auto thisfieldtype = fieldtype;
				if (name->ArraySize != nullptr)
				{
					bool nosize;
					thisfieldtype = ResolveArraySize(thisfieldtype, name->ArraySize, type, &nosize);

					if (nosize)
					{
						Error(field, "Must specify array size");
					}
				}
				
				PField *f = nullptr;

				if (varflags & VARF_Native)
				{
					if (varflags & VARF_Meta)
					{
						Error(field, "Native meta variable %s not allowed", FName(name->Name).GetChars());
					}
					else
					{
						fd = FindField(type, FName(name->Name).GetChars());
						if (fd == nullptr)
						{
							Error(field, "The member variable '%s.%s' has not been exported from the executable.", type == nullptr? "" : type->TypeName.GetChars(), FName(name->Name).GetChars());
						}
						// For native structs a size check cannot be done because they normally have no size. But for a native reference they are still fine.
						else if (thisfieldtype->Size != ~0u && fd->FieldSize != ~0u && thisfieldtype->Size != fd->FieldSize && fd->BitValue == 0 && 
							(!thisfieldtype->isStruct() || !static_cast<PStruct*>(thisfieldtype)->isNative))
						{
							Error(field, "The member variable '%s.%s' has mismatching sizes in internal and external declaration. (Internal = %d, External = %d)", type == nullptr ? "" : type->TypeName.GetChars(), FName(name->Name).GetChars(), fd->FieldSize, thisfieldtype->Size);
						}
						// Q: Should we check alignment, too? A mismatch may be an indicator for bad assumptions.
						else if (type != nullptr)
						{
							// for bit fields the type must point to the source variable.
							if (fd->BitValue != 0) thisfieldtype = fd->FieldSize == 1 ? TypeUInt8 : fd->FieldSize == 2 ? TypeUInt16 : TypeUInt32;
							f = type->AddNativeField(name->Name, thisfieldtype, fd->FieldOffset, varflags, fd->BitValue);
						}
						else
						{

							// This is a global variable.
							if (fd->BitValue != 0) thisfieldtype = fd->FieldSize == 1 ? TypeUInt8 : fd->FieldSize == 2 ? TypeUInt16 : TypeUInt32;
							f = Create<PField>(name->Name, thisfieldtype, varflags | VARF_Native | VARF_Static, fd->FieldOffset, fd->BitValue);

							if (OutNamespace->Symbols.AddSymbol(f) == nullptr)
							{ // name is already in use
								if (type != nullptr)
								{
									type->SizeKnown = false;
								}
								f->Destroy();
								return false;
							}
						}
					}
				}
				else if (hasnativechildren && !(varflags & VARF_Meta))
				{
					Error(field, "Cannot add field %s to %s. %s has native children which means it size may not change", FName(name->Name).GetChars(), type->TypeName.GetChars(), type->TypeName.GetChars());
				}
				else if (type != nullptr)
				{
					f = type->AddField(name->Name, thisfieldtype, varflags);
				}
				else
				{
					Error(field, "Cannot declare non-native global variables. Tried to declare %s", FName(name->Name).GetChars());
				}

				if ((field->Flags & (ZCC_Version | ZCC_Deprecated)) && f != nullptr)
				{
					f->mVersion = field->Version;

					if (field->DeprecationMessage != nullptr)
					{
						f->DeprecationMessage = *field->DeprecationMessage;
					}
				}
			}
			name = static_cast<ZCC_VarName*>(name->SiblingNext);
		} while (name != field->Names);
		Fields.Delete(0);
	}

	if (type != nullptr)
	{
		type->SizeKnown = Fields.Size() == 0;
	}

	return Fields.Size() == 0;
}

//==========================================================================
//
// ZCCCompiler :: FieldFlagsToString
//
// creates a string for a field's flags
//
//==========================================================================

FString ZCCCompiler::FlagsToString(uint32_t flags)
{

	const char *flagnames[] = { "native", "static", "private", "protected", "latent", "final", "meta", "action", "deprecated", "readonly", "const", "abstract", "extend", "virtual", "override", "transient", "vararg", "ui", "play", "clearscope", "virtualscope" };
	FString build;

	for (size_t i = 0; i < countof(flagnames); i++)
	{
		if (flags & (1 << i))
		{
			if (build.IsNotEmpty()) build += ", ";
			build += flagnames[i];
		}
	}
	return build;
}

//==========================================================================
//
// ZCCCompiler :: DetermineType
//
// retrieves the type for this field, for arrays the type of a single entry.
//
//==========================================================================

PType *ZCCCompiler::DetermineType(PType *outertype, ZCC_TreeNode *field, FName name, ZCC_Type *ztype, bool allowarraytypes, bool formember)
{
	PType *retval = TypeError;
	if (!allowarraytypes && ztype->ArraySize != nullptr)
	{
		Error(field, "%s: Array type not allowed", name.GetChars());
		return TypeError;
	}
	switch (ztype->NodeType)
	{
	case AST_BasicType:
	{
		auto btype = static_cast<ZCC_BasicType *>(ztype);
		switch (btype->Type)
		{
		case ZCC_SInt8:
			retval = formember? TypeSInt8 : (PType*)TypeError;
			break;

		case ZCC_UInt8:
			retval = formember ? TypeUInt8 : (PType*)TypeError;
			break;

		case ZCC_SInt16:
			retval = formember ? TypeSInt16 : (PType*)TypeError;
			break;

		case ZCC_UInt16:
			retval = formember ? TypeUInt16 : (PType*)TypeError;
			break;

		case ZCC_SInt32:
		case ZCC_IntAuto: // todo: for enums, autoselect appropriately sized int
			retval = TypeSInt32;
			break;

		case ZCC_UInt32:
			retval = TypeUInt32;
			break;

		case ZCC_Bool:
			retval = TypeBool;
			break;

		case ZCC_FloatAuto:
			retval = formember ? TypeFloat32 : TypeFloat64;
			break;

		case ZCC_Float64:
			retval = TypeFloat64;
			break;

		case ZCC_String:
			retval = TypeString;
			break;

		case ZCC_Name:
			retval = TypeName;
			break;

		case ZCC_Vector2:
			retval = TypeVector2;
			break;

		case ZCC_Vector3:
			retval = TypeVector3;
			break;

		case ZCC_State:
			retval = TypeState;
			break;

		case ZCC_Color:
			retval = TypeColor;
			break;

		case ZCC_Sound:
			retval = TypeSound;
			break;

		case ZCC_Let:
			retval = TypeAuto;
			break;

		case ZCC_NativeType:

			// Creating an instance of a native struct is only allowed for internal definitions of native variables.
			if (fileSystem.GetFileContainer(Lump) != 0 || !formember)
			{
				Error(field, "%s: @ not allowed for user scripts", name.GetChars());
			}
			retval = ResolveUserType(btype, outertype? &outertype->Symbols : nullptr, true);
			break;

		case ZCC_UserType:
			// statelabel et.al. are not tokens - there really is no need to, it works just as well as an identifier. Maybe the same should be done for some other types, too?
			switch (btype->UserType->Id)
			{
			case NAME_Voidptr:
				retval = TypeVoidPtr;
				break;

			case NAME_StateLabel:
				retval = TypeStateLabel;
				break;

			case NAME_SpriteID:
				retval = TypeSpriteID;
				break;

			case NAME_TextureID:
				retval = TypeTextureID;
				break;

			default:
				retval = ResolveUserType(btype, outertype ? &outertype->Symbols : nullptr, false);
				break;
			}
			break;

		default:
			break;
		}
		break;
	}

	case AST_MapType:
		if (allowarraytypes)
		{
			Error(field, "%s: Map types not implemented yet", name.GetChars());
			// Todo: Decide what we allow here and if it makes sense to allow more complex constructs.
			auto mtype = static_cast<ZCC_MapType *>(ztype);
			retval = NewMap(DetermineType(outertype, field, name, mtype->KeyType, false, false), DetermineType(outertype, field, name, mtype->ValueType, false, false));
			break;
		}
		break;

	case AST_DynArrayType:
	{
		auto atype = static_cast<ZCC_DynArrayType *>(ztype);
		auto ftype = DetermineType(outertype, field, name, atype->ElementType, false, true);
		if (ftype->GetRegType() == REGT_NIL || ftype->GetRegCount() > 1)
		{
			if (field->NodeType == AST_VarDeclarator && (static_cast<ZCC_VarDeclarator*>(field)->Flags & ZCC_Native) && fileSystem.GetFileContainer(Lump) == 0)
			{
				// the internal definitions may declare native arrays to complex types.
				// As long as they can be mapped to a static array type the VM can handle them, in a limited but sufficient fashion.
				retval = NewPointer(NewStaticArray(ftype), false);
				retval->Size = ~0u;	// don't check for a size match, it's likely to fail anyway.
				retval->Align = ~0u;
			}
			else
			{
				Error(field, "%s: Base type for dynamic array types must be integral, but got %s", name.GetChars(), ftype->DescriptiveName());
			}
		}
		else
		{
			retval = NewDynArray(ftype);
		}
		break;
	}
	case AST_ClassType:
	{
		auto ctype = static_cast<ZCC_ClassType *>(ztype);
		if (ctype->Restriction == nullptr)
		{
			retval = NewClassPointer(RUNTIME_CLASS(DObject));
		}
		else
		{
			// This doesn't check the class list directly but the current symbol table to ensure that
			// this does not reference a type that got shadowed by a more local definition.
			// We first look in the current class and its parents, and then in the current namespace and its parents.
			auto sym = outertype ? outertype->Symbols.FindSymbol(ctype->Restriction->Id, true) : nullptr;
			if (sym == nullptr) sym = OutNamespace->Symbols.FindSymbol(ctype->Restriction->Id, true);
			if (sym == nullptr)
			{
				// A symbol with a given name cannot be reached from this definition point, so
				// even if a class with the given name exists, it is not accessible.
				Error(field, "%s: Unknown identifier", FName(ctype->Restriction->Id).GetChars());
				return TypeError;
			}
			auto typesym = dyn_cast<PSymbolType>(sym);
			if (typesym == nullptr || !typesym->Type->isClass())
			{
				Error(field, "%s does not represent a class type", FName(ctype->Restriction->Id).GetChars());
				return TypeError;
			}
			if (typesym->Type->mVersion > mVersion)
			{
				Error(field, "Class %s not accessible to ZScript version %d.%d.%d", FName(ctype->Restriction->Id).GetChars(), mVersion.major, mVersion.minor, mVersion.revision);
				return TypeError;
			}
			retval = NewClassPointer(static_cast<PClassType *>(typesym->Type)->Descriptor);
		}
		break;
	}

	default:
		break;
	}
	if (retval != TypeError && retval->MemberOnly && !formember)
	{
		Error(field, "Invalid type %s", retval->DescriptiveName());
		return TypeError;
	}
	return retval;
}

//==========================================================================
//
// ZCCCompiler :: ResolveUserType
//
// resolves a user type and returns a matching PType
//
//==========================================================================

PType *ZCCCompiler::ResolveUserType(ZCC_BasicType *type, PSymbolTable *symt, bool nativetype)
{
	// Check the symbol table for the identifier.
	PSymbol *sym = nullptr;
	
	// We first look in the current class and its parents, and then in the current namespace and its parents.
	if (symt != nullptr) sym = symt->FindSymbol(type->UserType->Id, true);
	if (sym == nullptr) sym = OutNamespace->Symbols.FindSymbol(type->UserType->Id, true);
	if (sym != nullptr && sym->IsKindOf(RUNTIME_CLASS(PSymbolType)))
	{
		auto ptype = static_cast<PSymbolType *>(sym)->Type;
		if (ptype->mVersion > mVersion)
		{
			Error(type, "Type %s not accessible to ZScript version %d.%d.%d", FName(type->UserType->Id).GetChars(), mVersion.major, mVersion.minor, mVersion.revision);
			return TypeError;
		}

		if (ptype->isEnum())
		{
			if (!nativetype) return TypeSInt32;	// hack this to an integer until we can resolve the enum mess.
		}
		else if (ptype->isClass()) // classes cannot be instantiated at all, they always get used as references.
		{
			return NewPointer(ptype, type->isconst);
		}
		else if (ptype->isStruct() && static_cast<PStruct*>(ptype)->isNative)	// native structs and classes cannot be instantiated, they always get used as reference.
		{
			if (!nativetype) return NewPointer(ptype, type->isconst);
			return ptype;	// instantiation of native structs. Only for internal use.
		}
		if (!nativetype) return ptype;
	}
	Error(type, "Unable to resolve %s%s as type.", nativetype? "@" : "", FName(type->UserType->Id).GetChars());
	return TypeError;
}


//==========================================================================
//
// ZCCCompiler :: ResolveArraySize
//
// resolves the array size and returns a matching type.
//
//==========================================================================

PType *ZCCCompiler::ResolveArraySize(PType *baseType, ZCC_Expression *arraysize, PContainerType *cls, bool *nosize)
{
	TArray<ZCC_Expression *> indices;

	// Simplify is too broken to resolve this inside the ring list so unravel the list into an array before starting to simplify its components.
	auto node = arraysize;
	do
	{
		indices.Push(node);
		node = static_cast<ZCC_Expression*>(node->SiblingNext);
	} while (node != arraysize);

	if (indices.Size() == 1 && indices [0]->Operation == PEX_Nil)
	{
		*nosize = true;
		return baseType;
	}

	if (mVersion >= MakeVersion(3, 7, 2))
	{
		TArray<ZCC_Expression *> fixedIndices;
		for (auto node : indices)
		{
			fixedIndices.Insert (0, node);
		}

		indices = std::move(fixedIndices);
	}

	FCompileContext ctx(OutNamespace, cls, false);
	for (auto node : indices)
	{
		// There is no float->int casting here.
		FxExpression *ex = ConvertNode(node);
		ex = ex->Resolve(ctx);

		if (ex == nullptr) return TypeError;
		if (!ex->isConstant() || !ex->ValueType->isInt())
		{
			Error(arraysize, "Array index must be an integer constant");
			return TypeError;
		}
		int size = static_cast<FxConstant*>(ex)->GetValue().GetInt();
		if (size < 1)
		{
			Error(arraysize, "Array size must be positive");
			return TypeError;
		}
		baseType = NewArray(baseType, size);
	}

	*nosize = false;
	return baseType;
}

//==========================================================================
//
// SetImplicitArgs
//
// Adds the parameters implied by the function flags.
//
//==========================================================================

void ZCCCompiler::SetImplicitArgs(TArray<PType*>* args, TArray<uint32_t>* argflags, TArray<FName>* argnames, PContainerType* cls, uint32_t funcflags, int useflags)
{
	// Must be called before adding any other arguments.
	assert(args == nullptr || args->Size() == 0);
	assert(argflags == nullptr || argflags->Size() == 0);

	if (funcflags & VARF_Method)
	{
		// implied self pointer
		if (args != nullptr)		args->Push(NewPointer(cls, !!(funcflags & VARF_ReadOnly)));
		if (argflags != nullptr)	argflags->Push(VARF_Implicit | VARF_ReadOnly);
		if (argnames != nullptr)	argnames->Push(NAME_self);
	}
}


void ZCCCompiler::InitDefaults()
{
	for (auto c : Classes)
	{
		if (c->ClassType()->ParentClass)
		{
			auto ti = c->ClassType();
			ti->InitializeDefaults();
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void ZCCCompiler::CompileFunction(ZCC_StructWork *c, ZCC_FuncDeclarator *f, bool forclass)
{
	TArray<PType *> rets(1);
	TArray<PType *> args;
	TArray<uint32_t> argflags;
	TArray<TypedVMValue> argdefaults;
	TArray<FName> argnames;

	rets.Clear();
	args.Clear();
	argflags.Clear();
	bool hasdefault = false;
	// For the time being, let's not allow overloading. This may be reconsidered later but really just adds an unnecessary amount of complexity here.
	if (AddTreeNode(f->Name, f, &c->TreeNodes, false))
	{
		auto t = f->Type;
		if (t != nullptr)
		{
			do
			{
				auto type = DetermineType(c->Type(), f, f->Name, t, false, false);
				if (type->isContainer() && type != TypeVector2 && type != TypeVector3)
				{
					// structs and classes only get passed by pointer.
					type = NewPointer(type);
				}
				else if (type->isDynArray())
				{
					Error(f, "The return type of a function cannot be a dynamic array");
					break;
				}
				// TBD: disallow certain types? For now, let everything pass that isn't an array.
				rets.Push(type);
				t = static_cast<decltype(t)>(t->SiblingNext);
			} while (t != f->Type);
		}

		int notallowed = ZCC_Latent | ZCC_Meta | ZCC_ReadOnly | ZCC_Internal;

		if (f->Flags & notallowed)
		{
			Error(f, "Invalid qualifiers for %s (%s not allowed)", FName(f->Name).GetChars(), FlagsToString(f->Flags & notallowed).GetChars());
			f->Flags &= notallowed;
		}
		uint32_t varflags = VARF_Method;
		int implicitargs = 1;
		AFuncDesc *afd = nullptr;
		int useflags = SUF_ACTOR | SUF_OVERLAY | SUF_WEAPON | SUF_ITEM;
		if (f->UseFlags != nullptr)
		{
			useflags = 0;
			auto p = f->UseFlags;
			do
			{
				switch (p->Id)
				{
				case NAME_Actor:
					useflags |= SUF_ACTOR;
					break;
				case NAME_Overlay:
					useflags |= SUF_OVERLAY;
					break;
				case NAME_Weapon:
					useflags |= SUF_WEAPON;
					break;
				case NAME_Item:
					useflags |= SUF_ITEM;
					break;
				default:
					Error(p, "Unknown Action qualifier %s", FName(p->Id).GetChars());
					break;
				}

				p = static_cast<decltype(p)>(p->SiblingNext);
			} while (p != f->UseFlags);
		}

		// map to implementation flags.
		if (f->Flags & ZCC_Private) varflags |= VARF_Private;
		if (f->Flags & ZCC_Protected) varflags |= VARF_Protected;
		if (f->Flags & ZCC_Deprecated) varflags |= VARF_Deprecated;
		if (f->Flags & ZCC_Virtual) varflags |= VARF_Virtual;
		if (f->Flags & ZCC_Override) varflags |= VARF_Override;
		if (f->Flags & ZCC_Abstract) varflags |= VARF_Abstract;
		if (f->Flags & ZCC_VarArg) varflags |= VARF_VarArg;
		if (f->Flags & ZCC_FuncConst) varflags |= VARF_ReadOnly; // FuncConst method is internally marked as VARF_ReadOnly
		if (mVersion >= MakeVersion(2, 4, 0))
		{
			if (c->Type()->ScopeFlags & Scope_UI)
				varflags |= VARF_UI;
			if (c->Type()->ScopeFlags & Scope_Play)
				varflags |= VARF_Play;
			//if (f->Flags & ZCC_FuncConst)
			//	varflags = FScopeBarrier::ChangeSideInFlags(varflags, FScopeBarrier::Side_PlainData); // const implies clearscope. this is checked a bit later to also not have ZCC_Play/ZCC_UIFlag.
			if (f->Flags & ZCC_UIFlag)
				varflags = FScopeBarrier::ChangeSideInFlags(varflags, FScopeBarrier::Side_UI);
			if (f->Flags & ZCC_Play)
				varflags = FScopeBarrier::ChangeSideInFlags(varflags, FScopeBarrier::Side_Play);
			if (f->Flags & ZCC_ClearScope)
				varflags = FScopeBarrier::ChangeSideInFlags(varflags, FScopeBarrier::Side_Clear);
			if (f->Flags & ZCC_VirtualScope)
				varflags = FScopeBarrier::ChangeSideInFlags(varflags, FScopeBarrier::Side_Virtual);
		}
		else
		{
			varflags |= VARF_Play;
		}

		if ((f->Flags & ZCC_VarArg) && !(f->Flags & ZCC_Native))
		{
			Error(f, "'VarArg' can only be used with native methods");
		}
		if (f->Flags & ZCC_Action)
		{
			implicitargs = CheckActionKeyword(f,varflags, useflags, c);
			if (implicitargs < 0)
			{
				Error(f, "'Action' not allowed as a function qualifier");
				// Set state to allow continued compilation to find more errors.
				varflags &= ~VARF_ReadOnly;
				implicitargs = 1;
			}
		}
		if (f->Flags & ZCC_Static) varflags = (varflags & ~VARF_Method) | VARF_Final, implicitargs = 0;	// Static implies Final.

		if (varflags & VARF_Override) varflags &= ~VARF_Virtual;	// allow 'virtual override'.
																	// Only one of these flags may be used.
		static int exclude[] = { ZCC_Abstract, ZCC_Virtual, ZCC_Override, ZCC_Action, ZCC_Static };
		int excludeflags = 0;
		int fc = 0;
		for (size_t i = 0; i < countof(exclude); i++)
		{
			if (f->Flags & exclude[i])
			{
				fc++;
				excludeflags |= exclude[i];
			}
		}
		if (fc > 1)
		{
			Error(f, "Invalid combination of qualifiers %s on function %s", FlagsToString(excludeflags).GetChars(), FName(f->Name).GetChars());
			varflags |= VARF_Method;
		}
		if (varflags & (VARF_Override | VARF_Abstract)) varflags |= VARF_Virtual;	// Now that the flags are checked, make all override and abstract functions virtual as well.

		// [ZZ] this doesn't make sense either.
		if ((varflags&(VARF_ReadOnly | VARF_Method)) == VARF_ReadOnly) // non-method const function
		{
			Error(f, "'Const' on a static method is not supported");
		}

		// [ZZ] neither this
		if ((varflags&(VARF_VirtualScope | VARF_Method)) == VARF_VirtualScope) // non-method virtualscope function
		{
			Error(f, "'VirtualScope' on a static method is not supported");
		}

		static int excludescope[] = { ZCC_UIFlag, ZCC_Play, ZCC_ClearScope, ZCC_VirtualScope };
		excludeflags = 0;
		fc = 0;
		for (size_t i = 0; i < countof(excludescope); i++)
		{
			if (f->Flags & excludescope[i])
			{
				fc++;
				excludeflags |= excludescope[i];
			}
		}
		if (fc > 1)
		{
			Error(f, "Invalid combination of scope qualifiers %s on function %s", FlagsToString(excludeflags).GetChars(), FName(f->Name).GetChars());
			varflags &= ~(VARF_UI | VARF_Play); // make plain data
		}

		if (f->Flags & ZCC_Native)
		{
			if (varflags & VARF_Abstract)
			{
				Error(f, "Native functions cannot be abstract");
				return;
			}

			varflags |= VARF_Native;
			afd = FindFunction(c->Type(), FName(f->Name).GetChars());
			if (afd == nullptr)
			{
				Error(f, "The function '%s.%s' has not been exported from the executable", c->Type()->TypeName.GetChars(), FName(f->Name).GetChars());
			}
			else
			{
				(*afd->VMPointer)->ImplicitArgs = uint8_t(implicitargs);
			}
		}
		SetImplicitArgs(&args, &argflags, &argnames, c->Type(), varflags, useflags);
		argdefaults.Resize(argnames.Size());
		auto p = f->Params;
		bool hasoptionals = false;
		if (p != nullptr)
		{
            bool overridemsg = false;
			do
			{
				int elementcount = 1;
				TypedVMValue vmval[3];	// default is REGT_NIL which means 'no default value' here.
				if (p->Type != nullptr)
				{
					auto type = DetermineType(c->Type(), p, f->Name, p->Type, false, false);
					int flags = 0;
					if ((type->isStruct() && type != TypeVector2 && type != TypeVector3) || type->isDynArray())
					{
						// Structs are being passed by pointer, but unless marked 'out' that pointer must be readonly.
						type = NewPointer(type /*, !(p->Flags & ZCC_Out)*/);
						flags |= VARF_Ref;
					}
					else if (type->GetRegType() != REGT_NIL)
					{
						if (p->Flags & ZCC_Out)	flags |= VARF_Out;
						if (type == TypeVector2)
						{
							elementcount = 2;
						}
						else if (type == TypeVector3)
						{
							elementcount = 3;
						}
					}
					if (type->GetRegType() == REGT_NIL && type != TypeVector2 && type != TypeVector3)
					{
						Error(p, "Invalid type %s for function parameter", type->DescriptiveName());
					}
					else if (p->Default != nullptr)
					{
						if (flags & VARF_Out)
						{
							Error(p, "Out parameters cannot have default values");
						}

						flags |= VARF_Optional;
						hasoptionals = true;
						
						if ((varflags & VARF_Override) && !overridemsg)
						{
							// This is illegal, but in older compilers wasn't checked, so there it has to be demoted to a warning.
							// Virtual calls always need to get their defaults from the base virtual method.
							if (mVersion >= MakeVersion(3, 3))
							{
								Error(p, "Default values for parameter of virtual override not allowed");
							}
							else
							{
								Warn(p, "Default values for parameter of virtual override will be ignored!");
							}
							overridemsg = true;
						}


						FxExpression *x = new FxTypeCast(ConvertNode(p->Default), type, false);
						FCompileContext ctx(OutNamespace, c->Type(), false);
						x = x->Resolve(ctx);

						if (x != nullptr)
						{
							// Vectors need special treatment because they use more than one entry in the Defaults and do not report as actual constants
							if (type == TypeVector2 && x->ExprType == EFX_VectorValue && static_cast<FxVectorValue *>(x)->isConstVector(2))
							{
								auto vx = static_cast<FxVectorValue *>(x);
								vmval[0] = static_cast<FxConstant *>(vx->xyz[0])->GetValue().GetFloat();
								vmval[1] = static_cast<FxConstant *>(vx->xyz[1])->GetValue().GetFloat();
							}
							else if (type == TypeVector3 && x->ExprType == EFX_VectorValue && static_cast<FxVectorValue *>(x)->isConstVector(3))
							{
								auto vx = static_cast<FxVectorValue *>(x);
								vmval[0] = static_cast<FxConstant *>(vx->xyz[0])->GetValue().GetFloat();
								vmval[1] = static_cast<FxConstant *>(vx->xyz[1])->GetValue().GetFloat();
								vmval[2] = static_cast<FxConstant *>(vx->xyz[2])->GetValue().GetFloat();
							}
							else if (!x->isConstant())
							{
								Error(p, "Default parameter %s is not constant in %s", FName(p->Name).GetChars(), FName(f->Name).GetChars());
							}
							else  if (x->ValueType != type)
							{
								Error(p, "Default parameter %s could not be converted to target type %s", FName(p->Name).GetChars(), c->Type()->TypeName.GetChars());
							}
							else
							{
								auto cnst = static_cast<FxConstant *>(x);
								switch (type->GetRegType())
								{
								case REGT_INT:
									vmval[0] = cnst->GetValue().GetInt();
									break;

								case REGT_FLOAT:
									vmval[0] = cnst->GetValue().GetFloat();
									break;

								case REGT_POINTER:
									if (type->isClassPointer())
										vmval[0] = (DObject*)cnst->GetValue().GetPointer();
									else
										vmval[0] = cnst->GetValue().GetPointer();
									break;

								case REGT_STRING:
									// We need a reference to something permanently stored here.
									vmval[0] = VMStringConstants.Alloc(cnst->GetValue().GetString());
									break;

								default:
									assert(0 && "no valid type for constant");
									break;
								}
							}

							hasdefault = true;
						}
						if (x != nullptr) delete x;
					}
					else if (hasoptionals)
					{
						Error(p, "All arguments after the first optional one need also be optional");
					}
					// TBD: disallow certain types? For now, let everything pass that isn't an array.
					args.Push(type);
					argflags.Push(flags);
					argnames.Push(p->Name);

				}
				else
				{
					args.Push(nullptr);
					argflags.Push(0);
					argnames.Push(NAME_None);
				}
				for (int i = 0; i<elementcount; i++) argdefaults.Push(vmval[i]);
				p = static_cast<decltype(p)>(p->SiblingNext);
			} while (p != f->Params);
		}

		PFunction *sym = Create<PFunction>(c->Type(), f->Name);
		sym->AddVariant(NewPrototype(rets, args), argflags, argnames, afd == nullptr ? nullptr : *(afd->VMPointer), varflags, useflags);
		c->Type()->Symbols.ReplaceSymbol(sym);

		if (f->DeprecationMessage != nullptr)
		{
			sym->Variants[0].DeprecationMessage = *f->DeprecationMessage;
		}

		auto vcls = PType::toClass(c->Type());
		auto cls = vcls ? vcls->Descriptor : nullptr;
		PFunction *virtsym = nullptr;
		if (cls != nullptr && cls->ParentClass != nullptr) virtsym = dyn_cast<PFunction>(cls->ParentClass->VMType->Symbols.FindSymbol(FName(f->Name), true));
		unsigned vindex = ~0u;
		if (virtsym != nullptr)
		{
			auto imp = virtsym->Variants[0].Implementation;
			if (imp != nullptr) vindex = imp->VirtualIndex;
			else Error(f, "Virtual base function %s not found in %s", FName(f->Name).GetChars(), cls->ParentClass->TypeName.GetChars());
		}
		if (f->Flags & (ZCC_Version | ZCC_Deprecated))
		{
			sym->mVersion = f->Version;
			if (varflags & VARF_Override)
			{
				Error(f, "Overridden function %s may not alter version restriction in %s", FName(f->Name).GetChars(), cls->ParentClass->TypeName.GetChars());
			}
		}

		VMFunction *newfunc = nullptr;

		if (!(f->Flags & ZCC_Native))
		{
			if (f->Body != nullptr && (varflags & VARF_Abstract))
			{
				Error(f, "Abstract function %s cannot have a body", FName(f->Name).GetChars());
				return;
			}

			if (f->Body == nullptr && !(varflags & VARF_Abstract))
			{
				Error(f, "Empty function %s", FName(f->Name).GetChars());
				return;
			}

			FxExpression* code = f->Body != nullptr ? ConvertAST(c->Type(), f->Body) : nullptr;
			newfunc = FunctionBuildList.AddFunction(OutNamespace, mVersion, sym, code, FStringf("%s.%s", c->Type()->TypeName.GetChars(), FName(f->Name).GetChars()), false, -1, 0, Lump);
		}
		if (sym->Variants[0].Implementation != nullptr && hasdefault)	// do not copy empty default lists, they only waste space and processing time.
		{
			sym->Variants[0].Implementation->DefaultArgs = std::move(argdefaults);
		}

		if (sym->Variants[0].Implementation != nullptr)
		{
			// [ZZ] unspecified virtual function inherits old scope. virtual function scope can't be changed.
			sym->Variants[0].Implementation->VarFlags = sym->Variants[0].Flags;
		}

		bool exactReturnType = mVersion < MakeVersion(4, 4);
		PClass *clstype = forclass? static_cast<PClassType *>(c->Type())->Descriptor : nullptr;
		if (varflags & VARF_Virtual)
		{
			if (sym->Variants[0].Implementation == nullptr)
			{
				Error(f, "Virtual function %s.%s not present", c->Type()->TypeName.GetChars(), FName(f->Name).GetChars());
				return;
			}

			if (forclass)
			{
				if ((varflags & VARF_Abstract) && !clstype->bAbstract)
				{
					Error(f, "Abstract functions can only be defined in abstract classes");
					return;
				}

				auto parentfunc = clstype->ParentClass? dyn_cast<PFunction>(clstype->ParentClass->VMType->Symbols.FindSymbol(sym->SymbolName, true)) : nullptr;

				int vindex = clstype->FindVirtualIndex(sym->SymbolName, &sym->Variants[0], parentfunc, exactReturnType);
				// specifying 'override' is necessary to prevent one of the biggest problem spots with virtual inheritance: Mismatching argument types.
				if (varflags & VARF_Override)
				{
					if (vindex == -1)
					{
						Error(f, "Attempt to override non-existent virtual function %s", FName(f->Name).GetChars());
					}
					else
					{
						auto oldfunc = clstype->Virtuals[vindex];
						if (parentfunc && parentfunc->mVersion > mVersion)
						{
							Error(f, "Attempt to override function %s which is incompatible with version %d.%d.%d", FName(f->Name).GetChars(), mVersion.major, mVersion.minor, mVersion.revision);
						}
						if (oldfunc->VarFlags & VARF_Final)
						{
							Error(f, "Attempt to override final function %s", FName(f->Name).GetChars());
						}
						// you can't change ui/play/clearscope for a virtual method.
						if (f->Flags & (ZCC_UIFlag|ZCC_Play|ZCC_ClearScope|ZCC_VirtualScope))
						{
							Error(f, "Attempt to change scope for virtual function %s", FName(f->Name).GetChars());
						}
						// you can't change const qualifier for a virtual method
						if ((sym->Variants[0].Implementation->VarFlags & VARF_ReadOnly) && !(oldfunc->VarFlags & VARF_ReadOnly))
						{
							Error(f, "Attempt to add const qualifier to virtual function %s", FName(f->Name).GetChars());
						}
						// you can't change protected qualifier for a virtual method (i.e. putting private), because this cannot be reliably checked without runtime stuff
						if (f->Flags & (ZCC_Private | ZCC_Protected))
						{
							Error(f, "Attempt to change private/protected qualifiers for virtual function %s", FName(f->Name).GetChars());
						}
						// inherit scope of original function if override not specified
						sym->Variants[0].Flags = FScopeBarrier::ChangeSideInFlags(sym->Variants[0].Flags, FScopeBarrier::SideFromFlags(oldfunc->VarFlags));
						// inherit const from original function
						if (oldfunc->VarFlags & VARF_ReadOnly)
							sym->Variants[0].Flags |= VARF_ReadOnly;
						if (oldfunc->VarFlags & VARF_Protected)
							sym->Variants[0].Flags |= VARF_Protected;
													
						clstype->Virtuals[vindex] = sym->Variants[0].Implementation;
						sym->Variants[0].Implementation->VirtualIndex = vindex;
						sym->Variants[0].Implementation->VarFlags = sym->Variants[0].Flags;
						
						// Defaults must be identical to parent class
						if (parentfunc->Variants[0].Implementation->DefaultArgs.Size() > 0)
						{
							sym->Variants[0].Implementation->DefaultArgs = parentfunc->Variants[0].Implementation->DefaultArgs;
							sym->Variants[0].ArgFlags = parentfunc->Variants[0].ArgFlags;
						}

						// Update argument flags for VM function if needed as their list could be incomplete
						// At the moment of function creation, arguments with default values were not copied yet from the parent function
						if (newfunc != nullptr && sym->Variants[0].ArgFlags.Size() != newfunc->ArgFlags.Size())
						{
							for (unsigned i = newfunc->ArgFlags.Size(), count = sym->Variants[0].ArgFlags.Size(); i < count; ++i)
							{
								newfunc->ArgFlags.Push(sym->Variants[0].ArgFlags[i]);
							}

							newfunc->Proto = sym->Variants[0].Proto;
						}
					}
				}
				else
				{
					if (vindex != -1)
					{
						Error(f, "Function %s attempts to override parent function without 'override' qualifier", FName(f->Name).GetChars());
					}
					sym->Variants[0].Implementation->VirtualIndex = clstype->Virtuals.Push(sym->Variants[0].Implementation);
				}
			}
			else
			{
				Error(p, "Virtual functions can only be defined for classes");
			}
		}
		else if (forclass)
		{
			int vindex = clstype->FindVirtualIndex(sym->SymbolName, &sym->Variants[0], nullptr, exactReturnType);
			if (vindex != -1)
			{
				Error(f, "Function %s attempts to override parent function without 'override' qualifier", FName(f->Name).GetChars());
			}
		}
	}
}

//==========================================================================
//
// Parses the functions list
//
//==========================================================================

void ZCCCompiler::InitFunctions()
{
	for (auto s : Structs)
	{
		for (auto f : s->Functions)
		{
			CompileFunction(s, f, false);
		}
	}

	for (auto c : Classes)
	{
		// cannot be done earlier because it requires the parent class to be processed by this code, too.
		if (c->ClassType()->ParentClass != nullptr)
		{
			c->ClassType()->Virtuals = c->ClassType()->ParentClass->Virtuals;
		}
		for (auto f : c->Functions)
		{
			CompileFunction(c, f, true);
		}

		// [Player701] Make sure all abstract functions are overridden
		if (!c->ClassType()->bAbstract)
		{
			for (auto v : c->ClassType()->Virtuals)
			{
				if (v->VarFlags & VARF_Abstract)
				{
					Error(c->cls, "Non-abstract class %s must override abstract function %s", c->Type()->TypeName.GetChars(), v->PrintableName.GetChars());
				}
			}
		}
	}
}

//==========================================================================
//
// Convert the AST data for the code generator.
//
//==========================================================================

FxExpression *ZCCCompiler::ConvertAST(PContainerType *cls, ZCC_TreeNode *ast)
{
	ConvertClass = cls;
	// there are two possibilities here: either a single function call or a compound statement. For a compound statement we also need to check if the last thing added was a return.
	if (ast->NodeType == AST_ExprFuncCall)
	{
		auto cp = new FxCompoundStatement(*ast);
		cp->Add(new FxReturnStatement(ConvertNode(ast), *ast));
		return cp;
	}
	else
	{
		// This must be done here so that we can check for a trailing return statement.
		auto x = new FxCompoundStatement(*ast);
		auto compound = static_cast<ZCC_CompoundStmt *>(ast);
		//bool isreturn = false;
		auto node = compound->Content;
		if (node != nullptr) do
		{
			x->Add(ConvertNode(node));
			//isreturn = node->NodeType == AST_ReturnStmt;
			node = static_cast<decltype(node)>(node->SiblingNext);
		} while (node != compound->Content);
		//if (!isreturn) x->Add(new FxReturnStatement(nullptr, *ast));
		return x;
	}
}


#define xx(a,z)	z,
static int Pex2Tok[] = {
#include "zcc_exprlist.h"
};

//==========================================================================
//
// Helper for modify/assign operators
//
//==========================================================================

static FxExpression *ModifyAssign(FxBinary *operation, FxExpression *left)
{
	auto assignself = static_cast<FxAssignSelf *>(operation->left);
	auto assignment = new FxAssign(left, operation, true);
	assignself->Assignment = assignment;
	return assignment;
}


//==========================================================================
//
// Convert an AST node and its children
//
//==========================================================================

FxExpression *ZCCCompiler::ConvertNode(ZCC_TreeNode *ast, bool substitute)
{
	if (ast == nullptr) return nullptr;

	switch (ast->NodeType)
	{
	case AST_ExprFuncCall:
	{
		auto fcall = static_cast<ZCC_ExprFuncCall *>(ast);

		// function names can either be
		// - plain identifiers
		// - class members
		// - array syntax for random() calls.
		// Everything else coming here is a syntax error.
		FArgumentList args;
		switch (fcall->Function->NodeType)
		{
		case AST_ExprID:
			// The function name is a simple identifier.
			return new FxFunctionCall(static_cast<ZCC_ExprID *>(fcall->Function)->Identifier, NAME_None, ConvertNodeList(args, fcall->Parameters), *ast);

		case AST_ExprMemberAccess:
		{
			auto ema = static_cast<ZCC_ExprMemberAccess *>(fcall->Function);
			return new FxMemberFunctionCall(ConvertNode(ema->Left, true), ema->Right, ConvertNodeList(args, fcall->Parameters), *ast);
		}

		case AST_ExprBinary:
			// Array syntax for randoms. They are internally stored as ExprBinary with both an identifier on the left and right side.
			if (fcall->Function->Operation == PEX_ArrayAccess)
			{
				auto binary = static_cast<ZCC_ExprBinary *>(fcall->Function);
				if (binary->Left->NodeType == AST_ExprID && binary->Right->NodeType == AST_ExprID)
				{
					return new FxFunctionCall(static_cast<ZCC_ExprID *>(binary->Left)->Identifier, static_cast<ZCC_ExprID *>(binary->Right)->Identifier, ConvertNodeList(args, fcall->Parameters), *ast);
				}
			}
			// fall through if this isn't an array access node.
			[[fallthrough]];

		default:
			Error(fcall, "Invalid function identifier");
			return new FxNop(*ast);	// return something so that the compiler can continue.
		}
		break;
	}

	case AST_ClassCast:
	{
		auto cc = static_cast<ZCC_ClassCast *>(ast);
		if (cc->Parameters == nullptr || cc->Parameters->SiblingNext != cc->Parameters)
		{
			Error(cc, "Class type cast requires exactly one parameter");
			return new FxNop(*ast);	// return something so that the compiler can continue.
		}
		auto cls = PClass::FindClass(cc->ClassName);
		if (cls == nullptr || cls->VMType == nullptr)
		{
			Error(cc, "Unknown class %s", FName(cc->ClassName).GetChars());
			return new FxNop(*ast);	// return something so that the compiler can continue.
		}
		return new FxClassPtrCast(cls, ConvertNode(cc->Parameters));
	}

	case AST_StaticArrayStatement:
	{
		auto sas = static_cast<ZCC_StaticArrayStatement *>(ast);
		PType *ztype = DetermineType(ConvertClass, sas, sas->Id, sas->Type, false, false);
		FArgumentList args;
		ConvertNodeList(args, sas->Values);
		// This has to let the code generator resolve the constants, not the Simplifier, which lacks all the necessary type info.
		return new FxStaticArray(ztype, sas->Id, args, *ast);
	}

	case AST_ExprMemberAccess:
	{
		auto memaccess = static_cast<ZCC_ExprMemberAccess *>(ast);
		return new FxMemberIdentifier(ConvertNode(memaccess->Left), memaccess->Right, *ast);
	}

	case AST_FuncParm:
	{
		auto fparm = static_cast<ZCC_FuncParm *>(ast);
		auto node = ConvertNode(fparm->Value);
		if (fparm->Label != NAME_None) node = new FxNamedNode(fparm->Label, node, *ast);
		return node;
	}

	case AST_ExprID:
	{
		auto id = static_cast<ZCC_ExprID *>(ast)->Identifier;
		if (id == NAME_LevelLocals && substitute) id = NAME_Level;	// All static methods of FLevelLocals are now non-static so remap the name right here before passing it to the backend.
		return new FxIdentifier(id, *ast);
	}

	case AST_ExprConstant:
	{
		auto cnst = static_cast<ZCC_ExprConstant *>(ast);
		if (cnst->Type == TypeName)
		{
			return new FxConstant(FName(ENamedName(cnst->IntVal)), *ast);
		}
		else if (cnst->Type->isInt())
		{
			return new FxConstant(cnst->IntVal, *ast);
		}
		else if (cnst->Type == TypeBool)
		{
			return new FxConstant(!!cnst->IntVal, *ast);
		}
		else if (cnst->Type->isFloat())
		{
			return new FxConstant(cnst->DoubleVal, *ast);
		}
		else if (cnst->Type == TypeString)
		{
			return new FxConstant(*cnst->StringVal, *ast);
		}
		else if (cnst->Type == TypeNullPtr)
		{
			return new FxConstant(*ast);
		}
		else
		{
			// can there be other types?
			Error(cnst, "Unknown constant type %s", cnst->Type->DescriptiveName());
			return new FxConstant(0, *ast);
		}
	}

	case AST_ExprUnary:
	{
		auto unary = static_cast<ZCC_ExprUnary *>(ast);
		auto operand = ConvertNode(unary->Operand);
		auto op = unary->Operation;
		switch (op)
		{
		case PEX_PostDec:
		case PEX_PostInc:
			return new FxPostIncrDecr(operand, Pex2Tok[op]);

		case PEX_PreDec:
		case PEX_PreInc:
			return new FxPreIncrDecr(operand, Pex2Tok[op]);

		case PEX_Negate:
			return new FxMinusSign(operand);

		case PEX_AntiNegate:
			return new FxPlusSign(operand);

		case PEX_BitNot:
			return new FxUnaryNotBitwise(operand);

		case PEX_BoolNot:
			return new FxUnaryNotBoolean(operand);

		case PEX_SizeOf:
		case PEX_AlignOf:
			return new FxSizeAlign(operand, Pex2Tok[op]);

		default:
			assert(0 && "Unknown unary operator.");	// should never happen
			Error(unary, "Unknown unary operator ID #%d", op);
			return new FxNop(*ast);
		}
		break;
	}


	case AST_ExprBinary:
	{
		auto binary = static_cast<ZCC_ExprBinary *>(ast);
		auto left = ConvertNode(binary->Left);
		auto right = ConvertNode(binary->Right);
		auto op = binary->Operation;
		auto tok = Pex2Tok[op];
		switch (op)
		{
		case PEX_Add:
		case PEX_Sub:
			return new FxAddSub(tok, left, right);

		case PEX_Mul:
		case PEX_Div:
		case PEX_Mod:
			return new FxMulDiv(tok, left, right);

		case PEX_Pow:
			return new FxPow(left, right);

		case PEX_LeftShift:
		case PEX_RightShift:
		case PEX_URightShift:
			return new FxShift(tok, left, right);

		case PEX_BitAnd:
		case PEX_BitOr:
		case PEX_BitXor:
			return new FxBitOp(tok, left, right);

		case PEX_BoolOr:
		case PEX_BoolAnd:
			return new FxBinaryLogical(tok, left, right);

		case PEX_LT:
		case PEX_LTEQ:
		case PEX_GT:
		case PEX_GTEQ:
			return new FxCompareRel(tok, left, right);

		case PEX_EQEQ:
		case PEX_NEQ:
		case PEX_APREQ:
			return new FxCompareEq(tok, left, right);

		case PEX_Assign:
			return new FxAssign(left, right);

		case PEX_AddAssign:
		case PEX_SubAssign:
			return ModifyAssign(new FxAddSub(tok, new FxAssignSelf(*ast), right), left);

		case PEX_MulAssign:
		case PEX_DivAssign:
		case PEX_ModAssign:
			return ModifyAssign(new FxMulDiv(tok, new FxAssignSelf(*ast), right), left);

		case PEX_LshAssign:
		case PEX_RshAssign:
		case PEX_URshAssign:
			return ModifyAssign(new FxShift(tok, new FxAssignSelf(*ast), right), left);

		case PEX_AndAssign:
		case PEX_OrAssign:
		case PEX_XorAssign:
			return ModifyAssign(new FxBitOp(tok, new FxAssignSelf(*ast), right), left);

		case PEX_LTGTEQ:
			return new FxLtGtEq(left, right);

		case PEX_ArrayAccess:
			return new FxArrayElement(left, right);

		case PEX_CrossProduct:
		case PEX_DotProduct:
			return new FxDotCross(tok, left, right);

		case PEX_Is:
			return new FxTypeCheck(left, right);

		case PEX_Concat:
			return new FxConcat(left, right);

		default:
			I_Error("Binary operator %d not implemented yet", op);
		}
		break;
	}

	case AST_ExprTrinary:
	{
		auto trinary = static_cast<ZCC_ExprTrinary *>(ast);
		auto condition = ConvertNode(trinary->Test);
		auto left = ConvertNode(trinary->Left);
		auto right = ConvertNode(trinary->Right);

		return new FxConditional(condition, left, right);
	}

	case AST_VectorValue:
	{
		auto vecini = static_cast<ZCC_VectorValue *>(ast);
		auto xx = ConvertNode(vecini->X);
		auto yy = ConvertNode(vecini->Y);
		auto zz = ConvertNode(vecini->Z);
		return new FxVectorValue(xx, yy, zz, *ast);
	}

	case AST_LocalVarStmt:
	{
		auto loc = static_cast<ZCC_LocalVarStmt *>(ast);
		auto node = loc->Vars;
		FxSequence *list = new FxSequence(*ast);

		PType *ztype = DetermineType(ConvertClass, node, node->Name, loc->Type, true, false);

		if (loc->Type->ArraySize != nullptr)
		{
			bool nosize;
			ztype = ResolveArraySize(ztype, loc->Type->ArraySize, ConvertClass, &nosize);

			if (nosize)
			{
				Error(node, "Must specify array size");
			}
		}

		do
		{
			PType *type;

			bool nosize = false;
			if (node->ArraySize != nullptr)
			{
				type = ResolveArraySize(ztype, node->ArraySize, ConvertClass, &nosize);

				if (nosize && !node->InitIsArray)
				{
					Error(node, "Must specify array size for non-initialized arrays");
				}
			}
			else
			{
				type = ztype;
			}

			if (node->InitIsArray && (type->isArray() || type->isDynArray() || nosize))
			{
				auto arrtype = static_cast<PArray *>(type);
				if (!nosize && (arrtype->ElementType->isArray() || arrtype->ElementType->isDynArray()))
				{
					Error(node, "Compound initializer not implemented yet for multi-dimensional arrays");
				}
				FArgumentList args;
				ConvertNodeList(args, node->Init);

				if (nosize)
				{
					type = NewArray(type, args.Size());
				}
				list->Add(new FxLocalArrayDeclaration(type, node->Name, args, 0, *node));
			}
			else
			{
				FxExpression *val = node->Init ? ConvertNode(node->Init) : nullptr;
				list->Add(new FxLocalVariableDeclaration(type, node->Name, val, 0, *node));	// todo: Handle flags in the grammar.
			}

			node = static_cast<decltype(node)>(node->SiblingNext);
		} while (node != loc->Vars);
		return list;
	}

	case AST_Expression:
	{
		auto ret = static_cast<ZCC_Expression *>(ast);
		if (ret->Operation == PEX_Super)
		{
			return new FxSuper(*ast);
		}
		break;
	}

	case AST_ExpressionStmt:
		return ConvertNode(static_cast<ZCC_ExpressionStmt *>(ast)->Expression);

	case AST_ReturnStmt:
	{
		auto ret = static_cast<ZCC_ReturnStmt *>(ast);
		FArgumentList args;
		ConvertNodeList(args, ret->Values);
		if (args.Size() == 0)
		{
			return new FxReturnStatement(nullptr, *ast);
		}
		else
		{
			return new FxReturnStatement(args, *ast);
		}
	}

	case AST_BreakStmt:
	case AST_ContinueStmt:
		return new FxJumpStatement(ast->NodeType == AST_BreakStmt ? TK_Break : TK_Continue, *ast);

	case AST_IfStmt:
	{
		auto iff = static_cast<ZCC_IfStmt *>(ast);
		FxExpression *const truePath = ConvertImplicitScopeNode(ast, iff->TruePath);
		FxExpression *const falsePath = ConvertImplicitScopeNode(ast, iff->FalsePath);
		return new FxIfStatement(ConvertNode(iff->Condition), truePath, falsePath, *ast);
	}

	case AST_IterationStmt:
	{
		auto iter = static_cast<ZCC_IterationStmt *>(ast);
		if (iter->CheckAt == ZCC_IterationStmt::End)
		{
			assert(iter->LoopBumper == nullptr);
			FxExpression *const loop = ConvertImplicitScopeNode(ast, iter->LoopStatement);
			return new FxDoWhileLoop(ConvertNode(iter->LoopCondition), loop, *ast);
		}
		else if (iter->LoopBumper != nullptr)
		{
			FArgumentList bumper;
			ConvertNodeList(bumper, iter->LoopBumper);
			FxCompoundStatement *bumps = new FxCompoundStatement(*ast);
			for (auto &ex : bumper)
			{
				bumps->Add(ex);
				ex = nullptr;
			}
			return new FxForLoop(nullptr, ConvertNode(iter->LoopCondition), bumps, ConvertNode(iter->LoopStatement), *ast);
		}
		else
		{
			FxExpression *const loop = ConvertImplicitScopeNode(ast, iter->LoopStatement);
			return new FxWhileLoop(ConvertNode(iter->LoopCondition), loop, *ast);
		}
	}

	// not yet done
	case AST_SwitchStmt:
	{
		auto swtch = static_cast<ZCC_SwitchStmt *>(ast);
		if (swtch->Content->NodeType != AST_CompoundStmt)
		{
			Error(ast, "Expecting { after 'switch'");
			return new FxNop(*ast);	// allow compiler to continue looking for errors.
		}
		else
		{
			// The switch content is wrapped into a compound statement which needs to be unraveled here.
			auto cmpnd = static_cast<ZCC_CompoundStmt *>(swtch->Content);
			FArgumentList args;
			return new FxSwitchStatement(ConvertNode(swtch->Condition), ConvertNodeList(args, cmpnd->Content), *ast);
		}
	}

	case AST_CaseStmt:
	{
		auto cases = static_cast<ZCC_CaseStmt *>(ast);
		return new FxCaseStatement(ConvertNode(cases->Condition), *ast);
	}

	case AST_CompoundStmt:
	{
		auto x = new FxCompoundStatement(*ast);
		auto compound = static_cast<ZCC_CompoundStmt *>(ast);
		auto node = compound->Content;
		if (node != nullptr) do
		{
			x->Add(ConvertNode(node));
			node = static_cast<decltype(node)>(node->SiblingNext);
		} while (node != compound->Content);
		return x;
	}

	case AST_AssignStmt:
	{
		auto ass = static_cast<ZCC_AssignStmt *>(ast);
		FArgumentList args;
		ConvertNodeList(args, ass->Dests);
		assert(ass->Sources->SiblingNext == ass->Sources);	// right side should be a single function call - nothing else
		if (ass->Sources->NodeType != AST_ExprFuncCall)
		{
			// don't let this through to the code generator. This node is only used to assign multiple returns of a function to more than one variable.
			Error(ass, "Right side of multi-assignment must be a function call");
			return new FxNop(*ast);	// allow compiler to continue looking for errors.
		}
		return new FxMultiAssign(args, ConvertNode(ass->Sources), *ast);
	}

	default:
		break;
	}
	// only for development. I_Error is more convenient here than a normal error.
	I_Error("ConvertNode encountered unsupported node of type %d", ast->NodeType);
	return nullptr;
}

//==========================================================================
//
// Wrapper around ConvertNode() that adds a scope (a compound statement)
// when needed to avoid leaking of variable or contant to an outer scope:
//
//  if (true) int i; else bool b[1];
//  while (false) readonly<Actor> a;
//  do static const float f[] = {0}; while (false);
//
// Accessing such variables outside of their statements is now an error
//
//==========================================================================

FxExpression *ZCCCompiler::ConvertImplicitScopeNode(ZCC_TreeNode *node, ZCC_Statement *nested)
{
	assert(nullptr != node);

	if (nullptr == nested)
	{
		return nullptr;
	}
	
	FxExpression *nestedExpr = ConvertNode(nested);
	assert(nullptr != nestedExpr);

	const EZCCTreeNodeType nestedType = nested->NodeType;
	const bool needScope = AST_LocalVarStmt == nestedType || AST_StaticArrayStatement == nestedType;

	if (needScope)
	{
		FxCompoundStatement *implicitCompound = new FxCompoundStatement(*node);
		implicitCompound->Add(nestedExpr);

		nestedExpr = implicitCompound;
	}

	return nestedExpr;
}


FArgumentList &ZCCCompiler::ConvertNodeList(FArgumentList &args, ZCC_TreeNode *head)
{
	if (head != nullptr)
	{
		auto node = head;
		do
		{
			args.Push(ConvertNode(node));
			node = node->SiblingNext;
		} while (node != head);
	}
	return args;
}

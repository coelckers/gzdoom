/*
** p_states.cpp
** state management
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
** Copyright 2006-2008 Christoph Oelckers
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
**
*/
#include "actor.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "thingdef.h"
#include "r_state.h"


// stores indices for symbolic state labels for some old-style DECORATE functions.
FStateLabelStorage StateLabels;

// Each state is owned by an actor. Actors can own any number of
// states, but a single state cannot be owned by more than one
// actor. States are archived by recording the actor they belong
// to and the index into that actor's list of states.


//==========================================================================
//
// This wraps everything needed to get a current sprite from a state into
// one single script function.
//
//==========================================================================

DEFINE_ACTION_FUNCTION(FState, GetSpriteTexture)
{
	PARAM_SELF_STRUCT_PROLOGUE(FState);
	PARAM_INT(rotation);
	PARAM_INT(skin);
	PARAM_FLOAT(scalex);
	PARAM_FLOAT(scaley);

	spriteframe_t *sprframe;
	if (skin == 0)
	{
		sprframe = &SpriteFrames[sprites[self->sprite].spriteframes + self->GetFrame()];
	}
	else
	{
		sprframe = &SpriteFrames[sprites[Skins[skin].sprite].spriteframes + self->GetFrame()];
		scalex = Skins[skin].Scale.X;
		scaley = Skins[skin].Scale.Y;
	}
	if (numret > 0) ret[0].SetInt(sprframe->Texture[rotation].GetIndex());
	if (numret > 1) ret[1].SetInt(!!(sprframe->Flip & (1 << rotation)));
	if (numret > 2) ret[2].SetVector2(DVector2(scalex, scaley));
	return MIN(3, numret);
}


//==========================================================================
//
// Find the actor that a state belongs to.
//
//==========================================================================

PClassActor *FState::StaticFindStateOwner (const FState *state)
{
	for (unsigned int i = 0; i < PClassActor::AllActorClasses.Size(); ++i)
	{
		PClassActor *info = PClassActor::AllActorClasses[i];
		if (info->OwnsState(state))
		{
			return info;
		}
	}

	return NULL;
}

//==========================================================================
//
// Find the actor that a state belongs to, but restrict the search to
// the specified type and its ancestors.
//
//==========================================================================

PClassActor *FState::StaticFindStateOwner (const FState *state, PClassActor *info)
{
	while (info != NULL)
	{
		if (info->OwnsState(state))
		{
			return info;
		}
		info = ValidateActor(info->ParentClass);
	}
	return NULL;
}

//==========================================================================
//
//
//==========================================================================

FString FState::StaticGetStateName(const FState *state, PClassActor *info)
{
	auto so = FState::StaticFindStateOwner(state);
	if (so == nullptr)
	{
		so = FState::StaticFindStateOwner(state, info);
	}
	if (so == nullptr)
	{
		return "<unknown>";
	}
	return FStringf("%s.%d", so->TypeName.GetChars(), int(state - so->GetStates()));
}

//==========================================================================
//
//
//==========================================================================

FStateLabel *FStateLabels::FindLabel (FName label)
{
	return const_cast<FStateLabel *>(BinarySearch<FStateLabel, FName>(Labels, NumLabels, &FStateLabel::Label, label));
}

void FStateLabels::Destroy ()
{
	for(int i = 0; i < NumLabels; i++)
	{
		if (Labels[i].Children != NULL)
		{
			Labels[i].Children->Destroy();
			M_Free(Labels[i].Children);	// These are malloc'd, not new'd!
			Labels[i].Children = NULL;
		}
	}
}


//===========================================================================
//
// HasStates
//
// Checks whether the actor has special death states.
//
//===========================================================================

bool AActor::HasSpecialDeathStates () const
{
	const PClassActor *info = static_cast<PClassActor *>(GetClass());

	if (info->GetStateLabels() != NULL)
	{
		FStateLabel *slabel = info->GetStateLabels()->FindLabel (NAME_Death);
		if (slabel != NULL && slabel->Children != NULL)
		{
			for(int i = 0; i < slabel->Children->NumLabels; i++)
			{
				if (slabel->Children->Labels[i].State != NULL)
				{
					return true;
				}
			}
		}
	}
	return false;
}

//==========================================================================
//
// Creates a list of names from a string. Dots are used as separator
//
//==========================================================================

TArray<FName> &MakeStateNameList(const char * fname)
{
	static TArray<FName> namelist(3);
	FName firstpart = NAME_None, secondpart = NAME_None;
	char *c;

	// Handle the old names for the existing death states
	char *name = copystring(fname);
	firstpart = strtok(name, ".");
	switch (firstpart.GetIndex())
	{
	case NAME_Burn:
		firstpart = NAME_Death;
		secondpart = NAME_Fire;
		break;
	case NAME_Ice:
		firstpart = NAME_Death;
		secondpart = NAME_Ice;
		break;
	case NAME_Disintegrate:
		firstpart = NAME_Death;
		secondpart = NAME_Disintegrate;
		break;
	case NAME_XDeath:
		firstpart = NAME_Death;
		secondpart = NAME_Extreme;
		break;
	}

	namelist.Clear();
	namelist.Push(firstpart);
	if (secondpart != NAME_None)
	{
		namelist.Push(secondpart);
	}

	while ((c = strtok(NULL, ".")) != NULL)
	{
		FName cc = c;
		namelist.Push(cc);
	}
	delete[] name;
	return namelist;
}

//===========================================================================
//
// FindState (multiple names version)
//
// Finds a state that matches as many of the supplied names as possible.
// A state with more names than those provided does not match.
// A state with fewer names can match if there are no states with the exact
// same number of names.
//
// The search proceeds like this. For the current class, keeping matching
// names until there are no more. If both the argument list and the state
// are out of names, it's an exact match, so return it. If the state still
// has names, ignore it. If the argument list still has names, remember it.
//
//===========================================================================
FState *PClassActor::FindState(int numnames, FName *names, bool exact) const
{
	FStateLabels *labels = GetStateLabels();
	FState *best = nullptr;

	if (labels != nullptr)
	{
		int count = 0;

		// Find the best-matching label for this class.
		while (labels != nullptr && count < numnames)
		{
			FName label = *names++;
			FStateLabel *slabel = labels->FindLabel(label);

			if (slabel != nullptr)
			{
				count++;
				labels = slabel->Children;
				best = slabel->State;
			}
			else
			{
				break;
			}
		}
		if (count < numnames && exact)
		{
			return nullptr;
		}
	}
	return best;
}

//==========================================================================
//
// Finds the state associated with the given string
//
//==========================================================================

FState *PClassActor::FindStateByString(const char *name, bool exact)
{
	TArray<FName> &namelist = MakeStateNameList(name);
	return FindState(namelist.Size(), &namelist[0], exact);
}


//==========================================================================
//
// validate a runtime state index.
//
//==========================================================================

static bool VerifyJumpTarget(PClassActor *cls, FState *CallingState, int index)
{
	while (cls != RUNTIME_CLASS(AActor))
	{
		// both calling and target state need to belong to the same class.
		if (cls->OwnsState(CallingState))
		{
			return cls->OwnsState(CallingState + index);
		}

		// We can safely assume the ParentClass is of type PClassActor
		// since we stop when we see the Actor base class.
		cls = static_cast<PClassActor *>(cls->ParentClass);
	}
	return false;
}

//==========================================================================
//
// Get a statw pointer from a symbolic label
//
//==========================================================================

FState *FStateLabelStorage::GetState(int pos, PClassActor *cls, bool exact)
{
	if (pos >= 0x10000000)
	{
		return cls? cls->FindState(ENamedName(pos - 0x10000000)) : nullptr;
	}
	else if (pos < 0)
	{
		// decode the combined value produced by the script.
		int index = (pos >> 16) & 32767;
		pos = ((pos & 65535) - 1) * 4;
		FState *state;
		memcpy(&state, &Storage[pos + sizeof(int)], sizeof(state));
		if (VerifyJumpTarget(cls, state, index))
			return state + index;
		else
			return nullptr;
	}
	else if (pos > 0)
	{
		int val;
		pos = (pos - 1) * 4;
		memcpy(&val, &Storage[pos], sizeof(int));

		if (val == 0)
		{
			FState *state;
			memcpy(&state, &Storage[pos + sizeof(int)], sizeof(state));
			return state;
		}
		else if (cls != nullptr)
		{
			FName *labels = (FName*)&Storage[pos + sizeof(int)];
			return cls->FindState(val, labels, exact);
		}
	}
	return nullptr;
}

//==========================================================================
//
// State label conversion function for scripts
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, FindState)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(newstate);
	PARAM_BOOL(exact)
	ACTION_RETURN_STATE(StateLabels.GetState(newstate, self->GetClass(), exact));
}

// same as above but context aware.
DEFINE_ACTION_FUNCTION(AActor, ResolveState)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_STATE_ACTION(newstate);
	ACTION_RETURN_STATE(newstate);
}

//==========================================================================
//
// Search one list of state definitions for the given name
//
//==========================================================================

FStateDefine *FStateDefinitions::FindStateLabelInList(TArray<FStateDefine> & list, FName name, bool create)
{
	for(unsigned i = 0; i<list.Size(); i++)
	{
		if (list[i].Label == name)
		{
			return &list[i];
		}
	}
	if (create)
	{
		FStateDefine def;
		def.Label = name;
		def.State = nullptr;
		def.DefineFlags = SDF_NEXT;
		return &list[list.Push(def)];
	}
	return NULL;
}

//==========================================================================
//
// Finds the address of a state label given by name. 
// Adds the state label if it doesn't exist
//
//==========================================================================

FStateDefine *FStateDefinitions::FindStateAddress(const char *name)
{
	FStateDefine *statedef = NULL;
	TArray<FName> &namelist = MakeStateNameList(name);
	TArray<FStateDefine> *statelist = &StateLabels;

	for(unsigned i = 0; i < namelist.Size(); i++)
	{
		statedef = FindStateLabelInList(*statelist, namelist[i], true);
		statelist = &statedef->Children;
	}
	return statedef;
}

//==========================================================================
//
// Adds a new state to the curremt list
//
//==========================================================================

void FStateDefinitions::SetStateLabel(const char *statename, FState *state, uint8_t defflags)
{
	FStateDefine *std = FindStateAddress(statename);
	std->State = state;
	std->DefineFlags = defflags;
}

//==========================================================================
//
// Adds a new state to the current list
//
//==========================================================================

void FStateDefinitions::AddStateLabel(const char *statename)
{
	intptr_t index = StateArray.Size();
	FStateDefine *std = FindStateAddress(statename);
	std->State = (FState *)(index+1);
	std->DefineFlags = SDF_INDEX;
	laststate = NULL;
	lastlabel = index;
}

//==========================================================================
//
// Returns the index a state label points to. May only be called before
// installing states.
//
//==========================================================================

int FStateDefinitions::GetStateLabelIndex (FName statename)
{
	FStateDefine *std = FindStateLabelInList(StateLabels, statename, false);
	if (std == NULL)
	{
		return -1;
	}
	assert((size_t)std->State <= StateArray.Size() + 1);
	return (int)((ptrdiff_t)std->State - 1);
}

//==========================================================================
//
// Finds the state associated with the given name
// returns NULL if none found
//
//==========================================================================

FState *FStateDefinitions::FindState(const char * name)
{
	FStateDefine *statedef = NULL;

	TArray<FName> &namelist = MakeStateNameList(name);

	TArray<FStateDefine> *statelist = &StateLabels;
	for(unsigned i = 0; i < namelist.Size(); i++)
	{
		statedef = FindStateLabelInList(*statelist, namelist[i], false);
		if (statedef == NULL)
		{
			return NULL;
		}
		statelist = &statedef->Children;
	}
	return statedef ? statedef->State : NULL;
}

//==========================================================================
//
// Creates the final list of states from the state definitions
//
//==========================================================================

static int labelcmp(const void *a, const void *b)
{
	FStateLabel *A = (FStateLabel *)a;
	FStateLabel *B = (FStateLabel *)b;
	return ((int)A->Label.GetIndex() - (int)B->Label.GetIndex());
}

FStateLabels *FStateDefinitions::CreateStateLabelList(TArray<FStateDefine> & statelist)
{
	// First delete all empty labels from the list
	for (int i = statelist.Size() - 1; i >= 0; i--)
	{
		if (statelist[i].Label == NAME_None || (statelist[i].State == NULL && statelist[i].Children.Size() == 0))
		{
			statelist.Delete(i);
		}
	}

	int count = statelist.Size();

	if (count == 0)
	{
		return NULL;
	}
	FStateLabels *list = (FStateLabels*)M_Malloc(sizeof(FStateLabels)+(count-1)*sizeof(FStateLabel));
	list->NumLabels = count;

	for (int i=0;i<count;i++)
	{
		list->Labels[i].Label = statelist[i].Label;
		list->Labels[i].State = statelist[i].State;
		list->Labels[i].Children = CreateStateLabelList(statelist[i].Children);
	}
	qsort(list->Labels, count, sizeof(FStateLabel), labelcmp);
	return list;
}

//===========================================================================
//
// InstallStates
//
// Creates the actor's state list from the current definition
//
//===========================================================================

void FStateDefinitions::InstallStates(PClassActor *info, AActor *defaults)
{
	if (defaults == nullptr)
	{
		I_Error("Called InstallStates without actor defaults in %s", info->TypeName.GetChars());
	}

	// First ensure we have a valid spawn state.
	FState *state = FindState("Spawn");

	if (state == NULL)
	{
		// A NULL spawn state will crash the engine so set it to something valid.
		SetStateLabel("Spawn", GetDefault<AActor>()->SpawnState);
	}

	auto &sl = info->ActorInfo()->StateList;
	if (sl != NULL) 
	{
		sl->Destroy();
		M_Free(sl);
	}
	sl = CreateStateLabelList(StateLabels);

	// Cache these states as member veriables.
	defaults->SpawnState = info->FindState(NAME_Spawn);
	defaults->SeeState = info->FindState(NAME_See);
	// Melee and Missile states are manipulated by the scripted marines so they
	// have to be stored locally
	defaults->MeleeState = info->FindState(NAME_Melee);
	defaults->MissileState = info->FindState(NAME_Missile);
}

//===========================================================================
//
// MakeStateDefines
//
// Creates a list of state definitions from an existing actor
// Used by Dehacked to modify an actor's state list
//
//===========================================================================

void FStateDefinitions::MakeStateList(const FStateLabels *list, TArray<FStateDefine> &dest)
{
	dest.Clear();
	if (list != NULL) for (int i = 0; i < list->NumLabels; i++)
	{
		FStateDefine def;

		def.Label = list->Labels[i].Label;
		def.State = list->Labels[i].State;
		def.DefineFlags = SDF_STATE;
		dest.Push(def);
		if (list->Labels[i].Children != NULL)
		{
			MakeStateList(list->Labels[i].Children, dest[dest.Size()-1].Children);
		}
	}
}

void FStateDefinitions::MakeStateDefines(const PClassActor *cls)
{
	StateArray.Clear();
	laststate = NULL;
	laststatebeforelabel = NULL;
	lastlabel = -1;

	if (cls != NULL && cls->GetStateLabels() != NULL)
	{
		MakeStateList(cls->GetStateLabels(), StateLabels);
	}
	else
	{
		StateLabels.Clear();
	}
}

//===========================================================================
//
// AddStateDefines
//
// Adds a list of states to the current definitions
//
//===========================================================================

void FStateDefinitions::AddStateDefines(const FStateLabels *list)
{
	if (list != NULL) for(int i = 0; i < list->NumLabels; i++)
	{
		if (list->Labels[i].Children == NULL)
		{
			if (!FindStateLabelInList(StateLabels, list->Labels[i].Label, false))
			{
				FStateDefine def;

				def.Label = list->Labels[i].Label;
				def.State = list->Labels[i].State;
				def.DefineFlags = SDF_STATE;
				StateLabels.Push(def);
			}
		}
	}
}

//==========================================================================
//
// RetargetState(Pointer)s
//
// These functions are used when a goto follows one or more labels.
// Because multiple labels are permitted to occur consecutively with no
// intervening states, it is not enough to remember the last label defined
// and adjust it. So these functions search for all labels that point to
// the current position in the state array and give them a copy of the
// target string instead.
//
//==========================================================================

void FStateDefinitions::RetargetStatePointers (intptr_t count, const char *target, TArray<FStateDefine> & statelist)
{
	for(unsigned i = 0;i<statelist.Size(); i++)
	{
		if (statelist[i].State == (FState*)count && statelist[i].DefineFlags == SDF_INDEX)
		{
			if (target == NULL)
			{
				statelist[i].State = NULL;
				statelist[i].DefineFlags = SDF_STOP;
			}
			else
			{
				statelist[i].State = (FState *)copystring (target);
				statelist[i].DefineFlags = SDF_LABEL;
			}
		}
		if (statelist[i].Children.Size() > 0)
		{
			RetargetStatePointers(count, target, statelist[i].Children);
		}
	}
}

void FStateDefinitions::RetargetStates (intptr_t count, const char *target)
{
	RetargetStatePointers(count, target, StateLabels);
}


//==========================================================================
//
// ResolveGotoLabel
//
// Resolves any strings being stored in a state's NextState field
//
//==========================================================================

FState *FStateDefinitions::ResolveGotoLabel (PClassActor *mytype, char *name)
{
	PClassActor *type = mytype;
	FState *state;
	char *namestart = name;
	char *label, *offset, *pt;
	int v;

	// Check for classname
	if ((pt = strstr (name, "::")) != NULL)
	{
		const char *classname = name;
		*pt = '\0';
		name = pt + 2;

		// The classname may either be "Super" to identify this class's immediate
		// superclass, or it may be the name of any class that this one derives from.
		if (stricmp (classname, "Super") == 0)
		{
			type = ValidateActor(type->ParentClass);
		}
		else
		{
			// first check whether a state of the desired name exists
			PClass *stype = PClass::FindClass (classname);
			if (stype == NULL)
			{
				I_Error ("%s is an unknown class.", classname);
			}
			if (!stype->IsDescendantOf (RUNTIME_CLASS(AActor)))
			{
				I_Error ("%s is not an actor class, so it has no states.", stype->TypeName.GetChars());
			}
			if (!stype->IsAncestorOf (type))
			{
				I_Error ("%s is not derived from %s so cannot access its states.",
					type->TypeName.GetChars(), stype->TypeName.GetChars());
			}
			if (type != stype)
			{
				type = static_cast<PClassActor *>(stype);
			}
		}
	}
	label = name;
	// Check for offset
	offset = NULL;
	if ((pt = strchr (name, '+')) != NULL)
	{
		*pt = '\0';
		offset = pt + 1;
	}
	v = offset ? (int)strtoll (offset, NULL, 0) : 0;

	// Get the state's address.
	if (type == mytype)
	{
		state = FindState (label);
	}
	else
	{
		state = type->FindStateByString(label, true);
	}

	if (state != NULL)
	{
		state += v;
	}
	else if (v != 0)
	{
		I_Error ("Attempt to get invalid state %s from actor %s.", label, type->TypeName.GetChars());
	}
	else
	{
		Printf (TEXTCOLOR_RED "Attempt to get invalid state %s from actor %s.\n", label, type->TypeName.GetChars());
	}
	delete[] namestart;		// free the allocated string buffer
	return state;
}

//==========================================================================
//
// FixStatePointers
//
// Fixes an actor's default state pointers.
//
//==========================================================================

void FStateDefinitions::FixStatePointers (PClassActor *actor, TArray<FStateDefine> & list)
{
	for (unsigned i = 0; i < list.Size(); i++)
	{
		if (list[i].DefineFlags == SDF_INDEX)
		{
			size_t v = (size_t)list[i].State;
			list[i].State = actor->GetStates() + v - 1;
			list[i].DefineFlags = SDF_STATE;
		}
		if (list[i].Children.Size() > 0)
		{
			FixStatePointers(actor, list[i].Children);
		}
	}
}

//==========================================================================
//
// ResolveGotoLabels
//
// Resolves an actor's state pointers that were specified as jumps.
//
//==========================================================================

void FStateDefinitions::ResolveGotoLabels (PClassActor *actor, TArray<FStateDefine> & list)
{
	for (unsigned i = 0; i < list.Size(); i++)
	{
		if (list[i].State != NULL && list[i].DefineFlags == SDF_LABEL)
		{ // It's not a valid state, so it must be a label string. Resolve it.
			list[i].State = ResolveGotoLabel (actor, (char *)list[i].State);
			list[i].DefineFlags = SDF_STATE;
		}
		if (list[i].Children.Size() > 0) ResolveGotoLabels(actor, list[i].Children);
	}
}


//==========================================================================
//
// SetGotoLabel
//
// sets a jump at the current state or retargets a label
//
//==========================================================================

bool FStateDefinitions::SetGotoLabel(const char *string)
{
	// copy the text - this must be resolved later!
	if (laststate != NULL)
	{ // Following a state definition: Modify it.
		laststate->NextState = (FState*)copystring(string);	
		laststate->DefineFlags = SDF_LABEL;
		laststatebeforelabel = NULL;
		return true;
	}
	else if (lastlabel >= 0)
	{ // Following a label: Retarget it.
		RetargetStates (lastlabel+1, string);
		if (laststatebeforelabel != NULL)
		{
			laststatebeforelabel->NextState = (FState*)copystring(string);	
			laststatebeforelabel->DefineFlags = SDF_LABEL;
			laststatebeforelabel = NULL;
		}
		return true;
	}
	return false;
}

//==========================================================================
//
// SetStop
//
// sets a stop operation
//
//==========================================================================

bool FStateDefinitions::SetStop()
{
	if (laststate != NULL)
	{
		laststate->DefineFlags = SDF_STOP;
		laststatebeforelabel = NULL;
		return true;
	}
	else if (lastlabel >=0)
	{
		RetargetStates (lastlabel+1, NULL);
		if (laststatebeforelabel != NULL)
		{
			laststatebeforelabel->DefineFlags = SDF_STOP;
			laststatebeforelabel = NULL;
		}
		return true;
	}
	return false;
}

//==========================================================================
//
// SetWait
//
// sets a wait or fail operation
//
//==========================================================================

bool FStateDefinitions::SetWait()
{
	if (laststate != NULL)
	{
		laststate->DefineFlags = SDF_WAIT;
		laststatebeforelabel = NULL;
		return true;
	}
	return false;
}

//==========================================================================
//
// SetLoop
//
// sets a loop operation
//
//==========================================================================

bool FStateDefinitions::SetLoop()
{
	if (laststate != NULL)
	{
		laststate->DefineFlags = SDF_INDEX;
		laststate->NextState = (FState*)(lastlabel+1);
		laststatebeforelabel = NULL;
		return true;
	}
	return false;
}

//==========================================================================
//
// AddStates
//
// Adds some state to the current definition set. Returns the number of
// states added. Positive = no errors, negative = errors.
//
//==========================================================================

int FStateDefinitions::AddStates(FState *state, const char *framechars, const FScriptPosition &sc)
{
	bool error = false;
	int frame = 0;
	int count = 0;
	while (*framechars)
	{
		bool noframe = false;

		if (*framechars == '#')
			noframe = true;
		else if (*framechars == '^')
			frame = '\\' - 'A';
		else
			frame = (*framechars & 223) - 'A';

		framechars++;
		if (frame < 0 || frame > 28)
		{
			frame = 0;
			error = true;
		}

		state->Frame = frame;
		if (noframe) state->StateFlags |= STF_SAMEFRAME;
		else state->StateFlags &= ~STF_SAMEFRAME;
		StateArray.Push(*state);
		SourceLines.Push(sc);
		++count;

		// NODELAY flag is not carried past the first state
		state->StateFlags &= ~STF_NODELAY;
	}
	laststate = &StateArray[StateArray.Size() - 1];
	laststatebeforelabel = laststate;
	return !error ? count : -count;
}

//==========================================================================
//
// FinishStates
// copies a state block and fixes all state links using the current list of labels
//
//==========================================================================

int FStateDefinitions::FinishStates(PClassActor *actor)
{
	int count = StateArray.Size();

	if (count > 0)
	{
		FState *realstates = (FState*)ClassDataAllocator.Alloc(count * sizeof(FState));
		int i;

		memcpy(realstates, &StateArray[0], count*sizeof(FState));
		actor->ActorInfo()->OwnedStates = realstates;
		actor->ActorInfo()->NumOwnedStates = count;
		SaveStateSourceLines(realstates, SourceLines);

		// adjust the state pointers
		// In the case new states are added these must be adjusted, too!
		FixStatePointers(actor, StateLabels);

		// Fix state pointers that are gotos
		ResolveGotoLabels(actor, StateLabels);

		for (i = 0; i < count; i++)
		{
			// resolve labels and jumps
			switch (realstates[i].DefineFlags)
			{
			case SDF_STOP:		// stop
				realstates[i].NextState = NULL;
				break;

			case SDF_WAIT:		// wait
				realstates[i].NextState = &realstates[i];
				break;

			case SDF_NEXT:		// next
				realstates[i].NextState = (i < count-1 ? &realstates[i+1] : &realstates[0]);
				break;

			case SDF_INDEX:		// loop
				realstates[i].NextState = &realstates[(size_t)realstates[i].NextState-1];
				break;

			case SDF_LABEL:
				realstates[i].NextState = ResolveGotoLabel(actor, (char *)realstates[i].NextState);
				break;
			}
		}
	}
	else
	{
		// Fix state pointers that are gotos
		ResolveGotoLabels(actor, StateLabels);
	}
	return count;
}



//==========================================================================
//
// Prints all state label info to the logfile
//
//==========================================================================

void DumpStateHelper(FStateLabels *StateList, const FString &prefix)
{
	for (int i = 0; i < StateList->NumLabels; i++)
	{
		if (StateList->Labels[i].State != NULL)
		{
			const PClassActor *owner = FState::StaticFindStateOwner(StateList->Labels[i].State);
			if (owner == NULL)
			{
				Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "%s%s: invalid\n", prefix.GetChars(), StateList->Labels[i].Label.GetChars());
			}
			else
			{
				Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "%s%s: %s\n", prefix.GetChars(), StateList->Labels[i].Label.GetChars(), FState::StaticGetStateName(StateList->Labels[i].State).GetChars());
			}
		}
		if (StateList->Labels[i].Children != NULL)
		{
			DumpStateHelper(StateList->Labels[i].Children, prefix + '.' + StateList->Labels[i].Label.GetChars());
		}
	}
}

CCMD(dumpstates)
{
	for (unsigned int i = 0; i < PClassActor::AllActorClasses.Size(); ++i)
	{
		PClassActor *info = PClassActor::AllActorClasses[i];
		Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "State labels for %s\n", info->TypeName.GetChars());
		DumpStateHelper(info->GetStateLabels(), "");
		Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "----------------------------\n");
	}
}

//==========================================================================
//
// sets up the script-side version of states
//
//==========================================================================

DEFINE_FIELD(FState, NextState)
DEFINE_FIELD(FState, sprite)
DEFINE_FIELD(FState, Tics)
DEFINE_FIELD(FState, TicRange)
DEFINE_FIELD(FState, Frame)
DEFINE_FIELD(FState, UseFlags)
DEFINE_FIELD(FState, Misc1)
DEFINE_FIELD(FState, Misc2)
DEFINE_FIELD_BIT(FState, StateFlags, bSlow, STF_SLOW)
DEFINE_FIELD_BIT(FState, StateFlags, bFast, STF_FAST)
DEFINE_FIELD_BIT(FState, StateFlags, bFullbright, STF_FULLBRIGHT)
DEFINE_FIELD_BIT(FState, StateFlags, bNoDelay, STF_NODELAY)
DEFINE_FIELD_BIT(FState, StateFlags, bSameFrame, STF_SAMEFRAME)
DEFINE_FIELD_BIT(FState, StateFlags, bCanRaise, STF_CANRAISE)
DEFINE_FIELD_BIT(FState, StateFlags, bDehacked, STF_DEHACKED)

DEFINE_ACTION_FUNCTION(FState, DistanceTo)
{
	PARAM_SELF_STRUCT_PROLOGUE(FState);
	PARAM_POINTER(other, FState);
	int retv = INT_MIN;
	if (other != nullptr)
	{
		// Safely calculate the distance between two states.
		auto o1 = FState::StaticFindStateOwner(self);
		if (o1->OwnsState(other)) retv = int(other - self);
	}
	ACTION_RETURN_INT(retv);
}

DEFINE_ACTION_FUNCTION(FState, ValidateSpriteFrame)
{
	PARAM_SELF_STRUCT_PROLOGUE(FState);
	ACTION_RETURN_BOOL(self->Frame < sprites[self->sprite].numframes);
}

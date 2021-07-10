/*
** p_converstation.cpp
** Implements Strife style conversation dialogs
**
**---------------------------------------------------------------------------
** Copyright 2004-2008 Randy Heit
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

#include <assert.h>

#include "actor.h"
#include "p_conversation.h"
#include "filesystem.h"
#include "cmdlib.h"
#include "v_text.h"
#include "gi.h"
#include "a_keys.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "i_music.h"
#include "p_setup.h"
#include "d_net.h"
#include "d_event.h"
#include "doomstat.h"
#include "c_console.h"
#include "sbar.h"
#include "p_lnspec.h"
#include "p_local.h"
#include "menu.h"
#include "g_levellocals.h"
#include "vm.h"
#include "v_video.h"
#include "actorinlines.h"
#include "v_draw.h"
#include "doommenu.h"

static FRandom pr_randomspeech("RandomSpeech");

static int ConversationMenuY;

// These two should be moved to player_t...
static FStrifeDialogueNode *PrevNode;
static int StaticLastReply;

static bool DrawConversationMenu ();
static void PickConversationReply (int replyindex);
static void TerminalResponse (const char *str);

CVAR(Bool, dlg_vgafont, false, CVAR_ARCHIVE)

//============================================================================
//
// GetStrifeType
//
// Given an item type number, returns the corresponding PClass.
//
//============================================================================

void FLevelLocals::SetConversation(int convid, PClassActor *Class, int dlgindex)
{
	if (convid != -1)
	{
		DialogueRoots[convid] = dlgindex;
	}
	if (Class != NULL)
	{
		ClassRoots[Class->TypeName] = dlgindex;
	}
}

PClassActor *GetStrifeType (int typenum)
{
	PClassActor **ptype = StrifeTypes.CheckKey(typenum);
	if (ptype == NULL) return NULL;
	else return *ptype;
}

int FLevelLocals::GetConversation(int conv_id)
{
	int *pindex = DialogueRoots.CheckKey(conv_id);
	if (pindex == NULL) return -1;
	else return *pindex;
}

int FLevelLocals::GetConversation(FName classname)
{
	int *pindex = ClassRoots.CheckKey(classname);
	if (pindex == NULL) return -1;
	else return *pindex;
}

//============================================================================
//
// FStrifeDialogueNode :: ~FStrifeDialogueNode
//
//============================================================================

FStrifeDialogueNode::~FStrifeDialogueNode ()
{
	FStrifeDialogueReply *tokill = Children;
	while (tokill != NULL)
	{
		FStrifeDialogueReply *next = tokill->Next;
		delete tokill;
		tokill = next;
	}
}

//============================================================================
//
// FindNode
//
// Returns the index that matches the given conversation node.
//
//============================================================================

int FLevelLocals::FindNode (const FStrifeDialogueNode *node)
{
	int rootnode = 0;

	while (StrifeDialogues[rootnode] != node)
	{
		rootnode++;
	}
	return rootnode;
}

//============================================================================
//
// ClearConversationStuff
//
// Clear the conversation pointers on the player
//
//============================================================================

static void ClearConversationStuff(player_t* player)
{
	player->ConversationFaceTalker = false;
	player->ConversationNPC = nullptr;
	player->ConversationPC = nullptr;
	player->ConversationNPCAngle = 0.;
}

//============================================================================
//
// CheckStrifeItem
//
// Checks if you have an item. A NULL itemtype is always considered to be
// present.
//
//============================================================================

static bool CheckStrifeItem (player_t *player, PClassActor *itemtype, int amount=-1)
{
	if (itemtype == NULL || amount == 0)
		return true;

	auto item = player->ConversationPC->FindInventory (itemtype);
	if (item == NULL)
		return false;

	return amount < 0 || item->IntVar(NAME_Amount) >= amount;
}

//============================================================================
//
// TakeStrifeItem
//
// Takes away some of an item, unless that item is special and should not
// be removed.
//
//============================================================================

static void TakeStrifeItem (player_t *player, PClassActor *itemtype, int amount)
{
	if (itemtype == NULL || amount == 0)
		return;

	// Don't take quest items.
	if (itemtype->IsDescendantOf (PClass::FindClass(NAME_QuestItem)))
		return;

	// Don't take keys.
	if (itemtype->IsDescendantOf (PClass::FindActor(NAME_Key)))
		return;

	// Don't take the sigil.
	if (itemtype->TypeName == NAME_Sigil)
		return;

	IFVM(Actor, TakeInventory)
	{
		VMValue params[] = { player->mo, itemtype, amount, false, false };
		VMCall(func, params, 5, nullptr, 0);
	}
}

CUSTOM_CVAR(Float, dlg_musicvolume, 1.0f, CVAR_ARCHIVE)
{
	if (self < 0.f) self = 0.f;
	else if (self > 1.f) self = 1.f;
}

//============================================================================
//
// ShouldSkipReply
//
// Determines whether this reply should be skipped or not.
//
//============================================================================

static bool ShouldSkipReply(FStrifeDialogueReply *reply, player_t *player)
{
	if (reply->Reply.IsEmpty())
		return true;

	int i;
	for (i = 0; i < (int)reply->ItemCheckRequire.Size(); ++i)
	{
		if (!CheckStrifeItem(player, reply->ItemCheckRequire[i].Item, reply->ItemCheckRequire[i].Amount))
		{
			return true;
		}
	}

	for (i = 0; i < (int)reply->ItemCheckExclude.Size(); ++i)
	{
		if (CheckStrifeItem(player, reply->ItemCheckExclude[i].Item, reply->ItemCheckExclude[i].Amount))
		{
			return true;
		}
	}
	return false;
}

DEFINE_ACTION_FUNCTION(FStrifeDialogueReply, ShouldSkipReply)
{
	PARAM_SELF_STRUCT_PROLOGUE(FStrifeDialogueReply);
	PARAM_POINTER(player, player_t);
	ACTION_RETURN_BOOL(ShouldSkipReply(self, player));
}

DEFINE_ACTION_FUNCTION(DConversationMenu, SendConversationReply)
{
	PARAM_PROLOGUE;
	PARAM_INT(node);
	PARAM_INT(reply);
	switch (node)
	{
	case -1:
		Net_WriteByte(DEM_CONVNULL);
		break;

	case -2:
		Net_WriteByte(DEM_CONVCLOSE);
		break;

	default:
		Net_WriteByte(DEM_CONVREPLY);
		Net_WriteWord(node);
		Net_WriteByte(reply);
		break;
	}
	StaticLastReply = reply;
	return 0;
}


//============================================================================
//
// P_FreeStrifeConversations
//
//============================================================================

void P_FreeStrifeConversations ()
{
	PrevNode = NULL;
	if (CurrentMenu != NULL && CurrentMenu->IsKindOf("ConversationMenu"))
	{
		CurrentMenu->Close();
	}
}

//============================================================================
//
// P_StartConversation
//
// Begins a conversation between a PC and NPC.
//
//============================================================================

void P_StartConversation (AActor *npc, AActor *pc, bool facetalker, bool saveangle)
{
	AActor *oldtarget;
	int i;

	// Make sure this is actually a player.
	if (pc == nullptr || pc->player == nullptr || npc == nullptr || !pc->Level->isPrimaryLevel()) return;
	auto Level = pc->Level;

	// [CW] If an NPC is talking to a PC already, then don't let
	// anyone else talk to the NPC.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!Level->PlayerInGame(i) || pc->player == Level->Players[i])
			continue;

		if (npc == Level->Players[i]->ConversationNPC)
			return;
	}

	pc->Vel.Zero();
	pc->player->Vel.Zero();
	PlayIdle (pc);

	pc->player->ConversationPC = pc;
	pc->player->ConversationNPC = npc;
	npc->flags5 |= MF5_INCONVERSATION;

	FStrifeDialogueNode *CurNode = npc->Conversation;

	if (pc->player == Level->GetConsolePlayer())
	{
		S_Sound (CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1, ATTN_NONE);
	}

	npc->reactiontime = 2;
	pc->player->ConversationFaceTalker = facetalker;
	if (saveangle)
	{
		pc->player->ConversationNPCAngle = npc->Angles.Yaw;
	}
	oldtarget = npc->target;
	npc->target = pc;
	if (facetalker)
	{
		if (!(npc->flags8 & MF8_DONTFACETALKER))
			A_FaceTarget (npc);
		pc->Angles.Yaw = pc->AngleTo(npc);
	}
	if ((npc->flags & MF_FRIENDLY) || (npc->flags4 & MF4_NOHATEPLAYERS))
	{
		npc->target = oldtarget;
	}

	// Check if we should jump to another node
	while (CurNode->ItemCheck.Size() > 0 && CurNode->ItemCheck[0].Item != NULL)
	{
		bool jump = true;
		for (i = 0; i < (int)CurNode->ItemCheck.Size(); ++i)
		{
			if(!CheckStrifeItem (pc->player, CurNode->ItemCheck[i].Item, CurNode->ItemCheck[i].Amount))
			{
				jump = false;
				break;
			}
		}
		if (jump && CurNode->ItemCheckNode > 0)
		{
			int root = pc->player->ConversationNPC->ConversationRoot;
			CurNode = Level->StrifeDialogues[root + CurNode->ItemCheckNode - 1];
		}
		else
		{
			break;
		}
	}

	// [Nash] Play voice clip from the actor so that positional audio can be heard by all players
	if (CurNode->SpeakerVoice != 0) S_Sound(npc, CHAN_VOICE, CHANF_NOPAUSE, CurNode->SpeakerVoice, 1, ATTN_NORM);

	// The rest is only done when the conversation is actually displayed.
	if (pc->player == Level->GetConsolePlayer())
	{
		if (CurNode->SpeakerVoice != 0)
		{
			I_SetMusicVolume (dlg_musicvolume);
		}
		M_StartControlPanel(false, true);

		// Create the menu. This may be a user-defined class so check if it is good to use.
		FName cls = CurNode->MenuClassName;
		if (cls == NAME_None) cls = gameinfo.DefaultConversationMenuClass;
		if (cls == NAME_None) cls = "ConversationMenu";
		auto mcls = PClass::FindClass(cls);
		if (mcls == nullptr || !mcls->IsDescendantOf("ConversationMenu")) mcls = PClass::FindClass("ConversationMenu");
		assert(mcls);

		auto cmenu = mcls->CreateNew();
		IFVIRTUALPTRNAME(cmenu, "ConversationMenu", Init)
		{
			VMValue params[] = { cmenu, CurNode, pc->player, StaticLastReply };
			VMReturn ret(&ConversationMenuY);
			VMCall(func, params, countof(params), &ret, 1);
		}

		if (CurNode != PrevNode)
		{ // Only reset the selection if showing a different menu.
			StaticLastReply = 0;
			PrevNode = CurNode;
		}

		// And open the menu
		M_ActivateMenu((DMenu*)cmenu);
		menuactive = MENU_OnNoPause;
	}
}

//============================================================================
//
// P_ResumeConversation
//
// Resumes a conversation that was interrupted by a slideshow.
//
//============================================================================

void P_ResumeConversation ()
{
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		player_t *p = &players[i];

		if (p->ConversationPC != NULL && p->ConversationNPC != NULL)
		{
			P_StartConversation (p->ConversationNPC, p->ConversationPC, p->ConversationFaceTalker, false);
		}
	}
}

//============================================================================
//
// HandleReply
//
// Run by the netcode on all machines.
//
//============================================================================

static void HandleReply(player_t *player, bool isconsole, int nodenum, int replynum)
{
	const char *replyText = NULL;
	FStrifeDialogueReply *reply;
	FStrifeDialogueNode *node;
	AActor *npc;
	bool takestuff;
	int i;
	auto Level = player->mo->Level;
	if (player->ConversationNPC == nullptr || (unsigned)nodenum >= Level->StrifeDialogues.Size())
	{
		return;
	}

	// Find the reply.
	node = Level->StrifeDialogues[nodenum];
	for (i = 0, reply = node->Children; reply != NULL && i != replynum; ++i, reply = reply->Next)
	{ }
	npc = player->ConversationNPC;
	if (reply == NULL)
	{
		// The default reply was selected
		if (!(npc->flags8 & MF8_DONTFACETALKER))
			npc->Angles.Yaw = player->ConversationNPCAngle;
		npc->flags5 &= ~MF5_INCONVERSATION;
		if (gameaction != ga_slideshow) ClearConversationStuff(player);
		return;
	}

	// Check if you have the requisite items for this choice
	for (i = 0; i < (int)reply->ItemCheck.Size(); ++i)
	{
		if (!CheckStrifeItem(player, reply->ItemCheck[i].Item, reply->ItemCheck[i].Amount))
		{
			// No, you don't. Say so and let the NPC animate negatively.
			if (reply->QuickNo.IsNotEmpty() && isconsole)
			{
				TerminalResponse(reply->QuickNo);
			}
			npc->ConversationAnimation(2);
			if (!(npc->flags8 & MF8_DONTFACETALKER))
				npc->Angles.Yaw = player->ConversationNPCAngle;
			npc->flags5 &= ~MF5_INCONVERSATION;
			if (gameaction != ga_slideshow) ClearConversationStuff(player);
			return;
		}
	}

	// Yay, you do! Let the NPC animate affirmatively.
	npc->ConversationAnimation(1);

	// If this reply gives you something, then try to receive it.
	takestuff = true;
	if (reply->GiveType != NULL)
	{
		if (reply->GiveType->IsDescendantOf(NAME_Inventory))
		{
			if (reply->GiveType->IsDescendantOf(NAME_Weapon))
			{
				if (player->mo->FindInventory(reply->GiveType) != NULL)
				{
					takestuff = false;
				}
			}
	
			if (takestuff)
			{
				auto item = Spawn(player->mo->Level, reply->GiveType);
				// Items given here should not count as items!
				item->ClearCounters();
				if (item->GetClass()->TypeName == NAME_FlameThrower)
				{
					// The flame thrower gives less ammo when given in a dialog
					item->IntVar(NAME_AmmoGive1) = 40;
				}
				item->flags |= MF_DROPPED;
				if (!CallTryPickup(item, player->mo))
				{
					item->Destroy();
					takestuff = false;
				}
			}
		
			if (reply->GiveType->IsDescendantOf("SlideshowStarter"))
				gameaction = ga_slideshow;
		}
		else
		{
			// Trying to give a non-inventory item.
			takestuff = false;
			if (isconsole)
			{
				Printf("Attempting to give non-inventory item %s\n", reply->GiveType->TypeName.GetChars());
			}
		}
	}

	if (reply->ActionSpecial != 0)
	{
		takestuff |= !!P_ExecuteSpecial(player->mo->Level, reply->ActionSpecial, NULL, player->mo, false,
			reply->Args[0], reply->Args[1], reply->Args[2], reply->Args[3], reply->Args[4]);
	}

	// Take away required items if the give was successful or none was needed.
	if (takestuff)
	{
		for (i = 0; i < (int)reply->ItemCheck.Size(); ++i)
		{
			TakeStrifeItem (player, reply->ItemCheck[i].Item, reply->ItemCheck[i].Amount);
		}
		replyText = reply->QuickYes;
	}
	else
	{
		replyText = "$txt_haveenough";
	}

	// Update the quest log, if needed.
	if (reply->LogString.IsNotEmpty())
	{
		const char *log = reply->LogString;
		if (log[0] == '$')
		{
			log = GStrings(log + 1);
		}

		player->SetLogText(log);
	}
	else if (reply->LogNumber != 0) 
	{
		player->SetLogNumber(reply->LogNumber);
	}

	if (nullptr != replyText && '\0' != replyText[0] && isconsole)
	{
		TerminalResponse(replyText);
	}

	// Does this reply alter the speaker's conversation node? If NextNode is positive,
	// the next time they talk, they will show the new node. If it is negative, then they
	// will show the new node right away without terminating the dialogue.
	if (reply->NextNode != 0)
	{
		int rootnode = npc->ConversationRoot;
		const unsigned next = (unsigned)(rootnode + reply->NextNode - 1);
		FString nextname = reply->NextNodeName;

		if (next < Level->StrifeDialogues.Size())
		{
			npc->Conversation = Level->StrifeDialogues[next];

			if (!(reply->CloseDialog))
			{
				if (gameaction != ga_slideshow)
				{
					P_StartConversation (npc, player->mo, player->ConversationFaceTalker, false);
					return;
				}
				else
				{
					S_StopSound (npc, CHAN_VOICE);
				}
			}
		}
		else
		{
			if (nextname.IsEmpty())
				Printf ("Next node %u is invalid, no such dialog page\n", next);
			else
				Printf ("Next node %u ('%s') is invalid, no such dialog page\n", next, nextname.GetChars());
		}
	}

	if (!(npc->flags8 & MF8_DONTFACETALKER))
		npc->Angles.Yaw = player->ConversationNPCAngle;

	// [CW] Set these to NULL because we're not using to them
	// anymore. However, this can interfere with slideshows
	// so we don't set them to NULL in that case.
	if (gameaction != ga_slideshow)
	{
		npc->flags5 &= ~MF5_INCONVERSATION;
		ClearConversationStuff(player);
	}

	if (isconsole)
	{
		I_SetMusicVolume (player->mo->Level->MusicVolume);
	}
}

//============================================================================
//
// P_ConversationCommand
//
// Complete a conversation command.
//
//============================================================================

void P_ConversationCommand (int netcode, int pnum, uint8_t **stream)
{
	player_t *player = &players[pnum];

	// The conversation menus are normally closed by the menu code, but that
	// doesn't happen during demo playback, so we need to do it here.
	if (demoplayback && CurrentMenu != NULL && CurrentMenu->IsKindOf("ConversationMenu"))
	{
		CurrentMenu->Close();
	}
	if (netcode == DEM_CONVREPLY)
	{
		int nodenum = ReadWord(stream);
		int replynum = ReadByte(stream);
		HandleReply(player, pnum == consoleplayer, nodenum, replynum);
	}
	else
	{
		assert(netcode == DEM_CONVNULL || netcode == DEM_CONVCLOSE);
		if (player->ConversationNPC != NULL)
		{
			if (!(player->ConversationNPC->flags8 & MF8_DONTFACETALKER))
				player->ConversationNPC->Angles.Yaw = player->ConversationNPCAngle;
			player->ConversationNPC->flags5 &= ~MF5_INCONVERSATION;
		}
		if (netcode == DEM_CONVNULL)
		{
			ClearConversationStuff(player);
		}
	}
}

//============================================================================
//
// TerminalResponse
//
// Similar to C_MidPrint, but lower and colored and sized to match the
// rest of the dialogue text.
//
//============================================================================

static void TerminalResponse (const char *str)
{
	if (str != NULL)
	{
		// handle string table replacement
		if (str[0] == '$')
		{
			str = GStrings(str + 1);
		}

		if (StatusBar != NULL)
		{
			Printf(PRINT_DEFAULT, PRINTF_NONOTIFY, "%s\n", str);
			// The message is positioned a bit above the menu choices, because
			// merchants can tell you something like this but continue to show
			// their dialogue screen. I think most other conversations use this
			// only as a response for terminating the dialogue.
			StatusBar->AttachMessage(Create<DHUDMessageFadeOut>(nullptr, str,
				float(CleanWidth/2) + 0.4f, float(ConversationMenuY - 110 + CleanHeight/2), CleanWidth, -CleanHeight,
				CR_UNTRANSLATED, 3.f, 1.f), MAKE_ID('T','A','L','K'));
		}
		else
		{
			Printf("%s\n", str);
		}
	}
}

DEFINE_FIELD(FStrifeDialogueNode, DropType);
DEFINE_FIELD(FStrifeDialogueNode, ThisNodeNum);
DEFINE_FIELD(FStrifeDialogueNode, ItemCheckNode);
DEFINE_FIELD(FStrifeDialogueNode, SpeakerType);
DEFINE_FIELD(FStrifeDialogueNode, SpeakerName);
DEFINE_FIELD(FStrifeDialogueNode, SpeakerVoice);
DEFINE_FIELD(FStrifeDialogueNode, Backdrop);
DEFINE_FIELD(FStrifeDialogueNode, Dialogue);
DEFINE_FIELD(FStrifeDialogueNode, Goodbye);
DEFINE_FIELD(FStrifeDialogueNode, Children);
DEFINE_FIELD(FStrifeDialogueNode, MenuClassName);
DEFINE_FIELD(FStrifeDialogueNode, UserData);

DEFINE_FIELD(FStrifeDialogueReply, Next);
DEFINE_FIELD(FStrifeDialogueReply, GiveType);
DEFINE_FIELD(FStrifeDialogueReply, ActionSpecial);
DEFINE_FIELD(FStrifeDialogueReply, Args);
DEFINE_FIELD(FStrifeDialogueReply, PrintAmount);
DEFINE_FIELD(FStrifeDialogueReply, Reply);
DEFINE_FIELD(FStrifeDialogueReply, QuickYes);
DEFINE_FIELD(FStrifeDialogueReply, QuickNo);
DEFINE_FIELD(FStrifeDialogueReply, LogString);
DEFINE_FIELD(FStrifeDialogueReply, NextNode);
DEFINE_FIELD(FStrifeDialogueReply, LogNumber);
DEFINE_FIELD(FStrifeDialogueReply, NeedsGold);

/*
** playermenu.cpp
** The player setup menu
**
**---------------------------------------------------------------------------
** Copyright 2001-2010 Randy Heit
** Copyright 2010-2017 Christoph Oelckers
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

//=============================================================================
//
// 
//
//=============================================================================

class OptionMenuItemPlayerNameField : OptionMenuItemTextField
{
	OptionMenuItemTextField Init(String label)
	{
		Super.Init(label, "", null);
		mEnter = null;
		return self;
	}

	override bool, String GetString (int i)
	{
		if (i == 0)
		{
			return true, players[consoleplayer].GetUserName();
;
		}
		return false, "";
	}

	override bool SetString (int i, String s)
	{
		if (i == 0)
		{
			PlayerMenu.PlayerNameChanged(s);
			return true;
		}
		return false;
	}
}

//=============================================================================
//
// 
//
//=============================================================================

class OptionMenuItemPlayerTeamItem : OptionMenuItemOptionBase
{
	OptionMenuItemPlayerTeamItem Init(String label, Name values)
	{
		Super.Init(label, 'none', values, null , false);
		return self;
	}

	//=============================================================================
	override int GetSelection()
	{
		int Selection = -1;
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 0)
		{
			int myteam = players[consoleplayer].GetTeam();
			let f = double(myteam ==  Team.NoTeam? 0 : myteam + 1);
			for(int i = 0; i < cnt; i++)
			{ 
				if (f ~== OptionValues.GetValue(mValues, i))
				{
					Selection = i;
					break;
				}
			}
		}
		return Selection;
	}

	override void SetSelection(int Selection)
	{
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 0)
		{
			PlayerMenu.TeamChanged(Selection);
		}
	}
}

//=============================================================================
//
// 
//
//=============================================================================

class OptionMenuItemPlayerColorItem : OptionMenuItemOptionBase
{
	OptionMenuItemPlayerColorItem Init(String label, Name values)
	{
		Super.Init(label, 'none', values, null , false);
		return self;
	}

	//=============================================================================
	override int GetSelection()
	{
		int Selection = -1;
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 0)
		{
			int mycolorset = players[consoleplayer].GetColorSet();
			let f = double(mycolorset);
			for(int i = 0; i < cnt; i++)
			{ 
				if (f ~== OptionValues.GetValue(mValues, i))
				{
					Selection = i;
					break;
				}
			}
		}
		return Selection;
	}

	override void SetSelection(int Selection)
	{
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 0)
		{
			let val = int(OptionValues.GetValue(mValues, Selection));
			PlayerMenu.ColorSetChanged(val);
			let menu = NewPlayerMenu(Menu.GetCurrentMenu());
			if (menu) menu.UpdateTranslation();
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

class OptionMenuItemPlayerColorSlider : OptionMenuSliderBase
{
	int mChannel;
	
	OptionMenuItemPlayerColorSlider Init(String label, int channel)
	{
		Super.Init(label, 0, 255, 16, false, 'none');
		mChannel = channel;
		return self;
	}

	override double GetSliderValue()
	{
		Color colr = players[consoleplayer].GetColor();
		if (mChannel == 0) return colr.r;
		else if (mChannel == 1) return colr.g;
		else return colr.b;
	}

	override void SetSliderValue(double val)
	{
		Color colr = players[consoleplayer].GetColor();
		int r = colr.r;
		int g = colr.g;
		int b = colr.b;
		if (mChannel == 0) r = int(val);
		else if (mChannel == 1) g = int(val);
		else b = int(val);
		PlayerMenu.ColorChanged(r, g, b);
		let menu = NewPlayerMenu(Menu.GetCurrentMenu());
		if (menu) menu.UpdateTranslation();
	}
	
	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		int mycolorset = players[consoleplayer].GetColorSet();
		if (mycolorset == -1)
		{
			return super.Draw(desc, y, indent, selected);
		}
		return indent;
	}
	
	override bool Selectable()
	{
		int mycolorset = players[consoleplayer].GetColorSet();
		return (mycolorset == -1);
	}
}

//=============================================================================
//
// 
//
//=============================================================================

class OptionMenuItemPlayerClassItem : OptionMenuItemOptionBase
{
	OptionMenuItemPlayerClassItem Init(String label, Name values)
	{
		Super.Init(label, 'none', values, null , false);
		return self;
	}

	//=============================================================================
	override int GetSelection()
	{
		int Selection = -1;
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 0)
		{
			double f = players[consoleplayer].GetPlayerClassNum();
			for(int i = 0; i < cnt; i++)
			{ 
				if (f ~== OptionValues.GetValue(mValues, i))
				{
					Selection = i;
					break;
				}
			}
		}
		return Selection;
	}

	override void SetSelection(int Selection)
	{
		
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 1)
		{
			let val = int(OptionValues.GetValue(mValues, Selection));
			let menu = NewPlayerMenu(Menu.GetCurrentMenu());
			if (menu) 
			{
				menu.PickPlayerClass(val);
				PlayerMenu.ClassChanged(val, menu.mPlayerClass);
				menu.mPlayerDisplay.SetValue(ListMenuItemPlayerDisplay.PDF_CLASS, players[consoleplayer].GetPlayerClassNum());
				menu.UpdateSkins();
			}
		}
	}
}

//=============================================================================
//
// 
//
//=============================================================================

class OptionMenuItemPlayerSkinItem : OptionMenuItemOptionBase
{
	OptionMenuItemPlayerSkinItem Init(String label, Name values)
	{
		Super.Init(label, 'none', values, null , false);
		return self;
	}

	//=============================================================================
	override int GetSelection()
	{
		int Selection = 0;
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 1)
		{
			double f = players[consoleplayer].GetSkin();
			for(int i = 0; i < cnt; i++)
			{ 
				if (f ~== OptionValues.GetValue(mValues, i))
				{
					Selection = i;
					break;
				}
			}
		}
		return Selection;
	}

	override void SetSelection(int Selection)
	{
		
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 1)
		{
			let val = int(OptionValues.GetValue(mValues, Selection));
			let menu = NewPlayerMenu(Menu.GetCurrentMenu());
			PlayerMenu.SkinChanged(val);
			if (menu) 
			{
				menu.mPlayerDisplay.SetValue(ListMenuItemPlayerDisplay.PDF_SKIN, val);
				menu.UpdateTranslation();
			}
		}
	}
}

//=============================================================================
//
// 
//
//=============================================================================

class OptionMenuItemPlayerGenderItem : OptionMenuItemOptionBase
{
	OptionMenuItemPlayerGenderItem Init(String label, Name values)
	{
		Super.Init(label, 'none', values, null , false);
		return self;
	}

	//=============================================================================
	override int GetSelection()
	{
		return players[consoleplayer].GetGender();
	}

	override void SetSelection(int Selection)
	{
		PlayerMenu.GenderChanged(Selection);
	}
}

//=============================================================================
//
//
//
//=============================================================================

class OptionMenuItemAutoaimSlider : OptionMenuSliderBase
{
	OptionMenuItemAutoaimSlider Init(String label)
	{
		Super.Init(label, 0, 35, 1, false, 'none');
		return self;
	}

	override double GetSliderValue()
	{
		return players[consoleplayer].GetAutoaim();
	}

	override void SetSliderValue(double val)
	{
		PlayerMenu.AutoaimChanged(val);
	}
}

//=============================================================================
//
// 
//
//=============================================================================

class OptionMenuItemPlayerSwitchOnPickupItem : OptionMenuItemOptionBase
{
	OptionMenuItemPlayerSwitchOnPickupItem Init(String label, Name values)
	{
		Super.Init(label, 'none', values, null , false);
		return self;
	}

	//=============================================================================
	override int GetSelection()
	{
		return players[consoleplayer].GetNeverSwitch()? 1:0;
	}

	override void SetSelection(int Selection)
	{
		PlayerMenu.SwitchOnPickupChanged(Selection);
	}
}

//=============================================================================
//
//
//
//=============================================================================

class OptionMenuItemPlayerPronounsItem : OptionMenuItemOptionBase
{
	OptionMenuItemPlayerPronounsItem Init(String label, Name values)
	{
		Super.Init(label, 'none', values, null, false);
		return self;
	}

	//=============================================================================
	override int GetSelection()
	{
		let pronounsStr = players[consoleplayer].GetPronouns();
		Array<String> pronouns;
		pronounsStr.Split(pronouns, "/");

		let pronoun = "Custom";
		for(int i = 0; i < PRONOUN_MAX; i++)
		{
			bool matched = true;
			for(int j = 0; j < pronouns.Size(); j++)
			{
				if (!(pronouns[j] ~== PlayerInfo.DefaultPronouns[i * PRONOUN_SET_SIZE + j]))
				{
					matched = false;
					break;
				}
			}

			if (matched)
			{
				pronoun = PlayerInfo.DefaultPronouns[i * PRONOUN_SET_SIZE];
				break;
			}
		}

		int Selection = -1;
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 0)
		{
			for(int i = 0; i < cnt; i++)
			{
				if (pronoun ~== OptionValues.GetText(mValues, i))
				{
					Selection = i;
					break;
				}
			}
		}
		return Selection;
	}

	override void SetSelection(int Selection)
	{
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 0)
		{
			// Don't select 'Custom'
			if (OptionValues.GetText(mValues, Selection) == "Custom")
			{
				Selection = GetSelection() > 1? 1 : cnt - 1;
			}

			let val = OptionValues.GetText(mValues, Selection);
			PlayerMenu.PronounsChanged(val.MakeLower());
		}
	}

	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		if (Selectable())
		{
			return super.Draw(desc, y, indent, selected);
		}
		return indent;
	}

	override bool Selectable()
	{
		// auto falls back to enu
		return language == "auto" || language.Left(2) == "en";
	}
}

//=============================================================================
//
//
//
//=============================================================================

class OptionMenuItemPlayerPronounField : OptionMenuItemTextField
{
	int mPronoun;

	OptionMenuItemPlayerPronounField Init(String label, int pronoun)
	{
		Super.Init(label, "");
		mPronoun = pronoun;
		return self;
	}

	static clearscope String BuildPronounsString(Array<String> pronouns, int count = -1)
	{
		if (count < 0) count = pronouns.Size();

		let pronounsStr = pronouns[0];
		for(int j = 1; j < count; j++)
		{
			pronounsStr.AppendFormat("/%s", pronouns[j]);
		}

		return pronounsStr;
	}

	static clearscope String ShortestUniqueMatch(Array<String> pronouns)
	{
		bool notMatching[PRONOUN_MAX];
		int matchCount = PRONOUN_MAX;
		int uniqueMatchEnd = -1;

		for(int i = 0; i < pronouns.Size(); i++)
		{
			for(int j = 0; j < PRONOUN_MAX; j++)
			{
				if (notMatching[j]) continue;

				if (PlayerInfo.DefaultPronouns[j * PRONOUN_SET_SIZE + i] != pronouns[i])
				{
					notMatching[j] = true;
					matchCount--;

					if (matchCount == 1 && uniqueMatchEnd < 0)
					{
						// Only one match left - unique match ends here
						uniqueMatchEnd = i;
					}
					else if (matchCount == 0)
					{
						// Didn't match any defaults - return the whole thing
						return BuildPronounsString(pronouns);
					}
				}
			}
		}

		// Fully matched a default pronoun set - return it
		return BuildPronounsString(pronouns, uniqueMatchEnd);
	}

	override bool, String GetString (int i)
	{
		if (i == 0)
		{
			let pronounsStr = players[consoleplayer].GetPronouns();
			Array<String> pronouns;
			pronounsStr.Split(pronouns, "/");

			return true, pronouns[mPronoun];
		}
		return false, "";
	}

	override bool SetString (int i, String s)
	{
		if (i == 0)
		{
			let slash = s.IndexOf("/");
			if (slash != -1) s = s.Left(slash);

			Array<String> pronouns;
			players[consoleplayer].GetPronouns().Split(pronouns, "/");
			pronouns[mPronoun] = s;

			PlayerMenu.PronounsChanged(ShortestUniqueMatch(pronouns));
			return true;
		}
		return false;
	}

	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		if (Selectable())
		{
			return super.Draw(desc, y, indent, selected);
		}
		return indent;
	}

	override bool Selectable()
	{
		// auto falls back to enu
		return language == "auto" || language.Left(2) == "en";
	}
}

//=============================================================================
//
//
//
//=============================================================================

class NewPlayerMenu : OptionMenu
{
	protected native static void UpdateColorsets(PlayerClass cls);
	protected native static void UpdateSkinOptions(PlayerClass cls);

	PlayerClass mPlayerClass;
	int mRotation;
	PlayerMenuPlayerDisplay mPlayerDisplay;
	
	const PLAYERDISPLAY_X = 170;
	const PLAYERDISPLAY_Y = 60;
	const PLAYERDISPLAY_W = 144;
	const PLAYERDISPLAY_H = 160;
	const PLAYERDISPLAY_SPACE = 180;
	
	override void Init(Menu parent, OptionMenuDescriptor desc)
	{
		Super.Init(parent, desc);
		let BaseColor = gameinfo.gametype == GAME_Hexen? 0x200000 : 0x000700;
		let AddColor = gameinfo.gametype == GAME_Hexen? 0x800040 : 0x405340;
		mPlayerDisplay = new("PlayerMenuPlayerDisplay");
		mPlayerDisplay.init(BaseColor, AddColor);
		PickPlayerClass();
		
		PlayerInfo p = players[consoleplayer];
		mRotation = 0;

		mPlayerDisplay.SetValue(ListMenuItemPlayerDisplay.PDF_ROTATION, 0);
		mPlayerDisplay.SetValue(ListMenuItemPlayerDisplay.PDF_MODE, 1);
		mPlayerDisplay.SetValue(ListMenuItemPlayerDisplay.PDF_TRANSLATE, 1);
		mPlayerDisplay.SetValue(ListMenuItemPlayerDisplay.PDF_CLASS, p.GetPlayerClassNum());
		UpdateSkins();
	}

	override int GetIndent()
	{
		return Super.GetIndent() - 75*CleanXfac_1;
	}
	

	//=============================================================================
	//
	//
	//
	//=============================================================================
	
	void UpdateTranslation()
	{
		Translation.SetPlayerTranslation(TRANSLATION_Players, MAXPLAYERS, consoleplayer, mPlayerClass);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	static int GetPlayerClassIndex(int pick = -100)
	{
		int pclass = 0;
		// [GRB] Pick a class from player class list
		if (PlayerClasses.Size () > 1)
		{
			pclass = pick == -100? players[consoleplayer].GetPlayerClassNum() : pick;

			if (pclass < 0)
			{
				pclass = (MenuTime() >> 7) % PlayerClasses.Size ();
			}
		}
		return pclass;
	}
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	void PickPlayerClass(int pick = -100)
	{
		let PlayerClassIndex = GetPlayerClassIndex(pick);
		mPlayerClass = PlayerClasses[PlayerClassIndex];
		UpdateColorsets(mPlayerClass);
		UpdateSkinOptions(mPlayerClass);
		UpdateTranslation();
	}

	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	void UpdateSkins()
	{
		PlayerInfo p = players[consoleplayer];
		if (mPlayerClass != NULL && !(GetDefaultByType (mPlayerClass.Type).bNoSkin) && p.GetPlayerClassNum() != -1)
		{
			mPlayerDisplay.SetValue(ListMenuItemPlayerDisplay.PDF_SKIN, p.GetSkin());
		}
		UpdateTranslation();
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool OnUIEvent(UIEvent ev)
	{
		if (ev.Type == UIEvent.Type_Char && ev.KeyChar == 32)
		{
			// turn the player sprite around
			mRotation = 8 - mRotation;
			mPlayerDisplay.SetValue(ListMenuItemPlayerDisplay.PDF_ROTATION, mRotation);
			return true;
		}
		return Super.OnUIEvent(ev);
	}


	//=============================================================================
	//
	//
	//
	//=============================================================================
	
	override void Ticker()
	{
		mPlayerDisplay.Ticker();
	}
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer()
	{
		Super.Drawer();
		mPlayerDisplay.Drawer(false);
		
		int x = screen.GetWidth()/(CleanXfac_1*2) + PLAYERDISPLAY_X + PLAYERDISPLAY_W/2;
		int y = PLAYERDISPLAY_Y + PLAYERDISPLAY_H + 5;
		String str = Stringtable.Localize("$PLYRMNU_PRESSSPACE");
		screen.DrawText (NewSmallFont, Font.CR_GOLD, x - NewSmallFont.StringWidth(str)/2, y, str, DTA_VirtualWidth, CleanWidth_1, DTA_VirtualHeight, CleanHeight_1, DTA_KeepRatio, true);
		str = Stringtable.Localize(mRotation ? "$PLYRMNU_SEEFRONT" : "$PLYRMNU_SEEBACK");
		y += NewSmallFont.GetHeight();
		screen.DrawText (NewSmallFont, Font.CR_GOLD,x - NewSmallFont.StringWidth(str)/2, y, str, DTA_VirtualWidth, CleanWidth_1, DTA_VirtualHeight, CleanHeight_1, DTA_KeepRatio, true);

	}	
}

/*
** optionmenu.cpp
** Handler class for the option menus and associated items
**
**---------------------------------------------------------------------------
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

struct FOptionMenuSettings native version("2.4")
{
	int mTitleColor;
	int mFontColor;
	int mFontColorValue;
	int mFontColorMore;
	int mFontColorHeader;
	int mFontColorHighlight;
	int mFontColorSelection;
	int mLinespacing;
}

class OptionMenuDescriptor : MenuDescriptor native
{
	native Array<OptionMenuItem> mItems;
	native String mTitle;
	native int mSelectedItem;
	native int mDrawTop;
	native int mScrollTop;
	native int mScrollPos;
	native int mIndent;
	native int mPosition;
	native bool mDontDim;
	native Font mFont;

	void Reset()
	{
		// Reset the default settings (ignore all other values in the struct)
		mPosition = 0;
		mScrollTop = 0;
		mIndent = 0;
		mDontDim = 0;
	}
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	void CalcIndent()
	{
		// calculate the menu indent
		int widest = 0, thiswidth;

		for (int i = 0; i < mItems.Size(); i++)
		{
			thiswidth = mItems[i].GetIndent();
			if (thiswidth > widest) widest = thiswidth;
		}
		mIndent =  widest + 4;
	}
}


class OptionMenu : Menu
{
	OptionMenuDescriptor mDesc;
	bool CanScrollUp;
	bool CanScrollDown;
	int VisBottom;
	OptionMenuItem mFocusControl;

	//=============================================================================
	//
	//
	//
	//=============================================================================

	virtual void Init(Menu parent, OptionMenuDescriptor desc)
	{
		mParentMenu = parent;
		mDesc = desc;
		DontDim = desc.mDontDim;

		let itemCount = mDesc.mItems.size();
		if (itemCount > 0)
		{
			let last = mDesc.mItems[itemCount - 1];
			bool lastIsText = (last is "OptionMenuItemStaticText");
			if (lastIsText)
			{
				String text = last.mLabel;
				bool lastIsSpace = (text == "" || text == " ");
				if (lastIsSpace)
				{
					mDesc.mItems.Pop();
				}
			}
		}

		if (mDesc.mSelectedItem == -1) mDesc.mSelectedItem = FirstSelectable();
		mDesc.CalcIndent();

		// notify all items that the menu was just created.
		for(int i=0;i<mDesc.mItems.Size(); i++)
		{
			mDesc.mItems[i].OnMenuCreated();
		}
	}

	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	OptionMenuItem GetItem(Name name)
	{
		for(int i = 0; i < mDesc.mItems.Size(); i++)
		{
			Name nm = mDesc.mItems[i].GetAction();
			if (nm == name) return mDesc.mItems[i];
		}
		return NULL;
	}
	

	//=============================================================================
	//
	//
	//
	//=============================================================================

	int FirstSelectable()
	{
		// Go down to the first selectable item
		int i = -1;
		do
		{
			i++;
		}
		while (i < mDesc.mItems.Size() && !mDesc.mItems[i].Selectable());
		if (i>=0 && i < mDesc.mItems.Size()) return i;
		else return -1;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool OnUIEvent(UIEvent ev)
	{
		if (ev.type == UIEvent.Type_WheelUp)
		{
			int scrollamt = MIN(2, mDesc.mScrollPos);
			mDesc.mScrollPos -= scrollamt;
			return true;
		}
		else if (ev.type == UIEvent.Type_WheelDown)
		{
			if (CanScrollDown)
			{
				if (VisBottom >= 0 && VisBottom < (mDesc.mItems.Size()-2))
				{
					mDesc.mScrollPos += 2;
					VisBottom += 2;
				}
				else
				{
					mDesc.mScrollPos++;
					VisBottom++;
				}
			}
			return true;
		}
		else if (ev.type == UIEvent.Type_Char)
		{
			int key = String.CharLower(ev.keyChar);
			int itemsNumber = mDesc.mItems.Size();
			int direction = ev.IsAlt ? -1 : 1;
			for (int i = 0; i < itemsNumber; ++i)
			{
				int index = (mDesc.mSelectedItem + direction * (i + 1) + itemsNumber) % itemsNumber;
				if (!mDesc.mItems[index].Selectable()) continue;
				String label = StringTable.Localize(mDesc.mItems[index].mLabel);
				int firstLabelCharacter = String.CharLower(label.GetNextCodePoint(0));
				if (firstLabelCharacter == key)
				{
					mDesc.mSelectedItem = index;
					break;
				}
			}
			if (mDesc.mSelectedItem <= mDesc.mScrollTop + mDesc.mScrollPos
				|| mDesc.mSelectedItem >= VisBottom)
			{
				mDesc.mScrollPos = MAX(mDesc.mSelectedItem - mDesc.mScrollTop - 1, 0);
			}
		}
		return Super.OnUIEvent(ev);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		int startedAt = mDesc.mSelectedItem;

		switch (mkey)
		{
		case MKEY_Up:
			if (mDesc.mSelectedItem == -1)
			{
				mDesc.mSelectedItem = FirstSelectable();
				break;
			}
			do
			{
				--mDesc.mSelectedItem;

				if (mDesc.mScrollPos > 0 &&
					mDesc.mSelectedItem <= mDesc.mScrollTop + mDesc.mScrollPos)
				{
					mDesc.mScrollPos = MAX(mDesc.mSelectedItem - mDesc.mScrollTop - 1, 0);
				}

				if (mDesc.mSelectedItem < 0) 
				{
					// Figure out how many lines of text fit on the menu
					int y = mDesc.mPosition;

					if (y <= 0)
					{
						y = DrawCaption(mDesc.mTitle, -y, false);
					}
					int	rowheight = OptionMenuSettings.mLinespacing * CleanYfac_1;
					int maxitems = (screen.GetHeight() - rowheight - y) / rowheight + 1;

					mDesc.mScrollPos = MAX (0, mDesc.mItems.Size() - maxitems + mDesc.mScrollTop);
					mDesc.mSelectedItem = mDesc.mItems.Size()-1;
				}
			}
			while (!mDesc.mItems[mDesc.mSelectedItem].Selectable() && mDesc.mSelectedItem != startedAt);
			break;

		case MKEY_Down:
			if (mDesc.mSelectedItem == -1)
			{
				mDesc.mSelectedItem = FirstSelectable();
				break;
			}
			do
			{
				++mDesc.mSelectedItem;
				
				if (CanScrollDown && mDesc.mSelectedItem == VisBottom)
				{
					mDesc.mScrollPos++;
					VisBottom++;
				}
				if (mDesc.mSelectedItem >= mDesc.mItems.Size()) 
				{
					if (startedAt == -1)
					{
						mDesc.mSelectedItem = -1;
						mDesc.mScrollPos = -1;
						break;
					}
					else
					{
						mDesc.mSelectedItem = 0;
						mDesc.mScrollPos = 0;
					}
				}
			}
			while (!mDesc.mItems[mDesc.mSelectedItem].Selectable() && mDesc.mSelectedItem != startedAt);
			break;

		case MKEY_PageUp:
			if (mDesc.mScrollPos > 0)
			{
				mDesc.mScrollPos -= VisBottom - mDesc.mScrollPos - mDesc.mScrollTop;
				if (mDesc.mScrollPos < 0)
				{
					mDesc.mScrollPos = 0;
				}
				if (mDesc.mSelectedItem != -1)
				{
					mDesc.mSelectedItem = mDesc.mScrollTop + mDesc.mScrollPos + 1;
					while (!mDesc.mItems[mDesc.mSelectedItem].Selectable())
					{
						if (++mDesc.mSelectedItem >= mDesc.mItems.Size())
						{
							mDesc.mSelectedItem = 0;
						}
					}
					if (mDesc.mScrollPos > mDesc.mSelectedItem)
					{
						mDesc.mScrollPos = mDesc.mSelectedItem;
					}
				}
			}
			break;

		case MKEY_PageDown:
			if (CanScrollDown)
			{
				int pagesize = VisBottom - mDesc.mScrollPos - mDesc.mScrollTop;
				mDesc.mScrollPos += pagesize;
				if (mDesc.mScrollPos + mDesc.mScrollTop + pagesize > mDesc.mItems.Size())
				{
					mDesc.mScrollPos = mDesc.mItems.Size() - mDesc.mScrollTop - pagesize;
				}
				if (mDesc.mSelectedItem != -1)
				{
					mDesc.mSelectedItem = mDesc.mScrollTop + mDesc.mScrollPos;
					while (!mDesc.mItems[mDesc.mSelectedItem].Selectable())
					{
						if (++mDesc.mSelectedItem >= mDesc.mItems.Size())
						{
							mDesc.mSelectedItem = 0;
						}
					}
					if (mDesc.mScrollPos > mDesc.mSelectedItem)
					{
						mDesc.mScrollPos = mDesc.mSelectedItem;
					}
				}
			}
			break;

		case MKEY_Enter:
			if (mDesc.mSelectedItem >= 0 && mDesc.mItems[mDesc.mSelectedItem].Activate()) 
			{
				return true;
			}
			// fall through to default
		default:
			if (mDesc.mSelectedItem >= 0 && 
				mDesc.mItems[mDesc.mSelectedItem].MenuEvent(mkey, fromcontroller)) return true;
			return Super.MenuEvent(mkey, fromcontroller);
		}

		if (mDesc.mSelectedItem != startedAt)
		{
			MenuSound ("menu/cursor");
		}
		return true;
	}

	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MouseEvent(int type, int x, int y)
	{
		y = (y / CleanYfac_1) - mDesc.mDrawTop;

		if (mFocusControl)
		{
			mFocusControl.MouseEvent(type, x, y);
			return true;
		}
		else
		{
			int yline = (y / OptionMenuSettings.mLinespacing);
			if (yline >= mDesc.mScrollTop)
			{
				yline += mDesc.mScrollPos;
			}
			if (yline >= 0 && yline < mDesc.mItems.Size() && mDesc.mItems[yline].Selectable())
			{
				if (yline != mDesc.mSelectedItem)
				{
					mDesc.mSelectedItem = yline;
				}
				mDesc.mItems[yline].MouseEvent(type, x, y);
				return true;
			}
		}
		mDesc.mSelectedItem = -1;
		return Super.MouseEvent(type, x, y);
	}

	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Ticker ()
	{
		Super.Ticker();
		for(int i = 0; i < mDesc.mItems.Size(); i++)
		{
			mDesc.mItems[i].Ticker();
		}
	}
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	virtual int GetIndent()
	{
		int indent = max(0, (mDesc.mIndent + 40) - CleanWidth_1 / 2);
		return screen.GetWidth() / 2 + indent * CleanXfac_1;
	}

	//=============================================================================
	//
	// draws and/or measures the caption. 
	//
	//=============================================================================

	virtual int DrawCaption(String title, int y, bool drawit)
	{
		let font = menuDelegate.PickFont(mDesc.mFont);
		if (font && mDesc.mTitle.Length() > 0)
		{
			return menuDelegate.DrawCaption(title, font, y, drawit);
		}
		else
		{
			return y;
		}

	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer ()
	{
		int y = mDesc.mPosition;

		if (y <= 0)
		{
			y = DrawCaption(mDesc.mTitle, -y, true);
		}
		mDesc.mDrawTop = y / CleanYfac_1; // mouse checks are done in clean space.
		int fontheight = OptionMenuSettings.mLinespacing * CleanYfac_1;

		int indent = GetIndent();

		int ytop = y + mDesc.mScrollTop * 8 * CleanYfac_1;
		int lastrow = screen.GetHeight() - OptionHeight() * CleanYfac_1;

		int i;
		for (i = 0; i < mDesc.mItems.Size() && y <= lastrow; i++)
		{
			// Don't scroll the uppermost items
			if (i == mDesc.mScrollTop)
			{
				i += mDesc.mScrollPos;
				if (i >= mDesc.mItems.Size()) break;	// skipped beyond end of menu 
			}
			bool isSelected = mDesc.mSelectedItem == i;
			int cur_indent = mDesc.mItems[i].Draw(mDesc, y, indent, isSelected);
			if (cur_indent >= 0 && isSelected && mDesc.mItems[i].Selectable())
			{
				if (((MenuTime() % 8) < 6) || GetCurrentMenu() != self)
				{
					DrawOptionText(cur_indent + 3 * CleanXfac_1, y, OptionMenuSettings.mFontColorSelection, "◄");
				}
			}
			y += fontheight;
		}

		CanScrollUp = (mDesc.mScrollPos > 0);
		CanScrollDown = (i < mDesc.mItems.Size());
		VisBottom = i - 1;

		if (CanScrollUp)
		{
			DrawOptionText(screen.GetWidth() - 11 * CleanXfac_1, ytop, OptionMenuSettings.mFontColorSelection, "▲");
		}
		if (CanScrollDown)
		{
			DrawOptionText(screen.GetWidth() - 11 * CleanXfac_1 , y - 8*CleanYfac_1, OptionMenuSettings.mFontColorSelection, "▼");
		}
		Super.Drawer();
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void SetFocus(MenuItemBase fc)
	{
		mFocusControl = OptionMenuItem(fc);
	}
	
	override bool CheckFocus(MenuItemBase fc)
	{
		return mFocusControl == fc;
	}
	
	override void ReleaseFocus()
	{
		mFocusControl = NULL;
	}
}


class GLTextureGLOptions : OptionMenu
{
	private int mWarningIndex;
	private string mWarningLabel;

	override void Init(Menu parent, OptionMenuDescriptor desc)
	{
		super.Init(parent, desc);

		// Find index of warning item placeholder
		mWarningIndex = -1;
		mWarningLabel = "!HQRESIZE_WARNING!";

		for (int i=0; i < mDesc.mItems.Size(); ++i)
		{
			if (mDesc.mItems[i].mLabel == mWarningLabel)
			{
				mWarningIndex = i;
				break;
			}
		}
	}

	override void OnDestroy()
	{
		// Restore warning item placeholder
		if (mWarningIndex >= 0)
		{
			mDesc.mItems[mWarningIndex].mLabel = mWarningLabel;
		}

		Super.OnDestroy();
	}

	override void Ticker()
	{
		Super.Ticker();

		if (mWarningIndex >= 0)
		{
			string message;

			if (gl_texture_hqresizemult > 1 && gl_texture_hqresizemode > 0)
			{
				int multiplier = gl_texture_hqresizemult * gl_texture_hqresizemult;

				message = StringTable.Localize("$GLTEXMNU_HQRESIZEWARN");
				string mult = String.Format("%d", multiplier);
				message.Replace("%d", mult);
			}

			mDesc.mItems[mWarningIndex].mLabel = Font.TEXTCOLOR_CYAN .. message;
		}
	}
}

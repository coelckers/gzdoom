class HereticStatusBar : BaseStatusBar
{
	DynamicValueInterpolator mHealthInterpolator;
	HUDFont mHUDFont;
	HUDFont mIndexFont;
	HUDFont mBigFont;
	InventoryBarState diparms;
	InventoryBarState diparms_sbar;
	private int wiggle;
	

	override void Init()
	{
		Super.Init();
		SetSize(42, 320, 200);

		// Create the font used for the fullscreen HUD
		Font fnt = "HUDFONT_RAVEN";
		mHUDFont = HUDFont.Create(fnt, fnt.GetCharWidth("0") + 1, Mono_CellLeft, 1, 1);
		fnt = "INDEXFONT_RAVEN";
		mIndexFont = HUDFont.Create(fnt, fnt.GetCharWidth("0"), Mono_CellLeft);
		fnt = "BIGFONT";
		mBigFont = HUDFont.Create(fnt, fnt.GetCharWidth("0"), Mono_CellLeft, 2, 2);
		diparms = InventoryBarState.Create(mIndexFont);
		diparms_sbar = InventoryBarState.CreateNoBox(mIndexFont, boxsize:(31, 31), arrowoffs:(0,-10));
		mHealthInterpolator = DynamicValueInterpolator.Create(0, 0.25, 1, 8);
	}
	
	override int GetProtrusion(double scaleratio) const
	{
		return scaleratio > 0.7? 8 : 0;
	}

	override void NewGame ()
	{
		Super.NewGame();
		mHealthInterpolator.Reset (0);
	}

	override void Tick()
	{
		Super.Tick();
		mHealthInterpolator.Update(CPlayer.health);

		// wiggle the chain if it moves
		if (Level.time & 1)
		{
			wiggle = (mHealthInterpolator.GetValue() != CPlayer.health) && Random[ChainWiggle](0, 1);
		}
	}

	override void Draw (int state, double TicFrac)
	{
		Super.Draw (state, TicFrac);

		if (state == HUD_StatusBar)
		{
			BeginStatusBar();
			DrawMainBar (TicFrac);
		}
		else if (state == HUD_Fullscreen)
		{
			BeginHUD();
			DrawFullScreenStuff ();
		}
	}

	protected void DrawMainBar (double TicFrac)
	{
		DrawImage("BARBACK", (0, 158), DI_ITEM_OFFSETS);
		DrawImage("LTFCTOP", (0, 148), DI_ITEM_OFFSETS);
		DrawImage("RTFCTOP", (290, 148), DI_ITEM_OFFSETS);
		if (isInvulnerable())
		{
			//god mode
			DrawImage("GOD1", (16, 167), DI_ITEM_OFFSETS);
			DrawImage("GOD2", (287, 167), DI_ITEM_OFFSETS);
		}
		//health
		DrawImage("CHAINBAC", (0, 190), DI_ITEM_OFFSETS);
		// wiggle the chain if it moves
		int inthealth =  mHealthInterpolator.GetValue();
		DrawGem("CHAIN", "LIFEGEM2",inthealth, CPlayer.mo.GetMaxHealth(true), (2, 191 + wiggle), 15, 25, 16, (multiplayer? DI_TRANSLATABLE : 0) | DI_ITEM_LEFT_TOP); 
		DrawImage("LTFACE", (0, 190), DI_ITEM_OFFSETS);
		DrawImage("RTFACE", (276, 190), DI_ITEM_OFFSETS);
		DrawShader(SHADER_HORZ, (19, 190), (16, 10));
		DrawShader(SHADER_HORZ|SHADER_REVERSE, (278, 190), (16, 10));

		if (!isInventoryBarVisible())
		{
			//statbar
			if (!deathmatch)
			{
				DrawImage("LIFEBAR", (34, 160), DI_ITEM_OFFSETS);
				DrawImage("ARMCLEAR", (57, 171), DI_ITEM_OFFSETS);
				DrawString(mHUDFont, FormatNumber(mHealthInterpolator.GetValue(), 3), (88, 170), DI_TEXT_ALIGN_RIGHT);
			}
			else
			{
				DrawImage("STATBAR", (34, 160), DI_ITEM_OFFSETS);
				DrawImage("ARMCLEAR", (57, 171), DI_ITEM_OFFSETS);
				DrawString(mHUDFont, FormatNumber(CPlayer.FragCount, 3), (88, 170), DI_TEXT_ALIGN_RIGHT);
			}
			DrawString(mHUDFont, FormatNumber(GetArmorAmount(), 3), (255, 170), DI_TEXT_ALIGN_RIGHT);

			//ammo
			Ammo ammo1, ammo2;
			[ammo1, ammo2] = GetCurrentAmmo();
			if (ammo1 != null && ammo2 == null)
			{
				DrawString(mHUDFont, FormatNumber(ammo1.Amount, 3), (136, 162), DI_TEXT_ALIGN_RIGHT);
				DrawTexture(ammo1.icon, (123, 180), DI_ITEM_CENTER);
			}
			else if (ammo2 != null)
			{
				DrawString(mIndexFont, FormatNumber(ammo1.Amount, 3), (137, 165), DI_TEXT_ALIGN_RIGHT);
				DrawString(mIndexFont, FormatNumber(ammo2.Amount, 3), (137, 177), DI_TEXT_ALIGN_RIGHT);
				DrawTexture(ammo1.icon, (115, 169), DI_ITEM_CENTER);
				DrawTexture(ammo2.icon, (115, 180), DI_ITEM_CENTER);
			}

			//keys
			if (CPlayer.mo.CheckKeys(3, false, true)) DrawImage("YKEYICON", (153, 164), DI_ITEM_OFFSETS);
			if (CPlayer.mo.CheckKeys(1, false, true)) DrawImage("GKEYICON", (153, 172), DI_ITEM_OFFSETS);
			if (CPlayer.mo.CheckKeys(2, false, true)) DrawImage("BKEYICON", (153, 180), DI_ITEM_OFFSETS);

			//inventory box
			if (CPlayer.mo.InvSel != null)
			{
				DrawInventoryIcon(CPlayer.mo.InvSel, (194, 175), DI_ARTIFLASH|DI_ITEM_CENTER|DI_DIMDEPLETED, boxsize:(28, 28));
				if (CPlayer.mo.InvSel.Amount > 1)
				{
					DrawString(mIndexFont, FormatNumber(CPlayer.mo.InvSel.Amount, 3), (209, 182), DI_TEXT_ALIGN_RIGHT);
				}
			}
		}
		else
		{
			DrawImage("INVBAR", (34, 160), DI_ITEM_OFFSETS);
			DrawInventoryBar(diparms_sbar, (49, 160), 7, DI_ITEM_LEFT_TOP, HX_SHADOW);
		}
	}

	protected void DrawFullScreenStuff ()
	{
		//health
		DrawImage("PTN1A0", (51, -3));
		DrawString(mBigFont, FormatNumber(mHealthInterpolator.GetValue()), (41, -21), DI_TEXT_ALIGN_RIGHT);

		//armor
		let armor = CPlayer.mo.FindInventory("BasicArmor");
		if (armor != null && armor.Amount > 0)
		{
			DrawInventoryIcon(armor, (58, -24));
			DrawString(mBigFont, FormatNumber(armor.Amount, 3), (41, -43), DI_TEXT_ALIGN_RIGHT);
		}
		//frags/keys
		if (deathmatch)
		{
			DrawString(mHUDFont, FormatNumber(CPlayer.FragCount, 3), (70, -16));
		}
		else
		{
			Vector2 keypos = (60, -1);
			int rowc = 0;
			double roww = 0;
			for(let i = CPlayer.mo.Inv; i != null; i = i.Inv)
			{
				if (i is "Key" && i.Icon.IsValid())
				{
					DrawTexture(i.Icon, keypos, DI_ITEM_LEFT_BOTTOM);
					Vector2 size = TexMan.GetScaledSize(i.Icon);
					keypos.Y -= size.Y + 2;
					roww = max(roww, size.X);
					if (++rowc == 3)
					{
						keypos.Y = -1;
						keypos.X += roww + 2;
						roww = 0;
						rowc = 0;
					}
				}
			}
		}
		
		//ammo
		Ammo ammo1, ammo2;
		[ammo1, ammo2] = GetCurrentAmmo();
		int y = -22;
		if (ammo1 != null)
		{
			DrawTexture(ammo1.Icon, (-17, y));
			DrawString(mHUDFont, FormatNumber(ammo1.Amount, 3), (-3, y+7), DI_TEXT_ALIGN_RIGHT);
			y -= 40;
		}
		if (ammo2 != null)
		{
			DrawTexture(ammo2.Icon, (-14, y));
			DrawString(mHUDFont, FormatNumber(ammo2.Amount, 3), (-3, y+7), DI_TEXT_ALIGN_RIGHT);
			y -= 40;
		}

		if (!isInventoryBarVisible() && !Level.NoInventoryBar && CPlayer.mo.InvSel != null)
		{
			// This code was changed to always fit the item into the box, regardless of alignment or sprite size.
			// Heretic's ARTIBOX is 30x30 pixels. 
			DrawImage("ARTIBOX", (-46, -1), 0, HX_SHADOW);
			DrawInventoryIcon(CPlayer.mo.InvSel, (-46, -15), DI_ARTIFLASH|DI_ITEM_CENTER|DI_DIMDEPLETED, boxsize:(28, 28));
			if (CPlayer.mo.InvSel.Amount > 1)
			{
				DrawString(mIndexFont, FormatNumber(CPlayer.mo.InvSel.Amount, 3), (-32, -2 - mIndexFont.mFont.GetHeight()), DI_TEXT_ALIGN_RIGHT);
			}
		}
		if (isInventoryBarVisible())
		{
			DrawInventoryBar(diparms, (0, 0), 7, DI_SCREEN_CENTER_BOTTOM, HX_SHADOW);
		}
	}
}

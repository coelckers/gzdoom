
struct MugShot
{
	enum StateFlags
	{
		STANDARD = 0x0,

		XDEATHFACE = 0x1,
		ANIMATEDGODMODE = 0x2,
		DISABLEGRIN = 0x4,
		DISABLEOUCH = 0x8,
		DISABLEPAIN = 0x10,
		DISABLERAMPAGE = 0x20,
		CUSTOM = 0x40,
	}
}

class InventoryBarState ui
{
	TextureID box;
	TextureID selector;
	Vector2 boxsize;
	Vector2 boxofs;
	Vector2 selectofs;
	Vector2 innersize;
		
	TextureID left;
	TextureID right;
	Vector2 arrowoffset;

	double itemalpha;
	
	HUDFont amountfont;
	int cr;
	int flags;

	private static void Init(InventoryBarState me, HUDFont indexfont, int cr, double itemalpha, Vector2 innersize,	String leftgfx, String rightgfx, Vector2 arrowoffs, int flags)
	{
		me.itemalpha = itemalpha;
		if (innersize == (0, 0))
		{
			me.boxofs = (2, 2);
			me.innersize = me.boxsize - (4, 4);	// Default is based on Heretic's and Hexens icons.
		}
		else
		{
			me.innersize = innersize;
			me.boxofs = (me.boxsize - me.innersize) / 2;
		}
		if (me.selector.IsValid())
		{
			Vector2 sel = TexMan.GetScaledSize(me.selector);
			me.selectofs = (me.boxsize - sel) / 2;
		}
		me.left = TexMan.CheckForTexture(leftgfx, TexMan.TYPE_MiscPatch);
		me.right = TexMan.CheckForTexture(rightgfx, TexMan.TYPE_MiscPatch);
		me.arrowoffset = arrowoffs;
		me.arrowoffset.Y += me.boxsize.Y/2;	// default is centered to the side of the box.
		if (indexfont == null) 
		{
			me.amountfont = HUDFont.Create("INDEXFONT");
			if (cr == Font.CR_UNTRANSLATED) cr = Font.CR_GOLD;
		}
		else me.amountfont = indexfont;
		me.cr = cr;
		me.flags = flags;
	}

	// The default settings here are what SBARINFO is using.
	static InventoryBarState Create(HUDFont indexfont = null, int cr = Font.CR_UNTRANSLATED, double itemalpha = 1.,
							String boxgfx = "ARTIBOX", String selgfx = "SELECTBO", Vector2 innersize = (0, 0),
							String leftgfx = "INVGEML1", String rightgfx = "INVGEMR1", Vector2 arrowoffs = (0, 0), int flags = 0)
	{
		let me = new ("InventoryBarState");
		me.box = TexMan.CheckForTexture(boxgfx, TexMan.TYPE_MiscPatch);
		me.selector = TexMan.CheckForTexture(selgfx, TexMan.TYPE_MiscPatch);
		if (me.box.IsValid() || me.selector.IsValid()) me.boxsize = TexMan.GetScaledSize(me.box.IsValid()? me.box : me.selector);
		else me.boxsize = (32., 32.);
		Init(me, indexfont, cr, itemalpha, innersize, leftgfx, rightgfx, arrowoffs, flags);
		return me;
	}
	
	// The default settings here are what SBARINFO is using.
	static InventoryBarState CreateNoBox(HUDFont indexfont = null, int cr = Font.CR_UNTRANSLATED, double itemalpha = 1.,
							Vector2 boxsize = (32, 32), String selgfx = "SELECTBO", Vector2 innersize = (0, 0),
							String leftgfx = "INVGEML1", String rightgfx = "INVGEMR1", Vector2 arrowoffs = (0, 0), int flags = 0)
	{
		let me = new ("InventoryBarState");
		me.box.SetInvalid();
		me.selector = TexMan.CheckForTexture(selgfx, TexMan.TYPE_MiscPatch);
		me.boxsize = boxsize;
		Init(me, indexfont, cr, itemalpha, innersize, leftgfx, rightgfx, arrowoffs, flags);
		return me;
	}
	
}

class HUDMessageBase native ui
{
	virtual native bool Tick();
	virtual native void ScreenSizeChanged();
	virtual native void Draw(int bottom, int visibility);
}

class BaseStatusBar : StatusBarCore native
{
	enum EPop
	{
		POP_NoChange = -1,
		POP_None,
		POP_Log,
		POP_Keys,
		POP_Status
	}

	// Status face stuff
	enum EMug
	{
		ST_NUMPAINFACES		= 5,
		ST_NUMSTRAIGHTFACES	= 3,
		ST_NUMTURNFACES		= 2,
		ST_NUMSPECIALFACES	= 3,
		ST_NUMEXTRAFACES	= 2,
		ST_FACESTRIDE		= ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES,
		ST_NUMFACES			= ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES,

		ST_TURNOFFSET		= ST_NUMSTRAIGHTFACES,
		ST_OUCHOFFSET		= ST_TURNOFFSET + ST_NUMTURNFACES,
		ST_EVILGRINOFFSET	= ST_OUCHOFFSET + 1,
		ST_RAMPAGEOFFSET	= ST_EVILGRINOFFSET + 1,
		ST_GODFACE			= ST_NUMPAINFACES*ST_FACESTRIDE,
		ST_DEADFACE			= ST_GODFACE + 1
	}
	
	enum EHudState
	{
		HUD_StatusBar,
		HUD_Fullscreen,
		HUD_None,

		HUD_AltHud // Used for passing through popups to the alt hud
	}

	enum EHUDMSGLayer
	{
		HUDMSGLayer_OverHUD,
		HUDMSGLayer_UnderHUD,
		HUDMSGLayer_OverMap,

		NUM_HUDMSGLAYERS,
		HUDMSGLayer_Default = HUDMSGLayer_OverHUD,
	};

	enum IconType
	{
		ITYPE_PLAYERICON = 1000,
		ITYPE_AMMO1,
		ITYPE_AMMO2,
		ITYPE_ARMOR,
		ITYPE_WEAPON,
		ITYPE_SIGIL,
		ITYPE_WEAPONSLOT,
		ITYPE_SELECTEDINVENTORY,
	}
	
	enum HexArmorType
	{
		HEXENARMOR_ARMOR,
		HEXENARMOR_SHIELD,
		HEXENARMOR_HELM,
		HEXENARMOR_AMULET,
	};
	
	enum AmmoModes
	{
		AMMO_PRIMARY,
		AMMO_SECONDARY,
		AMMO_ANY,
		AMMO_BOTH
	};
	
	enum EHudDraw
	{
		HUD_Normal,
		HUD_HorizCenter
	}
	
	enum EShade
	{
		SHADER_HORZ = 0,
		SHADER_VERT = 2,
		SHADER_REVERSE = 1
	}
	
	const XHAIRSHRINKSIZE =(1./18);
	const XHAIRPICKUPSIZE = (2+XHAIRSHRINKSIZE);
	const POWERUPICONSIZE = 32;
	
	native bool Centering;
	native bool FixedOrigin;
	native double CrosshairSize;
	native double Displacement;
	native PlayerInfo CPlayer;
	native bool ShowLog;
	clearscope native int artiflashTick;
	clearscope native double itemflashFade;

	native void AttachMessage(HUDMessageBase msg, uint msgid = 0, int layer = HUDMSGLayer_Default);
	native HUDMessageBase DetachMessage(HUDMessageBase msg);
	native HUDMessageBase DetachMessageID(uint msgid);
	native void DetachAllMessages();
	
	native void UpdateScreenGeometry();

	virtual void Init() 
	{
	}

	native virtual void Tick ();
	native virtual void Draw (int state, double TicFrac);
	native virtual void ScreenSizeChanged ();
	native virtual clearscope void ReceivedWeapon (Weapon weapn);
	native virtual clearscope void SetMugShotState (String state_name, bool wait_till_done=false, bool reset=false);

	clearscope virtual void FlashItem (class<Inventory> itemtype) { artiflashTick = 4; itemflashFade = 0.75; }
	virtual void AttachToPlayer (PlayerInfo player) { CPlayer = player; UpdateScreenGeometry(); }
	virtual void FlashCrosshair () { CrosshairSize = XHAIRPICKUPSIZE; }
	virtual void NewGame () { if (CPlayer != null) AttachToPlayer(CPlayer); }
	virtual void ShowPop (int popnum) { ShowLog = (popnum == POP_Log && !ShowLog); }
	virtual bool MustDrawLog(int state) { return true; }

	// [MK] let the HUD handle notifications and centered print messages
	virtual bool ProcessNotify(EPrintLevel printlevel, String outline) { return false; }
	virtual void FlushNotify() {}
	virtual bool ProcessMidPrint(Font fnt, String msg, bool bold) { return false; }
	// [MK] let the HUD handle drawing the chat prompt
	virtual bool DrawChat(String txt) { return false; }

	native TextureID GetMugshot(int accuracy, int stateflags=MugShot.STANDARD, String default_face = "STF");
	native int GetTopOfStatusBar();

	// These functions are kept native solely for performance reasons. They get called repeatedly and can drag down performance easily if they get too slow.
	native static TextureID, bool GetInventoryIcon(Inventory item, int flags);

	//---------------------------------------------------------------------------
	//
	// ValidateInvFirst
	//
	// Returns an inventory item that, when drawn as the first item, is sure to
	// include the selected item in the inventory bar.
	//
	//---------------------------------------------------------------------------

	Inventory ValidateInvFirst (int numVisible) const
	{
		Inventory item;
		int i;
		let pmo = CPlayer.mo;

		if (pmo.InvFirst == NULL)
		{
			pmo.InvFirst = pmo.FirstInv();
			if (pmo.InvFirst == NULL)
			{ // Nothing to show
				return NULL;
			}
		}

		// If there are fewer than numVisible items shown, see if we can shift the
		// view left to show more.
		for (i = 0, item = pmo.InvFirst; item != NULL && i < numVisible; ++i, item = item.NextInv())
		{ }

		while (i < numVisible)
		{
			item = pmo.InvFirst.PrevInv ();
			if (item == NULL)
			{
				break;
			}
			else
			{
				pmo.InvFirst = item;
				++i;
			}
		}

		if (pmo.InvSel == NULL)
		{
			// Nothing selected, so don't move the view.
			return pmo.InvFirst == NULL ? pmo.Inv : pmo.InvFirst;
		}
		else
		{
			// Check if InvSel is already visible
			for (item = pmo.InvFirst, i = numVisible;
				 item != NULL && i != 0;
				 item = item.NextInv(), --i)
			{
				if (item == pmo.InvSel)
				{
					return pmo.InvFirst;
				}
			}
			// Check if InvSel is to the right of the visible range
			for (i = 1; item != NULL; item = item.NextInv(), ++i)
			{
				if (item == pmo.InvSel)
				{
					// Found it. Now advance InvFirst
					for (item = pmo.InvFirst; i != 0; --i)
					{
						item = item.NextInv();
					}
					return item;
				}
			}
			// Check if InvSel is to the left of the visible range
			for (item = pmo.Inv;
				item != pmo.InvSel;
				item = item.NextInv())
			{ }
			if (item != NULL)
			{
				// Found it, so let it become the first item shown
				return item;
			}
			// Didn't find the selected item, so don't move the view.
			// This should never happen.
			return pmo.InvFirst;
		}
	}
	
	//============================================================================
	//
	// DBaseStatusBar :: GetCurrentAmmo
	//
	// Returns the types and amounts of ammo used by the current weapon. If the
	// weapon only uses one type of ammo, it is always returned as ammo1.
	//
	//============================================================================

	Ammo, Ammo, int, int GetCurrentAmmo () const
	{
		Ammo ammo1, ammo2;
		if (CPlayer.ReadyWeapon != NULL)
		{
			ammo1 = CPlayer.ReadyWeapon.Ammo1;
			ammo2 = CPlayer.ReadyWeapon.Ammo2;
			if (ammo1 == NULL)
			{
				ammo1 = ammo2;
				ammo2 = NULL;
			}
		}
		else
		{
			ammo1 = ammo2 = NULL;
		}
		let ammocount1 = ammo1 != NULL ? ammo1.Amount : 0;
		let ammocount2 = ammo2 != NULL ? ammo2.Amount : 0;
		return ammo1, ammo2, ammocount1, ammocount2;
	}

	//============================================================================
	//
	// Get an icon
	//
	//============================================================================

	TextureID, Vector2 GetIcon(Inventory item, int flags, bool showdepleted = true)
	{
		TextureID icon;
		Vector2 scale = (1,1);
		icon.SetInvalid();
		if (item != null)
		{
			bool applyscale;
			[icon, applyscale] = GetInventoryIcon(item, flags);
			
			if (item.Amount == 0 && !showdepleted) return icon, scale;
			
			if (applyscale)
				scale = item.Scale;
		}
		return icon, scale;
	}
	
	//============================================================================
	//
	// Convenience functions to retrieve item tags
	//
	//============================================================================

	String GetAmmoTag(bool secondary = false)
	{
		let w = CPlayer.ReadyWeapon;
		if (w == null) return "";
		let ammo = secondary? w.ammo2 : w.ammo1;
		return ammo.GetTag();
	}	

	String GetWeaponTag()
	{
		let w = CPlayer.ReadyWeapon;
		if (w == null) return "";
		return w.GetTag();
	}	

	String GetSelectedInventoryTag()
	{
		let w = CPlayer.mo.InvSel;
		if (w == null) return "";
		return w.GetTag();
	}	

	// These cannot be done in ZScript.
	native static String GetGlobalACSString(int index);
	native static String GetGlobalACSArrayString(int arrayno, int index);
	native static int GetGlobalACSValue(int index);
	native static int GetGlobalACSArrayValue(int arrayno, int index);
	
	//============================================================================
	//
	// Convenience functions to retrieve some numbers
	//
	//============================================================================
	
	int GetArmorAmount()
	{
		let armor = CPlayer.mo.FindInventory("BasicArmor");
		return armor? armor.Amount : 0;
	}
	
	int, int GetAmount(class<Inventory> item)
	{
		let it = CPlayer.mo.FindInventory(item);
		int ret1 = it? it.Amount : 0;
		int ret2 = it? it.MaxAmount : GetDefaultByType(item).MaxAmount;
		return ret1, ret2;
	}

	int GetMaxAmount(class<Inventory> item)
	{
		let it = CPlayer.mo.FindInventory(item);
		return it? it.MaxAmount : GetDefaultByType(item).MaxAmount;
	}

	int GetArmorSavePercent()
	{
		double add = 0;
		let harmor = HexenArmor(CPlayer.mo.FindInventory("HexenArmor"));
		if(harmor != NULL)
		{
			add = harmor.Slots[0] + harmor.Slots[1] + harmor.Slots[2] + harmor.Slots[3] + harmor.Slots[4];
		}
		//Hexen counts basic armor also so we should too.
		let armor = BasicArmor(CPlayer.mo.FindInventory("BasicArmor"));
		if(armor != NULL && armor.Amount > 0)
		{
			add += armor.SavePercent * 100;
		}
		return int(add);
	}
	
	// Note that this retrieves the value in tics, not seconds like the equivalent SBARINFO function.
	// The idea is to let the caller decide what to do with it instead of destroying accuracy here.
	int GetAirTime()
	{
		if(CPlayer.mo.waterlevel < 3)
			return Level.airsupply;
		else
			return max(CPlayer.air_finished - Level.maptime, 0);
	}
	
	int GetSelectedInventoryAmount()
	{
		if(CPlayer.mo.InvSel != NULL) return CPlayer.mo.InvSel.Amount;
		return 0;
	}
	
	int GetKeyCount()
	{
		int num = 0;
		for(Inventory item = CPlayer.mo.Inv;item != NULL;item = item.Inv)
		{
			if(item is "Key") num++;
		}
		return num;
	}
	
	//============================================================================
	//
	// various checker functions, based on SBARINFOs condition nodes.
	//
	//============================================================================

	//============================================================================
	//
	// checks ammo use of current weapon
	//
	//============================================================================

	bool WeaponUsesAmmo(int ValidModes)
	{
		if (CPlayer == null) return false;
		let w = CPlayer.ReadyWeapon;
		if (w == null) return false;
		bool usesammo1 = w.AmmoType1 != NULL;
		bool usesammo2 = w.AmmoType2 != NULL;
		
		if (ValidModes == AMMO_PRIMARY) return usesammo1;
		if (ValidModes == AMMO_SECONDARY) return usesammo2;
		if (ValidModes == AMMO_ANY) return (usesammo1 || usesammo2);
		if (ValidModes == AMMO_BOTH) return (usesammo1 && usesammo2);
		return false;
	}

	//============================================================================
	//
	// checks if inventory bar is visible
	//
	//============================================================================

	bool isInventoryBarVisible()
	{
		if (CPlayer == null) return false;
		return (CPlayer.inventorytics > 0 && !Level.NoInventoryBar);
	}
	
	//============================================================================
	//
	// checks if aspect ratio is in a given range
	//
	//============================================================================
	
	bool CheckAspectRatio(double min, double max)
	{
		if (CPlayer == null) return false;
		double aspect = screen.GetAspectRatio();
		return (aspect >= min && aspect < max);
	}

	//============================================================================
	//
	// checks if weapon is selected.
	//
	//============================================================================

	bool CheckWeaponSelected(class<Weapon> weap, bool checksister = true)
	{
		if (CPlayer == null) return false;
		let w = CPlayer.ReadyWeapon;
		if (w == null) return false;
		if (w.GetClass() == weap) return true;
		if (checksister && w.SisterWeapon != null && w.SisterWeapon.GetClass() == weap) return true;
		return false;
	}

	//============================================================================
	//
	// checks if player has the given display name
	//
	//============================================================================

	bool CheckDisplayName(String displayname)
	{
		if (CPlayer == null) return false;
		return displayname == PlayerPawn.GetPrintableDisplayName(CPlayer.mo.GetClass());
	}

	//============================================================================
	//
	// checks if player has the given weapon piece
	//
	//============================================================================

	int CheckWeaponPiece(class<Weapon> weap, int piecenum)
	{
		if (CPlayer == null) return false;
		for(let inv = CPlayer.mo.Inv; inv != NULL; inv = inv.Inv)
		{
			let wh = WeaponHolder(inv);
			if (wh != null && wh.PieceWeapon == weap)
			{
				return (!!(wh.PieceMask & (1 << (PieceNum-1))));
			}
		}
		return false;
	}

	//============================================================================
	//
	// checks if player has the given weapon piece
	//
	//============================================================================

	int GetWeaponPieceMask(class<Weapon> weap)
	{
		if (CPlayer == null) return false;
		for(let inv = CPlayer.mo.Inv; inv != NULL; inv = inv.Inv)
		{
			let wh = WeaponHolder(inv);
			if (wh != null && wh.PieceWeapon == weap)
			{
				return wh.PieceMask;
			}
		}
		return 0;
	}

	//============================================================================
	//
	// checks if player has the given weapon piece
	//
	//============================================================================

	bool WeaponUsesAmmoType(class<Ammo> ammotype)
	{
		if (CPlayer == null) return false;
		let w = CPlayer.ReadyWeapon;
		if (w == NULL) return false;
		return w.AmmoType1 == ammotype || w.AmmoType2 == ammotype;
	}

	//============================================================================
	//
	// checks if player has the required health
	//
	//============================================================================

	bool CheckHealth(int Amount, bool percentage = false)
	{
		if (CPlayer == null) return false;
		
		int phealth = percentage ? CPlayer.mo.health * 100 / CPlayer.mo.GetMaxHealth() : CPlayer.mo.health;
		return (phealth >= Amount);
	}
	
	//============================================================================
	//
	// checks if player is invulnerable
	//
	//============================================================================

	bool isInvulnerable()
	{
		if (CPlayer == null) return false;
		return ((CPlayer.mo.bInvulnerable) || (CPlayer.cheats & (CF_GODMODE | CF_GODMODE2)));
	}

	//============================================================================
	//
	// checks if player owns enough of the item
	//
	//============================================================================

	bool CheckInventory(class<Inventory> item, int amount = 1)
	{
		if (CPlayer == null) return false;

		let it = CPlayer.mo.FindInventory(item);
		return it != null && it.Amount >= amount;
	}

	//============================================================================
	//
	// mypos cheat
	//
	//============================================================================

	virtual void DrawMyPos()
	{
		int height = SmallFont.GetHeight();
		int i;

		int vwidth;
		int vheight;
		int xpos;
		int y;
		let scalevec = GetHUDScale();
		int scale = int(scalevec.X);

		vwidth = screen.GetWidth() / scale;
		vheight = screen.GetHeight() / scale;
		xpos = vwidth - SmallFont.StringWidth("X: -00000")-6;
		y = GetTopOfStatusBar() / (3*scale) - height;

		Vector3 pos = CPlayer.mo.Pos;
		
		for (i = 0; i < 3; y += height, ++i)
		{
			double v = i == 0? pos.X : i == 1? pos.Y : pos.Z;
			String line = String.Format("%c: %d", int("X") + i, v);
			screen.DrawText (SmallFont, Font.CR_GREEN, xpos, y, line, DTA_KeepRatio, true,
				DTA_VirtualWidth, vwidth, DTA_VirtualHeight, vheight);
		}
	}
	
	//============================================================================
	//
	// automap HUD common drawer
	// This is not called directly to give a status bar the opportunity to
	// change the text colors. If you want to do something different,
	// override DrawAutomapHUD directly.
	//
	//============================================================================

	protected native void DoDrawAutomapHUD(int crdefault, int highlight);
	
	virtual void DrawAutomapHUD(double ticFrac)
	{
		// game needs to be checked here, so that SBARINFO also gets the correct colors.
		DoDrawAutomapHUD(Font.CR_GREY, gameinfo.gametype & GAME_DoomChex? Font.CR_UNTRANSLATED : Font.CR_YELLOW);
	}
	
	//---------------------------------------------------------------------------
	//
	// DrawPowerups
	//
	//---------------------------------------------------------------------------

	virtual void DrawPowerups ()
	{
		// The AltHUD specific adjustments have been removed here, because the AltHUD uses its own variant of this function
		// that can obey AltHUD rules - which this cannot.
		Vector2 pos = (-20, POWERUPICONSIZE * 5 / 4);
		double maxpos = screen.GetWidth() / 2;
		for (let iitem = CPlayer.mo.Inv; iitem != NULL; iitem = iitem.Inv)
		{
			let item = Powerup(iitem);
			if (item != null)
			{
				let icon = item.GetPowerupIcon();
				if (icon.IsValid() && !item.IsBlinking())
				{
					// Each icon gets a 32x32 block.
					DrawTexture(icon, pos, DI_SCREEN_RIGHT_TOP, 1.0, (POWERUPICONSIZE, POWERUPICONSIZE));
					pos.x -= POWERUPICONSIZE;
					if (pos.x < -maxpos)
					{
						pos.x = -20;
						pos.y += POWERUPICONSIZE * 3 / 2;
					}
				}
			}
		}
	}
	
	//============================================================================
	//
	// draw stuff
	//
	//============================================================================
	
	int GetTranslation() const
	{
		if(gameinfo.gametype & GAME_Raven)
			return Translation.MakeID(TRANSLATION_PlayersExtra, CPlayer.mo.PlayerNumber());
		else
			return Translation.MakeID(TRANSLATION_Players, CPlayer.mo.PlayerNumber());
	}
	
	//============================================================================
	//
	//
	//
	//============================================================================

	void DrawHexenArmor(int armortype, String image, Vector2 pos, int flags = 0, double alpha = 1.0, Vector2 boxsize = (-1, -1), Vector2 scale = (1.,1.))
	{
		let harmor = HexenArmor(statusBar.CPlayer.mo.FindInventory("HexenArmor"));
		if (harmor != NULL)
		{
			let slotval = harmor.Slots[armorType];
			let slotincr = harmor.SlotsIncrement[armorType];
			
			if (slotval > 0 && slotincr > 0)
			{
				//combine the alpha values
				alpha *= MIN(1., slotval / slotincr);
			}
			else return;
		}
		DrawImage(image, pos, flags, alpha, boxsize, scale);
	}
	
	//============================================================================
	//
	//
	//
	//============================================================================

	void DrawInventoryIcon(Inventory item, Vector2 pos, int flags = 0, double alpha = 1.0, Vector2 boxsize = (-1, -1), Vector2 scale = (1.,1.))
	{
		static const String flashimgs[]= { "USEARTID", "USEARTIC", "USEARTIB", "USEARTIA" };
		TextureID texture;
		Vector2 applyscale;
		[texture, applyscale] = GetIcon(item, flags, false);
		
		if((flags & DI_ARTIFLASH) && artiflashTick > 0)
		{
			DrawImage(flashimgs[artiflashTick-1], pos, flags, alpha, boxsize);
		}
		else if (texture.IsValid())
		{
			if ((flags & DI_DIMDEPLETED) && item.Amount <= 0) flags |= DI_DIM;
			applyscale.X *= scale.X;
			applyscale.Y *= scale.Y;
			DrawTexture(texture, pos, flags, alpha, boxsize, applyscale);
		}
	}
	
	//============================================================================
	//
	//
	//
	//============================================================================
	
	void DrawShader(int which, Vector2 pos, Vector2 size, int flags = 0, double alpha = 1.)
	{
		static const String texnames[] = {"BarShaderHF", "BarShaderHR", "BarShaderVF", "BarShaderVR" };
		DrawImage(texnames[which], pos, DI_ITEM_LEFT_TOP|DI_ALPHAMAPPED|DI_FORCEFILL | (flags & ~(DI_ITEM_HMASK|DI_ITEM_VMASK)), alpha, size);
	}
	
	//============================================================================
	//
	//
	//
	//============================================================================
	
	Vector2, int AdjustPosition(Vector2 position, int flags, double width, double height)
	{
		// This must be done here, before altered coordinates get sent to the draw functions.
		if (!(flags & DI_SCREEN_MANUAL_ALIGN))
		{
			if (position.x < 0) flags |= DI_SCREEN_RIGHT;
			else flags |= DI_SCREEN_LEFT;
			if (position.y < 0) flags |= DI_SCREEN_BOTTOM;
			else flags |= DI_SCREEN_TOP;
		}

		// placement by offset is not supported because the inventory bar is a composite.
		switch (flags & DI_ITEM_HMASK)
		{
		case DI_ITEM_HCENTER:	position.x -= width / 2; break;
		case DI_ITEM_RIGHT:		position.x -= width; break;
		}

		switch (flags & DI_ITEM_VMASK)
		{
		case DI_ITEM_VCENTER: position.y -= height / 2; break;
		case DI_ITEM_BOTTOM:  position.y -= height; break;
		}
		
		// clear all alignment flags so that the following code only passes on the rest
		flags &= ~(DI_ITEM_VMASK|DI_ITEM_HMASK);
		return position, flags;
	}	
	
	//============================================================================
	//
	// note that this does not implement chain wiggling, this is because
	// it would severely complicate the parameter list. The calling code is 
	// normally in a better position to do the needed calculations anyway.
	//
	//============================================================================
	
	void DrawGem(String chain, String gem, int displayvalue, int maxrange, Vector2 pos, int leftpadding, int rightpadding, int chainmod, int flags = 0)
	{
		TextureID chaintex = TexMan.CheckForTexture(chain, TexMan.TYPE_MiscPatch);
		if (!chaintex.IsValid()) return;
		Vector2 chainsize = TexMan.GetScaledSize(chaintex);
		[pos, flags] = AdjustPosition(pos, flags, chainsize.X, chainsize.Y);

		displayvalue = clamp(displayvalue, 0, maxrange);
		int offset = int(double(chainsize.X - leftpadding - rightpadding) * displayvalue / maxrange);
	
		DrawTexture(chaintex, pos + (offset % chainmod, 0), flags | DI_ITEM_LEFT_TOP);
		DrawImage(gem, pos + (offset + leftPadding, 0), flags | DI_ITEM_LEFT_TOP);
	}
	
	//============================================================================
	//
	// DrawBar
	//
	//============================================================================

	void DrawBar(String ongfx, String offgfx, double curval, double maxval, Vector2 position, int border, int vertical, int flags = 0, double alpha = 1.0)
	{
		let ontex = TexMan.CheckForTexture(ongfx, TexMan.TYPE_MiscPatch);
		if (!ontex.IsValid()) return;
		let offtex = TexMan.CheckForTexture(offgfx, TexMan.TYPE_MiscPatch);

		Vector2 texsize = TexMan.GetScaledSize(ontex);
		[position, flags] = AdjustPosition(position, flags, texsize.X, texsize.Y);
		
		double value = (maxval != 0) ? clamp(curval / maxval, 0, 1) : 0;
		if(border != 0) value = 1. - value; //invert since the new drawing method requires drawing the bg on the fg.
		
		
		// {cx, cb, cr, cy}
		double Clip[4];
		Clip[0] = Clip[1] = Clip[2] = Clip[3] = 0;
		
		bool horizontal = !(vertical & SHADER_VERT);
		bool reverse = !!(vertical & SHADER_REVERSE);
		double sizeOfImage = (horizontal ? texsize.X - border*2 : texsize.Y - border*2);
		Clip[(!horizontal) | ((!reverse)<<1)] = sizeOfImage - sizeOfImage *value;
		
		// preserve the active clipping rectangle
		int cx, cy, cw, ch;
		[cx, cy, cw, ch] = screen.GetClipRect();

		if(border != 0)
		{
			for(int i = 0; i < 4; i++) Clip[i] += border;

			//Draw the whole foreground
			DrawTexture(ontex, position, flags | DI_ITEM_LEFT_TOP, alpha);
			SetClipRect(position.X + Clip[0], position.Y + Clip[1], texsize.X - Clip[0] - Clip[2], texsize.Y - Clip[1] - Clip[3], flags);
		}
		
		if (offtex.IsValid() && TexMan.GetScaledSize(offtex) == texsize) DrawTexture(offtex, position, flags | DI_ITEM_LEFT_TOP, alpha);
		else Fill(color(int(255*alpha),0,0,0), position.X + Clip[0], position.Y + Clip[1], texsize.X - Clip[0] - Clip[2], texsize.Y - Clip[1] - Clip[3]);
		
		if (border == 0)
		{
			SetClipRect(position.X + Clip[0], position.Y + Clip[1], texsize.X - Clip[0] - Clip[2], texsize.Y - Clip[1] - Clip[3], flags);
			DrawTexture(ontex, position, flags | DI_ITEM_LEFT_TOP, alpha);
		}
		// restore the previous clipping rectangle
		screen.SetClipRect(cx, cy, cw, ch);
	}
	
	//============================================================================
	//
	// DrawInventoryBar
	//
	// This function needs too many parameters, so most have been offloaded to
	// a struct to keep code readable and allow initialization somewhere outside
	// the actual drawing code.
	//
	//============================================================================
	
	// Except for the placement information this gets all info from the struct that gets passed in.
	void DrawInventoryBar(InventoryBarState parms, Vector2 position, int numfields, int flags = 0, double bgalpha = 1.)
	{
		double width = parms.boxsize.X * numfields;
		[position, flags] = AdjustPosition(position, flags, width, parms.boxsize.Y);
		
		CPlayer.mo.InvFirst = ValidateInvFirst(numfields);
		if (CPlayer.mo.InvFirst == null) return;	// Player has no listed inventory items.
		
		Vector2 boxsize = parms.boxsize;
		// First draw all the boxes
		for(int i = 0; i < numfields; i++)
		{
			DrawTexture(parms.box, position + (boxsize.X * i, 0), flags | DI_ITEM_LEFT_TOP, bgalpha);
		}
		
		// now the items and the rest
		
		Vector2 itempos = position + boxsize / 2;
		Vector2 textpos = position + boxsize - (1, 1 + parms.amountfont.mFont.GetHeight());

		int i = 0;
		Inventory item;
		for(item = CPlayer.mo.InvFirst; item != NULL && i < numfields; item = item.NextInv())
		{
			for(int j = 0; j < 2; j++)
			{
				if (j ^ !!(flags & DI_DRAWCURSORFIRST))
				{
					if (item == CPlayer.mo.InvSel)
					{
						double flashAlpha = bgalpha;
						if (flags & DI_ARTIFLASH) flashAlpha *= itemflashFade;
						DrawTexture(parms.selector, position + parms.selectofs + (boxsize.X * i, 0), flags | DI_ITEM_LEFT_TOP, flashAlpha);
					}
				}
				else
				{
					DrawInventoryIcon(item, itempos + (boxsize.X * i, 0), flags | DI_ITEM_CENTER | DI_DIMDEPLETED);
				}
			}
			
			if (parms.amountfont != null && (item.Amount > 1 || (flags & DI_ALWAYSSHOWCOUNTERS)))
			{
				DrawString(parms.amountfont, FormatNumber(item.Amount, 0, 5), textpos + (boxsize.X * i, 0), flags | DI_TEXT_ALIGN_RIGHT, parms.cr, parms.itemalpha);
			}
			i++;
		}
		// Is there something to the left?
		if (CPlayer.mo.FirstInv() != CPlayer.mo.InvFirst)
		{
			DrawTexture(parms.left, position + (-parms.arrowoffset.X, parms.arrowoffset.Y), flags | DI_ITEM_RIGHT|DI_ITEM_VCENTER);
		}
		// Is there something to the right?
		if (item != NULL)
		{
			DrawTexture(parms.right, position + parms.arrowoffset + (width, 0), flags | DI_ITEM_LEFT|DI_ITEM_VCENTER);
		}
	}
}



struct UserCmd native
{
	native uint	buttons;
	native int16	pitch;			// up/down
	native int16	yaw;			// left/right
	native int16	roll;			// "tilt"
	native int16	forwardmove;
	native int16	sidemove;
	native int16	upmove;
}

class PlayerPawn : Actor
{
	const CROUCHSPEED = (1./12);
	// [RH] # of ticks to complete a turn180
	const TURN180_TICKS = ((TICRATE / 4) + 1);
	// 16 pixels of bob
	const MAXBOB = 16.;
	
	int			crouchsprite;
	int			MaxHealth;
	int			BonusHealth;
	int			MugShotMaxHealth;
	int			RunHealth;
	private int	PlayerFlags;
	clearscope Inventory	InvFirst;		// first inventory item displayed on inventory bar
	clearscope Inventory	InvSel;			// selected inventory item
	Name 		SoundClass;		// Sound class
	Name 		Portrait;
	Name 		Slot[10];
	double 		HexenArmor[5];

	// [GRB] Player class properties
	double		JumpZ;
	double		GruntSpeed;
	double		FallingScreamMinSpeed, FallingScreamMaxSpeed;
	double		ViewHeight;
	double		ForwardMove1, ForwardMove2;
	double		SideMove1, SideMove2;
	TextureID	ScoreIcon;
	int			SpawnMask;
	Name			MorphWeapon;
	double		AttackZOffset;			// attack height, relative to player center
	double		UseRange;				// [NS] Distance at which player can +use
	double		AirCapacity;			// Multiplier for air supply underwater.
	Class<Inventory> FlechetteType;
	color 		DamageFade;				// [CW] Fades for when you are being damaged.
	double		ViewBob;				// [SP] ViewBob Multiplier
	double		FullHeight;
	double		curBob;
	double		prevBob;

	meta Name HealingRadiusType;
	meta Name InvulMode;
	meta Name Face;
	meta int TeleportFreezeTime;
	meta int ColorRangeStart;	// Skin color range
	meta int ColorRangeEnd;
	
	property prefix: Player;
	property HealRadiusType: HealingradiusType;
	property InvulnerabilityMode: InvulMode;
	property AttackZOffset: AttackZOffset;
	property JumpZ: JumpZ;
	property GruntSpeed: GruntSpeed;
	property FallingScreamSpeed: FallingScreamMinSpeed, FallingScreamMaxSpeed;
	property ViewHeight: ViewHeight;
	property UseRange: UseRange;
	property AirCapacity: AirCapacity;
	property MaxHealth: MaxHealth;
	property MugshotMaxHealth: MugshotMaxHealth;
	property RunHealth: RunHealth;
	property MorphWeapon: MorphWeapon;
	property FlechetteType: FlechetteType;
	property Portrait: Portrait;
	property TeleportFreezeTime: TeleportFreezeTime;
	property ViewBob: ViewBob;
	
	flagdef NoThrustWhenInvul: PlayerFlags, 0;
	flagdef CanSuperMorph: PlayerFlags, 1;
	flagdef CrouchableMorph: PlayerFlags, 2;
	flagdef WeaponLevel2Ended: PlayerFlags, 3;
	
	Default
	{
		Health 100;
		Radius 16;
		Height 56;
		Mass 100;
		Painchance 255;
		Speed 1;
		+SOLID
		+SHOOTABLE
		+DROPOFF
		+PICKUP
		+NOTDMATCH
		+FRIENDLY
		+SLIDESONWALLS
		+CANPASS
		+CANPUSHWALLS
		+FLOORCLIP
		+WINDTHRUST
		+TELESTOMP
		+NOBLOCKMONST
		Player.AttackZOffset 8;
		Player.JumpZ 8;
		Player.GruntSpeed 12;
		Player.FallingScreamSpeed 35,40;
		Player.ViewHeight 41;
		Player.UseRange 64;
		Player.ForwardMove 1,1;
		Player.SideMove 1,1;
		Player.ColorRange 0,0;
		Player.SoundClass "player";
		Player.DamageScreenColor "ff 00 00";
		Player.MugShotMaxHealth 0;
		Player.FlechetteType "ArtiPoisonBag3";
		Player.AirCapacity 1;
		Player.ViewBob 1;
		Player.TeleportFreezeTime 18;
		Obituary "$OB_MPDEFAULT";
	}
	
	
	//===========================================================================
	//
	// PlayerPawn :: Tick
	//
	//===========================================================================

	override void Tick()
	{
		if (player != NULL && player.mo == self && CanCrouch() && player.playerstate != PST_DEAD)
		{
			Height = FullHeight * player.crouchfactor;
		}
		else
		{
			if (health > 0) Height = FullHeight;
		}

		if (bWeaponLevel2Ended)
		{
			bWeaponLevel2Ended = false;
			if (player.ReadyWeapon != NULL && player.ReadyWeapon.bPowered_Up)
			{
				player.ReadyWeapon.EndPowerup ();
			}
			if (player.PendingWeapon != NULL && player.PendingWeapon != WP_NOCHANGE &&
				player.PendingWeapon.bPowered_Up &&
				player.PendingWeapon.SisterWeapon != NULL)
			{
				player.PendingWeapon = player.PendingWeapon.SisterWeapon;
			}
		}
		Super.Tick();
	}

	//===========================================================================
	//
	// 
	//
	//===========================================================================

	override void BeginPlay()
	{
		Super.BeginPlay ();
		ChangeStatNum (STAT_PLAYER);
		FullHeight = Height;
		if (!SetupCrouchSprite(crouchsprite)) crouchsprite = 0;
	}
	
	//===========================================================================
	//
	// PlayerPawn :: PostBeginPlay
	//
	//===========================================================================

	override void PostBeginPlay()
	{
		Super.PostBeginPlay();
		WeaponSlots.SetupWeaponSlots(self);

		// Voodoo dolls: restore original floorz/ceilingz logic
		if (player == NULL || player.mo != self)
		{
			FindFloorCeiling(FFCF_ONLYSPAWNPOS|FFCF_NOPORTALS);
			SetZ(floorz);
			FindFloorCeiling(FFCF_ONLYSPAWNPOS);
		}
		else
		{
			player.SendPitchLimits();
		}
	}

	
	//===========================================================================
	//
	// PlayerPawn :: MarkPrecacheSounds
	//
	//===========================================================================

	override void MarkPrecacheSounds()
	{
		Super.MarkPrecacheSounds();
		MarkPlayerSounds();
	}
	
	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	virtual void PlayIdle ()
	{
		if (InStateSequence(CurState, SeeState))
			SetState (SpawnState);
	}

	virtual void PlayRunning ()
	{
		if (InStateSequence(CurState, SpawnState) && SeeState != NULL)
			SetState (SeeState);
	}
	
	virtual void PlayAttacking ()
	{
		if (MissileState != null) SetState (MissileState);
	}

	virtual void PlayAttacking2 ()
	{
		if (MeleeState != null) SetState (MeleeState);
	}
	
	virtual void MorphPlayerThink()
	{
	}
	
	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	virtual void OnRespawn()
	{
		if (sv_respawnprotect && (deathmatch || alwaysapplydmflags))
		{
			let invul = Powerup(Spawn("PowerInvulnerable"));
			invul.EffectTics = 3 * TICRATE;
			invul.BlendColor = 0;			// don't mess with the view
			invul.bUndroppable = true;		// Don't drop self
			bRespawnInvul = true;			// [RH] special effect
		}
	}
	
	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	override String GetObituary(Actor victim, Actor inflictor, Name mod, bool playerattack)
	{
		if (victim.player != player && victim.IsTeammate(self))
		{
			victim = self;
			return String.Format("$OB_FRIENDLY%d", random[Obituary](1, 4));
		}
		else
		{
			if (mod == 'Telefrag') return "$OB_MPTELEFRAG";

			String message;
			if (inflictor != NULL && inflictor != self)
			{
				message = inflictor.GetObituary(victim, inflictor, mod, playerattack);
			}
			if (message.Length() == 0 && playerattack && player.ReadyWeapon != NULL)
			{
				message = player.ReadyWeapon.GetObituary(victim, inflictor, mod, playerattack);
			}
			if (message.Length() == 0)
			{
				if (mod == 'BFGSplash') return "$OB_MPBFG_SPLASH";
				if (mod == 'Railgun') return "$OB_RAILGUN";
				message = Obituary;
			}
			return message;
		}
	}
	
	//----------------------------------------------------------------------------
	//
	// This is for SBARINFO.
	//
	//----------------------------------------------------------------------------

	clearscope int, int GetEffectTicsForItem(class<Inventory> item) const 
	{
		let pg = (class<PowerupGiver>)(item);
		if (pg != null)
		{
			let powerupType = (class<Powerup>)(GetDefaultByType(pg).PowerupType);
			let powerup = Powerup(FindInventory(powerupType));
			if(powerup != null) 
			{
				let maxtics = GetDefaultByType(pg).EffectTics;
				if (maxtics == 0) maxtics = powerup.default.EffectTics;
				return powerup.EffectTics, maxtics;
			}
		}
		return -1, -1;
	}
	

	//===========================================================================
	//
	// PlayerPawn :: CheckWeaponSwitch
	//
	// Checks if weapons should be changed after picking up ammo
	//
	//===========================================================================

	void CheckWeaponSwitch(Class<Ammo> ammotype)
	{
		let player = self.player;
		if (!player.GetNeverSwitch() &&	player.PendingWeapon == WP_NOCHANGE && 
			(player.ReadyWeapon == NULL || player.ReadyWeapon.bWimpy_Weapon))
		{
			let best = BestWeapon (ammotype);
			if (best != NULL && !best.bNoAutoSwitchTo && 
				(player.ReadyWeapon == NULL || best.SelectionOrder < player.ReadyWeapon.SelectionOrder))
			{
				player.PendingWeapon = best;
			}
		}
	}

	//---------------------------------------------------------------------------
	//
	// PROC P_FireWeapon
	//
	//---------------------------------------------------------------------------

	virtual void FireWeapon (State stat)
	{
		let player = self.player;
		
		let weapn = player.ReadyWeapon;
		if (weapn == null || !weapn.CheckAmmo (Weapon.PrimaryFire, true))
		{
			return;
		}

		player.WeaponState &= ~WF_WEAPONBOBBING;
		PlayAttacking ();
		weapn.bAltFire = false;
		if (stat == null)
		{
			stat = weapn.GetAtkState(!!player.refire);
		}
		player.SetPsprite(PSP_WEAPON, stat);
		if (!weapn.bNoAlert)
		{
			SoundAlert (self, false);
		}
	}

	//---------------------------------------------------------------------------
	//
	// PROC P_FireWeaponAlt
	//
	//---------------------------------------------------------------------------

	virtual void FireWeaponAlt (State stat)
	{
		let weapn = player.ReadyWeapon;
		if (weapn == null || weapn.FindState('AltFire') == null || !weapn.CheckAmmo (Weapon.AltFire, true))
		{
			return;
		}

		player.WeaponState &= ~WF_WEAPONBOBBING;
		PlayAttacking ();
		weapn.bAltFire = true;

		if (stat == null)
		{
			stat = weapn.GetAltAtkState(!!player.refire);
		}

		player.SetPsprite(PSP_WEAPON, stat);
		if (!weapn.bNoAlert)
		{
			SoundAlert (self, false);
		}
	}

	//---------------------------------------------------------------------------
	//
	// PROC P_CheckWeaponFire
	//
	// The player can fire the weapon.
	// [RH] This was in A_WeaponReady before, but that only works well when the
	// weapon's ready frames have a one tic delay.
	//
	//---------------------------------------------------------------------------

	void CheckWeaponFire ()
	{
		let player = self.player;
		let weapon = player.ReadyWeapon;

		if (weapon == NULL)
			return;

		// Check for fire. Some weapons do not auto fire.
		if ((player.WeaponState & WF_WEAPONREADY) && (player.cmd.buttons & BT_ATTACK))
		{
			if (!player.attackdown || !weapon.bNoAutofire)
			{
				player.attackdown = true;
				FireWeapon (NULL);
				return;
			}
		}
		else if ((player.WeaponState & WF_WEAPONREADYALT) && (player.cmd.buttons & BT_ALTATTACK))
		{
			if (!player.attackdown || !weapon.bNoAutofire)
			{
				player.attackdown = true;
				FireWeaponAlt (NULL);
				return;
			}
		}
		else
		{
			player.attackdown = false;
		}
	}

	//---------------------------------------------------------------------------
	//
	// PROC P_CheckWeaponChange
	//
	// The player can change to another weapon at self time.
	// [GZ] This was cut from P_CheckWeaponFire.
	//
	//---------------------------------------------------------------------------

	virtual void CheckWeaponChange ()
	{
		let player = self.player;
		if ((player.WeaponState & WF_DISABLESWITCH) || // Weapon changing has been disabled.
			player.morphTics != 0)					// Morphed classes cannot change weapons.
		{ // ...so throw away any pending weapon requests.
			player.PendingWeapon = WP_NOCHANGE;
		}

		// Put the weapon away if the player has a pending weapon or has died, and
		// we're at a place in the state sequence where dropping the weapon is okay.
		if ((player.PendingWeapon != WP_NOCHANGE || player.health <= 0) &&
			player.WeaponState & WF_WEAPONSWITCHOK)
		{
			DropWeapon();
		}
	}
	
	//------------------------------------------------------------------------
	//
	// PROC P_MovePsprites
	//
	// Called every tic by player thinking routine
	//
	//------------------------------------------------------------------------

	virtual void TickPSprites()
	{
		let player = self.player;
		let pspr = player.psprites;
		while (pspr)
		{
			// Destroy the psprite if it's from a weapon that isn't currently selected by the player
			// or if it's from an inventory item that the player no longer owns. 
			if ((pspr.Caller == null ||
				(pspr.Caller is "Inventory" && Inventory(pspr.Caller).Owner != pspr.Owner.mo) ||
				(pspr.Caller is "Weapon" && pspr.Caller != pspr.Owner.ReadyWeapon)))
			{
				pspr.Destroy();
			}
			else
			{
				pspr.Tick();
			}

			pspr = pspr.Next;
		}

		if ((health > 0) || (player.ReadyWeapon != null && !player.ReadyWeapon.bNoDeathInput))
		{
			if (player.ReadyWeapon == null)
			{
				if (player.PendingWeapon != WP_NOCHANGE)
					player.mo.BringUpWeapon();
			}
			else
			{
				CheckWeaponChange();
				if (player.WeaponState & (WF_WEAPONREADY | WF_WEAPONREADYALT))
				{
					CheckWeaponFire();
				}
				// Check custom buttons
				CheckWeaponButtons();
			}
		}
	}
	
	/*
	==================
	=
	= P_CalcHeight
	=
	=
	Calculate the walking / running height adjustment
	=
	==================
	*/

	virtual void CalcHeight()
	{
		let player = self.player;
		double angle;
		double bob;
		bool still = false;

		// Regular movement bobbing
		// (needs to be calculated for gun swing even if not on ground)

		// killough 10/98: Make bobbing depend only on player-applied motion.
		//
		// Note: don't reduce bobbing here if on ice: if you reduce bobbing here,
		// it causes bobbing jerkiness when the player moves from ice to non-ice,
		// and vice-versa.

		if (player.cheats & CF_NOCLIP2)
		{
			player.bob = 0;
		}
		else if (bNoGravity && !player.onground)
		{
			player.bob = 0.5;
		}
		else
		{
			player.bob = player.Vel dot player.Vel;
			if (player.bob == 0)
			{
				still = true;
			}
			else
			{
				player.bob *= player.GetMoveBob();

				if (player.bob > MAXBOB)
					player.bob = MAXBOB;
			}
		}

		double defaultviewheight = ViewHeight + player.crouchviewdelta;

		if (player.cheats & CF_NOVELOCITY)
		{
			player.viewz = pos.Z + defaultviewheight;

			if (player.viewz > ceilingz-4)
				player.viewz = ceilingz-4;

			return;
		}

		if (still)
		{
			if (player.health > 0)
			{
				angle = Level.maptime / (120 * TICRATE / 35.) * 360.;
				bob = player.GetStillBob() * sin(angle);
			}
			else
			{
				bob = 0;
			}
		}
		else
		{
			angle = Level.maptime / (20 * TICRATE / 35.) * 360.;
			bob = player.bob * sin(angle) * (waterlevel > 1 ? 0.25f : 0.5f);
		}

		// move viewheight
		if (player.playerstate == PST_LIVE)
		{
			player.viewheight += player.deltaviewheight;

			if (player.viewheight > defaultviewheight)
			{
				player.viewheight = defaultviewheight;
				player.deltaviewheight = 0;
			}
			else if (player.viewheight < (defaultviewheight/2))
			{
				player.viewheight = defaultviewheight/2;
				if (player.deltaviewheight <= 0)
					player.deltaviewheight = 1 / 65536.;
			}
			
			if (player.deltaviewheight)	
			{
				player.deltaviewheight += 0.25;
				if (!player.deltaviewheight)
					player.deltaviewheight = 1/65536.;
			}
		}

		if (player.morphTics)
		{
			bob = 0;
		}
		player.viewz = pos.Z + player.viewheight + (bob * clamp(ViewBob, 0. , 1.5)); // [SP] Allow DECORATE changes to view bobbing speed.
		if (Floorclip && player.playerstate != PST_DEAD
			&& pos.Z <= floorz)
		{
			player.viewz -= Floorclip;
		}
		if (player.viewz > ceilingz - 4)
		{
			player.viewz = ceilingz - 4;
		}
		if (player.viewz < floorz + 4)
		{
			player.viewz = floorz + 4;
		}
	}


	//==========================================================================
	//
	// P_DeathThink
	//
	//==========================================================================

	virtual void DeathThink ()
	{
		let player = self.player;
		int dir;
		double delta;

		player.Uncrouch();
		TickPSprites();

		player.onground = (pos.Z <= floorz);
		if (self is "PlayerChunk")
		{ // Flying bloody skull or flying ice chunk
			player.viewheight = 6;
			player.deltaviewheight = 0;
			if (player.onground)
			{
				if (Pitch > -19.)
				{
					double lookDelta = (-19. - Pitch) / 8;
					Pitch += lookDelta;
				}
			}
		}
		else if (!bIceCorpse)
		{ // Fall to ground (if not frozen)
			player.deltaviewheight = 0;
			if (player.viewheight > 6)
			{
				player.viewheight -= 1;
			}
			if (player.viewheight < 6)
			{
				player.viewheight = 6;
			}
			if (Pitch < 0)
			{
				Pitch += 3;
			}
			else if (Pitch > 0)
			{
				Pitch -= 3;
			}
			if (abs(Pitch) < 3)
			{
				Pitch = 0.;
			}
		}
		player.mo.CalcHeight ();
			
		if (player.attacker && player.attacker != self)
		{ // Watch killer
			double diff = deltaangle(angle, AngleTo(player.attacker));
			double delta = abs(diff);
	
			if (delta < 10)
			{ // Looking at killer, so fade damage and poison counters
				if (player.damagecount)
				{
					player.damagecount--;
				}
				if (player.poisoncount)
				{
					player.poisoncount--;
				}
			}
			delta /= 8;
			Angle += clamp(diff, -5., 5.);
		}
		else
		{
			if (player.damagecount)
			{
				player.damagecount--;
			}
			if (player.poisoncount)
			{
				player.poisoncount--;
			}
		}		

		if ((player.cmd.buttons & BT_USE ||
			((deathmatch || alwaysapplydmflags) && sv_forcerespawn)) && !sv_norespawn)
		{
			if (Level.maptime >= player.respawn_time || ((player.cmd.buttons & BT_USE) && player.Bot == NULL))
			{
				player.cls = NULL;		// Force a new class if the player is using a random class
				player.playerstate = (multiplayer || level.AllowRespawn || sv_singleplayerrespawn || G_SkillPropertyInt(SKILLP_PlayerRespawn)) ? PST_REBORN : PST_ENTER;
				if (special1 > 2)
				{
					special1 = 0;
				}
			}
		}
	}

	//===========================================================================
	//
	// PlayerPawn :: Die
	//
	//===========================================================================

	override void Die (Actor source, Actor inflictor, int dmgflags, Name MeansOfDeath)
	{
		Super.Die (source, inflictor, dmgflags, MeansOfDeath);

		if (player != NULL && player.mo == self) player.bonuscount = 0;

		if (player != NULL && player.mo != self)
		{ // Make the real player die, too
			player.mo.Die (source, inflictor, dmgflags, MeansOfDeath);
		}
		else
		{
			if (player != NULL && sv_weapondrop)
			{ // Voodoo dolls don't drop weapons
				let weap = player.ReadyWeapon;
				if (weap != NULL)
				{
					// kgDROP - start - modified copy from a_action.cpp
					let di = weap.GetDropItems();

					if (di != NULL)
					{
						while (di != NULL)
						{
							if (di.Name != 'None')
							{
								class<Actor> ti = di.Name;
								if (ti) A_DropItem (ti, di.Amount, di.Probability);
							}
							di = di.Next;
						}
					} 
					else if (weap.SpawnState != NULL &&
						weap.SpawnState != GetDefaultByType('Actor').SpawnState)
					{
						let weapitem = Weapon(A_DropItem (weap.GetClass(), -1, 256));
						if (weapitem)
						{
							if (weap.AmmoGive1 && weap.Ammo1)
							{
								weapitem.AmmoGive1 = weap.Ammo1.Amount;
							}
							if (weap.AmmoGive2 && weap.Ammo2)
							{
								weapitem.AmmoGive2 = weap.Ammo2.Amount;
							}
							weapitem.bIgnoreSkill = true;
						}
					}
					else
					{
						let item = Inventory(A_DropItem (weap.AmmoType1, -1, 256));
						if (item != NULL)
						{
							item.Amount = weap.Ammo1.Amount;
							item.bIgnoreSkill = true;
						}
						item = Inventory(A_DropItem (weap.AmmoType2, -1, 256));
						if (item != NULL)
						{
							item.Amount = weap.Ammo2.Amount;
							item.bIgnoreSkill = true;
						}
					}
				}
			}
			if (!multiplayer && level.deathsequence != 'None')
			{
				level.StartIntermission(level.deathsequence, FSTATE_EndingGame);
			}
		}
	}
	
	//===========================================================================
	//
	// PlayerPawn :: FilterCoopRespawnInventory
	//
	// When respawning in coop, this function is called to walk through the dead
	// player's inventory and modify it according to the current game flags so
	// that it can be transferred to the new live player. This player currently
	// has the default inventory, and the oldplayer has the inventory at the time
	// of death.
	//
	//===========================================================================

	void FilterCoopRespawnInventory (PlayerPawn oldplayer)
	{
		// If we're losing everything, this is really simple.
		if (sv_cooploseinventory)
		{
			oldplayer.DestroyAllInventory();
			return;
		}

		// Walk through the old player's inventory and destroy or modify
		// according to dmflags.
		Inventory next;
		for (Inventory item = oldplayer.Inv; item != NULL; item = next)
		{
			next = item.Inv;

			// If this item is part of the default inventory, we never want
			// to destroy it, although we might want to copy the default
			// inventory amount.
			let defitem = FindInventory (item.GetClass());

			if (sv_cooplosekeys && defitem == NULL && item is 'Key')
			{
				item.Destroy();
			}
			else if (sv_cooploseweapons && defitem == NULL && item is 'Weapon')
			{
				item.Destroy();
			}
			else if (sv_cooplosearmor && item is 'Armor')
			{
				if (defitem == NULL)
				{
					item.Destroy();
				}
				else if (item is 'BasicArmor')
				{
					BasicArmor(item).SavePercent = BasicArmor(defitem).SavePercent;
					item.Amount = defitem.Amount;
				}
				else if (item is 'HexenArmor')
				{
					let to = HexenArmor(item);
					let from = HexenArmor(defitem);
					to.Slots[0] = from.Slots[0];
					to.Slots[1] = from.Slots[1];
					to.Slots[2] = from.Slots[2];
					to.Slots[3] = from.Slots[3];
				}
			}
			else if (sv_cooplosepowerups &&	defitem == NULL && item is  'Powerup')
			{
				item.Destroy();
			}
			else if ((sv_cooploseammo || sv_coophalveammo) && item is 'Ammo')
			{
				if (defitem == NULL)
				{
					if (sv_cooploseammo)
					{
						// Do NOT destroy the ammo, because a weapon might reference it.
						item.Amount = 0;
					}
					else if (item.Amount > 1)
					{
						item.Amount /= 2;
					}
				}
				else
				{
					// When set to lose ammo, you get to keep all your starting ammo.
					// When set to halve ammo, you won't be left with less than your starting amount.
					if (sv_cooploseammo)
					{
						item.Amount = defitem.Amount;
					}
					else if (item.Amount > 1)
					{
						item.Amount = MAX(item.Amount / 2, defitem.Amount);
					}
				}
			}
		}

		// Now destroy the default inventory this player is holding and move
		// over the old player's remaining inventory.
		DestroyAllInventory();
		ObtainInventory (oldplayer);

		player.ReadyWeapon = NULL;
		PickNewWeapon (NULL);
	}

	
	//----------------------------------------------------------------------------
	//
	// PROC P_CheckFOV
	//
	//----------------------------------------------------------------------------

	virtual void CheckFOV()
	{
		let player = self.player;

		if (!player) return;

		// [RH] Zoom the player's FOV
		float desired = player.DesiredFOV;
		// Adjust FOV using on the currently held weapon.
		if (player.playerstate != PST_DEAD &&		// No adjustment while dead.
			player.ReadyWeapon != NULL &&			// No adjustment if no weapon.
			player.ReadyWeapon.FOVScale != 0)		// No adjustment if the adjustment is zero.
		{
			// A negative scale is used to prevent G_AddViewAngle/G_AddViewPitch
			// from scaling with the FOV scale.
			desired *= abs(player.ReadyWeapon.FOVScale);
		}
		if (player.FOV != desired)
		{
			if (abs(player.FOV - desired) < 7.)
			{
				player.FOV = desired;
			}
			else
			{
				float zoom = MAX(7., abs(player.FOV - desired) * 0.025);
				if (player.FOV > desired)
				{
					player.FOV = player.FOV - zoom;
				}
				else
				{
					player.FOV = player.FOV + zoom;
				}
			}
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_CheckCheats
	//
	//----------------------------------------------------------------------------

	virtual void CheckCheats()
	{
		let player = self.player;
		// No-clip cheat
		if ((player.cheats & (CF_NOCLIP | CF_NOCLIP2)) == CF_NOCLIP2)
		{ // No noclip2 without noclip
			player.cheats &= ~CF_NOCLIP2;
		}
		bNoClip = (player.cheats & (CF_NOCLIP | CF_NOCLIP2) || Default.bNoClip);
		if (player.cheats & CF_NOCLIP2)
		{
			bNoGravity = true;
		}
		else if (!bFly && !Default.bNoGravity)
		{
			bNoGravity = false;
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_CheckFrozen
	//
	//----------------------------------------------------------------------------

	virtual bool CheckFrozen()
	{
		let player = self.player;
		UserCmd cmd = player.cmd;
		bool totallyfrozen = player.IsTotallyFrozen();

		// [RH] Being totally frozen zeros out most input parameters.
		if (totallyfrozen)
		{
			if (gamestate == GS_TITLELEVEL)
			{
				cmd.buttons = 0;
			}
			else
			{
				cmd.buttons &= BT_USE;
			}
			cmd.pitch = 0;
			cmd.yaw = 0;
			cmd.roll = 0;
			cmd.forwardmove = 0;
			cmd.sidemove = 0;
			cmd.upmove = 0;
			player.turnticks = 0;
		}
		else if (player.cheats & CF_FROZEN)
		{
			cmd.forwardmove = 0;
			cmd.sidemove = 0;
			cmd.upmove = 0;
		}
		return totallyfrozen;
	}

	virtual bool CanCrouch() const
	{
		return player.morphTics == 0 || bCrouchableMorph;
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_CrouchMove
	//
	//----------------------------------------------------------------------------

	virtual void CrouchMove(int direction)
	{
		let player = self.player;
		
		double defaultheight = FullHeight;
		double savedheight = Height;
		double crouchspeed = direction * CROUCHSPEED;
		double oldheight = player.viewheight;

		player.crouchdir = direction;
		player.crouchfactor += crouchspeed;

		// check whether the move is ok
		Height  = defaultheight * player.crouchfactor;
		if (!TryMove(Pos.XY, false, NULL))
		{
			Height = savedheight;
			if (direction > 0)
			{
				// doesn't fit
				player.crouchfactor -= crouchspeed;
				return;
			}
		}
		Height = savedheight;

		player.crouchfactor = clamp(player.crouchfactor, 0.5, 1.);
		player.viewheight = ViewHeight * player.crouchfactor;
		player.crouchviewdelta = player.viewheight - ViewHeight;

		// Check for eyes going above/below fake floor due to crouching motion.
		CheckFakeFloorTriggers(pos.Z + oldheight, true);
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_CheckCrouch
	//
	//----------------------------------------------------------------------------

	virtual void CheckCrouch(bool totallyfrozen)
	{
		let player = self.player;
		UserCmd cmd = player.cmd;

		if (cmd.buttons & BT_JUMP)
		{
			cmd.buttons &= ~BT_CROUCH;
		}
		if (CanCrouch() && player.health > 0 && level.IsCrouchingAllowed())
		{
			if (!totallyfrozen)
			{
				int crouchdir = player.crouching;

				if (crouchdir == 0)
				{
					crouchdir = (cmd.buttons & BT_CROUCH) ? -1 : 1;
				}
				else if (cmd.buttons & BT_CROUCH)
				{
					player.crouching = 0;
				}
				if (crouchdir == 1 && player.crouchfactor < 1 && pos.Z + height < ceilingz)
				{
					CrouchMove(1);
				}
				else if (crouchdir == -1 && player.crouchfactor > 0.5)
				{
					CrouchMove(-1);
				}
			}
		}
		else
		{
			player.Uncrouch();
		}

		player.crouchoffset = -(ViewHeight) * (1 - player.crouchfactor);
	}

	//----------------------------------------------------------------------------
	//
	// P_Thrust
	//
	// moves the given origin along a given angle
	//
	//----------------------------------------------------------------------------

	void ForwardThrust (double move, double angle)
	{
		if ((waterlevel || bNoGravity) && Pitch != 0 && !player.GetClassicFlight())
		{
			double zpush = move * sin(Pitch);
			if (waterlevel && waterlevel < 2 && zpush < 0) zpush = 0;
			Vel.Z -= zpush;
			move *= cos(Pitch);
		}
		Thrust(move, angle);
	}

	//----------------------------------------------------------------------------
	//
	// P_Bob
	// Same as P_Thrust, but only affects bobbing.
	//
	// killough 10/98: We apply thrust separately between the real physical player
	// and the part which affects bobbing. This way, bobbing only comes from player
	// motion, nothing external, avoiding many problems, e.g. bobbing should not
	// occur on conveyors, unless the player walks on one, and bobbing should be
	// reduced at a regular rate, even on ice (where the player coasts).
	//
	//----------------------------------------------------------------------------

	void Bob (double angle, double move, bool forward)
	{
		if (forward && (waterlevel || bNoGravity) && Pitch != 0)
		{
			move *= cos(Pitch);
		}
		player.Vel += AngleToVector(angle, move);
	}
	
	//===========================================================================
	//
	// PlayerPawn :: TweakSpeeds
	//
	//===========================================================================

	virtual double, double TweakSpeeds (double forward, double side)
	{
		// Strife's player can't run when its health is below 10
		if (health <= RunHealth)
		{
			forward = clamp(forward, -gameinfo.normforwardmove[0]*256, gameinfo.normforwardmove[0]*256);
			side = clamp(side, -gameinfo.normsidemove[0]*256, gameinfo.normsidemove[0]*256);
		}

		// [GRB]
		if (abs(forward) < 0x3200)
		{
			forward *= ForwardMove1;
		}
		else
		{
			forward *= ForwardMove2;
		}

		if (abs(side) < 0x2800)
		{
			side *= SideMove1;
		}
		else
		{
			side *= SideMove2;
		}

		if (!player.morphTics)
		{
			double factor = 1.;
			for(let it = Inv; it != null; it = it.Inv)
			{
				factor *= it.GetSpeedFactor ();
			}
			forward *= factor;
			side *= factor;
		}
		return forward, side;
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_MovePlayer
	//
	//----------------------------------------------------------------------------

	virtual void MovePlayer ()
	{
		let player = self.player;
		UserCmd cmd = player.cmd;

		// [RH] 180-degree turn overrides all other yaws
		if (player.turnticks)
		{
			player.turnticks--;
			Angle += (180. / TURN180_TICKS);
		}
		else
		{
			Angle += cmd.yaw * (360./65536.);
		}

		player.onground = (pos.z <= floorz) || bOnMobj || bMBFBouncer || (player.cheats & CF_NOCLIP2);

		// killough 10/98:
		//
		// We must apply thrust to the player and bobbing separately, to avoid
		// anomalies. The thrust applied to bobbing is always the same strength on
		// ice, because the player still "works just as hard" to move, while the
		// thrust applied to the movement varies with 'movefactor'.

		if (cmd.forwardmove | cmd.sidemove)
		{
			double forwardmove, sidemove;
			double bobfactor;
			double friction, movefactor;
			double fm, sm;

			[friction, movefactor] = GetFriction();
			bobfactor = friction < ORIG_FRICTION ? movefactor : ORIG_FRICTION_FACTOR;
			if (!player.onground && !bNoGravity && !waterlevel)
			{
				// [RH] allow very limited movement if not on ground.
				movefactor *= level.aircontrol;
				bobfactor*= level.aircontrol;
			}

			fm = cmd.forwardmove;
			sm = cmd.sidemove;
			[fm, sm] = TweakSpeeds (fm, sm);
			fm *= Speed / 256;
			sm *= Speed / 256;

			// When crouching, speed and bobbing have to be reduced
			if (CanCrouch() && player.crouchfactor != 1)
			{
				fm *= player.crouchfactor;
				sm *= player.crouchfactor;
				bobfactor *= player.crouchfactor;
			}

			forwardmove = fm * movefactor * (35 / TICRATE);
			sidemove = sm * movefactor * (35 / TICRATE);

			if (forwardmove)
			{
				Bob(Angle, cmd.forwardmove * bobfactor / 256., true);
				ForwardThrust(forwardmove, Angle);
			}
			if (sidemove)
			{
				let a = Angle - 90;
				Bob(a, cmd.sidemove * bobfactor / 256., false);
				Thrust(sidemove, a);
			}

			if (!(player.cheats & CF_PREDICTING) && (forwardmove != 0 || sidemove != 0))
			{
				PlayRunning ();
			}

			if (player.cheats & CF_REVERTPLEASE)
			{
				player.cheats &= ~CF_REVERTPLEASE;
				player.camera = player.mo;
			}
		}
	}		

	//----------------------------------------------------------------------------
	//
	// PROC P_CheckPitch
	//
	//----------------------------------------------------------------------------

	virtual void CheckPitch()
	{
		let player = self.player;
		// [RH] Look up/down stuff
		if (!level.IsFreelookAllowed())
		{
			Pitch = 0.;
		}
		else
		{
			// The player's view pitch is clamped between -32 and +56 degrees,
			// which translates to about half a screen height up and (more than)
			// one full screen height down from straight ahead when view panning
			// is used.
			int clook = player.cmd.pitch;
			if (clook != 0)
			{
				if (clook == -32768)
				{ // center view
					player.centering = true;
				}
				else if (!player.centering)
				{
					// no more overflows with floating point. Yay! :)
					Pitch = clamp(Pitch - clook * (360. / 65536.), player.MinPitch, player.MaxPitch);
				}
			}
		}
		if (player.centering)
		{
			if (abs(Pitch) > 2.)
			{
				Pitch *= (2. / 3.);
			}
			else
			{
				Pitch = 0.;
				player.centering = false;
				if (PlayerNumber() == consoleplayer)
				{
					LocalViewPitch = 0;
				}
			}
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_CheckJump
	//
	//----------------------------------------------------------------------------

	virtual void CheckJump()
	{
		let player = self.player;
		// [RH] check for jump
		if (player.cmd.buttons & BT_JUMP)
		{
			if (player.crouchoffset != 0)
			{
				// Jumping while crouching will force an un-crouch but not jump
				player.crouching = 1;
			}
			else if (waterlevel >= 2)
			{
				Vel.Z = 4 * Speed;
			}
			else if (bNoGravity)
			{
				Vel.Z = 3.;
			}
			else if (level.IsJumpingAllowed() && player.onground && player.jumpTics == 0)
			{
				double jumpvelz = JumpZ * 35 / TICRATE;
				double jumpfac = 0;

				// [BC] If the player has the high jump power, double his jump velocity.
				// (actually, pick the best factors from all active items.)
				for (let p = Inv; p != null; p = p.Inv)
				{
					let pp = PowerHighJump(p);
					if (pp)
					{
						double f = pp.Strength;
						if (f > jumpfac) jumpfac = f;
					}
				}
				if (jumpfac > 0) jumpvelz *= jumpfac;

				Vel.Z += jumpvelz;
				bOnMobj = false;
				player.jumpTics = -1;
				if (!(player.cheats & CF_PREDICTING)) A_StartSound("*jump", CHAN_BODY);
			}
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_CheckMoveUpDown
	//
	//----------------------------------------------------------------------------

	virtual void CheckMoveUpDown()
	{
		let player = self.player;
		UserCmd cmd = player.cmd;

		if (cmd.upmove == -32768)
		{ // Only land if in the air
			if (bNoGravity && waterlevel < 2)
			{
				bNoGravity = false;
			}
		}
		else if (cmd.upmove != 0)
		{
			// Clamp the speed to some reasonable maximum.
			cmd.upmove = clamp(cmd.upmove, -0x300, 0x300);
			if (waterlevel >= 2 ||  bFly || (player.cheats & CF_NOCLIP2))
			{
				Vel.Z = Speed * cmd.upmove / 128.;
				if (waterlevel < 2 && !bNoGravity)
				{
					bFly = true;
					bNoGravity = true;
					if ((Vel.Z <= -39) && !(player.cheats & CF_PREDICTING))
					{ // Stop falling scream
						A_StopSound(CHAN_VOICE);
					}
				}
			}
			else if (cmd.upmove > 0 && !(player.cheats & CF_PREDICTING))
			{
				let fly = FindInventory("ArtiFly");
				if (fly != NULL)
				{
					UseInventory(fly);
				}
			}
		}
	}


	//----------------------------------------------------------------------------
	//
	// PROC P_HandleMovement
	//
	//----------------------------------------------------------------------------

	virtual void HandleMovement()
	{
		let player = self.player;
		// [RH] Check for fast turn around
		if (player.cmd.buttons & BT_TURN180 && !(player.oldbuttons & BT_TURN180))
		{
			player.turnticks = TURN180_TICKS;
		}

		// Handle movement
		if (reactiontime)
		{ // Player is frozen
			reactiontime--;
		}
		else
		{
			MovePlayer();
			CheckJump();
			CheckMoveUpDown();
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_CheckUndoMorph
	//
	//----------------------------------------------------------------------------

	virtual void CheckUndoMorph()
	{
		let player = self.player;
		// Morph counter
		if (player.morphTics)
		{
			if (player.chickenPeck)
			{ // Chicken attack counter
				player.chickenPeck -= 3;
			}
			if (!--player.morphTics)
			{ // Attempt to undo the chicken/pig
				player.mo.UndoPlayerMorph(player, MRF_UNDOBYTIMEOUT);
			}
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_CheckPoison
	//
	//----------------------------------------------------------------------------

	virtual void CheckPoison()
	{
		let player = self.player;
		if (player.poisoncount && !(Level.maptime & 15))
		{
			player.poisoncount -= 5;
			if (player.poisoncount < 0)
			{
				player.poisoncount = 0;
			}
			player.PoisonDamage(player.poisoner, 1, true);
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_CheckDegeneration
	//
	//----------------------------------------------------------------------------

	virtual void CheckDegeneration()
	{
		// Apply degeneration.
		if (sv_degeneration)
		{
			let player = self.player;
			int maxhealth = GetMaxHealth(true);
			if ((Level.maptime % TICRATE) == 0 && player.health > maxhealth)
			{
				if (player.health - 5 < maxhealth)
					player.health = maxhealth;
				else
					player.health--;

				health = player.health;
			}
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_CheckAirSupply
	//
	//----------------------------------------------------------------------------

	virtual void CheckAirSupply()
	{
		// Handle air supply
		//if (level.airsupply > 0)
		{
			let player = self.player;
			if (waterlevel < 3 || (bInvulnerable) || (player.cheats & (CF_GODMODE | CF_NOCLIP2)) ||	(player.cheats & CF_GODMODE2))
			{
				ResetAirSupply();
			}
			else if (player.air_finished <= Level.maptime && !(Level.maptime & 31))
			{
				DamageMobj(NULL, NULL, 2 + ((Level.maptime - player.air_finished) / TICRATE), 'Drowning');
			}
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC P_PlayerThink
	//
	//----------------------------------------------------------------------------
	
	virtual void PlayerThink()
	{
		let player = self.player;
		UserCmd cmd = player.cmd;
		
		CheckFOV();

		if (player.inventorytics)
		{
			player.inventorytics--;
		}
		CheckCheats();

		if (bJustAttacked)
		{ // Chainsaw/Gauntlets attack auto forward motion
			cmd.yaw = 0;
			cmd.forwardmove = 0xc800/2;
			cmd.sidemove = 0;
			bJustAttacked = false;
		}

		bool totallyfrozen = CheckFrozen();

		// Handle crouching
		CheckCrouch(totallyfrozen);
		CheckMusicChange();

		if (player.playerstate == PST_DEAD)
		{
			DeathThink ();
			return;
		}
		if (player.jumpTics != 0)
		{
			player.jumpTics--;
			if (player.onground && player.jumpTics < -18)
			{
				player.jumpTics = 0;
			}
		}
		if (player.morphTics && !(player.cheats & CF_PREDICTING))
		{
			MorphPlayerThink ();
		}

		CheckPitch();
		HandleMovement();
		CalcHeight ();

		if (!(player.cheats & CF_PREDICTING))
		{
			CheckEnvironment();
			// Note that after this point the PlayerPawn may have changed due to getting unmorphed or getting its skull popped so 'self' is no longer safe to use.
			// This also must not read mo into a local variable because several functions in this block can change the attached PlayerPawn.
			player.mo.CheckUse();
			player.mo.CheckUndoMorph();
			// Cycle psprites.
			player.mo.TickPSprites();
			// Other Counters
			if (player.damagecount)	player.damagecount--;
			if (player.bonuscount) player.bonuscount--;

			if (player.hazardcount)
			{
				player.hazardcount--;
				if (player.hazardinterval <= 0)
					player.hazardinterval = 32; // repair invalid hazardinterval
				if (!(Level.maptime % player.hazardinterval) && player.hazardcount > 16*TICRATE)
					player.mo.DamageMobj (NULL, NULL, 5, player.hazardtype);
			}
			player.mo.CheckPoison();
			player.mo.CheckDegeneration();
			player.mo.CheckAirSupply();
		}
	}

	//---------------------------------------------------------------------------
	//
	// PROC P_BringUpWeapon
	//
	// Starts bringing the pending weapon up from the bottom of the screen.
	// This is only called to start the rising, not throughout it.
	//
	//---------------------------------------------------------------------------

	void BringUpWeapon ()
	{
		let player = self.player;
		if (player.PendingWeapon == WP_NOCHANGE)
		{
			if (player.ReadyWeapon != null)
			{
				let psp = player.GetPSprite(PSP_WEAPON);
				if (psp) 
				{
					psp.y = WEAPONTOP;
					player.ReadyWeapon.ResetPSprite(psp);
				}
				player.SetPsprite(PSP_WEAPON, player.ReadyWeapon.GetReadyState());
			}
			return;
		}

		let weapon = player.PendingWeapon;

		// If the player has a tome of power, use self weapon's powered up
		// version, if one is available.
		if (weapon != null &&
			weapon.SisterWeapon &&
			weapon.SisterWeapon.bPowered_Up &&
			player.mo.FindInventory ('PowerWeaponLevel2', true))
		{
			weapon = weapon.SisterWeapon;
		}

		player.PendingWeapon = WP_NOCHANGE;
		player.ReadyWeapon = weapon;
		player.mo.weaponspecial = 0;

		if (weapon != null)
		{
			weapon.PlayUpSound(self);
			player.refire = 0;

			let psp = player.GetPSprite(PSP_WEAPON);
			if (psp) psp.y = player.cheats & CF_INSTANTWEAPSWITCH? WEAPONTOP : WEAPONBOTTOM;
			// make sure that the previous weapon's flash state is terminated.
			// When coming here from a weapon drop it may still be active.
			player.SetPsprite(PSP_FLASH, null);
			player.SetPsprite(PSP_WEAPON, weapon.GetUpState());
		}
	}
	
	//===========================================================================
	//
	// PlayerPawn :: BestWeapon
	//
	// Returns the best weapon a player has, possibly restricted to a single
	// type of ammo.
	//
	//===========================================================================

	Weapon BestWeapon(Class<Ammo> ammotype)
	{
		Weapon bestMatch = NULL;
		int bestOrder = int.max;
		Inventory item;
		bool tomed = !!FindInventory ('PowerWeaponLevel2', true);

		// Find the best weapon the player has.
		for (item = Inv; item != NULL; item = item.Inv)
		{
			let weap = Weapon(item);
			if (weap == null)
				continue;

			// Don't select it if it's worse than what was already found.
			if (weap.SelectionOrder > bestOrder)
				continue;

			// Don't select it if its primary fire doesn't use the desired ammo.
			if (ammotype != NULL &&
				(weap.Ammo1 == NULL ||
				 weap.Ammo1.GetClass() != ammotype))
				continue;

			// Don't select it if the Tome is active and self isn't the powered-up version.
			if (tomed && weap.SisterWeapon != NULL && weap.SisterWeapon.bPowered_Up)
				continue;

			// Don't select it if it's powered-up and the Tome is not active.
			if (!tomed && weap.bPowered_Up)
				continue;

			// Don't select it if there isn't enough ammo to use its primary fire.
			if (!(weap.bAMMO_OPTIONAL) &&
				!weap.CheckAmmo (Weapon.PrimaryFire, false))
				continue;

			// Don't select if if there isn't enough ammo as determined by the weapon's author.
			if (weap.MinSelAmmo1 > 0 && (weap.Ammo1 == NULL || weap.Ammo1.Amount < weap.MinSelAmmo1))
				continue;
			if (weap.MinSelAmmo2 > 0 && (weap.Ammo2 == NULL || weap.Ammo2.Amount < weap.MinSelAmmo2))
				continue;

			// This weapon is usable!
			bestOrder = weap.SelectionOrder;
			bestMatch = weap;
		}
		return bestMatch;
	}

	
	//---------------------------------------------------------------------------
	//
	// PROC P_DropWeapon
	//
	// The player died, so put the weapon away.
	//
	//---------------------------------------------------------------------------

	void DropWeapon ()
	{
		let player = self.player;
		if (player == null)
		{
			return;
		}
		// Since the weapon is dropping, stop blocking switching.
		player.WeaponState &= ~WF_DISABLESWITCH;
		Weapon weap = player.ReadyWeapon;
		if ((weap != null) && (player.health > 0 || !weap.bNoDeathDeselect))
		{
			player.SetPsprite(PSP_WEAPON, weap.GetDownState());
		}
	}
	
	//===========================================================================
	//
	// PlayerPawn :: PickNewWeapon
	//
	// Picks a new weapon for this player. Used mostly for running out of ammo,
	// but it also works when an ACS script explicitly takes the ready weapon
	// away or the player picks up some ammo they had previously run out of.
	//
	//===========================================================================

	Weapon PickNewWeapon(Class<Ammo> ammotype)
	{
		Weapon best = BestWeapon (ammotype);

		if (best != NULL)
		{
			player.PendingWeapon = best;
			if (player.ReadyWeapon != NULL)
			{
				DropWeapon();
			}
			else if (player.PendingWeapon != WP_NOCHANGE)
			{
				BringUpWeapon ();
			}
		}
		return best;
	}

	//===========================================================================
	//
	// PlayerPawn :: GiveDefaultInventory
	//
	//===========================================================================

	virtual void GiveDefaultInventory ()
	{
		let player = self.player;
		if (player == NULL) return;

		// HexenArmor must always be the first item in the inventory because
		// it provides player class based protection that should not affect
		// any other protection item.
		let myclass = GetClass();
		GiveInventoryType('HexenArmor');
		let harmor = HexenArmor(FindInventory('HexenArmor'));

		harmor.Slots[4] = self.HexenArmor[0];
		for (int i = 0; i < 4; ++i)
		{
			harmor.SlotsIncrement[i] = self.HexenArmor[i + 1];
		}

		// BasicArmor must come right after that. It should not affect any
		// other protection item as well but needs to process the damage
		// before the HexenArmor does.
		GiveInventoryType('BasicArmor');

		// Now add the items from the DECORATE definition
		let di = GetDropItems();

		while (di)
		{
			Class<Actor> ti = di.Name;
			if (ti)
			{
				let tinv = (class<Inventory>)(ti);
				if (!tinv)
				{
					Console.Printf(TEXTCOLOR_ORANGE .. "%s is not an inventory item and cannot be given to a player as start item.\n", di.Name);
				}
				else
				{
					let item = FindInventory(tinv);
					if (item != NULL)
					{
						item.Amount = clamp(
							item.Amount + (di.Amount ? di.Amount : item.default.Amount), 0, item.MaxAmount);
					}
					else
					{
						item = Inventory(Spawn(ti));
						item.bIgnoreSkill = true;	// no skill multipliers here
						item.Amount = di.Amount;
						let weap = Weapon(item);
						if (weap)
						{
							// To allow better control any weapon is emptied of
							// ammo before being given to the player.
							weap.AmmoGive1 = weap.AmmoGive2 = 0;
						}
						bool res;
						Actor check;
						[res, check] = item.CallTryPickup(self);
						if (!res)
						{
							item.Destroy();
							item = NULL;
						}
						else if (check != self)
						{
							// Player was morphed. This is illegal at game start.
							// This problem is only detectable when it's too late to do something about it...
							ThrowAbortException("Cannot give morph item '%s' when starting a game!", di.Name);
						}
					}
					let weap = Weapon(item);
					if (weap != NULL && weap.CheckAmmo(Weapon.EitherFire, false))
					{
						player.ReadyWeapon = player.PendingWeapon = weap;
					}
				}
			}
			di = di.Next;
		}
	}

	//===========================================================================
	//
	// PlayerPawn :: GiveDeathmatchInventory
	//
	// Gives players items they should have in addition to their default
	// inventory when playing deathmatch. (i.e. all keys)
	//
	//===========================================================================

	virtual void GiveDeathmatchInventory()
	{
		for ( int i = 0; i < AllActorClasses.Size(); ++i)
		{
			let cls = (class<Key>)(AllActorClasses[i]);
			if (cls)
			{
				let keyobj = GetDefaultByType(cls);
				if (keyobj.special1 != 0)
				{
					GiveInventoryType(cls);
				}
			}
		}
	}

	//===========================================================================
	//
	// 
	//
	//===========================================================================

	override int GetMaxHealth(bool withupgrades) const
	{
		int ret = MaxHealth > 0? MaxHealth : ((Level.compatflags & COMPATF_DEHHEALTH)? 100 : deh.MaxHealth);
		if (withupgrades) ret += stamina + BonusHealth;
		return ret;
	}
	
	//===========================================================================
	//
	// 
	//
	//===========================================================================

	virtual int GetTeleportFreezeTime()
	{
		if (TeleportFreezeTime <= 0) return 0;
		let item = inv;
		while (item != null)
		{
			if (item.GetNoTeleportFreeze()) return 0;
			item = item.inv;
		}
		return TeleportFreezeTime;
	}
	
	
	//===========================================================================
	//
	// G_PlayerFinishLevel
	// Called when a player completes a level.
	//
	// flags is checked for RESETINVENTORY and RESETHEALTH only.
	//
	//===========================================================================

	void PlayerFinishLevel (int mode, int flags)
	{
		Inventory item, next;
		let p = player;

		if (p.morphTics != 0)
		{ // Undo morph
			Unmorph(self, 0, true);
		}
		// 'self' will be no longer valid from here on in case of an unmorph
		let me = p.mo;

		// Strip all current powers, unless moving in a hub and the power is okay to keep.
		item = me.Inv;
		while (item != NULL)
		{
			next = item.Inv;
			if (item is 'Powerup')
			{
				if (deathmatch || ((mode != FINISH_SameHub || !item.bHUBPOWER) && !item.bPERSISTENTPOWER)) // Keep persistent powers in non-deathmatch games
				{
					item.Destroy ();
				}
			}
			item = next;
		}
		let ReadyWeapon = p.ReadyWeapon;
		if (ReadyWeapon != NULL && ReadyWeapon.bPOWERED_UP && p.PendingWeapon == ReadyWeapon.SisterWeapon)
		{
			// Unselect powered up weapons if the unpowered counterpart is pending
			p.ReadyWeapon = p.PendingWeapon;
		}
		// reset invisibility to default
		me.RestoreRenderStyle();
		p.extralight = 0;					// cancel gun flashes
		p.fixedcolormap = PlayerInfo.NOFIXEDCOLORMAP;	// cancel ir goggles
		p.fixedlightlevel = -1;
		p.damagecount = 0; 				// no palette changes
		p.bonuscount = 0;
		p.poisoncount = 0;
		p.inventorytics = 0;

		if (mode != FINISH_SameHub)
		{
			// Take away flight and keys (and anything else with IF_INTERHUBSTRIP set)
			item = me.Inv;
			while (item != NULL)
			{
				next = item.Inv;
				if (item.InterHubAmount < 1)
				{
					item.DepleteOrDestroy ();
				}
				item = next;
			}
		}

		if (mode == FINISH_NoHub && !level.KEEPFULLINVENTORY)
		{ // Reduce all owned (visible) inventory to defined maximum interhub amount
			Array<Inventory> todelete;
			for (item = me.Inv; item != NULL; item = item.Inv)
			{
				// If the player is carrying more samples of an item than allowed, reduce amount accordingly
				if (item.bINVBAR && item.Amount > item.InterHubAmount)
				{
					item.Amount = item.InterHubAmount;
					if (level.REMOVEITEMS && !item.bUNDROPPABLE && !item.bUNCLEARABLE)
					{
						todelete.Push(item);
					}
				}
			}
			for (int i = 0; i < toDelete.Size(); i++)
			{
				let it = toDelete[i];
				if (!it.bDestroyed)
				{
					item.DepleteOrDestroy();
				}
			}
		}

		// Resets player health to default if not dead.
		if ((flags & CHANGELEVEL_RESETHEALTH) && p.playerstate != PST_DEAD)
		{
			p.health = me.health = me.SpawnHealth();
		}

		// Clears the entire inventory and gives back the defaults for starting a game
		if ((flags & CHANGELEVEL_RESETINVENTORY) && p.playerstate != PST_DEAD)
		{
			me.ClearInventory();
			me.GiveDefaultInventory();
		}
	}
	
	//===========================================================================
	//
	// FWeaponSlot :: PickWeapon
	//
	// Picks a weapon from this slot. If no weapon is selected in this slot,
	// or the first weapon in this slot is selected, returns the last weapon.
	// Otherwise, returns the previous weapon in this slot. This means
	// precedence is given to the last weapon in the slot, which by convention
	// is probably the strongest. Does not return weapons you have no ammo
	// for or which you do not possess.
	//
	//===========================================================================

	virtual Weapon PickWeapon(int slot, bool checkammo)
	{
		int i, j;

		let player = self.player;
		int Size = player.weapons.SlotSize(slot);
		// Does this slot even have any weapons?
		if (Size == 0)
		{
			return player.ReadyWeapon;
		}
		let ReadyWeapon = player.ReadyWeapon;
		if (ReadyWeapon != null)
		{
			for (i = 0; i < Size; i++)
			{
				let weapontype = player.weapons.GetWeapon(slot, i);
				if (weapontype == ReadyWeapon.GetClass() ||
					(ReadyWeapon.bPOWERED_UP && ReadyWeapon.SisterWeapon != null && ReadyWeapon.SisterWeapon.GetClass() == weapontype))
				{
					for (j = (i == 0 ? Size - 1 : i - 1);
						j != i;
						j = (j == 0 ? Size - 1 : j - 1))
					{
						let weapontype2 = player.weapons.GetWeapon(slot, j);
						let weap = Weapon(player.mo.FindInventory(weapontype2));

						if (weap != null)
						{
							if (!checkammo || weap.CheckAmmo(Weapon.EitherFire, false))
							{
								return weap;
							}
						}
					}
				}
			}
		}
		for (i = Size - 1; i >= 0; i--)
		{
			let weapontype = player.weapons.GetWeapon(slot, i);
			let weap = Weapon(player.mo.FindInventory(weapontype));

			if (weap != null)
			{
				if (!checkammo || weap.CheckAmmo(Weapon.EitherFire, false))
				{
					return weap;
				}
			}
		}
		return ReadyWeapon;
	}

	//===========================================================================
	//
	// FindMostRecentWeapon
	//
	// Locates the slot and index for the most recently selected weapon. If the
	// player is in the process of switching to a new weapon, that is the most
	// recently selected weapon. Otherwise, the current weapon is the most recent
	// weapon.
	//
	//===========================================================================

	bool, int, int FindMostRecentWeapon()
	{
		let player = self.player;
		let ReadyWeapon = player.ReadyWeapon;
		if (player.PendingWeapon != WP_NOCHANGE)
		{
			// Workaround for the current inability 
			bool found;
			int slot;
			int index;
			[found, slot, index] = player.weapons.LocateWeapon(player.PendingWeapon.GetClass());
			return found, slot, index;
		}
		else if (ReadyWeapon != null)
		{
			bool found;
			int slot;
			int index;
			[found, slot, index] = player.weapons.LocateWeapon(ReadyWeapon.GetClass());
			if (!found)
			{
				// If the current weapon wasn't found and is powered up,
				// look for its non-powered up version.
				if (ReadyWeapon.bPOWERED_UP && ReadyWeapon.SisterWeaponType != null)
				{
					[found, slot, index] = player.weapons.LocateWeapon(ReadyWeapon.SisterWeaponType);
					return found, slot, index;
				}
				return false, 0, 0;
			}
			return true, slot, index;
		}
		else
		{
			return false, 0, 0;
		}
	}

	//===========================================================================
	//
	// FWeaponSlots :: PickNextWeapon
	//
	// Returns the "next" weapon for this player. If the current weapon is not
	// in a slot, then it just returns that weapon, since there's nothing to
	// consider it relative to.
	//
	//===========================================================================
	const NUM_WEAPON_SLOTS = 10;

	virtual Weapon PickNextWeapon()
	{
		let player = self.player;
		bool found;
		int startslot, startindex;
		int slotschecked = 0;

		[found, startslot, startindex] = FindMostRecentWeapon();
		let ReadyWeapon = player.ReadyWeapon;
		if (ReadyWeapon == null || found)
		{
			int slot;
			int index;

			if (ReadyWeapon == null)
			{
				startslot = NUM_WEAPON_SLOTS - 1;
				startindex = player.weapons.SlotSize(startslot) - 1;
			}

			slot = startslot;
			index = startindex;
			do
			{
				if (++index >= player.weapons.SlotSize(slot))
				{
					index = 0;
					slotschecked++;
					if (++slot >= NUM_WEAPON_SLOTS)
					{
						slot = 0;
					}
				}
				let type = player.weapons.GetWeapon(slot, index);
				let weap = Weapon(FindInventory(type));
				if (weap != null && weap.CheckAmmo(Weapon.EitherFire, false))
				{
					return weap;
				}
			} while ((slot != startslot || index != startindex) && slotschecked <= NUM_WEAPON_SLOTS);
		}
		return ReadyWeapon;
	}

	//===========================================================================
	//
	// FWeaponSlots :: PickPrevWeapon
	//
	// Returns the "previous" weapon for this player. If the current weapon is
	// not in a slot, then it just returns that weapon, since there's nothing to
	// consider it relative to.
	//
	//===========================================================================

	virtual Weapon PickPrevWeapon()
	{
		let player = self.player;
		int startslot, startindex;
		bool found;
		int slotschecked = 0;

		[found, startslot, startindex] = FindMostRecentWeapon();
		if (player.ReadyWeapon == null || found)
		{
			int slot;
			int index;

			if (player.ReadyWeapon == null)
			{
				startslot = 0;
				startindex = 0;
			}

			slot = startslot;
			index = startindex;
			do
			{
				if (--index < 0)
				{
					slotschecked++;
					if (--slot < 0)
					{
						slot = NUM_WEAPON_SLOTS - 1;
					}
					index = player.weapons.SlotSize(slot) - 1;
				}
				let type = player.weapons.GetWeapon(slot, index);
				let weap = Weapon(FindInventory(type));
				if (weap != null && weap.CheckAmmo(Weapon.EitherFire, false))
				{
					return weap;
				}
			} while ((slot != startslot || index != startindex) && slotschecked <= NUM_WEAPON_SLOTS);
		}
		return player.ReadyWeapon;
	}

	//============================================================================
	//
	// P_BobWeapon
	//
	// [RH] Moved this out of A_WeaponReady so that the weapon can bob every
	// tic and not just when A_WeaponReady is called. Not all weapons execute
	// A_WeaponReady every tic, and it looks bad if they don't bob smoothly.
	//
	// [XA] Added new bob styles and exposed bob properties. Thanks, Ryan Cordell!
	// [SP] Added new user option for bob speed
	//
	//============================================================================

	virtual Vector2 BobWeapon (double ticfrac)
	{
		Vector2 p1, p2, r;
		Vector2 result;

		let player = self.player;
		if (!player) return (0, 0);
		let weapon = player.ReadyWeapon;

		if (weapon == null || weapon.bDontBob)
		{
			return (0, 0);
		}

		// [XA] Get the current weapon's bob properties.
		int bobstyle = weapon.BobStyle;
		double BobSpeed = (weapon.BobSpeed * 128);
		double Rangex = weapon.BobRangeX;
		double Rangey = weapon.BobRangeY;

		for (int i = 0; i < 2; i++)
		{
			// Bob the weapon based on movement speed. ([SP] And user's bob speed setting)
			double angle = (BobSpeed * player.GetWBobSpeed() * 35 /	TICRATE*(Level.maptime - 1 + i)) * (360. / 8192.);

			// [RH] Smooth transitions between bobbing and not-bobbing frames.
			// This also fixes the bug where you can "stick" a weapon off-center by
			// shooting it when it's at the peak of its swing.
			if (curbob != player.bob)
			{
				if (abs(player.bob - curbob) <= 1)
				{
					curbob = player.bob;
				}
				else
				{
					double zoom = MAX(1., abs(curbob - player.bob) / 40);
					if (curbob > player.bob)
					{
						curbob -= zoom;
					}
					else
					{
						curbob += zoom;
					}
				}
			}

			// The weapon bobbing intensity while firing can be adjusted by the player.
			double BobIntensity = (player.WeaponState & WF_WEAPONBOBBING) ? 1. : player.GetWBobFire();

			if (curbob != 0)
			{
				double bobVal = player.bob;
				if (i == 0)
				{
					bobVal = prevBob;
				}
				//[SP] Added in decorate player.viewbob checks
				double bobx = (bobVal * BobIntensity * Rangex * ViewBob);
				double boby = (bobVal * BobIntensity * Rangey * ViewBob);
				switch (bobstyle)
				{
				case Bob_Normal:
					r.X = bobx * cos(angle);
					r.Y = boby * abs(sin(angle));
					break;

				case Bob_Inverse:
					r.X = bobx*cos(angle);
					r.Y = boby * (1. - abs(sin(angle)));
					break;

				case Bob_Alpha:
					r.X = bobx * sin(angle);
					r.Y = boby * abs(sin(angle));
					break;

				case Bob_InverseAlpha:
					r.X = bobx * sin(angle);
					r.Y = boby * (1. - abs(sin(angle)));
					break;

				case Bob_Smooth:
					r.X = bobx*cos(angle);
					r.Y = 0.5f * (boby * (1. - (cos(angle * 2))));
					break;

				case Bob_InverseSmooth:
					r.X = bobx*cos(angle);
					r.Y = 0.5f * (boby * (1. + (cos(angle * 2))));
				}
			}
			else
			{
				r = (0, 0);
			}
			if (i == 0) p1 = r; else p2 = r;
		}
		return p1 * (1. - ticfrac) + p2 * ticfrac;
	}
	
	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	virtual clearscope color GetPainFlash() const
	{
		Color painFlash = GetPainFlashForType(DamageTypeReceived);
		if (painFlash == 0) painFlash = DamageFade;
		return painFlash;
	}
		
	//===========================================================================
	//
	// PlayerPawn :: ResetAirSupply
	//
	// Gives the player a full "tank" of air. If they had previously completely
	// run out of air, also plays the *gasp sound. Returns true if the player
	// was drowning.
	//
	//===========================================================================

	virtual bool ResetAirSupply (bool playgasp = true)
	{
		let player = self.player;
		bool wasdrowning = (player.air_finished < Level.maptime);

		if (playgasp && wasdrowning)
		{
			A_StartSound("*gasp", CHAN_VOICE);
		}
		if (Level.airsupply > 0 && AirCapacity > 0) player.air_finished = Level.maptime + int(Level.airsupply * AirCapacity);
		else player.air_finished = int.max;
		return wasdrowning;
	}

	//===========================================================================
	//
	// PlayerPawn :: Travelled
	//
	// Called when the player moves to another map, in case it needs to do
	// special reinitialization. This is called after all carried items have
	// executed their respective Travelled() virtuals too.
	//
	//===========================================================================

	virtual void Travelled() {}

	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	native clearscope static String GetPrintableDisplayName(Class<Actor> cls);
	native void CheckMusicChange();
	native void CheckEnvironment();
	native void CheckUse();
	native void CheckWeaponButtons();
	native void MarkPlayerSounds();
	private native int SetupCrouchSprite(int c);
	private native clearscope Color GetPainFlashForType(Name type);
}

extend class Actor
{
	//----------------------------------------------------------------------------
	//
	// PROC A_SkullPop
	// This should really be in PlayerPawn but cannot be.
	//
	//----------------------------------------------------------------------------

	void A_SkullPop(class<PlayerChunk> skulltype = "BloodySkull")
	{
		// [GRB] Parameterized version
		if (skulltype == NULL || !(skulltype is "PlayerChunk"))
		{
			skulltype = "BloodySkull";
			if (skulltype == NULL)
				return;
		}

		bSolid = false;
		let mo = PlayerPawn(Spawn (skulltype,  Pos + (0, 0, 48), NO_REPLACE));
		//mo.target = self;
		mo.Vel.X = Random2[SkullPop]() / 128.;
		mo.Vel.Y = Random2[SkullPop]() / 128.;
		mo.Vel.Z = 2. + (Random[SkullPop]() / 1024.);
		// Attach player mobj to bloody skull
		let player = self.player;
		self.player = NULL;
		mo.ObtainInventory (self);
		mo.player = player;
		mo.health = health;
		mo.Angle = Angle;
		if (player != NULL)
		{
			player.mo = mo;
			player.damagecount = 32;
		}
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].camera == self)
			{
				players[i].camera = mo;
			}
		}
	}
}

class PlayerChunk : PlayerPawn
{
	Default
	{
		+NOSKIN
		-SOLID
		-SHOOTABLE
		-PICKUP
		-NOTDMATCH
		-FRIENDLY
		-SLIDESONWALLS
		-CANPUSHWALLS
		-FLOORCLIP
		-WINDTHRUST
		-TELESTOMP
	}
}

class PSprite : Object native play
{
	enum PSPLayers
	{
		STRIFEHANDS = -1,
		WEAPON = 1,
		FLASH = 1000,
		TARGETCENTER = 0x7fffffff - 2,
		TARGETLEFT,
		TARGETRIGHT,
	};

	native readonly State CurState; 
	native Actor Caller;
	native readonly PSprite Next;
	native readonly PlayerInfo Owner;
	native SpriteID Sprite;
	native int Frame;
	//native readonly int RenderStyle;	had to be blocked because the internal representation was not ok. Renderstyle is still pending a proper solution.
	native readonly int ID;
	native Bool processPending;
	native double x;
	native double y;
	native double oldx;
	native double oldy;
	native Vector2 pivot;
	native Vector2 scale;
	native double rotation;
	native int HAlign, VAlign;
	native Vector2 Coord0;		// [MC] Not the actual coordinates. Just the offsets by A_OverlayVertexOffset.
	native Vector2 Coord1;
	native Vector2 Coord2;
	native Vector2 Coord3;
	native double alpha;
	native Bool firstTic;
	native bool InterpolateTic;
	native int Tics;
	native uint Translation;
	native bool bAddWeapon;
	native bool bAddBob;
	native bool bPowDouble;
	native bool bCVarFast;
	native bool bFlip;	
	native bool bMirror;
	native bool bPlayerTranslated;
	native bool bPivotPercent;
	native bool bInterpolate;

	native void SetState(State newstate, bool pending = false);

	//------------------------------------------------------------------------
	//
	//
	//
	//------------------------------------------------------------------------

	void Tick()
	{
		if (processPending)
		{
			// drop tic count and possibly change state
			if (Tics != -1)	// a -1 tic count never changes
			{
				Tics--;
				// [BC] Apply double firing speed.
				if (bPowDouble && Tics && (Owner.mo.FindInventory ("PowerDoubleFiringSpeed", true))) Tics--;
				if (!Tics && Caller != null) SetState(CurState.NextState);
			}
		}
	}
	
	void ResetInterpolation() 
	{ 
		oldx = x; 
		oldy = y; 
	}
		
}

enum EPlayerState
{
	PST_LIVE,	// Playing or camping.
	PST_DEAD,	// Dead on the ground, view follows killer.
	PST_REBORN,	// Ready to restart/respawn???
	PST_ENTER,	// [BC] Entered the game
	PST_GONE	// Player has left the game
}

enum EPlayerGender
{
	GENDER_MALE,
	GENDER_FEMALE,
	GENDER_NEUTRAL,
	GENDER_OTHER
}

enum EPlayerDefaultPronouns
{
	PRONOUN_THEY,
	PRONOUN_SHE,
	PRONOUN_HE,
	PRONOUN_IT,
	PRONOUN_MAX
}

const PRONOUN_SET_SIZE = 6;

struct PlayerInfo native play	// self is what internally is known as player_t
{
	// technically engine constants but the only part of the playsim using them is the player.
	const NOFIXEDCOLORMAP = -1;
	const NUMCOLORMAPS = 32;
	
	native PlayerPawn mo;
	native uint8 playerstate;
	native readonly uint buttons;
	native uint original_oldbuttons;
	native Class<PlayerPawn> cls;
	native float DesiredFOV;
	native float FOV;
	native double viewz;
	native double viewheight;
	native double deltaviewheight;
	native double bob;
	native vector2 vel;
	native bool centering;
	native uint8 turnticks;
	native bool attackdown;
	native bool usedown;
	native uint oldbuttons;
	native int health;
	native clearscope int inventorytics;
	native uint8 CurrentPlayerClass;
	native int frags[MAXPLAYERS];
	native int fragcount;
	native int lastkilltime;
	native uint8 multicount;
	native uint8 spreecount;
	native uint16 WeaponState;
	native Weapon ReadyWeapon;
	native Weapon PendingWeapon;
	native PSprite psprites;
	native int cheats;
	native int timefreezer;
	native int16 refire;
	native int16 inconsistent;
	native bool waiting;
	native int killcount;
	native int itemcount;
	native int secretcount;
	native uint damagecount;
	native uint bonuscount;
	native int hazardcount;
	native int hazardinterval;
	native Name hazardtype;
	native int poisoncount;
	native Name poisontype;
	native Name poisonpaintype;
	native Actor poisoner;
	native Actor attacker;
	native int extralight;
	native int16 fixedcolormap;
	native int16 fixedlightlevel;
	native int morphtics;
	native Class<PlayerPawn>MorphedPlayerClass;
	native int MorphStyle;
	native Class<Actor> MorphExitFlash;
	native Weapon PremorphWeapon;
	native int chickenPeck;
	native int jumpTics;
	native bool onground;
	native int respawn_time;
	native Actor camera;
	native int air_finished;
	native Name LastDamageType;
	native Actor MUSINFOactor;
	native int8 MUSINFOtics;
	native bool settings_controller;
	native int8 crouching;
	native int8 crouchdir;
	native Bot bot;
	native float BlendR;
	native float BlendG;
	native float BlendB;
	native float BlendA;
	native String LogText;
	native double MinPitch;
	native double MaxPitch;
	native double crouchfactor;
	native double crouchoffset;
	native double crouchviewdelta;
	native Actor ConversationNPC;
	native Actor ConversationPC;
	native double ConversationNPCAngle;
	native bool ConversationFaceTalker;
	native @WeaponSlots weapons;
	native @UserCmd cmd;
	native readonly @UserCmd original_cmd;

	static const String DefaultPronouns[] =
	{
		"they", "them", "their", "theirs", "themself", "they're",
		"she",  "her",  "her",   "hers",   "herself",  "she's",
		"he",   "him",  "his",   "his",    "himself",  "he's",
		"it",   "it",   "its",   "its",    "itself",   "it's"
	};

	native bool PoisonPlayer(Actor poisoner, Actor source, int poison);
	native void PoisonDamage(Actor source, int damage, bool playPainSound);
	native void SetPsprite(int id, State stat, bool pending = false);
	native void SetSafeFlash(Weapon weap, State flashstate, int index);
	native PSprite GetPSprite(int id) const;
	native PSprite FindPSprite(int id) const;
	native void SetLogNumber (int text);
	native void SetLogText (String text);
	native void SetSubtitleNumber (int text, Sound sound_id = 0);
	native bool Resurrect();

	native clearscope String GetUserName() const;
	native clearscope Color GetColor() const;
	native clearscope Color GetDisplayColor() const;
	native clearscope int GetColorSet() const;
	native clearscope int GetPlayerClassNum() const;
	native clearscope int GetSkin() const;
	native clearscope bool GetNeverSwitch() const;
	native clearscope int GetGender() const;
	native clearscope String GetPronouns() const;
	native clearscope int GetTeam() const;
	native clearscope float GetAutoaim() const;
	native clearscope bool GetNoAutostartMap() const;
	native double GetWBobSpeed() const;
	native double GetWBobFire() const;
	native double GetMoveBob() const;
	native double GetStillBob() const;
	native void SetFOV(float fov);
	native clearscope bool GetClassicFlight() const;
	native void SendPitchLimits();
	native clearscope bool HasWeaponsInSlot(int slot) const;

	// The actual implementation is on PlayerPawn where it can be overridden. Use that directly in the future.
	deprecated("3.7", "MorphPlayer() should be used on a PlayerPawn object") bool MorphPlayer(playerinfo p, Class<PlayerPawn> spawntype, int duration, int style, Class<Actor> enter_flash = null, Class<Actor> exit_flash = null)
	{
		if (mo != null)
		{
			return mo.MorphPlayer(p, spawntype, duration, style, enter_flash, exit_flash);
		}
		return false;
	}
	
	// This somehow got its arguments mixed up. 'self' should have been the player to be unmorphed, not the activator
	deprecated("3.7", "UndoPlayerMorph() should be used on a PlayerPawn object") bool UndoPlayerMorph(playerinfo player, int unmorphflag = 0, bool force = false)
	{
		if (player.mo != null)
		{
 			return player.mo.UndoPlayerMorph(self, unmorphflag, force);
		}
		return false;
	}

	deprecated("3.7", "DropWeapon() should be used on a PlayerPawn object") void DropWeapon()
	{
		if (mo != null)
		{
			mo.DropWeapon();
		}
	}

	deprecated("3.7", "BringUpWeapon() should be used on a PlayerPawn object") void BringUpWeapon()
	{
		if (mo) mo.BringUpWeapon();
	}
	
	clearscope bool IsTotallyFrozen() const
	{
		return
			gamestate == GS_TITLELEVEL ||
			(cheats & CF_TOTALLYFROZEN) ||
			mo.isFrozen();
	}

	void Uncrouch()
	{
		if (crouchfactor != 1)
		{
			crouchfactor = 1;
			crouchoffset = 0;
			crouchdir = 0;
			crouching = 0;
			crouchviewdelta = 0;
			viewheight = mo.ViewHeight;
		}
	}
	

	clearscope int fragSum () const
	{
		int i;
		int allfrags = 0;
		int playernum = mo.PlayerNumber();
	
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i]
				&& i!=playernum)
			{
				allfrags += frags[i];
			}
		}
		
		// JDC hack - negative frags.
		allfrags -= frags[playernum];
		return allfrags;
	}
	
	double GetDeltaViewHeight()
	{
		return (mo.ViewHeight + crouchviewdelta - viewheight) / 8;
	}
	
}

struct PlayerClass native
{
	native class<Actor> Type;
	native uint Flags;
	native Array<int> Skins;
	
	native bool CheckSkin(int skin);
	native void EnumColorsets(out Array<int> data);
	native Name GetColorsetName(int setnum);
}

struct PlayerSkin native
{
	native readonly String		SkinName;
	native readonly String		Face;
	native readonly uint8		gender;
	native readonly uint8		range0start;
	native readonly uint8		range0end;
	native readonly bool		othergame;
	native readonly Vector2		Scale;
	native readonly int			sprite;
	native readonly int			crouchsprite;
	native readonly int			namespc;
};

struct Team native
{
	const NoTeam = 255;
	const Max = 16;
	native String mName;
}

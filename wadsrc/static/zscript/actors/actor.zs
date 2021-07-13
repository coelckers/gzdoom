struct FCheckPosition
{
	// in
	native Actor		thing;
	native Vector3		pos;

	// out
	native Sector		cursector;
	native double		floorz;
	native double		ceilingz;
	native double		dropoffz;
	native TextureID	floorpic;
	native int			floorterrain;
	native Sector		floorsector;
	native TextureID	ceilingpic;
	native Sector		ceilingsector;
	native bool			touchmidtex;
	native bool			abovemidtex;
	native bool			floatok;
	native bool			FromPMove;
	native line			ceilingline;
	native Actor		stepthing;
	native bool			DoRipping;
	native bool			portalstep;
	native int			portalgroup;

	native int			PushTime;
	
	// These are internal helpers to properly initialize an object of this type.
	private native void _Constructor();
	private native void _Destructor();
	
	native void ClearLastRipped();
}

struct FLineTraceData
{
	enum ETraceResult
	{
		TRACE_HitNone,
		TRACE_HitFloor,
		TRACE_HitCeiling,
		TRACE_HitWall,
		TRACE_HitActor,
		TRACE_CrossingPortal,
		TRACE_HasHitSky
	};

	native Actor HitActor;
	native Line HitLine;
	native Sector HitSector;
	native F3DFloor Hit3DFloor;
	native TextureID HitTexture;
	native Vector3 HitLocation;
	native Vector3 HitDir;
	native double Distance;
	native int NumPortals;
	native int LineSide;
	native int LinePart;
	native int SectorPlane;
	native int HitType;
}

struct LinkContext
{
	voidptr sector_list;	// really msecnode but that's not exported yet.
	voidptr render_list;
}


class Actor : Thinker native
{
	const DEFAULT_HEALTH = 1000;
	const ONFLOORZ = -2147483648.0;
	const ONCEILINGZ = 2147483647.0;
	const STEEPSLOPE = (46342./65536.);	// [RH] Minimum floorplane.c value for walking
	const FLOATRANDZ = ONCEILINGZ-1;
	const TELEFRAG_DAMAGE = 1000000;
	const MinVel = 1./65536;
	const LARGE_MASS = 10000000;	// not INT_MAX on purpose
	const ORIG_FRICTION = (0xE800/65536.);	// original value
	const ORIG_FRICTION_FACTOR = (2048/65536.);	// original value
	const DEFMORPHTICS = 40 * TICRATE;


	// flags are not defined here, the native fields for those get synthesized from the internal tables.
	
	// for some comments on these fields, see their native representations in actor.h.
	native readonly Actor snext;	// next in sector list.
	native PlayerInfo Player;
	native readonly vector3 Pos;
	native vector3 Prev;
	native uint ThruBits;
	native vector2 SpriteOffset;
	native double spriteAngle;
	native double spriteRotation;
	native float VisibleStartAngle;
	native float VisibleStartPitch;
	native float VisibleEndAngle;
	native float VisibleEndPitch;
	native double Angle;
	native double Pitch;
	native double Roll;
	native vector3 Vel;
	native double Speed;
	native double FloatSpeed;
	native SpriteID sprite;
	native uint8 frame;
	native vector2 Scale;
	native TextureID picnum;
	native double Alpha;
	native readonly color fillcolor;	// must be set with SetShade to initialize correctly.
	native Sector CurSector;
	native double CeilingZ;
	native double FloorZ;
	native double DropoffZ;
	native Sector floorsector;
	native TextureID floorpic;
	native int floorterrain;
	native Sector ceilingsector;
	native TextureID ceilingpic;
	native double Height;
	native readonly double Radius;
    native readonly double RenderRadius;
	native double projectilepassheight;
	native int tics;
	native readonly State CurState;
	native readonly int Damage;
	native int projectilekickback;
	native int special1;
	native int special2;
	native double specialf1;
	native double specialf2;
	native int weaponspecial;
	native int Health;
	native uint8 movedir;
	native int8 visdir;
	native int16 movecount;
	native int16 strafecount;
	native Actor Target;
	native Actor Master;
	native Actor Tracer;
	native Actor LastHeard;
	native Actor LastEnemy;
	native Actor LastLookActor;
	native int ReactionTime;
	native int Threshold;
	native readonly int DefThreshold;
	native vector3 SpawnPoint;
	native uint16 SpawnAngle;
	native int StartHealth;
	native uint8 WeaveIndexXY;
	native uint8 WeaveIndexZ;
	native uint16 skillrespawncount;
	native int Args[5];
	native int Mass;
	native int Special;
	native readonly int TID;
	native readonly int TIDtoHate;
	native readonly int WaterLevel;
	native int Score;
	native int Accuracy;
	native int Stamina;
	native double MeleeRange;
	native int PainThreshold;
	native double Gravity;
	native double FloorClip;
	native name DamageType;
	native name DamageTypeReceived;
	native uint8 FloatBobPhase;
	native double FloatBobStrength;
	native int RipperLevel;
	native int RipLevelMin;
	native int RipLevelMax;
	native name Species;
	native Actor Alternative;
	native Actor goal;
	native uint8 MinMissileChance;
	native int8 LastLookPlayerNumber;
	native uint SpawnFlags;
	native double meleethreshold;
	native double maxtargetrange;
	native double bouncefactor;
	native double wallbouncefactor;
	native int bouncecount;
	native double friction;
	native int FastChaseStrafeCount;
	native double pushfactor;
	native int lastpush;
	native int activationtype;
	native int lastbump;
	native int DesignatedTeam;
	native Actor BlockingMobj;
	native Line BlockingLine;
	native Sector Blocking3DFloor;
	native Sector BlockingCeiling;
	native Sector BlockingFloor;
	native int PoisonDamage;
	native name PoisonDamageType;
	native int PoisonDuration;
	native int PoisonPeriod;
	native int PoisonDamageReceived;
	native name PoisonDamageTypeReceived;
	native int PoisonDurationReceived;
	native int PoisonPeriodReceived;
	native Actor Poisoner;
	native Inventory Inv;
	native uint8 smokecounter;
	native uint8 FriendPlayer;
	native uint Translation;
	native sound AttackSound;
	native sound DeathSound;
	native sound SeeSound;
	native sound PainSound;
	native sound ActiveSound;
	native sound UseSound;
	native sound BounceSound;
	native sound WallBounceSound;
	native sound CrushPainSound;
	native double MaxDropoffHeight;
	native double MaxStepHeight;
	native double MaxSlopeSteepness;
	native int16 PainChance;
	native name PainType;
	native name DeathType;
	native double DamageFactor;
	native double DamageMultiply;
	native Class<Actor> TelefogSourceType;
	native Class<Actor> TelefogDestType;
	native readonly State SpawnState;
	native readonly State SeeState;
	native State MeleeState;
	native State MissileState;
	native voidptr /*DecalBase*/ DecalGenerator;
	native uint8 fountaincolor;
	native double CameraHeight;	// Height of camera when used as such
	native double CameraFOV;
	native double ViewAngle, ViewPitch, ViewRoll;
	native double RadiusDamageFactor;		// Radius damage factor
	native double SelfDamageFactor;
	native double StealthAlpha;
	native int WoundHealth;		// Health needed to enter wound state
	native readonly color BloodColor;
	native readonly int BloodTranslation;
	native int RenderHidden;
	native int RenderRequired;
	native readonly int FriendlySeeBlocks;
	native readonly int SpawnTime;
	private native int InventoryID;	// internal counter.

	meta String Obituary;		// Player was killed by this actor
	meta String HitObituary;		// Player was killed by this actor in melee
	meta double DeathHeight;	// Height on normal death
	meta double BurnHeight;		// Height on burning death
	meta int GibHealth;			// Negative health below which this monster dies an extreme death
	meta Sound HowlSound;		// Sound being played when electrocuted or poisoned
	meta Name BloodType;		// Blood replacement type
	meta Name BloodType2;		// Bloopsplatter replacement type
	meta Name BloodType3;		// AxeBlood replacement type
	meta bool DontHurtShooter;
	meta int ExplosionRadius;
	meta int ExplosionDamage;
	meta int MeleeDamage;
	meta Sound MeleeSound;
	meta Sound RipSound;
	meta double MissileHeight;
	meta Name MissileName;
	meta double FastSpeed;		// speed in fast mode

	// todo: implement access to native meta properties.
	// native meta int infighting_group;
	// native meta int projectile_group;
	// native meta int splash_group;

	Property prefix: none;
	Property Obituary: Obituary;
	Property HitObituary: HitObituary;
	Property MeleeDamage: MeleeDamage;
	Property MeleeSound: MeleeSound;
	Property MissileHeight: MissileHeight;
	Property MissileType: MissileName;
	Property DontHurtShooter: DontHurtShooter;
	Property ExplosionRadius: ExplosionRadius;
	Property ExplosionDamage: ExplosionDamage;
	//Property BloodType: BloodType, BloodType2, BloodType3;
	Property FastSpeed: FastSpeed;
	Property HowlSound: HowlSound;
	Property GibHealth: GibHealth;
	Property DeathHeight: DeathHeight;
	Property BurnHeight: BurnHeight;
	property Health: health;
	property WoundHealth: WoundHealth;
	property ReactionTime: reactiontime;
	property PainThreshold: PainThreshold;
	property DamageMultiply: DamageMultiply;
	property ProjectileKickback: ProjectileKickback;
	property Speed: speed;
	property FloatSpeed: FloatSpeed;
	property Radius: radius;
	property RenderRadius: RenderRadius;
	property Height: height;
	property ProjectilePassHeight: ProjectilePassHeight;
	property Mass: mass;
	property XScale: ScaleX;
	property YScale: ScaleY;
	property SeeSound: SeeSound;
	property AttackSound: AttackSound;
	property BounceSound: BounceSound;
	property WallBounceSound: WallBounceSound;
	property PainSound: PainSound;
	property DeathSound: DeathSound;
	property ActiveSound: ActiveSound;
	property CrushPainSound: CrushPainSound;
	property Alpha: Alpha;
	property MaxTargetRange: MaxTargetRange;
	property MeleeThreshold: MeleeThreshold;
	property MeleeRange: MeleeRange;
	property PushFactor: PushFactor;
	property BounceCount: BounceCount;
	property WeaveIndexXY: WeaveIndexXY;
	property WeaveIndexZ: WeaveIndexZ;
	property MinMissileChance: MinMissileChance;
	property MaxStepHeight: MaxStepHeight;
	property MaxDropoffHeight: MaxDropoffHeight;
	property MaxSlopeSteepness: MaxSlopeSteepness;
	property PoisonDamageType: PoisonDamageType;
	property RadiusDamageFactor: RadiusDamageFactor;
	property SelfDamageFactor: SelfDamageFactor;
	property StealthAlpha: StealthAlpha;
	property CameraHeight: CameraHeight;
	property CameraFOV: CameraFOV;
	property VSpeed: velz;
	property SpriteRotation: SpriteRotation;
	property VisibleAngles: VisibleStartAngle, VisibleEndAngle;
	property VisiblePitch: VisibleStartPitch, VisibleEndPitch;
	property Species: Species;
	property Accuracy: accuracy;
	property Stamina: stamina;
	property TelefogSourceType: TelefogSourceType;
	property TelefogDestType: TelefogDestType;
	property Ripperlevel: RipperLevel;
	property RipLevelMin: RipLevelMin;
	property RipLevelMax: RipLevelMax;
	property RipSound: RipSound;
	property RenderHidden: RenderHidden;
	property RenderRequired: RenderRequired;
	property FriendlySeeBlocks: FriendlySeeBlocks;
	property ThruBits: ThruBits;
	
	// need some definition work first
	//FRenderStyle RenderStyle;
	native private int RenderStyle;	// This is kept private until its real type has been implemented into the VM. But some code needs to copy this.
	//int ConversationRoot; // THe root of the current dialogue

	// deprecated things.
	native readonly deprecated("2.3", "Use Pos.X instead") double X;
	native readonly deprecated("2.3", "Use Pos.Y instead") double Y;
	native readonly deprecated("2.3", "Use Pos.Z instead") double Z;
	native readonly deprecated("2.3", "Use Vel.X instead") double VelX;
	native readonly deprecated("2.3", "Use Vel.Y instead") double VelY;
	native readonly deprecated("2.3", "Use Vel.Z instead") double VelZ;
	native readonly deprecated("2.3", "Use Vel.X instead") double MomX;
	native readonly deprecated("2.3", "Use Vel.Y instead") double MomY;
	native readonly deprecated("2.3", "Use Vel.Z instead") double MomZ;
	native deprecated("2.3", "Use Scale.X instead") double ScaleX;
	native deprecated("2.3", "Use Scale.Y instead") double ScaleY;

	//FStrifeDialogueNode *Conversation; // [RH] The dialogue to show when this actor is used.;
	
	
	Default
	{
		Scale 1;
		Health DEFAULT_HEALTH;
		Reactiontime 8;
		Radius 20;
		RenderRadius 0;
		Height 16;
		Mass 100;
		RenderStyle 'Normal';
		Alpha 1;
		MinMissileChance 200;
		MeleeRange 64 - 20;
		MaxDropoffHeight 24;
		MaxStepHeight 24;
		MaxSlopeSteepness STEEPSLOPE;
		BounceFactor 0.7;
		WallBounceFactor 0.75;
		BounceCount -1;
		FloatSpeed 4;
		FloatBobPhase -1;	// randomly initialize by default
		FloatBobStrength 1.0;
		Gravity 1;
		Friction 1;
		DamageFactor 1.0;		// damage multiplier as target of damage.
		DamageMultiply 1.0;		// damage multiplier as source of damage.
		PushFactor 0.25;
		WeaveIndexXY 0;
		WeaveIndexZ 16;
		DesignatedTeam 255;
		PainType "Normal";
		DeathType "Normal";
		TeleFogSourceType "TeleportFog";
		TeleFogDestType 'TeleportFog';
		RipperLevel 0;
		RipLevelMin 0;
		RipLevelMax 0;
		RipSound "misc/ripslop";
		DefThreshold 100;
		BloodType "Blood", "BloodSplatter", "AxeBlood";
		ExplosionDamage 128;
		ExplosionRadius -1;	// i.e. use ExplosionDamage value
		MissileHeight 32;
		SpriteAngle 0;
		SpriteRotation 0;
		StencilColor "00 00 00";
		VisibleAngles 0, 0;
		VisiblePitch 0, 0;
		DefaultStateUsage SUF_ACTOR|SUF_OVERLAY;
		CameraHeight int.min;
		CameraFOV 90.f;
		FastSpeed -1;
		RadiusDamageFactor 1;
		SelfDamageFactor 1;
		StealthAlpha 0;
		WoundHealth 6;
		GibHealth int.min;
		DeathHeight -1;
		BurnHeight -1;
		RenderHidden 0;
		RenderRequired 0;
		FriendlySeeBlocks 10; // 10 (blocks) * 128 (one map unit block)
	}
	
	// Functions

	// 'parked' global functions.
	native clearscope static double deltaangle(double ang1, double ang2);
	native clearscope static double absangle(double ang1, double ang2);
	native clearscope static Vector2 AngleToVector(double angle, double length = 1);
	native clearscope static Vector2 RotateVector(Vector2 vec, double angle);
	native clearscope static double Normalize180(double ang);
	
	virtual void MarkPrecacheSounds()
	{
		MarkSound(SeeSound);
		MarkSound(AttackSound);
		MarkSound(PainSound);
		MarkSound(DeathSound);
		MarkSound(ActiveSound);
		MarkSound(UseSound);
		MarkSound(BounceSound);
		MarkSound(WallBounceSound);
		MarkSound(CrushPainSound);
		MarkSound(HowlSound);
		MarkSound(MeleeSound);
	}

	bool IsPointerEqual(int ptr_select1, int ptr_select2)
	{
		return GetPointer(ptr_select1) == GetPointer(ptr_select2);
	}
	
	clearscope static double BobSin(double fb)
	{
		return sin(fb * (180./32)) * 8;
	}

	native clearscope bool isFrozen() const;
	virtual native void BeginPlay();
	virtual native void Activate(Actor activator);
	virtual native void Deactivate(Actor activator);
	virtual native int DoSpecialDamage (Actor target, int damage, Name damagetype);
	virtual native int TakeSpecialDamage (Actor inflictor, Actor source, int damage, Name damagetype);
	virtual native void Die(Actor source, Actor inflictor, int dmgflags = 0, Name MeansOfDeath = 'none');
	virtual native bool Slam(Actor victim);
	virtual native void Touch(Actor toucher);
	private native void Substitute(Actor replacement);
	native ui void DisplayNameTag();

	// Called by inventory items to see if this actor is capable of touching them.
	// If true, the item will attempt to be picked up. Useful for things like
	// allowing morphs to pick up limited items such as keys while preventing
	// them from picking other items up.
	virtual bool CanTouchItem(Inventory item)
	{
		return true;
	}

	// Called by PIT_CheckThing to check if two actors actually can collide.
	virtual bool CanCollideWith(Actor other, bool passive)
	{
		return true;
	}

	// Called by revival/resurrection to check if one can resurrect the other.
	// "other" can be null when not passive.
	virtual bool CanResurrect(Actor other, bool passive)
	{
		return true;
	}

	// Called when an actor is to be reflected by a disc of repulsion.
	// Returns true to continue normal blast processing.
	virtual bool SpecialBlastHandling (Actor source, double strength)
	{
		return true;
	}

	// This is called before a missile gets exploded.
	virtual int SpecialMissileHit (Actor victim)
	{
		return -1;
	}

	// Called when the player presses 'use' and an actor is found, except if the
	// UseSpecial flag is set. Use level.ExecuteSpecial to call action specials
	// instead.
	virtual bool Used(Actor user)
	{
		return false;
	}
	
	virtual class<Actor> GetBloodType(int type)
	{
		Class<Actor> bloodcls;
		if (type == 0)
		{
			bloodcls = BloodType;
		}
		else if (type == 1)
		{
			bloodcls = BloodType2;
		}
		else if (type == 2)
		{
			bloodcls = BloodType3;
		}
		else
		{
			return NULL;
		}

		if (bloodcls != NULL)
		{
			bloodcls = GetReplacement(bloodcls);
		}
		return bloodcls;
	}
	
	virtual int GetGibHealth()
	{
		if (GibHealth != int.min)
		{
			return -abs(GibHealth);
		}
		else
		{
			return -int(GetSpawnHealth() * gameinfo.gibfactor);
		}
	}
	
	virtual double GetDeathHeight()
	{
		// [RH] Allow the death height to be overridden using metadata.
		double metaheight = -1;
		if (DamageType == 'Fire')
		{
			metaheight = BurnHeight;
		}
		if (metaheight < 0)
		{
			metaheight = DeathHeight;
		}
		if (metaheight < 0)
		{
			return Height * 0.25;
		}
		else
		{
			return MAX(metaheight, 0);
		}
	}
	
	virtual String GetObituary(Actor victim, Actor inflictor, Name mod, bool playerattack)
	{
		if (mod == 'Telefrag')
		{
			return "$OB_MONTELEFRAG";
		}
		else if (mod == 'Melee' && HitObituary.Length() > 0)
		{
			return HitObituary;
		}
		return Obituary;
	}
	
	virtual int OnDrain(Actor victim, int damage, Name dmgtype)
	{
		return damage;
	}

	// called on getting a secret, return false to disable default "secret found" message/sound
	virtual bool OnGiveSecret(bool printmsg, bool playsound) { return true; }

	// called before and after triggering a teleporter
	// return false in PreTeleport() to cancel the action early
	virtual bool PreTeleport( Vector3 destpos, double destangle, int flags ) { return true; }
	virtual void PostTeleport( Vector3 destpos, double destangle, int flags ) {}
	
	native virtual bool OkayToSwitchTarget(Actor other);
	native static class<Actor> GetReplacement(class<Actor> cls);
	native static class<Actor> GetReplacee(class<Actor> cls);
	native static int GetSpriteIndex(name sprt);
	native clearscope static double GetDefaultSpeed(class<Actor> type);
	native static class<Actor> GetSpawnableType(int spawnnum);
	native static int ApplyDamageFactors(class<Inventory> itemcls, Name damagetype, int damage, int defdamage);
	native void RemoveFromHash();
	native void ChangeTid(int newtid);
	deprecated("3.8", "Use Level.FindUniqueTid() instead") static int FindUniqueTid(int start = 0, int limit = 0)
	{
		return level.FindUniqueTid(start, limit);
	}
	native void SetShade(color col);
	native clearscope int GetRenderStyle() const;
	native clearscope bool CheckKeys(int locknum, bool remote, bool quiet = false);
	protected native void CheckPortalTransition(bool linked = true);
		
	native clearscope string GetTag(string defstr = "") const;
	native clearscope string GetCharacterName() const;
	native void SetTag(string defstr = "");
	native clearscope double GetBobOffset(double frac = 0) const;
	native void ClearCounters();
	native bool GiveBody (int num, int max=0);
	native bool HitFloor();
	native virtual bool Grind(bool items);
	native clearscope bool isTeammate(Actor other) const;
	native clearscope int PlayerNumber() const;
	native void SetFriendPlayer(PlayerInfo player);
	native void SoundAlert(Actor target, bool splash = false, double maxdist = 0);
	native void ClearBounce();
	native TerrainDef GetFloorTerrain();
	native bool CheckLocalView(int consoleplayer = -1 /* parameter is not used anymore but needed for backward compatibilityö. */);
	native bool CheckNoDelay();
	native bool UpdateWaterLevel (bool splash = true);
	native bool IsZeroDamage();
	native void ClearInterpolation();
	native clearscope Vector3 PosRelative(sector sec) const;
	native void RailAttack(FRailParams p);
		
	native void HandleSpawnFlags();
	native void ExplodeMissile(line lin = null, Actor target = null, bool onsky = false);
	native void RestoreDamage();
	native clearscope int SpawnHealth() const;
	virtual clearscope int GetMaxHealth(bool withupgrades = false) const	// this only exists to make checks for player health easier so they can avoid the type check and just call this method.
	{
		return SpawnHealth();
	}
	native void SetDamage(int dmg);
	native clearscope double Distance2D(Actor other) const;
	native clearscope double Distance3D(Actor other) const;
	native clearscope double Distance2DSquared(Actor other) const;
	native clearscope double Distance3DSquared(Actor other) const;
	native void SetOrigin(vector3 newpos, bool moving);
	native void SetXYZ(vector3 newpos);
	native clearscope Actor GetPointer(int aaptr);
	native double BulletSlope(out FTranslatedLineTarget pLineTarget = null, int aimflags = 0);
	native void CheckFakeFloorTriggers (double oldz, bool oldz_has_viewheight = false);
	native bool CheckFor3DFloorHit(double z, bool trigger);
	native bool CheckFor3DCeilingHit(double z, bool trigger);
	native int CheckMonsterUseSpecials();
	
	native bool CheckMissileSpawn(double maxdist);
	native bool CheckPosition(Vector2 pos, bool actorsonly = false, FCheckPosition tm = null);
	native bool TestMobjLocation();
	native static Actor Spawn(class<Actor> type, vector3 pos = (0,0,0), int replace = NO_REPLACE);
	native Actor SpawnMissile(Actor dest, class<Actor> type, Actor owner = null);
	native Actor SpawnMissileXYZ(Vector3 pos, Actor dest, Class<Actor> type, bool checkspawn = true, Actor owner = null);
	native Actor SpawnMissileZ (double z, Actor dest, class<Actor> type);
	native Actor SpawnMissileAngleZSpeed (double z, class<Actor> type, double angle, double vz, double speed, Actor owner = null, bool checkspawn = true);
	native Actor SpawnMissileZAimed (double z, Actor dest, Class<Actor> type);
	native Actor SpawnSubMissile(Class<Actor> type, Actor target);
	native Actor, Actor SpawnPlayerMissile(class<Actor> type, double angle = 1e37, double x = 0, double y = 0, double z = 0, out FTranslatedLineTarget pLineTarget = null, bool nofreeaim = false, bool noautoaim = false, int aimflags = 0);
	native void SpawnTeleportFog(Vector3 pos, bool beforeTele, bool setTarget);
	native Actor RoughMonsterSearch(int distance, bool onlyseekable = false, bool frontonly = false, double fov = 0);
	native int ApplyDamageFactor(Name damagetype, int damage);
	native int GetModifiedDamage(Name damagetype, int damage, bool passive, Actor inflictor = null, Actor source = null, int flags = 0);
	native bool CheckBossDeath();
	native bool CheckFov(Actor target, double fov);

	void A_Light(int extralight) { if (player) player.extralight = clamp(extralight, -20, 20); }
	void A_Light0() { if (player) player.extralight = 0; }
	void A_Light1() { if (player) player.extralight = 1; }
	void A_Light2() { if (player) player.extralight = 2; }
	void A_LightInverse() { if (player) player.extralight = 0x80000000; }
	
	native Actor OldSpawnMissile(Actor dest, class<Actor> type, Actor owner = null);
	native Actor SpawnPuff(class<Actor> pufftype, vector3 pos, double hitdir, double particledir, int updown, int flags = 0, Actor victim = null);
	native void SpawnBlood (Vector3 pos1, double dir, int damage);
	native void BloodSplatter (Vector3 pos, double hitangle, bool axe = false);
	native bool HitWater (sector sec, Vector3 pos, bool checkabove = false, bool alert = true, bool force = false);
	native void PlaySpawnSound(Actor missile);
	native clearscope bool CountsAsKill() const;
	
	native bool Teleport(Vector3 pos, double angle, int flags);
	native void TraceBleed(int damage, Actor missile);
	native void TraceBleedAngle(int damage, double angle, double pitch);

	native void SetIdle(bool nofunction = false);
	native bool CheckMeleeRange(double range = -1);
	native bool TriggerPainChance(Name mod, bool forcedPain = false);
	native virtual int DamageMobj(Actor inflictor, Actor source, int damage, Name mod, int flags = 0, double angle = 0);
	native void PoisonMobj (Actor inflictor, Actor source, int damage, int duration, int period, Name type);
	native double AimLineAttack(double angle, double distance, out FTranslatedLineTarget pLineTarget = null, double vrange = 0., int flags = 0, Actor target = null, Actor friender = null);
	native Actor, int LineAttack(double angle, double distance, double pitch, int damage, Name damageType, class<Actor> pufftype, int flags = 0, out FTranslatedLineTarget victim = null, double offsetz = 0., double offsetforward = 0., double offsetside = 0.);
	native bool LineTrace(double angle, double distance, double pitch, int flags = 0, double offsetz = 0., double offsetforward = 0., double offsetside = 0., out FLineTraceData data = null);
	native bool CheckSight(Actor target, int flags = 0);
	native bool IsVisible(Actor other, bool allaround, LookExParams params = null);
	native bool HitFriend();
	native bool MonsterMove();
	
	native SeqNode StartSoundSequenceID (int sequence, int type, int modenum, bool nostop = false);
	native SeqNode StartSoundSequence (Name seqname, int modenum);
	native void StopSoundSequence();

	native void FindFloorCeiling(int flags = 0);
	native double, double GetFriction();
	native bool, Actor TestMobjZ(bool quick = false);
	native clearscope static bool InStateSequence(State newstate, State basestate);
	
	bool TryWalk ()
	{
		if (!MonsterMove ())
		{
			return false;
		}
		movecount = random[TryWalk](0, 15);
		return true;
	}
	
	native bool TryMove(vector2 newpos, int dropoff, bool missilecheck = false, FCheckPosition tm = null);
	native bool CheckMove(vector2 newpos, int flags = 0, FCheckPosition tm = null);
	native vector2 CarryingSectorsHandling();
	native void NewChaseDir();
	native void RandomChaseDir();
	native bool CheckMissileRange();
	native bool SetState(state st, bool nofunction = false);
	native bool AdvanceState();
	clearscope native state FindState(statelabel st, bool exact = false) const;
	bool SetStateLabel(statelabel st, bool nofunction = false) { return SetState(FindState(st), nofunction); }
	native action state ResolveState(statelabel st);	// this one, unlike FindState, is context aware.
	native void LinkToWorld(LinkContext ctx = null);
	native void UnlinkFromWorld(out LinkContext ctx = null);
	native bool CanSeek(Actor target);
	native clearscope double AngleTo(Actor target, bool absolute = false) const;
	native void AddZ(double zadd, bool moving = true);
	native void SetZ(double z);
	native clearscope vector2 Vec2To(Actor other) const;
	native clearscope vector3 Vec3To(Actor other) const;
	native clearscope vector3 Vec3Offset(double x, double y, double z, bool absolute = false) const;
	native clearscope vector3 Vec3Angle(double length, double angle, double z = 0, bool absolute = false) const;
	native clearscope vector2 Vec2Angle(double length, double angle, bool absolute = false) const;
	native clearscope vector2 Vec2Offset(double x, double y, bool absolute = false) const;
	native clearscope vector3 Vec2OffsetZ(double x, double y, double atz, bool absolute = false) const;
	native double PitchFromVel();
	native void VelIntercept(Actor targ, double speed = -1, bool aimpitch = true, bool oldvel = false, bool resetvel = false);
	native void VelFromAngle(double speed = 1e37, double angle = 1e37);
	native void Vel3DFromAngle(double speed, double angle, double pitch);
	native void Thrust(double speed = 1e37, double angle = 1e37);
	native clearscope bool isFriend(Actor other) const;
	native clearscope bool isHostile(Actor other) const;
	native void AdjustFloorClip();
	native clearscope DropItem GetDropItems() const;
	native void CopyFriendliness (Actor other, bool changeTarget, bool resetHealth = true);
	native bool LookForMonsters();
	native bool LookForTid(bool allaround, LookExParams params = null);
	native bool LookForEnemies(bool allaround, LookExParams params = null);
	native bool LookForPlayers(bool allaround, LookExParams params = null);
	native bool TeleportMove(Vector3 pos, bool telefrag, bool modifyactor = true);
	native clearscope double DistanceBySpeed(Actor other, double speed) const;
	native name GetSpecies();
	native void PlayActiveSound();
	native void Howl();
	native void DrawSplash (int count, double angle, int kind);
	native void GiveSecret(bool printmsg = true, bool playsound = true);
	native clearscope double GetCameraHeight() const;
	native clearscope double GetGravity() const;
	native void DoMissileDamage(Actor target);
	native void RespawnHandling();
	native void BotThink();
	native void SlopedFloorHandling();
	native void FullAxisMovement(vector2 carryoffset);

	//==========================================================================
	//
	// AActor :: GetLevelSpawnTime
	//
	// Returns the time when this actor was spawned, 
	// relative to the current level.
	//
	//==========================================================================
	clearscope int GetLevelSpawnTime() const
	{
		return SpawnTime - level.totaltime + level.time;
	}

	//==========================================================================
	//
	// AActor :: GetAge
	//
	// Returns the number of ticks passed since this actor was spawned.
	//
	//==========================================================================
	
	clearscope int GetAge() const
	{
		return level.totaltime - SpawnTime;
	}

	clearscope double AccuracyFactor() const
	{
		return 1. / (1 << (accuracy * 5 / 100));
	}

	
	
	protected native void DestroyAllInventory();	// This is not supposed to be called by user code!
	native clearscope Inventory FindInventory(class<Inventory> itemtype, bool subclass = false) const;
	native Inventory GiveInventoryType(class<Inventory> itemtype);
	native void ApplyPowerups();
	native bool UsePuzzleItem(int PuzzleItemType);

	action native void SetCamera(Actor cam, bool revert = false);
	native bool Warp(Actor dest, double xofs = 0, double yofs = 0, double zofs = 0, double angle = 0, int flags = 0, double heightoffset = 0, double radiusoffset = 0, double pitch = 0);

	// DECORATE compatible functions
	native double GetZAt(double px = 0, double py = 0, double angle = 0, int flags = 0, int pick_pointer = AAPTR_DEFAULT);
	native clearscope int GetSpawnHealth() const;
	native double GetCrouchFactor(int ptr = AAPTR_PLAYER1);
	native double GetCVar(string cvar);
	native string GetCVarString(string cvar);
	native int GetPlayerInput(int inputnum, int ptr = AAPTR_DEFAULT);
	native int CountProximity(class<Actor> classname, double distance, int flags = 0, int ptr = AAPTR_DEFAULT);
	native int GetMissileDamage(int mask, int add, int ptr = AAPTR_DEFAULT);
	action native int OverlayID();
	action native double OverlayX(int layer = 0);
	action native double OverlayY(int layer = 0);
	action native double OverlayAlpha(int layer = 0);

	// DECORATE setters - it probably makes more sense to set these values directly now...
	void A_SetMass(int newmass) { mass = newmass; }
	void A_SetInvulnerable() { bInvulnerable = true; }
	void A_UnSetInvulnerable() { bInvulnerable = false; }
	void A_SetReflective() { bReflective = true; }
	void A_UnSetReflective() { bReflective = false; }
	void A_SetReflectiveInvulnerable() { bInvulnerable = true; bReflective = true; }
	void A_UnSetReflectiveInvulnerable() { bInvulnerable = false; bReflective = false; }
	void A_SetShootable() { bShootable = true; bNonShootable = false; }
	void A_UnSetShootable() { bShootable = false; bNonShootable = true; }
	void A_NoGravity() { bNoGravity = true; }
	void A_Gravity() { bNoGravity = false; Gravity = 1; }
	void A_LowGravity() { bNoGravity = false; Gravity = 0.125; }
	void A_SetGravity(double newgravity) { gravity = clamp(newgravity, 0., 10.); }
	void A_SetFloorClip() { bFloorClip = true; AdjustFloorClip(); }
	void A_UnSetFloorClip() { bFloorClip = false;  FloorClip = 0; }
	void A_HideThing() { bInvisible = true; }
	void A_UnHideThing() { bInvisible = false; }
	void A_SetArg(int arg, int val) { if (arg >= 0 && arg < 5) args[arg] = val;	}
	void A_Turn(double turn = 0) { angle += turn; }
	void A_SetDamageType(name newdamagetype) { damagetype = newdamagetype; }
	void A_SetSolid() { bSolid = true; }
	void A_UnsetSolid() { bSolid = false; }
	void A_SetFloat() { bFloat = true; }
	void A_UnsetFloat() { bFloat = false; }
	void A_SetFloatBobPhase(int bob) { if (bob >= 0 && bob <= 63) FloatBobPhase = bob; }
	void A_SetRipperLevel(int level) { RipperLevel = level; }
	void A_SetRipMin(int minimum) { RipLevelMin = minimum; }
	void A_SetRipMax(int maximum) { RipLevelMax = maximum; }
	void A_ScreamAndUnblock() { A_Scream(); A_NoBlocking(); }
	void A_ActiveAndUnblock() { A_ActiveSound(); A_NoBlocking(); }
	
	//---------------------------------------------------------------------------
	//
	// FUNC P_SpawnMissileAngle
	//
	// Returns NULL if the missile exploded immediately, otherwise returns
	// a mobj_t pointer to the missile.
	//
	//---------------------------------------------------------------------------

	Actor SpawnMissileAngle (class<Actor> type, double angle, double vz)
	{
		return SpawnMissileAngleZSpeed (pos.z + 32 + GetBobOffset(), type, angle, vz, GetDefaultSpeed (type));
	}

	Actor SpawnMissileAngleZ (double z, class<Actor> type, double angle, double vz, Actor owner = null)
	{
		return SpawnMissileAngleZSpeed (z, type, angle, vz, GetDefaultSpeed (type), owner);
	}
	

	void A_SetScale(double scalex, double scaley = 0, int ptr = AAPTR_DEFAULT, bool usezero = false)
	{
		Actor aptr = GetPointer(ptr);
		if (aptr)
		{
			aptr.Scale.X = scalex;
			aptr.Scale.Y = scaley != 0 || usezero? scaley : scalex;	// use scalex here, too, if only one parameter.
		}
	}
	void A_SetSpeed(double speed, int ptr = AAPTR_DEFAULT)
	{
		Actor aptr = GetPointer(ptr);
		if (aptr) aptr.Speed = speed;
	}
	void A_SetFloatSpeed(double speed, int ptr = AAPTR_DEFAULT)
	{
		Actor aptr = GetPointer(ptr);
		if (aptr) aptr.FloatSpeed = speed;
	}
	void A_SetPainThreshold(int threshold, int ptr = AAPTR_DEFAULT)
	{
		Actor aptr = GetPointer(ptr);
		if (aptr) aptr.PainThreshold = threshold;
	}
	bool A_SetSpriteAngle(double angle = 0, int ptr = AAPTR_DEFAULT)
	{
		Actor aptr = GetPointer(ptr);
		if (!aptr) return false;
		aptr.SpriteAngle = angle;
		return true;
	}
	bool A_SetSpriteRotation(double angle = 0, int ptr = AAPTR_DEFAULT)
	{
		Actor aptr = GetPointer(ptr);
		if (!aptr) return false;
		aptr.SpriteRotation = angle;
		return true;
	}

	deprecated("2.3", "This function does nothing and is only for Zandronum compatibility") void A_FaceConsolePlayer(double MaxTurnAngle = 0) {}

	void A_SetSpecial(int spec, int arg0 = 0, int arg1 = 0, int arg2 = 0, int arg3 = 0, int arg4 = 0)
	{
		special = spec;
		args[0] = arg0;
		args[1] = arg1;
		args[2] = arg2;
		args[3] = arg3;
		args[4] = arg4;
	}

	void A_ClearTarget()
	{
		target = null;
		lastheard = null;
		lastenemy = null;
	}

	void A_ChangeLinkFlags(int blockmap = FLAG_NO_CHANGE, int sector = FLAG_NO_CHANGE)
	{	
		UnlinkFromWorld();
		if (blockmap != FLAG_NO_CHANGE) bNoBlockmap = blockmap;
		if (sector != FLAG_NO_CHANGE) bNoSector = sector;
		LinkToWorld();
	}

	// killough 11/98: kill an object
	void A_Die(name damagetype = "none")
	{
		DamageMobj(null, null, health, damagetype, DMG_FORCED);
	}

	void SpawnDirt (double radius)
	{
		static const class<Actor> chunks[] = { "Dirt1", "Dirt2", "Dirt3", "Dirt4", "Dirt5", "Dirt6" };
		double zo = random[Dirt]() / 128. + 1;
		Vector3 pos = Vec3Angle(radius, random[Dirt]() * (360./256), zo);

		Actor mo = Spawn (chunks[random[Dirt](0, 5)], pos, ALLOW_REPLACE);
		if (mo)
		{
			mo.Vel.Z = random[Dirt]() / 64.;
		}
	}
	
	//
	// A_SinkMobj
	// Sink a mobj incrementally into the floor
	//

	bool SinkMobj (double speed)
	{
		if (Floorclip < Height)
		{
			Floorclip += speed;
			return false;
		}
		return true;
	}

	//
	// A_RaiseMobj
	// Raise a mobj incrementally from the floor to 
	// 

	bool RaiseMobj (double speed)
	{
		// Raise a mobj from the ground
		if (Floorclip > 0)
		{
			Floorclip -= speed;
			if (Floorclip <= 0)
			{
				Floorclip = 0;
				return true;
			}
			else
			{
				return false;
			}
		}
		return true;
	}
	
	Actor AimTarget()
	{
		FTranslatedLineTarget t;
		BulletSlope(t, ALF_PORTALRESTRICT);
		return t.linetarget;
	}
	
	void RestoreRenderStyle()
	{
		bShadow = default.bShadow;
		bGhost = default.bGhost;
		RenderStyle = default.RenderStyle;
		Alpha = default.Alpha;
	}
	
	virtual bool ShouldSpawn()
	{
		return true;
	}

	native void A_Face(Actor faceto, double max_turn = 0, double max_pitch = 270, double ang_offset = 0, double pitch_offset = 0, int flags = 0, double z_ofs = 0);

	void A_FaceTarget(double max_turn = 0, double max_pitch = 270, double ang_offset = 0, double pitch_offset = 0, int flags = 0, double z_ofs = 0)
	{
		A_Face(target, max_turn, max_pitch, ang_offset, pitch_offset, flags, z_ofs);
	}
	void A_FaceTracer(double max_turn = 0, double max_pitch = 270, double ang_offset = 0, double pitch_offset = 0, int flags = 0, double z_ofs = 0)
	{
		A_Face(tracer, max_turn, max_pitch, ang_offset, pitch_offset, flags, z_ofs);
	}
	void A_FaceMaster(double max_turn = 0, double max_pitch = 270, double ang_offset = 0, double pitch_offset = 0, int flags = 0, double z_ofs = 0)
	{
		A_Face(master, max_turn, max_pitch, ang_offset, pitch_offset, flags, z_ofs);
	}

	// Action functions
	// Meh, MBF redundant functions. Only for DeHackEd support.
	native bool A_LineEffect(int boomspecial = 0, int tag = 0);
	// End of MBF redundant functions.
	
	native void A_MonsterRail();
	native void A_Pain();
	native void A_NoBlocking(bool drop = true);
	void A_Fall() { A_NoBlocking(); }
	native void A_Look();
	native void A_Chase(statelabel melee = '_a_chase_default', statelabel missile = '_a_chase_default', int flags = 0);
	native void A_VileChase();
	native bool A_CheckForResurrection(State state = null, Sound snd = 0);
	native void A_BossDeath();
	bool A_CallSpecial(int special, int arg1=0, int arg2=0, int arg3=0, int arg4=0, int arg5=0)
	{
		return Level.ExecuteSpecial(special, self, null, false, arg1, arg2, arg3, arg4, arg5);
	}

	native void A_FastChase();
	native void A_PlayerScream();
	native void A_CheckTerrain();

	native void A_Wander(int flags = 0);
	native void A_Look2();

	deprecated("2.3", "Use A_CustomBulletAttack() instead") native void A_BulletAttack();
	native void A_WolfAttack(int flags = 0, sound whattoplay = "weapons/pistol", double snipe = 1.0, int maxdamage = 64, int blocksize = 128, int pointblank = 2, int longrange = 4, double runspeed = 160.0, class<Actor> pufftype = "BulletPuff");
	deprecated("4.3", "Use A_StartSound() instead") native clearscope void A_PlaySound(sound whattoplay = "weapons/pistol", int slot = CHAN_BODY, double volume = 1.0, bool looping = false, double attenuation = ATTN_NORM, bool local = false, double pitch = 0.0);
	native clearscope void A_StartSound(sound whattoplay, int slot = CHAN_BODY, int flags = 0, double volume = 1.0, double attenuation = ATTN_NORM, double pitch = 0.0, double startTime = 0.0);
	native void A_SoundVolume(int slot, double volume);
	native void A_SoundPitch(int slot, double pitch);
	deprecated("2.3", "Use A_StartSound(<sound>, CHAN_WEAPON) instead") void A_PlayWeaponSound(sound whattoplay, bool fullvol = false) { A_StartSound(whattoplay, CHAN_WEAPON, 0, 1, fullvol? ATTN_NONE : ATTN_NORM); }
	native void A_StopSound(int slot = CHAN_VOICE);	// Bad default but that's what is originally was...
	void A_StopAllSounds()	{	A_StopSounds(0,0);	}
	native void A_StopSounds(int chanmin, int chanmax);
	deprecated("2.3", "Use A_StartSound() instead") native void A_PlaySoundEx(sound whattoplay, name slot, bool looping = false, int attenuation = 0);
	deprecated("2.3", "Use A_StopSound() instead") native void A_StopSoundEx(name slot);
	native clearscope bool IsActorPlayingSound(int channel, Sound snd = 0);
	native void A_SeekerMissile(int threshold, int turnmax, int flags = 0, int chance = 50, int distance = 10);
	native action state A_Jump(int chance, statelabel label, ...);
	native Actor A_SpawnProjectile(class<Actor> missiletype, double spawnheight = 32, double spawnofs_xy = 0, double angle = 0, int flags = 0, double pitch = 0, int ptr = AAPTR_TARGET);
	native void A_CustomRailgun(int damage, int spawnofs_xy = 0, color color1 = 0, color color2 = 0, int flags = 0, int aim = 0, double maxdiff = 0, class<Actor> pufftype = "BulletPuff", double spread_xy = 0, double spread_z = 0, double range = 0, int duration = 0, double sparsity = 1.0, double driftspeed = 1.0, class<Actor> spawnclass = null, double spawnofs_z = 0, int spiraloffset = 270, int limit = 0, double veleffect = 3);
	native void A_Print(string whattoprint, double time = 0, name fontname = "none");
	native void A_PrintBold(string whattoprint, double time = 0, name fontname = "none");
	native void A_Log(string whattoprint, bool local = false);
	native void A_LogInt(int whattoprint, bool local = false);
	native void A_LogFloat(double whattoprint, bool local = false);
	native void A_SetTranslucent(double alpha, int style = 0);
	native void A_SetRenderStyle(double alpha, int style);
	native void A_FadeIn(double reduce = 0.1, int flags = 0);
	native void A_FadeOut(double reduce = 0.1, int flags = 1); //bool remove == true
	native void A_FadeTo(double target, double amount = 0.1, int flags = 0);
	native void A_SpawnDebris(class<Actor> spawntype, bool transfer_translation = false, double mult_h = 1, double mult_v = 1);
	native void A_SpawnParticle(color color1, int flags = 0, int lifetime = TICRATE, double size = 1, double angle = 0, double xoff = 0, double yoff = 0, double zoff = 0, double velx = 0, double vely = 0, double velz = 0, double accelx = 0, double accely = 0, double accelz = 0, double startalphaf = 1, double fadestepf = -1, double sizestep = 0);
	native void A_ExtChase(bool usemelee, bool usemissile, bool playactive = true, bool nightmarefast = false);
	native void A_DropInventory(class<Inventory> itemtype, int amount = -1);
	native void A_SetBlend(color color1, double alpha, int tics, color color2 = 0, double alpha2 = 0.);
	deprecated("2.3", "Use 'b<FlagName> = [true/false]' instead") native void A_ChangeFlag(string flagname, bool value);
	native void A_ChangeCountFlags(int kill = FLAG_NO_CHANGE, int item = FLAG_NO_CHANGE, int secret = FLAG_NO_CHANGE);

	void A_SetFriendly (bool set)
	{
		if (CountsAsKill() && health > 0) level.total_monsters--;
		bFriendly = set;
		if (CountsAsKill() && health > 0) level.total_monsters++;
	}

	native void A_RaiseMaster(int flags = 0);
	native void A_RaiseChildren(int flags = 0);
	native void A_RaiseSiblings(int flags = 0);
	native bool A_RaiseSelf(int flags = 0);
	native bool RaiseActor(Actor other, int flags = 0);
	native bool CanRaise();
	native void Revive();
	native void A_Weave(int xspeed, int yspeed, double xdist, double ydist);

	action native state, bool A_Teleport(statelabel teleportstate = null, class<SpecialSpot> targettype = "BossSpot", class<Actor> fogtype = "TeleportFog", int flags = 0, double mindist = 128, double maxdist = 0, int ptr = AAPTR_DEFAULT);
	action native state, bool A_Warp(int ptr_destination, double xofs = 0, double yofs = 0, double zofs = 0, double angle = 0, int flags = 0, statelabel success_state = null, double heightoffset = 0, double radiusoffset = 0, double pitch = 0);
	native void A_CountdownArg(int argnum, statelabel targstate = null);
	native state A_MonsterRefire(int chance, statelabel label);
	native void A_LookEx(int flags = 0, double minseedist = 0, double maxseedist = 0, double maxheardist = 0, double fov = 0, statelabel label = null);
	
	native void A_Recoil(double xyvel);
	native int A_RadiusGive(class<Inventory> itemtype, double distance, int flags, int amount = 0, class<Actor> filter = null, name species = "None", double mindist = 0, int limit = 0);
	native void A_CustomMeleeAttack(int damage = 0, sound meleesound = "", sound misssound = "", name damagetype = "none", bool bleed = true);
	native void A_CustomComboAttack(class<Actor> missiletype, double spawnheight, int damage, sound meleesound = "", name damagetype = "none", bool bleed = true);
	native void A_Burst(class<Actor> chunktype);
	native void A_RadiusDamageSelf(int damage = 128, double distance = 128, int flags = 0, class<Actor> flashtype = null);
	native int GetRadiusDamage(Actor thing, int damage, int distance, int fulldmgdistance = 0, bool oldradiusdmg = false);
	native int RadiusAttack(Actor bombsource, int bombdamage, int bombdistance, Name bombmod = 'none', int flags = RADF_HURTSOURCE, int fulldamagedistance = 0, name species = "None");
	
	native void A_Respawn(int flags = 1);
	native void A_RestoreSpecialPosition();
	native void A_QueueCorpse();
	native void A_DeQueueCorpse();
	native void A_ClearLastHeard();

	native void A_ClassBossHealth();
	native void A_SetAngle(double angle = 0, int flags = 0, int ptr = AAPTR_DEFAULT);
	native void A_SetPitch(double pitch, int flags = 0, int ptr = AAPTR_DEFAULT);
	native void A_SetRoll(double roll, int flags = 0, int ptr = AAPTR_DEFAULT);
	native void A_SetViewAngle(double angle = 0, int flags = 0, int ptr = AAPTR_DEFAULT);
	native void A_SetViewPitch(double pitch, int flags = 0, int ptr = AAPTR_DEFAULT);
	native void A_SetViewRoll(double roll, int flags = 0, int ptr = AAPTR_DEFAULT);
	deprecated("2.3", "User variables are deprecated in ZScript. Actor variables are directly accessible") native void A_SetUserVar(name varname, int value);
	deprecated("2.3", "User variables are deprecated in ZScript. Actor variables are directly accessible") native void A_SetUserArray(name varname, int index, int value);
	deprecated("2.3", "User variables are deprecated in ZScript. Actor variables are directly accessible") native void A_SetUserVarFloat(name varname, double value);
	deprecated("2.3", "User variables are deprecated in ZScript. Actor variables are directly accessible") native void A_SetUserArrayFloat(name varname, int index, double value);
	native void A_Quake(int intensity, int duration, int damrad, int tremrad, sound sfx = "world/quake");
	native void A_QuakeEx(int intensityX, int intensityY, int intensityZ, int duration, int damrad, int tremrad, sound sfx = "world/quake", int flags = 0, double mulWaveX = 1, double mulWaveY = 1, double mulWaveZ = 1, int falloff = 0, int highpoint = 0, double rollIntensity = 0, double rollWave = 0);
	action native void A_SetTics(int tics);
	native void A_DamageSelf(int amount, name damagetype = "none", int flags = 0, class<Actor> filter = null, name species = "None", int src = AAPTR_DEFAULT, int inflict = AAPTR_DEFAULT);
	native void A_DamageTarget(int amount, name damagetype = "none", int flags = 0, class<Actor> filter = null, name species = "None", int src = AAPTR_DEFAULT, int inflict = AAPTR_DEFAULT);
	native void A_DamageMaster(int amount, name damagetype = "none", int flags = 0, class<Actor> filter = null, name species = "None", int src = AAPTR_DEFAULT, int inflict = AAPTR_DEFAULT);
	native void A_DamageTracer(int amount, name damagetype = "none", int flags = 0, class<Actor> filter = null, name species = "None", int src = AAPTR_DEFAULT, int inflict = AAPTR_DEFAULT);
	native void A_DamageChildren(int amount, name damagetype = "none", int flags = 0, class<Actor> filter = null, name species = "None", int src = AAPTR_DEFAULT, int inflict = AAPTR_DEFAULT);
	native void A_DamageSiblings(int amount, name damagetype = "none", int flags = 0, class<Actor> filter = null, name species = "None", int src = AAPTR_DEFAULT, int inflict = AAPTR_DEFAULT);
	native void A_KillTarget(name damagetype = "none", int flags = 0, class<Actor> filter = null, name species = "None", int src = AAPTR_DEFAULT, int inflict = AAPTR_DEFAULT);
	native void A_KillMaster(name damagetype = "none", int flags = 0, class<Actor> filter = null, name species = "None", int src = AAPTR_DEFAULT, int inflict = AAPTR_DEFAULT);
	native void A_KillTracer(name damagetype = "none", int flags = 0, class<Actor> filter = null, name species = "None", int src = AAPTR_DEFAULT, int inflict = AAPTR_DEFAULT);
	native void A_KillChildren(name damagetype = "none", int flags = 0, class<Actor> filter = null, name species = "None", int src = AAPTR_DEFAULT, int inflict = AAPTR_DEFAULT);
	native void A_KillSiblings(name damagetype = "none", int flags = 0, class<Actor> filter = null, name species = "None", int src = AAPTR_DEFAULT, int inflict = AAPTR_DEFAULT);
	native void A_RemoveTarget(int flags = 0, class<Actor> filter = null, name species = "None");
	native void A_RemoveMaster(int flags = 0, class<Actor> filter = null, name species = "None");
	native void A_RemoveTracer(int flags = 0, class<Actor> filter = null, name species = "None");
	native void A_RemoveChildren(bool removeall = false, int flags = 0, class<Actor> filter = null, name species = "None");
	native void A_RemoveSiblings(bool removeall = false, int flags = 0, class<Actor> filter = null, name species = "None");
	native void A_Remove(int removee, int flags = 0, class<Actor> filter = null, name species = "None");
	native void A_SetTeleFog(class<Actor> oldpos, class<Actor> newpos);
	native void A_SwapTeleFog();
	native void A_SetHealth(int health, int ptr = AAPTR_DEFAULT);
	native void A_ResetHealth(int ptr = AAPTR_DEFAULT);
	native void A_SetSpecies(name species, int ptr = AAPTR_DEFAULT);
	native void A_SetChaseThreshold(int threshold, bool def = false, int ptr = AAPTR_DEFAULT);
	native bool A_FaceMovementDirection(double offset = 0, double anglelimit = 0, double pitchlimit = 0, int flags = 0, int ptr = AAPTR_DEFAULT);
	native int A_ClearOverlays(int sstart = 0, int sstop = 0, bool safety = true);
	native bool A_CopySpriteFrame(int from, int to, int flags = 0);
	native bool A_SetVisibleRotation(double anglestart = 0, double angleend = 0, double pitchstart = 0, double pitchend = 0, int flags = 0, int ptr = AAPTR_DEFAULT);
	native void A_SetTranslation(name transname);
	native bool A_SetSize(double newradius = -1, double newheight = -1, bool testpos = false);
	native void A_SprayDecal(String name, double dist = 172, vector3 offset = (0, 0, 0), vector3 direction = (0, 0, 0), bool useBloodColor = false, color decalColor = 0);
	native void A_SetMugshotState(String name);
	native void CopyBloodColor(Actor other);

	native void A_RearrangePointers(int newtarget, int newmaster = AAPTR_DEFAULT, int newtracer = AAPTR_DEFAULT, int flags=0);
	native void A_TransferPointer(int ptr_source, int ptr_recipient, int sourcefield, int recipientfield=AAPTR_DEFAULT, int flags=0);
	native void A_CopyFriendliness(int ptr_source = AAPTR_MASTER);

	action native bool A_Overlay(int layer, statelabel start = null, bool nooverride = false);
	native void A_WeaponOffset(double wx = 0, double wy = 32, int flags = 0);
	action native void A_OverlayScale(int layer, double wx = 1, double wy = 0, int flags = 0);
	action native void A_OverlayRotate(int layer, double degrees = 0, int flags = 0);
	action native void A_OverlayPivot(int layer, double wx = 0.5, double wy = 0.5, int flags = 0);
	action native void A_OverlayPivotAlign(int layer, int halign, int valign);
	action native void A_OverlayVertexOffset(int layer, int index, double x, double y, int flags = 0);
	action native void A_OverlayOffset(int layer = PSP_WEAPON, double wx = 0, double wy = 32, int flags = 0);
	action native void A_OverlayFlags(int layer, int flags, bool set);
	action native void A_OverlayAlpha(int layer, double alph);
	action native void A_OverlayRenderStyle(int layer, int style);
	action native void A_OverlayTranslation(int layer, name trname);
	
	native bool A_AttachLightDef(Name lightid, Name lightdef);
	native bool A_AttachLight(Name lightid, int type, Color lightcolor, int radius1, int radius2, int flags = 0, Vector3 ofs = (0,0,0), double param = 0, double spoti = 10, double spoto = 25, double spotp = 0);
	native bool A_RemoveLight(Name lightid);

	int ACS_NamedExecute(name script, int mapnum=0, int arg1=0, int arg2=0, int arg3=0)
	{
		return ACS_Execute(-int(script), mapnum, arg1, arg2, arg3);
	}
	int ACS_NamedSuspend(name script, int mapnum=0)
	{
		return ACS_Suspend(-int(script), mapnum);
	}
	int ACS_NamedTerminate(name script, int mapnum=0)
	{
		return ACS_Terminate(-int(script), mapnum);
	}
	int ACS_NamedLockedExecute(name script, int mapnum=0, int arg1=0, int arg2=0, int lock=0)
	{
		return ACS_LockedExecute(-int(script), mapnum, arg1, arg2, lock);
	}
	int ACS_NamedLockedExecuteDoor(name script, int mapnum=0, int arg1=0, int arg2=0, int lock=0)
	{
		return ACS_LockedExecuteDoor(-int(script), mapnum, arg1, arg2, lock);
	}
	int ACS_NamedExecuteWithResult(name script, int arg1=0, int arg2=0, int arg3=0, int arg4=0)
	{
		return ACS_ExecuteWithResult(-int(script), arg1, arg2, arg3, arg4);
	}
	int ACS_NamedExecuteAlways(name script, int mapnum=0, int arg1=0, int arg2=0, int arg3=0)
	{
		return ACS_ExecuteAlways(-int(script), mapnum, arg1, arg2, arg3);
	}
	int ACS_ScriptCall(name script, int arg1=0, int arg2=0, int arg3=0, int arg4=0)
	{
		return ACS_ExecuteWithResult(-int(script), arg1, arg2, arg3, arg4);
	}
	
	//===========================================================================
	//
	// Sounds
	//
	//===========================================================================

	void A_Scream()
	{
		if (DeathSound)
		{
			A_StartSound(DeathSound, CHAN_VOICE, CHANF_DEFAULT, 1, bBoss? ATTN_NONE : ATTN_NORM);
		}
	}

	void A_XScream()
	{
		A_StartSound(player? Sound("*gibbed") : Sound("misc/gibbed"), CHAN_VOICE);
	}

	void A_ActiveSound()
	{
		if (ActiveSound)
		{
			A_StartSound(ActiveSound, CHAN_VOICE);
		}
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_CheckSkullDone
	//
	//----------------------------------------------------------------------------

	void A_CheckPlayerDone()
	{
		if (player == NULL) Destroy();
	}

	
	
	States(Actor, Overlay, Weapon, Item)
	{
	Spawn:
		TNT1 A -1;
		Stop;
	Null:
		TNT1 A 1;
		Stop;
	GenericFreezeDeath:
		// Generic freeze death frames. Woo!
		#### # 5 A_GenericFreezeDeath;
		---- A 1 A_FreezeDeathChunks;
		Wait;
	GenericCrush:
		POL5 A -1;
		Stop;
	}

	// Internal functions
	deprecated("2.3") private native int __decorate_internal_int__(int i);
	deprecated("2.3") private native bool __decorate_internal_bool__(bool b);
	deprecated("2.3") private native double __decorate_internal_float__(double f);
}

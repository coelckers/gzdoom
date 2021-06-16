// for flag changer functions.
const FLAG_NO_CHANGE = -1;
const MAXPLAYERS = 8;

enum EStateUseFlags
{
	SUF_ACTOR = 1,
	SUF_OVERLAY = 2,
	SUF_WEAPON = 4,
	SUF_ITEM = 8,
};

// Flags for A_PainAttack
enum EPainAttackFlags
{
	PAF_NOSKULLATTACK = 1,
	PAF_AIMFACING = 2,
	PAF_NOTARGET = 4,
};

// Flags for A_VileAttack
enum EVileAttackFlags
{
	VAF_DMGTYPEAPPLYTODIRECT = 1,
};

// Flags for A_Saw
enum ESawFlags
{
	SF_NORANDOM = 1,
	SF_RANDOMLIGHTMISS = 2,
	SF_RANDOMLIGHTHIT = 4,
	SF_RANDOMLIGHTBOTH = 6,
	SF_NOUSEAMMOMISS = 8,
	SF_NOUSEAMMO = 16,
	SF_NOPULLIN = 32,
	SF_NOTURN = 64,
	SF_STEALARMOR = 128,
	SF_NORANDOMPUFFZ = 256,
};

// Flags for A_BFGSpray
enum EBFGSprayFlags
{
	BFGF_HURTSOURCE = 1,
	BFGF_MISSILEORIGIN = 2,
};

// Flags for A_SpawnProjectile
enum ECustomMissileFlags
{
	CMF_AIMOFFSET = 1,
	CMF_AIMDIRECTION = 2,
	CMF_TRACKOWNER = 4,
	CMF_CHECKTARGETDEAD = 8,
	CMF_ABSOLUTEPITCH = 16,
	CMF_OFFSETPITCH = 32,
	CMF_SAVEPITCH = 64,
	CMF_ABSOLUTEANGLE = 128,
	CMF_BADPITCH = 256,	// for compatibility handling only - avoid!
};

// Flags for A_CustomBulletAttack
enum ECustomBulletAttackFlags
{
	CBAF_AIMFACING = 1,
	CBAF_NORANDOM = 2,
	CBAF_EXPLICITANGLE = 4,
	CBAF_NOPITCH = 8,
	CBAF_NORANDOMPUFFZ = 16,
	CBAF_PUFFTARGET = 32,
	CBAF_PUFFMASTER = 64,
	CBAF_PUFFTRACER = 128,
};

// Flags for A_GunFlash
enum EGunFlashFlags
{
	GFF_NOEXTCHANGE = 1,
};

// Flags for A_FireBullets
enum EFireBulletsFlags
{
	FBF_USEAMMO = 1,
	FBF_NORANDOM = 2,
	FBF_EXPLICITANGLE = 4,
	FBF_NOPITCH = 8,
	FBF_NOFLASH = 16,
	FBF_NORANDOMPUFFZ = 32,
	FBF_PUFFTARGET = 64,
	FBF_PUFFMASTER = 128,
	FBF_PUFFTRACER = 256,
};

// Flags for A_SpawnItemEx
enum ESpawnItemFlags
{
	SXF_TRANSFERTRANSLATION =		1 << 0,
	SXF_ABSOLUTEPOSITION =		1 << 1,
	SXF_ABSOLUTEANGLE =			1 << 2,
	SXF_ABSOLUTEMOMENTUM =		1 << 3,	//Since "momentum" is declared to be deprecated in the expressions, for compatibility
	SXF_ABSOLUTEVELOCITY =		1 << 3,	//purposes, this was made. It does the same thing though. Do not change the value.
	SXF_SETMASTER =				1 << 4,
	SXF_NOCHECKPOSITION =			1 << 5,
	SXF_TELEFRAG =				1 << 6,
	SXF_CLIENTSIDE =				1 << 7,	// only used by Skulltag
	SXF_TRANSFERAMBUSHFLAG =		1 << 8,
	SXF_TRANSFERPITCH =			1 << 9,
	SXF_TRANSFERPOINTERS =		1 << 10,
	SXF_USEBLOODCOLOR =			1 << 11,
	SXF_CLEARCALLERTID =			1 << 12,
	SXF_MULTIPLYSPEED =			1 << 13,
	SXF_TRANSFERSCALE =			1 << 14,
	SXF_TRANSFERSPECIAL =			1 << 15,
	SXF_CLEARCALLERSPECIAL =		1 << 16,
	SXF_TRANSFERSTENCILCOL =		1 << 17,
	SXF_TRANSFERALPHA =			1 << 18,
	SXF_TRANSFERRENDERSTYLE =		1 << 19,
	SXF_SETTARGET =				1 << 20,
	SXF_SETTRACER =				1 << 21,
	SXF_NOPOINTERS =				1 << 22,
	SXF_ORIGINATOR =				1 << 23,
	SXF_TRANSFERSPRITEFRAME	=	1 << 24,
	SXF_TRANSFERROLL =			1 << 25,
	SXF_ISTARGET				=	1 << 26,
	SXF_ISMASTER				=	1 << 27,
	SXF_ISTRACER				=	1 << 28,
};

// Flags for A_Chase
enum EChaseFlags
{
	CHF_FASTCHASE =						1,
	CHF_NOPLAYACTIVE =					2,
	CHF_NIGHTMAREFAST =					4,
	CHF_RESURRECT =						8,
	CHF_DONTMOVE =						16,
	CHF_NORANDOMTURN =					32,
	CHF_NODIRECTIONTURN =				64,
	CHF_NOPOSTATTACKTURN =				128,
	CHF_STOPIFBLOCKED =					256,

	CHF_DONTTURN = CHF_NORANDOMTURN | CHF_NOPOSTATTACKTURN | CHF_STOPIFBLOCKED
};

// Flags for A_LookEx
enum ELookFlags
{
	LOF_NOSIGHTCHECK = 1,
	LOF_NOSOUNDCHECK = 2,
	LOF_DONTCHASEGOAL = 4,
	LOF_NOSEESOUND = 8,
	LOF_FULLVOLSEESOUND = 16,
	LOF_NOJUMP = 32,
};

// Flags for A_Respawn
enum ERespawnFlags
{
	RSF_FOG = 1,
	RSF_KEEPTARGET = 2,
	RSF_TELEFRAG = 4,
};

// Flags for A_JumpIfTargetInLOS and A_JumpIfInTargetLOS
enum EJumpFlags
{
	JLOSF_PROJECTILE =		1,
	JLOSF_NOSIGHT =			1 << 1,
	JLOSF_CLOSENOFOV =		1 << 2,
	JLOSF_CLOSENOSIGHT =	1 << 3,
	JLOSF_CLOSENOJUMP =		1 << 4,
	JLOSF_DEADNOJUMP =		1 << 5,
	JLOSF_CHECKMASTER =		1 << 6,
	JLOSF_TARGETLOS =		1 << 7,
	JLOSF_FLIPFOV =			1 << 8,
	JLOSF_ALLYNOJUMP =		1 << 9,
	JLOSF_COMBATANTONLY =	1 << 10,
	JLOSF_NOAUTOAIM =		1 << 11,
	JLOSF_CHECKTRACER =		1 << 12,
};

// Flags for A_ChangeVelocity
enum EChangeVelocityFlags
{
	CVF_RELATIVE = 1,
	CVF_REPLACE = 2,
};

// Flags for A_WeaponReady
enum EWeaponReadyFlags
{ 
	WRF_NOBOB = 1,
	WRF_NOSWITCH = 2,
	WRF_NOPRIMARY = 4,
	WRF_NOSECONDARY = 8,
	WRF_NOFIRE = WRF_NOPRIMARY | WRF_NOSECONDARY,
	WRF_ALLOWRELOAD = 16,
	WRF_ALLOWZOOM = 32,
	WRF_DISABLESWITCH = 64,
	WRF_ALLOWUSER1 = 128,
	WRF_ALLOWUSER2 = 256,
	WRF_ALLOWUSER3 = 512,
	WRF_ALLOWUSER4 = 1024,
};

// Flags for A_SelectWeapon
enum ESelectWeaponFlags
{
	SWF_SELECTPRIORITY = 1,
};

// Morph constants
enum EMorphFlags
{
	MRF_OLDEFFECTS			= 0x00000000,
	MRF_ADDSTAMINA			= 0x00000001,
	MRF_FULLHEALTH			= 0x00000002,
	MRF_UNDOBYTOMEOFPOWER	= 0x00000004,
	MRF_UNDOBYCHAOSDEVICE	= 0x00000008,
	MRF_FAILNOTELEFRAG		= 0x00000010,
	MRF_FAILNOLAUGH			= 0x00000020,
	MRF_WHENINVULNERABLE	= 0x00000040,
	MRF_LOSEACTUALWEAPON	= 0x00000080,
	MRF_NEWTIDBEHAVIOUR		= 0x00000100,
	MRF_UNDOBYDEATH			= 0x00000200,
	MRF_UNDOBYDEATHFORCED	= 0x00000400,
	MRF_UNDOBYDEATHSAVES	= 0x00000800,
	MRF_UNDOBYTIMEOUT		= 0x00001000,
	MRF_UNDOALWAYS			= 0x00002000,
	MRF_TRANSFERTRANSLATION = 0x00004000,
	MRF_STANDARDUNDOING	= MRF_UNDOBYTOMEOFPOWER | MRF_UNDOBYCHAOSDEVICE | MRF_UNDOBYTIMEOUT,
};

// Flags for A_RailAttack and A_CustomRailgun
enum ERailFlags
{
	RGF_SILENT = 1,
	RGF_NOPIERCING = 2,
	RGF_EXPLICITANGLE = 4,
	RGF_FULLBRIGHT = 8,
	RGF_CENTERZ = 16,
	RGF_NORANDOMPUFFZ = 32,
};

// Flags for A_Mushroom
enum EMushroomFlags
{
	MSF_Standard = 0,
	MSF_Classic = 1,
	MSF_DontHurt = 2,
};

// Flags for A_Explode
enum EExplodeFlags
{
	XF_HURTSOURCE = 1,
	XF_NOTMISSILE = 4,
	XF_EXPLICITDAMAGETYPE = 8,
	XF_NOSPLASH = 16,
	XF_THRUSTZ = 32,

};

// Flags for A_RadiusThrust
enum ERadiusThrustFlags
{
	RTF_AFFECTSOURCE = 1,
	RTF_NOIMPACTDAMAGE = 2,
	RTF_NOTMISSILE = 4,
	RTF_THRUSTZ = 16,
};

// Flags for A_RadiusDamageSelf
enum ERadiusDamageSelfFlags
{
	RDSF_BFGDAMAGE = 1,
};

// Flags for A_Blast
enum EBlastFlags
{
	BF_USEAMMO = 1,
	BF_DONTWARN = 2,
	BF_AFFECTBOSSES = 4,
	BF_NOIMPACTDAMAGE = 8,
};

// Flags for A_SeekerMissile
enum ESeekerMissileFlags
{
	SMF_LOOK = 1,
	SMF_PRECISE = 2,
	SMF_CURSPEED = 4,
};

// Flags for A_CustomPunch
enum ECustomPunchFlags
{
	CPF_USEAMMO = 1,
	CPF_DAGGER = 2,
	CPF_PULLIN = 4,
	CPF_NORANDOMPUFFZ = 8,
	CPF_NOTURN = 16,
	CPF_STEALARMOR = 32,
};

enum EFireCustomMissileFlags
{
	FPF_AIMATANGLE = 1,
	FPF_TRANSFERTRANSLATION = 2,
	FPF_NOAUTOAIM = 4,
};

// Flags for A_Teleport
enum ETeleportFlags
{
	TF_TELEFRAG =		0x00000001, // Allow telefrag in order to teleport.
	TF_RANDOMDECIDE =	0x00000002, // Randomly fail based on health. (A_Srcr2Decide)
	TF_FORCED =			0x00000004, // Forget what's in the way. TF_Telefrag takes precedence though.
	TF_KEEPVELOCITY =	0x00000008, // Preserve velocity.
	TF_KEEPANGLE =		0x00000010, // Keep angle.
	TF_USESPOTZ =		0x00000020, // Set the z to the spot's z, instead of the floor.
	TF_NOSRCFOG =		0x00000040, // Don't leave any fog behind when teleporting.
	TF_NODESTFOG =		0x00000080, // Don't spawn any fog at the arrival position.
	TF_USEACTORFOG =	0x00000100, // Use the actor's TeleFogSourceType and TeleFogDestType fogs.
	TF_NOJUMP =			0x00000200, // Don't jump after teleporting.
	TF_OVERRIDE =		0x00000400, // Ignore NOTELEPORT.
	TF_SENSITIVEZ =		0x00000800, // Fail if the actor wouldn't fit in the position (for Z).

	TF_KEEPORIENTATION = TF_KEEPVELOCITY|TF_KEEPANGLE,
	TF_NOFOG = TF_NOSRCFOG|TF_NODESTFOG,
};

// Flags for A_WolfAttack
enum EWolfAttackFlags
{
	WAF_NORANDOM = 1,
	WAF_USEPUFF = 2
};

// Flags for A_RadiusGive
enum ERadiusGiveFlags
{
	RGF_GIVESELF	=   1,
	RGF_PLAYERS		=   1 << 1,
	RGF_MONSTERS	=   1 << 2,
	RGF_OBJECTS		=   1 << 3,
	RGF_VOODOO		=	1 << 4,
	RGF_CORPSES		=	1 << 5,
	RGF_NOTARGET	=	1 << 6,
	RGF_NOTRACER	=	1 << 7,
	RGF_NOMASTER	=	1 << 8,
	RGF_CUBE		=	1 << 9,
	RGF_NOSIGHT		=	1 << 10,
	RGF_MISSILES	=	1 << 11,
	RGF_INCLUSIVE	=	1 << 12,
	RGF_ITEMS		=	1 << 13,
	RGF_KILLED		=	1 << 14,
	RGF_EXFILTER	=	1 << 15,
	RGF_EXSPECIES	=	1 << 16,
	RGF_EITHER		=	1 << 17,
};

// Activation flags
enum EActivationFlags
{
	THINGSPEC_Default = 0,
	THINGSPEC_ThingActs = 1,
	THINGSPEC_ThingTargets = 2,
	THINGSPEC_TriggerTargets = 4,
	THINGSPEC_MonsterTrigger = 8,
	THINGSPEC_MissileTrigger = 16,
	THINGSPEC_ClearSpecial = 32,
	THINGSPEC_NoDeathSpecial = 64,
	THINGSPEC_TriggerActs = 128,
	THINGSPEC_Activate			= 1<<8,		// The thing is activated when triggered
	THINGSPEC_Deactivate		= 1<<9,		// The thing is deactivated when triggered
	THINGSPEC_Switch			= 1<<10,	// The thing is alternatively activated and deactivated when triggered
	
	// Shorter aliases for same
	AF_Default = 0,
	AF_ThingActs = 1,
	AF_ThingTargets = 2,
	AF_TriggerTargets = 4,
	AF_MonsterTrigger = 8,
	AF_MissileTrigger = 16,
	AF_ClearSpecial = 32,
	AF_NoDeathSpecial = 64,
	AF_TriggerActs = 128,
	AF_Activate			= 1<<8,		// The thing is activated when triggered
	AF_Deactivate		= 1<<9,		// The thing is deactivated when triggered
	AF_Switch			= 1<<10,	// The thing is alternatively activated and deactivated when triggered
	
};

// Flags for A_TakeInventory and A_TakeFromTarget
enum ETakeFlags
{
	TIF_NOTAKEINFINITE = 1
};
// For SetPlayerProperty action special
enum EPlayerProperties
{
	PROP_FROZEN = 0,
	PROP_NOTARGET = 1,
	PROP_INSTANTWEAPONSWITCH = 2,
	PROP_FLY = 3,
	PROP_TOTALLYFROZEN = 4,
	PROP_INVULNERABILITY  = 5, // (Deprecated)
	PROP_STRENGTH = 6, // (Deprecated)
	PROP_INVISIBILITY = 7, // (Deprecated)
	PROP_RADIATIONSUIT = 8, // (Deprecated)
	PROP_ALLMAP = 9, // (Deprecated)
	PROP_INFRARED = 10, // (Deprecated)
	PROP_WEAPONLEVEL2 = 11, // (Deprecated)
	PROP_FLIGHT = 12, // (Deprecated)
	PROP_SPEED = 15, // (Deprecated)
	PROP_BUDDHA = 16,
}

// Line_SetBlocking
enum EBlockFlags
{
	BLOCKF_CREATURES = 1,
	BLOCKF_MONSTERS = 2,
	BLOCKF_PLAYERS = 4,
	BLOCKF_FLOATERS = 8,
	BLOCKF_PROJECTILES = 16,
	BLOCKF_EVERYTHING = 32,
	BLOCKF_RAILING = 64,
	BLOCKF_USE = 128,
};

// Pointer constants, bitfield-enabled
enum EPointerFlags
{
	AAPTR_DEFAULT = 0,
	AAPTR_NULL = 0x1,
	AAPTR_TARGET = 0x2,
	AAPTR_MASTER = 0x4,
	AAPTR_TRACER = 0x8,
	
	AAPTR_PLAYER_GETTARGET = 0x10,
	AAPTR_PLAYER_GETCONVERSATION = 0x20,
	
	AAPTR_PLAYER1 = 0x40,
	AAPTR_PLAYER2 = 0x80,
	AAPTR_PLAYER3 = 0x100,
	AAPTR_PLAYER4 = 0x200,
	AAPTR_PLAYER5 = 0x400,
	AAPTR_PLAYER6 = 0x800,
	AAPTR_PLAYER7 = 0x1000,
	AAPTR_PLAYER8 = 0x2000,
	AAPTR_FRIENDPLAYER = 0x4000,
	AAPTR_LINETARGET = 0x8000,
};

// Pointer operation flags

enum EPointerOperations
{
	PTROP_UNSAFETARGET = 1,
	PTROP_UNSAFEMASTER = 2,
	PTROP_NOSAFEGUARDS = PTROP_UNSAFETARGET|PTROP_UNSAFEMASTER,
};

// Flags for A_Warp

enum EWarpFlags
{
	WARPF_ABSOLUTEOFFSET = 0x1,
	WARPF_ABSOLUTEANGLE = 0x2,
	WARPF_USECALLERANGLE = 0x4,
	WARPF_NOCHECKPOSITION = 0x8,
	WARPF_INTERPOLATE = 0x10,
	WARPF_WARPINTERPOLATION = 0x20,
	WARPF_COPYINTERPOLATION = 0x40,
	WARPF_STOP = 0x80,
	WARPF_TOFLOOR = 0x100,
	WARPF_TESTONLY = 0x200,
	WAPRF_ABSOLUTEPOSITION = 0x400,
	WARPF_ABSOLUTEPOSITION = 0x400,
	WARPF_BOB = 0x800,
	WARPF_MOVEPTR = 0x1000,
	WARPF_USETID = 0x2000,
	WARPF_COPYVELOCITY = 0x4000,
	WARPF_COPYPITCH = 0x8000,
};

// Flags for Actor.CheckMove()

enum ECheckMoveFlags
{
	PCM_DROPOFF  = 1,
	PCM_NOACTORS = 1 << 1,
	PCM_NOLINES  = 1 << 2,
};

// flags for A_SetPitch/SetAngle/SetRoll
enum EAngleFlags
{
	SPF_FORCECLAMP = 1,
	SPF_INTERPOLATE = 2,
};

// flags for A_CheckLOF

enum ELOFFlags
{
	CLOFF_NOAIM_VERT =			0x1,
	CLOFF_NOAIM_HORZ =			0x2,

	CLOFF_JUMPENEMY =			0x4,
	CLOFF_JUMPFRIEND =			0x8,
	CLOFF_JUMPOBJECT =			0x10,
	CLOFF_JUMPNONHOSTILE =		0x20,

	CLOFF_SKIPENEMY =			0x40,
	CLOFF_SKIPFRIEND =			0x80,
	CLOFF_SKIPOBJECT =			0x100,
	CLOFF_SKIPNONHOSTILE =		0x200,

	CLOFF_MUSTBESHOOTABLE =		0x400,

	CLOFF_SKIPTARGET =			0x800,
	CLOFF_ALLOWNULL =			0x1000,
	CLOFF_CHECKPARTIAL =		0x2000,

	CLOFF_MUSTBEGHOST =			0x4000,
	CLOFF_IGNOREGHOST =			0x8000,
	
	CLOFF_MUSTBESOLID =			0x10000,
	CLOFF_BEYONDTARGET =		0x20000,

	CLOFF_FROMBASE =			0x40000,
	CLOFF_MUL_HEIGHT =			0x80000,
	CLOFF_MUL_WIDTH =			0x100000,

	CLOFF_JUMP_ON_MISS =		0x200000,
	CLOFF_AIM_VERT_NOOFFSET =	0x400000,

	CLOFF_SETTARGET =			0x800000,
	CLOFF_SETMASTER =			0x1000000,
	CLOFF_SETTRACER =			0x2000000,

	CLOFF_SKIPOBSTACLES = CLOFF_SKIPENEMY|CLOFF_SKIPFRIEND|CLOFF_SKIPOBJECT|CLOFF_SKIPNONHOSTILE,
	CLOFF_NOAIM = CLOFF_NOAIM_VERT|CLOFF_NOAIM_HORZ
};

// Flags for A_Kill (Master/Target/Tracer/Children/Siblings) series
enum EKillFlags
{
	KILS_FOILINVUL =		0x00000001,
	KILS_KILLMISSILES =		0x00000002,
	KILS_NOMONSTERS =		0x00000004,
	KILS_FOILBUDDHA =		0x00000008,
	KILS_EXFILTER =			0x00000010,
	KILS_EXSPECIES =		0x00000020,
	KILS_EITHER =			0x00000040,
};

// Flags for A_Damage (Master/Target/Tracer/Children/Siblings/Self) series
enum EDamageFlags
{
	DMSS_FOILINVUL =		0x00000001,
	DMSS_AFFECTARMOR =		0x00000002,
	DMSS_KILL =				0x00000004,
	DMSS_NOFACTOR =			0x00000008,
	DMSS_FOILBUDDHA =		0x00000010,
	DMSS_NOPROTECT =		0x00000020,
	DMSS_EXFILTER =			0x00000040,
	DMSS_EXSPECIES =		0x00000080,
	DMSS_EITHER =			0x00000100,
	DMSS_INFLICTORDMGTYPE =	0x00000200,
};

// Flags for A_AlertMonsters
enum EAlertFlags
{
	AMF_TARGETEMITTER = 1,
	AMF_TARGETNONPLAYER = 2,
	AMF_EMITFROMTARGET = 4,
}

// Flags for A_Remove*
enum ERemoveFlags
{
	RMVF_MISSILES =			0x00000001,
	RMVF_NOMONSTERS =		0x00000002,
	RMVF_MISC =				0x00000004,
	RMVF_EVERYTHING =		0x00000008,
	RMVF_EXFILTER =			0x00000010,
	RMVF_EXSPECIES =		0x00000020,
	RMVF_EITHER =			0x00000040,
};

// Flags for A_Fade*
enum EFadeFlags
{
	FTF_REMOVE =	1 << 0,
	FTF_CLAMP =		1 << 1,
};

// Flags for A_Face*
enum EFaceFlags
{
	FAF_BOTTOM = 1,
	FAF_MIDDLE = 2,
	FAF_TOP = 4,
	FAF_NODISTFACTOR = 8,
};

// Flags for A_QuakeEx
enum EQuakeFlags
{
	QF_RELATIVE =		1,
	QF_SCALEDOWN =		1 << 1,
	QF_SCALEUP =		1 << 2,
	QF_MAX =			1 << 3,
	QF_FULLINTENSITY =	1 << 4,
	QF_WAVE =			1 << 5,
};

// A_CheckProximity flags
enum EProximityFlags
{
	CPXF_ANCESTOR			= 1,
	CPXF_LESSOREQUAL		= 1 << 1,
	CPXF_NOZ				= 1 << 2,
	CPXF_COUNTDEAD			= 1 << 3,
	CPXF_DEADONLY			= 1 << 4,
	CPXF_EXACT				= 1 << 5,
	CPXF_SETTARGET =		1 << 6,
	CPXF_SETMASTER =		1 << 7,
	CPXF_SETTRACER =		1 << 8,
	CPXF_FARTHEST =			1 << 9,
	CPXF_CLOSEST =			1 << 10,
	CPXF_SETONPTR =			1 << 11,
	CPXF_CHECKSIGHT =		1 << 12,
};

// Flags for A_CheckBlock
// These flags only affect the calling actor('s pointer), not the ones being searched.
enum ECheckBlockFlags
{
	CBF_NOLINES			= 1 << 0,	//Don't check actors.
	CBF_SETTARGET		= 1 << 1,	//Sets the caller/pointer's target to the actor blocking it. Actors only.
	CBF_SETMASTER		= 1 << 2,	//^ but with master.
	CBF_SETTRACER		= 1 << 3,	//^ but with tracer.
	CBF_SETONPTR		= 1 << 4,	//Sets the pointer change on the actor doing the checking instead of self.
	CBF_DROPOFF			= 1 << 5,	//Check for dropoffs.
	CBF_NOACTORS		= 1 << 6,	//Don't check actors.
	CBF_ABSOLUTEPOS		= 1 << 7,	//Absolute position for offsets.
	CBF_ABSOLUTEANGLE	= 1 << 8,	//Absolute angle for offsets.
};

enum EParticleFlags
{
	SPF_FULLBRIGHT =	1,
	SPF_RELPOS =		1 << 1,
	SPF_RELVEL =		1 << 2,
	SPF_RELACCEL =		1 << 3,
	SPF_RELANG =		1 << 4,
	SPF_NOTIMEFREEZE =	1 << 5,

	SPF_RELATIVE =	SPF_RELPOS|SPF_RELVEL|SPF_RELACCEL|SPF_RELANG
};

//Flags for A_FaceMovementDirection
enum EMovementFlags
{
	FMDF_NOPITCH =			1 << 0,
	FMDF_INTERPOLATE =		1 << 1,
	FMDF_NOANGLE =			1 << 2,
};

// Flags for GetZAt
enum EZFlags
{
	GZF_ABSOLUTEPOS =			1,			// Use the absolute position instead of an offsetted one.
	GZF_ABSOLUTEANG =			1 << 1,		// Don't add the actor's angle to the parameter.
	GZF_CEILING =				1 << 2,		// Check the ceiling instead of the floor.
	GZF_3DRESTRICT =			1 << 3,		// Ignore midtextures and 3D floors above the pointer's z.
	GZF_NOPORTALS =				1 << 4,		// Don't pass through any portals.
	GZF_NO3DFLOOR =				1 << 5,		// Pass all 3D floors.
};

// Flags for A_WeaponOffset
enum EWeaponOffsetFlags
{
	WOF_KEEPX =		1,
	WOF_KEEPY =		1 << 1,
	WOF_ADD =		1 << 2,
	WOF_INTERPOLATE = 1 << 3,
	WOF_RELATIVE	= 1 << 4,
	WOF_ZEROY		= 1 << 5,
};

// Flags for psprite layers
enum EPSpriteFlags
{
	PSPF_ADDWEAPON		= 1 << 0,
	PSPF_ADDBOB			= 1 << 1,
	PSPF_POWDOUBLE		= 1 << 2,
	PSPF_CVARFAST		= 1 << 3,
	PSPF_ALPHA			= 1 << 4,
	PSPF_RENDERSTYLE	= 1 << 5,
	PSPF_FLIP			= 1 << 6,
	PSPF_FORCEALPHA		= 1 << 7,
	PSPF_FORCESTYLE		= 1 << 8,
	PSPF_MIRROR			= 1 << 9,
	PSPF_PLAYERTRANSLATED = 1 << 10,
	PSPF_PIVOTPERCENT	= 1 << 11,
	PSPF_INTERPOLATE	= 1 << 12,
};

// Alignment constants for A_OverlayPivotAlign
enum EPSpriteAlign
{
	PSPA_TOP = 0,
	PSPA_CENTER,
	PSPA_BOTTOM,
	PSPA_LEFT = PSPA_TOP,
	PSPA_RIGHT = 2
};

// Default psprite layers
enum EPSPLayers
{
	PSP_STRIFEHANDS = -1,
	PSP_WEAPON = 1,
	PSP_FLASH = 1000,
	PSP_TARGETCENTER = int.max - 2,
	PSP_TARGETLEFT,
	PSP_TARGETRIGHT
};

enum EInputFlags
{
	// These are the original inputs sent by the player.
	INPUT_OLDBUTTONS,
	INPUT_BUTTONS,
	INPUT_PITCH,
	INPUT_YAW,
	INPUT_ROLL,
	INPUT_FORWARDMOVE,
	INPUT_SIDEMOVE,
	INPUT_UPMOVE,

	// These are the inputs, as modified by P_PlayerThink().
	// Most of the time, these will match the original inputs, but
	// they can be different if a player is frozen or using a
	// chainsaw.
	MODINPUT_OLDBUTTONS,
	MODINPUT_BUTTONS,
	MODINPUT_PITCH,
	MODINPUT_YAW,
	MODINPUT_ROLL,
	MODINPUT_FORWARDMOVE,
	MODINPUT_SIDEMOVE,
	MODINPUT_UPMOVE
};

enum EButtons
{
	BT_ATTACK		= 1<<0,	// Press "Fire".
	BT_USE			= 1<<1,	// Use button, to open doors, activate switches.
    BT_JUMP			= 1<<2,
    BT_CROUCH		= 1<<3,
	BT_TURN180		= 1<<4,
	BT_ALTATTACK	= 1<<5,	// Press your other "Fire".
	BT_RELOAD		= 1<<6,	// [XA] Reload key. Causes state jump in A_WeaponReady.
	BT_ZOOM			= 1<<7,	// [XA] Zoom key. Ditto.

	// The rest are all ignored by the play simulation and are for scripts.
	BT_SPEED		= 1<<8,
	BT_STRAFE		= 1<<9,

	BT_MOVERIGHT	= 1<<10,
	BT_MOVELEFT		= 1<<11,
	BT_BACK			= 1<<12,
	BT_FORWARD		= 1<<13,
	BT_RIGHT		= 1<<14,
	BT_LEFT			= 1<<15,
	BT_LOOKUP		= 1<<16,
	BT_LOOKDOWN		= 1<<17,
	BT_MOVEUP		= 1<<18,
	BT_MOVEDOWN		= 1<<19,
	BT_SHOWSCORES	= 1<<20,

	BT_USER1		= 1<<21,
	BT_USER2		= 1<<22,
	BT_USER3		= 1<<23,
	BT_USER4		= 1<<24,

	BT_RUN			= 1<<25,
};

// Flags for GetAngle
enum EGetAngleFlags
{
	GAF_RELATIVE =			1,
	GAF_SWITCH =			1 << 1,
};

//Flags for A_CopySpriteFrame
enum ECopySpriteFrameFlags
{
	CPSF_NOSPRITE =			1,
	CPSF_NOFRAME =			1 << 1,
};

//Flags for A_SetMaskRotation
enum EMaskRotationFlags
{
	VRF_NOANGLESTART =		1,
	VRF_NOANGLEEND =		1 << 1,
	VRF_NOPITCHSTART =		1 << 2,
	VRF_NOPITCHEND =		1 << 3,

	VRF_NOANGLE = VRF_NOANGLESTART|VRF_NOANGLEEND,
	VRF_NOPITCH = VRF_NOPITCHSTART|VRF_NOPITCHEND,
};
// Type definition for the implicit 'callingstate' parameter that gets passed to action functions.
enum EStateType
{
	STATE_Actor,
	STATE_Psprite,
	STATE_StateChain,
}

struct FStateParamInfo
{
	state mCallingState;
	/*EStateType*/int mStateType;
	int mPSPIndex;
}

// returned by AimLineAttack.
struct FTranslatedLineTarget
{
	Actor linetarget;
	double angleFromSource;
	double attackAngleFromSource;
	bool unlinked;	// found by a trace that went through an unlinked portal.
	
	native void TraceBleed(int damage, Actor missile);
}

enum EAimFlags
{
	ALF_FORCENOSMART = 1,
	ALF_CHECK3D = 2,
	ALF_CHECKNONSHOOTABLE = 4,
	ALF_CHECKCONVERSATION = 8,
	ALF_NOFRIENDS = 16,
	ALF_PORTALRESTRICT = 32,	// only work through portals with a global offset (to be used for stuff that cannot remember the calculated FTranslatedLineTarget info)
	ALF_NOWEAPONCHECK = 64,		// ignore NOAUTOAIM flag on a player's weapon.
}

enum ELineAttackFlags
{
	LAF_ISMELEEATTACK  = 1,
	LAF_NORANDOMPUFFZ  = 1 << 1,
	LAF_NOIMPACTDECAL  = 1 << 2,
	LAF_NOINTERACT     = 1 << 3,
	LAF_TARGETISSOURCE = 1 << 4,
	LAF_OVERRIDEZ      = 1 << 5,
	LAF_ABSOFFSET      = 1 << 6,
	LAF_ABSPOSITION    = 1 << 7,
}

enum ELineTraceFlags
{
	TRF_ABSPOSITION = 1,
	TRF_ABSOFFSET = 2,
	TRF_THRUSPECIES = 4,
	TRF_THRUACTORS = 8,
	TRF_THRUBLOCK = 16,
	TRF_THRUHITSCAN = 32,
	TRF_NOSKY = 64,
	TRF_ALLACTORS = 128,
	TRF_SOLIDACTORS = 256,
	TRF_BLOCKUSE = 512,
	TRF_BLOCKSELF = 1024,
}

const DEFMELEERANGE = 64;
const SAWRANGE = (64.+(1./65536.));	// use meleerange + 1 so the puff doesn't skip the flash (i.e. plays all states)
const MISSILERANGE = (32*64);
const PLAYERMISSILERANGE = 8192;	// [RH] New MISSILERANGE for players

enum ESightFlags
{
	SF_IGNOREVISIBILITY=1,
	SF_SEEPASTSHOOTABLELINES=2,
	SF_SEEPASTBLOCKEVERYTHING=4,
	SF_IGNOREWATERBOUNDARY=8
}

enum EDmgFlags
{
	DMG_NO_ARMOR = 1,
	DMG_INFLICTOR_IS_PUFF = 2,
	DMG_THRUSTLESS = 4,
	DMG_FORCED = 8,
	DMG_NO_FACTOR = 16,
	DMG_PLAYERATTACK = 32,
	DMG_FOILINVUL = 64,
	DMG_FOILBUDDHA = 128,
	DMG_NO_PROTECT = 256,
	DMG_USEANGLE = 512,
	DMG_NO_PAIN = 1024,
	DMG_EXPLOSION = 2048,
	DMG_NO_ENHANCE = 4096,
	DMG_NO_CVAR = 8192,
	DMG_NO_SKILL = 16384,
	DMG_SECTOR = 32768,
}

enum EReplace
{
	NO_REPLACE = 0,
	ALLOW_REPLACE = 1
}

// This translucency value produces the closest match to Heretic's TINTTAB.
// ~40% of the value of the overlaid image shows through.
const HR_SHADOW = (0x6800 / 65536.);
// Hexen's TINTTAB is the same as Heretic's, just reversed.
const HX_SHADOW = (0x9800 / 65536.);
const HX_ALTSHADOW = (0x6800 / 65536.);

enum EMapThingFlags
{
	MTF_AMBUSH			= 0x0008,	// Thing is deaf
	MTF_DORMANT			= 0x0010,	// Thing is dormant (use Thing_Activate)

	MTF_SINGLE			= 0x0100,	// Thing appears in single-player games
	MTF_COOPERATIVE		= 0x0200,	// Thing appears in cooperative games
	MTF_DEATHMATCH		= 0x0400,	// Thing appears in deathmatch games

	MTF_SHADOW			= 0x0800,
	MTF_ALTSHADOW		= 0x1000,
	MTF_FRIENDLY		= 0x2000,
	MTF_STANDSTILL		= 0x4000,
	MTF_STRIFESOMETHING	= 0x8000,

	MTF_SECRET			= 0x080000,	// Secret pickup
	MTF_NOINFIGHTING	= 0x100000,
	MTF_NOCOUNT			= 0x200000,	// Removes COUNTKILL/COUNTITEM
};

enum ESkillProperty
{
	SKILLP_FastMonsters,
	SKILLP_Respawn,
	SKILLP_RespawnLimit,
	SKILLP_DisableCheats,
	SKILLP_AutoUseHealth,
	SKILLP_SpawnFilter,
	SKILLP_EasyBossBrain,
	SKILLP_ACSReturn,
	SKILLP_NoPain,
	SKILLP_EasyKey,
	SKILLP_SlowMonsters,
	SKILLP_Infight,
	SKILLP_PlayerRespawn,
	SKILLP_SpawnMulti,
	SKILLP_InstantReaction,
};
enum EFSkillProperty	// floating point properties
{
	SKILLP_AmmoFactor,
	SKILLP_DropAmmoFactor,
	SKILLP_ArmorFactor,
	SKILLP_HealthFactor,
	SKILLP_DamageFactor,
	SKILLP_Aggressiveness,
	SKILLP_MonsterHealth,
	SKILLP_FriendlyHealth,
	SKILLP_KickbackFactor,
};

enum EWeaponPos
{
	WEAPONBOTTOM = 128,
	WEAPONTOP = 32
}

enum ETranslationTable
{
	TRANSLATION_Invalid,
	TRANSLATION_Players,
	TRANSLATION_PlayersExtra,
	TRANSLATION_Standard,
	TRANSLATION_LevelScripted,
	TRANSLATION_Decals,
	TRANSLATION_PlayerCorpses,
	TRANSLATION_Decorate,
	TRANSLATION_Blood,
	TRANSLATION_RainPillar,
	TRANSLATION_Custom,
};

enum EFindFloorCeiling
{
	FFCF_ONLYSPAWNPOS = 1,
	FFCF_SAMESECTOR = 2,
	FFCF_ONLY3DFLOORS = 4,	// includes 3D midtexes
	FFCF_3DRESTRICT = 8,	// ignore 3D midtexes and floors whose floorz are above thing's z
	FFCF_NOPORTALS = 16,	// ignore portals (considers them impassable.)
	FFCF_NOFLOOR = 32,
	FFCF_NOCEILING = 64,
	FFCF_RESTRICTEDPORTAL = 128,	// current values in the iterator's return are through a restricted portal type (i.e. some features are blocked.)
	FFCF_NODROPOFF = 256,			// Caller does not need a dropoff (saves some time when checking portals)
};

enum ERaise
{
	RF_TRANSFERFRIENDLINESS = 1,
	RF_NOCHECKPOSITION = 2
}

enum eFogParm
{
	FOGP_DENSITY = 0,
	FOGP_OUTSIDEDENSITY = 1,
	FOGP_SKYFOG = 2,
}

enum ETeleport
{
	TELF_DESTFOG			= 1,
	TELF_SOURCEFOG			= 2,
	TELF_KEEPORIENTATION	= 4,
	TELF_KEEPVELOCITY		= 8,
	TELF_KEEPHEIGHT			= 16,
	TELF_ROTATEBOOM			= 32,
	TELF_ROTATEBOOMINVERSE	= 64,
};

enum EGameType
{
	GAME_Any	 = 0,
	GAME_Doom	 = 1,
	GAME_Heretic = 2,
	GAME_Hexen	 = 4,
	GAME_Strife	 = 8,
	GAME_Chex	 = 16, //Chex is basically Doom, but we need to have a different set of actors.

	GAME_Raven			= GAME_Heretic|GAME_Hexen,
	GAME_DoomChex		= GAME_Doom|GAME_Chex,
	GAME_DoomStrifeChex	= GAME_Doom|GAME_Strife|GAME_Chex
}

enum PaletteFlashFlags
{
	PF_HEXENWEAPONS		= 1,
	PF_POISON			= 2,
	PF_ICE				= 4,
	PF_HAZARD			= 8,
};

enum EGameAction
{
	ga_nothing,
	ga_loadlevel,
	ga_newgame,
	ga_newgame2,
	ga_recordgame,
	ga_loadgame,
	ga_loadgamehidecon,
	ga_loadgameplaydemo,
	ga_autoloadgame,
	ga_savegame,
	ga_autosave,
	ga_playdemo,
	ga_completed,
	ga_slideshow,
	ga_worlddone,
	ga_screenshot,
	ga_togglemap,
	ga_fullconsole,
};

enum EPuffFlags
{
	PF_HITTHING = 1,
	PF_MELEERANGE = 2,
	PF_TEMPORARY = 4,
	PF_HITTHINGBLEED = 8,
	PF_NORANDOMZ = 16,
	PF_HITSKY = 32
};

enum EPlayerCheats
{
	CF_NOCLIP			= 1 << 0,		// No clipping, walk through barriers.
	CF_GODMODE			= 1 << 1,		// No damage, no health loss.
	CF_NOVELOCITY		= 1 << 2,		// Not really a cheat, just a debug aid.
	CF_NOTARGET			= 1 << 3,		// [RH] Monsters don't target
	CF_FLY				= 1 << 4,		// [RH] Flying player
	CF_CHASECAM			= 1 << 5,		// [RH] Put camera behind player
	CF_FROZEN			= 1 << 6,		// [RH] Don't let the player move
	CF_REVERTPLEASE		= 1 << 7,		// [RH] Stick camera in player's head if (s)he moves
	CF_STEPLEFT			= 1 << 9,		// [RH] Play left footstep sound next time
	CF_FRIGHTENING		= 1 << 10,		// [RH] Scare monsters away
	CF_INSTANTWEAPSWITCH= 1 << 11,		// [RH] Switch weapons instantly
	CF_TOTALLYFROZEN	= 1 << 12,		// [RH] All players can do is press +use
	CF_PREDICTING		= 1 << 13,		// [RH] Player movement is being predicted
	CF_INTERPVIEW		= 1 << 14,		// [RH] view was changed outside of input, so interpolate one frame

	CF_EXTREMELYDEAD	= 1 << 22,		// [RH] Reliably let the status bar know about extreme deaths.

	CF_BUDDHA2			= 1 << 24,		// [MC] Absolute buddha. No voodoo can kill it either.
	CF_GODMODE2			= 1 << 25,		// [MC] Absolute godmode. No voodoo can kill it either.
	CF_BUDDHA			= 1 << 27,		// [SP] Buddha mode - take damage, but don't die
	CF_NOCLIP2			= 1 << 30,		// [RH] More Quake-like noclip

	// These flags no longer exist, but keep the names for some stray mod that might have used them. 
	CF_DRAIN			= 0,
	CF_HIGHJUMP			= 0,
	CF_REFLECTION		= 0,
	CF_PROSPERITY		= 0,
	CF_DOUBLEFIRINGSPEED= 0,
	CF_INFINITEAMMO		= 0,
};

enum EWeaponState
{
	WF_WEAPONREADY		= 1 << 0,		// [RH] Weapon is in the ready state and can fire its primary attack
	WF_WEAPONBOBBING	= 1 << 1,		// [HW] Bob weapon while the player is moving
	WF_WEAPONREADYALT	= 1 << 2,		// Weapon can fire its secondary attack
	WF_WEAPONSWITCHOK	= 1 << 3,		// It is okay to switch away from this weapon
	WF_DISABLESWITCH	= 1 << 4,		// Disable weapon switching completely
	WF_WEAPONRELOADOK	= 1 << 5,		// [XA] Okay to reload this weapon.
	WF_WEAPONZOOMOK		= 1 << 6,		// [XA] Okay to use weapon zoom function.
	WF_REFIRESWITCHOK	= 1 << 7,		// Mirror WF_WEAPONSWITCHOK for A_ReFire
	WF_USER1OK			= 1 << 8,		// [MC] Allow pushing of custom state buttons 1-4
	WF_USER2OK			= 1 << 9,
	WF_USER3OK			= 1 << 10,
	WF_USER4OK			= 1 << 11,
};

// these flags are for filtering actor visibility based on certain conditions of the renderer's feature support.
// currently, no renderer supports every single one of these features.
enum ActorRenderFeatureFlag
{
	RFF_FLATSPRITES		= 1<<0, // flat sprites
	RFF_MODELS			= 1<<1, // 3d models
	RFF_SLOPE3DFLOORS	= 1<<2, // sloped 3d floor support
	RFF_TILTPITCH		= 1<<3, // full free-look
	RFF_ROLLSPRITES		= 1<<4, // roll sprites
	RFF_UNCLIPPEDTEX	= 1<<5, // midtex and sprite can render "into" flats and walls
	RFF_MATSHADER		= 1<<6, // material shaders
	RFF_POSTSHADER		= 1<<7, // post-process shaders (renderbuffers)
	RFF_BRIGHTMAP		= 1<<8, // brightmaps
	RFF_COLORMAP		= 1<<9, // custom colormaps (incl. ability to fullbright certain ranges, ala Strife)
	RFF_POLYGONAL		= 1<<10, // uses polygons instead of wallscans/visplanes (i.e. softpoly and hardware opengl)
	RFF_TRUECOLOR		= 1<<11, // renderer is currently truecolor
	RFF_VOXELS		= 1<<12, // renderer is capable of voxels
};

// Special activation types
enum SPAC
{
	SPAC_Cross = 1<<0,		// when player crosses line
	SPAC_Use = 1<<1,		// when player uses line
	SPAC_MCross = 1<<2,		// when monster crosses line
	SPAC_Impact = 1<<3,		// when projectile hits line
	SPAC_Push = 1<<4,		// when player pushes line	
	SPAC_PCross = 1<<5,		// when projectile crosses line
	SPAC_UseThrough = 1<<6,	// when player uses line (doesn't block)
	// SPAC_PTOUCH is mapped to SPAC_PCross|SPAC_Impact
	SPAC_AnyCross = 1<<7,	// when anything without the MF2_TELEPORT flag crosses the line
	SPAC_MUse = 1<<8,		// monsters can use
	SPAC_MPush = 1<<9,		// monsters can push
	SPAC_UseBack = 1<<10,	// Can be used from the backside
	SPAC_Damage = 1<<11,	// [ZZ] when linedef receives damage
	SPAC_Death = 1<<12,		// [ZZ] when linedef receives damage and has 0 health

	SPAC_PlayerActivate = (SPAC_Cross|SPAC_Use|SPAC_Impact|SPAC_Push|SPAC_AnyCross|SPAC_UseThrough|SPAC_UseBack),
};

enum RadiusDamageFlags
{
	RADF_HURTSOURCE = 1,
	RADF_NOIMPACTDAMAGE = 2,
	RADF_SOURCEISSPOT = 4,
	RADF_NODAMAGE = 8,
	RADF_THRUSTZ = 16,
	RADF_OLDRADIUSDAMAGE = 32
};

enum IntermissionSequenceType
{
	FSTATE_EndingGame = 0,
	FSTATE_ChangingLevel = 1,
	FSTATE_InLevel = 2
};

enum Bobbing
{
	Bob_Normal,
	Bob_Inverse,
	Bob_Alpha,
	Bob_InverseAlpha,
	Bob_Smooth,
	Bob_InverseSmooth
};

enum EFinishLevelType
{
	FINISH_SameHub,
	FINISH_NextHub,
	FINISH_NoHub
};

enum EChangeLevelFlags
{
	CHANGELEVEL_KEEPFACING = 1,
	CHANGELEVEL_RESETINVENTORY = 2,
	CHANGELEVEL_NOMONSTERS = 4,
	CHANGELEVEL_CHANGESKILL = 8,
	CHANGELEVEL_NOINTERMISSION = 16,
	CHANGELEVEL_RESETHEALTH = 32,
	CHANGELEVEL_PRERAISEWEAPON = 64,
};

enum ELevelFlags
{
	LEVEL_NOINTERMISSION		= 0x00000001,
	LEVEL_NOINVENTORYBAR		= 0x00000002,	// This effects Doom only, since it's the only one without a standard inventory bar.
	LEVEL_DOUBLESKY				= 0x00000004,
	LEVEL_HASFADETABLE			= 0x00000008,	// Level uses Hexen's fadetable mapinfo to get fog

	LEVEL_MAP07SPECIAL			= 0x00000010,
	LEVEL_BRUISERSPECIAL		= 0x00000020,
	LEVEL_CYBORGSPECIAL			= 0x00000040,
	LEVEL_SPIDERSPECIAL			= 0x00000080,

	LEVEL_SPECLOWERFLOOR		= 0x00000100,
	LEVEL_SPECOPENDOOR			= 0x00000200,
	LEVEL_SPECLOWERFLOORTOHIGHEST=0x00000300,
	LEVEL_SPECACTIONSMASK		= 0x00000300,

	LEVEL_MONSTERSTELEFRAG		= 0x00000400,
	LEVEL_ACTOWNSPECIAL			= 0x00000800,
	LEVEL_SNDSEQTOTALCTRL		= 0x00001000,
	LEVEL_FORCETILEDSKY		= 0x00002000,

	LEVEL_CROUCH_NO				= 0x00004000,
	LEVEL_JUMP_NO				= 0x00008000,
	LEVEL_FREELOOK_NO			= 0x00010000,
	LEVEL_FREELOOK_YES			= 0x00020000,

	// The absence of both of the following bits means that this level does not
	// use falling damage (though damage can be forced with dmflags,.
	LEVEL_FALLDMG_ZD			= 0x00040000,	// Level uses ZDoom's falling damage
	LEVEL_FALLDMG_HX			= 0x00080000,	// Level uses Hexen's falling damage

	LEVEL_HEADSPECIAL			= 0x00100000,	// Heretic episode 1/4
	LEVEL_MINOTAURSPECIAL		= 0x00200000,	// Heretic episode 2/5
	LEVEL_SORCERER2SPECIAL		= 0x00400000,	// Heretic episode 3
	LEVEL_SPECKILLMONSTERS		= 0x00800000,

	LEVEL_STARTLIGHTNING		= 0x01000000,	// Automatically start lightning
	LEVEL_FILTERSTARTS			= 0x02000000,	// Apply mapthing filtering to player starts
	LEVEL_LOOKUPLEVELNAME		= 0x04000000,	// Level name is the name of a language string
	LEVEL_USEPLAYERSTARTZ		= 0x08000000,	// Use the Z position of player starts

	LEVEL_SWAPSKIES				= 0x10000000,	// Used by lightning
	LEVEL_NOALLIES				= 0x20000000,	// i.e. Inside Strife's front base
	LEVEL_CHANGEMAPCHEAT		= 0x40000000,	// Don't display cluster messages
	LEVEL_VISITED				= 0x80000000,	// Used for intermission map

	// The flags uint64_t is now split into 2 DWORDs 
	LEVEL2_RANDOMPLAYERSTARTS	= 0x00000001,	// Select single player starts randomnly (no voodoo dolls)
	LEVEL2_ALLMAP				= 0x00000002,	// The player picked up a map on this level

	LEVEL2_LAXMONSTERACTIVATION	= 0x00000004,	// Monsters can open doors depending on the door speed
	LEVEL2_LAXACTIVATIONMAPINFO	= 0x00000008,	// LEVEL_LAXMONSTERACTIVATION is not a default.

	LEVEL2_MISSILESACTIVATEIMPACT=0x00000010,	// Missiles are the activators of SPAC_IMPACT events, not their shooters
	LEVEL2_NEEDCLUSTERTEXT		= 0x00000020,	// A map with this flag needs to retain its cluster intermission texts when being redefined in UMAPINFO

	LEVEL2_KEEPFULLINVENTORY	= 0x00000040,	// doesn't reduce the amount of inventory items to 1

	LEVEL2_PRERAISEWEAPON		= 0x00000080,	// players should spawn with their weapons fully raised (but not when respawning it multiplayer)
	LEVEL2_MONSTERFALLINGDAMAGE	= 0x00000100,
	LEVEL2_CLIPMIDTEX			= 0x00000200,
	LEVEL2_WRAPMIDTEX			= 0x00000400,

	LEVEL2_CHECKSWITCHRANGE		= 0x00000800,	

	LEVEL2_PAUSE_MUSIC_IN_MENUS	= 0x00001000,
	LEVEL2_TOTALINFIGHTING		= 0x00002000,
	LEVEL2_NOINFIGHTING			= 0x00004000,

	LEVEL2_NOMONSTERS			= 0x00008000,
	LEVEL2_INFINITE_FLIGHT		= 0x00010000,

	LEVEL2_ALLOWRESPAWN			= 0x00020000,

	LEVEL2_FORCETEAMPLAYON		= 0x00040000,
	LEVEL2_FORCETEAMPLAYOFF		= 0x00080000,

	LEVEL2_CONV_SINGLE_UNFREEZE	= 0x00100000,
	LEVEL2_NOCLUSTERTEXT		= 0x00200000,	// ignore intermission texts fro clusters. This gets set when UMAPINFO is used to redefine its properties.
	LEVEL2_DUMMYSWITCHES		= 0x00400000,
	LEVEL2_HEXENHACK			= 0x00800000,	// Level was defined in a Hexen style MAPINFO

	LEVEL2_SMOOTHLIGHTING		= 0x01000000,	// Level uses the smooth lighting feature.
	LEVEL2_POLYGRIND			= 0x02000000,	// Polyobjects grind corpses to gibs.
	LEVEL2_RESETINVENTORY		= 0x04000000,	// Resets player inventory when starting this level (unless in a hub)
	LEVEL2_RESETHEALTH			= 0x08000000,	// Resets player health when starting this level (unless in a hub)

	LEVEL2_NOSTATISTICS			= 0x10000000,	// This level should not have statistics collected
	LEVEL2_ENDGAME				= 0x20000000,	// This is an epilogue level that cannot be quit.
	LEVEL2_NOAUTOSAVEHINT		= 0x40000000,	// tell the game that an autosave for this level does not need to be kept
	LEVEL2_FORGETSTATE			= 0x80000000,	// forget this map's state in a hub
	
	// More flags!
	LEVEL3_FORCEFAKECONTRAST	= 0x00000001,	// forces fake contrast even with fog enabled
	LEVEL3_REMOVEITEMS			= 0x00000002,	// kills all INVBAR items on map change.
	LEVEL3_ATTENUATE			= 0x00000004,	// attenuate lights?
	LEVEL3_NOLIGHTFADE			= 0x00000008,	// no light fading to black.
	LEVEL3_NOCOLOREDSPRITELIGHTING = 0x00000010,	// draw sprites only with color-less light
	LEVEL3_EXITNORMALUSED		= 0x00000020,
	LEVEL3_EXITSECRETUSED		= 0x00000040,
	LEVEL3_FORCEWORLDPANNING	= 0x00000080,	// Forces the world panning flag for all textures, even those without it explicitly set.
	LEVEL3_HIDEAUTHORNAME		= 0x00000100,
	LEVEL3_PROPERMONSTERFALLINGDAMAGE	= 0x00000200,	// Properly apply falling damage to the monsters
	LEVEL3_SKYBOXAO				= 0x00000400,	// Apply SSAO to sector skies
};

// [RH] Compatibility flags.
enum ECompatFlags
{
	COMPATF_SHORTTEX		= 1 << 0,	// Use Doom's shortest texture around behavior?
	COMPATF_STAIRINDEX		= 1 << 1,	// Don't fix loop index for stair building?
	COMPATF_LIMITPAIN		= 1 << 2,	// Pain elemental is limited to 20 lost souls?
	COMPATF_SILENTPICKUP	= 1 << 3,	// Pickups are only heard locally?
	COMPATF_NO_PASSMOBJ		= 1 << 4,	// Pretend every actor is infinitely tall?
	COMPATF_MAGICSILENCE	= 1 << 5,	// Limit actors to one sound at a time?
	COMPATF_WALLRUN			= 1 << 6,	// Enable buggier wall clipping so players can wallrun?
	COMPATF_NOTOSSDROPS		= 1 << 7,	// Spawn dropped items directly on the floor?
	COMPATF_USEBLOCKING		= 1 << 8,	// Any special line can block a use line
	COMPATF_NODOORLIGHT		= 1 << 9,	// Don't do the BOOM local door light effect
	COMPATF_RAVENSCROLL		= 1 << 10,	// Raven's scrollers use their original carrying speed
	COMPATF_SOUNDTARGET		= 1 << 11,	// Use sector based sound target code.
	COMPATF_DEHHEALTH		= 1 << 12,	// Limit deh.MaxHealth to the health bonus (as in Doom2.exe)
	COMPATF_TRACE			= 1 << 13,	// Trace ignores lines with the same sector on both sides
	COMPATF_DROPOFF			= 1 << 14,	// Monsters cannot move when hanging over a dropoff
	COMPATF_BOOMSCROLL		= 1 << 15,	// Scrolling sectors are additive like in Boom
	COMPATF_INVISIBILITY	= 1 << 16,	// Monsters can see semi-invisible players
	COMPATF_SILENT_INSTANT_FLOORS = 1<<17,	// Instantly moving floors are not silent
	COMPATF_SECTORSOUNDS	= 1 << 18,	// Sector sounds use original method for sound origin.
	COMPATF_MISSILECLIP		= 1 << 19,	// Use original Doom heights for clipping against projectiles
	COMPATF_CROSSDROPOFF	= 1 << 20,	// monsters can't be pushed over dropoffs
	COMPATF_ANYBOSSDEATH	= 1 << 21,	// [GZ] Any monster which calls BOSSDEATH counts for level specials
	COMPATF_MINOTAUR		= 1 << 22,	// Minotaur's floor flame is exploded immediately when feet are clipped
	COMPATF_MUSHROOM		= 1 << 23,	// Force original velocity calculations for A_Mushroom in Dehacked mods.
	COMPATF_MBFMONSTERMOVE	= 1 << 24,	// Monsters are affected by friction and pushers/pullers.
	COMPATF_CORPSEGIBS		= 1 << 25,	// Crushed monsters are turned into gibs, rather than replaced by gibs.
	COMPATF_NOBLOCKFRIENDS	= 1 << 26,	// Friendly monsters aren't blocked by monster-blocking lines.
	COMPATF_SPRITESORT		= 1 << 27,	// Invert sprite sorting order for sprites of equal distance
	COMPATF_HITSCAN			= 1 << 28,	// Hitscans use original blockmap anf hit check code.
	COMPATF_LIGHT			= 1 << 29,	// Find neighboring light level like Doom
	COMPATF_POLYOBJ			= 1 << 30,	// Draw polyobjects the old fashioned way
	COMPATF_MASKEDMIDTEX	= 1 << 31,	// Ignore compositing when drawing masked midtextures

	COMPATF2_BADANGLES		= 1 << 0,	// It is impossible to face directly NSEW.
	COMPATF2_FLOORMOVE		= 1 << 1,	// Use the same floor motion behavior as Doom.
	COMPATF2_SOUNDCUTOFF	= 1 << 2,	// Cut off sounds when an actor vanishes instead of making it owner-less
	COMPATF2_POINTONLINE	= 1 << 3,	// Use original but buggy P_PointOnLineSide() and P_PointOnDivlineSideCompat()
	COMPATF2_MULTIEXIT		= 1 << 4,	// Level exit can be triggered multiple times (required by Daedalus's travel tubes, thanks to a faulty script)
	COMPATF2_TELEPORT		= 1 << 5,	// Don't let indirect teleports trigger sector actions
	COMPATF2_PUSHWINDOW		= 1 << 6,	// Disable the window check in CheckForPushSpecial()
	COMPATF2_CHECKSWITCHRANGE = 1 << 7,	// Enable buggy CheckSwitchRange behavior
	COMPATF2_EXPLODE1		= 1 << 8,	// No vertical explosion thrust
	COMPATF2_EXPLODE2		= 1 << 9,	// Use original explosion code throughout.
	COMPATF2_RAILING		= 1 << 10,	// Bugged Strife railings.
};

const M_E        = 2.7182818284590452354;  // e
const M_LOG2E    = 1.4426950408889634074;  // log_2 e
const M_LOG10E   = 0.43429448190325182765; // log_10 e
const M_LN2      = 0.69314718055994530942; // log_e 2
const M_LN10     = 2.30258509299404568402; // log_e 10
const M_PI       = 3.14159265358979323846; // pi
const M_PI_2     = 1.57079632679489661923; // pi/2
const M_PI_4     = 0.78539816339744830962; // pi/4
const M_1_PI     = 0.31830988618379067154; // 1/pi
const M_2_PI     = 0.63661977236758134308; // 2/pi
const M_2_SQRTPI = 1.12837916709551257390; // 2/sqrt(pi)
const M_SQRT2    = 1.41421356237309504880; // sqrt(2)
const M_SQRT1_2  = 0.70710678118654752440; // 1/sqrt(2)

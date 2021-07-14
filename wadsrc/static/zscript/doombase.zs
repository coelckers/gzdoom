
extend struct _
{
	native readonly Array<class<Actor> > AllActorClasses;
	native readonly Array<@PlayerClass> PlayerClasses;
	native readonly Array<@PlayerSkin> PlayerSkins;
	native readonly Array<@Team> Teams;
	native int validcount;
	native play @DehInfo deh;
	native readonly bool automapactive;
	native readonly TextureID skyflatnum;
	native readonly int gametic;
	native readonly int Net_Arbitrator;
	native ui BaseStatusBar StatusBar;
	native readonly Weapon WP_NOCHANGE;
	deprecated("3.8", "Use Actor.isFrozen() or Level.isFrozen() instead") native readonly bool globalfreeze;
	native int LocalViewPitch;
	
	// sandbox state in multi-level setups:
	native play @PlayerInfo players[MAXPLAYERS];
	native readonly bool playeringame[MAXPLAYERS];
	native play LevelLocals Level;

}

extend struct TexMan
{
	native static void SetCameraToTexture(Actor viewpoint, String texture, double fov);
	native static void SetCameraTextureAspectRatio(String texture, double aspectScale, bool useTextureRatio = true);
	deprecated("3.8", "Use Level.ReplaceTextures() instead") static void ReplaceTextures(String from, String to, int flags)
	{
		level.ReplaceTextures(from, to, flags);
	}
}

extend struct Screen
{
	native static void DrawFrame(int x, int y, int w, int h);
	// This is a leftover of the abandoned Inventory.DrawPowerup method.
	deprecated("2.5", "Use StatusBar.DrawTexture() instead") static ui void DrawHUDTexture(TextureID tex, double x, double y)
	{
		statusBar.DrawTexture(tex, (x, y), BaseStatusBar.DI_SCREEN_RIGHT_TOP, 1., (32, 32));
	}
}

extend struct Console
{
	native static void MidPrint(Font fontname, string textlabel, bool bold = false);
}

extend struct Translation
{
	Color colors[256];
	
	native int AddTranslation();
	native static bool SetPlayerTranslation(int group, int num, int plrnum, PlayerClass pclass);
	native static int GetID(Name transname);
}

// This is needed because Actor contains a field named 'translation' which shadows the above.
struct Translate version("4.5")
{
	static int MakeID(int group, int num)
	{
		return (group << 16) + num;
	}
	static bool SetPlayerTranslation(int group, int num, int plrnum, PlayerClass pclass)
	{
		return Translation.SetPlayerTranslation(group, num, plrnum, pclass);
	}
	static int GetID(Name transname)
	{
		return Translation.GetID(transname);
	}
}

struct DamageTypeDefinition native
{
	native static bool IgnoreArmor(Name type);
}

extend struct CVar
{
	native static CVar GetCVar(Name name, PlayerInfo player = null);
}

extend struct GameInfoStruct
{
	// will be extended as needed.
	native Name backpacktype;
	native double Armor2Percent;
	native String ArmorIcon1;
	native String ArmorIcon2;
	native bool norandomplayerclass;
	native Array<Name> infoPages;
	native GIFont mStatscreenMapNameFont;
	native GIFont mStatscreenEnteringFont;
	native GIFont mStatscreenFinishedFont;
	native GIFont mStatscreenContentFont;
	native GIFont mStatscreenAuthorFont;
	native double gibfactor;
	native bool intermissioncounter;
	native Color defaultbloodcolor;
	native double telefogheight;
	native int defKickback;
	native int defaultdropstyle;
	native TextureID healthpic;
	native TextureID berserkpic;
	native double normforwardmove[2];
	native double normsidemove[2];
	native bool mHideParTimes;
}

extend class Object
{
	private native static Object BuiltinNewDoom(Class<Object> cls, int outerclass, int compatibility);
	private native static int BuiltinCallLineSpecial(int special, Actor activator, int arg1, int arg2, int arg3, int arg4, int arg5);
	// These really should be global functions...
	native static String G_SkillName();
	native static int G_SkillPropertyInt(int p);
	native static double G_SkillPropertyFloat(int p);
	deprecated("3.8", "Use Level.PickDeathMatchStart() instead") static vector3, int G_PickDeathmatchStart()
	{
		return level.PickDeathmatchStart();
	}
	deprecated("3.8", "Use Level.PickPlayerStart() instead") static vector3, int G_PickPlayerStart(int pnum, int flags = 0)
	{
		return level.PickPlayerStart(pnum, flags);
	}
	deprecated("4.3", "Use S_StartSound() instead") native static void S_Sound (Sound sound_id, int channel, float volume = 1, float attenuation = ATTN_NORM, float pitch = 0.0, float startTime = 0.0);
	native static void S_StartSound (Sound sound_id, int channel, int flags = 0, float volume = 1, float attenuation = ATTN_NORM, float pitch = 0.0, float startTime = 0.0);
	native static void S_PauseSound (bool notmusic, bool notsfx);
	native static void S_ResumeSound (bool notsfx);
	native static bool S_ChangeMusic(String music_name, int order = 0, bool looping = true, bool force = false);
	native static float S_GetLength(Sound sound_id);
	native static void MarkSound(Sound snd);
	native static uint BAM(double angle);
	native static void SetMusicVolume(float vol);
}

class Thinker : Object native play
{
	enum EStatnums
	{
 		// Thinkers that don't actually think
		STAT_INFO,								// An info queue
		STAT_DECAL,								// A decal
		STAT_AUTODECAL,							// A decal that can be automatically deleted
		STAT_CORPSEPOINTER,						// An entry in Hexen's corpse queue
		STAT_TRAVELLING,						// An actor temporarily travelling to a new map
		STAT_STATIC,

		// Thinkers that do think
		STAT_FIRST_THINKING=32,
		STAT_SCROLLER=STAT_FIRST_THINKING,		// A DScroller thinker
		STAT_PLAYER,							// A player actor
		STAT_BOSSTARGET,						// A boss brain target
		STAT_LIGHTNING,							// The lightning thinker
		STAT_DECALTHINKER,						// An object that thinks for a decal
		STAT_INVENTORY,							// An inventory item
		STAT_LIGHT,								// A sector light effect
		STAT_LIGHTTRANSFER,						// A sector light transfer. These must be ticked after the light effects.
		STAT_EARTHQUAKE,						// Earthquake actors
		STAT_MAPMARKER,							// Map marker actors
		STAT_DLIGHT,							// Dynamic lights

		STAT_USER = 70,
		STAT_USER_MAX = 90,

		STAT_DEFAULT = 100,						// Thinkers go here unless specified otherwise.
		STAT_SECTOREFFECT,						// All sector effects that cause floor and ceiling movement
		STAT_ACTORMOVER,						// actor movers
		STAT_SCRIPTS,							// The ACS thinker. This is to ensure that it can't tick before all actors called PostBeginPlay
		STAT_BOT,								// Bot thinker
		MAX_STATNUM = 127
	}


	native LevelLocals Level;
	
	virtual native void Tick();
	virtual native void PostBeginPlay();
	native void ChangeStatNum(int stat);
	
	static clearscope int Tics2Seconds(int tics)
	{
		return int(tics / TICRATE);
	}

}

class ThinkerIterator : Object native
{
	native static ThinkerIterator Create(class<Object> type = "Actor", int statnum=Thinker.MAX_STATNUM+1);
	native Thinker Next(bool exact = false);
	native void Reinit();
}

class ActorIterator : Object native
{
	deprecated("3.8", "Use Level.CreateActorIterator() instead") static ActorIterator Create(int tid, class<Actor> type = "Actor")
	{
		return level.CreateActorIterator(tid, type);
	}
	native Actor Next();
	native void Reinit();
}

class BlockThingsIterator : Object native
{
	native Actor thing;
	native Vector3 position;
	native int portalflags;
	
	native static BlockThingsIterator Create(Actor origin, double checkradius = -1, bool ignorerestricted = false);
	native static BlockThingsIterator CreateFromPos(double checkx, double checky, double checkz, double checkh, double checkradius, bool ignorerestricted);
	native bool Next();
}

class BlockLinesIterator : Object native
{
	native Line CurLine;
	native Vector3 position;
	native int portalflags;
	
	native static BlockLinesIterator Create(Actor origin, double checkradius = -1);
	native static BlockLinesIterator CreateFromPos(Vector3 pos, double checkh, double checkradius, Sector sec = null);
	native bool Next();
}

enum ETraceStatus
{
	TRACE_Stop,		// stop the trace, returning this hit
	TRACE_Continue,		// continue the trace, returning this hit if there are none further along
	TRACE_Skip,		// continue the trace; do not return this hit
	TRACE_Abort		// stop the trace, returning no hits
}

enum ETraceFlags
{
	TRACE_NoSky		= 0x0001,	// Hitting the sky returns TRACE_HitNone
	//TRACE_PCross		= 0x0002,	// Trigger SPAC_PCROSS lines
	//TRACE_Impact		= 0x0004,	// Trigger SPAC_IMPACT lines
	TRACE_PortalRestrict	= 0x0008,	// Cannot go through portals without a static link offset.
	TRACE_ReportPortals	= 0x0010,	// Report any portal crossing to the TraceCallback
	//TRACE_3DCallback	= 0x0020,	// [ZZ] use TraceCallback to determine whether we need to go through a line to do 3D floor check, or not. without this, only line flag mask is used
	TRACE_HitSky		= 0x0040	// Hitting the sky returns TRACE_HasHitSky
}


enum ETraceResult
{
	TRACE_HitNone,
	TRACE_HitFloor,
	TRACE_HitCeiling,
	TRACE_HitWall,
	TRACE_HitActor,
	TRACE_CrossingPortal,
	TRACE_HasHitSky
}

enum ELineTier
{
	TIER_Middle,
	TIER_Upper,
	TIER_Lower,
	TIER_FFloor
}

struct TraceResults native
{
	native Sector HitSector; // originally called "Sector". cannot be named like this in ZScript.
	native TextureID HitTexture;
	native vector3 HitPos;
	native vector3 HitVector;
	native vector3 SrcFromTarget;
	native double SrcAngleFromTarget;

	native double Distance;
	native double Fraction;

	native Actor HitActor;		// valid if hit an actor. // originally called "Actor".

	native Line HitLine;		// valid if hit a line // originally called "Line".
	native uint8 Side;
	native uint8 Tier;		// see Tracer.ELineTier
	native bool unlinked;		// passed through a portal without static offset.

	native ETraceResult HitType;
	native F3DFloor ffloor;

	native Sector CrossedWater;		// For Boom-style, Transfer_Heights-based deep water
	native vector3 CrossedWaterPos;	// remember the position so that we can use it for spawning the splash
	native Sector Crossed3DWater;		// For 3D floor-based deep water
	native vector3 Crossed3DWaterPos;
}

class LineTracer : Object native
{
	native @TraceResults Results;
	native bool Trace(vector3 start, Sector sec, vector3 direction, double maxDist, ETraceFlags traceFlags);

	virtual ETraceStatus TraceCallback()
	{
		// Normally you would examine Results.HitType (for ETraceResult), and determine either:
		//  - stop tracing and return the entity that was found (return TRACE_Stop)
		//  - ignore some object, like noclip, e.g. only count solid walls and floors, and ignore actors (return TRACE_Skip)
		//  - find last object of some type (return TRACE_Continue)
		//  - stop tracing entirely and assume we found nothing (return TRACE_Abort)
		// TRACE_Abort and TRACE_Continue are of limited use in scripting.

		return TRACE_Stop; // default callback returns first hit, whatever it is.
	}
}

struct DropItem native
{
	native readonly DropItem Next;
	native readonly name Name;
	native readonly int Probability;
	native readonly int Amount;
}

struct LevelInfo native
{
	native readonly int levelnum;
	native readonly String MapName;
	native readonly String NextMap;
	native readonly String NextSecretMap;
	native readonly String SkyPic1;
	native readonly String SkyPic2;
	native readonly String F1Pic;
	native readonly int cluster;
	native readonly int partime;
	native readonly int sucktime;
	native readonly int flags;
	native readonly int flags2;
	native readonly int flags3;
	native readonly String Music;
	native readonly String LevelName;
	native readonly String AuthorName;
	native readonly int musicorder;
	native readonly float skyspeed1;
	native readonly float skyspeed2;
	native readonly int cdtrack;
	native readonly double gravity;
	native readonly double aircontrol;
	native readonly int airsupply;
	native readonly int compatflags;
	native readonly int compatflags2;
	native readonly name deathsequence;
	native readonly int fogdensity;
	native readonly int outsidefogdensity;
	native readonly int skyfog;
	native readonly float pixelstretch;
	native readonly name RedirectType;
	native readonly String RedirectMapName;
	native readonly double teamdamage;

	native bool isValid() const;
	native String LookupLevelName() const;

	native static int GetLevelInfoCount();
	native static LevelInfo GetLevelInfo(int index);
	native static LevelInfo FindLevelInfo(String mapname);
	native static LevelInfo FindLevelByNum(int num);
	native static bool MapExists(String mapname);
	native static String MapChecksum(String mapname);
}

struct LevelLocals native
{
	enum EUDMF
	{
		UDMF_Line,
		UDMF_Side,
		UDMF_Sector,
		//UDMF_Thing // not implemented
	};
	
	const CLUSTER_HUB = 0x00000001;	// Cluster uses hub behavior


	native Array<@Sector> Sectors;
	native Array<@Line> Lines;
	native Array<@Side> Sides;
	native readonly Array<@Vertex> Vertexes;
	native internal Array<@SectorPortal> SectorPortals;
	
	native readonly int time;
	native readonly int maptime;
	native readonly int totaltime;
	native readonly int starttime;
	native readonly int partime;
	native readonly int sucktime;
	native readonly int cluster;
	native readonly int clusterflags;
	native readonly int levelnum;
	native readonly String LevelName;
	native readonly String MapName;
	native String NextMap;
	native String NextSecretMap;
	native readonly String F1Pic;
	native readonly int maptype;
	native readonly String AuthorName;
	native readonly String Music;
	native readonly int musicorder;
	native readonly TextureID skytexture1;
	native readonly TextureID skytexture2;
	native float skyspeed1;
	native float skyspeed2;
	native int total_secrets;
	native int found_secrets;
	native int total_items;
	native int found_items;
	native int total_monsters;
	native int killed_monsters;
	native play double gravity;
	native play double aircontrol;
	native play double airfriction;
	native play int airsupply;
	native readonly double teamdamage;
	native readonly bool noinventorybar;
	native readonly bool monsterstelefrag;
	native readonly bool actownspecial;
	native readonly bool sndseqtotalctrl;
	native bool allmap;
	native readonly bool missilesactivateimpact;
	native readonly bool monsterfallingdamage;
	native readonly bool checkswitchrange;
	native readonly bool polygrind;
	native readonly bool nomonsters;
	native readonly bool allowrespawn;
	deprecated("3.8", "Use Level.isFrozen() instead") native bool frozen;
	native readonly bool infinite_flight;
	native readonly bool no_dlg_freeze;
	native readonly bool keepfullinventory;
	native readonly bool removeitems;
	native readonly int fogdensity;
	native readonly int outsidefogdensity;
	native readonly int skyfog;
	native readonly float pixelstretch;
	native readonly float MusicVolume;
	native name deathsequence;
	native readonly int compatflags;
	native readonly int compatflags2;
	native readonly LevelInfo info;

	native String GetUDMFString(int type, int index, Name key);
	native int GetUDMFInt(int type, int index, Name key);
	native double GetUDMFFloat(int type, int index, Name key);
	native play int ExecuteSpecial(int special, Actor activator, line linedef, bool lineside, int arg1 = 0, int arg2 = 0, int arg3 = 0, int arg4 = 0, int arg5 = 0);
	native void GiveSecret(Actor activator, bool printmsg = true, bool playsound = true);
	native void StartSlideshow(Name whichone = 'none');
	native static void MakeScreenShot();
	native static void MakeAutoSave();
	native void WorldDone();
	deprecated("3.8", "This function does nothing") static void RemoveAllBots(bool fromlist) { /* intentionally left as no-op. */ }
	native ui Vector2 GetAutomapPosition();
	native void SetInterMusic(String nextmap);
	native String FormatMapName(int mapnamecolor);
	native bool IsJumpingAllowed() const;
	native bool IsCrouchingAllowed() const;
	native bool IsFreelookAllowed() const;
	native void StartIntermission(Name type, int state) const;
	native play SpotState GetSpotState(bool create = true);
	native int FindUniqueTid(int start = 0, int limit = 0);
	native uint GetSkyboxPortal(Actor actor);
	native void ReplaceTextures(String from, String to, int flags);
    clearscope native HealthGroup FindHealthGroup(int id);
	native vector3, int PickDeathmatchStart();
	native vector3, int PickPlayerStart(int pnum, int flags = 0);
	native int isFrozen() const;
	native void setFrozen(bool on);

	native clearscope Sector PointInSector(Vector2 pt) const;

	native clearscope bool IsPointInLevel(vector3 p) const;
	deprecated("3.8", "Use Level.IsPointInLevel() instead") clearscope static bool IsPointInMap(vector3 p)
	{
		return level.IsPointInLevel(p);
	}

	native clearscope vector2 Vec2Diff(vector2 v1, vector2 v2) const;
	native clearscope vector3 Vec3Diff(vector3 v1, vector3 v2) const;
	native clearscope vector3 SphericalCoords(vector3 viewpoint, vector3 targetPos, vector2 viewAngles = (0, 0), bool absolute = false) const;
	
	native clearscope vector2 Vec2Offset(vector2 pos, vector2 dir, bool absolute = false) const;
	native clearscope vector3 Vec2OffsetZ(vector2 pos, vector2 dir, double atz, bool absolute = false) const;
	native clearscope vector3 Vec3Offset(vector3 pos, vector3 dir, bool absolute = false) const;

	native String GetChecksum() const;

	native void ChangeSky(TextureID sky1, TextureID sky2 );

	native SectorTagIterator CreateSectorTagIterator(int tag, line defline = null);
	native LineIdIterator CreateLineIdIterator(int tag);
	native ActorIterator CreateActorIterator(int tid, class<Actor> type = "Actor");

	String TimeFormatted(bool totals = false)
	{
		int sec = Thinker.Tics2Seconds(totals? totaltime : time); 
		return String.Format("%02d:%02d:%02d", sec / 3600, (sec % 3600) / 60, sec % 60);
	}

	native play bool CreateCeiling(sector sec, int type, line ln, double speed, double speed2, double height = 0, int crush = -1, int silent = 0, int change = 0, int crushmode = 0 /*Floor.crushDoom*/);
	native play bool CreateFloor(sector sec, int floortype, line ln, double speed, double height = 0, int crush = -1, int change = 0, bool crushmode = false, bool hereticlower = false);

	native void ExitLevel(int position, bool keepFacing);
	native void SecretExitLevel(int position);
	native void ChangeLevel(string levelname, int position = 0, int flags = 0, int skill = -1);
}

// a few values of this need to be readable by the play code.
// Most are handled at load time and are omitted here.
struct DehInfo native
{
	native readonly int MaxSoulsphere;
	native readonly uint8 ExplosionStyle;
	native readonly double ExplosionAlpha;
	native readonly int NoAutofreeze;
	native readonly int BFGCells;
	native readonly int BlueAC;
	native readonly int MaxHealth;
}

struct State native
{
	native readonly State NextState;
	native readonly int sprite;
	native readonly int16 Tics;
	native readonly uint16 TicRange;
	native readonly uint8 Frame;		
	native readonly uint8 UseFlags;	
	native readonly int Misc1;
	native readonly int Misc2;
	native readonly uint16 bSlow;
	native readonly uint16 bFast;
	native readonly bool bFullbright;
	native readonly bool bNoDelay;
	native readonly bool bSameFrame;
	native readonly bool bCanRaise;
	native readonly bool bDehacked;
	
	native int DistanceTo(state other);
	native bool ValidateSpriteFrame();
	native TextureID, bool, Vector2 GetSpriteTexture(int rotation, int skin = 0, Vector2 scale = (0,0));
	native bool InStateSequence(State base);
}

struct TerrainDef native
{
	native Name TerrainName;
	native int Splash;
	native int DamageAmount;
	native Name DamageMOD;
	native int DamageTimeMask;
	native double FootClip;
	native float StepVolume;
	native int WalkStepTics;
	native int RunStepTics;
	native Sound LeftStepSound;
	native Sound RightStepSound;
	native bool IsLiquid;
	native bool AllowProtection;
	native bool DamageOnLand;
	native double Friction;
	native double MoveFactor;
};

enum EPickStart
{
	PPS_FORCERANDOM			= 1,
	PPS_NOBLOCKINGCHECK		= 2,
}


class SectorEffect : Thinker native
{
	native protected Sector m_Sector;

	native Sector GetSector();
}

class Mover : SectorEffect native
{}

class MovingFloor : Mover native
{}

class MovingCeiling : Mover native
{}

class Floor : MovingFloor native
{
	// only here so that some constants and functions can be added. Not directly usable yet.
	enum EFloor
	{
		floorLowerToLowest,
		floorLowerToNearest,
		floorLowerToHighest,
		floorLowerByValue,
		floorRaiseByValue,
		floorRaiseToHighest,
		floorRaiseToNearest,
		floorRaiseAndCrush,
		floorRaiseAndCrushDoom,
		floorCrushStop,
		floorLowerInstant,
		floorRaiseInstant,
		floorMoveToValue,
		floorRaiseToLowestCeiling,
		floorRaiseByTexture,

		floorLowerAndChange,
		floorRaiseAndChange,

		floorRaiseToLowest,
		floorRaiseToCeiling,
		floorLowerToLowestCeiling,
		floorLowerByTexture,
		floorLowerToCeiling,

		donutRaise,

		buildStair,
		waitStair,
		resetStair,

		// Not to be used as parameters to DoFloor()
		genFloorChg0,
		genFloorChgT,
		genFloorChg
	};

	deprecated("3.8", "Use Level.CreateFloor() instead") static bool CreateFloor(sector sec, int floortype, line ln, double speed, double height = 0, int crush = -1, int change = 0, bool crushmode = false, bool hereticlower = false)
	{
		return level.CreateFloor(sec, floortype, ln, speed, height, crush, change, crushmode, hereticlower);
	}
}

class Ceiling : MovingCeiling native
{
	enum ECeiling
	{
		ceilLowerByValue,
		ceilRaiseByValue,
		ceilMoveToValue,
		ceilLowerToHighestFloor,
		ceilLowerInstant,
		ceilRaiseInstant,
		ceilCrushAndRaise,
		ceilLowerAndCrush,
		ceil_placeholder,
		ceilCrushRaiseAndStay,
		ceilRaiseToNearest,
		ceilLowerToLowest,
		ceilLowerToFloor,

		// The following are only used by Generic_Ceiling
		ceilRaiseToHighest,
		ceilLowerToHighest,
		ceilRaiseToLowest,
		ceilLowerToNearest,
		ceilRaiseToHighestFloor,
		ceilRaiseToFloor,
		ceilRaiseByTexture,
		ceilLowerByTexture,

		genCeilingChg0,
		genCeilingChgT,
		genCeilingChg
	}

	enum ECrushMode
	{
		crushDoom = 0,
		crushHexen = 1,
		crushSlowdown = 2
	}
	
	deprecated("3.8", "Use Level.CreateCeiling() instead") static bool CreateCeiling(sector sec, int type, line ln, double speed, double speed2, double height = 0, int crush = -1, int silent = 0, int change = 0, int crushmode = crushDoom)
	{
		return level.CreateCeiling(sec, type, ln, speed, speed2, height, crush, silent, change, crushmode);
	}
	
}

struct LookExParams
{
	double Fov;
	double minDist;
	double maxDist;
	double maxHeardist;
	int flags;
	State seestate;
};

class Lighting : SectorEffect native
{
}

struct Shader native
{
	native clearscope static void SetEnabled(PlayerInfo player, string shaderName, bool enable);
	native clearscope static void SetUniform1f(PlayerInfo player, string shaderName, string uniformName, float value);
	native clearscope static void SetUniform2f(PlayerInfo player, string shaderName, string uniformName, vector2 value);
	native clearscope static void SetUniform3f(PlayerInfo player, string shaderName, string uniformName, vector3 value);
	native clearscope static void SetUniform1i(PlayerInfo player, string shaderName, string uniformName, int value);
}

struct FRailParams
{
	native int damage;
	native double offset_xy;
	native double offset_z;
	native int color1, color2;
	native double maxdiff;
	native int flags;
	native Class<Actor> puff;
	native double angleoffset;
	native double pitchoffset;
	native double distance;
	native int duration;
	native double sparsity;
	native double drift;
	native Class<Actor> spawnclass;
	native int SpiralOffset;
	native int limit;
};	// [RH] Shoot a railgun


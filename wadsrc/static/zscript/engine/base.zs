

// constants for A_PlaySound
enum ESoundFlags
{
	CHAN_AUTO = 0,
	CHAN_WEAPON = 1,
	CHAN_VOICE = 2,
	CHAN_ITEM = 3,
	CHAN_BODY = 4,
	CHAN_5 = 5,
	CHAN_6 = 6,
	CHAN_7 = 7,
	
	// modifier flags
	CHAN_LISTENERZ = 8,
	CHAN_MAYBE_LOCAL = 16,
	CHAN_UI = 32,
	CHAN_NOPAUSE = 64,
	CHAN_LOOP = 256,
	CHAN_PICKUP = (CHAN_ITEM|CHAN_MAYBE_LOCAL), // Do not use this with A_StartSound! It would not do what is expected.
	CHAN_NOSTOP = 4096,
	CHAN_OVERLAP = 8192,

	// Same as above, with an F appended to allow better distinction of channel and channel flags.
	CHANF_DEFAULT = 0,	// just to make the code look better and avoid literal 0's.
	CHANF_LISTENERZ = 8,
	CHANF_MAYBE_LOCAL = 16,
	CHANF_UI = 32,
	CHANF_NOPAUSE = 64,
	CHANF_LOOP = 256,
	CHANF_NOSTOP = 4096,
	CHANF_OVERLAP = 8192,
	CHANF_LOCAL = 16384,


	CHANF_LOOPING = CHANF_LOOP | CHANF_NOSTOP, // convenience value for replicating the old 'looping' boolean.

};

// sound attenuation values
const ATTN_NONE = 0;
const ATTN_NORM = 1;
const ATTN_IDLE = 1.001;
const ATTN_STATIC = 3;


enum ERenderStyle
{
	STYLE_None,				// Do not draw
	STYLE_Normal,			// Normal; just copy the image to the screen
	STYLE_Fuzzy,			// Draw silhouette using "fuzz" effect
	STYLE_SoulTrans,		// Draw translucent with amount in r_transsouls
	STYLE_OptFuzzy,			// Draw as fuzzy or translucent, based on user preference
	STYLE_Stencil,			// Fill image interior with alphacolor
	STYLE_Translucent,		// Draw translucent
	STYLE_Add,				// Draw additive
	STYLE_Shaded,			// Treat patch data as alpha values for alphacolor
	STYLE_TranslucentStencil,
	STYLE_Shadow,
	STYLE_Subtract,			// Actually this is 'reverse subtract' but this is what normal people would expect by 'subtract'.
	STYLE_AddStencil,		// Fill image interior with alphacolor
	STYLE_AddShaded,		// Treat patch data as alpha values for alphacolor
	STYLE_Multiply,			// Multiply source with destination (HW renderer only.)
	STYLE_InverseMultiply,	// Multiply source with inverse of destination (HW renderer only.)
	STYLE_ColorBlend,		// Use color intensity as transparency factor
	STYLE_Source,			// No blending (only used internally)
	STYLE_ColorAdd,			// Use color intensity as transparency factor and blend additively.

};


enum EGameState
{
	GS_LEVEL,
	GS_INTERMISSION,
	GS_FINALE,
	GS_DEMOSCREEN,
	GS_MENUSCREEN = GS_DEMOSCREEN,
	GS_FULLCONSOLE,
	GS_HIDECONSOLE,
	GS_STARTUP,
	GS_TITLELEVEL,
}

const TEXTCOLOR_BRICK			= "\034A";
const TEXTCOLOR_TAN				= "\034B";
const TEXTCOLOR_GRAY			= "\034C";
const TEXTCOLOR_GREY			= "\034C";
const TEXTCOLOR_GREEN			= "\034D";
const TEXTCOLOR_BROWN			= "\034E";
const TEXTCOLOR_GOLD			= "\034F";
const TEXTCOLOR_RED				= "\034G";
const TEXTCOLOR_BLUE			= "\034H";
const TEXTCOLOR_ORANGE			= "\034I";
const TEXTCOLOR_WHITE			= "\034J";
const TEXTCOLOR_YELLOW			= "\034K";
const TEXTCOLOR_UNTRANSLATED	= "\034L";
const TEXTCOLOR_BLACK			= "\034M";
const TEXTCOLOR_LIGHTBLUE		= "\034N";
const TEXTCOLOR_CREAM			= "\034O";
const TEXTCOLOR_OLIVE			= "\034P";
const TEXTCOLOR_DARKGREEN		= "\034Q";
const TEXTCOLOR_DARKRED			= "\034R";
const TEXTCOLOR_DARKBROWN		= "\034S";
const TEXTCOLOR_PURPLE			= "\034T";
const TEXTCOLOR_DARKGRAY		= "\034U";
const TEXTCOLOR_CYAN			= "\034V";
const TEXTCOLOR_ICE				= "\034W";
const TEXTCOLOR_FIRE			= "\034X";
const TEXTCOLOR_SAPPHIRE		= "\034Y";
const TEXTCOLOR_TEAL			= "\034Z";

const TEXTCOLOR_NORMAL			= "\034-";
const TEXTCOLOR_BOLD			= "\034+";

const TEXTCOLOR_CHAT			= "\034*";
const TEXTCOLOR_TEAMCHAT		= "\034!";


enum EMonospacing
{
	Mono_Off = 0,
	Mono_CellLeft = 1,
	Mono_CellCenter = 2,
	Mono_CellRight = 3
};

enum EPrintLevel
{
	PRINT_LOW,		// pickup messages
	PRINT_MEDIUM,	// death messages
	PRINT_HIGH,		// critical messages
	PRINT_CHAT,		// chat messages
	PRINT_TEAMCHAT,	// chat messages from a teammate
	PRINT_LOG,		// only to logfile
	PRINT_BOLD = 200,				// What Printf_Bold used
	PRINT_TYPES = 1023,		// Bitmask.
	PRINT_NONOTIFY = 1024,	// Flag - do not add to notify buffer
	PRINT_NOLOG = 2048,		// Flag - do not print to log file
};

/*
// These are here to document the intrinsic methods and fields available on
// the built-in ZScript types
struct Vector2
{
	Vector2(x, y);
	double x, y;
	native double Length();
	native Vector2 Unit();
	// The dot product of two vectors can be calculated like this:
	// double d = a dot b;
}

struct Vector3
{
	Vector3(x, y, z);
	double x, y, z;
	Vector2 xy; // Convenient access to the X and Y coordinates of a 3D vector
	native double Length();
	native Vector3 Unit();
	// The dot product of two vectors can be calculated like this:
	// double d = a dot b;
	// The cross product of two vectors can be calculated like this:
	// Vector3 d = a cross b;
}
*/

struct _ native	// These are the global variables, the struct is only here to avoid extending the parser for this.
{
	native readonly Array<class> AllClasses;
	native readonly bool multiplayer;
	native @KeyBindings Bindings;
	native @KeyBindings AutomapBindings;
	native readonly @GameInfoStruct gameinfo;
	native readonly ui bool netgame;
	native readonly uint gameaction;
	native readonly int gamestate;
	native readonly Font smallfont;
	native readonly Font smallfont2;
	native readonly Font bigfont;
	native readonly Font confont;
	native readonly Font NewConsoleFont;
	native readonly Font NewSmallFont;
	native readonly Font AlternativeSmallFont;
	native readonly Font AlternativeBigFont;
	native readonly Font OriginalSmallFont;
	native readonly Font OriginalBigFont;
	native readonly Font intermissionfont;
	native readonly int CleanXFac;
	native readonly int CleanYFac;
	native readonly int CleanWidth;
	native readonly int CleanHeight;
	native readonly int CleanXFac_1;
	native readonly int CleanYFac_1;
	native readonly int CleanWidth_1;
	native readonly int CleanHeight_1;
	native ui int menuactive;
	native readonly @FOptionMenuSettings OptionMenuSettings;
	native readonly bool demoplayback;
	native ui int BackbuttonTime;
	native ui float BackbuttonAlpha;
	native readonly @MusPlayingInfo musplaying;
	native readonly bool generic_ui;
	native readonly int GameTicRate;
	native MenuDelegateBase menuDelegate;
	native readonly int consoleplayer;
	native readonly double NotifyFontScale;
}

struct MusPlayingInfo native
{
	native String name;
	native int baseorder;
	native bool loop;
	native voidptr handle;
	
};

struct TexMan
{
	enum EUseTypes
	{
		Type_Any,
		Type_Wall,
		Type_Flat,
		Type_Sprite,
		Type_WallPatch,
		Type_Build,
		Type_SkinSprite,
		Type_Decal,
		Type_MiscPatch,
		Type_FontChar,
		Type_Override,	// For patches between TX_START/TX_END
		Type_Autopage,	// Automap background - used to enable the use of FAutomapTexture
		Type_SkinGraphic,
		Type_Null,
		Type_FirstDefined,
	};

	enum EFlags
	{
		TryAny = 1,
		Overridable = 2,
		ReturnFirst = 4,
		AllowSkins = 8,
		ShortNameOnly = 16,
		DontCreate = 32,
		Localize = 64,
		ForceLookup = 128,
		NoAlias = 256
	};
	
	enum ETexReplaceFlags
	{
		NOT_BOTTOM			= 1,
		NOT_MIDDLE			= 2,
		NOT_TOP				= 4,
		NOT_FLOOR			= 8,
		NOT_CEILING			= 16,
		NOT_WALL			= 7,
		NOT_FLAT			= 24
	};

	native static TextureID CheckForTexture(String name, int usetype = Type_Any, int flags = TryAny);
	native static String GetName(TextureID tex);
	native static int, int GetSize(TextureID tex);
	native static Vector2 GetScaledSize(TextureID tex);
	native static Vector2 GetScaledOffset(TextureID tex);
	native static int CheckRealHeight(TextureID tex);
	native static bool OkForLocalization(TextureID patch, String textSubstitute);
	native static bool UseGamePalette(TextureID tex);
}

/*
// Intrinsic TextureID methods
// This isn't really a class, and can be used as an integer
struct TextureID
{
	native bool IsValid();
	native bool IsNull();
	native bool Exists();
	native void SetInvalid();
	native void SetNull();
}

// 32-bit RGBA color - each component is one byte, or 8-bit
// This isn't really a class, and can be used as an integer
struct Color
{
	// Constructor - alpha channel is optional
	Color(int alpha, int red, int green, int blue);
	Color(int red, int green, int blue); // Alpha is 0 if omitted
	int r; // Red
	int g; // Green
	int b; // Blue
	int a; // Alpha
}

// Name - a string with an integer ID
struct Name
{
	Name(Name name);
	Name(String name);
}

// Sound ID - can be created by casting from a string (name from SNDINFO) or an
// integer (sound ID as integer).
struct Sound
{
	Sound(String soundName);
	Sound(int id);
}
*/

enum EScaleMode
{
	FSMode_None = 0,
	FSMode_ScaleToFit = 1,
	FSMode_ScaleToFill = 2,
	FSMode_ScaleToFit43 = 3,
	FSMode_ScaleToScreen = 4,
	FSMode_ScaleToFit43Top = 5,
	FSMode_ScaleToFit43Bottom = 6,
	FSMode_ScaleToHeight = 7,


	FSMode_Max,

	// These all use ScaleToFit43, their purpose is to cut down on verbosity because they imply the virtual screen size.
	FSMode_Predefined = 1000,
	FSMode_Fit320x200 = 1000,
	FSMode_Fit320x240,
	FSMode_Fit640x400,
	FSMode_Fit640x480,
	FSMode_Fit320x200Top,
	FSMode_Predefined_Max,
};

enum DrawTextureTags
{
	TAG_USER = (1<<30),
	DTA_Base = TAG_USER + 5000,
	DTA_DestWidth,		// width of area to draw to
	DTA_DestHeight,		// height of area to draw to
	DTA_Alpha,			// alpha value for translucency
	DTA_FillColor,		// color to stencil onto the destination (RGB is the color for truecolor drawers, A is the palette index for paletted drawers)
	DTA_TranslationIndex, // translation table to recolor the source
	DTA_AlphaChannel,	// bool: the source is an alpha channel; used with DTA_FillColor
	DTA_Clean,			// bool: scale texture size and position by CleanXfac and CleanYfac
	DTA_320x200,		// bool: scale texture size and position to fit on a virtual 320x200 screen
	DTA_Bottom320x200,	// bool: same as DTA_320x200 but centers virtual screen on bottom for 1280x1024 targets
	DTA_CleanNoMove,	// bool: like DTA_Clean but does not reposition output position
	DTA_CleanNoMove_1,	// bool: like DTA_CleanNoMove, but uses Clean[XY]fac_1 instead
	DTA_FlipX,			// bool: flip image horizontally	//FIXME: Does not work with DTA_Window(Left|Right)
	DTA_ShadowColor,	// color of shadow
	DTA_ShadowAlpha,	// alpha of shadow
	DTA_Shadow,			// set shadow color and alphas to defaults
	DTA_VirtualWidth,	// pretend the canvas is this wide
	DTA_VirtualHeight,	// pretend the canvas is this tall
	DTA_TopOffset,		// override texture's top offset
	DTA_LeftOffset,		// override texture's left offset
	DTA_CenterOffset,	// bool: override texture's left and top offsets and set them for the texture's middle
	DTA_CenterBottomOffset,// bool: override texture's left and top offsets and set them for the texture's bottom middle
	DTA_WindowLeft,		// don't draw anything left of this column (on source, not dest)
	DTA_WindowRight,	// don't draw anything at or to the right of this column (on source, not dest)
	DTA_ClipTop,		// don't draw anything above this row (on dest, not source)
	DTA_ClipBottom,		// don't draw anything at or below this row (on dest, not source)
	DTA_ClipLeft,		// don't draw anything to the left of this column (on dest, not source)
	DTA_ClipRight,		// don't draw anything at or to the right of this column (on dest, not source)
	DTA_Masked,			// true(default)=use masks from texture, false=ignore masks
	DTA_HUDRules,		// use fullscreen HUD rules to position and size textures
	DTA_HUDRulesC,		// only used internally for marking HUD_HorizCenter
	DTA_KeepRatio,		// doesn't adjust screen size for DTA_Virtual* if the aspect ratio is not 4:3
	DTA_RenderStyle,	// same as render style for actors
	DTA_ColorOverlay,	// DWORD: ARGB to overlay on top of image; limited to black for software
	DTA_Internal1,
	DTA_Internal2,
	DTA_Desaturate,		// explicit desaturation factor (does not do anything in Legacy OpenGL)
	DTA_Fullscreen,		// Draw image fullscreen (same as DTA_VirtualWidth/Height with graphics size.)

	// floating point duplicates of some of the above:
	DTA_DestWidthF,
	DTA_DestHeightF,
	DTA_TopOffsetF,
	DTA_LeftOffsetF,
	DTA_VirtualWidthF,
	DTA_VirtualHeightF,
	DTA_WindowLeftF,
	DTA_WindowRightF,

	// For DrawText calls only:
	DTA_TextLen,		// stop after this many characters, even if \0 not hit
	DTA_CellX,			// horizontal size of character cell
	DTA_CellY,			// vertical size of character cell

	DTA_Color,
	DTA_FlipY,			// bool: flip image vertically
	DTA_SrcX,			// specify a source rectangle (this supersedes the poorly implemented DTA_WindowLeft/Right
	DTA_SrcY,
	DTA_SrcWidth,
	DTA_SrcHeight,
	DTA_LegacyRenderStyle,	// takes an old-style STYLE_* constant instead of an FRenderStyle
	DTA_Internal3,
	DTA_Spacing,			// Strings only: Additional spacing between characters
	DTA_Monospace,			// Strings only: Use a fixed distance between characters.

	DTA_FullscreenEx,		// advanced fullscreen control.
	DTA_FullscreenScale,	// enable DTA_Fullscreen coordinate calculation for placed overlays.

	DTA_ScaleX,
	DTA_ScaleY,

	DTA_ViewportX,			// Defines the viewport on the screen that should be rendered to.
	DTA_ViewportY,
	DTA_ViewportWidth,
	DTA_ViewportHeight,
	DTA_CenterOffsetRel,	// Apply texture offsets relative to center, instead of top left. This is standard alignment for Build's 2D content.
	DTA_TopLeft,			// always align to top left. Added to have a boolean condition for this alignment.
	DTA_Pin,				// Pin a non-widescreen image to the left/right edge of the screen.
	DTA_Rotate,
	DTA_FlipOffsets,		// Flips offsets when using DTA_FlipX and DTA_FlipY, this cannot be automatic due to unexpected behavior with unoffsetted graphics.
	DTA_Indexed,			// Use an indexed texture combined with the given translation.
	DTA_CleanTop,			// Like DTA_Clean but aligns to the top of the screen instead of the center.
	DTA_NoOffset,			// Ignore 2D drawer's offset.

};

class Shape2DTransform : Object native
{
	native void Clear();
	native void Rotate(double angle);
	native void Scale(Vector2 scaleVec);
	native void Translate(Vector2 translateVec);
}

class Shape2D : Object native
{
	enum EClearWhich
	{
		C_Verts = 1,
		C_Coords = 2,
		C_Indices = 4,
	};

	native void SetTransform(Shape2DTransform transform);

	native void Clear( int which = C_Verts|C_Coords|C_Indices );
	native void PushVertex( Vector2 v );
	native void PushCoord( Vector2 c );
	native void PushTriangle( int a, int b, int c );
}

struct Screen native
{
	native static Color PaletteColor(int index);
	native static int GetWidth();
	native static int GetHeight();
	native static void Clear(int left, int top, int right, int bottom, Color color, int palcolor = -1);
	native static void Dim(Color col, double amount, int x, int y, int w, int h);

	native static vararg void DrawTexture(TextureID tex, bool animate, double x, double y, ...);
	native static vararg void DrawShape(TextureID tex, bool animate, Shape2D s, ...);
	native static vararg void DrawChar(Font font, int normalcolor, double x, double y, int character, ...);
	native static vararg void DrawText(Font font, int normalcolor, double x, double y, String text, ...);
	native static void DrawLine(int x0, int y0, int x1, int y1, Color color, int alpha = 255);
	native static void DrawLineFrame(Color color, int x0, int y0, int w, int h, int thickness = 1);
	native static void DrawThickLine(int x0, int y0, int x1, int y1, double thickness, Color color, int alpha = 255);
	native static Vector2, Vector2 VirtualToRealCoords(Vector2 pos, Vector2 size, Vector2 vsize, bool vbottom=false, bool handleaspect=true);
	native static double GetAspectRatio();
	native static void SetClipRect(int x, int y, int w, int h);
	native static void ClearClipRect();
	native static int, int, int, int GetClipRect();
	native static int, int, int, int GetViewWindow();
	native static double, double, double, double GetFullscreenRect(double vwidth, double vheight, int fsmode);
	native static Vector2 SetOffset(double x, double y);
	native static void ClearScreen(color col = 0);
	native static void SetScreenFade(double factor);
}

struct Font native
{
	enum EColorRange
	{
		CR_UNDEFINED = -1,
		CR_BRICK,
		CR_TAN,
		CR_GRAY,
		CR_GREY = CR_GRAY,
		CR_GREEN,
		CR_BROWN,
		CR_GOLD,
		CR_RED,
		CR_BLUE,
		CR_ORANGE,
		CR_WHITE,
		CR_YELLOW,
		CR_UNTRANSLATED,
		CR_BLACK,
		CR_LIGHTBLUE,
		CR_CREAM,
		CR_OLIVE,
		CR_DARKGREEN,
		CR_DARKRED,
		CR_DARKBROWN,
		CR_PURPLE,
		CR_DARKGRAY,
		CR_CYAN,
		CR_ICE,
		CR_FIRE,
		CR_SAPPHIRE,
		CR_TEAL,
		NUM_TEXT_COLORS
	};
	
	const TEXTCOLOR_BRICK			= "\034A";
	const TEXTCOLOR_TAN				= "\034B";
	const TEXTCOLOR_GRAY			= "\034C";
	const TEXTCOLOR_GREY			= "\034C";
	const TEXTCOLOR_GREEN			= "\034D";
	const TEXTCOLOR_BROWN			= "\034E";
	const TEXTCOLOR_GOLD			= "\034F";
	const TEXTCOLOR_RED				= "\034G";
	const TEXTCOLOR_BLUE			= "\034H";
	const TEXTCOLOR_ORANGE			= "\034I";
	const TEXTCOLOR_WHITE			= "\034J";
	const TEXTCOLOR_YELLOW			= "\034K";
	const TEXTCOLOR_UNTRANSLATED	= "\034L";
	const TEXTCOLOR_BLACK			= "\034M";
	const TEXTCOLOR_LIGHTBLUE		= "\034N";
	const TEXTCOLOR_CREAM			= "\034O";
	const TEXTCOLOR_OLIVE			= "\034P";
	const TEXTCOLOR_DARKGREEN		= "\034Q";
	const TEXTCOLOR_DARKRED			= "\034R";
	const TEXTCOLOR_DARKBROWN		= "\034S";
	const TEXTCOLOR_PURPLE			= "\034T";
	const TEXTCOLOR_DARKGRAY		= "\034U";
	const TEXTCOLOR_CYAN			= "\034V";
	const TEXTCOLOR_ICE				= "\034W";
	const TEXTCOLOR_FIRE			= "\034X";
	const TEXTCOLOR_SAPPHIRE		= "\034Y";
	const TEXTCOLOR_TEAL			= "\034Z";

	const TEXTCOLOR_NORMAL			= "\034-";
	const TEXTCOLOR_BOLD			= "\034+";

	const TEXTCOLOR_CHAT			= "\034*";
	const TEXTCOLOR_TEAMCHAT		= "\034!";
	// native Font(const String name);  // String/name to font casts
	// native Font(const Name name);

	native int GetCharWidth(int code);
	native int StringWidth(String code);
	native int GetMaxAscender(String code);
	native bool CanPrint(String code);
	native int GetHeight();
	native int GetDisplacement();
	native String GetCursor();

	native static int FindFontColor(Name color);
	native double GetBottomAlignOffset(int code);
	native static Font FindFont(Name fontname);
	native static Font GetFont(Name fontname);
	native BrokenLines BreakLines(String text, int maxlen);
	native int GetGlyphHeight(int code);
}

struct Console native
{
	native static void HideConsole();
	native static vararg void Printf(string fmt, ...);
}

struct CVar native
{
	enum ECVarType
	{
		CVAR_Bool,
		CVAR_Int,
		CVAR_Float,
		CVAR_String,
		CVAR_Color,
	};

	native static CVar FindCVar(Name name);
	bool GetBool() { return GetInt(); }
	native int GetInt();
	native double GetFloat();
	native String GetString();
	void SetBool(bool b) { SetInt(b); }
	native void SetInt(int v);
	native void SetFloat(double v);
	native void SetString(String s);
	native int GetRealType();
	native int ResetToDefault();
}

struct GIFont version("2.4")
{
	Name fontname;
	Name color;
};

struct GameInfoStruct native
{
	native int gametype;
	native String mBackButton;
	native Name mSliderColor;
	native Name mSliderBackColor;
}

struct SystemTime
{
	native static ui int Now(); // This returns the epoch time
	native static clearscope String Format(String timeForm, int timeVal); // This converts an epoch time to a local time, then uses the strftime syntax to format it
}

class Object native
{
	const TICRATE = 35;
	native bool bDestroyed;

	// These must be defined in some class, so that the compiler can find them. Object is just fine, as long as they are private to external code.
	private native static Object BuiltinNew(Class<Object> cls, int outerclass, int compatibility);
	private native static int BuiltinRandom(voidptr rng, int min, int max);
	private native static double BuiltinFRandom(voidptr rng, double min, double max);
	private native static int BuiltinRandom2(voidptr rng, int mask);
	private native static void BuiltinRandomSeed(voidptr rng, int seed);
	private native static Class<Object> BuiltinNameToClass(Name nm, Class<Object> filter);
	private native static Object BuiltinClassCast(Object inptr, Class<Object> test);
	
	native static uint MSTime();
	native vararg static void ThrowAbortException(String fmt, ...);

	native virtualscope void Destroy();

	// This does not call into the native method of the same name to avoid problems with objects that get garbage collected late on shutdown.
	virtual virtualscope void OnDestroy() {}
	//
	// Object intrinsics
	// Every ZScript "class" inherits from Object, and so inherits these methods as well
	// clearscope bool IsAbstract(); // Intrinsic - Query whether or not the class of this object is abstract
	// clearscope Object GetParentClass(); // Intrinsic - Get the parent class of this object
	// clearscope Name GetClassName(); // Intrinsic - Get the name of this object's class
	// clearscope Class<Object> GetClass(); // Intrinsic - Get the object's class
	// clearscope Object new(class<Object> type); // Intrinsic - Create a new object with this class. This is only valid for thinkers and plain objects, except menus. For actors, use Actor.Spawn();
	//
	//
	// Intrinsic random number generation functions. Note that the square
	// bracket syntax for specifying an RNG ID is only available for these
	// functions.
	// clearscope void SetRandomSeed[Name rngId = 'None'](int seed); // Intrinsic - Set the seed for the given RNG.
	// clearscope int Random[Name rngId = 'None'](int min, int max); // Intrinsic - Use the given RNG to generate a random integer number in the range (min, max) inclusive.
	// clearscope int Random2[Name rngId = 'None'](int mask); // Intrinsic - Use the given RNG to generate a random integer number, and do a "union" (bitwise AND, AKA &) operation with the bits in the mask integer.
	// clearscope int FRandom[Name rngId = 'None'](double min, double max); // Intrinsic - Use the given RNG to generate a random real number in the range (min, max) inclusive.
	// clearscope int RandomPick[Name rngId = 'None'](int choices...); // Intrinsic - Use the given RNG to generate a random integer from the given choices.
	// clearscope double FRandomPick[Name rngId = 'None'](double choices...); // Intrinsic - Use the given RNG to generate a random real number from the given choices.
	//
	//
	// Intrinsic math functions - the argument and return types for these
	// functions depend on the arguments given. Other than that, they work the
	// same way similarly-named functions in other programming languages work.
	// Note that trigonometric functions work with degrees instead of radians
	// clearscope T abs(T x);
	// clearscope T atan2(T y, T x); // NOTE: Returns a value in degrees instead of radians
	// clearscope T vectorangle(T x, T y); // Same as Atan2 with the arguments in a different order
	// clearscope T min(T x...);
	// clearscope T max(T x...);
	// clearscope T clamp(T x, T min, T max);
	//
	// These math functions only work with doubles - they are defined in FxFlops
	// clearscope double exp(double x);
	// clearscope double log(double x);
	// clearscope double log10(double x);
	// clearscope double sqrt(double x);
	// clearscope double ceil(double x);
	// clearscope double floor(double x);
	// clearscope double acos(double x);
	// clearscope double asin(double x);
	// clearscope double atan(double x);
	// clearscope double cos(double x);
	// clearscope double sin(double x);
	// clearscope double tan(double x);
	// clearscope double cosh(double x);
	// clearscope double sinh(double x);
	// clearscope double tanh(double x);
	// clearscope double round(double x);
}

class BrokenLines : Object native version("2.4")
{
	native int Count();
	native int StringWidth(int line);
	native String StringAt(int line);
}

struct StringTable native
{
	native static String Localize(String val, bool prefixed = true);
}

struct Wads	// todo: make FileSystem an alias to 'Wads'
{
	enum WadNamespace
	{
		ns_hidden = -1,

		ns_global = 0,
		ns_sprites,
		ns_flats,
		ns_colormaps,
		ns_acslibrary,
		ns_newtextures,
		ns_bloodraw,
		ns_bloodsfx,
		ns_bloodmisc,
		ns_strifevoices,
		ns_hires,
		ns_voxels,

		ns_specialzipdirectory,
		ns_sounds,
		ns_patches,
		ns_graphics,
		ns_music,

		ns_firstskin,
	}

	enum FindLumpNamespace
	{
		GlobalNamespace = 0,
		AnyNamespace = 1,
	}

	native static int CheckNumForName(string name, int ns, int wadnum = -1, bool exact = false);
	native static int CheckNumForFullName(string name);
	native static int FindLump(string name, int startlump = 0, FindLumpNamespace ns = GlobalNamespace);
	native static string ReadLump(int lump);

	native static int GetNumLumps();
	native static string GetLumpName(int lump);
	native static string GetLumpFullName(int lump);
	native static int GetLumpNamespace(int lump);
}

enum EmptyTokenType
{
	TOK_SKIPEMPTY = 0,
	TOK_KEEPEMPTY = 1,
}

// Although String is a builtin type, this is a convenient way to attach methods to it.
// All of these methods are available on strings
struct StringStruct native
{
	native static vararg String Format(String fmt, ...);
	native vararg void AppendFormat(String fmt, ...);
	// native int Length();  // Intrinsic
	// native bool operator~==(String other)  // Case-insensitive equality comparison
	// native String operator..(String other)  // Concatenate with another String
	native void Replace(String pattern, String replacement);
	native String Left(int len) const;
	native String Mid(int pos = 0, int len = 2147483647) const;
	native void Truncate(int newlen);
	native void Remove(int index, int remlen);
	deprecated("4.1", "use Left() or Mid() instead") native String CharAt(int pos) const;
	deprecated("4.1", "use ByteAt() instead") native int CharCodeAt(int pos) const;
	native int ByteAt(int pos) const;
	native String Filter();
	native int IndexOf(String substr, int startIndex = 0) const;
	deprecated("3.5.1", "use RightIndexOf() instead") native int LastIndexOf(String substr, int endIndex = 2147483647) const;
	native int RightIndexOf(String substr, int endIndex = 2147483647) const;
	deprecated("4.1", "use MakeUpper() instead") native void ToUpper();
	deprecated("4.1", "use MakeLower() instead") native void ToLower();
	native String MakeUpper() const;
	native String MakeLower() const;
	native static int CharUpper(int ch);
	native static int CharLower(int ch);
	native int ToInt(int base = 0) const;
	native double ToDouble() const;
	native void Split(out Array<String> tokens, String delimiter, EmptyTokenType keepEmpty = TOK_KEEPEMPTY) const;
	native void AppendCharacter(int c);
	native void DeleteLastCharacter();
	native int CodePointCount() const;
	native int, int GetNextCodePoint(int position) const;
	native void Substitute(String str, String replace);
}

struct Translation version("2.4")
{
	static int MakeID(int group, int num)
	{
		return (group << 16) + num;
	}
}


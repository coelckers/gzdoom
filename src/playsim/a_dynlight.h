#pragma once
#include "c_cvars.h"
#include "actor.h"
#include "cycler.h"
#include "g_levellocals.h"

struct side_t;
struct seg_t;

class FSerializer;
struct FSectionLine;

enum ELightType
{
	PointLight,
	PulseLight,
	FlickerLight,
	RandomFlickerLight,
	SectorLight,
	DummyLight,
	ColorPulseLight,
	ColorFlickerLight,
	RandomColorFlickerLight
};

enum
{
	LIGHT_RED = 0,
	LIGHT_GREEN = 1,
	LIGHT_BLUE = 2,
	LIGHT_INTENSITY = 3,
	LIGHT_SECONDARY_INTENSITY = 4,
};

enum LightFlag
{
	LF_SUBTRACTIVE = 1,
	LF_ADDITIVE = 2,
	LF_DONTLIGHTSELF = 4,
	LF_ATTENUATE = 8,
	LF_NOSHADOWMAP = 16,
	LF_DONTLIGHTACTORS = 32,
	LF_SPOT = 64,
	LF_DONTLIGHTOTHERS = 128,
	LF_DONTLIGHTMAP = 256
};

typedef TFlags<LightFlag> LightFlags;
DEFINE_TFLAGS_OPERATORS(LightFlags)

//==========================================================================
//
// Light definitions
//
//==========================================================================
class FLightDefaults
{
public:
	FLightDefaults(FName name, ELightType type = PointLight)
	{
		m_Name = name;
		m_type = type;
	}

	void ApplyProperties(FDynamicLight * light) const;
	FName GetName() const { return m_Name; }
	void SetParameter(double p) { m_Param = p; }
	void SetArg(int arg, int val) { m_Args[arg] = val; }
	int GetArg(int arg) { return m_Args[arg]; }
	uint8_t GetAttenuate() const { return m_attenuate; }
	void SetOffset(float* ft) { m_Pos.X = ft[0]; m_Pos.Z = ft[1]; m_Pos.Y = ft[2]; }
	void SetSubtractive(bool subtract) { if (subtract) m_lightFlags |= LF_SUBTRACTIVE; else m_lightFlags &= ~LF_SUBTRACTIVE; }
	void SetAdditive(bool add) { if (add) m_lightFlags |= LF_ADDITIVE; else m_lightFlags &= ~LF_ADDITIVE; }
	void SetDontLightSelf(bool add) { if (add) m_lightFlags |= LF_DONTLIGHTSELF; else m_lightFlags &= ~LF_DONTLIGHTSELF; }
	void SetAttenuate(bool on) { m_attenuate = on; if (on) m_lightFlags |= LF_ATTENUATE; else m_lightFlags &= ~LF_ATTENUATE; }
	void SetDontLightActors(bool on) { if (on) m_lightFlags |= LF_DONTLIGHTACTORS; else m_lightFlags &= ~LF_DONTLIGHTACTORS; }
	void SetDontLightOthers(bool on) { if (on) m_lightFlags |= LF_DONTLIGHTOTHERS; else m_lightFlags &= ~LF_DONTLIGHTOTHERS; }
	void SetDontLightMap(bool on) { if (on) m_lightFlags |= LF_DONTLIGHTMAP; else m_lightFlags &= ~LF_DONTLIGHTMAP; }
	void SetNoShadowmap(bool on) { if (on) m_lightFlags |= LF_NOSHADOWMAP; else m_lightFlags &= ~LF_NOSHADOWMAP; }
	void SetSpot(bool spot) { if (spot) m_lightFlags |= LF_SPOT; else m_lightFlags &= ~LF_SPOT; }
	void SetSpotInnerAngle(double angle) { m_spotInnerAngle = angle; }
	void SetSpotOuterAngle(double angle) { m_spotOuterAngle = angle; }
	void SetSpotPitch(double pitch)
	{
		m_pitch = pitch;
		m_explicitPitch = true;
	}
	void UnsetSpotPitch()
	{
		m_pitch = 0.;
		m_explicitPitch = false;
	}
	
	void SetType(ELightType type) { m_type = type; }
	void CopyFrom(const FLightDefaults &other)
	{
		auto n = m_Name;
		*this = other;
		m_Name = n;
	}
	void SetFlags(LightFlags lf)
	{
		m_lightFlags = lf;
		m_attenuate = !!(m_lightFlags & LF_ATTENUATE);
	}
	static void SetAttenuationForLevel(bool);

	void OrderIntensities()
	{
		if (m_Args[LIGHT_INTENSITY] > m_Args[LIGHT_SECONDARY_INTENSITY])
		{
			std::swap(m_Args[LIGHT_INTENSITY], m_Args[LIGHT_SECONDARY_INTENSITY]);
			m_swapped = true;
		}
	}

protected:
	FName m_Name = NAME_None;
	int m_Args[5] = { 0,0,0,0,0 };
	double m_Param = 0;
	DVector3 m_Pos = { 0,0,0 };
	int m_type;
	int8_t m_attenuate = -1;
	LightFlags m_lightFlags = 0;
	bool m_swapped = false;
	bool m_spot = false;
	bool m_explicitPitch = false;
	DAngle m_spotInnerAngle = 10.0;
	DAngle m_spotOuterAngle = 25.0;
	DAngle m_pitch = 0.0;
	
	friend FSerializer &Serialize(FSerializer &arc, const char *key, FLightDefaults &value, FLightDefaults *def);
};

FSerializer &Serialize(FSerializer &arc, const char *key, TDeletingArray<FLightDefaults *> &value, TDeletingArray<FLightDefaults *> *def);

//==========================================================================
//
// Light associations (intermediate parser data)
//
//==========================================================================

class FLightAssociation
{
public:
	//FLightAssociation();
	FLightAssociation(FName actorName, const char *frameName, FName lightName)
		: m_ActorName(actorName), m_AssocLight(lightName)
	{
		strncpy(m_FrameName, frameName, 8);
	}

	FName ActorName() { return m_ActorName; }
	const char *FrameName() { return m_FrameName; }
	FName Light() { return m_AssocLight; }
	void ReplaceLightName(FName newName) { m_AssocLight = newName; }
protected:
	char m_FrameName[8];
	FName m_ActorName, m_AssocLight;
};


//==========================================================================
//
// Light associations per actor class
//
//==========================================================================

class FInternalLightAssociation
{
public:
	FInternalLightAssociation(FLightAssociation * asso);
	int Sprite() const { return m_sprite; }
	int Frame() const { return m_frame; }
	const FLightDefaults *Light() const { return m_AssocLight; }
protected:
	int m_sprite;
	int m_frame;
	FLightDefaults * m_AssocLight;
};


struct FLightNode
{
	FLightNode ** prevTarget;
	FLightNode * nextTarget;
	FLightNode ** prevLight;
	FLightNode * nextLight;
	FDynamicLight * lightsource;
	union
	{
		side_t * targLine;
		subsector_t * targSubsector;
		void * targ;
	};
};

struct FDynamicLight
{
	friend class FLightDefaults;

	inline DVector3 PosRelative(int portalgroup) const
	{
		return Pos + Level->Displacements.getOffset(Sector->PortalGroup, portalgroup);
	}

	bool ShouldLightActor(AActor *check)
	{
		return visibletoplayer && IsActive() && 
				(!((*pLightFlags) & LF_DONTLIGHTSELF) || target != check) &&
				(!((*pLightFlags) & LF_DONTLIGHTOTHERS) || target == check) && 
				(!((*pLightFlags) & LF_DONTLIGHTACTORS));
	}

	void SetOffset(const DVector3 &pos)
	{
		m_off = pos;
	}


	bool IsActive() const { return m_active; }
	float GetRadius() const { return (IsActive() ? m_currentRadius * 2.f : 0.f); }
	int GetRed() const { return pArgs[LIGHT_RED]; }
	int GetGreen() const { return pArgs[LIGHT_GREEN]; }
	int GetBlue() const { return pArgs[LIGHT_BLUE]; }
	int GetIntensity() const { return pArgs[LIGHT_INTENSITY]; }
	int GetSecondaryIntensity() const { return pArgs[LIGHT_SECONDARY_INTENSITY]; }

	bool IsSubtractive() const { return !!((*pLightFlags) & LF_SUBTRACTIVE); }
	bool IsAdditive() const { return !!((*pLightFlags) & LF_ADDITIVE); }
	bool IsSpot() const { return !!((*pLightFlags) & LF_SPOT); }
	bool IsAttenuated() const { return !!((*pLightFlags) & LF_ATTENUATE); }
	bool DontShadowmap() const { return !!((*pLightFlags) & LF_NOSHADOWMAP); }
	bool DontLightSelf() const { return !!((*pLightFlags) & (LF_DONTLIGHTSELF|LF_DONTLIGHTACTORS)); }	// dontlightactors implies dontlightself.
	bool DontLightActors() const { return !!((*pLightFlags) & LF_DONTLIGHTACTORS); }
	bool DontLightOthers() const { return !!((*pLightFlags) & (LF_DONTLIGHTOTHERS)); }
	bool DontLightMap() const { return !!((*pLightFlags) & (LF_DONTLIGHTMAP)); }
	void Deactivate() { m_active = false; }
	void Activate();

	void SetActor(AActor *ac, bool isowned) { target = ac; owned = isowned; }
	double X() const { return Pos.X; }
	double Y() const { return Pos.Y; }
	double Z() const { return Pos.Z; }

	void Tick();
	void UpdateLocation();
	void LinkLight();
	void UnlinkLight();
	void ReleaseLight();

private:
	double DistToSeg(const DVector3 &pos, vertex_t *start, vertex_t *end);
	void CollectWithinRadius(const DVector3 &pos, FSection *section, float radius);

public:
	FCycler m_cycler;
	DVector3 Pos;
	DVector3 m_off;

	// This date can either come from the owning actor or from a light definition
	// To avoid having to copy these around every tic, these are pointers to the source data.
	const DAngle *pSpotInnerAngle;
	const DAngle *pSpotOuterAngle;
	const DAngle *pPitch;	// This is to handle pitch overrides through GLDEFS, it can either point to the target's pitch or the light definition.
	const int *pArgs;
	const LightFlags *pLightFlags;

	double specialf1;
	FDynamicLight *next, *prev;
	sector_t *Sector;
	FLevelLocals *Level;
	TObjPtr<AActor *> target;
	FLightNode * touching_sides;
	FLightNode * touching_sector;
	float radius;			// The maximum size the light can be with its current settings.
	float m_currentRadius;	// The current light size.
	int m_tickCount;
	int m_lastUpdate;
	int mShadowmapIndex;
	bool m_active;
	bool visibletoplayer;
	bool shadowmapped;
	uint8_t lighttype;
	bool owned;
	bool swapped;
	bool explicitpitch;

};



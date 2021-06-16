//-----------------------------------------------------------------------------
//
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"
#include "m_bbox.h"
#include "s_sndseq.h"
#include "a_sharedglobal.h"
#include "p_3dmidtex.h"
#include "p_lnspec.h"
#include "r_data/r_interpolate.h"
#include "po_man.h"
#include "p_setup.h"
#include "serializer_doom.h"
#include "serialize_obj.h"
#include "p_blockmap.h"
#include "p_maputl.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "v_text.h"

// TYPES -------------------------------------------------------------------

class DRotatePoly : public DPolyAction
{
	DECLARE_CLASS (DRotatePoly, DPolyAction)
public:
	void Construct(FPolyObj *polyNum);
	void Tick ();

	friend bool EV_RotatePoly (FLevelLocals *Level, line_t *line, int polyNum, int speed, int byteAngle, int direction, bool overRide);
};


class DMovePoly : public DPolyAction
{
	DECLARE_CLASS (DMovePoly, DPolyAction)
public:
	void Construct(FPolyObj *polyNum);
	void Serialize(FSerializer &arc);
	void Tick ();
protected:
	DAngle m_Angle;
	DVector2 m_Speedv;

	friend bool EV_MovePoly(FLevelLocals *Level, line_t *line, int polyNum, double speed, DAngle angle, double dist, bool overRide);
};

class DMovePolyTo : public DPolyAction
{
	DECLARE_CLASS(DMovePolyTo, DPolyAction)
public:
	void Construct(FPolyObj *polyNum);
	void Serialize(FSerializer &arc);
	void Tick();
protected:
	DVector2 m_Speedv;
	DVector2 m_Target;

	friend bool EV_MovePolyTo(FLevelLocals *Level, line_t *line, int polyNum, double speed, const DVector2 &pos, bool overRide);
};


class DPolyDoor : public DMovePoly
{
	DECLARE_CLASS (DPolyDoor, DMovePoly)
public:
	void Construct(FPolyObj *polyNum, podoortype_t type);
	void Serialize(FSerializer &arc);
	void Tick ();
protected:
	DAngle m_Direction;
	double m_TotalDist;
	int m_Tics;
	int m_WaitTics;
	podoortype_t m_Type;
	bool m_Close;

	friend bool EV_OpenPolyDoor(FLevelLocals *Level, line_t *line, int polyNum, double speed, DAngle angle, int delay, double distance, podoortype_t type);
};

class FPolyMirrorIterator
{
	FPolyObj *CurPoly;
	int UsedPolys[100];	// tracks mirrored polyobjects we've seen
	int NumUsedPolys;

public:
	FPolyMirrorIterator(FPolyObj *poly);
	FPolyObj *NextMirror();
};

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void UnLinkPolyobj (FPolyObj *po);
static void LinkPolyobj (FPolyObj *po);
static bool CheckMobjBlocking (side_t *seg, FPolyObj *po);
static void SpawnPolyobj (int index, int tag, int type);
static void DoMovePolyobj (FPolyObj *po, const DVector2 & move);
static FPolyNode *NewPolyNode();
static void FreePolyNode();
static void ReleaseAllPolyNodes();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FPolyNode *FreePolyNodes;

// CODE --------------------------------------------------------------------



//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DPolyAction, false, true)

IMPLEMENT_POINTERS_START(DPolyAction)
	IMPLEMENT_POINTER(m_Interpolation)
IMPLEMENT_POINTERS_END

void DPolyAction::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("polyobj", m_PolyObj)
		("speed", m_Speed)
		("dist", m_Dist)
		("interpolation", m_Interpolation);
}

void DPolyAction::Construct(FPolyObj *polyNum)
{
	m_PolyObj = polyNum;
	m_Speed = 0;
	m_Dist = 0;
	SetInterpolation ();
}

void DPolyAction::OnDestroy()
{
	if (m_PolyObj->specialdata == this)
	{
		m_PolyObj->specialdata = nullptr;
	}

	StopInterpolation();
	Super::OnDestroy();
}

void DPolyAction::Stop()
{
	SN_StopSequence(m_PolyObj);
	Destroy();
}

void DPolyAction::SetInterpolation ()
{
	m_Interpolation = m_PolyObj->SetInterpolation();
}

void DPolyAction::StopInterpolation ()
{
	if (m_Interpolation != nullptr)
	{
		m_Interpolation->DelRef();
		m_Interpolation = nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DRotatePoly, false, false)

void DRotatePoly::Construct(FPolyObj *polyNum)
{
	Super::Construct(polyNum);
}

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DMovePoly, false, false)

void DMovePoly::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("angle", m_Angle)
		("speedv", m_Speedv);
}

void DMovePoly::Construct(FPolyObj *polyNum)
{
	Super::Construct(polyNum);
	m_Angle = 0.;
	m_Speedv = { 0,0 };
}

//==========================================================================
//
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DMovePolyTo, false, false)

void DMovePolyTo::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("speedv", m_Speedv)
		("target", m_Target);
}

void DMovePolyTo::Construct(FPolyObj *polyNum)
{
	Super::Construct(polyNum);
	m_Speedv = m_Target = { 0,0 };
}

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DPolyDoor, false, false)

void DPolyDoor::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc.Enum("type", m_Type)
		("direction", m_Direction)
		("totaldist", m_TotalDist)
		("tics", m_Tics)
		("waittics", m_WaitTics)
		("close", m_Close);
}

void DPolyDoor::Construct (FPolyObj * polyNum, podoortype_t type)
{
	Super::Construct(polyNum);
	m_Type = type;
	m_Direction = 0.;
	m_TotalDist = 0;
	m_Tics = 0;
	m_WaitTics = 0;
	m_Close = false;
}

// ===== Polyobj Event Code =====

//==========================================================================
//
// T_RotatePoly
//
//==========================================================================

void DRotatePoly::Tick ()
{
	if (m_PolyObj == nullptr) return;

	// Don't let non-perpetual polyobjs overshoot their targets.
	if (m_Dist != -1 && m_Dist < fabs(m_Speed))
	{
		m_Speed = m_Speed < 0 ? -m_Dist : m_Dist;
	}

	if (m_PolyObj->RotatePolyobj (m_Speed))
	{
		if (m_Dist == -1)
		{ // perpetual polyobj
			return;
		}
		m_Dist -= fabs(m_Speed);
		if (m_Dist == 0)
		{
			SN_StopSequence (m_PolyObj);
			Destroy ();
		}
	}
}

//==========================================================================
//
// EV_RotatePoly
//
//==========================================================================


bool EV_RotatePoly (FLevelLocals *Level, line_t *line, int polyNum, int speed, int byteAngle, int direction, bool overRide)
{
	DRotatePoly *pe = nullptr;
	FPolyObj *poly;

	if ((poly = Level->GetPolyobj(polyNum)) == nullptr)
	{
		Printf("EV_RotatePoly: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	while ((poly = it.NextMirror()) != nullptr)
	{
		if ((poly->specialdata != nullptr || poly->bBlocked) && !overRide)
		{ // poly is already in motion
			break;
		}
		if (poly->bHasPortals == 2)
		{
			// cannot do rotations on linked polyportals.
			break;
		}
		pe = Level->CreateThinker<DRotatePoly>(poly);
		poly->specialdata = pe;
		poly->bBlocked = false;
		if (byteAngle != 0)
		{
			if (byteAngle == 255)
			{
				pe->m_Dist = -1.;
			}
			else
			{
				pe->m_Dist =  byteAngle*(90./64); // Angle
			}
		}
		else
		{
			pe->m_Dist = 360.;
		}
		pe->m_Speed = speed*direction*(90./(64<<3));
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
		direction = -direction;	// Reverse the direction
	}
	return pe != nullptr;	// Return true if something started moving.
}

//==========================================================================
//
// T_MovePoly
//
//==========================================================================

void DMovePoly::Tick ()
{
	if (m_PolyObj != nullptr)
	{
		if (m_PolyObj->MovePolyobj (m_Speedv))
		{
			double absSpeed = fabs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				SN_StopSequence (m_PolyObj);
				Destroy ();
			}
			else if (m_Dist < absSpeed)
			{
				m_Speed = m_Dist * (m_Speed < 0 ? -1 : 1);
				m_Speedv = m_Angle.ToVector(m_Speed);
			}
			m_PolyObj->UpdateLinks();
		}
	}
}

//==========================================================================
//
// EV_MovePoly
//
//==========================================================================

bool EV_MovePoly (FLevelLocals *Level, line_t *line, int polyNum, double speed, DAngle angle, double dist, bool overRide)
{
	DMovePoly *pe = nullptr;
	FPolyObj *poly;
	DAngle an = angle;

	if ((poly = Level->GetPolyobj(polyNum)) == nullptr)
	{
		Printf("EV_MovePoly: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	while ((poly = it.NextMirror()) != nullptr)
	{
		if ((poly->specialdata != nullptr || poly->bBlocked) && !overRide)
		{ // poly is already in motion
			break;
		}
		pe = Level->CreateThinker<DMovePoly>(poly);
		poly->specialdata = pe;
		poly->bBlocked = false;
		pe->m_Dist = dist; // Distance
		pe->m_Speed = speed;
		pe->m_Angle = angle;
		pe->m_Speedv = angle.ToVector(speed);
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);

		// Do not interpolate very fast moving polyobjects. The minimum tic count is
		// 3 instead of 2, because the moving crate effect in Massmouth 2, Hostitality
		// that this fixes isn't quite fast enough to move the crate back to its start
		// in just 1 tic.
		if (dist/speed <= 2)
		{
			pe->StopInterpolation ();
		}

		angle += 180.;	// Reverse the angle.
	}
	return pe != nullptr;	// Return true if something started moving.
}

//==========================================================================
//
// DMovePolyTo :: Tick
//
//==========================================================================

void DMovePolyTo::Tick ()
{
	if (m_PolyObj != nullptr)
	{
		if (m_PolyObj->MovePolyobj (m_Speedv))
		{
			double absSpeed = fabs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				SN_StopSequence (m_PolyObj);
				Destroy ();
			}
			else if (m_Dist < absSpeed)
			{
				m_Speed = m_Dist * (m_Speed < 0 ? -1 : 1);
				m_Speedv = m_Target - m_PolyObj->StartSpot.pos;
			}
			m_PolyObj->UpdateLinks();
		}
	}
}

//==========================================================================
//
// EV_MovePolyTo
//
//==========================================================================

bool EV_MovePolyTo(FLevelLocals *Level, line_t *line, int polyNum, double speed, const DVector2 &targ, bool overRide)
{
	DMovePolyTo *pe = nullptr;
	FPolyObj *poly;
	DVector2 dist;
	double distlen;

	if ((poly = Level->GetPolyobj(polyNum)) == nullptr)
	{
		Printf("EV_MovePolyTo: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	dist = targ - poly->StartSpot.pos;
	distlen = dist.MakeUnit();
	while ((poly = it.NextMirror()) != nullptr)
	{
		if ((poly->specialdata != nullptr || poly->bBlocked) && !overRide)
		{ // poly is already in motion
			break;
		}
		pe = Level->CreateThinker<DMovePolyTo>(poly);
		poly->specialdata = pe;
		poly->bBlocked = false;
		pe->m_Dist = distlen;
		pe->m_Speed = speed;
		pe->m_Speedv = dist * speed;
		pe->m_Target = poly->StartSpot.pos + dist * distlen;
		SN_StartSequence(poly, poly->seqType, SEQ_DOOR, 0);
		if ((pe->m_Dist / pe->m_Speed) <= 2)
		{
			pe->StopInterpolation();
		}
		dist = -dist;	// reverse the direction
	}
	return pe != nullptr; // Return true if something started moving.
}

//==========================================================================
//
// T_PolyDoor
//
//==========================================================================

void DPolyDoor::Tick ()
{
	if (m_PolyObj == nullptr) return;

	if (m_Tics)
	{
		if (!--m_Tics)
		{
			SN_StartSequence (m_PolyObj, m_PolyObj->seqType, SEQ_DOOR, m_Close);
		}
		return;
	}
	switch (m_Type)
	{
	case PODOOR_SLIDE:
		if (m_Dist <= 0 || m_PolyObj->MovePolyobj (m_Speedv))
		{
			double absSpeed = fabs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				SN_StopSequence (m_PolyObj);
				if (!m_Close && m_WaitTics >= 0)
				{
					m_Dist = m_TotalDist;
					m_Close = true;
					m_Tics = m_WaitTics;
					m_Direction = -m_Direction;
					m_Speedv = -m_Speedv;
				}
				else
				{
					// if set to wait infinitely, Hexen kept the dead thinker to block the polyobject from getting activated again but that causes some problems
					// with the subsectorlinks and the interpolation. Better delete the thinker and use a different means to block it.
					if (!m_Close) m_PolyObj->bBlocked = true;
					Destroy ();
				}
			}
			m_PolyObj->UpdateLinks();
		}
		else
		{
			if (m_PolyObj->crush || !m_Close)
			{ // continue moving if the poly is a crusher, or is opening
				return;
			}
			else
			{ // open back up
				m_Dist = m_TotalDist - m_Dist;
				m_Direction = -m_Direction;
				m_Speedv = -m_Speedv;
				m_Close = false;
				SN_StartSequence (m_PolyObj, m_PolyObj->seqType, SEQ_DOOR, 0);
			}
		}
		break;

	case PODOOR_SWING:
		if (m_Dist <= 0 || m_PolyObj->RotatePolyobj (m_Speed))
		{
			double absSpeed = fabs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				SN_StopSequence (m_PolyObj);
				if (!m_Close && m_WaitTics >= 0)
				{
					m_Dist = m_TotalDist;
					m_Close = true;
					m_Tics = m_WaitTics;
					m_Speed = -m_Speed;
				}
				else
				{
					if (!m_Close) m_PolyObj->bBlocked = true;
					Destroy ();
				}
			}
		}
		else
		{
			if(m_PolyObj->crush || !m_Close)
			{ // continue moving if the poly is a crusher, or is opening
				return;
			}
			else
			{ // open back up and rewait
				m_Dist = m_TotalDist - m_Dist;
				m_Speed = -m_Speed;
				m_Close = false;
				SN_StartSequence (m_PolyObj, m_PolyObj->seqType, SEQ_DOOR, 0);
			}
		}			
		break;

	default:
		break;
	}
}

//==========================================================================
//
// EV_OpenPolyDoor
//
//==========================================================================

bool EV_OpenPolyDoor(FLevelLocals *Level, line_t *line, int polyNum, double speed, DAngle angle, int delay, double distance, podoortype_t type)
{
	DPolyDoor *pd = nullptr;
	FPolyObj *poly;
	int swingdir = 1;	// ADD:  PODOOR_SWINGL, PODOOR_SWINGR

	if ((poly = Level->GetPolyobj(polyNum)) == nullptr)
	{
		Printf("EV_OpenPolyDoor: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	while ((poly = it.NextMirror()) != nullptr)
	{
		if ((poly->specialdata != nullptr || poly->bBlocked))
		{ // poly is already moving
			break;
		}
		if (poly->bHasPortals == 2 && type == PODOOR_SWING)
		{
			// cannot do rotations on linked polyportals.
			break;
		}

		pd = Level->CreateThinker<DPolyDoor>(poly, type);
		poly->specialdata = pd;
		if (type == PODOOR_SLIDE)
		{
			pd->m_WaitTics = delay;
			pd->m_Speed = speed;
			pd->m_Dist = pd->m_TotalDist = distance; // Distance
			pd->m_Direction = angle;
			pd->m_Speedv = angle.ToVector(speed);
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
			angle += 180.;	// reverse the angle
		}
		else if (type == PODOOR_SWING)
		{
			pd->m_WaitTics = delay;
			pd->m_Direction.Degrees = swingdir; 
			pd->m_Speed = (speed*swingdir*(90. / 64)) / 8;
			pd->m_Dist = pd->m_TotalDist = angle.Degrees;
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
			swingdir = -swingdir;	// reverse the direction
		}

	}
	return pd != nullptr;	// Return true if something started moving.
}

//==========================================================================
//
// EV_StopPoly
//
//==========================================================================

bool EV_StopPoly(FLevelLocals *Level, int polynum)
{
	FPolyObj *poly;

	if (nullptr != (poly = Level->GetPolyobj(polynum)))
	{
		if (poly->specialdata != nullptr)
		{
			poly->specialdata->Stop();
		}
		return true;
	}
	return false;
}

// ===== Higher Level Poly Interface code =====



//==========================================================================
//
// 
//
//==========================================================================

FPolyObj::FPolyObj()
{
	StartSpot.pos = { 0,0 };
	Angle = 0.;
	tag = 0;
	memset(bbox, 0, sizeof(bbox));
	validcount = 0;
	crush = 0;
	bHurtOnTouch = false;
	seqType = 0;
	Size = 0;
	bBlocked = false;
	subsectorlinks = nullptr;
	specialdata = nullptr;
	interpolation = nullptr;
}

//==========================================================================
//
// GetPolyobjMirror
//
//==========================================================================

int FPolyObj::GetMirror()
{
	return MirrorNum;
}

//==========================================================================
//
// ThrustMobj
//
//==========================================================================

void FPolyObj::ThrustMobj (AActor *actor, side_t *side)
{
	DAngle thrustAngle;
	DPolyAction *pe;

	double force;

	if (!(actor->flags&MF_SHOOTABLE) && !actor->player)
	{
		return;
	}
	vertex_t *v1 = side->V1();
	vertex_t *v2 = side->V2();
	thrustAngle = (v2->fPos() - v1->fPos()).Angle() - 90.;

	pe = static_cast<DPolyAction *>(specialdata);
	if (pe)
	{
		if (pe->IsKindOf (RUNTIME_CLASS (DRotatePoly)))
		{
			force = pe->GetSpeed() * (90. / 2048);	// For DRotatePoly m_Speed stores an angle which needs to be converted differently
		}
		else
		{
			force = pe->GetSpeed() / 8;
		}
		force = clamp(force, 1., 4.);
	}
	else
	{
		force = 1;
	}

	DVector2 thrust = thrustAngle.ToVector(force);
	actor->Vel += thrust;

	if (crush)
	{
		DVector2 pos = actor->Vec2Offset(thrust.X, thrust.Y);
		if (bHurtOnTouch || !P_CheckMove (actor, pos))
		{
			int newdam = P_DamageMobj (actor, nullptr, nullptr, crush, NAME_Crush, DMG_SECTOR);
			P_TraceBleed (newdam > 0 ? newdam : crush, actor);
		}
	}
	if (Level->flags2 & LEVEL2_POLYGRIND) actor->CallGrind(false); // crush corpses that get caught in a polyobject's way
}

//==========================================================================
//
// UpdateSegBBox
//
//==========================================================================

void FPolyObj::UpdateLinks()
{
	if (bHasPortals == 2)
	{
		TMap<int, bool> processed;
		for (unsigned i = 0; i < Linedefs.Size(); i++)
		{
			if (Linedefs[i]->isLinePortal())
			{
				FLinePortal *port = Linedefs[i]->getPortal();
				if (port->mType == PORTT_LINKED)
				{
					DVector2 old = port->mDisplacement;
					port->mDisplacement = port->mDestination->v2->fPos() - port->mOrigin->v1->fPos();
					FLinePortal *port2 = port->mDestination->getPortal();
					if (port2) port2->mDisplacement = -port->mDisplacement;
					int destgroup = port->mDestination->frontsector->PortalGroup;
					bool *done = processed.CheckKey(destgroup);
					if (!done || !*done)
					{
						processed[destgroup] = true;
						DVector2 delta = port->mDisplacement - old;
						Level->Displacements.MoveGroup(destgroup, delta);
					}
				}
			}
		}
	}
}

void FPolyObj::UpdateBBox ()
{
	for(unsigned i=0;i<Linedefs.Size(); i++)
	{
		Linedefs[i]->AdjustLine();
	}
	CalcCenter();
}

void FPolyObj::CalcCenter()
{
	DVector2 c = { 0, 0 };
	for(unsigned i=0;i<Vertices.Size(); i++)
	{
		c += Vertices[i]->fPos();
	}
	CenterSpot.pos = c / Vertices.Size();
}

//==========================================================================
//
// PO_MovePolyobj
//
//==========================================================================

bool FPolyObj::MovePolyobj (const DVector2 &pos, bool force)
{
	FBoundingBox oldbounds = Bounds;
	UnLinkPolyobj ();
	DoMovePolyobj (pos);

	if (!force)
	{
		bool blocked = false;

		for(unsigned i=0;i < Sidedefs.Size(); i++)
		{
			if (CheckMobjBlocking(Sidedefs[i]))
			{
				blocked = true;
			}
		}
		if (blocked)
		{
			DoMovePolyobj (-pos);
			LinkPolyobj();
			return false;
		}
	}
	StartSpot.pos += pos;
	CenterSpot.pos += pos;
	LinkPolyobj ();
	ClearSubsectorLinks();
	RecalcActorFloorCeil(Bounds | oldbounds);
	return true;
}

//==========================================================================
//
// DoMovePolyobj
//
//==========================================================================

void FPolyObj::DoMovePolyobj (const DVector2 &pos)
{
	for(unsigned i=0;i < Vertices.Size(); i++)
	{
		Vertices[i]->set(Vertices[i]->fX() + pos.X, Vertices[i]->fY() + pos.Y);
		PrevPts[i].pos += pos;
	}
	for (unsigned i = 0; i < Linedefs.Size(); i++)
	{
		Linedefs[i]->bbox[BOXTOP] += pos.Y;
		Linedefs[i]->bbox[BOXBOTTOM] += pos.Y;
		Linedefs[i]->bbox[BOXLEFT] += pos.X;
		Linedefs[i]->bbox[BOXRIGHT] += pos.X;
	}
}

//==========================================================================
//
// RotatePt
//
//==========================================================================

static void RotatePt (DAngle an, DVector2 &out, const DVector2 &start)
{
	DVector2 tr = out;

	double s = an.Sin();
	double c = an.Cos();

	out.X = tr.X * c - tr.Y * s + start.X;
	out.Y = tr.X * s + tr.Y * c + start.Y;
}

//==========================================================================
//
// PO_RotatePolyobj
//
//==========================================================================

bool FPolyObj::RotatePolyobj (DAngle angle, bool fromsave)
{
	DAngle an;
	bool blocked;
	FBoundingBox oldbounds = Bounds;

	an = Angle + angle;

	UnLinkPolyobj();

	for(unsigned i=0;i < Vertices.Size(); i++)
	{
		PrevPts[i].pos = Vertices[i]->fPos();
		FPolyVertex torot = OriginalPts[i];
		RotatePt(an, torot.pos, StartSpot.pos);
		Vertices[i]->set(torot.pos.X, torot.pos.Y);
	}
	blocked = false;
	validcount++;
	UpdateBBox();

	// If we are loading a savegame we do not really want to damage actors and be blocked by them. This can also cause crashes when trying to damage incompletely deserialized player pawns.
	if (!fromsave)
	{
		for (unsigned i = 0; i < Sidedefs.Size(); i++)
		{
			if (CheckMobjBlocking(Sidedefs[i]))
			{
				blocked = true;
			}
		}
		if (blocked)
		{
			for(unsigned i=0;i < Vertices.Size(); i++)
			{
				Vertices[i]->set(PrevPts[i].pos.X, PrevPts[i].pos.Y);
			}
			UpdateBBox();
			LinkPolyobj();
			return false;
		}
	}
	Angle += angle;
	LinkPolyobj();
	ClearSubsectorLinks();
	RecalcActorFloorCeil(Bounds | oldbounds);
	return true;
}

//==========================================================================
//
// UnLinkPolyobj
//
//==========================================================================

void FPolyObj::UnLinkPolyobj ()
{
	polyblock_t *link;
	int i, j;
	int index;

	// remove the polyobj from each blockmap section
	for(j = bbox[BOXBOTTOM]; j <= bbox[BOXTOP]; j++)
	{
		index = j*Level->blockmap.bmapwidth;
		for(i = bbox[BOXLEFT]; i <= bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < Level->blockmap.bmapwidth && j >= 0 && j < Level->blockmap.bmapheight)
			{
				link = Level->PolyBlockMap[index+i];
				while(link != nullptr && link->polyobj != this)
				{
					link = link->next;
				}
				if(link == nullptr)
				{ // polyobj not located in the link cell
					continue;
				}
				link->polyobj = nullptr;
			}
		}
	}
}

//==========================================================================
//
// CheckMobjBlocking
//
//==========================================================================

bool FPolyObj::CheckMobjBlocking (side_t *sd)
{
	static TArray<AActor *> checker;
	FBlockNode *block;
	AActor *mobj;
	int i, j, k;
	int left, right, top, bottom;
	line_t *ld;
	bool blocked;
	bool performBlockingThrust;
	int bmapwidth = Level->blockmap.bmapwidth;
	int bmapheight = Level->blockmap.bmapheight;

	ld = sd->linedef;

	top = Level->blockmap.GetBlockY(ld->bbox[BOXTOP]);
	bottom = Level->blockmap.GetBlockY(ld->bbox[BOXBOTTOM]);
	left = Level->blockmap.GetBlockX(ld->bbox[BOXLEFT]);
	right = Level->blockmap.GetBlockX(ld->bbox[BOXRIGHT]);

	blocked = false;
	checker.Clear();

	bottom = bottom < 0 ? 0 : bottom;
	bottom = bottom >= bmapheight ? bmapheight-1 : bottom;
	top = top < 0 ? 0 : top;
	top = top >= bmapheight  ? bmapheight-1 : top;
	left = left < 0 ? 0 : left;
	left = left >= bmapwidth ? bmapwidth-1 : left;
	right = right < 0 ? 0 : right;
	right = right >= bmapwidth ?  bmapwidth-1 : right;

	for (j = bottom*bmapwidth; j <= top*bmapwidth; j += bmapwidth)
	{
		for (i = left; i <= right; i++)
		{
			for (block = Level->blockmap.blocklinks[j+i]; block != nullptr; block = block->NextActor)
			{
				mobj = block->Me;
				for (k = (int)checker.Size()-1; k >= 0; --k)
				{
					if (checker[k] == mobj)
					{
						break;
					}
				}
				if (k < 0)
				{
					checker.Push (mobj);
					if ((mobj->flags&MF_SOLID) && !(mobj->flags&MF_NOCLIP))
					{
						FLineOpening open;
						open.top = LINEOPEN_MAX;
						open.bottom = LINEOPEN_MIN;
						// [TN] Check wether this actor gets blocked by the line.
						if (ld->backsector != nullptr &&
							!(ld->flags & (ML_BLOCKING|ML_BLOCKEVERYTHING))
							&& !(ld->flags & ML_BLOCK_PLAYERS && (mobj->player || (mobj->flags8 & MF8_BLOCKASPLAYER))) 
							&& !(ld->flags & ML_BLOCKMONSTERS && mobj->flags3 & MF3_ISMONSTER)
							&& !((mobj->flags & MF_FLOAT) && (ld->flags & ML_BLOCK_FLOATERS))
							&& (!(ld->flags & ML_3DMIDTEX) ||
								(!P_LineOpening_3dMidtex(mobj, ld, open) &&
									(mobj->Top() < open.top)
								) || (open.abovemidtex && mobj->Z() > mobj->floorz))
							)
						{
							// [BL] We can't just continue here since we must
							// determine if the line's backsector is going to
							// be blocked.
							performBlockingThrust = false;
						}
						else
						{
							performBlockingThrust = true;
						}

						DVector2 pos = mobj->PosRelative(ld);
						FBoundingBox box(pos.X, pos.Y, mobj->radius);

						if (!inRange(box, ld) || BoxOnLineSide(box, ld) != -1)
						{
							continue;
						}

						if (ld->isLinePortal())
						{
							// Fixme: this still needs to figure out if the polyobject move made the player cross the portal line.
							if (P_TryMove(mobj, mobj->Pos(), false))
							{
								continue;
							}
						}
						// We have a two-sided linedef so we should only check one side
						// so that the thrust from both sides doesn't cancel each other out.
						// Best use the one facing the player and ignore the back side.
						if (ld->sidedef[1] != nullptr)
						{
							int side = P_PointOnLineSidePrecise(mobj->Pos(), ld);
							if (ld->sidedef[side] != sd)
							{
								continue;
							}
							// [BL] See if we hit below the floor/ceiling of the poly.
							else if(!performBlockingThrust && (
									mobj->Z() < ld->sidedef[!side]->sector->GetSecPlane(sector_t::floor).ZatPoint(mobj) ||
									mobj->Top() > ld->sidedef[!side]->sector->GetSecPlane(sector_t::ceiling).ZatPoint(mobj)
								))
							{
								performBlockingThrust = true;
							}
						}

						if(performBlockingThrust)
						{
							ThrustMobj (mobj, sd);
							blocked = true;
						}
						else
							continue;
					}
				}
			}
		}
	}
	return blocked;
}

//==========================================================================
//
// LinkPolyobj
//
//==========================================================================

void FPolyObj::LinkPolyobj ()
{
	polyblock_t **link;
	polyblock_t *tempLink;
	int bmapwidth = Level->blockmap.bmapwidth;
	int bmapheight = Level->blockmap.bmapheight;

	// calculate the polyobj bbox
	Bounds.ClearBox();
	for(unsigned i = 0; i < Sidedefs.Size(); i++)
	{
		vertex_t *vt;
		
		vt = Sidedefs[i]->linedef->v1;
		Bounds.AddToBox(vt->fPos());
		vt = Sidedefs[i]->linedef->v2;
		Bounds.AddToBox(vt->fPos());
	}
	bbox[BOXRIGHT] = Level->blockmap.GetBlockX(Bounds.Right());
	bbox[BOXLEFT] = Level->blockmap.GetBlockX(Bounds.Left());
	bbox[BOXTOP] = Level->blockmap.GetBlockY(Bounds.Top());
	bbox[BOXBOTTOM] = Level->blockmap.GetBlockY(Bounds.Bottom());
	// add the polyobj to each blockmap section
	for(int j = bbox[BOXBOTTOM]*bmapwidth; j <= bbox[BOXTOP]*bmapwidth;
		j += bmapwidth)
	{
		for(int i = bbox[BOXLEFT]; i <= bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight*bmapwidth)
			{
				link = &Level->PolyBlockMap[j+i];
				if(!(*link))
				{ // CreateThinker a new link at the current block cell
					*link = new polyblock_t;
					(*link)->next = nullptr;
					(*link)->prev = nullptr;
					(*link)->polyobj = this;
					continue;
				}
				else
				{
					tempLink = *link;
					while(tempLink->next != nullptr && tempLink->polyobj != nullptr)
					{
						tempLink = tempLink->next;
					}
				}
				if(tempLink->polyobj == nullptr)
				{
					tempLink->polyobj = this;
					continue;
				}
				else
				{
					tempLink->next = new polyblock_t;
					tempLink->next->next = nullptr;
					tempLink->next->prev = tempLink;
					tempLink->next->polyobj = this;
				}
			}
			// else, don't link the polyobj, since it's off the map
		}
	}
}

//===========================================================================
//
// FPolyObj :: RecalcActorFloorCeil
//
// For each actor within the bounding box, recalculate its floorz, ceilingz,
// and related values.
//
//===========================================================================

void FPolyObj::RecalcActorFloorCeil(FBoundingBox bounds) const
{
	FBlockThingsIterator it(Level, bounds);
	AActor *actor;

	while ((actor = it.Next()) != nullptr)
	{
		// skip everything outside the bounding box.
		if (actor->X() + actor->radius <= bounds.Left() ||
			actor->X() - actor->radius >= bounds.Right() ||
			actor->Y() + actor->radius <= bounds.Bottom() ||
			actor->Y() - actor->radius >= bounds.Top())
		{
			continue;
		}
		// Todo: Be a little more thorough with what gets altered here
		// because this can dislocate a lot of items that were spawned on 
		// the lower side of a sector boundary.
		P_FindFloorCeiling(actor);
	}
}

//===========================================================================
//
// PO_ClosestPoint
//
// Given a point (x,y), returns the point (ox,oy) on the polyobject's walls
// that is nearest to (x,y). Also returns the seg this point came from.
//
//===========================================================================

void FPolyObj::ClosestPoint(const DVector2 &fpos, DVector2 &out, side_t **side) const
{
	unsigned int i;
	double x = fpos.X, y = fpos.Y;
	double bestdist = HUGE_VAL;
	double bestx = 0, besty = 0;
	side_t *bestline = nullptr;

	for (i = 0; i < Sidedefs.Size(); ++i)
	{
		vertex_t *v1 = Sidedefs[i]->V1();
		vertex_t *v2 = Sidedefs[i]->V2();
		double a = v2->fX() - v1->fX();
		double b = v2->fY() - v1->fY();
		double den = a*a + b*b;
		double ix, iy, dist;

		if (den == 0)
		{ // Line is actually a point!
			ix = v1->fX();
			iy = v1->fY();
		}
		else
		{
			double num = (x - v1->fX()) * a + (y - v1->fY()) * b;
			double u = num / den;
			if (u <= 0)
			{
				ix = v1->fX();
				iy = v1->fY();
			}
			else if (u >= 1)
			{
				ix = v2->fX();
				iy = v2->fY();
			}
			else
			{
				ix = v1->fX() + u * a;
				iy = v1->fY() + u * b;
			}
		}
		a = (ix - x);
		b = (iy - y);
		dist = a*a + b*b;
		if (dist < bestdist)
		{
			bestdist = dist;
			bestx = ix;
			besty = iy;
			bestline = Sidedefs[i];
		}
	}
	out = { bestx, besty };
	if (side != nullptr)
	{
		*side = bestline;
	}
}

//==========================================================================
//
// PO_Busy
//
//==========================================================================

bool PO_Busy (FLevelLocals *Level, int polyobj)
{
	FPolyObj *poly;

	poly = Level->GetPolyobj(polyobj);
	return (poly != nullptr && poly->specialdata != nullptr);
}



//==========================================================================
//
//
//
//==========================================================================

void FPolyObj::ClearSubsectorLinks()
{
	while (subsectorlinks != nullptr)
	{
		assert(subsectorlinks->state == 1337);

		FPolyNode *next = subsectorlinks->snext;

		if (subsectorlinks->pnext != nullptr)
		{
			assert(subsectorlinks->pnext->state == 1337);
			subsectorlinks->pnext->pprev = subsectorlinks->pprev;
		}

		if (subsectorlinks->pprev != nullptr)
		{
			assert(subsectorlinks->pprev->state == 1337);
			subsectorlinks->pprev->pnext = subsectorlinks->pnext;
		}
		else
		{
			subsectorlinks->subsector->polys = subsectorlinks->pnext;
		}

		if (subsectorlinks->subsector->BSP != nullptr)
		{
			subsectorlinks->subsector->BSP->bDirty = true;
		}

		subsectorlinks->state = -1;
		delete subsectorlinks;
		subsectorlinks = next;
	}
	subsectorlinks = nullptr;
}

void FLevelLocals::ClearAllSubsectorLinks()
{
	for(auto &poly : Polyobjects)
	{
		poly.ClearSubsectorLinks();
	}
	ReleaseAllPolyNodes();
}

//==========================================================================
//
// GetIntersection
//
// adapted from P_InterceptVector
//
//==========================================================================

static bool GetIntersection(FPolySeg *seg, node_t *bsp, FPolyVertex *v)
{
	double frac;
	double num;
	double den;

	double v2x = seg->v1.pos.X;
	double v2y = seg->v1.pos.Y;
	double v2dx = seg->v2.pos.X - v2x;
	double v2dy = seg->v2.pos.Y - v2y;
	double v1x =  FIXED2DBL(bsp->x);
	double v1y =  FIXED2DBL(bsp->y);
	double v1dx = FIXED2DBL(bsp->dx);
	double v1dy = FIXED2DBL(bsp->dy);
		
	den = v1dy*v2dx - v1dx*v2dy;

	if (den == 0)
		return false;		// parallel
	
	num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
	frac = num / den;

	if (frac < 0. || frac > 1.) return false;

	v->pos.X = v2x + frac * v2dx;
	v->pos.Y = v2y + frac * v2dy;
	return true;
}

//==========================================================================
//
// PartitionDistance
//
// Determine the distance of a vertex to a node's partition line.
//
//==========================================================================

static double PartitionDistance(FPolyVertex *vt, node_t *node)
{	
	return fabs(FIXED2DBL(-node->dy) * (vt->pos.X - FIXED2DBL(node->x)) + FIXED2DBL(node->dx) * (vt->pos.Y - FIXED2DBL(node->y))) / node->len;
}

//==========================================================================
//
// AddToBBox
//
//==========================================================================

static void AddToBBox(float child[4], float parent[4])
{
	if (child[BOXTOP] > parent[BOXTOP])
	{
		parent[BOXTOP] = child[BOXTOP];
	}
	if (child[BOXBOTTOM] < parent[BOXBOTTOM])
	{
		parent[BOXBOTTOM] = child[BOXBOTTOM];
	}
	if (child[BOXLEFT] < parent[BOXLEFT])
	{
		parent[BOXLEFT] = child[BOXLEFT];
	}
	if (child[BOXRIGHT] > parent[BOXRIGHT])
	{
		parent[BOXRIGHT] = child[BOXRIGHT];
	}
}

//==========================================================================
//
// AddToBBox
//
//==========================================================================

static void AddToBBox(FPolyVertex *v, float bbox[4])
{
	float x = float(v->pos.X);
	float y = float(v->pos.Y);
	if (x < bbox[BOXLEFT])
	{
		bbox[BOXLEFT] = x;
	}
	if (x > bbox[BOXRIGHT])
	{
		bbox[BOXRIGHT] = x;
	}
	if (y < bbox[BOXBOTTOM])
	{
		bbox[BOXBOTTOM] = y;
	}
	if (y > bbox[BOXTOP])
	{
		bbox[BOXTOP] = y;
	}
}

//==========================================================================
//
// SplitPoly
//
//==========================================================================

static void SplitPoly(FPolyNode *pnode, void *node, float bbox[4])
{
	static TArray<FPolySeg> lists[2];
	static const double POLY_EPSILON = 0.3125;

	if (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		int centerside = R_PointOnSide(pnode->poly->CenterSpot.pos, bsp);

		lists[0].Clear();
		lists[1].Clear();
		for(unsigned i=0;i<pnode->segs.Size(); i++)
		{
			FPolySeg *seg = &pnode->segs[i];

			// Parts of the following code were taken from Eternity and are
			// being used with permission.

			// get distance of vertices from partition line
			// If the distance is too small, we may decide to
			// change our idea of sidedness.
			double dist_v1 = PartitionDistance(&seg->v1, bsp);
			double dist_v2 = PartitionDistance(&seg->v2, bsp);

			// If the distances are less than epsilon, consider the points as being
			// on the same side as the polyobj origin. Why? People like to build
			// polyobject doors flush with their door tracks. This breaks using the
			// usual assumptions.
			

			// Addition to Eternity code: We must also check any seg with only one
			// vertex inside the epsilon threshold. If not, these lines will get split but
			// adjoining ones with both vertices inside the threshold won't thus messing up
			// the order in which they get drawn.

			if(dist_v1 <= POLY_EPSILON)
			{
				if (dist_v2 <= POLY_EPSILON)
				{
					lists[centerside].Push(*seg);
				}
				else
				{
					int side = R_PointOnSide(seg->v2.pos, bsp);
					lists[side].Push(*seg);
				}
			}
			else if (dist_v2 <= POLY_EPSILON)
			{
				int side = R_PointOnSide(seg->v1.pos, bsp);
				lists[side].Push(*seg);
			}
			else 
			{
				int side1 = R_PointOnSide(seg->v1.pos, bsp);
				int side2 = R_PointOnSide(seg->v2.pos, bsp);

				if(side1 != side2)
				{
					// if the partition line crosses this seg, we must split it.

					FPolyVertex vert;

					if (GetIntersection(seg, bsp, &vert))
					{
						lists[0].Push(*seg);
						lists[1].Push(*seg);
						lists[side1].Last().v2 = vert;
						lists[side2].Last().v1 = vert;
					}
					else
					{
						// should never happen
						lists[side1].Push(*seg);
					}
				}
				else 
				{
					// both points on the same side.
					lists[side1].Push(*seg);
				}
			}
		}
		if (lists[1].Size() == 0)
		{
			SplitPoly(pnode, bsp->children[0], bsp->bbox[0]);
			AddToBBox(bsp->bbox[0], bbox);
		}
		else if (lists[0].Size() == 0)
		{
			SplitPoly(pnode, bsp->children[1], bsp->bbox[1]);
			AddToBBox(bsp->bbox[1], bbox);
		}
		else
		{
			// create the new node 
			FPolyNode *newnode = NewPolyNode();
			newnode->poly = pnode->poly;
			newnode->segs = lists[1];

			// set segs for original node
			pnode->segs = lists[0];
		
			// recurse back side
			SplitPoly(newnode, bsp->children[1], bsp->bbox[1]);
			
			// recurse front side
			SplitPoly(pnode, bsp->children[0], bsp->bbox[0]);

			AddToBBox(bsp->bbox[0], bbox);
			AddToBBox(bsp->bbox[1], bbox);
		}
	}
	else
	{
		// we reached a subsector so we can link the node with this subsector
		subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);

		// Link node to subsector
		pnode->pnext = sub->polys;
		if (pnode->pnext != nullptr) 
		{
			assert(pnode->pnext->state == 1337);
			pnode->pnext->pprev = pnode;
		}
		pnode->pprev = nullptr;
		sub->polys = pnode;

		// link node to polyobject
		pnode->snext = pnode->poly->subsectorlinks;
		pnode->poly->subsectorlinks = pnode;
		pnode->subsector = sub;

		// calculate bounding box for this polynode
		assert(pnode->segs.Size() != 0);
		float subbbox[4] = { FLT_MIN, FLT_MAX, FLT_MAX, FLT_MIN };

		for (unsigned i = 0; i < pnode->segs.Size(); ++i)
		{
			AddToBBox(&pnode->segs[i].v1, subbbox);
			AddToBBox(&pnode->segs[i].v2, subbbox);
		}
		// Potentially expand the parent node's bounding box to contain these bits of polyobject.
		AddToBBox(subbbox, bbox);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FPolyObj::CreateSubsectorLinks()
{
	FPolyNode *node = NewPolyNode();
	// Even though we don't care about it, we need to initialize this
	// bounding box to something so that Valgrind won't complain about it
	// when SplitPoly modifies it.
	float dummybbox[4] = { 0 };

	node->poly = this;
	node->segs.Resize(Sidedefs.Size());

	for(unsigned i=0; i<Sidedefs.Size(); i++)
	{
		FPolySeg *seg = &node->segs[i];
		side_t *side = Sidedefs[i];

		seg->v1 = side->V1();
		seg->v2 = side->V2();
		seg->wall = side;
	}
	if (!(Level->i_compatflags & COMPATF_POLYOBJ))
	{
		SplitPoly(node, Level->HeadNode(), dummybbox);
	}
	else
	{
		subsector_t *sub = CenterSubsector;

		// Link node to subsector
		node->pnext = sub->polys;
		if (node->pnext != nullptr) 
		{
			assert(node->pnext->state == 1337);
			node->pnext->pprev = node;
		}
		node->pprev = nullptr;
		sub->polys = node;

		// link node to polyobject
		node->snext = node->poly->subsectorlinks;
		node->poly->subsectorlinks = node;
		node->subsector = sub;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void PO_LinkToSubsectors(FLevelLocals *Level)
{
	for(auto &poly : Level->Polyobjects)
	{
		if (poly.subsectorlinks == nullptr)
		{
			poly.CreateSubsectorLinks();
		}
	}
}

//==========================================================================
//
// NewPolyNode
//
//==========================================================================

static FPolyNode *NewPolyNode()
{
	FPolyNode *node;

	if (FreePolyNodes != nullptr)
	{
		node = FreePolyNodes;
		FreePolyNodes = node->pnext;
	}
	else
	{
		node = new FPolyNode;
	}
	node->state = 1337;
	node->poly = nullptr;
	node->pnext = nullptr;
	node->pprev = nullptr;
	node->subsector = nullptr;
	node->snext = nullptr;
	return node;
}

//==========================================================================
//
// FreePolyNode
//
//==========================================================================

void FreePolyNode(FPolyNode *node)
{
	node->segs.Clear();
	node->pnext = FreePolyNodes;
	FreePolyNodes = node;
}

//==========================================================================
//
// ReleaseAllPolyNodes
//
//==========================================================================

void ReleaseAllPolyNodes()
{
	FPolyNode *node, *next;

	for (node = FreePolyNodes; node != nullptr; node = next)
	{
		next = node->pnext;
		delete node;
	}
}

//==========================================================================
//
// FPolyMirrorIterator Constructor
//
// This class is used to avoid infinitely looping on cyclical chains of
// mirrored polyobjects.
//
//==========================================================================

FPolyMirrorIterator::FPolyMirrorIterator(FPolyObj *poly)
{
	CurPoly = poly;
	if (poly != nullptr)
	{
		UsedPolys[0] = poly->tag;
		NumUsedPolys = 1;
	}
	else
	{
		NumUsedPolys = 0;
	}
}

//==========================================================================
//
// FPolyMirrorIterator :: NextMirror
//
// Returns the polyobject that mirrors the current one, or nullptr if there
// is no mirroring polyobject, or there is a mirroring polyobject but it was
// already returned.
//
//==========================================================================

FPolyObj *FPolyMirrorIterator::NextMirror()
{
	FPolyObj *poly = CurPoly, *nextpoly;

	if (poly == nullptr)
	{
		return nullptr;
	}

	// Do the work to decide which polyobject to return the next time this
	// function is called.
	int mirror = poly->GetMirror(), i;
	nextpoly = nullptr;

	// Is there a mirror and we have room to remember it?
	if (mirror != 0 && NumUsedPolys != countof(UsedPolys))
	{
		// Has this polyobject been returned already?
		for (i = 0; i < NumUsedPolys; ++i)
		{
			if (UsedPolys[i] == mirror)
			{
				break;	// Yes, it has been returned.
			}
		}
		if (i == NumUsedPolys)
		{ // No, it has not been returned.
			UsedPolys[NumUsedPolys++] = mirror;
			nextpoly = poly->Level->GetPolyobj(mirror);
			if (nextpoly == nullptr)
			{
				Printf("Invalid mirror polyobj num %d for polyobj num %d\n", mirror, UsedPolys[i - 1]);
			}
		}
	}
	CurPoly = nextpoly;
	return poly;
}

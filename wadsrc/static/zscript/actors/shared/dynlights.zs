class DynamicLight : Actor
{
	double SpotInnerAngle;
	double SpotOuterAngle;
	private int lighttype;
	private int lightflags;

	property SpotInnerAngle: SpotInnerAngle;
	property SpotOuterAngle: SpotOuterAngle;
	
	flagdef subtractive: lightflags, 0;
	flagdef additive: lightflags, 1;
	flagdef dontlightself: lightflags, 2;
	flagdef attenuate: lightflags, 3;
	flagdef noshadowmap: lightflags, 4;
	flagdef dontlightactors: lightflags, 5;
	flagdef spot: lightflags, 6;
	flagdef dontlightothers: lightflags, 7;
	flagdef dontlightmap: lightflags, 8;

	enum EArgs
	{
	   LIGHT_RED = 0,
	   LIGHT_GREEN = 1,
	   LIGHT_BLUE = 2,
	   LIGHT_INTENSITY = 3,
	   LIGHT_SECONDARY_INTENSITY = 4,
	   LIGHT_SCALE = 3,
	};

	// These are for use in A_AttachLight calls.
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
		LF_DONTLIGHTMAP = 256,
	};

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

	native void SetOffset(Vector3 offset);
	private native void AttachLight();
	private native void ActivateLight();
	private native void DeactivateLight();

	Default
	{
		Height 0;
		Radius 0.1;
		FloatBobPhase 0;
		RenderRadius -1;
		DynamicLight.SpotInnerAngle 10;
		DynamicLight.SpotOuterAngle 25;
		+NOBLOCKMAP
		+NOGRAVITY
		+FIXMAPTHINGPOS
		+INVISIBLE
	}
	
	//==========================================================================
	//
	//
	//
	//==========================================================================

	override void Tick()
	{
		// Lights do not call the super method.
	}
	
	override void BeginPlay()
	{
		ChangeStatNum(STAT_DLIGHT);
	}

	override void PostBeginPlay()
	{
		Super.PostBeginPlay();
		AttachLight();
		
		if (!(SpawnFlags & MTF_DORMANT))
		{
			Activate(self);
		}
	}

	override void Activate(Actor activator)
	{
		bDormant = false;
		ActivateLight();
	}

	override void Deactivate(Actor activator)
	{
		bDormant = true;
		DeactivateLight();
	}
	
}


class PointLight : DynamicLight
{
	Default
	{
		DynamicLight.Type "Point";
	}
}

class PointLightPulse : PointLight
{
	Default
	{
		DynamicLight.Type "Pulse";
	}
}

class PointLightFlicker : PointLight
{
	Default
	{
		DynamicLight.Type "Flicker";
	}
}

class SectorPointLight : PointLight
{
	Default
	{
		DynamicLight.Type "Sector";
	}
}

class PointLightFlickerRandom : PointLight
{
	Default
	{
		DynamicLight.Type "RandomFlicker";
	}
}

// DYNAMICLIGHT.ADDITIVE and DYNAMICLIGHT.SUBTRACTIVE are used by the lights for additive and subtractive lights

class PointLightAdditive : PointLight
{
	Default
	{
		+DYNAMICLIGHT.ADDITIVE
	}
}

class PointLightPulseAdditive : PointLightPulse
{
	Default
	{
		+DYNAMICLIGHT.ADDITIVE
	}
}

class PointLightFlickerAdditive : PointLightFlicker
{
	Default
	{
		+DYNAMICLIGHT.ADDITIVE
	}
}

class SectorPointLightAdditive : SectorPointLight
{
	Default
	{
		+DYNAMICLIGHT.ADDITIVE
	}
}

class PointLightFlickerRandomAdditive :PointLightFlickerRandom
{
	Default
	{
		+DYNAMICLIGHT.ADDITIVE
	}
}

class PointLightSubtractive : PointLight
{
	Default
	{
		+DYNAMICLIGHT.SUBTRACTIVE
	}
}

class PointLightPulseSubtractive : PointLightPulse
{
	Default
	{
		+DYNAMICLIGHT.SUBTRACTIVE
	}
}

class PointLightFlickerSubtractive : PointLightFlicker
{
	Default
	{
		+DYNAMICLIGHT.SUBTRACTIVE
	}
}

class SectorPointLightSubtractive : SectorPointLight
{
	Default
	{
		+DYNAMICLIGHT.SUBTRACTIVE
	}
}

class PointLightFlickerRandomSubtractive : PointLightFlickerRandom
{
	Default
	{
		+DYNAMICLIGHT.SUBTRACTIVE
	}
}

class PointLightAttenuated : PointLight
{
	Default
	{
		+DYNAMICLIGHT.ATTENUATE
	}
}

class PointLightPulseAttenuated : PointLightPulse
{
	Default
	{
		+DYNAMICLIGHT.ATTENUATE
	}
}

class PointLightFlickerAttenuated : PointLightFlicker
{
	Default
	{
		+DYNAMICLIGHT.ATTENUATE
	}
}

class SectorPointLightAttenuated : SectorPointLight
{
	Default
	{
		+DYNAMICLIGHT.ATTENUATE
	}
}

class PointLightFlickerRandomAttenuated :PointLightFlickerRandom
{
	Default
	{
		+DYNAMICLIGHT.ATTENUATE
	}
}

class SpotLight : DynamicLight
{
	Default
	{
		DynamicLight.Type "Point";
		+DYNAMICLIGHT.SPOT
	}
}

class SpotLightPulse : SpotLight
{
	Default
	{
		DynamicLight.Type "Pulse";
	}
}

class SpotLightFlicker : SpotLight
{
	Default
	{
		DynamicLight.Type "Flicker";
	}
}

class SectorSpotLight : SpotLight
{
	Default
	{
		DynamicLight.Type "Sector";
	}
}

class SpotLightFlickerRandom : SpotLight
{
	Default
	{
		DynamicLight.Type "RandomFlicker";
	}
}

class SpotLightAdditive : SpotLight
{
	Default
	{
		+DYNAMICLIGHT.ADDITIVE
	}
}

class SpotLightPulseAdditive : SpotLightPulse
{
	Default
	{
		+DYNAMICLIGHT.ADDITIVE
	}
}

class SpotLightFlickerAdditive : SpotLightFlicker
{
	Default
	{
		+DYNAMICLIGHT.ADDITIVE
	}
}

class SectorSpotLightAdditive : SectorSpotLight
{
	Default
	{
		+DYNAMICLIGHT.ADDITIVE
	}
}

class SpotLightFlickerRandomAdditive : SpotLightFlickerRandom
{
	Default
	{
		+DYNAMICLIGHT.ADDITIVE
	}
}

class SpotLightSubtractive : SpotLight
{
	Default
	{
		+DYNAMICLIGHT.SUBTRACTIVE
	}
}

class SpotLightPulseSubtractive : SpotLightPulse
{
	Default
	{
		+DYNAMICLIGHT.SUBTRACTIVE
	}
}

class SpotLightFlickerSubtractive : SpotLightFlicker
{
	Default
	{
		+DYNAMICLIGHT.SUBTRACTIVE
	}
}

class SectorSpotLightSubtractive : SectorSpotLight
{
	Default
	{
		+DYNAMICLIGHT.SUBTRACTIVE
	}
}

class SpotLightFlickerRandomSubtractive : SpotLightFlickerRandom
{
	Default
	{
		+DYNAMICLIGHT.SUBTRACTIVE
	}
}

class SpotLightAttenuated : SpotLight
{
	Default
	{
		+DYNAMICLIGHT.ATTENUATE
	}
}

class SpotLightPulseAttenuated : SpotLightPulse
{
	Default
	{
		+DYNAMICLIGHT.ATTENUATE
	}
}

class SpotLightFlickerAttenuated : SpotLightFlicker
{
	Default
	{
		+DYNAMICLIGHT.ATTENUATE
	}
}

class SectorSpotLightAttenuated : SectorSpotLight
{
	Default
	{
		+DYNAMICLIGHT.ATTENUATE
	}
}

class SpotLightFlickerRandomAttenuated : SpotLightFlickerRandom
{
	Default
	{
		+DYNAMICLIGHT.ATTENUATE
	}
}

class VavoomLight : DynamicLight 
{
	Default
	{
		DynamicLight.Type "Point";
	}
	
	override void BeginPlay ()
	{
		if (CurSector) AddZ(-CurSector.floorplane.ZatPoint(pos.XY), false); // z is absolute for Vavoom lights
		Super.BeginPlay();
	}
}

class VavoomLightWhite : VavoomLight
{
	override void BeginPlay ()
	{
		args[LIGHT_INTENSITY] = args[0] * 4;
		args[LIGHT_RED] = 128;
		args[LIGHT_GREEN] = 128;
		args[LIGHT_BLUE] = 128;

		Super.BeginPlay();
	}
}

class VavoomLightColor : VavoomLight
{
	override void BeginPlay ()
	{
		int radius = args[0] * 4;
		args[LIGHT_RED] = args[1] >> 1;
		args[LIGHT_GREEN] = args[2] >> 1;
		args[LIGHT_BLUE] = args[3] >> 1;
		args[LIGHT_INTENSITY] = radius;
		Super.BeginPlay();
	}
}



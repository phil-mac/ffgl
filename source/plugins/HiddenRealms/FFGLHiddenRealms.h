#pragma once
#include "../_common/GeneratorCommon.h"

// Hidden Realms: a spherical border portal. A rim-lit glowing ring with turbulence, rays, plasma,
// inner/outer glow and an optional solid centre so a layered video reads as the inside of the portal.
class FFGLHiddenRealms : public gen::GeneratorPlugin
{
public:
	FFGLHiddenRealms();

	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override;
	FFResult DeInitGL() override;
	FFResult SetFloatParameter( unsigned int index, float value ) override;
	float GetFloatParameter( unsigned int index ) override;

private:
	void UpdateVisibility( bool raiseEvent );

	// ---- Shape ----
	float preset    = 0.0f;   // option
	float radius    = 0.38f;  // frame units (shorter side = 1)
	float mode      = 0.0f;   // option: 0 fill outside, 1 ring
	float width     = 0.04f;  // hot core width at the inner edge, frame units
	float tilt      = 0.0f;   // degrees, squashes the ring into an ellipse
	float rotation  = 0.0f;   // degrees
	float spin      = 0.0f;   // degrees per second
	float softness  = 1.0f;   // px
	double spinAccum = 0.0;

	// ---- Energy ----
	float sphere    = 0.6f;   // 0..1 rim lighting amount
	float lightAng  = 135.0f; // degrees
	float outerGlow = 0.5f;   // 0..1 fade distance outward (fraction of radius)
	float edgeLevel = 0.35f;  // 0..1 brightness left at the far edge in Fill mode
	float innerGlow = 0.2f;
	float hotCore   = 0.7f;   // 0..1 whitening of the core band
	float tendrils  = 0.6f;   // 0..1 electric filaments creeping into the centre
	float reach     = 0.5f;   // 0..1 how far tendrils reach (fraction of radius)
	float glowInt   = 1.0f;   // 0..3
	float plasma    = 0.5f;   // 0..1 brightness modulation along the ring
	float turb      = 0.3f;   // 0..1 radial wobble
	float turbScale = 4.0f;   // int harmonics around the ring
	float rays      = 0.0f;   // int count
	float rayLen    = 0.3f;   // 0..1 fraction of radius
	float pulse     = 0.3f;   // 0..1 radius breathing
	float flicker   = 0.0f;   // 0..1

	// ---- Center ----
	float centerMode = 1.0f;  // 0 transparent, 1 solid
	float centerCol[ 4 ] = { 0.0f, 0.0f, 0.0f, 1.0f }; // HSBA, black opaque
	float feather    = 0.0f;  // 0..1 fraction of radius

	// ---- Color ----
	float colorBy   = 3.0f;   // option (Depth)
	unsigned pColorBy = 0;

	ffglex::FFGLShader shader;
	ffglex::FFGLScreenQuad quad;
	GLint uMode = -1, uEdgeLevel = -1, uHotCore = -1, uTendrils = -1, uReach = -1;
	GLint uResolution = -1, uRadius = -1, uWidth = -1, uTilt = -1, uRotation = -1, uSoft = -1, uPhase = -1;
	GLint uSphere = -1, uLight = -1, uOGlow = -1, uIGlow = -1, uGlowInt = -1, uPlasma = -1, uTurb = -1, uTurbScale = -1;
	GLint uRays = -1, uRayLen = -1, uPulse = -1, uFlicker = -1, uCenterMode = -1, uCenterCol = -1, uFeather = -1, uColorBy = -1;
};

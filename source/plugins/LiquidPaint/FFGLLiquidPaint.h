#pragma once
#include "../_common/GeneratorCommon.h"

// Liquid Paint: paints of different colours mixing and flowing into each other (acrylic pour /
// liquid light show). Domain-warped, loop-periodic noise quantised into distinct paints, with
// pour-cell lacing, floating droplets, ink veins, a stirring vortex and glossy relief lighting.
class FFGLLiquidPaint : public gen::GeneratorPlugin
{
public:
	FFGLLiquidPaint();

	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override;
	FFResult DeInitGL() override;
	FFResult SetFloatParameter( unsigned int index, float value ) override;
	float GetFloatParameter( unsigned int index ) override;

private:
	void UpdateVisibility( bool raiseEvent );

	// ---- Flow ----
	float preset     = 0.0f;  // option
	float scale      = 1.6f;  // noise cells across the frame
	float detail     = 6.0f;  // octaves
	float roughness  = 0.5f;
	float warp       = 1.2f;
	float turbulence = 0.5f;
	float swirl      = 0.0f;  // turns at the centre
	float swirlRad   = 0.35f;
	float symmetry   = 1.0f;  // kaleidoscope sectors
	float rotation   = 0.0f;  // degrees
	float spin       = 0.0f;  // degrees per second
	float flow       = 0.0f;  // drift speed, frame units per second
	float flowAngle  = 0.0f;  // degrees
	float churn      = 2.0f;  // noise cells travelled per loop

	// ---- Paint ----
	float paints     = 4.0f;  // number of distinct paints (1 = continuous)
	float blend      = 0.25f; // 0 hard edges .. 1 smooth gradient
	float softness   = 1.0f;  // px
	float contrast   = 2.4f;
	float colorBy    = 0.0f;  // option
	float coverage   = 1.0f;  // 1 = paint everywhere, less = holes showing the background
	float veins      = 0.0f;
	float veinCount  = 8.0f;

	// ---- Surface ----
	float cells      = 0.35f;
	float cellSize   = 0.12f;
	float cellTone   = 0.85f; // 0 dark lace .. 1 light lace
	float droplets   = 0.3f;
	float dropSize   = 0.5f;
	float gloss      = 0.3f;
	float relief     = 0.35f;
	float lightAng   = 135.0f; // degrees

	double spinAccum = 0.0;
	double flowX = 0.0, flowY = 0.0;

	ffglex::FFGLShader shader;
	ffglex::FFGLScreenQuad quad;
	GLint uResolution = -1, uPhase = -1, uScale = -1, uDetail = -1, uRough = -1, uWarp = -1, uTurb = -1,
	      uSwirl = -1, uSwirlRad = -1, uSym = -1, uRotation = -1, uFlowOff = -1, uChurn = -1,
	      uPaints = -1, uBlend = -1, uSoft = -1, uContrast = -1, uColorBy = -1, uCoverage = -1, uVeins = -1, uVeinCount = -1,
	      uCells = -1, uCellSize = -1, uCellTone = -1, uDroplets = -1, uDropSize = -1, uGloss = -1, uRelief = -1, uLight = -1;
};

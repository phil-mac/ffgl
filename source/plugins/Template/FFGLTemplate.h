#pragma once
#include "../_common/GeneratorCommon.h"

// Minimal generator skeleton: animated concentric rings. Copy this folder to start a new generator.
class FFGLTemplate : public gen::GeneratorPlugin
{
public:
	FFGLTemplate();

	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override;
	FFResult DeInitGL() override;
	FFResult SetFloatParameter( unsigned int index, float value ) override;
	float GetFloatParameter( unsigned int index ) override;

private:
	void UpdateVisibility( bool raiseEvent );

	// ---- Shape (plugin specific) ----
	float preset    = 0.0f;   // option
	float rings     = 8.0f;
	float radius    = 0.45f;
	float thickness = 0.5f;   // 0..1 fraction of ring spacing
	float rotation  = 0.0f;   // degrees
	float spin      = 0.0f;   // degrees per second
	float softness  = 1.0f;   // px
	double spinAccum = 0.0;

	ffglex::FFGLShader shader;
	ffglex::FFGLScreenQuad quad;
	GLint uResolution = -1, uRings = -1, uRadius = -1, uThickness = -1, uRotation = -1, uSoft = -1, uPhase = -1;
};

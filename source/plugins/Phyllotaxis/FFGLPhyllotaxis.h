#pragma once
#include <FFGLSDK.h>
#include <chrono>

// Phyllotaxis spiral generator. Full port of atjenkins/spiral-generator to an FFGL Source plugin,
// plus VJ-oriented extras: rotation/spin, auto animation, seamless palettes.
class FFGLPhyllotaxis : public CFFGLPlugin
{
public:
	FFGLPhyllotaxis();

	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override;
	FFResult DeInitGL() override;

	FFResult SetFloatParameter( unsigned int index, float value ) override;
	float GetFloatParameter( unsigned int index ) override;

private:
	void UpdateVisibility( bool raiseEvent );
	float EffectiveAngle() const;

	// ---- Pattern ----
	float preset    = 1.0f;       // option: 0 = custom, 1.. = named angles
	float angle     = 137.50776f; // degrees, used when preset == custom
	float seeds     = 300.0f;
	float scaleMode = 0.0f;       // 0 = fit radius, 1 = scale by c
	float radius    = 0.45f;      // fraction of the shorter frame side
	float scaleC    = 0.02f;
	float dotMin    = 0.006f;
	float dotMax    = 0.012f;
	float rotation  = 0.0f;       // degrees
	float spin      = 0.0f;       // degrees per second
	float softness  = 1.0f;       // edge anti-alias width in pixels

	// ---- Animation ----
	float animate   = 0.0f;       // bool
	float reveal    = 1.0f;       // manual 0..1
	float speed     = 100.0f;     // seeds per second
	float loop      = 1.0f;       // bool
	double revealSeeds = 0.0;     // running count while animating
	double spinAccum   = 0.0;     // accumulated spin angle, degrees
	std::chrono::steady_clock::time_point lastTick;
	bool haveTick = false;

	// ---- Color ----
	float palette   = 0.0f;       // option
	float colorBy   = 0.0f;       // option
	float arms      = 13.0f;
	float cycles    = 1.0f;
	float hueShift  = 0.0f;
	float saturation = 1.0f;
	float brightness = 1.0f;
	float colA[ 4 ] = { 0.55f, 0.80f, 1.0f, 1.0f }; // HSBA
	float colB[ 4 ] = { 0.95f, 0.70f, 1.0f, 1.0f }; // HSBA
	float bg[ 4 ]   = { 0.0f, 0.0f, 0.0f, 1.0f };   // HSBA

	ffglex::FFGLShader shader;
	ffglex::FFGLScreenQuad quad;

	GLint uResolution = -1, uAngle = -1, uSeeds = -1, uC = -1, uDotMin = -1, uDotMax = -1;
	GLint uRotation = -1, uSoft = -1, uShown = -1, uRMax = -1, uRhoScale = -1;
	GLint uPaletteMode = -1, uColA = -1, uColB = -1, uColC = -1;
	GLint uColorBy = -1, uArms = -1, uCycles = -1, uHueShift = -1, uSat = -1, uBri = -1, uBg = -1;
};

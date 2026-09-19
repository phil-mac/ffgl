#pragma once
// Shared building blocks for Resolume FFGL *Source* (generator) plugins.
//
// Derive from gen::GeneratorPlugin instead of CFFGLPlugin and you get, for free:
//   * an Animation parameter block  (Animate / Progress / Speed / Loop / Reset)  -> `phase` 0..1 each frame
//   * a Color parameter block       (Palette / Cycles / Hue Shift / Saturation / Brightness / Color A / Color B)
//   * a Background parameter block  (HSBA colour picker, alpha 0 = transparent layer)
//   * seamless palettes: every palette is periodic so palette(0) == palette(1); no hue seams anywhere
//   * GLSL helpers (kColorGLSL) to paste into the fragment shader: shade(t), background(), over(dst, col, cover)
//
// See ../Template/FFGLTemplate.cpp for the smallest complete example and ../Phyllotaxis for a full one.

#include <FFGLSDK.h>
#include <chrono>
#include <string>
#include <algorithm>
#include <cmath>
#include <climits>

namespace gen
{

// ---------------------------------------------------------------- palettes
enum PaletteID { PAL_RAINBOW, PAL_SUNSET, PAL_OCEAN, PAL_FIRE, PAL_FOREST, PAL_NEON, PAL_ICE, PAL_CANDY, PAL_GOLD, PAL_PASTEL, PAL_MONO, PAL_DUOTONE, PAL_COUNT };

inline const char* const kPaletteNames[ PAL_COUNT ] = {
	"Rainbow", "Sunset", "Ocean", "Fire", "Forest", "Neon", "Ice", "Candy", "Gold", "Pastel", "Mono (Color A)", "Duotone (A/B)"
};

// Three-stop palettes rendered A -> B -> C -> B -> A over one cycle (ping-pong), hence seamless.
struct Tri { float a[ 3 ], b[ 3 ], c[ 3 ]; };
inline const Tri kTri[ PAL_COUNT ] = {
	{ { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },                                     // rainbow  (hsv, unused)
	{ { 0.16f, 0.03f, 0.30f }, { 0.85f, 0.25f, 0.30f }, { 1.00f, 0.75f, 0.25f } }, // sunset
	{ { 0.02f, 0.08f, 0.25f }, { 0.00f, 0.55f, 0.65f }, { 0.60f, 0.95f, 0.95f } }, // ocean
	{ { 0.25f, 0.00f, 0.00f }, { 1.00f, 0.35f, 0.00f }, { 1.00f, 0.90f, 0.40f } }, // fire
	{ { 0.03f, 0.18f, 0.08f }, { 0.25f, 0.60f, 0.20f }, { 0.85f, 0.90f, 0.55f } }, // forest
	{ { 0.80f, 0.00f, 1.00f }, { 0.00f, 0.90f, 1.00f }, { 0.30f, 1.00f, 0.40f } }, // neon
	{ { 0.05f, 0.15f, 0.40f }, { 0.45f, 0.70f, 0.95f }, { 0.95f, 0.98f, 1.00f } }, // ice
	{ { 1.00f, 0.45f, 0.70f }, { 0.65f, 0.90f, 0.85f }, { 1.00f, 0.90f, 0.60f } }, // candy
	{ { 0.30f, 0.15f, 0.03f }, { 0.90f, 0.65f, 0.15f }, { 1.00f, 0.95f, 0.75f } }, // gold
	{ { 0.70f, 0.80f, 1.00f }, { 1.00f, 0.80f, 0.85f }, { 0.80f, 1.00f, 0.85f } }, // pastel
	{ { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },                                     // mono     (Color A)
	{ { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },                                     // duotone  (Color A/B)
};

inline void HSBtoRGB( const float hsba[ 4 ], float out[ 3 ] )
{
	float h = hsba[ 0 ] >= 1.0f ? 0.0f : hsba[ 0 ];
	ffglex::HSVtoRGB( h, hsba[ 1 ], hsba[ 2 ], out[ 0 ], out[ 1 ], out[ 2 ] );
}

// ---------------------------------------------------------------- GLSL
// Paste after "#version 410 core". Provides:
//   vec3 shade( float t )                 palette colour for t (any real; periodic), with cycles/hueShift/sat/bri applied
//   vec4 background()                     premultiplied background colour
//   vec4 over( vec4 dst, vec3 col, float cover )   composite an opaque colour over dst with coverage 0..1
//   vec3 hsv2rgb( vec3 )
// GLSL reserved words to avoid in your own code: half, sample, filter, input, output, common, partition, active.
inline const char* const kColorGLSL = R"(
uniform int   paletteMode;   // 0 rainbow, 1 tri-stop, 2 mono, 3 duotone
uniform vec3  colA;
uniform vec3  colB;
uniform vec3  colC;
uniform float cycles;
uniform float hueShift;
uniform float sat;
uniform float bri;
uniform vec4  bg;            // straight RGBA

vec3 hsv2rgb( vec3 c )
{
	vec3 p = abs( fract( c.xxx + vec3( 0.0, 2.0 / 3.0, 1.0 / 3.0 ) ) * 6.0 - 3.0 );
	return c.z * mix( vec3( 1.0 ), clamp( p - 1.0, 0.0, 1.0 ), c.y );
}

vec3 paletteRaw( float t )
{
	t = fract( t );
	if( paletteMode == 0 )
		return hsv2rgb( vec3( t, 1.0, 1.0 ) );
	float u = 0.5 - 0.5 * cos( 6.28318530718 * t );   // smooth ping-pong, seamless
	if( paletteMode == 2 )
		return colA * mix( 0.45, 1.0, u );
	if( paletteMode == 3 )
		return mix( colA, colB, u );
	return u < 0.5 ? mix( colA, colB, u * 2.0 ) : mix( colB, colC, u * 2.0 - 1.0 );
}

vec3 shade( float t )
{
	vec3 col = paletteRaw( t * cycles + hueShift );
	float l = dot( col, vec3( 0.299, 0.587, 0.114 ) );
	col = mix( vec3( l ), col, sat );
	return clamp( col * bri, 0.0, 1.0 );
}

vec4 background() { return vec4( bg.rgb * bg.a, bg.a ); }

vec4 over( vec4 dst, vec3 col, float cover )
{
	cover = clamp( cover, 0.0, 1.0 );
	return vec4( mix( dst.rgb, col, cover ), mix( dst.a, 1.0, cover ) );
}
)";

inline const char* const kVertexGLSL = R"(#version 410 core
layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec2 vUV;
out vec2 uv;
void main()
{
	gl_Position = vPosition;
	uv = vUV;
}
)";

// ---------------------------------------------------------------- base class
class GeneratorPlugin : public CFFGLPlugin
{
public:
	GeneratorPlugin()
	{
		SetMinInputs( 0 );
		SetMaxInputs( 0 );
	}

protected:
	// ---------- parameter blocks (call from your constructor; each returns the next free index) ----------
	unsigned AddAnimationParams( unsigned first, const char* group = "Animation" )
	{
		animFirst = first;
		SetParamInfo( first + A_ANIMATE, "Animate", FF_TYPE_BOOLEAN, anim.animate != 0.0f );
		SetParamInfo( first + A_PROGRESS, "Progress", FF_TYPE_STANDARD, anim.progress );
		SetParamRange( first + A_PROGRESS, 0.0f, 1.0f );
		SetParamInfo( first + A_SPEED, "Speed", FF_TYPE_STANDARD, anim.speed );
		SetParamRange( first + A_SPEED, 0.0f, 4.0f ); // cycles per second
		SetParamInfo( first + A_LOOP, "Loop", FF_TYPE_BOOLEAN, anim.loop != 0.0f );
		SetParamInfo( first + A_RESET, "Reset", FF_TYPE_EVENT, false );
		for( unsigned i = 0; i < A_COUNT; ++i )
			SetParamGroup( first + i, group );
		return first + A_COUNT;
	}

	unsigned AddColorParams( unsigned first, const char* group = "Color" )
	{
		colorFirst = first;
		SetOptionParamInfo( first + C_PALETTE, "Palette", PAL_COUNT, color.palette );
		for( int i = 0; i < PAL_COUNT; ++i )
			SetParamElementInfo( first + C_PALETTE, i, kPaletteNames[ i ], (float)i );
		SetParamInfo( first + C_CYCLES, "Cycles", FF_TYPE_INTEGER, color.cycles );
		SetParamRange( first + C_CYCLES, 1.0f, 8.0f );
		SetParamInfo( first + C_HUESHIFT, "Hue Shift", FF_TYPE_STANDARD, color.hueShift );
		SetParamRange( first + C_HUESHIFT, 0.0f, 1.0f );
		SetParamInfo( first + C_SAT, "Saturation", FF_TYPE_STANDARD, color.sat );
		SetParamRange( first + C_SAT, 0.0f, 2.0f );
		SetParamInfo( first + C_BRI, "Brightness", FF_TYPE_STANDARD, color.bri );
		SetParamRange( first + C_BRI, 0.0f, 2.0f );
		SetParamInfo( first + C_COLA + 0, "Color A", FF_TYPE_HUE, color.colA[ 0 ] );
		SetParamInfo( first + C_COLA + 1, "Color A Sat", FF_TYPE_SATURATION, color.colA[ 1 ] );
		SetParamInfo( first + C_COLA + 2, "Color A Bri", FF_TYPE_BRIGHTNESS, color.colA[ 2 ] );
		SetParamInfo( first + C_COLA + 3, "Color A Alpha", FF_TYPE_ALPHA, color.colA[ 3 ] );
		SetParamInfo( first + C_COLB + 0, "Color B", FF_TYPE_HUE, color.colB[ 0 ] );
		SetParamInfo( first + C_COLB + 1, "Color B Sat", FF_TYPE_SATURATION, color.colB[ 1 ] );
		SetParamInfo( first + C_COLB + 2, "Color B Bri", FF_TYPE_BRIGHTNESS, color.colB[ 2 ] );
		SetParamInfo( first + C_COLB + 3, "Color B Alpha", FF_TYPE_ALPHA, color.colB[ 3 ] );
		for( unsigned i = 0; i < C_COUNT; ++i )
			SetParamGroup( first + i, group );
		return first + C_COUNT;
	}

	unsigned AddBackgroundParams( unsigned first, const char* group = "Background" )
	{
		bgFirst = first;
		SetParamInfo( first + 0, "Background", FF_TYPE_HUE, bg[ 0 ] );
		SetParamInfo( first + 1, "Background Sat", FF_TYPE_SATURATION, bg[ 1 ] );
		SetParamInfo( first + 2, "Background Bri", FF_TYPE_BRIGHTNESS, bg[ 2 ] );
		SetParamInfo( first + 3, "Background Alpha", FF_TYPE_ALPHA, bg[ 3 ] );
		for( unsigned i = 0; i < 4; ++i )
			SetParamGroup( first + i, group );
		return first + 4;
	}

	// Call at the end of your constructor with false, and whenever a mode changes with true.
	void UpdateCommonVisibility( bool raiseEvent )
	{
		if( animFirst != UINT_MAX )
		{
			const bool a = anim.animate != 0.0f;
			SetParamVisibility( animFirst + A_PROGRESS, !a, raiseEvent );
			SetParamVisibility( animFirst + A_SPEED, a, raiseEvent );
			SetParamVisibility( animFirst + A_LOOP, a, raiseEvent );
			SetParamVisibility( animFirst + A_RESET, a, raiseEvent );
		}
		if( colorFirst != UINT_MAX )
		{
			const int pal = (int)color.palette;
			SetParamVisibility( colorFirst + C_COLA, pal == PAL_MONO || pal == PAL_DUOTONE, raiseEvent ); // HSBA group follows its hue param
			SetParamVisibility( colorFirst + C_COLB, pal == PAL_DUOTONE, raiseEvent );
		}
	}

	// Route these from your SetFloatParameter / GetFloatParameter first; they return true when the index was theirs.
	bool SetCommonParam( unsigned index, float value )
	{
		if( animFirst != UINT_MAX && index >= animFirst && index < animFirst + A_COUNT )
		{
			switch( index - animFirst )
			{
			case A_ANIMATE:
				anim.animate = value;
				phaseAccum   = 0.0;
				UpdateCommonVisibility( true );
				break;
			case A_PROGRESS: anim.progress = value; break;
			case A_SPEED:    anim.speed = value; break;
			case A_LOOP:     anim.loop = value; break;
			case A_RESET:
				if( value != 0.0f )
					phaseAccum = 0.0;
				break;
			}
			return true;
		}
		if( colorFirst != UINT_MAX && index >= colorFirst && index < colorFirst + C_COUNT )
		{
			const unsigned i = index - colorFirst;
			if( i == C_PALETTE )       { color.palette = value; UpdateCommonVisibility( true ); }
			else if( i == C_CYCLES )   color.cycles = value;
			else if( i == C_HUESHIFT ) color.hueShift = value;
			else if( i == C_SAT )      color.sat = value;
			else if( i == C_BRI )      color.bri = value;
			else if( i < C_COLB )      color.colA[ i - C_COLA ] = value;
			else                       color.colB[ i - C_COLB ] = value;
			return true;
		}
		if( bgFirst != UINT_MAX && index >= bgFirst && index < bgFirst + 4 )
		{
			bg[ index - bgFirst ] = value;
			return true;
		}
		return false;
	}

	bool GetCommonParam( unsigned index, float& out )
	{
		if( animFirst != UINT_MAX && index >= animFirst && index < animFirst + A_COUNT )
		{
			switch( index - animFirst )
			{
			case A_ANIMATE:  out = anim.animate; break;
			case A_PROGRESS: out = anim.progress; break;
			case A_SPEED:    out = anim.speed; break;
			case A_LOOP:     out = anim.loop; break;
			default:         out = 0.0f; break;
			}
			return true;
		}
		if( colorFirst != UINT_MAX && index >= colorFirst && index < colorFirst + C_COUNT )
		{
			const unsigned i = index - colorFirst;
			if( i == C_PALETTE )       out = color.palette;
			else if( i == C_CYCLES )   out = color.cycles;
			else if( i == C_HUESHIFT ) out = color.hueShift;
			else if( i == C_SAT )      out = color.sat;
			else if( i == C_BRI )      out = color.bri;
			else if( i < C_COLB )      out = color.colA[ i - C_COLA ];
			else                       out = color.colB[ i - C_COLB ];
			return true;
		}
		if( bgFirst != UINT_MAX && index >= bgFirst && index < bgFirst + 4 )
		{
			out = bg[ index - bgFirst ];
			return true;
		}
		return false;
	}

	// ---------- per frame ----------
	// Call first thing in ProcessOpenGL. Updates dt (seconds), time (seconds since InitGL) and phase (0..1).
	void Tick()
	{
		auto now = std::chrono::steady_clock::now();
		dt = haveTick ? std::chrono::duration< double >( now - lastTick ).count() : 0.0;
		lastTick = now;
		haveTick = true;
		dt   = std::min( std::max( dt, 0.0 ), 0.25 );
		time += dt;

		if( anim.animate != 0.0f )
		{
			phaseAccum += anim.speed * dt;
			if( anim.loop != 0.0f )
				phaseAccum = phaseAccum - std::floor( phaseAccum );
			else
				phaseAccum = std::min( phaseAccum, 1.0 );
			phase = phaseAccum;
		}
		else
		{
			phase = anim.progress;
		}
	}

	// ---------- colour uniforms ----------
	void FindColorUniforms( ffglex::FFGLShader& shader )
	{
		uPaletteMode = shader.FindUniform( "paletteMode" );
		uColA        = shader.FindUniform( "colA" );
		uColB        = shader.FindUniform( "colB" );
		uColC        = shader.FindUniform( "colC" );
		uCycles      = shader.FindUniform( "cycles" );
		uHueShift    = shader.FindUniform( "hueShift" );
		uSat         = shader.FindUniform( "sat" );
		uBri         = shader.FindUniform( "bri" );
		uBg          = shader.FindUniform( "bg" );
	}

	// Call inside a ScopedShaderBinding.
	void UploadColorUniforms()
	{
		float a[ 3 ] = { 0, 0, 0 }, b[ 3 ] = { 0, 0, 0 }, c[ 3 ] = { 0, 0, 0 };
		const int pal = std::min( std::max( (int)color.palette, 0 ), (int)PAL_COUNT - 1 );
		int mode = 1;
		if( pal == PAL_RAINBOW )      mode = 0;
		else if( pal == PAL_MONO )    { mode = 2; HSBtoRGB( color.colA, a ); }
		else if( pal == PAL_DUOTONE ) { mode = 3; HSBtoRGB( color.colA, a ); HSBtoRGB( color.colB, b ); }
		else
		{
			for( int i = 0; i < 3; ++i ) { a[ i ] = kTri[ pal ].a[ i ]; b[ i ] = kTri[ pal ].b[ i ]; c[ i ] = kTri[ pal ].c[ i ]; }
		}
		float bgRGB[ 3 ];
		HSBtoRGB( bg, bgRGB );

		glUniform1i( uPaletteMode, mode );
		glUniform3f( uColA, a[ 0 ], a[ 1 ], a[ 2 ] );
		glUniform3f( uColB, b[ 0 ], b[ 1 ], b[ 2 ] );
		glUniform3f( uColC, c[ 0 ], c[ 1 ], c[ 2 ] );
		glUniform1f( uCycles, std::max( std::floor( color.cycles ), 1.0f ) );
		glUniform1f( uHueShift, color.hueShift );
		glUniform1f( uSat, color.sat );
		glUniform1f( uBri, color.bri );
		glUniform4f( uBg, bgRGB[ 0 ], bgRGB[ 1 ], bgRGB[ 2 ], bg[ 3 ] );
	}

	// ---------- state you may read ----------
	double dt    = 0.0;
	double time  = 0.0;
	double phase = 0.0;

	struct { float animate = 0.0f, progress = 1.0f, speed = 0.25f, loop = 1.0f; } anim;
	struct
	{
		float palette = 0.0f, cycles = 1.0f, hueShift = 0.0f, sat = 1.0f, bri = 1.0f;
		float colA[ 4 ] = { 0.55f, 0.80f, 1.0f, 1.0f };
		float colB[ 4 ] = { 0.95f, 0.70f, 1.0f, 1.0f };
	} color;
	float bg[ 4 ] = { 0.0f, 0.0f, 0.0f, 1.0f };

private:
	enum { A_ANIMATE, A_PROGRESS, A_SPEED, A_LOOP, A_RESET, A_COUNT };
	enum { C_PALETTE, C_CYCLES, C_HUESHIFT, C_SAT, C_BRI, C_COLA, C_COLB = C_COLA + 4, C_COUNT = C_COLB + 4 };

	unsigned animFirst = UINT_MAX, colorFirst = UINT_MAX, bgFirst = UINT_MAX;
	bool haveTick = false;
	std::chrono::steady_clock::time_point lastTick;
	double phaseAccum = 0.0;
	GLint uPaletteMode = -1, uColA = -1, uColB = -1, uColC = -1, uCycles = -1, uHueShift = -1, uSat = -1, uBri = -1, uBg = -1;
};

} // namespace gen

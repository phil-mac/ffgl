#include "FFGLPhyllotaxis.h"
#include <cmath>
#include <algorithm>
#include <string>
using namespace ffglex;

enum ParamType : FFUInt32
{
	// Pattern
	PT_PRESET,
	PT_ANGLE,
	PT_SEEDS,
	PT_SCALEMODE,
	PT_RADIUS,
	PT_SCALEC,
	PT_DOTMIN,
	PT_DOTMAX,
	PT_ROTATION,
	PT_SPIN,
	PT_SOFT,
	// Animation
	PT_ANIMATE,
	PT_REVEAL,
	PT_SPEED,
	PT_LOOP,
	PT_RESET,
	// Color
	PT_PALETTE,
	PT_COLORBY,
	PT_ARMS,
	PT_CYCLES,
	PT_HUESHIFT,
	PT_SAT,
	PT_BRI,
	PT_COLA_H, PT_COLA_S, PT_COLA_B, PT_COLA_A,
	PT_COLB_H, PT_COLB_S, PT_COLB_B, PT_COLB_A,
	PT_BG_H, PT_BG_S, PT_BG_B, PT_BG_A,
};

static CFFGLPluginInfo PluginInfo(
	PluginFactory< FFGLPhyllotaxis >,
	"PHYL",
	"Phyllotaxis",
	2, 1,   // API version
	1, 100, // plugin version 1.1
	FF_SOURCE,
	"Phyllotaxis (golden angle) spiral generator with seamless palettes",
	"Port of atjenkins/spiral-generator"
);

// Angle presets, same list as the Python app's config.py. Index 0 = Custom (use Angle slider).
static const struct { const char* name; float deg; } kPresets[] = {
	{ "Custom",             0.0f      },
	{ "Golden angle",       137.507764f },
	{ "Trivalent",          39.6f     },
	{ "Monostichous",       47.3f     },
	{ "Bijugate",           68.8f     },
	{ "Spiro-decussate",    83.7f     },
	{ "Lucas angle",        99.5f     },
	{ "Silver angle",       99.7356f  },
	{ "Bronze angle",       146.3099f },
	{ "Plastic angle",      151.13f   },
	{ "Spiro-distichous",   167.4f    },
	{ "Golden complement",  222.492f  },
};
static const int kNumPresets = sizeof( kPresets ) / sizeof( kPresets[ 0 ] );

// Three-stop palettes. Rendered as A -> B -> C -> B -> A around one cycle, so they are seamless.
struct Tri { float a[ 3 ], b[ 3 ], c[ 3 ]; };
enum PaletteID { PAL_RAINBOW, PAL_SUNSET, PAL_OCEAN, PAL_FIRE, PAL_FOREST, PAL_NEON, PAL_ICE, PAL_CANDY, PAL_GOLD, PAL_PASTEL, PAL_MONO, PAL_DUOTONE, PAL_COUNT };
static const char* kPaletteNames[ PAL_COUNT ] = {
	"Rainbow", "Sunset", "Ocean", "Fire", "Forest", "Neon", "Ice", "Candy", "Gold", "Pastel", "Mono (Color A)", "Duotone (A/B)"
};
static const Tri kTri[ PAL_COUNT ] = {
	{ { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },                                           // rainbow (unused)
	{ { 0.16f, 0.03f, 0.30f }, { 0.85f, 0.25f, 0.30f }, { 1.00f, 0.75f, 0.25f } },       // sunset
	{ { 0.02f, 0.08f, 0.25f }, { 0.00f, 0.55f, 0.65f }, { 0.60f, 0.95f, 0.95f } },       // ocean
	{ { 0.25f, 0.00f, 0.00f }, { 1.00f, 0.35f, 0.00f }, { 1.00f, 0.90f, 0.40f } },       // fire
	{ { 0.03f, 0.18f, 0.08f }, { 0.25f, 0.60f, 0.20f }, { 0.85f, 0.90f, 0.55f } },       // forest
	{ { 0.80f, 0.00f, 1.00f }, { 0.00f, 0.90f, 1.00f }, { 0.30f, 1.00f, 0.40f } },       // neon
	{ { 0.05f, 0.15f, 0.40f }, { 0.45f, 0.70f, 0.95f }, { 0.95f, 0.98f, 1.00f } },       // ice
	{ { 1.00f, 0.45f, 0.70f }, { 0.65f, 0.90f, 0.85f }, { 1.00f, 0.90f, 0.60f } },       // candy
	{ { 0.30f, 0.15f, 0.03f }, { 0.90f, 0.65f, 0.15f }, { 1.00f, 0.95f, 0.75f } },       // gold
	{ { 0.70f, 0.80f, 1.00f }, { 1.00f, 0.80f, 0.85f }, { 0.80f, 1.00f, 0.85f } },       // pastel
	{ { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },                                           // mono (from Color A)
	{ { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },                                           // duotone (from A/B)
};

enum ColorByID { CB_ORIGINAL, CB_ANGLE, CB_RADIUS, CB_INDEX, CB_ARMS, CB_COUNT };
static const char* kColorByNames[ CB_COUNT ] = { "Original (MATLAB)", "Angle", "Radius", "Index", "Parastichy arms" };

static void HSBtoRGB( const float hsba[ 4 ], float out[ 3 ] )
{
	float h = hsba[ 0 ] >= 1.0f ? 0.0f : hsba[ 0 ];
	HSVtoRGB( h, hsba[ 1 ], hsba[ 2 ], out[ 0 ], out[ 1 ], out[ 2 ] );
}

static const char vertexShaderCode[] = R"(#version 410 core
layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec2 vUV;
out vec2 uv;
void main()
{
	gl_Position = vPosition;
	uv = vUV;
}
)";

static const char fragmentShaderCode[] = R"(#version 410 core
uniform vec2  resolution;
uniform float angleDeg;    // divergence angle
uniform float seeds;       // N
uniform float c;           // rho = c * sqrt(n)
uniform float dotMin;      // diameter at centre (frame units)
uniform float dotMax;      // diameter at rim
uniform float rotation;    // radians
uniform float softPx;      // anti-alias width in pixels
uniform float shown;       // fractional number of seeds revealed
uniform float rMax;        // radius of the outermost seed centre
uniform float rhoScale;    // maps our rho to the Python app's data units for the original hue formula

uniform int   paletteMode; // 0 rainbow, 1 tri-stop, 2 mono, 3 duotone
uniform vec3  colA;
uniform vec3  colB;
uniform vec3  colC;
uniform int   colorBy;     // 0 original, 1 angle, 2 radius, 3 index, 4 arms
uniform float arms;
uniform float cycles;
uniform float hueShift;
uniform float sat;
uniform float bri;
uniform vec4  bg;          // straight (non-premultiplied) RGBA

in vec2 uv;
out vec4 fragColor;

vec3 hsv2rgb( vec3 c )
{
	vec3 p = abs( fract( c.xxx + vec3( 0.0, 2.0 / 3.0, 1.0 / 3.0 ) ) * 6.0 - 3.0 );
	return c.z * mix( vec3( 1.0 ), clamp( p - 1.0, 0.0, 1.0 ), c.y );
}

// Seamless palette: t in [0,1) maps to a colour, and palette(0) == palette(1).
vec3 palette( float t )
{
	t = fract( t );
	if( paletteMode == 0 )
		return hsv2rgb( vec3( t, 1.0, 1.0 ) );

	// Smooth triangle wave 0 -> 1 -> 0 over one cycle, so the gradient ping-pongs and never seams.
	float u = 0.5 - 0.5 * cos( 6.28318530718 * t );
	if( paletteMode == 2 )
		return colA * mix( 0.45, 1.0, u );
	if( paletteMode == 3 )
		return mix( colA, colB, u );
	// tri-stop: A -> B for u < 0.5, B -> C for u > 0.5
	return u < 0.5 ? mix( colA, colB, u * 2.0 ) : mix( colB, colC, u * 2.0 - 1.0 );
}

vec3 adjust( vec3 col )
{
	float l = dot( col, vec3( 0.299, 0.587, 0.114 ) );
	col = mix( vec3( l ), col, sat );
	return clamp( col * bri, 0.0, 1.0 );
}

void main()
{
	float px = 1.0 / min( resolution.x, resolution.y );
	vec2 p = ( uv - 0.5 ) * resolution * px;                 // shorter side spans -0.5..0.5
	// rotate the frame the opposite way so the pattern appears rotated by +rotation
	float cs = cos( -rotation ), sn = sin( -rotation );
	p = vec2( cs * p.x - sn * p.y, sn * p.x + cs * p.y );

	float r     = length( p );
	float N     = max( seeds, 1.0 );
	float aRad  = radians( angleDeg );
	float soft  = max( softPx, 0.01 ) * px;
	float halfR = max( dotMax, dotMin ) * 0.5 + soft;

	vec4 outCol = vec4( bg.rgb * bg.a, bg.a );               // premultiplied background

	if( r <= rMax + halfR && shown > 0.0 )
	{
		// Only seeds whose ring radius is within one dot of this pixel can cover it.
		float lo = ( r - halfR ) / c;
		float hi = ( r + halfR ) / c;
		int nLo = int( max( floor( lo * lo ), 0.0 ) );
		int nHi = int( min( ceil( hi * hi ), ceil( shown ) - 1.0 ) );
		nHi = min( nHi, nLo + 1500 );

		for( int i = nLo; i <= nHi; i++ )
		{
			float n = float( i );
			float appear = clamp( shown - n, 0.0, 1.0 );      // fractional reveal grows each new seed in
			if( appear <= 0.0 )
				continue;
			appear = appear * appear * ( 3.0 - 2.0 * appear ); // smoothstep
			float rho  = c * sqrt( n );
			float th   = n * aRad;
			vec2 seed  = rho * vec2( cos( th ), sin( th ) );
			float diam = mix( dotMin, dotMax, n / max( N - 1.0, 1.0 ) ) * appear;
			float d    = length( p - seed ) - diam * 0.5;
			float cover = 1.0 - smoothstep( -soft * 0.5, soft * 0.5, d );
			if( cover <= 0.0 )
				continue;

			float t;
			if( colorBy == 0 )      t = ( n * angleDeg - rho * rhoScale ) / 360.0;
			else if( colorBy == 1 ) t = n * angleDeg / 360.0;
			else if( colorBy == 2 ) t = rho / max( rMax, 1e-5 );
			else if( colorBy == 3 ) t = n / max( N - 1.0, 1.0 );
			else                    t = mod( n, arms ) / arms;
			t = t * cycles + hueShift;

			vec3 col = adjust( palette( t ) );
			// later (outer) seeds draw on top, like the original's draw order
			outCol = vec4( mix( outCol.rgb, col, cover ), mix( outCol.a, 1.0, cover ) );
		}
	}
	fragColor = outCol;
}
)";

// Used by tests/shader_compile_test.cpp
static std::string GetFragmentShaderSource() { return fragmentShaderCode; }

FFGLPhyllotaxis::FFGLPhyllotaxis()
{
	SetMinInputs( 0 );
	SetMaxInputs( 0 );

	// ---- Pattern ----
	SetOptionParamInfo( PT_PRESET, "Angle Preset", kNumPresets, preset );
	for( int i = 0; i < kNumPresets; ++i )
		SetParamElementInfo( PT_PRESET, i, kPresets[ i ].name, (float)i );
	SetParamInfof( PT_ANGLE, "Angle", FF_TYPE_STANDARD );
	SetParamRange( PT_ANGLE, 0.0f, 360.0f );
	SetParamInfof( PT_SEEDS, "Seeds", FF_TYPE_INTEGER );
	SetParamRange( PT_SEEDS, 1.0f, 3000.0f );
	SetOptionParamInfo( PT_SCALEMODE, "Scaling", 2, scaleMode );
	SetParamElementInfo( PT_SCALEMODE, 0, "Fit radius R", 0.0f );
	SetParamElementInfo( PT_SCALEMODE, 1, "Scale by c", 1.0f );
	SetParamInfof( PT_RADIUS, "Radius R", FF_TYPE_STANDARD );
	SetParamRange( PT_RADIUS, 0.02f, 1.0f );
	SetParamInfof( PT_SCALEC, "Scale c", FF_TYPE_STANDARD );
	SetParamRange( PT_SCALEC, 0.001f, 0.1f );
	SetParamInfof( PT_DOTMIN, "Dot Min", FF_TYPE_STANDARD );
	SetParamRange( PT_DOTMIN, 0.0f, 0.1f );
	SetParamInfof( PT_DOTMAX, "Dot Max", FF_TYPE_STANDARD );
	SetParamRange( PT_DOTMAX, 0.0f, 0.1f );
	SetParamInfof( PT_ROTATION, "Rotation", FF_TYPE_STANDARD );
	SetParamRange( PT_ROTATION, 0.0f, 360.0f );
	SetParamInfof( PT_SPIN, "Spin", FF_TYPE_STANDARD );
	SetParamRange( PT_SPIN, -180.0f, 180.0f );
	SetParamInfof( PT_SOFT, "Edge Softness", FF_TYPE_STANDARD );
	SetParamRange( PT_SOFT, 0.0f, 4.0f );
	for( unsigned int i = PT_PRESET; i <= PT_SOFT; ++i )
		SetParamGroup( i, "Pattern" );

	// ---- Animation ----
	SetParamInfo( PT_ANIMATE, "Animate", FF_TYPE_BOOLEAN, animate != 0.0f );
	SetParamInfof( PT_REVEAL, "Reveal", FF_TYPE_STANDARD );
	SetParamRange( PT_REVEAL, 0.0f, 1.0f );
	SetParamInfof( PT_SPEED, "Speed", FF_TYPE_STANDARD );
	SetParamRange( PT_SPEED, 1.0f, 3000.0f );
	SetParamInfo( PT_LOOP, "Loop", FF_TYPE_BOOLEAN, loop != 0.0f );
	SetParamInfof( PT_RESET, "Reset", FF_TYPE_EVENT );
	for( unsigned int i = PT_ANIMATE; i <= PT_RESET; ++i )
		SetParamGroup( i, "Animation" );

	// ---- Color ----
	SetOptionParamInfo( PT_PALETTE, "Palette", PAL_COUNT, palette );
	for( int i = 0; i < PAL_COUNT; ++i )
		SetParamElementInfo( PT_PALETTE, i, kPaletteNames[ i ], (float)i );
	SetOptionParamInfo( PT_COLORBY, "Color By", CB_COUNT, colorBy );
	for( int i = 0; i < CB_COUNT; ++i )
		SetParamElementInfo( PT_COLORBY, i, kColorByNames[ i ], (float)i );
	SetParamInfof( PT_ARMS, "Arms", FF_TYPE_INTEGER );
	SetParamRange( PT_ARMS, 1.0f, 89.0f );
	SetParamInfof( PT_CYCLES, "Cycles", FF_TYPE_INTEGER );
	SetParamRange( PT_CYCLES, 1.0f, 8.0f );
	SetParamInfof( PT_HUESHIFT, "Hue Shift", FF_TYPE_STANDARD );
	SetParamRange( PT_HUESHIFT, 0.0f, 1.0f );
	SetParamInfof( PT_SAT, "Saturation", FF_TYPE_STANDARD );
	SetParamRange( PT_SAT, 0.0f, 2.0f );
	SetParamInfof( PT_BRI, "Brightness", FF_TYPE_STANDARD );
	SetParamRange( PT_BRI, 0.0f, 2.0f );
	SetParamInfof( PT_COLA_H, "Color A", FF_TYPE_HUE );
	SetParamInfof( PT_COLA_S, "Color A Sat", FF_TYPE_SATURATION );
	SetParamInfof( PT_COLA_B, "Color A Bri", FF_TYPE_BRIGHTNESS );
	SetParamInfof( PT_COLA_A, "Color A Alpha", FF_TYPE_ALPHA );
	SetParamInfof( PT_COLB_H, "Color B", FF_TYPE_HUE );
	SetParamInfof( PT_COLB_S, "Color B Sat", FF_TYPE_SATURATION );
	SetParamInfof( PT_COLB_B, "Color B Bri", FF_TYPE_BRIGHTNESS );
	SetParamInfof( PT_COLB_A, "Color B Alpha", FF_TYPE_ALPHA );
	for( unsigned int i = PT_PALETTE; i <= PT_COLB_A; ++i )
		SetParamGroup( i, "Color" );

	SetParamInfof( PT_BG_H, "Background", FF_TYPE_HUE );
	SetParamInfof( PT_BG_S, "Background Sat", FF_TYPE_SATURATION );
	SetParamInfof( PT_BG_B, "Background Bri", FF_TYPE_BRIGHTNESS );
	SetParamInfof( PT_BG_A, "Background Alpha", FF_TYPE_ALPHA );
	for( unsigned int i = PT_BG_H; i <= PT_BG_A; ++i )
		SetParamGroup( i, "Background" );

	UpdateVisibility( false );
	FFGLLog::LogToHost( "Created Phyllotaxis generator" );
}

void FFGLPhyllotaxis::UpdateVisibility( bool raiseEvent )
{
	const bool custom = (int)preset == 0;
	const bool fit    = (int)scaleMode == 0;
	const bool anim   = animate != 0.0f;
	const int  pal    = (int)palette;
	const bool usesA  = pal == PAL_MONO || pal == PAL_DUOTONE;
	const bool usesB  = pal == PAL_DUOTONE;

	SetParamVisibility( PT_ANGLE,  custom, raiseEvent );
	SetParamVisibility( PT_RADIUS, fit,    raiseEvent );
	SetParamVisibility( PT_SCALEC, !fit,   raiseEvent );
	SetParamVisibility( PT_REVEAL, !anim,  raiseEvent );
	SetParamVisibility( PT_SPEED,  anim,   raiseEvent );
	SetParamVisibility( PT_LOOP,   anim,   raiseEvent );
	SetParamVisibility( PT_RESET,  anim,   raiseEvent );
	SetParamVisibility( PT_ARMS,   (int)colorBy == CB_ARMS, raiseEvent );
	SetParamVisibility( PT_COLA_H, usesA,  raiseEvent ); // HSBA group is controlled via the hue param
	SetParamVisibility( PT_COLB_H, usesB,  raiseEvent );
}

float FFGLPhyllotaxis::EffectiveAngle() const
{
	int i = (int)preset;
	if( i <= 0 || i >= kNumPresets )
		return angle;
	return kPresets[ i ].deg;
}

FFResult FFGLPhyllotaxis::InitGL( const FFGLViewportStruct* vp )
{
	if( !shader.Compile( vertexShaderCode, fragmentShaderCode ) )
	{
		FFGLLog::LogToHost( "Phyllotaxis: shader compile failed" );
		DeInitGL();
		return FF_FAIL;
	}
	if( !quad.Initialise() )
	{
		DeInitGL();
		return FF_FAIL;
	}

	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	uResolution  = shader.FindUniform( "resolution" );
	uAngle       = shader.FindUniform( "angleDeg" );
	uSeeds       = shader.FindUniform( "seeds" );
	uC           = shader.FindUniform( "c" );
	uDotMin      = shader.FindUniform( "dotMin" );
	uDotMax      = shader.FindUniform( "dotMax" );
	uRotation    = shader.FindUniform( "rotation" );
	uSoft        = shader.FindUniform( "softPx" );
	uShown       = shader.FindUniform( "shown" );
	uRMax        = shader.FindUniform( "rMax" );
	uRhoScale    = shader.FindUniform( "rhoScale" );
	uPaletteMode = shader.FindUniform( "paletteMode" );
	uColA        = shader.FindUniform( "colA" );
	uColB        = shader.FindUniform( "colB" );
	uColC        = shader.FindUniform( "colC" );
	uColorBy     = shader.FindUniform( "colorBy" );
	uArms        = shader.FindUniform( "arms" );
	uCycles      = shader.FindUniform( "cycles" );
	uHueShift    = shader.FindUniform( "hueShift" );
	uSat         = shader.FindUniform( "sat" );
	uBri         = shader.FindUniform( "bri" );
	uBg          = shader.FindUniform( "bg" );

	haveTick = false;
	return CFFGLPlugin::InitGL( vp );
}

FFResult FFGLPhyllotaxis::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	// ---- time step (wall clock; independent of whether the host calls SetTime) ----
	auto now = std::chrono::steady_clock::now();
	double dt = 0.0;
	if( haveTick )
		dt = std::chrono::duration< double >( now - lastTick ).count();
	lastTick = now;
	haveTick = true;
	dt = std::min( std::max( dt, 0.0 ), 0.25 );

	const float N = std::max( std::floor( seeds ), 1.0f );

	// ---- animation state ----
	float shownF;
	if( animate != 0.0f )
	{
		revealSeeds += speed * dt;
		const double hold = std::max( 0.75 * speed, 1.0 ); // pause ~0.75 s on the full pattern before looping
		if( loop != 0.0f )
		{
			if( revealSeeds >= N + hold )
				revealSeeds = 0.0;
		}
		else
		{
			revealSeeds = std::min( revealSeeds, (double)N );
		}
		shownF = (float)std::min( revealSeeds, (double)N );
	}
	else
	{
		shownF = reveal * N;
	}

	spinAccum = std::fmod( spinAccum + spin * dt, 360.0 );
	const float rotRad = (float)( ( rotation + spinAccum ) * M_PI / 180.0 );

	// ---- scaling, mirroring the Python app's two modes ----
	const float maxN = std::max( N - 1.0f, 1.0f );
	float c, rMax, rhoScale;
	if( (int)scaleMode == 0 )
	{
		// Fit radius: shrink so the outermost seed's edge (not centre) touches R.
		const float effR = std::max( radius - dotMax * 0.5f, 1e-4f );
		c    = effR / std::sqrt( maxN );
		rMax = effR;
		rhoScale = 12.0f / effR; // Python default R = 12 data units
	}
	else
	{
		c    = scaleC;
		rMax = c * std::sqrt( maxN );
		rhoScale = 2.0f / std::max( c, 1e-6f ); // Python default c = 2 data units
	}

	// ---- palette colours ----
	float a[ 3 ], b[ 3 ], cc[ 3 ];
	const int pal = std::min( std::max( (int)palette, 0 ), PAL_COUNT - 1 );
	int paletteMode;
	if( pal == PAL_RAINBOW )
	{
		paletteMode = 0;
		a[ 0 ] = a[ 1 ] = a[ 2 ] = b[ 0 ] = b[ 1 ] = b[ 2 ] = cc[ 0 ] = cc[ 1 ] = cc[ 2 ] = 0.0f;
	}
	else if( pal == PAL_MONO )
	{
		paletteMode = 2;
		HSBtoRGB( colA, a );
		b[ 0 ] = b[ 1 ] = b[ 2 ] = cc[ 0 ] = cc[ 1 ] = cc[ 2 ] = 0.0f;
	}
	else if( pal == PAL_DUOTONE )
	{
		paletteMode = 3;
		HSBtoRGB( colA, a );
		HSBtoRGB( colB, b );
		cc[ 0 ] = cc[ 1 ] = cc[ 2 ] = 0.0f;
	}
	else
	{
		paletteMode = 1;
		for( int i = 0; i < 3; ++i )
		{
			a[ i ]  = kTri[ pal ].a[ i ];
			b[ i ]  = kTri[ pal ].b[ i ];
			cc[ i ] = kTri[ pal ].c[ i ];
		}
	}
	float bgRGB[ 3 ];
	HSBtoRGB( bg, bgRGB );

	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	glUniform2f( uResolution, (float)currentViewport.width, (float)currentViewport.height );
	glUniform1f( uAngle, EffectiveAngle() );
	glUniform1f( uSeeds, N );
	glUniform1f( uC, c );
	glUniform1f( uDotMin, dotMin );
	glUniform1f( uDotMax, dotMax );
	glUniform1f( uRotation, rotRad );
	glUniform1f( uSoft, softness );
	glUniform1f( uShown, shownF );
	glUniform1f( uRMax, rMax );
	glUniform1f( uRhoScale, rhoScale );
	glUniform1i( uPaletteMode, paletteMode );
	glUniform3f( uColA, a[ 0 ], a[ 1 ], a[ 2 ] );
	glUniform3f( uColB, b[ 0 ], b[ 1 ], b[ 2 ] );
	glUniform3f( uColC, cc[ 0 ], cc[ 1 ], cc[ 2 ] );
	glUniform1i( uColorBy, std::min( std::max( (int)colorBy, 0 ), CB_COUNT - 1 ) );
	glUniform1f( uArms, std::max( std::floor( arms ), 1.0f ) );
	glUniform1f( uCycles, std::max( std::floor( cycles ), 1.0f ) );
	glUniform1f( uHueShift, hueShift );
	glUniform1f( uSat, saturation );
	glUniform1f( uBri, brightness );
	glUniform4f( uBg, bgRGB[ 0 ], bgRGB[ 1 ], bgRGB[ 2 ], bg[ 3 ] );

	quad.Draw();
	return FF_SUCCESS;
}

FFResult FFGLPhyllotaxis::DeInitGL()
{
	shader.FreeGLResources();
	quad.Release();
	return FF_SUCCESS;
}

FFResult FFGLPhyllotaxis::SetFloatParameter( unsigned int index, float value )
{
	switch( index )
	{
	case PT_PRESET:    preset = value;    UpdateVisibility( true ); break;
	case PT_ANGLE:     angle = value;     break;
	case PT_SEEDS:     seeds = value;     break;
	case PT_SCALEMODE: scaleMode = value; UpdateVisibility( true ); break;
	case PT_RADIUS:    radius = value;    break;
	case PT_SCALEC:    scaleC = value;    break;
	case PT_DOTMIN:    dotMin = value;    break;
	case PT_DOTMAX:    dotMax = value;    break;
	case PT_ROTATION:  rotation = value;  break;
	case PT_SPIN:      spin = value;      break;
	case PT_SOFT:      softness = value;  break;

	case PT_ANIMATE:
		animate = value;
		if( animate != 0.0f )
			revealSeeds = 0.0;
		UpdateVisibility( true );
		break;
	case PT_REVEAL:    reveal = value;    break;
	case PT_SPEED:     speed = value;     break;
	case PT_LOOP:      loop = value;      break;
	case PT_RESET:
		if( value != 0.0f )
			revealSeeds = 0.0;
		break;

	case PT_PALETTE:   palette = value;   UpdateVisibility( true ); break;
	case PT_COLORBY:   colorBy = value;   UpdateVisibility( true ); break;
	case PT_ARMS:      arms = value;      break;
	case PT_CYCLES:    cycles = value;    break;
	case PT_HUESHIFT:  hueShift = value;  break;
	case PT_SAT:       saturation = value; break;
	case PT_BRI:       brightness = value; break;

	case PT_COLA_H: case PT_COLA_S: case PT_COLA_B: case PT_COLA_A:
		colA[ index - PT_COLA_H ] = value; break;
	case PT_COLB_H: case PT_COLB_S: case PT_COLB_B: case PT_COLB_A:
		colB[ index - PT_COLB_H ] = value; break;
	case PT_BG_H: case PT_BG_S: case PT_BG_B: case PT_BG_A:
		bg[ index - PT_BG_H ] = value; break;

	default:
		return FF_FAIL;
	}
	return FF_SUCCESS;
}

float FFGLPhyllotaxis::GetFloatParameter( unsigned int index )
{
	switch( index )
	{
	case PT_PRESET:    return preset;
	case PT_ANGLE:     return angle;
	case PT_SEEDS:     return seeds;
	case PT_SCALEMODE: return scaleMode;
	case PT_RADIUS:    return radius;
	case PT_SCALEC:    return scaleC;
	case PT_DOTMIN:    return dotMin;
	case PT_DOTMAX:    return dotMax;
	case PT_ROTATION:  return rotation;
	case PT_SPIN:      return spin;
	case PT_SOFT:      return softness;
	case PT_ANIMATE:   return animate;
	case PT_REVEAL:    return reveal;
	case PT_SPEED:     return speed;
	case PT_LOOP:      return loop;
	case PT_RESET:     return 0.0f;
	case PT_PALETTE:   return palette;
	case PT_COLORBY:   return colorBy;
	case PT_ARMS:      return arms;
	case PT_CYCLES:    return cycles;
	case PT_HUESHIFT:  return hueShift;
	case PT_SAT:       return saturation;
	case PT_BRI:       return brightness;
	case PT_COLA_H: case PT_COLA_S: case PT_COLA_B: case PT_COLA_A:
		return colA[ index - PT_COLA_H ];
	case PT_COLB_H: case PT_COLB_S: case PT_COLB_B: case PT_COLB_A:
		return colB[ index - PT_COLB_H ];
	case PT_BG_H: case PT_BG_S: case PT_BG_B: case PT_BG_A:
		return bg[ index - PT_BG_H ];
	}
	return 0.0f;
}

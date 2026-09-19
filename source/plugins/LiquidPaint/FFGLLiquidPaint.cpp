#include "FFGLLiquidPaint.h"
using namespace ffglex;

// ---- parameter indices: plugin-specific blocks first, then the shared blocks ----
enum ParamType : FFUInt32
{
	// Flow
	PT_PRESET,
	PT_SCALE,
	PT_DETAIL,
	PT_ROUGH,
	PT_WARP,
	PT_TURB,
	PT_SWIRL,
	PT_SWIRLRAD,
	PT_SYM,
	PT_ROTATION,
	PT_SPIN,
	PT_FLOW,
	PT_FLOWANG,
	PT_CHURN,
	PT_FLOW_END,
	// Paint
	PT_PAINTS = PT_FLOW_END,
	PT_BLEND,
	PT_SOFT,
	PT_CONTRAST,
	PT_COLORBY,
	PT_COVERAGE,
	PT_VEINS,
	PT_VEINCOUNT,
	PT_PAINT_END,
	// Surface
	PT_CELLS = PT_PAINT_END,
	PT_CELLSIZE,
	PT_CELLTONE,
	PT_DROPLETS,
	PT_DROPSIZE,
	PT_GLOSS,
	PT_RELIEF,
	PT_LIGHT,
	PT_SURFACE_END,
	PT_OWN_END = PT_SURFACE_END, // shared blocks start here
};

static CFFGLPluginInfo PluginInfo(
	PluginFactory< FFGLLiquidPaint >,
	"LQPT",                       // 4-char unique ID
	"Liquid Paint",               // name shown in Resolume's Sources tab
	2, 1,                         // FFGL API version
	1, 0,                         // plugin version
	FF_SOURCE,
	"Coloured paints mixing and flowing: acrylic pour cells, droplets, ink veins, liquid light show",
	"Generated with the /generator skill"
);

// Presets: bundles of Flow/Paint/Surface values. Index 0 = Custom leaves the sliders alone.
static const struct
{
	const char* name;
	float detail, rough, warp, turb, swirl, paints, blend, contrast, veins, veinCount, cells, cellSize, cellTone, droplets, dropSize, gloss, relief;
} kPresets[] = {
	{ "Custom",          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ "Acrylic Pour",    6, 0.50f, 1.20f, 0.50f, 0.00f, 4, 0.25f, 2.40f, 0.00f, 8,  0.60f, 0.10f, 0.85f, 0.35f, 0.50f, 0.30f, 0.35f },
	{ "Liquid Light",    4, 0.35f, 0.80f, 0.30f, 0.40f, 3, 0.60f, 2.60f, 0.00f, 8,  0.00f, 0.15f, 0.90f, 0.75f, 0.85f, 0.55f, 0.20f },
	{ "Honeycomb Lace",  5, 0.45f, 0.90f, 0.40f, 0.00f, 3, 0.15f, 2.40f, 0.00f, 8,  1.00f, 0.06f, 0.95f, 0.20f, 0.35f, 0.15f, 0.25f },
	{ "Ink Veins",       7, 0.60f, 1.60f, 0.70f, 0.00f, 2, 0.20f, 2.80f, 0.80f, 12, 0.10f, 0.12f, 0.20f, 0.00f, 0.50f, 0.25f, 0.30f },
	{ "Oil Slick",       7, 0.42f, 2.20f, 0.90f, 0.00f, 1, 1.00f, 2.60f, 0.00f, 8,  0.00f, 0.12f, 0.85f, 0.00f, 0.50f, 0.80f, 0.60f },
	{ "Stirred Cup",     6, 0.50f, 1.40f, 0.60f, 1.20f, 5, 0.30f, 2.40f, 0.30f, 6,  0.20f, 0.10f, 0.85f, 0.15f, 0.40f, 0.45f, 0.40f },
	{ "Wet Marble",      7, 0.55f, 2.60f, 1.00f, 0.00f, 6, 0.10f, 2.20f, 0.00f, 8,  0.00f, 0.12f, 0.85f, 0.00f, 0.50f, 0.20f, 0.15f },
};
static const int kNumPresets = sizeof( kPresets ) / sizeof( kPresets[ 0 ] );

static const char* const kColorByNames[] = { "Paint", "Flow", "Radial", "Angular" };
static const int kNumColorBy = 4;

static const char fragmentBody[] = R"(
uniform vec2  resolution;
uniform float phase;       // 0..1 from the Animation block
uniform float scale;
uniform float detail;      // octaves, integer
uniform float rough;
uniform float warpAmt;
uniform float turbAmt;
uniform float swirlAmt;    // turns at the centre
uniform float swirlRad;
uniform float symmetry;    // integer sectors
uniform float rotation;    // radians
uniform vec2  flowOff;     // accumulated drift, frame units
uniform float churn;       // integer: noise cells travelled per loop (z period)
uniform float paints;      // integer
uniform float blendAmt;
uniform float softPx;
uniform float contrast;
uniform int   colorBy;
uniform float coverage;
uniform float veinAmt;
uniform float veinCount;
uniform float cellAmt;
uniform float cellSize;
uniform float cellTone;
uniform float dropAmt;
uniform float dropSize;
uniform float gloss;
uniform float relief;
uniform float lightAng;    // radians

in vec2 uv;
out vec4 fragColor;

const float TAU = 6.28318530718;

vec3 hash3( vec3 p )
{
	p = fract( p * vec3( 0.1031, 0.1030, 0.0973 ) );
	p += dot( p, p.yxz + 33.33 );
	return fract( ( p.xxy + p.yxx ) * p.zyx );
}
vec2 hash2( vec2 p )
{
	vec3 q = fract( vec3( p.xyx ) * vec3( 0.1031, 0.1030, 0.0973 ) );
	q += dot( q, q.yzx + 33.33 );
	return fract( ( q.xx + q.yz ) * q.zy );
}

// 3D gradient noise, periodic in z with integer period `per` so a loop over z = 0..per is seamless.
float gcorner( vec3 cell, float zc, vec3 d )
{
	vec3 g = hash3( vec3( cell.xy, zc ) ) * 2.0 - 1.0;
	return dot( g, d );
}
float gnoise( vec3 p, float per )
{
	vec3 i = floor( p );
	vec3 f = fract( p );
	vec3 u = f * f * ( 3.0 - 2.0 * f );
	float z0 = mod( i.z, per ), z1 = mod( i.z + 1.0, per );
	float n000 = gcorner( i + vec3( 0, 0, 0 ), z0, f - vec3( 0, 0, 0 ) );
	float n100 = gcorner( i + vec3( 1, 0, 0 ), z0, f - vec3( 1, 0, 0 ) );
	float n010 = gcorner( i + vec3( 0, 1, 0 ), z0, f - vec3( 0, 1, 0 ) );
	float n110 = gcorner( i + vec3( 1, 1, 0 ), z0, f - vec3( 1, 1, 0 ) );
	float n001 = gcorner( i + vec3( 0, 0, 1 ), z1, f - vec3( 0, 0, 1 ) );
	float n101 = gcorner( i + vec3( 1, 0, 1 ), z1, f - vec3( 1, 0, 1 ) );
	float n011 = gcorner( i + vec3( 0, 1, 1 ), z1, f - vec3( 0, 1, 1 ) );
	float n111 = gcorner( i + vec3( 1, 1, 1 ), z1, f - vec3( 1, 1, 1 ) );
	float nx00 = mix( n000, n100, u.x ), nx10 = mix( n010, n110, u.x );
	float nx01 = mix( n001, n101, u.x ), nx11 = mix( n011, n111, u.x );
	float nxy0 = mix( nx00, nx10, u.y ), nxy1 = mix( nx01, nx11, u.y );
	return mix( nxy0, nxy1, u.z ) * 1.3;   // roughly -1..1
}

// Fractal sum. The time axis stays at base frequency so every octave shares the loop period.
float fbm( vec2 p, float z, float per )
{
	float gain = mix( 0.33, 0.72, rough );
	float amp = 0.5, sum = 0.0, norm = 0.0;
	mat2 rot = mat2( 0.80, 0.60, -0.60, 0.80 );
	for( int i = 0; i < 8; ++i )
	{
		if( float( i ) >= detail ) break;
		sum += amp * gnoise( vec3( p, z ), per );
		norm += amp;
		p = rot * p * 2.0 + vec2( 3.1, 1.7 );
		z += 1.0;                                  // integer shift keeps the period intact
		amp *= gain;
	}
	return 0.5 + 0.5 * sum / max( norm, 0.001 );
}

// Animated Voronoi: points orbit inside their cell (integer number of orbits per loop).
// Returns (F1, F2, cellID hash).
vec3 voronoi( vec2 x, float ph )
{
	vec2 n = floor( x ), f = fract( x );
	float f1 = 8.0, f2 = 8.0, id = 0.0;
	for( int j = -1; j <= 1; ++j )
	for( int i = -1; i <= 1; ++i )
	{
		vec2 g = vec2( float( i ), float( j ) );
		vec2 h = hash2( n + g );
		vec2 o = 0.5 + 0.38 * sin( TAU * ( ph + h ) + vec2( 0.0, 1.7 ) );
		float d = length( g + o - f );
		if( d < f1 ) { f2 = f1; f1 = d; id = h.x; }
		else if( d < f2 ) f2 = d;
	}
	return vec3( f1, f2, id );
}

void main()
{
	float px = 1.0 / min( resolution.x, resolution.y );
	vec2 p = ( uv - 0.5 ) * resolution * px;             // shorter side spans -0.5..0.5
	float cs = cos( -rotation ), sn = sin( -rotation );
	p = vec2( cs * p.x - sn * p.y, sn * p.x + cs * p.y );
	float soft = max( softPx, 0.01 );

	float a = atan( p.y, p.x );
	float r = length( p );

	// kaleidoscope symmetry
	if( symmetry > 1.5 )
	{
		float sector = TAU / symmetry;
		float aa = mod( a, sector );
		aa = abs( aa - sector * 0.5 );
		p = r * vec2( cos( aa ), sin( aa ) );
	}
	// stirring vortex: twist falls off with radius
	if( abs( swirlAmt ) > 0.001 )
	{
		float tw = swirlAmt * TAU * exp( -r / max( swirlRad, 0.02 ) );
		float c2 = cos( tw ), s2 = sin( tw );
		p = vec2( c2 * p.x - s2 * p.y, s2 * p.x + c2 * p.y );
	}

	// ---------- domain-warped paint field, periodic in phase ----------
	vec2 d = ( p + flowOff ) * scale;
	float z = phase * churn;
	float per = churn;
	vec2 q = vec2( fbm( d, z, per ), fbm( d + vec2( 5.2, 1.3 ), z + 0.37, per ) );
	vec2 d1 = d + warpAmt * ( q - 0.5 ) * 2.0;
	vec2 rr = vec2( fbm( d1 + vec2( 1.7, 9.2 ), z + 0.61, per ), fbm( d1 + vec2( 8.3, 2.8 ), z + 0.83, per ) );
	vec2 d2 = d1 + warpAmt * turbAmt * ( rr - 0.5 ) * 2.0;
	float f = fbm( d2, z + 0.19, per );
	f = clamp( ( f - 0.5 ) * contrast + 0.5, 0.0, 1.0 );

	// ---------- colour coordinate ----------
	float t;
	if( colorBy == 0 )      t = f;
	else if( colorBy == 1 ) t = ( q.x + rr.y ) * 0.5 * 0.7 + f * 0.3;
	else if( colorBy == 2 ) t = r * 1.6 + ( f - 0.5 ) * 0.5;
	else                    t = a / TAU + ( f - 0.5 ) * 0.4;

	// quantise into distinct paints with a controllable blend width, anti-aliased in screen space
	float n = max( floor( paints ), 1.0 );
	float tq = t;
	if( n > 1.5 )
	{
		float k = t * n;
		float aaW = fwidth( k ) * soft;
		float w = blendAmt * 0.5;
		float i = floor( k );
		float fr = fract( k );
		tq = ( i + smoothstep( 0.5 - w - aaW, 0.5 + w + aaW, fr ) ) / n;
	}
	// non-periodic coordinates use half a palette period (A -> B -> C) so every paint is a distinct colour
	float tc = colorBy == 3 ? tq : tq * 0.5;
	vec3 col = shade( tc );

	// ---------- surface: relief shading + gloss from the paint height field ----------
	vec2 grad = vec2( dFdx( f ), dFdy( f ) ) / max( px, 1e-5 );
	vec3 nrm = normalize( vec3( -grad * relief * 0.008, 1.0 ) );
	vec3 L = normalize( vec3( cos( lightAng ), sin( lightAng ), 1.2 ) );
	float diff = max( dot( nrm, L ), 0.0 );
	float spec = pow( max( dot( reflect( -L, nrm ), vec3( 0.0, 0.0, 1.0 ) ), 0.0 ), 40.0 );
	col *= mix( 1.0, 0.55 + 0.7 * diff, relief );
	col += vec3( spec ) * gloss * 0.9;

	// ---------- ink veins: thin bright lines following the warped domain ----------
	if( veinAmt > 0.001 )
	{
		float m = 0.5 + 0.5 * sin( TAU * veinCount * 0.15 * d2.x + 6.0 * f );
		float vein = pow( m, 14.0 );
		vec3 veinCol = mix( shade( tc + 0.5 ), vec3( 1.0 ), 0.6 );
		col = mix( col, veinCol, vein * veinAmt );
	}

	// ---------- pour cells: lacing at Voronoi edges, riding on the warped field ----------
	if( cellAmt > 0.001 )
	{
		vec2 cp = d2 / max( cellSize * scale, 0.01 ) * 0.25;
		vec3 v = voronoi( cp, phase * min( churn, 2.0 ) );
		float edge = v.y - v.x;
		float lw = 0.05;
		float aaC = fwidth( edge ) * soft;
		float lace = 1.0 - smoothstep( lw, lw + max( aaC, 0.02 ), edge );
		// thin the lace inside dark paint so it reads as pigment separation
		float shadeTone = mix( 0.0, 1.0, cellTone );
		vec3 laceCol = mix( vec3( shadeTone ), shade( tc + 0.5 ), 0.25 );
		col = mix( col, laceCol, lace * min( cellAmt * 1.5, 1.0 ) );
	}

	// ---------- droplets: floating spots of another paint ----------
	if( dropAmt > 0.001 )
	{
		vec2 dp = ( d + ( q - 0.5 ) * 0.6 ) * 2.6;
		vec3 v = voronoi( dp, phase * min( churn, 2.0 ) );
		vec2 hh = hash2( vec2( v.z * 91.7, v.z * 37.1 ) );
		float present = step( hh.x, dropAmt );
		float rad = dropSize * 0.32 * mix( 0.25, 1.0, hh.y );
		float aaD = fwidth( v.x ) * soft;
		float spot = ( 1.0 - smoothstep( rad - aaD, rad + aaD, v.x ) ) * present;
		vec3 dropCol = shade( tc + 0.2 + 0.6 * hh.y );
		float rim = smoothstep( rad * 0.55, rad, v.x );          // glossy rim on each drop
		dropCol = dropCol * ( 0.75 + 0.35 * rim ) + vec3( spec ) * gloss * 0.5;
		col = mix( col, dropCol, spot );
	}

	// ---------- coverage: holes where the paint is thin, revealing the background ----------
	vec4 outCol = background();
	float cover = 1.0;
	if( coverage < 0.999 )
	{
		float thr = 1.0 - coverage;
		float aaF = fwidth( f ) * soft;
		cover = smoothstep( thr - aaF - 0.02, thr + aaF + 0.02, f );
	}
	outCol = over( outCol, clamp( col, 0.0, 1.0 ), cover );
	fragColor = outCol;
}
)";

// The shader test harness and InitGL both use this.
static std::string GetFragmentShaderSource()
{
	return std::string( "#version 410 core\n" ) + gen::kColorGLSL + fragmentBody;
}
static const char* vertexShaderCode = gen::kVertexGLSL;

FFGLLiquidPaint::FFGLLiquidPaint()
{
	// ---- Flow ----
	SetOptionParamInfo( PT_PRESET, "Preset", kNumPresets, preset );
	for( int i = 0; i < kNumPresets; ++i )
		SetParamElementInfo( PT_PRESET, i, kPresets[ i ].name, (float)i );
	SetParamInfo( PT_SCALE, "Scale", FF_TYPE_STANDARD, scale );
	SetParamRange( PT_SCALE, 0.2f, 8.0f );
	SetParamInfo( PT_DETAIL, "Detail", FF_TYPE_INTEGER, detail );
	SetParamRange( PT_DETAIL, 1.0f, 8.0f );
	SetParamInfo( PT_ROUGH, "Roughness", FF_TYPE_STANDARD, roughness );
	SetParamRange( PT_ROUGH, 0.0f, 1.0f );
	SetParamInfo( PT_WARP, "Warp", FF_TYPE_STANDARD, warp );
	SetParamRange( PT_WARP, 0.0f, 4.0f );
	SetParamInfo( PT_TURB, "Turbulence", FF_TYPE_STANDARD, turbulence );
	SetParamRange( PT_TURB, 0.0f, 1.0f );
	SetParamInfo( PT_SWIRL, "Swirl", FF_TYPE_STANDARD, swirl );
	SetParamRange( PT_SWIRL, -3.0f, 3.0f );
	SetParamInfo( PT_SWIRLRAD, "Swirl Radius", FF_TYPE_STANDARD, swirlRad );
	SetParamRange( PT_SWIRLRAD, 0.05f, 1.0f );
	SetParamInfo( PT_SYM, "Symmetry", FF_TYPE_INTEGER, symmetry );
	SetParamRange( PT_SYM, 1.0f, 16.0f );
	SetParamInfo( PT_ROTATION, "Rotation", FF_TYPE_STANDARD, rotation );
	SetParamRange( PT_ROTATION, 0.0f, 360.0f );
	SetParamInfo( PT_SPIN, "Spin", FF_TYPE_STANDARD, spin );
	SetParamRange( PT_SPIN, -180.0f, 180.0f );
	SetParamInfo( PT_FLOW, "Flow", FF_TYPE_STANDARD, flow );
	SetParamRange( PT_FLOW, 0.0f, 1.0f );
	SetParamInfo( PT_FLOWANG, "Flow Angle", FF_TYPE_STANDARD, flowAngle );
	SetParamRange( PT_FLOWANG, 0.0f, 360.0f );
	SetParamInfo( PT_CHURN, "Churn", FF_TYPE_INTEGER, churn );
	SetParamRange( PT_CHURN, 1.0f, 8.0f );
	for( unsigned i = PT_PRESET; i < PT_FLOW_END; ++i )
		SetParamGroup( i, "Flow" );

	// ---- Paint ----
	SetParamInfo( PT_PAINTS, "Paints", FF_TYPE_INTEGER, paints );
	SetParamRange( PT_PAINTS, 1.0f, 12.0f );
	SetParamInfo( PT_BLEND, "Blend", FF_TYPE_STANDARD, blend );
	SetParamRange( PT_BLEND, 0.0f, 1.0f );
	SetParamInfo( PT_SOFT, "Edge Softness", FF_TYPE_STANDARD, softness );
	SetParamRange( PT_SOFT, 0.0f, 4.0f );
	SetParamInfo( PT_CONTRAST, "Contrast", FF_TYPE_STANDARD, contrast );
	SetParamRange( PT_CONTRAST, 0.5f, 5.0f );
	SetOptionParamInfo( PT_COLORBY, "Color By", kNumColorBy, colorBy );
	for( int i = 0; i < kNumColorBy; ++i )
		SetParamElementInfo( PT_COLORBY, i, kColorByNames[ i ], (float)i );
	SetParamInfo( PT_COVERAGE, "Coverage", FF_TYPE_STANDARD, coverage );
	SetParamRange( PT_COVERAGE, 0.0f, 1.0f );
	SetParamInfo( PT_VEINS, "Veins", FF_TYPE_STANDARD, veins );
	SetParamRange( PT_VEINS, 0.0f, 1.0f );
	SetParamInfo( PT_VEINCOUNT, "Vein Count", FF_TYPE_INTEGER, veinCount );
	SetParamRange( PT_VEINCOUNT, 1.0f, 40.0f );
	for( unsigned i = PT_FLOW_END; i < PT_PAINT_END; ++i )
		SetParamGroup( i, "Paint" );

	// ---- Surface ----
	SetParamInfo( PT_CELLS, "Cells", FF_TYPE_STANDARD, cells );
	SetParamRange( PT_CELLS, 0.0f, 1.0f );
	SetParamInfo( PT_CELLSIZE, "Cell Size", FF_TYPE_STANDARD, cellSize );
	SetParamRange( PT_CELLSIZE, 0.02f, 0.5f );
	SetParamInfo( PT_CELLTONE, "Cell Tone", FF_TYPE_STANDARD, cellTone );
	SetParamRange( PT_CELLTONE, 0.0f, 1.0f );
	SetParamInfo( PT_DROPLETS, "Droplets", FF_TYPE_STANDARD, droplets );
	SetParamRange( PT_DROPLETS, 0.0f, 1.0f );
	SetParamInfo( PT_DROPSIZE, "Drop Size", FF_TYPE_STANDARD, dropSize );
	SetParamRange( PT_DROPSIZE, 0.05f, 1.0f );
	SetParamInfo( PT_GLOSS, "Gloss", FF_TYPE_STANDARD, gloss );
	SetParamRange( PT_GLOSS, 0.0f, 1.0f );
	SetParamInfo( PT_RELIEF, "Relief", FF_TYPE_STANDARD, relief );
	SetParamRange( PT_RELIEF, 0.0f, 1.0f );
	SetParamInfo( PT_LIGHT, "Light Angle", FF_TYPE_STANDARD, lightAng );
	SetParamRange( PT_LIGHT, 0.0f, 360.0f );
	for( unsigned i = PT_PAINT_END; i < PT_SURFACE_END; ++i )
		SetParamGroup( i, "Surface" );

	// ---- shared blocks ----
	anim.animate = 1.0f;
	anim.speed   = 0.05f;             // one loop every 20 s: paint creeps rather than boils
	color.palette = (float)gen::PAL_SUNSET;
	unsigned next = AddAnimationParams( PT_OWN_END );
	next          = AddColorParams( next );
	next          = AddBackgroundParams( next );

	UpdateVisibility( false );
	FFGLLog::LogToHost( "Created Liquid Paint generator" );
}

void FFGLLiquidPaint::UpdateVisibility( bool raiseEvent )
{
	UpdateCommonVisibility( raiseEvent );
	const bool custom = (int)preset == 0;
	const unsigned overridden[] = { PT_DETAIL, PT_ROUGH, PT_WARP, PT_TURB, PT_SWIRL, PT_PAINTS, PT_BLEND, PT_CONTRAST,
	                                PT_VEINS, PT_VEINCOUNT, PT_CELLS, PT_CELLSIZE, PT_CELLTONE, PT_DROPLETS, PT_DROPSIZE, PT_GLOSS, PT_RELIEF };
	for( unsigned idx : overridden )
		SetParamVisibility( idx, custom, raiseEvent );
	// swirl radius only matters when there is a swirl
	const float effSwirl = custom ? swirl : kPresets[ std::min( std::max( (int)preset, 0 ), kNumPresets - 1 ) ].swirl;
	SetParamVisibility( PT_SWIRLRAD, std::fabs( effSwirl ) > 0.001f, raiseEvent );
}

FFResult FFGLLiquidPaint::InitGL( const FFGLViewportStruct* vp )
{
	if( !shader.Compile( vertexShaderCode, GetFragmentShaderSource().c_str() ) )
	{
		FFGLLog::LogToHost( "Liquid Paint: shader compile failed" );
		DeInitGL();
		return FF_FAIL;
	}
	if( !quad.Initialise() )
	{
		DeInitGL();
		return FF_FAIL;
	}
	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	FindColorUniforms( shader );
	uResolution = shader.FindUniform( "resolution" );
	uPhase      = shader.FindUniform( "phase" );
	uScale      = shader.FindUniform( "scale" );
	uDetail     = shader.FindUniform( "detail" );
	uRough      = shader.FindUniform( "rough" );
	uWarp       = shader.FindUniform( "warpAmt" );
	uTurb       = shader.FindUniform( "turbAmt" );
	uSwirl      = shader.FindUniform( "swirlAmt" );
	uSwirlRad   = shader.FindUniform( "swirlRad" );
	uSym        = shader.FindUniform( "symmetry" );
	uRotation   = shader.FindUniform( "rotation" );
	uFlowOff    = shader.FindUniform( "flowOff" );
	uChurn      = shader.FindUniform( "churn" );
	uPaints     = shader.FindUniform( "paints" );
	uBlend      = shader.FindUniform( "blendAmt" );
	uSoft       = shader.FindUniform( "softPx" );
	uContrast   = shader.FindUniform( "contrast" );
	uColorBy    = shader.FindUniform( "colorBy" );
	uCoverage   = shader.FindUniform( "coverage" );
	uVeins      = shader.FindUniform( "veinAmt" );
	uVeinCount  = shader.FindUniform( "veinCount" );
	uCells      = shader.FindUniform( "cellAmt" );
	uCellSize   = shader.FindUniform( "cellSize" );
	uCellTone   = shader.FindUniform( "cellTone" );
	uDroplets   = shader.FindUniform( "dropAmt" );
	uDropSize   = shader.FindUniform( "dropSize" );
	uGloss      = shader.FindUniform( "gloss" );
	uRelief     = shader.FindUniform( "relief" );
	uLight      = shader.FindUniform( "lightAng" );
	return CFFGLPlugin::InitGL( vp );
}

FFResult FFGLLiquidPaint::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	Tick(); // updates dt, time, phase

	spinAccum = std::fmod( spinAccum + spin * dt, 360.0 );
	const float rotRad = (float)( ( rotation + spinAccum ) * M_PI / 180.0 );
	const double fa = flowAngle * M_PI / 180.0;
	flowX += std::cos( fa ) * flow * 0.15 * dt;
	flowY += std::sin( fa ) * flow * 0.15 * dt;
	// keep the drift offset bounded without a visible jump: the noise has no spatial period, so wrap on a
	// large distance where floating point precision would otherwise start to degrade
	if( std::fabs( flowX ) > 4096.0 ) flowX = 0.0;
	if( std::fabs( flowY ) > 4096.0 ) flowY = 0.0;

	float eDetail = detail, eRough = roughness, eWarp = warp, eTurb = turbulence, eSwirl = swirl, ePaints = paints, eBlend = blend, eContrast = contrast,
	      eVeins = veins, eVeinCount = veinCount, eCells = cells, eCellSize = cellSize, eCellTone = cellTone, eDrops = droplets, eDropSize = dropSize,
	      eGloss = gloss, eRelief = relief;
	const int pi = (int)preset;
	if( pi > 0 && pi < kNumPresets )
	{
		const auto& P = kPresets[ pi ];
		eDetail = P.detail; eRough = P.rough; eWarp = P.warp; eTurb = P.turb; eSwirl = P.swirl; ePaints = P.paints; eBlend = P.blend; eContrast = P.contrast;
		eVeins = P.veins; eVeinCount = P.veinCount; eCells = P.cells; eCellSize = P.cellSize; eCellTone = P.cellTone; eDrops = P.droplets; eDropSize = P.dropSize;
		eGloss = P.gloss; eRelief = P.relief;
	}

	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	UploadColorUniforms();
	glUniform2f( uResolution, (float)currentViewport.width, (float)currentViewport.height );
	glUniform1f( uPhase, (float)phase );
	glUniform1f( uScale, scale );
	glUniform1f( uDetail, std::max( std::floor( eDetail ), 1.0f ) );
	glUniform1f( uRough, eRough );
	glUniform1f( uWarp, eWarp );
	glUniform1f( uTurb, eTurb );
	glUniform1f( uSwirl, eSwirl );
	glUniform1f( uSwirlRad, swirlRad );
	glUniform1f( uSym, std::max( std::floor( symmetry ), 1.0f ) );
	glUniform1f( uRotation, rotRad );
	glUniform2f( uFlowOff, (float)flowX, (float)flowY );
	glUniform1f( uChurn, std::max( std::floor( churn ), 1.0f ) );
	glUniform1f( uPaints, std::max( std::floor( ePaints ), 1.0f ) );
	glUniform1f( uBlend, eBlend );
	glUniform1f( uSoft, softness );
	glUniform1f( uContrast, eContrast );
	glUniform1i( uColorBy, std::min( std::max( (int)colorBy, 0 ), kNumColorBy - 1 ) );
	glUniform1f( uCoverage, coverage );
	glUniform1f( uVeins, eVeins );
	glUniform1f( uVeinCount, std::max( std::floor( eVeinCount ), 1.0f ) );
	glUniform1f( uCells, eCells );
	glUniform1f( uCellSize, eCellSize );
	glUniform1f( uCellTone, eCellTone );
	glUniform1f( uDroplets, eDrops );
	glUniform1f( uDropSize, eDropSize );
	glUniform1f( uGloss, eGloss );
	glUniform1f( uRelief, eRelief );
	glUniform1f( uLight, (float)( lightAng * M_PI / 180.0 ) );
	quad.Draw();
	return FF_SUCCESS;
}

FFResult FFGLLiquidPaint::DeInitGL()
{
	shader.FreeGLResources();
	quad.Release();
	return FF_SUCCESS;
}

FFResult FFGLLiquidPaint::SetFloatParameter( unsigned int index, float value )
{
	if( SetCommonParam( index, value ) )
		return FF_SUCCESS;
	switch( index )
	{
	case PT_PRESET:    preset = value; UpdateVisibility( true ); break;
	case PT_SCALE:     scale = value; break;
	case PT_DETAIL:    detail = value; break;
	case PT_ROUGH:     roughness = value; break;
	case PT_WARP:      warp = value; break;
	case PT_TURB:      turbulence = value; break;
	case PT_SWIRL:     swirl = value; UpdateVisibility( true ); break;
	case PT_SWIRLRAD:  swirlRad = value; break;
	case PT_SYM:       symmetry = value; break;
	case PT_ROTATION:  rotation = value; break;
	case PT_SPIN:      spin = value; break;
	case PT_FLOW:      flow = value; break;
	case PT_FLOWANG:   flowAngle = value; break;
	case PT_CHURN:     churn = value; break;
	case PT_PAINTS:    paints = value; break;
	case PT_BLEND:     blend = value; break;
	case PT_SOFT:      softness = value; break;
	case PT_CONTRAST:  contrast = value; break;
	case PT_COLORBY:   colorBy = value; break;
	case PT_COVERAGE:  coverage = value; break;
	case PT_VEINS:     veins = value; break;
	case PT_VEINCOUNT: veinCount = value; break;
	case PT_CELLS:     cells = value; break;
	case PT_CELLSIZE:  cellSize = value; break;
	case PT_CELLTONE:  cellTone = value; break;
	case PT_DROPLETS:  droplets = value; break;
	case PT_DROPSIZE:  dropSize = value; break;
	case PT_GLOSS:     gloss = value; break;
	case PT_RELIEF:    relief = value; break;
	case PT_LIGHT:     lightAng = value; break;
	default:
		return FF_FAIL;
	}
	return FF_SUCCESS;
}

float FFGLLiquidPaint::GetFloatParameter( unsigned int index )
{
	float v;
	if( GetCommonParam( index, v ) )
		return v;
	switch( index )
	{
	case PT_PRESET:    return preset;
	case PT_SCALE:     return scale;
	case PT_DETAIL:    return detail;
	case PT_ROUGH:     return roughness;
	case PT_WARP:      return warp;
	case PT_TURB:      return turbulence;
	case PT_SWIRL:     return swirl;
	case PT_SWIRLRAD:  return swirlRad;
	case PT_SYM:       return symmetry;
	case PT_ROTATION:  return rotation;
	case PT_SPIN:      return spin;
	case PT_FLOW:      return flow;
	case PT_FLOWANG:   return flowAngle;
	case PT_CHURN:     return churn;
	case PT_PAINTS:    return paints;
	case PT_BLEND:     return blend;
	case PT_SOFT:      return softness;
	case PT_CONTRAST:  return contrast;
	case PT_COLORBY:   return colorBy;
	case PT_COVERAGE:  return coverage;
	case PT_VEINS:     return veins;
	case PT_VEINCOUNT: return veinCount;
	case PT_CELLS:     return cells;
	case PT_CELLSIZE:  return cellSize;
	case PT_CELLTONE:  return cellTone;
	case PT_DROPLETS:  return droplets;
	case PT_DROPSIZE:  return dropSize;
	case PT_GLOSS:     return gloss;
	case PT_RELIEF:    return relief;
	case PT_LIGHT:     return lightAng;
	}
	return 0.0f;
}

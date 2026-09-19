#include "FFGLHiddenRealms.h"
using namespace ffglex;

// ---- parameter indices: plugin-specific block first, then the shared blocks ----
enum ParamType : FFUInt32
{
	// Shape
	PT_PRESET,
	PT_MODE,
	PT_RADIUS,
	PT_WIDTH,
	PT_TILT,
	PT_ROTATION,
	PT_SPIN,
	PT_SOFT,
	PT_SHAPE_END,
	// Energy
	PT_SPHERE = PT_SHAPE_END,
	PT_LIGHT,
	PT_OGLOW,
	PT_EDGELEVEL,
	PT_IGLOW,
	PT_HOTCORE,
	PT_TENDRILS,
	PT_REACH,
	PT_GLOWINT,
	PT_PLASMA,
	PT_TURB,
	PT_TURBSCALE,
	PT_RAYS,
	PT_RAYLEN,
	PT_PULSE,
	PT_FLICKER,
	PT_ENERGY_END,
	// Center
	PT_CENTERMODE = PT_ENERGY_END,
	PT_CENTERCOL,                     // 4 consecutive: hue, sat, bri, alpha
	PT_FEATHER = PT_CENTERCOL + 4,
	PT_CENTER_END,
	PT_OWN_END = PT_CENTER_END,       // shared blocks start here
};

static CFFGLPluginInfo PluginInfo(
	PluginFactory< FFGLHiddenRealms >,
	"HRLM",                       // 4-char unique ID
	"Hidden Realms",              // name shown in Resolume's Sources tab
	2, 1,                         // FFGL API version
	1, 0,                         // plugin version
	FF_SOURCE,
	"Portal energy filling the frame outside a dark circle, with hot rim, tendrils, plasma and rays",
	"Generated with the /generator skill"
);

// Presets: bundles of Energy values plus core width. Index 0 = Custom leaves the sliders alone.
static const struct
{
	const char* name;
	float width, sphere, oglow, edge, iglow, hot, tend, reach, plasma, turb, turbScale, rays, rayLen, pulse;
} kPresets[] = {
	{ "Custom",         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ "Electric Gate",  0.040f, 0.60f, 0.50f, 0.35f, 0.20f, 0.70f, 0.60f, 0.50f, 0.50f, 0.30f, 4, 0,  0.30f, 0.30f },
	{ "Storm Wall",     0.060f, 0.40f, 0.80f, 0.60f, 0.30f, 0.80f, 0.90f, 0.70f, 0.80f, 0.50f, 6, 0,  0.30f, 0.40f },
	{ "Calm Rim",       0.025f, 0.80f, 0.35f, 0.15f, 0.10f, 0.50f, 0.15f, 0.30f, 0.20f, 0.08f, 3, 0,  0.20f, 0.10f },
	{ "Stargate",       0.070f, 0.90f, 0.40f, 0.30f, 0.30f, 0.60f, 0.30f, 0.40f, 0.40f, 0.10f, 6, 24, 0.40f, 0.15f },
	{ "Solar Rift",     0.050f, 0.50f, 0.60f, 0.50f, 0.10f, 0.90f, 0.40f, 0.30f, 0.60f, 0.80f, 8, 48, 0.60f, 0.25f },
	{ "Void Lens",      0.020f, 1.00f, 0.25f, 0.05f, 0.90f, 0.30f, 0.80f, 0.90f, 0.30f, 0.15f, 5, 0,  0.20f, 0.05f },
};
static const int kNumPresets = sizeof( kPresets ) / sizeof( kPresets[ 0 ] );

static const char* const kColorByNames[] = { "Angle", "Ring Width", "Swirl", "Depth" };
static const int kNumColorBy = 4;

static const char fragmentBody[] = R"(
uniform vec2  resolution;
uniform int   mode;        // 0 fill outside, 1 ring
uniform float radius;
uniform float ringWidth;   // hot core width
uniform float tilt;        // radians
uniform float rotation;    // radians
uniform float softPx;
uniform float phase;       // 0..1 from the Animation block
uniform float sphere;
uniform float lightAng;    // radians
uniform float outerGlow;   // fade distance outward
uniform float edgeLevel;   // brightness left at the far edge (fill mode)
uniform float innerGlow;
uniform float hotCore;
uniform float tendrilAmt;
uniform float reach;
uniform float glowInt;
uniform float plasmaAmt;
uniform float turbAmt;
uniform float turbScale;   // integer
uniform float rays;        // integer
uniform float rayLen;
uniform float pulseAmt;
uniform float flickerAmt;
uniform int   centerMode;  // 0 transparent, 1 solid
uniform vec4  centerCol;   // straight RGBA
uniform float feather;
uniform int   colorBy;

in vec2 uv;
out vec4 fragColor;

const float TAU = 6.28318530718;

// Periodic in angle (2*pi, integer harmonics) and in phase (integer cycles), so loops are seamless.
float pnoise( float a, float ph, float sc )
{
	float n = 0.0;
	n += sin( a * sc + TAU * ph );
	n += 0.5 * sin( a * ( sc * 2.0 + 1.0 ) - TAU * 2.0 * ph + 1.7 );
	n += 0.25 * sin( a * ( sc * 3.0 + 2.0 ) + TAU * 3.0 * ph + 4.1 );
	return n / 1.75;
}

// 2D energy field over (angle, radial distance), periodic in angle and phase.
float pnoise2( float a, float q, float ph, float sc )
{
	float n = 0.0;
	n += sin( a * sc + q * 9.0 + TAU * ph );
	n += 0.6 * sin( a * ( sc * 2.0 + 3.0 ) - q * 15.0 + 1.3 - TAU * 2.0 * ph );
	n += 0.4 * sin( a * ( sc * 3.0 + 1.0 ) + q * 23.0 + 3.9 + TAU * ph );
	n += 0.25 * sin( a * ( sc * 5.0 + 2.0 ) - q * 37.0 + 0.7 - TAU * 3.0 * ph );
	return n / 2.25;
}

// Thin bright filaments: zero crossings of two layered fields.
float filaments( float a, float q, float ph, float w )
{
	float f1 = sin( a * 7.0 + q * 40.0 + TAU * ph ) + 0.6 * sin( a * 13.0 - q * 70.0 + 2.1 + TAU * 2.0 * ph ) + 0.4 * sin( a * 23.0 + q * 110.0 + 4.0 - TAU * ph );
	float f2 = sin( a * 11.0 - q * 55.0 + 1.1 - TAU * ph ) + 0.5 * sin( a * 19.0 + q * 90.0 + 3.3 + TAU * 2.0 * ph );
	float l1 = 1.0 - smoothstep( 0.0, w, abs( f1 ) );
	float l2 = 1.0 - smoothstep( 0.0, w * 0.7, abs( f2 ) );
	// gate so filaments break into branches instead of full loops
	float g1 = smoothstep( -0.1, 0.4, pnoise( a * 3.0 + q * 6.0, ph, 2.0 ) );
	float g2 = smoothstep( -0.1, 0.4, pnoise( a * 2.0 - q * 8.0 + 1.0, ph, 3.0 ) );
	return max( l1 * g1, 0.7 * l2 * g2 );
}

// Add a premultiplied glow of colour col with strength g onto dst (alpha union).
vec4 addGlow( vec4 dst, vec3 col, float g )
{
	g = clamp( g, 0.0, 1.0 );
	return vec4( dst.rgb + col * g, dst.a + ( 1.0 - dst.a ) * g );
}

void main()
{
	float px = 1.0 / min( resolution.x, resolution.y );
	vec2 p = ( uv - 0.5 ) * resolution * px;             // shorter side spans -0.5..0.5
	float cs = cos( -rotation ), sn = sin( -rotation );
	p = vec2( cs * p.x - sn * p.y, sn * p.x + cs * p.y );
	p.y /= max( cos( tilt ), 0.15 );                     // perspective squash into an ellipse

	float a = atan( p.y, p.x );
	float r = length( p );
	float soft = max( softPx, 0.01 ) * px;

	// inner edge of the portal: breathing + turbulent wobble
	float R = radius * ( 1.0 + pulseAmt * 0.12 * sin( TAU * phase ) );
	R += turbAmt * ringWidth * 1.5 * pnoise( a, phase, turbScale );
	float coreW = ringWidth;
	float d = r - R;                                     // signed distance to the inner edge (ring centre line in ring mode)
	float halfW = mode == 1 ? coreW * 0.5 : coreW;

	// brightness modulation along the ring and in time
	float pl = 0.5 + 0.5 * pnoise( a + 1.3, phase, turbScale + 1.0 );
	float mod = mix( 1.0, pl, plasmaAmt );
	float fl = 0.5 + 0.5 * sin( TAU * 5.0 * phase ) * sin( TAU * 11.0 * phase + 2.0 );
	mod *= 1.0 - flickerAmt * 0.6 * fl;

	// colour coordinate
	float t;
	float depth = clamp( abs( d ) / radius, 0.0, 1.0 );
	if( colorBy == 0 )      t = a / TAU;
	else if( colorBy == 1 ) t = mode == 1 ? 0.5 + 0.5 * clamp( d / halfW, -1.0, 1.0 ) : 0.5 * clamp( d / coreW, 0.0, 1.0 );
	else if( colorBy == 2 ) t = a / TAU + phase + d / ( coreW * 4.0 );
	else                    t = 0.5 - 0.5 * depth;
	vec3 col = shade( t );
	vec3 hot = mix( col, vec3( 1.0 ), hotCore );

	vec4 outCol = background();

	// solid centre (the dark inside of the portal; a layered video sits here in Arena)
	float innerEdge = mode == 1 ? -halfW : 0.0;
	if( centerMode == 1 )
	{
		float fe = max( feather * radius, soft );
		float cover = 1.0 - smoothstep( innerEdge - fe, innerEdge + soft, d );
		outCol = over( outCol, centerCol.rgb, cover * centerCol.a );
	}

	// glow bleeding into the centre
	float gIn = innerGlow > 0.001 ? exp( -max( innerEdge - d, 0.0 ) / ( innerGlow * radius * 0.5 ) ) : 0.0;
	gIn *= step( d, innerEdge );
	outCol = addGlow( outCol, col * mod * glowInt, gIn * 0.85 * glowInt * mod );

	// electric tendrils creeping from the rim into the dark centre
	if( tendrilAmt > 0.001 )
	{
		float q = ( innerEdge - d ) / radius;            // 0 at the rim, growing toward the centre
		float reachD = max( reach, 0.02 );
		float fade = 1.0 - smoothstep( 0.0, reachD, q );
		fade *= step( -0.15, q );                        // also allow a little spill outward
		float w = mix( 0.02, 0.12, tendrilAmt ) * ( 1.0 + 2.0 * q / reachD );
		float fil = filaments( a, r, phase, w ) * fade * fade;
		fil *= 0.5 + 0.5 * pnoise( a * 2.0, phase * 2.0 + q * 3.0, 3.0 ); // flicker along length
		outCol = addGlow( outCol, hot * glowInt, fil * tendrilAmt * 1.4 * glowInt );
	}

	if( mode == 1 )
	{
		// ---------- ring mode: glowing band with outer glow ----------
		float u = clamp( d / halfW, -1.0, 1.0 );
		float dist = abs( d ) - halfW;
		float gOut = outerGlow > 0.001 ? exp( -max( d - halfW, 0.0 ) / ( outerGlow * radius * 0.5 ) ) : 0.0;
		gOut *= step( 0.0, d ) * step( 0.0, dist );
		outCol = addGlow( outCol, col * mod * glowInt, gOut * 0.85 * glowInt * mod );

		if( rays > 0.5 )
		{
			float rk = a / TAU * rays + phase + 0.5 * pnoise( a, phase, 2.0 );
			float rayI = pow( 0.5 + 0.5 * cos( TAU * rk ), 6.0 );
			float rayF = exp( -max( dist, 0.0 ) / max( rayLen * radius * 0.5, 0.001 ) ) * step( 0.0, dist );
			outCol = addGlow( outCol, col * glowInt, rayI * rayF * 0.9 * mod );
		}

		float band = 1.0 - smoothstep( halfW - soft, halfW + soft, abs( d ) );
		float nz = sqrt( max( 1.0 - u * u, 0.0 ) );
		vec3 n = vec3( cos( a ) * u, sin( a ) * u, nz );
		vec3 L = normalize( vec3( cos( lightAng ), sin( lightAng ), 0.8 ) );
		float diff = max( dot( n, L ), 0.0 );
		float spec = pow( diff, 24.0 ) * 0.7;
		float lit = mix( 1.0, 0.2 + 0.9 * diff + spec, sphere );
		vec3 ringCol = clamp( mix( col, hot, band * 0.6 ) * lit * mod * max( glowInt, 0.25 ), 0.0, 1.0 );
		outCol = over( outCol, ringCol, band );
	}
	else
	{
		// ---------- fill mode: energy covers everything outside the inner edge, out to the frame ----------
		float cover = smoothstep( -soft, soft, d );      // opaque outside the portal edge
		float dn = max( d, 0.0 );

		// brightness: hot core at the edge, fading outward to edgeLevel, never below it
		float fadeD = max( outerGlow * radius, 0.02 );
		float falloff = exp( -dn / fadeD );
		float level = mix( edgeLevel, 1.0, falloff );

		// electric cloud texture across the fill
		float cloud = 0.5 + 0.5 * pnoise2( a, r, phase, turbScale );
		cloud = mix( 1.0, 0.35 + 0.9 * cloud, plasmaAmt );
		// bright filaments wandering through the fill too
		float fil = filaments( a + 2.0, r * 0.6, phase, 0.06 ) * ( 0.3 + 0.7 * falloff );

		// rays: radial streaks across the fill
		float ray = 0.0;
		if( rays > 0.5 )
		{
			float rk = a / TAU * rays + phase + 0.5 * pnoise( a, phase, 2.0 );
			ray = pow( 0.5 + 0.5 * cos( TAU * rk ), 6.0 ) * exp( -dn / max( rayLen * radius, 0.001 ) );
		}

		// core band: rim-lit like the edge of a sphere
		float core = exp( -dn / coreW );
		float u = clamp( 1.0 - 2.0 * dn / coreW, -1.0, 1.0 );
		float nz = sqrt( max( 1.0 - u * u, 0.0 ) );
		vec3 n = vec3( cos( a ) * u, sin( a ) * u, nz );
		vec3 L = normalize( vec3( cos( lightAng ), sin( lightAng ), 0.8 ) );
		float diff = max( dot( n, L ), 0.0 );
		float lit = mix( 1.0, 0.35 + 0.9 * diff + pow( diff, 24.0 ) * 0.7, sphere * core );

		float bright = level * cloud * lit * mod;
		vec3 fillCol = col * bright;
		fillCol = mix( fillCol, hot * lit * mod, core * 0.85 );          // whiten toward the rim
		fillCol += hot * ( fil * 0.8 * tendrilAmt + ray * 0.9 ) * mod;   // filaments and rays
		fillCol = clamp( fillCol * glowInt, 0.0, 1.0 );
		outCol = over( outCol, fillCol, cover );
	}

	fragColor = outCol;
}
)";

// The shader test harness and InitGL both use this.
static std::string GetFragmentShaderSource()
{
	return std::string( "#version 410 core\n" ) + gen::kColorGLSL + fragmentBody;
}
static const char* vertexShaderCode = gen::kVertexGLSL;

FFGLHiddenRealms::FFGLHiddenRealms()
{
	// ---- Shape ----
	SetOptionParamInfo( PT_PRESET, "Preset", kNumPresets, preset );
	for( int i = 0; i < kNumPresets; ++i )
		SetParamElementInfo( PT_PRESET, i, kPresets[ i ].name, (float)i );
	SetOptionParamInfo( PT_MODE, "Mode", 2, mode );
	SetParamElementInfo( PT_MODE, 0, "Fill Outside", 0.0f );
	SetParamElementInfo( PT_MODE, 1, "Ring", 1.0f );
	SetParamInfo( PT_RADIUS, "Radius", FF_TYPE_STANDARD, radius );
	SetParamRange( PT_RADIUS, 0.05f, 1.0f );
	SetParamInfo( PT_WIDTH, "Core Width", FF_TYPE_STANDARD, width );
	SetParamRange( PT_WIDTH, 0.003f, 0.3f );
	SetParamInfo( PT_TILT, "Tilt", FF_TYPE_STANDARD, tilt );
	SetParamRange( PT_TILT, 0.0f, 75.0f );
	SetParamInfo( PT_ROTATION, "Rotation", FF_TYPE_STANDARD, rotation );
	SetParamRange( PT_ROTATION, 0.0f, 360.0f );
	SetParamInfo( PT_SPIN, "Spin", FF_TYPE_STANDARD, spin );
	SetParamRange( PT_SPIN, -180.0f, 180.0f );
	SetParamInfo( PT_SOFT, "Edge Softness", FF_TYPE_STANDARD, softness );
	SetParamRange( PT_SOFT, 0.0f, 4.0f );
	for( unsigned i = PT_PRESET; i < PT_SHAPE_END; ++i )
		SetParamGroup( i, "Shape" );

	// ---- Energy ----
	SetParamInfo( PT_SPHERE, "Sphere Shade", FF_TYPE_STANDARD, sphere );
	SetParamRange( PT_SPHERE, 0.0f, 1.0f );
	SetParamInfo( PT_LIGHT, "Light Angle", FF_TYPE_STANDARD, lightAng );
	SetParamRange( PT_LIGHT, 0.0f, 360.0f );
	SetParamInfo( PT_OGLOW, "Outer Fade", FF_TYPE_STANDARD, outerGlow );
	SetParamRange( PT_OGLOW, 0.0f, 1.0f );
	SetParamInfo( PT_EDGELEVEL, "Edge Level", FF_TYPE_STANDARD, edgeLevel );
	SetParamRange( PT_EDGELEVEL, 0.0f, 1.0f );
	SetParamInfo( PT_IGLOW, "Inner Glow", FF_TYPE_STANDARD, innerGlow );
	SetParamRange( PT_IGLOW, 0.0f, 1.0f );
	SetParamInfo( PT_HOTCORE, "Hot Core", FF_TYPE_STANDARD, hotCore );
	SetParamRange( PT_HOTCORE, 0.0f, 1.0f );
	SetParamInfo( PT_TENDRILS, "Tendrils", FF_TYPE_STANDARD, tendrils );
	SetParamRange( PT_TENDRILS, 0.0f, 1.0f );
	SetParamInfo( PT_REACH, "Tendril Reach", FF_TYPE_STANDARD, reach );
	SetParamRange( PT_REACH, 0.0f, 1.0f );
	SetParamInfo( PT_GLOWINT, "Glow Intensity", FF_TYPE_STANDARD, glowInt );
	SetParamRange( PT_GLOWINT, 0.0f, 3.0f );
	SetParamInfo( PT_PLASMA, "Plasma", FF_TYPE_STANDARD, plasma );
	SetParamRange( PT_PLASMA, 0.0f, 1.0f );
	SetParamInfo( PT_TURB, "Turbulence", FF_TYPE_STANDARD, turb );
	SetParamRange( PT_TURB, 0.0f, 1.0f );
	SetParamInfo( PT_TURBSCALE, "Turb Scale", FF_TYPE_INTEGER, turbScale );
	SetParamRange( PT_TURBSCALE, 1.0f, 24.0f );
	SetParamInfo( PT_RAYS, "Rays", FF_TYPE_INTEGER, rays );
	SetParamRange( PT_RAYS, 0.0f, 96.0f );
	SetParamInfo( PT_RAYLEN, "Ray Length", FF_TYPE_STANDARD, rayLen );
	SetParamRange( PT_RAYLEN, 0.0f, 1.0f );
	SetParamInfo( PT_PULSE, "Pulse", FF_TYPE_STANDARD, pulse );
	SetParamRange( PT_PULSE, 0.0f, 1.0f );
	SetParamInfo( PT_FLICKER, "Flicker", FF_TYPE_STANDARD, flicker );
	SetParamRange( PT_FLICKER, 0.0f, 1.0f );
	for( unsigned i = PT_SHAPE_END; i < PT_ENERGY_END; ++i )
		SetParamGroup( i, "Energy" );

	// ---- Center ----
	SetOptionParamInfo( PT_CENTERMODE, "Center Fill", 2, centerMode );
	SetParamElementInfo( PT_CENTERMODE, 0, "Transparent", 0.0f );
	SetParamElementInfo( PT_CENTERMODE, 1, "Solid", 1.0f );
	SetParamInfo( PT_CENTERCOL + 0, "Center Color", FF_TYPE_HUE, centerCol[ 0 ] );
	SetParamInfo( PT_CENTERCOL + 1, "Center Sat", FF_TYPE_SATURATION, centerCol[ 1 ] );
	SetParamInfo( PT_CENTERCOL + 2, "Center Bri", FF_TYPE_BRIGHTNESS, centerCol[ 2 ] );
	SetParamInfo( PT_CENTERCOL + 3, "Center Alpha", FF_TYPE_ALPHA, centerCol[ 3 ] );
	SetParamInfo( PT_FEATHER, "Center Feather", FF_TYPE_STANDARD, feather );
	SetParamRange( PT_FEATHER, 0.0f, 1.0f );
	for( unsigned i = PT_ENERGY_END; i < PT_CENTER_END; ++i )
		SetParamGroup( i, "Center" );

	// ---- shared blocks ----
	bg[ 3 ] = 0.0f;              // outside the fill (only visible in Ring mode) is transparent by default
	color.palette = (float)gen::PAL_ICE;
	unsigned next = AddAnimationParams( PT_OWN_END );
	pColorBy      = next;
	SetOptionParamInfo( pColorBy, "Color By", kNumColorBy, colorBy );
	for( int i = 0; i < kNumColorBy; ++i )
		SetParamElementInfo( pColorBy, i, kColorByNames[ i ], (float)i );
	SetParamGroup( pColorBy, "Color" );
	next = AddColorParams( pColorBy + 1 );
	next = AddBackgroundParams( next );

	UpdateVisibility( false );
	FFGLLog::LogToHost( "Created Hidden Realms generator" );
}

void FFGLHiddenRealms::UpdateVisibility( bool raiseEvent )
{
	UpdateCommonVisibility( raiseEvent );
	const bool custom = (int)preset == 0;
	SetParamVisibility( PT_WIDTH, custom, raiseEvent );
	SetParamVisibility( PT_SPHERE, custom, raiseEvent );
	SetParamVisibility( PT_OGLOW, custom, raiseEvent );
	SetParamVisibility( PT_IGLOW, custom, raiseEvent );
	SetParamVisibility( PT_HOTCORE, custom, raiseEvent );
	SetParamVisibility( PT_TENDRILS, custom, raiseEvent );
	SetParamVisibility( PT_REACH, custom, raiseEvent );
	const bool fill = (int)mode == 0;
	SetParamVisibility( PT_EDGELEVEL, custom && fill, raiseEvent );
	SetParamVisibility( PT_PLASMA, custom, raiseEvent );
	SetParamVisibility( PT_TURB, custom, raiseEvent );
	SetParamVisibility( PT_TURBSCALE, custom, raiseEvent );
	SetParamVisibility( PT_RAYS, custom, raiseEvent );
	SetParamVisibility( PT_RAYLEN, custom, raiseEvent );
	SetParamVisibility( PT_PULSE, custom, raiseEvent );

	const bool solid = (int)centerMode == 1;
	SetParamVisibility( PT_CENTERCOL, solid, raiseEvent ); // HSBA picker follows its hue param
	SetParamVisibility( PT_FEATHER, solid, raiseEvent );
}

FFResult FFGLHiddenRealms::InitGL( const FFGLViewportStruct* vp )
{
	if( !shader.Compile( vertexShaderCode, GetFragmentShaderSource().c_str() ) )
	{
		FFGLLog::LogToHost( "Hidden Realms: shader compile failed" );
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
	uMode       = shader.FindUniform( "mode" );
	uEdgeLevel  = shader.FindUniform( "edgeLevel" );
	uHotCore    = shader.FindUniform( "hotCore" );
	uTendrils   = shader.FindUniform( "tendrilAmt" );
	uReach      = shader.FindUniform( "reach" );
	uRadius     = shader.FindUniform( "radius" );
	uWidth      = shader.FindUniform( "ringWidth" );
	uTilt       = shader.FindUniform( "tilt" );
	uRotation   = shader.FindUniform( "rotation" );
	uSoft       = shader.FindUniform( "softPx" );
	uPhase      = shader.FindUniform( "phase" );
	uSphere     = shader.FindUniform( "sphere" );
	uLight      = shader.FindUniform( "lightAng" );
	uOGlow      = shader.FindUniform( "outerGlow" );
	uIGlow      = shader.FindUniform( "innerGlow" );
	uGlowInt    = shader.FindUniform( "glowInt" );
	uPlasma     = shader.FindUniform( "plasmaAmt" );
	uTurb       = shader.FindUniform( "turbAmt" );
	uTurbScale  = shader.FindUniform( "turbScale" );
	uRays       = shader.FindUniform( "rays" );
	uRayLen     = shader.FindUniform( "rayLen" );
	uPulse      = shader.FindUniform( "pulseAmt" );
	uFlicker    = shader.FindUniform( "flickerAmt" );
	uCenterMode = shader.FindUniform( "centerMode" );
	uCenterCol  = shader.FindUniform( "centerCol" );
	uFeather    = shader.FindUniform( "feather" );
	uColorBy    = shader.FindUniform( "colorBy" );
	return CFFGLPlugin::InitGL( vp );
}

FFResult FFGLHiddenRealms::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	Tick(); // updates dt, time, phase

	spinAccum = std::fmod( spinAccum + spin * dt, 360.0 );
	const float rotRad   = (float)( ( rotation + spinAccum ) * M_PI / 180.0 );
	const float tiltRad  = (float)( tilt * M_PI / 180.0 );
	const float lightRad = (float)( lightAng * M_PI / 180.0 );

	float eWidth = width, eSphere = sphere, eOGlow = outerGlow, eEdge = edgeLevel, eIGlow = innerGlow, eHot = hotCore;
	float eTend = tendrils, eReach = reach, ePlasma = plasma;
	float eTurb = turb, eTurbScale = turbScale, eRays = rays, eRayLen = rayLen, ePulse = pulse;
	const int pi = (int)preset;
	if( pi > 0 && pi < kNumPresets )
	{
		const auto& P = kPresets[ pi ];
		eWidth = P.width; eSphere = P.sphere; eOGlow = P.oglow; eEdge = P.edge; eIGlow = P.iglow; eHot = P.hot;
		eTend = P.tend; eReach = P.reach; ePlasma = P.plasma;
		eTurb = P.turb; eTurbScale = P.turbScale; eRays = P.rays; eRayLen = P.rayLen; ePulse = P.pulse;
	}

	float cRGB[ 3 ];
	gen::HSBtoRGB( centerCol, cRGB );

	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	UploadColorUniforms();
	glUniform2f( uResolution, (float)currentViewport.width, (float)currentViewport.height );
	glUniform1i( uMode, (int)mode );
	glUniform1f( uEdgeLevel, eEdge );
	glUniform1f( uHotCore, eHot );
	glUniform1f( uTendrils, eTend );
	glUniform1f( uReach, eReach );
	glUniform1f( uRadius, radius );
	glUniform1f( uWidth, eWidth );
	glUniform1f( uTilt, tiltRad );
	glUniform1f( uRotation, rotRad );
	glUniform1f( uSoft, softness );
	glUniform1f( uPhase, (float)phase );
	glUniform1f( uSphere, eSphere );
	glUniform1f( uLight, lightRad );
	glUniform1f( uOGlow, eOGlow );
	glUniform1f( uIGlow, eIGlow );
	glUniform1f( uGlowInt, glowInt );
	glUniform1f( uPlasma, ePlasma );
	glUniform1f( uTurb, eTurb );
	glUniform1f( uTurbScale, std::max( std::floor( eTurbScale ), 1.0f ) );
	glUniform1f( uRays, std::max( std::floor( eRays ), 0.0f ) );
	glUniform1f( uRayLen, eRayLen );
	glUniform1f( uPulse, ePulse );
	glUniform1f( uFlicker, flicker );
	glUniform1i( uCenterMode, (int)centerMode );
	glUniform4f( uCenterCol, cRGB[ 0 ], cRGB[ 1 ], cRGB[ 2 ], centerCol[ 3 ] );
	glUniform1f( uFeather, feather );
	glUniform1i( uColorBy, (int)colorBy );
	quad.Draw();
	return FF_SUCCESS;
}

FFResult FFGLHiddenRealms::DeInitGL()
{
	shader.FreeGLResources();
	quad.Release();
	return FF_SUCCESS;
}

FFResult FFGLHiddenRealms::SetFloatParameter( unsigned int index, float value )
{
	if( SetCommonParam( index, value ) )
		return FF_SUCCESS;
	if( index == pColorBy )
	{
		colorBy = value;
		return FF_SUCCESS;
	}
	if( index >= PT_CENTERCOL && index < PT_CENTERCOL + 4 )
	{
		centerCol[ index - PT_CENTERCOL ] = value;
		return FF_SUCCESS;
	}
	switch( index )
	{
	case PT_PRESET:     preset = value; UpdateVisibility( true ); break;
	case PT_MODE:       mode = value; UpdateVisibility( true ); break;
	case PT_RADIUS:     radius = value; break;
	case PT_WIDTH:      width = value; break;
	case PT_TILT:       tilt = value; break;
	case PT_ROTATION:   rotation = value; break;
	case PT_SPIN:       spin = value; break;
	case PT_SOFT:       softness = value; break;
	case PT_SPHERE:     sphere = value; break;
	case PT_LIGHT:      lightAng = value; break;
	case PT_OGLOW:      outerGlow = value; break;
	case PT_EDGELEVEL:  edgeLevel = value; break;
	case PT_IGLOW:      innerGlow = value; break;
	case PT_HOTCORE:    hotCore = value; break;
	case PT_TENDRILS:   tendrils = value; break;
	case PT_REACH:      reach = value; break;
	case PT_GLOWINT:    glowInt = value; break;
	case PT_PLASMA:     plasma = value; break;
	case PT_TURB:       turb = value; break;
	case PT_TURBSCALE:  turbScale = value; break;
	case PT_RAYS:       rays = value; break;
	case PT_RAYLEN:     rayLen = value; break;
	case PT_PULSE:      pulse = value; break;
	case PT_FLICKER:    flicker = value; break;
	case PT_CENTERMODE: centerMode = value; UpdateVisibility( true ); break;
	case PT_FEATHER:    feather = value; break;
	default:
		return FF_FAIL;
	}
	return FF_SUCCESS;
}

float FFGLHiddenRealms::GetFloatParameter( unsigned int index )
{
	float v;
	if( GetCommonParam( index, v ) )
		return v;
	if( index == pColorBy )
		return colorBy;
	if( index >= PT_CENTERCOL && index < PT_CENTERCOL + 4 )
		return centerCol[ index - PT_CENTERCOL ];
	switch( index )
	{
	case PT_PRESET:     return preset;
	case PT_MODE:       return mode;
	case PT_RADIUS:     return radius;
	case PT_WIDTH:      return width;
	case PT_TILT:       return tilt;
	case PT_ROTATION:   return rotation;
	case PT_SPIN:       return spin;
	case PT_SOFT:       return softness;
	case PT_SPHERE:     return sphere;
	case PT_LIGHT:      return lightAng;
	case PT_OGLOW:      return outerGlow;
	case PT_EDGELEVEL:  return edgeLevel;
	case PT_IGLOW:      return innerGlow;
	case PT_HOTCORE:    return hotCore;
	case PT_TENDRILS:   return tendrils;
	case PT_REACH:      return reach;
	case PT_GLOWINT:    return glowInt;
	case PT_PLASMA:     return plasma;
	case PT_TURB:       return turb;
	case PT_TURBSCALE:  return turbScale;
	case PT_RAYS:       return rays;
	case PT_RAYLEN:     return rayLen;
	case PT_PULSE:      return pulse;
	case PT_FLICKER:    return flicker;
	case PT_CENTERMODE: return centerMode;
	case PT_FEATHER:    return feather;
	}
	return 0.0f;
}

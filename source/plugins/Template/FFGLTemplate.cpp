#include "FFGLTemplate.h"
using namespace ffglex;

// ---- parameter indices: plugin-specific block first, then the shared blocks ----
enum ParamType : FFUInt32
{
	PT_PRESET,
	PT_RINGS,
	PT_RADIUS,
	PT_THICKNESS,
	PT_ROTATION,
	PT_SPIN,
	PT_SOFT,
	PT_SHAPE_END, // shared blocks start here
};

static CFFGLPluginInfo PluginInfo(
	PluginFactory< FFGLTemplate >,
	"TMPL",                       // 4-char unique ID: change it!
	"Template Rings",             // name shown in Resolume's Sources tab
	2, 1,                         // FFGL API version
	1, 0,                         // plugin version
	FF_SOURCE,
	"Animated concentric rings (generator template)",
	"Generated with the /generator skill"
);

// Presets: bundles of shape values. Index 0 = Custom leaves the sliders alone.
static const struct { const char* name; float rings, thickness; } kPresets[] = {
	{ "Custom", 0, 0 },
	{ "Thin lines", 12, 0.15f },
	{ "Fat bands", 5, 0.7f },
	{ "Dense", 30, 0.5f },
};
static const int kNumPresets = sizeof( kPresets ) / sizeof( kPresets[ 0 ] );

static const char fragmentBody[] = R"(
uniform vec2  resolution;
uniform float rings;
uniform float radius;
uniform float thickness;
uniform float rotation;   // radians
uniform float softPx;
uniform float phase;      // 0..1 from the Animation block

in vec2 uv;
out vec4 fragColor;

void main()
{
	float px = 1.0 / min( resolution.x, resolution.y );
	vec2 p = ( uv - 0.5 ) * resolution * px;             // shorter side spans -0.5..0.5
	float cs = cos( -rotation ), sn = sin( -rotation );
	p = vec2( cs * p.x - sn * p.y, sn * p.x + cs * p.y );

	float r = length( p );
	float soft = max( softPx, 0.01 ) * px;

	vec4 outCol = background();
	if( r < radius )
	{
		// rings move outward with phase; wrapping keeps the loop seamless
		float spacing = radius / rings;
		float k = r / spacing - phase;                    // ring coordinate
		float f = fract( k );
		float band = 1.0 - smoothstep( thickness * 0.5, thickness * 0.5 + soft / spacing, abs( f - 0.5 ) );
		float ringIndex = floor( k );
		vec3 col = shade( ringIndex / rings );            // colour by ring, periodic in the palette
		outCol = over( outCol, col, band );
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

FFGLTemplate::FFGLTemplate()
{
	// ---- Shape ----
	SetOptionParamInfo( PT_PRESET, "Preset", kNumPresets, preset );
	for( int i = 0; i < kNumPresets; ++i )
		SetParamElementInfo( PT_PRESET, i, kPresets[ i ].name, (float)i );
	SetParamInfo( PT_RINGS, "Rings", FF_TYPE_INTEGER, rings );
	SetParamRange( PT_RINGS, 1.0f, 60.0f );
	SetParamInfo( PT_RADIUS, "Radius", FF_TYPE_STANDARD, radius );
	SetParamRange( PT_RADIUS, 0.05f, 1.0f );
	SetParamInfo( PT_THICKNESS, "Thickness", FF_TYPE_STANDARD, thickness );
	SetParamRange( PT_THICKNESS, 0.02f, 1.0f );
	SetParamInfo( PT_ROTATION, "Rotation", FF_TYPE_STANDARD, rotation );
	SetParamRange( PT_ROTATION, 0.0f, 360.0f );
	SetParamInfo( PT_SPIN, "Spin", FF_TYPE_STANDARD, spin );
	SetParamRange( PT_SPIN, -180.0f, 180.0f );
	SetParamInfo( PT_SOFT, "Edge Softness", FF_TYPE_STANDARD, softness );
	SetParamRange( PT_SOFT, 0.0f, 4.0f );
	for( unsigned i = PT_PRESET; i < PT_SHAPE_END; ++i )
		SetParamGroup( i, "Shape" );

	// ---- shared blocks ----
	unsigned next = AddAnimationParams( PT_SHAPE_END );
	next          = AddColorParams( next );
	next          = AddBackgroundParams( next );

	UpdateVisibility( false );
	FFGLLog::LogToHost( "Created Template Rings generator" );
}

void FFGLTemplate::UpdateVisibility( bool raiseEvent )
{
	UpdateCommonVisibility( raiseEvent );
	// Presets override rings/thickness; hide those sliders unless Custom is selected.
	const bool custom = (int)preset == 0;
	SetParamVisibility( PT_RINGS, custom, raiseEvent );
	SetParamVisibility( PT_THICKNESS, custom, raiseEvent );
}

FFResult FFGLTemplate::InitGL( const FFGLViewportStruct* vp )
{
	if( !shader.Compile( vertexShaderCode, GetFragmentShaderSource().c_str() ) )
	{
		FFGLLog::LogToHost( "Template Rings: shader compile failed" );
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
	uRings      = shader.FindUniform( "rings" );
	uRadius     = shader.FindUniform( "radius" );
	uThickness  = shader.FindUniform( "thickness" );
	uRotation   = shader.FindUniform( "rotation" );
	uSoft       = shader.FindUniform( "softPx" );
	uPhase      = shader.FindUniform( "phase" );
	return CFFGLPlugin::InitGL( vp );
}

FFResult FFGLTemplate::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	Tick(); // updates dt, time, phase

	spinAccum = std::fmod( spinAccum + spin * dt, 360.0 );
	const float rotRad = (float)( ( rotation + spinAccum ) * M_PI / 180.0 );

	float effRings = rings, effThick = thickness;
	const int pi = (int)preset;
	if( pi > 0 && pi < kNumPresets )
	{
		effRings = kPresets[ pi ].rings;
		effThick = kPresets[ pi ].thickness;
	}

	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	UploadColorUniforms();
	glUniform2f( uResolution, (float)currentViewport.width, (float)currentViewport.height );
	glUniform1f( uRings, std::max( std::floor( effRings ), 1.0f ) );
	glUniform1f( uRadius, radius );
	glUniform1f( uThickness, effThick );
	glUniform1f( uRotation, rotRad );
	glUniform1f( uSoft, softness );
	glUniform1f( uPhase, (float)phase );
	quad.Draw();
	return FF_SUCCESS;
}

FFResult FFGLTemplate::DeInitGL()
{
	shader.FreeGLResources();
	quad.Release();
	return FF_SUCCESS;
}

FFResult FFGLTemplate::SetFloatParameter( unsigned int index, float value )
{
	if( SetCommonParam( index, value ) )
		return FF_SUCCESS;
	switch( index )
	{
	case PT_PRESET:    preset = value; UpdateVisibility( true ); break;
	case PT_RINGS:     rings = value; break;
	case PT_RADIUS:    radius = value; break;
	case PT_THICKNESS: thickness = value; break;
	case PT_ROTATION:  rotation = value; break;
	case PT_SPIN:      spin = value; break;
	case PT_SOFT:      softness = value; break;
	default:
		return FF_FAIL;
	}
	return FF_SUCCESS;
}

float FFGLTemplate::GetFloatParameter( unsigned int index )
{
	float v;
	if( GetCommonParam( index, v ) )
		return v;
	switch( index )
	{
	case PT_PRESET:    return preset;
	case PT_RINGS:     return rings;
	case PT_RADIUS:    return radius;
	case PT_THICKNESS: return thickness;
	case PT_ROTATION:  return rotation;
	case PT_SPIN:      return spin;
	case PT_SOFT:      return softness;
	}
	return 0.0f;
}

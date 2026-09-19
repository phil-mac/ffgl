// Offscreen preview: instantiate the plugin, render N presets (and optionally phases) to PNGs.
// Build with -DPLUGIN_CLASS=FFGLLiquidPaint -DPLUGIN_CPP="\"source/plugins/X/FFGLX.cpp\"" -lz
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>
#include PLUGIN_CPP

static void writePNG( const char* path, int w, int h, const unsigned char* rgba )
{
	std::vector< unsigned char > raw;
	raw.reserve( ( w * 4 + 1 ) * h );
	for( int y = h - 1; y >= 0; --y ) { raw.push_back( 0 ); raw.insert( raw.end(), rgba + y * w * 4, rgba + ( y + 1 ) * w * 4 ); }
	uLongf clen = compressBound( raw.size() );
	std::vector< unsigned char > comp( clen );
	compress( comp.data(), &clen, raw.data(), raw.size() );
	FILE* f = fopen( path, "wb" );
	auto be = [&]( unsigned v ) { unsigned char b[ 4 ] = { (unsigned char)( v >> 24 ), (unsigned char)( v >> 16 ), (unsigned char)( v >> 8 ), (unsigned char)v }; fwrite( b, 1, 4, f ); };
	auto chunk = [&]( const char* type, const unsigned char* data, unsigned len ) {
		be( len ); fwrite( type, 1, 4, f ); if( len ) fwrite( data, 1, len, f );
		unsigned crc = crc32( 0, (const Bytef*)type, 4 ); crc = crc32( crc, data, len ); be( crc );
	};
	const unsigned char sig[ 8 ] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	fwrite( sig, 1, 8, f );
	unsigned char ihdr[ 13 ] = { (unsigned char)( w >> 24 ), (unsigned char)( w >> 16 ), (unsigned char)( w >> 8 ), (unsigned char)w,
	                             (unsigned char)( h >> 24 ), (unsigned char)( h >> 16 ), (unsigned char)( h >> 8 ), (unsigned char)h, 8, 6, 0, 0, 0 };
	chunk( "IHDR", ihdr, 13 );
	chunk( "IDAT", comp.data(), (unsigned)clen );
	chunk( "IEND", nullptr, 0 );
	fclose( f );
}

int main( int argc, char** argv )
{
	// usage: render_preview out_prefix W H  [paramIndex=value ...]  phases (comma list) via env PHASES
	const char* prefix = argc > 1 ? argv[ 1 ] : "preview";
	int W = argc > 2 ? atoi( argv[ 2 ] ) : 640, H = argc > 3 ? atoi( argv[ 3 ] ) : 360;
	CGLPixelFormatAttribute attrs[] = { kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core, kCGLPFAAccelerated, (CGLPixelFormatAttribute)0 };
	CGLPixelFormatObj pix; GLint npix;
	if( CGLChoosePixelFormat( attrs, &pix, &npix ) != kCGLNoError || !pix ) { puts( "no pixel format" ); return 2; }
	CGLContextObj ctx;
	if( CGLCreateContext( pix, nullptr, &ctx ) != kCGLNoError ) { puts( "no context" ); return 2; }
	CGLSetCurrentContext( ctx );

	GLuint fbo, tex;
	glGenTextures( 1, &tex ); glBindTexture( GL_TEXTURE_2D, tex );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
	glGenFramebuffers( 1, &fbo ); glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
	glViewport( 0, 0, W, H );

	PLUGIN_CLASS plugin;
	FFGLViewportStruct vp = { 0, 0, (GLuint)W, (GLuint)H };
	if( plugin.InitGL( &vp ) != FF_SUCCESS ) { puts( "InitGL failed" ); return 1; }
	// generic params from argv: idx=value
	for( int i = 4; i < argc; ++i ) { unsigned idx; float v; if( sscanf( argv[ i ], "%u=%f", &idx, &v ) == 2 ) plugin.SetFloatParameter( idx, v ); }

	ProcessOpenGLStruct pgl = {}; pgl.HostFBO = fbo;
	std::vector< unsigned char > pixels( W * H * 4 );
	glClearColor( 0, 0, 0, 0 ); glClear( GL_COLOR_BUFFER_BIT );
	plugin.ProcessOpenGL( &pgl );
	glReadPixels( 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data() );
	std::string path = std::string( prefix ) + ".png";
	writePNG( path.c_str(), W, H, pixels.data() );
	printf( "wrote %s\n", path.c_str() );
	plugin.DeInitGL();
	return 0;
}

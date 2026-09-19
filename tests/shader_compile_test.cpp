// Offscreen GLSL compile check. Build with -DPLUGIN_CPP="\"source/plugins/<Name>/FFGL<Name>.cpp\"" -I.
// The included plugin must define `vertexShaderCode` and `static std::string GetFragmentShaderSource()`.
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <cstdio>
#include <string>
#include PLUGIN_CPP

static bool compile( GLenum type, const char* src, const char* label )
{
	GLuint s = glCreateShader( type );
	glShaderSource( s, 1, &src, nullptr );
	glCompileShader( s );
	GLint ok = 0;
	glGetShaderiv( s, GL_COMPILE_STATUS, &ok );
	char log[ 8192 ] = { 0 };
	glGetShaderInfoLog( s, sizeof( log ), nullptr, log );
	printf( "%s shader: %s\n%s", label, ok ? "OK" : "FAILED", log );
	return ok;
}

int main()
{
	CGLPixelFormatAttribute attrs[] = { kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core, kCGLPFAAccelerated, (CGLPixelFormatAttribute)0 };
	CGLPixelFormatObj pix; GLint npix;
	if( CGLChoosePixelFormat( attrs, &pix, &npix ) != kCGLNoError || !pix ) { puts( "no pixel format" ); return 2; }
	CGLContextObj ctx;
	if( CGLCreateContext( pix, nullptr, &ctx ) != kCGLNoError ) { puts( "no context" ); return 2; }
	CGLSetCurrentContext( ctx );
	std::string frag = GetFragmentShaderSource();
	bool a = compile( GL_VERTEX_SHADER, vertexShaderCode, "vertex" );
	bool b = compile( GL_FRAGMENT_SHADER, frag.c_str(), "fragment" );
	if( !b )
	{
		// print numbered source so error line numbers can be matched
		int line = 1; size_t pos = 0;
		while( pos < frag.size() ) { size_t nl = frag.find( '\n', pos ); if( nl == std::string::npos ) nl = frag.size(); printf( "%4d  %s\n", line++, frag.substr( pos, nl - pos ).c_str() ); pos = nl + 1; }
	}
	return ( a && b ) ? 0 : 1;
}

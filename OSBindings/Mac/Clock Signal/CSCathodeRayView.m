//
//  CSCathodeRayView.m
//  CLK
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "CSCathodeRayView.h"
@import CoreVideo;
#import <OpenGL/gl3.h>
#import <OpenGL/gl3ext.h>


@implementation CSCathodeRayView {
	CVDisplayLinkRef displayLink;

	GLuint _vertexShader, _fragmentShader;
	GLuint _shaderProgram;
	GLuint _arrayBuffer, _vertexArray;
	GLint _positionAttribute;
	GLint _textureCoordinatesAttribute;
	GLint _lateralAttribute;

	GLint _texIDUniform;
	GLint _textureSizeUniform;
	GLint _alphaUniform;

	GLuint _textureName;
	CRTSize _textureSize;

	CRTFrame *_crtFrame;
}

- (void)prepareOpenGL
{
	// Synchronize buffer swaps with vertical refresh rate
	GLint swapInt = 1;
	[[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

	// Create a display link capable of being used with all active displays
	CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
 
	// Set the renderer output callback function
	CVDisplayLinkSetOutputCallback(displayLink, DisplayLinkCallback, (__bridge void * __nullable)(self));
 
	// Set the display link for the current renderer
	CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
	CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
	CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(displayLink, cglContext, cglPixelFormat);

	// get the shader ready, set the clear colour
	[self.openGLContext makeCurrentContext];
	glClearColor(0.0, 0.0, 0.0, 1.0);
	[self prepareShader];

	// Activate the display link
	CVDisplayLinkStart(displayLink);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}

static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp *now, const CVTimeStamp *outputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut, void *displayLinkContext)
{
	CSCathodeRayView *view = (__bridge CSCathodeRayView *)displayLinkContext;
	[view.delegate openGLView:view didUpdateToTime:*now];
	return kCVReturnSuccess;
}

- (void)invalidate
{
	CVDisplayLinkStop(displayLink);
}

- (void)dealloc
{
	// Release the display link
	CVDisplayLinkRelease(displayLink);

	// Release OpenGL buffers
	[self.openGLContext makeCurrentContext];
	glDeleteBuffers(1, &_arrayBuffer);
	glDeleteVertexArrays(1, &_vertexArray);
	glDeleteTextures(1, &_textureName);
	glDeleteProgram(_shaderProgram);
}

- (void)reshape
{
	[super reshape];

	[self.openGLContext makeCurrentContext];
	CGLLockContext([[self openGLContext] CGLContextObj]);

	NSPoint backingSize = {.x = self.bounds.size.width, .y = self.bounds.size.height};
	NSPoint viewSize = [self convertPointToBacking:backingSize];
	glViewport(0, 0, (GLsizei)viewSize.x, (GLsizei)viewSize.y);

	CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

- (void)awakeFromNib
{
	NSOpenGLPixelFormatAttribute attributes[] =
	{
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAOpenGLProfile,	NSOpenGLProfileVersion3_2Core,
		NSOpenGLPFASampleBuffers,	1,
		NSOpenGLPFASamples,			16,
		0
	};

	NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	NSOpenGLContext *context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];

#ifdef DEBUG
	// When we're using a CoreProfile context, crash if we call a legacy OpenGL function
	// This will make it much more obvious where and when such a function call is made so
	// that we can remove such calls.
	// Without this we'd simply get GL_INVALID_OPERATION error for calling legacy functions
	// but it would be more difficult to see where that function was called.
	CGLEnable([context CGLContextObj], kCGLCECrashOnRemovedFunctions);
#endif

	self.pixelFormat = pixelFormat;
	self.openGLContext = context;
	self.wantsBestResolutionOpenGLSurface = YES;
}

- (GLint)formatForDepth:(unsigned int)depth
{
	switch(depth)
	{
		default: return -1;
		case 1: return GL_RED;
		case 2: return GL_RG;
		case 3: return GL_RGB;
		case 4: return GL_RGBA;
	}
}

- (BOOL)pushFrame:(CRTFrame * __nonnull)crtFrame
{
	[[self openGLContext] makeCurrentContext];
	CGLLockContext([[self openGLContext] CGLContextObj]);

	BOOL hadFrame = _crtFrame ? YES : NO;
	_crtFrame = crtFrame;

	glBufferData(GL_ARRAY_BUFFER, _crtFrame->number_of_runs * kCRTSizeOfVertex * 6, _crtFrame->runs, GL_DYNAMIC_DRAW);

	glBindTexture(GL_TEXTURE_2D, _textureName);
	if(_textureSize.width != _crtFrame->size.width || _textureSize.height != _crtFrame->size.height)
	{
		GLint format = [self formatForDepth:_crtFrame->buffers[0].depth];
		glTexImage2D(GL_TEXTURE_2D, 0, format, _crtFrame->size.width, _crtFrame->size.height, 0, (GLenum)format, GL_UNSIGNED_BYTE, _crtFrame->buffers[0].data);
		_textureSize = _crtFrame->size;
	}
	else
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _crtFrame->size.width, _crtFrame->dirty_size.height, (GLenum)[self formatForDepth:_crtFrame->buffers[0].depth], GL_UNSIGNED_BYTE, _crtFrame->buffers[0].data);

	[self drawView];

	CGLUnlockContext([[self openGLContext] CGLContextObj]);

	return hadFrame;
}

#pragma mark - Frame output

// the main job of the vertex shader is just to map from an input area of [0,1]x[0,1], with the origin in the
// top left to OpenGL's [-1,1]x[-1,1] with the origin in the lower left, and to convert input data coordinates
// from integral to floating point.
const char *vertexShader =
	"#version 150\n"
	"\n"
	"in vec2 position;\n"
	"in vec2 srcCoordinates;\n"
	"in float lateral;\n"
	"\n"
	"out vec2 srcCoordinatesVarying[7];\n"
	"out float lateralVarying;\n"
	"out float phase;\n"
	"\n"
	"uniform vec2 textureSize;\n"
	"\n"
	"void main (void)\n"
	"{\n"
		"srcCoordinatesVarying[2] = vec2(srcCoordinates.x / textureSize.x, (srcCoordinates.y + 0.5) / textureSize.y);\n"
		"srcCoordinatesVarying[0] = srcCoordinatesVarying[2] - vec2(0.4 / textureSize.x, 0.0);\n"
		"srcCoordinatesVarying[1] = srcCoordinatesVarying[2] - vec2(0.2 / textureSize.x, 0.0);\n"
		"srcCoordinatesVarying[4] = srcCoordinatesVarying[2] + vec2(0.4 / textureSize.x, 0.0);\n"
		"srcCoordinatesVarying[3] = srcCoordinatesVarying[2] + vec2(0.2 / textureSize.x, 0.0);\n"
		"srcCoordinatesVarying[5] = srcCoordinatesVarying[2] - vec2(0.1 / textureSize.x, 0.0);\n"
		"srcCoordinatesVarying[6] = srcCoordinatesVarying[2] + vec2(0.1 / textureSize.x, 0.0);\n"
		"lateralVarying = lateral + 1.0707963267949;\n"
		"phase = srcCoordinates.x;\n"
		"gl_Position = vec4(position.x * 2.0 - 1.0, 1.0 - position.y * 2.0 , 0.0, 1.0);\n" // + position.x / 131.0
	"}\n";

// TODO: this should be factored out and be per project
//
// TODO: this is _horribly_ written. It parallelises nothing. It's an embarassment. Same as for the above.
// This will be fixed. But, for now, it works.
const char *fragmentShader =
	"#version 150\n"
	"\n"
	"in vec2 srcCoordinatesVarying[7];\n"
	"in float lateralVarying;\n"
	"in float phase;\n"
	"out vec4 fragColour;\n"
	"\n"
	"uniform sampler2D texID;\n"
	"uniform float alpha;\n"
	"\n"
	"float angles[5];\n"
	"\n"
	"float sample(float c, float angle)\n"
	"{\n"
		"float y = 0.1 + (mod(c * 128.0, 8.0) / 8.0) * 0.9;\n"
		"float rawCh = (c * 16.0);\n"
		"float ch = mod(rawCh, 16.0);\n"
		"float aOffset = 2.0 * 3.141592654 * (ch - mod(ch, 1.0)) / 17.0;\n"
		"return y + step(1.0, rawCh) * (0.1 * sin(angle + aOffset));\n"
	"}\n"
	"\n"
	"void main(void)\n"
	"{\n"
		"float samples[7];\n"
		"float angles[7], i[5], q[5];\n"
		"\n"
		"angles[0] = mod(phase + 0.6, 1.0) * 2.0 * 3.141592654;\n"
		"angles[1] = mod(phase + 0.8, 1.0) * 2.0 * 3.141592654;\n"
		"angles[2] = mod(phase, 1.0) * 2.0 * 3.141592654;\n"
		"angles[3] = mod(phase + 0.2, 1.0) * 2.0 * 3.141592654;\n"
		"angles[4] = mod(phase + 0.4, 1.0) * 2.0 * 3.141592654;\n"
		"angles[5] = mod(phase + 0.9, 1.0) * 2.0 * 3.141592654;\n"
		"angles[6] = mod(phase + 0.1, 1.0) * 2.0 * 3.141592654;\n"
		"\n"
		"samples[0] = sample(texture(texID, srcCoordinatesVarying[0]).r, angles[0]);\n"
		"samples[1] = sample(texture(texID, srcCoordinatesVarying[1]).r, angles[1]);\n"
		"samples[2] = sample(texture(texID, srcCoordinatesVarying[2]).r, angles[2]);\n"
		"samples[3] = sample(texture(texID, srcCoordinatesVarying[3]).r, angles[3]);\n"
		"samples[4] = sample(texture(texID, srcCoordinatesVarying[4]).r, angles[4]);\n"
		"samples[5] = sample(texture(texID, srcCoordinatesVarying[5]).r, angles[5]);\n"
		"samples[6] = sample(texture(texID, srcCoordinatesVarying[6]).r, angles[6]);\n"
		"float y = (samples[0] + samples[1] + samples[2] + samples[3] + samples[4]) / 5.0;\n"
		"\n"
		"i[0] = (samples[1] - y) * sin(angles[1]);\n"
		"i[1] = (samples[5] - y) * sin(angles[5]);\n"
		"i[2] = (samples[2] - y) * sin(angles[2]);\n"
		"i[3] = (samples[6] - y) * sin(angles[6]);\n"
		"i[4] = (samples[3] - y) * sin(angles[3]);\n"
		"float iSum = (i[0] + i[1] + i[2] + i[3] + i[4]) * 1.1914;\n"
		"\n"
		"q[0] = (samples[1] - y) * cos(angles[1]);\n"
		"q[1] = (samples[5] - y) * cos(angles[5]);\n"
		"q[2] = (samples[2] - y) * cos(angles[2]);\n"
		"q[3] = (samples[6] - y) * cos(angles[6]);\n"
		"q[4] = (samples[3] - y) * cos(angles[3]);\n"
		"float qSum = (q[0] + q[1] + q[2] + q[3] + q[4]) * 1.0452;\n"
		"\n"
		"\n"
		"fragColour = vec4(	y + 0.965 * iSum + 0.621 * qSum, "
							"y - 0.272 * iSum - 0.647 * qSum, "
							"y - 1.106 * iSum + 1.703 * qSum, "
							"1.0);\n"
	"}\n";

#if defined(DEBUG)
- (void)logErrorForObject:(GLuint)object
{
	GLint logLength;
	glGetShaderiv(object, GL_INFO_LOG_LENGTH, &logLength);
	if (logLength > 0) {
		GLchar *log = (GLchar *)malloc((size_t)logLength);
		glGetShaderInfoLog(object, logLength, &logLength, log);
		NSLog(@"Compile log:\n%s", log);
		free(log);
	}
}
#endif

- (GLuint)compileShader:(const char *)source type:(GLenum)type
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

#ifdef DEBUG
	[self logErrorForObject:shader];
#endif

	return shader;
}

- (void)prepareShader
{
	_shaderProgram = glCreateProgram();
	_vertexShader = [self compileShader:vertexShader type:GL_VERTEX_SHADER];
	_fragmentShader = [self compileShader:fragmentShader type:GL_FRAGMENT_SHADER];

	glAttachShader(_shaderProgram, _vertexShader);
	glAttachShader(_shaderProgram, _fragmentShader);
	glLinkProgram(_shaderProgram);

#ifdef DEBUG
	[self logErrorForObject:_shaderProgram];
#endif

	glGenVertexArrays(1, &_vertexArray);
	glBindVertexArray(_vertexArray);
	glGenBuffers(1, &_arrayBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, _arrayBuffer);

	glUseProgram(_shaderProgram);

	_positionAttribute				= glGetAttribLocation(_shaderProgram, "position");
	_textureCoordinatesAttribute	= glGetAttribLocation(_shaderProgram, "srcCoordinates");
	_lateralAttribute				= glGetAttribLocation(_shaderProgram, "lateral");
	_texIDUniform					= glGetUniformLocation(_shaderProgram, "texID");
	_alphaUniform					= glGetUniformLocation(_shaderProgram, "alpha");
	_textureSizeUniform				= glGetUniformLocation(_shaderProgram, "textureSize");

	glEnableVertexAttribArray((GLuint)_positionAttribute);
	glEnableVertexAttribArray((GLuint)_textureCoordinatesAttribute);
	glEnableVertexAttribArray((GLuint)_lateralAttribute);

	const GLsizei vertexStride = kCRTSizeOfVertex;
	glVertexAttribPointer((GLuint)_positionAttribute,			2, GL_UNSIGNED_SHORT,	GL_TRUE,	vertexStride, (void *)kCRTVertexOffsetOfPosition);
	glVertexAttribPointer((GLuint)_textureCoordinatesAttribute, 2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)kCRTVertexOffsetOfTexCoord);
	glVertexAttribPointer((GLuint)_lateralAttribute,			1, GL_UNSIGNED_BYTE,	GL_FALSE,	vertexStride, (void *)kCRTVertexOffsetOfLateral);

	glGenTextures(1, &_textureName);
	glBindTexture(GL_TEXTURE_2D, _textureName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

- (void)drawRect:(NSRect)dirtyRect
{
	[self drawView];
}

- (void)drawView
{
	[self.openGLContext makeCurrentContext];
	CGLLockContext([[self openGLContext] CGLContextObj]);

	glClear(GL_COLOR_BUFFER_BIT);

	if (_crtFrame)
	{
		glUniform2f(_textureSizeUniform, _crtFrame->size.width, _crtFrame->size.height);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(_crtFrame->number_of_runs*6));
	}

	CGLFlushDrawable([[self openGLContext] CGLContextObj]);
	CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

#pragma mark - NSResponder

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)keyDown:(NSEvent *)theEvent
{
	[self.responderDelegate keyDown:theEvent];
}

- (void)keyUp:(NSEvent *)theEvent
{
	[self.responderDelegate keyUp:theEvent];
}

- (void)flagsChanged:(NSEvent *)theEvent
{
	[self.responderDelegate flagsChanged:theEvent];
}

@end

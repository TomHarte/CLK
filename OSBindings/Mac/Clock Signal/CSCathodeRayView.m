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
}

static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp* now, const CVTimeStamp* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* displayLinkContext)
{
	CSCathodeRayView *view = (__bridge CSCathodeRayView *)displayLinkContext;
	[view.delegate openGLView:view didUpdateToTime:*now];
	return kCVReturnSuccess;
}
 
- (void)dealloc
{
	// Release the display link
	CVDisplayLinkRelease(displayLink);
}

- (void)reshape
{
	[super reshape];

	[self.openGLContext makeCurrentContext];
	NSPoint backingSize = {.x = self.bounds.size.width, .y = self.bounds.size.height};
	NSPoint viewSize = [self convertPointToBacking:backingSize];
	glViewport(0, 0, viewSize.x, viewSize.y);
}

- (void) awakeFromNib
{
	NSOpenGLPixelFormatAttribute attributes[] =
	{
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAOpenGLProfile,	NSOpenGLProfileVersion3_2Core,
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

- (void)setCrtFrame:(CRTFrame *)crtFrame
{
	_crtFrame = crtFrame;
	[self setNeedsDisplay:YES];

	[self.openGLContext makeCurrentContext];
	glBufferData(GL_ARRAY_BUFFER, _crtFrame->number_of_runs * sizeof(GLushort) * 8, _crtFrame->runs, GL_DYNAMIC_DRAW);
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
	"\n"
	"out vec2 srcCoordinatesVarying;\n"
	"\n"
	"void main (void)\n"
	"{\n"
		"srcCoordinatesVarying = srcCoordinates;\n"
		"gl_Position = vec4(position.x * 2.0 - 1.0, 1.0 - position.y * 2.0, 0.0, 1.0);\n"
	"}\n";

// TODO: this should be factored out and be per project
const char *fragmentShader =
	"#version 150\n"
	"\n"
	"in vec2 srcCoordinatesVarying;\n"
	"out vec4 fragColour;\n"
	"\n"
	"void main(void)\n"
	"{\n"
		"fragColour = vec4(1.0, 1.0, 1.0, 1.0);\n"
	"}\n";

#if defined(DEBUG)
- (void)logErrorForObject:(GLuint)object
{
	GLint logLength;
	glGetShaderiv(object, GL_INFO_LOG_LENGTH, &logLength);
	if (logLength > 0) {
		GLchar *log = (GLchar *)malloc(logLength);
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

	_positionAttribute = glGetAttribLocation(_shaderProgram, "position");
	_textureCoordinatesAttribute = glGetAttribLocation(_shaderProgram, "srcCoordinates");

	glEnableVertexAttribArray(_positionAttribute);
	glEnableVertexAttribArray(_textureCoordinatesAttribute);

	glVertexAttribPointer(_positionAttribute, 2, GL_UNSIGNED_SHORT, GL_TRUE, 4 * sizeof(GLushort), (void *)0);
	glVertexAttribPointer(_textureCoordinatesAttribute, 2, GL_UNSIGNED_SHORT, GL_TRUE, 4 * sizeof(GLushort), (void *)(2 * sizeof(GLushort)));
}

- (void)drawRect:(NSRect)dirtyRect
{
	[self.openGLContext makeCurrentContext];

	glClear(GL_COLOR_BUFFER_BIT);

	if (_crtFrame)
	{
		glBufferData(GL_ARRAY_BUFFER, _crtFrame->number_of_runs * sizeof(GLushort) * 8, _crtFrame->runs, GL_DYNAMIC_DRAW);
		glDrawArrays(GL_LINES, 0, _crtFrame->number_of_runs*2);
	}

	glSwapAPPLE();
}

@end

//
//  CSCathodeRayView.m
//  CLK
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "CSCathodeRayView.h"
@import CoreVideo;
@import GLKit;
#import <OpenGL/gl3.h>
//#import <OpenGL/gl3ext.h>
//#import <libkern/OSAtomic.h>


@implementation CSCathodeRayView {
	CVDisplayLinkRef _displayLink;
	CGRect _aspectRatioCorrectedBounds;
}

/*- (GLuint)textureForImageNamed:(NSString *)name
{
	NSImage *const image = [NSImage imageNamed:name];
	NSBitmapImageRep *bitmapRepresentation = [[NSBitmapImageRep alloc] initWithData: [image TIFFRepresentation]];

	GLuint textureName;
	glGenTextures(1, &textureName);
	glBindTexture(GL_TEXTURE_2D, textureName);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)image.size.width, (GLsizei)image.size.height, 0, GL_RGB, GL_UNSIGNED_BYTE, bitmapRepresentation.bitmapData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateMipmap(GL_TEXTURE_2D);

	return textureName;
}*/

- (void)prepareOpenGL
{
	// Synchronize buffer swaps with vertical refresh rate
	GLint swapInt = 1;
	[[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

	// Create a display link capable of being used with all active displays
	CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
 
	// Set the renderer output callback function
	CVDisplayLinkSetOutputCallback(_displayLink, DisplayLinkCallback, (__bridge void * __nullable)(self));
 
	// Set the display link for the current renderer
	CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
	CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
	CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_displayLink, cglContext, cglPixelFormat);

	// install the shadow mask texture as the second texture
/*	glActiveTexture(GL_TEXTURE1);
	_shadowMaskTextureName = [self textureForImageNamed:@"ShadowMask"];

	// otherwise, we'll be working on the first texture
	glActiveTexture(GL_TEXTURE0);*/

	// get the shader ready, set the clear colour
	[self.openGLContext makeCurrentContext];
	glClearColor(0.0, 0.0, 0.0, 1.0);

	// Activate the display link
	CVDisplayLinkStart(_displayLink);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}

static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp *now, const CVTimeStamp *outputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut, void *displayLinkContext)
{
	CSCathodeRayView *view = (__bridge CSCathodeRayView *)displayLinkContext;
	[view.delegate openGLView:view didUpdateToTime:*now];
	[view drawViewOnlyIfDirty:YES];
	return kCVReturnSuccess;
}

- (void)invalidate
{
	CVDisplayLinkStop(_displayLink);
}

- (void)dealloc
{
	// Release the display link
	CVDisplayLinkRelease(_displayLink);

	// Release OpenGL buffers
//	[self.openGLContext makeCurrentContext];
//	glDeleteBuffers(1, &_arrayBuffer);
//	glDeleteVertexArrays(1, &_vertexArray);
//	glDeleteTextures(1, &_textureName);
//	glDeleteTextures(1, &_shadowMaskTextureName);
//	glDeleteProgram(_shaderProgram);
}

- (CGSize)backingSize
{
	return [self convertSizeToBacking:self.bounds.size];
}

- (void)reshape
{
	[super reshape];

	[self.openGLContext makeCurrentContext];
	CGLLockContext([[self openGLContext] CGLContextObj]);

	CGSize viewSize = [self backingSize];
	glViewport(0, 0, (GLsizei)viewSize.width, (GLsizei)viewSize.height);

//	[self pushSizeUniforms];

	CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

//- (void)setFrameBounds:(CGRect)frameBounds
//{
//	_frameBounds = frameBounds;
//
//	[self.openGLContext makeCurrentContext];
//	CGLLockContext([[self openGLContext] CGLContextObj]);
//
//	[self pushSizeUniforms];
//
//	CGLUnlockContext([[self openGLContext] CGLContextObj]);
//}
//
//- (void)pushSizeUniforms
//{
//	if(_shaderProgram)
//	{
//		NSPoint viewSize = [self backingViewSize];
//		if(_windowSizeUniform >= 0)
//		{
//			glUniform2f(_windowSizeUniform, (GLfloat)viewSize.x, (GLfloat)viewSize.y);
//		}
//
//		CGFloat outputAspectRatioMultiplier = (viewSize.x / viewSize.y) / (4.0 / 3.0);
//
////		NSLog(@"%0.2f v %0.2f", outputAspectRatio, desiredOutputAspectRatio);
//		_aspectRatioCorrectedBounds = _frameBounds;
//
//		CGFloat bonusWidth = (outputAspectRatioMultiplier - 1.0f) * _frameBounds.size.width;
//		_aspectRatioCorrectedBounds.origin.x -= bonusWidth * 0.5f * _aspectRatioCorrectedBounds.size.width;
//		_aspectRatioCorrectedBounds.size.width *= outputAspectRatioMultiplier;
//
//		if(_boundsOriginUniform >= 0) glUniform2f(_boundsOriginUniform, (GLfloat)_aspectRatioCorrectedBounds.origin.x, (GLfloat)_aspectRatioCorrectedBounds.origin.y);
//		if(_boundsSizeUniform >= 0) glUniform2f(_boundsSizeUniform, (GLfloat)_aspectRatioCorrectedBounds.size.width, (GLfloat)_aspectRatioCorrectedBounds.size.height);
//	}
//}

- (void)awakeFromNib
{
	NSOpenGLPixelFormatAttribute attributes[] =
	{
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAOpenGLProfile,	NSOpenGLProfileVersion3_2Core,
		NSOpenGLPFASampleBuffers,	1,
		NSOpenGLPFASamples,			2,
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

	// establish default instance variable values
//	self.frameBounds = CGRectMake(0.0, 0.0, 1.0, 1.0);
}

/*

#pragma mark - Frame output

#if defined(DEBUG)
- (void)logErrorForObject:(GLuint)object
{
	GLint isCompiled = 0;
	glGetShaderiv(object, GL_COMPILE_STATUS, &isCompiled);
	if(isCompiled == GL_FALSE)
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

- (void)setSignalDecoder:(nonnull NSString *)signalDecoder type:(CSCathodeRayViewSignalType)type
{
	_signalType	= type;
	_signalDecoder = [signalDecoder copy];
	OSAtomicIncrement32(&_signalDecoderGeneration);
}

- (void)prepareShader
{
	if(_shaderProgram)
	{
		glDeleteProgram(_shaderProgram);
		glDeleteShader(_vertexShader);
		glDeleteShader(_fragmentShader);
	}

	if(!_signalDecoder)
		return;

	NSString *const vertexShader = [self vertexShaderForType:_signalType];
	NSString *const fragmentShader = [self fragmentShaderForType:_signalType];

	_shaderProgram = glCreateProgram();
	_vertexShader = [self compileShader:[vertexShader UTF8String] type:GL_VERTEX_SHADER];
	_fragmentShader = [self compileShader:_signalDecoder ?
							[[NSString stringWithFormat:fragmentShader, _signalDecoder] UTF8String] :
							[fragmentShader UTF8String]
						type:GL_FRAGMENT_SHADER];

	glAttachShader(_shaderProgram, _vertexShader);
	glAttachShader(_shaderProgram, _fragmentShader);
	glLinkProgram(_shaderProgram);

#ifdef DEBUG
//	[self logErrorForObject:_shaderProgram];
#endif


	glUseProgram(_shaderProgram);

	_positionAttribute				= glGetAttribLocation(_shaderProgram, "position");
	_textureCoordinatesAttribute	= glGetAttribLocation(_shaderProgram, "srcCoordinates");
	_lateralAttribute				= glGetAttribLocation(_shaderProgram, "lateral");
	_alphaUniform					= glGetUniformLocation(_shaderProgram, "alpha");
	_textureSizeUniform				= glGetUniformLocation(_shaderProgram, "textureSize");
	_windowSizeUniform				= glGetUniformLocation(_shaderProgram, "windowSize");
	_boundsSizeUniform				= glGetUniformLocation(_shaderProgram, "boundsSize");
	_boundsOriginUniform			= glGetUniformLocation(_shaderProgram, "boundsOrigin");

	GLint texIDUniform				= glGetUniformLocation(_shaderProgram, "texID");
	GLint shadowMaskTexIDUniform	= glGetUniformLocation(_shaderProgram, "shadowMaskTexID");

	[self pushSizeUniforms];

	glUniform1i(texIDUniform, 0);
	glUniform1i(shadowMaskTexIDUniform, 1);

	glEnableVertexAttribArray((GLuint)_positionAttribute);
	glEnableVertexAttribArray((GLuint)_textureCoordinatesAttribute);
	glEnableVertexAttribArray((GLuint)_lateralAttribute);

	const GLsizei vertexStride = kCRTSizeOfVertex;
	glVertexAttribPointer((GLuint)_positionAttribute,			2, GL_UNSIGNED_SHORT,	GL_TRUE,	vertexStride, (void *)kCRTVertexOffsetOfPosition);
	glVertexAttribPointer((GLuint)_textureCoordinatesAttribute, 2, GL_UNSIGNED_SHORT,	GL_FALSE,	vertexStride, (void *)kCRTVertexOffsetOfTexCoord);
	glVertexAttribPointer((GLuint)_lateralAttribute,			1, GL_UNSIGNED_BYTE,	GL_FALSE,	vertexStride, (void *)kCRTVertexOffsetOfLateral);

}*/

- (void)drawRect:(NSRect)dirtyRect
{
	[self drawViewOnlyIfDirty:NO];
}

- (void)drawViewOnlyIfDirty:(BOOL)onlyIfDirty
{
	[self.openGLContext makeCurrentContext];
	CGLLockContext([[self openGLContext] CGLContextObj]);

	[self.delegate openGLView:self drawViewOnlyIfDirty:onlyIfDirty];

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

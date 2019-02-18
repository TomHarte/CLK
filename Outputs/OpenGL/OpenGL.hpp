//
//  OpenGL.h
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef OpenGL_h
#define OpenGL_h

// TODO: figure out correct include paths for other platforms.
#ifdef __APPLE__
	#if TARGET_OS_IPHONE
	#else
		#include <OpenGL/OpenGL.h>
		#include <OpenGL/gl3.h>
		#include <OpenGL/gl3ext.h>
	#endif
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#endif

// To consider: might it be smarter to switch and log on error,
// rather than raising an exception? They're conventionally
// something you're permitted to ignore.
//
// (and, from that indecision, hence the pointless decision
// on whether to use an assert based on NDEBUG)
#ifndef NDEBUG
#define test_gl_error() assert(!glGetError());
#else
#define test_gl_error() while(false) {}
#endif

#ifndef NDEBUG
#define test_gl(command, ...) do { command(__VA_ARGS__); test_gl_error(); } while(false);
#else
#define test_gl(command, ...) command(__VA_ARGS__)
#endif

#endif /* OpenGL_h */

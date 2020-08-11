//
//  OpenGL.h
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef OpenGL_h
#define OpenGL_h

#include <cassert>
#include <iostream>

// TODO: figure out correct include paths for other platforms.
#ifdef __APPLE__
	#if TARGET_OS_IPHONE
	#else
		// These remain so that I can, at least for now, build the kiosk version under macOS.
		// They can be eliminated if and when Apple fully withdraws OpenGL support.
		#include <OpenGL/OpenGL.h>
		#include <OpenGL/gl3.h>
		#include <OpenGL/gl3ext.h>
	#endif
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#ifndef NDEBUG

#define test_gl_error() { \
	const auto error = glGetError();	\
	if(error) { \
		switch(error) {	\
			default:								std::cerr << "Error " << error;						break;	\
			case GL_INVALID_ENUM:					std::cerr << "GL_INVALID_ENUM";						break;	\
			case GL_INVALID_VALUE:					std::cerr << "GL_INVALID_VALUE";					break;	\
			case GL_INVALID_OPERATION:				std::cerr << "GL_INVALID_OPERATION";				break;	\
			case GL_INVALID_FRAMEBUFFER_OPERATION:	std::cerr << "GL_INVALID_FRAMEBUFFER_OPERATION";	break;	\
			case GL_OUT_OF_MEMORY:					std::cerr << "GL_OUT_OF_MEMORY";					break;	\
		};	\
		std::cerr << " at line " << __LINE__ << " in " << __FILE__ << std::endl;	\
		assert(false);	\
	}	\
	\
}

#else
#define test_gl_error() while(false) {}
#endif

#ifndef NDEBUG
#define test_gl(command, ...) do { command(__VA_ARGS__); test_gl_error(); } while(false);
#else
#define test_gl(command, ...) command(__VA_ARGS__)
#endif

#endif /* OpenGL_h */

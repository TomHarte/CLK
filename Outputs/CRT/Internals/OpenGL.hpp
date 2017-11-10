//
//  OpenGL.h
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
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

#endif /* OpenGL_h */

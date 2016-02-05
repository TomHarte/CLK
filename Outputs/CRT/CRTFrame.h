//
//  CRTFrame.h
//  Clock Signal
//
//  Created by Thomas Harte on 24/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef CRTFrame_h
#define CRTFrame_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint8_t *data;
	unsigned int depth;
} CRTBuffer;

typedef struct {
	uint16_t width, height;
} CRTSize;

typedef enum {
	CRTGeometryModeTriangles
} CRTGeometryMode;

typedef struct {
	/** The total size, in pixels, of the pixel buffer storage. Guaranteed to be a power of two. */
	CRTSize size;

	/** The portion of the pixel buffer that has been changed since the last time this set of buffers was provided. */
	CRTSize dirty_size;

	/** The number of individual buffers that adds up to the complete pixel buffer. */
	unsigned int number_of_buffers;

	/** A C array of those buffers. */
	CRTBuffer *buffers;

	/** The number of vertices that constitute the output. */
	unsigned int number_of_vertices;

	/** The type of output. */
	CRTGeometryMode geometry_mode;

	/** The size of each vertex in bytes. */
	size_t size_per_vertex;

	/** The vertex data. */
	uint8_t *vertices;
} CRTFrame;

// TODO: these should be private to whomever builds the shaders
static const size_t kCRTVertexOffsetOfPosition = 0;
static const size_t kCRTVertexOffsetOfTexCoord = 4;
static const size_t kCRTVertexOffsetOfLateral = 8;
static const size_t kCRTVertexOffsetOfPhase = 9;

static const int kCRTSizeOfVertex = 10;

#ifdef __cplusplus
}
#endif

#endif /* CRTFrame_h */

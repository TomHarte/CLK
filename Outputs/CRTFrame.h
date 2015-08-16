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

typedef struct {
	CRTSize size, dirty_size;

	unsigned int number_of_buffers;
	CRTBuffer *buffers;

	unsigned int number_of_runs;
	uint8_t *runs;
} CRTFrame;

#ifdef __cplusplus
}
#endif


typedef uint16_t	kCRTPositionType;
typedef uint16_t	kCRTTexCoordType;
typedef uint8_t		kCRTLateralType;
typedef uint8_t		kCRTPhaseType;

static const size_t kCRTVertexOffsetOfPosition = 0;
static const size_t kCRTVertexOffsetOfTexCoord = 4;
static const size_t kCRTVertexOffsetOfLateral = 8;
static const size_t kCRTVertexOffsetOfPhase = 9;

static const int kCRTSizeOfVertex = 10;

#endif /* CRTFrame_h */

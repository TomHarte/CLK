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
	int depth;
} CRTBuffer;

typedef struct {
	int width, height;
} CRTSize;

typedef struct {
	CRTSize size, dirty_size;

	int number_of_buffers;
	CRTBuffer *buffers;

	int number_of_runs;
	uint16_t *runs;
} CRTFrame;

#ifdef __cplusplus
}
#endif

#endif /* CRTFrame_h */

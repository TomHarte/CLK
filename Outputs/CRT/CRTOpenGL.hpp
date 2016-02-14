//
//  CRTOpenGL.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTOpenGL_h
#define CRTOpenGL_h

const size_t kCRTVertexOffsetOfPosition = 0;
const size_t kCRTVertexOffsetOfTexCoord = 2;
const size_t kCRTVertexOffsetOfTimestamp = 4;
const size_t kCRTVertexOffsetOfLateral = 8;

const size_t kCRTSizeOfVertex = 10;

const int CRTInputBufferBuilderWidth = 2048;
const int CRTInputBufferBuilderHeight = 1024;

const int CRTIntermediateBufferWidth = 2048;
const int CRTIntermediateBufferHeight = 2048;

#endif /* CRTOpenGL_h */

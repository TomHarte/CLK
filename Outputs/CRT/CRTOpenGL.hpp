//
//  CRTOpenGL.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTOpenGL_h
#define CRTOpenGL_h

// Output vertices are those used to copy from an input buffer — whether it describes data that maps directly to RGB
// or is one of the intermediate buffers that we've used to convert from composite towards RGB.
const size_t kCRTOutputVertexOffsetOfPosition = 0;
const size_t kCRTOutputVertexOffsetOfTexCoord = 4;
const size_t kCRTOutputVertexOffsetOfTimestamp = 8;
const size_t kCRTOutputVertexOffsetOfLateral = 12;

const size_t kCRTOutputVertexSize = 16;

// Input vertices, used only in composite mode, map from the input buffer to temporary buffer locations; such
// remapping occurs to ensure a continous stream of data for each scan, giving correct out-of-bounds behaviour
const size_t kCRTInputVertexOffsetOfInputPosition = 0;
const size_t kCRTInputVertexOffsetOfOutputPosition = 4;
const size_t kCRTInputVertexOffsetOfPhaseAndAmplitude = 8;
const size_t kCRTInputVertexOffsetOfPhaseTime = 12;

const size_t kCRTInputVertexSize = 16;

// These constants hold the size of the rolling buffer to which the CPU writes
const int CRTInputBufferBuilderWidth = 2048;
const int CRTInputBufferBuilderHeight = 1024;

// This is the size of the intermediate buffers used during composite to RGB conversion
const int CRTIntermediateBufferWidth = 2048;
const int CRTIntermediateBufferHeight = 2048;

// Runs are divided discretely by vertical syncs in order to put a usable bounds on the uniform used to track
// run age; that therefore creates a discrete number of fields that are stored. This number should be the
// number of historic fields that are required fully to 
const int kCRTNumberOfFields = 3;

#endif /* CRTOpenGL_h */

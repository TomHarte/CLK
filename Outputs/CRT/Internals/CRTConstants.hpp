//
//  CRTContants.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTConstants_h
#define CRTConstants_h

#include "OpenGL.hpp"
#include <cstddef>

namespace Outputs {
namespace CRT {

// Output vertices are those used to copy from an input buffer — whether it describes data that maps directly to RGB
// or is one of the intermediate buffers that we've used to convert from composite towards RGB.
const GLsizei OutputVertexOffsetOfPosition = 0;
const GLsizei OutputVertexOffsetOfTexCoord = 4;

const GLsizei OutputVertexSize = 8;

// Input vertices, used only in composite mode, map from the input buffer to temporary buffer locations; such
// remapping occurs to ensure a continous stream of data for each scan, giving correct out-of-bounds behaviour
const GLsizei SourceVertexOffsetOfInputPosition = 0;
const GLsizei SourceVertexOffsetOfOutputPosition = 4;
const GLsizei SourceVertexOffsetOfPhaseAmplitudeAndOffset = 8;
const GLsizei SourceVertexOffsetOfPhaseTime = 12;

const GLsizei SourceVertexSize = 16;

// These constants hold the size of the rolling buffer to which the CPU writes
const GLsizei InputBufferBuilderWidth = 2048;
const GLsizei InputBufferBuilderHeight = 1024;

// This is the size of the intermediate buffers used during composite to RGB conversion
const GLsizei IntermediateBufferWidth = 2048;
const GLsizei IntermediateBufferHeight = 2048;

// Some internal buffer sizes
const GLsizeiptr OutputVertexBufferDataSize = 59904;	// a multiple of 6 * OutputVertexSize
const GLsizeiptr SourceVertexBufferDataSize = 87360;	// a multiple of 2 * SourceVertexSize

}
}

#endif /* CRTContants_h */

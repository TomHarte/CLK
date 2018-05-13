//
//  CRTContants.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTConstants_h
#define CRTConstants_h

#include "OpenGL.hpp"
#include <cstddef>

namespace Outputs {
namespace CRT {

// Output vertices are those used to copy from an input buffer — whether it describes data that maps directly to RGB
// or is one of the intermediate buffers that we've used to convert from composite towards RGB.
const GLsizei OutputVertexOffsetOfHorizontal = 0;
const GLsizei OutputVertexOffsetOfVertical = 4;

const GLsizei OutputVertexSize = 8;

// Input vertices, used only in composite mode, map from the input buffer to temporary buffer locations; such
// remapping occurs to ensure a continous stream of data for each scan, giving correct out-of-bounds behaviour
const GLsizei SourceVertexOffsetOfInputStart = 0;
const GLsizei SourceVertexOffsetOfOutputStart = 4;
const GLsizei SourceVertexOffsetOfEnds = 8;
const GLsizei SourceVertexOffsetOfPhaseTimeAndAmplitude = 12;

const GLsizei SourceVertexSize = 16;

// These constants hold the size of the rolling buffer to which the CPU writes
const GLsizei InputBufferBuilderWidth = 2048;
const GLsizei InputBufferBuilderHeight = 512;

// This is the size of the intermediate buffers used during composite to RGB conversion
const GLsizei IntermediateBufferWidth = 2048;
const GLsizei IntermediateBufferHeight = 512;

// Some internal buffer sizes
const GLsizeiptr OutputVertexBufferDataSize = OutputVertexSize * IntermediateBufferHeight;		// i.e. the maximum number of scans of output that can be created between draws
const GLsizeiptr SourceVertexBufferDataSize = SourceVertexSize * IntermediateBufferHeight * 10;	// (the maximum number of scans) * conservative, high guess at a maximumum number of events likely to occur within a scan

// TODO: when SourceVertexBufferDataSize is exhausted, the CRT keeps filling OutputVertexBufferDataSize regardless,
// leading to empty scanlines that nevertheless clear old contents.

}
}

#endif /* CRTContants_h */

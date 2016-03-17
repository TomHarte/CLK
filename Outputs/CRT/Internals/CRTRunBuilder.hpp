//
//  CRTRunBuilder.h
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTRunBuilder_h
#define CRTRunBuilder_h

#import <vector>

namespace Outputs {
namespace CRT {

struct CRTRunBuilder {
	CRTRunBuilder(size_t vertex_size) : _vertex_size(vertex_size), duration(0), start(0), number_of_vertices(0) {}	// reset();

	// Resets the run builder.
//	void reset();

	// Getter for new storage plus backing storage; in RGB mode input runs will map directly
	// from the input buffer to the screen. In composite mode input runs will map from the
	// input buffer to the processing buffer, and output runs will map from the processing
	// buffer to the screen.
//	uint8_t *get_next_run(size_t number_of_vertices);
//	std::vector<uint8_t> _runs;

	// Container for total length in cycles of all contained runs.
	uint32_t duration;
	size_t start;

	// Storage for the length of run data uploaded so far; reset to zero by reset but otherwise
	// entrusted to the CRT to update.
//	size_t uploaded_vertices;
	size_t number_of_vertices;

	private:
		size_t _vertex_size;
};

}
}

#endif /* CRTRunBuilder_h */

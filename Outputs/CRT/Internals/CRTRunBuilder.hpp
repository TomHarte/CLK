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
	CRTRunBuilder() : start(0) { reset(); }

	// Resets the run builder.
	inline void reset()
	{
		duration = 0;
		amount_of_uploaded_data = 0;
		amount_of_data = 0;
	}

	// Container for total length in cycles of all contained runs.
	uint32_t duration;
	size_t start;

	// Storage for the length of run data uploaded so far; reset to zero by reset but otherwise
	// entrusted to the CRT to update.
	size_t amount_of_uploaded_data;
	size_t amount_of_data;
};

}
}

#endif /* CRTRunBuilder_h */

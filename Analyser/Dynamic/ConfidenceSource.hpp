//
//  ConfidenceSource.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef ConfidenceSource_hpp
#define ConfidenceSource_hpp

namespace Analyser {
namespace Dynamic {

/*!
	Provides an abstract interface through which objects can declare the probability
	that they are the proper target for their input; e.g. if an Acorn Electron is asked
	to run an Atari 2600 program then its confidence should shrink towards 0.0; if the
	program is handed to an Atari 2600 then its confidence should grow towards 1.0.
*/
struct ConfidenceSource {
	virtual float get_confidence() = 0;
};

}
}

#endif /* ConfidenceSource_hpp */

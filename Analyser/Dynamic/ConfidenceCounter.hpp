//
//  ConfidenceCounter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef ConfidenceCounter_hpp
#define ConfidenceCounter_hpp

#include "ConfidenceSource.hpp"

namespace Analyser {
namespace Dynamic {

/*!
	Provides a confidence source that calculates its probability by virtual of a history of events.

	The initial value of the confidence counter is 0.5.
*/
class ConfidenceCounter: public ConfidenceSource {
	public:
		/*! @returns The computed probability, based on the history of events. */
		float get_confidence() final;

		/*! Records an event that implies this is the appropriate class: pushes probability up towards 1.0. */
		void add_hit();

		/*! Records an event that implies this is not the appropriate class: pushes probability down towards 0.0. */
		void add_miss();

		/*!
			Records an event that could be correct but isn't necessarily so; which can push probability
			down towards 0.5, but will never push it upwards.
		*/
		void add_equivocal();

	private:
		int hits_ = 1;
		int misses_ = 1;
};

}
}

#endif /* ConfidenceCounter_hpp */

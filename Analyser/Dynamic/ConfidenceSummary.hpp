//
//  ConfidenceSummary.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "ConfidenceSource.hpp"

#include <vector>

namespace Analyser::Dynamic {

/*!
	Summaries a collection of confidence sources by calculating their weighted sum.
*/
class ConfidenceSummary: public ConfidenceSource {
public:
	/*!
		Instantiates a summary that will produce the weighted sum of
		@c sources, each using the corresponding entry of @c weights.

		Requires that @c sources and @c weights are of the same length.
	*/
	ConfidenceSummary(
		const std::vector<ConfidenceSource *> &sources,
		const std::vector<float> &weights);

	/*! @returns The weighted sum of all sources. */
	float confidence() const final;

private:
	const std::vector<ConfidenceSource *> sources_;
	const std::vector<float> weights_;
	float weight_sum_;
};

}

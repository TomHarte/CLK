//
//  ConfidenceSummary.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "ConfidenceSummary.hpp"

#include <cassert>
#include <numeric>

using namespace Analyser::Dynamic;

ConfidenceSummary::ConfidenceSummary(const std::vector<ConfidenceSource *> &sources, const std::vector<float> &weights) :
	sources_(sources), weights_(weights) {
	assert(weights.size() == sources.size());
	weight_sum_ = std::accumulate(weights.begin(), weights.end(), 0.0f);
}

float ConfidenceSummary::get_confidence() {
	float result = 0.0f;
	for(std::size_t index = 0; index < sources_.size(); ++index) {
		result += sources_[index]->get_confidence() * weights_[index];
	}
	return result / weight_sum_;
}

//
//  FilterGenerator.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

namespace Outputs::Display {

class FilterGenerator {
public:
	FilterGenerator(float samples_per_line, float subcarrier_frequency);

private:
	float samples_per_line_;
	float subcarrier_frequency_;
};

}

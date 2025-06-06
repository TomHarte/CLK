//
//  SNA.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/04/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"

namespace Storage::State {

struct SNA {
	static std::unique_ptr<Analyser::Static::Target> load(const std::string &file_name);
};

}

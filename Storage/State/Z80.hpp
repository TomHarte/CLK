//
//  Z80.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/04/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"

#include <filesystem>

namespace Storage::State {

struct Z80 {
	static std::unique_ptr<Analyser::Static::Target> load(const std::filesystem::path &);
};

}

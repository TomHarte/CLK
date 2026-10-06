//
//  SZX.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/04/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"

#include <filesystem>

namespace Storage::State {

struct SZX {
	static std::unique_ptr<Analyser::Static::Target> load(const std::filesystem::path &);
};

}

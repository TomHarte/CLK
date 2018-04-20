//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

Analyser::Static::TargetList Analyser::Static::AppleII::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms) {
	return TargetList();
}

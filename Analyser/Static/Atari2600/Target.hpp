//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../StaticAnalyser.hpp"

namespace Analyser::Static::Atari2600 {

struct Target: public ::Analyser::Static::Target {
	enum class PagingModel {
		None,
		CommaVid,
		Atari8k,
		Atari16k,
		Atari32k,
		ActivisionStack,
		ParkerBros,
		Tigervision,
		CBSRamPlus,
		MNetwork,
		MegaBoy,
		Pitfall2
	};

	// TODO: shouldn't these be properties of the cartridge?
	PagingModel paging_model = PagingModel::None;
	bool uses_superchip = false;

	Target() : Analyser::Static::Target(Machine::Atari2600) {}
};

}

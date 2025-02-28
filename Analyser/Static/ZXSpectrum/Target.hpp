//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Reflection/Enum.hpp"
#include "Reflection/Struct.hpp"

namespace Analyser::Static::ZXSpectrum {

struct Target: public ::Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(Model,
		SixteenK,
		FortyEightK,
		OneTwoEightK,
		Plus2,
		Plus2a,
		Plus3,
	);

	Model model = Model::Plus2;
	bool should_hold_enter = false;

	Target(): Analyser::Static::Target(Machine::ZXSpectrum) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		DeclareField(model);
		AnnounceEnum(Model);
	}
};

}

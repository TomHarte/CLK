//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Reflection/Enum.hpp"
#include "Reflection/Struct.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include <string>

namespace Analyser::Static::Thomson {

struct MOTarget: public Analyser::Static::Target, public Reflection::StructImpl<MOTarget> {
	std::string loading_command;

	ReflectableEnum(Floppy, None, CD90_640);
	Floppy floppy = Floppy::None;

	ReflectableEnum(Model, MO5v1, MO5v11, MO6v1, MO6v2, MO6v3);
	Model model = Model::MO5v11;

	MOTarget() : Analyser::Static::Target(Machine::ThomsonMO) {}

private:
	friend Reflection::StructImpl<MOTarget>;
	void declare_fields() {
		AnnounceEnum(Floppy);
		DeclareField(floppy);
		AnnounceEnum(Model);
		DeclareField(model);

		// TODO: eliminate this if/when MO6 emulation works.
		limit_enum(
			&model,
			Model::MO5v1,
			Model::MO5v11,
			-1
		);
	}
};

static constexpr bool is_mo6(const MOTarget::Model model) {
	return model >= MOTarget::Model::MO6v1;
}

}

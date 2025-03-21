//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Reflection/Enum.hpp"
#include "Reflection/Struct.hpp"

namespace Analyser::Static::Sega {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	enum class Model {
		SG1000,
		MasterSystem,
		MasterSystem2,
	};

	ReflectableEnum(Region,
		Japan,
		USA,
		Europe,
		Brazil
	);

	enum class PagingScheme {
		Sega,
		Codemasters
	};

	Model model = Model::MasterSystem;
	Region region = Region::Japan;
	PagingScheme paging_scheme = PagingScheme::Sega;

	Target() : Analyser::Static::Target(Machine::MasterSystem) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		DeclareField(region);
		AnnounceEnum(Region);
	}
};

constexpr bool is_master_system(const Analyser::Static::Sega::Target::Model model) {
	return model >= Analyser::Static::Sega::Target::Model::MasterSystem;
}

}

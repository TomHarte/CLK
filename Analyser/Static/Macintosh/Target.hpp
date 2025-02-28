//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Reflection/Enum.hpp"
#include "Reflection/Struct.hpp"

namespace Analyser::Static::Macintosh {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(Model, Mac128k, Mac512k, Mac512ke, MacPlus);
	Model model = Model::MacPlus;

	Target() : Analyser::Static::Target(Machine::Macintosh) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		DeclareField(model);
		AnnounceEnum(Model);
	}
};

}

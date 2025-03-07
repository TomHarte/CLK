//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Reflection/Struct.hpp"

namespace Analyser::Static::PCCompatible {

ReflectableEnum(Model,
	XT,
	TurboXT,
	AT
);

constexpr bool is_xt(const Model model) {
	return model <= Model::TurboXT;
}

constexpr bool is_at(const Model model) {
	return model >= Model::AT;
}

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(VideoAdaptor,
		MDA,
		CGA,
	);
	VideoAdaptor adaptor = VideoAdaptor::CGA;
	Model model = Model::TurboXT;

	Target() : Analyser::Static::Target(Machine::PCCompatible) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		AnnounceEnum(VideoAdaptor);
		AnnounceEnum(Model);
		DeclareField(adaptor);
		DeclareField(model);
	}
};

}

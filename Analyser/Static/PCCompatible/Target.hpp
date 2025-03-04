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

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(VideoAdaptor,
		MDA,
		CGA);
	VideoAdaptor adaptor = VideoAdaptor::CGA;

	ReflectableEnum(ModelApproximation,
		XT,
		TurboXT);
	ModelApproximation model = ModelApproximation::TurboXT;

	Target() : Analyser::Static::Target(Machine::PCCompatible) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		AnnounceEnum(VideoAdaptor);
		AnnounceEnum(ModelApproximation);
		DeclareField(adaptor);
		DeclareField(model);
	}
};

}

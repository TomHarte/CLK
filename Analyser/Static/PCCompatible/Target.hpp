//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser::Static::PCCompatible {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(VideoAdaptor,
		MDA,
		CGA);
	VideoAdaptor adaptor = VideoAdaptor::CGA;

	ReflectableEnum(Speed,
		ApproximatelyOriginal,
		Fast);
	Speed speed = Speed::Fast;

	Target() : Analyser::Static::Target(Machine::PCCompatible) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		AnnounceEnum(VideoAdaptor);
		AnnounceEnum(Speed);
		DeclareField(adaptor);
		DeclareField(speed);
	}
};

}

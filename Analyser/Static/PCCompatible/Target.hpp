//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_PCCompatible_Target_h
#define Analyser_Static_PCCompatible_Target_h

#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser::Static::PCCompatible {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(VideoAdaptor,
		MDA,
		CGA);
	VideoAdaptor adaptor = VideoAdaptor::MDA;

	Target() : Analyser::Static::Target(Machine::PCCompatible) {
		if(needs_declare()) {
			DeclareField(adaptor);
			AnnounceEnum(VideoAdaptor);
		}
	}
};

}

#endif /* Analyser_Static_PCCompatible_Target_h */
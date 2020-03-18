//
//  StandardOptions.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef StandardOptions_hpp
#define StandardOptions_hpp

#include "../Reflection/Enum.h"

namespace Configurable {

ReflectableEnum(Display,
	RGB,
	SVideo,
	CompositeColour,
	CompositeMonochrome
);

//===
// From here downward are a bunch of templates for individual option flags.
// Using them saves you marginally in syntax, but the primary gain is to
// ensure unified property naming.
//===

template <typename Owner> class DisplayOption {
	public:
		Configurable::Display output;
		DisplayOption(Configurable::Display output) : output(output) {}

	protected:
		void declare_display_option() {
			static_cast<Owner *>(this)->declare(&output, "output");
			AnnounceEnumNS(Configurable, Display);
		}
};

template <typename Owner> class QuickloadOption {
	public:
		bool quickload;
		QuickloadOption(bool quickload) : quickload(quickload) {}

	protected:
		void declare_quickload_option() {
			static_cast<Owner *>(this)->declare(&quickload, "quickload");
		}
};

}

#endif /* StandardOptions_hpp */

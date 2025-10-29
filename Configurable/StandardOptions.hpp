//
//  StandardOptions.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Reflection/Enum.hpp"

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

namespace Options {

template <typename Owner> class Display {
public:
	Configurable::Display output;
	Display(const Configurable::Display output) noexcept : output(output) {}

protected:
	void declare_display_option() {
		static_cast<Owner *>(this)->declare(&output, "output");
		AnnounceEnumNS(Configurable, Display);
	}
};

template <typename Owner> class QuickLoad {
public:
	bool quick_load;
	QuickLoad(const bool quick_load) noexcept : quick_load(quick_load) {}

protected:
	void declare_quickload_option() {
		static_cast<Owner *>(this)->declare(&quick_load, "quickload");
	}
};

template <typename Owner> class QuickBoot {
public:
	bool quick_boot;
	QuickBoot(const bool quick_boot) noexcept : quick_boot(quick_boot) {}

protected:
	void declare_quickboot_option() {
		static_cast<Owner *>(this)->declare(&quick_boot, "quickboot");
	}
};

template <typename Owner> class DynamicCrop {
public:
	bool dynamic_crop;
	DynamicCrop(const bool dynamic_crop) noexcept : dynamic_crop(dynamic_crop) {}

protected:
	void declare_quickboot_option() {
		static_cast<Owner *>(this)->declare(&dynamic_crop, "dynamiccrop");
	}
};

}
}

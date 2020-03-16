//
//  Configurable.h
//  Clock Signal
//
//  Created by Thomas Harte on 17/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Configurable_h
#define Configurable_h

#include "../Reflection/Struct.h"

#include <memory>

namespace Configurable {

/*!
	A Configurable::Device provides a reflective struct listing the available runtime options for this machine.
	It can provide it populated with 'accurate' options, 'user-friendly' options or just whatever the user
	currently has selected.

	'Accurate' options should correspond to the way that this device was usually used during its lifespan.
	E.g. a ColecoVision might accurately be given composite output.

	'User-friendly' options should be more like those that a user today might most expect from an emulator.
	E.g. the ColecoVision might bump itself up to S-Video output.
*/
struct Device {
	/// Sets the current options.
	virtual void set_options(const std::unique_ptr<Reflection::Struct> &options) = 0;

	enum class OptionsType {
		Current,
		Accurate,
		UserFriendly
	};

	/// @returns Options of type @c type.
	virtual std::unique_ptr<Reflection::Struct> get_options(OptionsType type) = 0;
};

}

#endif /* Configurable_h */

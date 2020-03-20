//
//  Configurable.h
//  Clock Signal
//
//  Created by Thomas Harte on 17/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Configurable_h
#define Configurable_h

#include "../Reflection/Struct.hpp"

#include <memory>

namespace Configurable {

/*!
	A Configurable::Device provides a reflective struct listing the available runtime options for this machine.
	You can ordinarily either get or set a machine's current options, or else construct a new instance of
	its options with one of the OptionsTypes defined below.
*/
struct Device {
	/// Sets the current options. The caller must ensure that the object passed in is either an instance of the machine's
	/// Options struct, or else was previously returned by get_options.
	virtual void set_options(const std::unique_ptr<Reflection::Struct> &options) = 0;

	/// @returns An options object
	virtual std::unique_ptr<Reflection::Struct> get_options() = 0;
};

/*!
	'Accurate' options should correspond to the way that this device was usually used during its lifespan.
	E.g. a ColecoVision might accurately be given composite output.

	'User-friendly' options should be more like those that a user today might most expect from an emulator.
	E.g. the ColecoVision might bump itself up to S-Video output.
*/
enum class OptionsType {
	Accurate,
	UserFriendly
};

}

#endif /* Configurable_h */

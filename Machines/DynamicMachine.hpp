//
//  DynamicMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef DynamicMachine_h
#define DynamicMachine_h

#include "../Configurable/Configurable.hpp"
#include "../Activity/Source.hpp"

#include "MachineTypes.hpp"

namespace Machine {

/*!
	Provides the structure for owning a machine and dynamically casting it as desired without knowledge of
	the machine's parent class or, therefore, the need to establish a common one.
*/
struct DynamicMachine {
	virtual ~DynamicMachine() {}

	virtual Activity::Source *activity_source() = 0;
	virtual Configurable::Device *configurable_device() = 0;
	virtual MachineTypes::TimedMachine *timed_machine() = 0;
	virtual MachineTypes::ScanProducer *scan_producer() = 0;
	virtual MachineTypes::AudioProducer *audio_producer() = 0;
	virtual MachineTypes::JoystickMachine *joystick_machine() = 0;
	virtual MachineTypes::KeyboardMachine *keyboard_machine() = 0;
	virtual MachineTypes::MouseMachine *mouse_machine() = 0;
	virtual MachineTypes::MediaTarget *media_target() = 0;

	/*!
		Provides a raw pointer to the underlying machine if and only if this dynamic machine really is
		only a single machine.

		Very unsafe. Very temporary.

		TODO: eliminate in favour of introspection for machine-specific inputs. This is here temporarily
		only to permit continuity of certain features in the Mac port that have not yet made their way
		to the SDL/console port.
	*/
	virtual void *raw_pointer() = 0;
};

/*!
	Provides a templateable means to access the above.
*/
template <typename MachineType> MachineType *get(DynamicMachine &);

#define SpecialisedGet(type, name)	\
template <>	\
inline type *get<type>(DynamicMachine &machine) {	\
	return machine.name();	\
}

SpecialisedGet(Activity::Source, activity_source)
SpecialisedGet(Configurable::Device, configurable_device)
SpecialisedGet(MachineTypes::TimedMachine, timed_machine)
SpecialisedGet(MachineTypes::ScanProducer, scan_producer)
SpecialisedGet(MachineTypes::AudioProducer, audio_producer)
SpecialisedGet(MachineTypes::JoystickMachine, joystick_machine)
SpecialisedGet(MachineTypes::KeyboardMachine, keyboard_machine)
SpecialisedGet(MachineTypes::MouseMachine, mouse_machine)
SpecialisedGet(MachineTypes::MediaTarget, media_target)

#undef SpecialisedGet

}

#endif /* DynamicMachine_h */

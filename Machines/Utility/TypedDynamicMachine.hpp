//
//  TypedDynamicMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef TypedDynamicMachine_h
#define TypedDynamicMachine_h

#include "MachineForTarget.hpp"

namespace Machine {

template<typename T> class TypedDynamicMachine: public ::Machine::DynamicMachine {
	public:
		TypedDynamicMachine(T *machine) : machine_(machine) {}
		T *get() { return machine_.get(); }

		TypedDynamicMachine() : TypedDynamicMachine(nullptr) {}
		TypedDynamicMachine(TypedDynamicMachine &&rhs) : machine_(std::move(rhs.machine_)) {}
		TypedDynamicMachine &operator=(TypedDynamicMachine &&rhs) {
			machine_ = std::move(rhs.machine_);
			return *this;
		}

#define Provide(type, name)	\
		type *name() final {	\
			return get<type>();	\
		}

		Provide(Activity::Source, activity_source)
		Provide(Configurable::Device, configurable_device)
		Provide(MachineTypes::TimedMachine, timed_machine)
		Provide(MachineTypes::ScanProducer, scan_producer)
		Provide(MachineTypes::AudioProducer, audio_producer)
		Provide(MachineTypes::JoystickMachine, joystick_machine)
		Provide(MachineTypes::KeyboardMachine, keyboard_machine)
		Provide(MachineTypes::MouseMachine, mouse_machine)
		Provide(MachineTypes::MediaTarget, media_target)

#undef Provide

		void *raw_pointer() final {
			return get();
		}

	private:
		template <typename Class> Class *get() {
			return dynamic_cast<Class *>(machine_.get());

			// Note to self: the below is not [currently] used
			// because in practice TypedDynamicMachine is instantiated
			// with an abstract parent of the actual class.
			//
			// TODO: rethink type hiding here. I think I've boxed myself
			// into an uncomfortable corner.
//			if constexpr (std::is_base_of_v<Class, T>) {
//				return static_cast<Class *>(machine_.get());
//			} else {
//				return nullptr;
//			}
		}
		std::unique_ptr<T> machine_;
};

}

#endif /* TypedDynamicMachine_h */

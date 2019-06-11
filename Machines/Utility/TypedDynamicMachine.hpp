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

		Activity::Source *activity_source() override {
			return get<Activity::Source>();
		}

		MediaTarget::Machine *media_target() override {
			return get<MediaTarget::Machine>();
		}

		CRTMachine::Machine *crt_machine() override {
			return get<CRTMachine::Machine>();
		}

		JoystickMachine::Machine *joystick_machine() override {
			return get<JoystickMachine::Machine>();
		}

		KeyboardMachine::Machine *keyboard_machine() override {
			return get<KeyboardMachine::Machine>();
		}

		MouseMachine::Machine *mouse_machine() override {
			return get<MouseMachine::Machine>();
		}

		Configurable::Device *configurable_device() override {
			return get<Configurable::Device>();
		}

		void *raw_pointer() override {
			return get();
		}

	private:
		template <typename Class> Class *get() {
			return dynamic_cast<Class *>(machine_.get());
		}
		std::unique_ptr<T> machine_;
};

}

#endif /* TypedDynamicMachine_h */

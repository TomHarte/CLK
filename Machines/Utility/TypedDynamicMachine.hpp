//
//  TypedDynamicMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
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

		ActivitySource::Machine *activity_source() override {
			return get<ActivitySource::Machine>();
		}

		ConfigurationTarget::Machine *configuration_target() override {
			return get<ConfigurationTarget::Machine>();
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

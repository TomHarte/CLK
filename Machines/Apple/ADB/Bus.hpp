//
//  Bus.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Bus_hpp
#define Bus_hpp

#include "../../../ClockReceiver/ClockReceiver.hpp"

#include <bitset>
#include <cstddef>
#include <ostream>
#include <vector>

namespace Apple {
namespace ADB {

struct Command {
	enum class Type {
		Reset,
		Flush,
		Reserved,
		/// The host wishes the device to store register contents.
		Listen,
		/// The host wishes the device to broadcast register contents.
		Talk
	};
	const Type type = Type::Reserved;
	const uint8_t device = 0xff;
	const uint8_t reg = 0xff;

	Command() {}
	Command(Type type) : type(type) {}
	Command(Type type, uint8_t device) : type(type), device(device) {}
	Command(Type type, uint8_t device, uint8_t reg) : type(type), device(device), reg(reg) {}
};

inline std::ostream &operator <<(std::ostream &stream, Command::Type type) {
	switch(type) {
		case Command::Type::Reset:	stream << "reset";		break;
		case Command::Type::Flush:	stream << "flush";		break;
		case Command::Type::Listen:	stream << "listen";		break;
		case Command::Type::Talk:	stream << "talk";		break;
		default: 					stream << "reserved";	break;
	}
	return stream;
}

inline std::ostream &operator <<(std::ostream &stream, Command command) {
	stream << "Command {";
	if(command.device != 0xff) stream << "device " << int(command.device) << ", ";
	if(command.reg != 0xff) stream << "register " << int(command.reg) << ", ";
	stream << command.type;
	stream << "}";
	return stream;
}

/*!
	@returns The @c Command encoded in @c code.
*/
inline Command decode_command(uint8_t code) {
	switch(code & 0x0f) {
		default: return Command();

		case 0: return Command(Command::Type::Reset);
		case 1: return Command(Command::Type::Flush, code >> 4);

		case 8: case 9: case 10: case 11:
		return Command(Command::Type::Listen, code >> 4, code & 3);

		case 12: case 13: case 14: case 15:
		return Command(Command::Type::Talk, code >> 4, code & 3);
	}
}

/*!
	The ADB bus models the data line of the ADB bus; it allows multiple devices to
	post their current data level, or read the current level, and also offers a tokenised
	version of all activity on the bus.

	In implementation terms, two types of device are envisaged:

		* proactive devices, which use @c add_device() and then merely @c set_device_output
		and @c get_state() as required, according to their own tracking of time; and

		* reactive devices, which use @c add_device(Device*) and then merely react to
		@c adb_bus_did_observe_event and @c advance_state in order to
		update @c set_device_output.
*/
class Bus {
	public:
		Bus(HalfCycles clock_speed);

		/*!
			Advances time; ADB is a clocked serial signal.
		*/
		void run_for(HalfCycles);

		/*!
			Adds a device to the bus, returning the index it should use
			to refer to itself in subsequent calls to set_device_output.
		*/
		size_t add_device();

		/*!
			Sets the current data line output for @c device.
		*/
		void set_device_output(size_t device_id, bool output);

		/*!
			@returns The current state of the ADB data line.
		*/
		bool get_state() const;

		enum class Event {
			Reset,
			Attention,
			Byte,
			ServiceRequest,

			Unrecognised
		};

		struct Device {
			/// Reports to an observer that @c event was observed in the activity
			/// observed on this bus. If this was a byte event, that byte's value is given as @c value.
			virtual void adb_bus_did_observe_event(Event event, uint8_t value = 0xff) = 0;

			/// Requests that the device update itself @c microseconds and, if necessary, post a
			/// new value ot @c set_device_output. This will be called only when the bus needs
			/// to reevaluate its current level. It cannot reliably be used to track the timing between
			/// observed events.
			virtual void advance_state(double microseconds, bool current_level) = 0;
		};
		/*!
			Adds a device.
		*/
		size_t add_device(Device *);

	private:
		HalfCycles time_in_state_;
		mutable HalfCycles time_since_get_state_;

		double half_cycles_to_microseconds_ = 1.0;
		std::vector<Device *> devices_;
		unsigned int shift_register_ = 0;
		bool data_level_ = true;

		// ADB addressing supports at most 16 devices but that doesn't include
		// the controller. So assume a maximum of 17 connected devices.
		std::bitset<17> bus_state_{0xffffffff};
		size_t next_device_id_ = 0;

		inline void shift(unsigned int);
};

}
}

#endif /* Bus_hpp */

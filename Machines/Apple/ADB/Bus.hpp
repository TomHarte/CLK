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
#include <vector>

namespace Apple {
namespace ADB {

/*!
	The ADB bus models the data line of the ADB bus; it allows multiple devices to
	post their current data level, or read the current level, and also offers a tokenised
	version of all activity on the bus.
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
		void set_device_output(size_t device, bool output);

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

		struct Observer {
			/// Reports to an observer that @c event was observed in the activity
			/// observed on this bus. If this was a byte event, that byte's value is given as @c value.
			virtual void adb_bus_did_observe_event(Bus *, Event event, uint8_t value = 0xff);
		};
		/*!
			Adds an observer.
		*/
		void add_observer(Observer *);

	private:
		HalfCycles time_in_state_;
		double half_cycles_to_microseconds_ = 1.0;
		std::vector<Observer *> observers_;
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

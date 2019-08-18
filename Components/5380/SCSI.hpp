//
//  SCSI.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef SCSI_hpp
#define SCSI_hpp

#include <limits>
#include <vector>

namespace SCSI {

typedef int BusState;

static const BusState DefaultBusState = 0;

/*!
	SCSI bus state is encoded entirely within an int.
	Bits correlate mostly but not exactly to the real SCSI bus.

	TODO: validate levels below. The bus uses open collector logic,
	so active low needs to be respected.
*/
enum Line: BusState {
	/// Provides the value currently on the data lines.
	Data 			= 0xff,
	/// Parity of the data lines.
	Parity		 	= 1 << 8,
	/// Set if the SEL line is currently selecting a target.
	/// Reset if it is selecting an initiator.
	SelectTarget	= 1 << 9,
	/// Set to indicate an attention condition. Reset otherwise.
	Attention		= 1 << 10,
	/// Set if control is on the bus. Reset if data is on the bus.
	Control			= 1 << 11,
	/// Set if the bus is busy. Reset otherwise.
	Busy			= 1 << 12,
	/// Set if acknowledging a data transfer request. Reset otherwise.
	Acknowledge		= 1 << 13,
	/// Set if a bus reset is being requested. Reset otherwise.
	Reset			= 1 << 14,
	/// Set if data is currently an input to the initiator. Reset if it is an output.
	Input			= 1 << 15,
	/// Set during the message phase. Reset otherwise.
	Message			= 1 << 16,
	/// Set if requesting a data transfer. Reset otherwise.
	Request			= 1 << 17,
};


class Bus {
	public:
		/*!
			Adds a device to the bus, returning the index it should use
			to refer to itself in subsequent calls to set_device_output.
		*/
		size_t add_device();

		/*!
			Sets the current output for @c device.
		*/
		void set_device_output(size_t device, BusState output);

		/*!
			@returns the current state of the bus.
		*/
		BusState get_state();

		struct Observer {
			virtual void scsi_bus_did_change(Bus *, BusState new_state) = 0;
		};
		/*!
			Adds an observer.
		*/
		void add_observer(Observer *);

	private:
		std::vector<BusState> device_states_;
		BusState state_ = DefaultBusState;
		std::vector<Observer *> observers_;
};

}

#endif /* SCSI_hpp */

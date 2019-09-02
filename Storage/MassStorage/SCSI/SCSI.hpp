//
//  SCSI.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef SCSI_hpp
#define SCSI_hpp

#include <array>
#include <limits>
#include <vector>

#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../ClockReceiver/ClockingHintSource.hpp"

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

#define us(x)	(x) / 1000000.0
#define ns(x)	(x) / 1000000000.0

/// The minimum amount of time that reset must be held for.
constexpr double ResetHoldTime		= us(25.0);

/// The minimum amount of time a SCSI device must wait after asserting ::Busy
/// until the data bus can be inspected to see whether arbitration has been won.
constexpr double ArbitrationDelay	= us(1.7);

/// The maximum amount of time a SCSI device can take from a detection that the
/// bus is free until it asserts ::Busy and its ID for the purposes of arbitration.
constexpr double BusSetDelay		= us(1.1);

/// The maximum amount of time a SCSI device is permitted to take to stop driving
/// all bus signals after: (i) the release of ::Busy ushering in a bus free phase;
/// or (ii) some other device has asserted ::Select during an arbitration phase.
constexpr double BusClearDelay		= ns(650.0);

/// The minimum amount of time to wait for the bus to settle after changing
/// "certain control signals". TODO: which?
constexpr double BusSettleDelay		= ns(450.0);

/// The minimum amount of time a SCSI must wait from detecting that the bus is free
/// and asserting ::Busy if starting an arbitration phase.
constexpr double BusFreeDelay		= ns(100.0);

/// The minimum amount of time required for deskew of "certain signals". TODO: which?
constexpr double DeskewDelay		= ns(45.0);

/// The maximum amount of time that propagation of a SCSI bus signal can take between
/// any two devices.
constexpr double CableSkew			= ns(10.0);

#undef ns
#undef us

class Bus: public ClockingHint::Source {
	public:
		Bus(HalfCycles clock_rate);

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
			/// Reports to an observer that the bus changed from a previous state to @c new_state,
			/// along with the time since that change was observed. The time is in seconds, and is
			/// intended for comparison with the various constants defined at namespace scope:
			/// ArbitrationDelay et al. Observers will be notified each time one of the thresholds
			/// defined by those constants is crossed.
			virtual void scsi_bus_did_change(Bus *, BusState new_state, double time_since_change) = 0;
		};
		/*!
			Adds an observer.
		*/
		void add_observer(Observer *);

		/*!
			SCSI buses don't have a clock. But devices on the bus are concerned with time-based factors,
			and `run_for` is the way that time propagates within this emulator. So please permit this
			fiction.
		*/
		void run_for(HalfCycles);

		/// As per ClockingHint::Source.
		ClockingHint::Preference preferred_clocking() final;

	private:
		HalfCycles time_in_state_;
		double cycles_to_time_ = 1.0;
		size_t dispatch_index_ = 0;
		std::array<int, 8> dispatch_times_;

		std::vector<BusState> device_states_;
		BusState state_ = DefaultBusState;
		std::vector<Observer *> observers_;
};

}

#endif /* SCSI_hpp */

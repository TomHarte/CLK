//
//  ncr5380.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef ncr5380_hpp
#define ncr5380_hpp

#include <cstdint>

#include "SCSI.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"


namespace NCR {
namespace NCR5380 {

/*!
	Models the NCR 5380, a SCSI interface chip.
*/
class NCR5380 {
	public:
		NCR5380();

		/*! Writes @c value to @c address.  */
		void write(int address, uint8_t value);

		/*! Reads from @c address. */
		uint8_t read(int address);

		/*!
			As per its design manual:

				"The NCR 5380 is a clockless device. Delays such as bus
				free delay, bus set delay and bus settle delay are
				implemented using gate delays."

			Therefore this fictitious implementation of an NCR5380 needs
			a way to track time even though the real one doesn't take in
			a clock. This is provided by `run_for`.

			Nevertheless, the clocking doesn't need to be very precise.
			Please provide a clock that is close to 200,000 Hz.
		*/
		void run_for(Cycles);

	private:
		SCSI::Bus bus_;
		size_t device_id_;

		SCSI::BusState bus_output_ = SCSI::DefaultBusState;
		uint8_t mode_ = 0xff;
		uint8_t initiator_command_ = 0xff;
		uint8_t data_bus_ = 0xff;
		bool test_mode_ = false;
		bool assert_data_bus_ = false;
};

}
}

#endif /* ncr5380_hpp */

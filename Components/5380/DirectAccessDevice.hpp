//
//  DirectAccessDevice.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef DirectAccessDevice_hpp
#define DirectAccessDevice_hpp

#include "SCSI.hpp"

namespace SCSI {

/*!
	Models a SCSI direct access device, ordinarily some sort of
	hard drive.
*/
class DirectAccessDevice: public Bus::Observer {
	public:
		/*!
			Instantiates a direct access device attached to @c bus,
			with SCSI ID @c scsi_id — a number in the range 0 to 7.
		*/
		DirectAccessDevice(Bus &bus, int scsi_id);

	private:
		void scsi_bus_did_change(Bus *, BusState new_state) final;

		Bus &bus_;
		const BusState scsi_id_mask_;
		const size_t scsi_bus_device_id_;

		enum class Phase {
			AwaitingSelection,
			Command
		} phase_ = Phase::AwaitingSelection;
		BusState bus_state_ = DefaultBusState;
};

}

#endif /* DirectAccessDevice_hpp */

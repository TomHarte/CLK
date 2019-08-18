//
//  DirectAccessDevice.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DirectAccessDevice.hpp"

using namespace SCSI;

DirectAccessDevice::DirectAccessDevice(Bus &bus, int scsi_id) :
	bus_(bus),
 	scsi_id_mask_(BusState(1 << scsi_id)),
 	scsi_bus_device_id_(bus.add_device()) {
	bus.add_observer(this);
}

void DirectAccessDevice::scsi_bus_did_change(Bus *, BusState new_state) {
	/*
		"The target determines that it is selected when the SEL# signal
		and its SCSI ID bit are active and the BSY# and I#/O signals
		are false. It then asserts the signal within a selection abort
		time."
	*/

	switch(state_) {
		case State::Inactive:
			if(
				(new_state & scsi_id_mask_) &&
				((new_state & (Line::SelectTarget | Line::Busy | Line::Input)) == Line::SelectTarget)
			) {
				state_ = State::Selected;
				bus_state_ |= Line::Busy | Line::Request;
				bus_.set_device_output(scsi_bus_device_id_, bus_state_);
			}
		break;

		case State::Selected:
			switch(new_state & (Line::Request | Line::Acknowledge)) {
				case Line::Request | Line::Acknowledge:
					bus_state_ &= ~Line::Request;
					printf("Got %02x maybe?\n", bus_state_ & 0xff);
				break;

				case Line::Acknowledge:
				case 0:
					bus_state_ |= Line::Request;
				break;

				default: break;
			}
			bus_.set_device_output(scsi_bus_device_id_, bus_state_);
		break;
	}
}

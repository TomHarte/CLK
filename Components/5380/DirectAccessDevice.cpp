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

	// A reset always takes precedence over anything else ongoing.
	if(new_state & Line::Reset) {
		phase_ = Phase::AwaitingSelection;
		bus_state_ = DefaultBusState;
		bus_.set_device_output(scsi_bus_device_id_, bus_state_);
		return;
	}

	switch(phase_) {
		case Phase::AwaitingSelection:
			if(
				(new_state & scsi_id_mask_) &&
				((new_state & (Line::SelectTarget | Line::Busy | Line::Input)) == Line::SelectTarget)
			) {
				printf("Selected\n");
				phase_ = Phase::Command;
				bus_state_ |= Line::Busy;	// Initiate the command phase: request a command byte.
				bus_.set_device_output(scsi_bus_device_id_, bus_state_);
			} else {
				if(!(new_state & scsi_id_mask_)) printf("No ID mask\n");
				else printf("Not SEL|~BSY|~IO");
			}
		break;

		case Phase::Command:
			// Wait for select to be disabled before beginning the control phase proper.
			if((new_state & Line::SelectTarget)) return;

			bus_state_ |= Line::Control;

			switch(new_state & (Line::Request | Line::Acknowledge)) {
				// If request and acknowledge are both enabled, grab a byte and cancel the request.
				case Line::Request | Line::Acknowledge:
					bus_state_ &= ~Line::Request;
					printf("Got %02x maybe?\n", new_state & 0xff);

					// TODO: is the command phase over?
				break;

				// The reset of request has caused the initiator to reset acknowledge, so it is now
				// safe to request the next byte.
				case 0:
					bus_state_ |= Line::Request;
				break;

				default: break;
			}
			bus_.set_device_output(scsi_bus_device_id_, bus_state_);
		break;
	}
}

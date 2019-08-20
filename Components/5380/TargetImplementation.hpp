//
//  TargetImplementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

template <typename Executor> Target<Executor>::Target(Bus &bus, int scsi_id) :
	bus_(bus),
	scsi_id_mask_(BusState(1 << scsi_id)),
	scsi_bus_device_id_(bus.add_device()) {
	bus.add_observer(this);
}

template <typename Executor> void Target<Executor>::scsi_bus_did_change(Bus *, BusState new_state) {
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

					if(command_.empty()) {
						begin_command(uint8_t(new_state));

						// TODO: if(command_.empty()) signal_error_somehow();
					} else {
						command_[command_pointer_] = uint8_t(new_state);
						++command_pointer_;
						if(command_pointer_ == command_.size()) {
							dispatch_command();

							// TODO: if(!dispatch_command()) signal_error_somehow();
						}
					}
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

template <typename Executor> void Target<Executor>::begin_command(uint8_t first_byte) {
	// The logic below is valid for SCSI-1. TODO: other SCSIs.
	switch(first_byte >> 5) {
		default: break;
		case 0:	command_.resize(6);		break;	// Group 0 commands: 6 bytes long.
		case 1:	command_.resize(10);	break;	// Group 1 commands: 10 bytes long.
		case 5:	command_.resize(12);	break;	// Group 5 commands: 12 bytes long.
	}

	// Store the first byte if it was recognised.
	if(!command_.empty()) {
		command_[0] = first_byte;
		command_pointer_ = 1;
	}
}

template <typename Executor> bool Target<Executor>::dispatch_command() {

	CommandArguments arguments(command_);

#define G0(x)	x
#define G1(x)	(0x20|x)
#define G5(x)	(0xa0|x)

	switch(command_[0]) {
		default:		return false;

		case G0(0x00):	return executor.test_unit_ready(arguments);
		case G0(0x01):	return executor.rezero_unit(arguments);
		case G0(0x03):	return executor.request_sense(arguments);
		case G0(0x04):	return executor.format_unit(arguments);
		case G0(0x08):	return executor.read(arguments);
		case G0(0x0a):	return executor.write(arguments);
		case G0(0x0b):	return executor.seek(arguments);
		case G0(0x16):	return executor.reserve_unit(arguments);
		case G0(0x17):	return executor.release_unit(arguments);
		case G0(0x1c):	return executor.read_diagnostic(arguments);
		case G0(0x1d):	return executor.write_diagnostic(arguments);
		case G0(0x12):	return executor.inquiry(arguments);

		case G1(0x05):	return executor.read_capacity(arguments);
		case G1(0x08):	return executor.read(arguments);
		case G1(0x0a):	return executor.write(arguments);
		case G1(0x0e):	return executor.write_and_verify(arguments);
		case G1(0x0f):	return executor.verify(arguments);
		case G1(0x11):	return executor.search_data_equal(arguments);
		case G1(0x10):	return executor.search_data_high(arguments);
		case G1(0x12):	return executor.search_data_low(arguments);

		case G5(0x09):	return executor.set_block_limits(arguments);
	}

#undef G0
#undef G1
#undef G5

	return false;
}

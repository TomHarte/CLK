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
				phase_ = Phase::Command;
				bus_state_ |= Line::Busy;	// Initiate the command phase: request a command byte.
				bus_.set_device_output(scsi_bus_device_id_, bus_state_);
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

		case Phase::ReceivingData:
			switch(new_state & (Line::Request | Line::Acknowledge)) {
				case Line::Request | Line::Acknowledge:
					bus_state_ &= ~Line::Request;

					data_[data_pointer_] = uint8_t(new_state);
					++data_pointer_;
					if(data_pointer_ == data_.size()) {
						next_function_(CommandState(command_), *this);
					}
				break;

				case 0:
					bus_state_ |= Line::Request;
				break;
			}
			bus_.set_device_output(scsi_bus_device_id_, bus_state_);
		break;

		case Phase::SendingData:
			switch(new_state & (Line::Request | Line::Acknowledge)) {
				case Line::Request | Line::Acknowledge:
					bus_state_ &= ~(Line::Request | 0xff);

					++data_pointer_;
					if(data_pointer_ == data_.size()) {
						next_function_(CommandState(command_), *this);
					}
				break;

				case 0:
					bus_state_ |= Line::Request;
					bus_state_ = (bus_state_ & ~0xff) | data_[data_pointer_];
				break;
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

	CommandState arguments(command_);

#define G0(x)	x
#define G1(x)	(0x20|x)
#define G5(x)	(0xa0|x)

	switch(command_[0]) {
		default:		return false;

		case G0(0x00):	return executor.test_unit_ready(arguments, *this);
		case G0(0x01):	return executor.rezero_unit(arguments, *this);
		case G0(0x03):	return executor.request_sense(arguments, *this);
		case G0(0x04):	return executor.format_unit(arguments, *this);
		case G0(0x08):	return executor.read(arguments, *this);
		case G0(0x0a):	return executor.write(arguments, *this);
		case G0(0x0b):	return executor.seek(arguments, *this);
		case G0(0x16):	return executor.reserve_unit(arguments, *this);
		case G0(0x17):	return executor.release_unit(arguments, *this);
		case G0(0x1c):	return executor.read_diagnostic(arguments, *this);
		case G0(0x1d):	return executor.write_diagnostic(arguments, *this);
		case G0(0x12):	return executor.inquiry(arguments, *this);

		case G1(0x05):	return executor.read_capacity(arguments, *this);
		case G1(0x08):	return executor.read(arguments, *this);
		case G1(0x0a):	return executor.write(arguments, *this);
		case G1(0x0e):	return executor.write_and_verify(arguments, *this);
		case G1(0x0f):	return executor.verify(arguments, *this);
		case G1(0x11):	return executor.search_data_equal(arguments, *this);
		case G1(0x10):	return executor.search_data_high(arguments, *this);
		case G1(0x12):	return executor.search_data_low(arguments, *this);

		case G5(0x09):	return executor.set_block_limits(arguments, *this);
	}

#undef G0
#undef G1
#undef G5

	return false;
}

template <typename Executor> void Target<Executor>::send_data(std::vector<uint8_t> &&data, continuation next) {
	// Data out phase: control and message all reset, input set.
	bus_state_ &= ~(Line::Control | Line::Input | Line::Message);
	bus_state_ |= Line::Input;
	phase_ = Phase::SendingData;
	data_ = std::move(data);
	data_pointer_ = 0;
	bus_.set_device_output(scsi_bus_device_id_, bus_state_);
}

template <typename Executor> void Target<Executor>::receive_data(size_t length, continuation next) {
	// Data out phase: control, input and message all reset.
	bus_state_ &= ~(Line::Control | Line::Input | Line::Message);
	phase_ = Phase::ReceivingData;
	data_.resize(length);
	data_pointer_ = 0;
	bus_.set_device_output(scsi_bus_device_id_, bus_state_);
}

template <typename Executor> void Target<Executor>::send_status(Status, continuation next) {
	// Status phase: message reset, control and input set.
	bus_state_ &= ~(Line::Control | Line::Input | Line::Message);
	bus_state_ |= Line::Input | Line::Control;
	phase_ = Phase::SendingStatus;
	bus_.set_device_output(scsi_bus_device_id_, bus_state_);
}

template <typename Executor> void Target<Executor>::send_message(Message, continuation next) {
	// Message out phase: message and control set, input reset.
	bus_state_ &= ~(Line::Control | Line::Input | Line::Message);
	bus_state_ |= Line::Message | Line::Control;
	phase_ = Phase::SendingMessage;
	bus_.set_device_output(scsi_bus_device_id_, bus_state_);
}

template <typename Executor> void Target<Executor>::end_command() {
	// TODO: was this a linked command?

	// Release all bus lines and return to awaiting selection.
	phase_ = Phase::AwaitingSelection;
	bus_state_ = DefaultBusState;
	bus_.set_device_output(scsi_bus_device_id_, bus_state_);
}

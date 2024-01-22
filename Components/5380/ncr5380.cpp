//
//  ncr5380.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "ncr5380.hpp"

#include "../../Outputs/Log.hpp"

namespace {
Log::Logger<Log::Source::NCR5380> logger;
}
// TODO:
//
//	end_of_dma_ should be set if: /EOP && /DACK && (/RD || /WR); for at least 100ns.


using namespace NCR::NCR5380;
using SCSI::Line;

NCR5380::NCR5380(SCSI::Bus &bus, int clock_rate) :
	bus_(bus),
	clock_rate_(clock_rate) {
	device_id_ = bus_.add_device();
	bus_.add_observer(this);

	// TODO: use clock rate and expected phase. This implementation currently
	// provides only CPU-driven polling behaviour.
	(void)clock_rate_;
	(void)expected_phase_;
}

void NCR5380::write(int address, uint8_t value, bool) {
	switch(address & 7) {
		case 0:
			logger.info().append("[0] Set current SCSI bus state to %02x", value);

			data_bus_ = value;
			if(dma_request_ && dma_operation_ == DMAOperation::Send) {
				dma_acknowledge(value);
			}
		break;

		case 1: {
			logger.info().append("[1] Initiator command register set: %02x", value);
			initiator_command_ = value;

			bus_output_ &= ~(Line::Reset | Line::Acknowledge | Line::Busy | Line::SelectTarget | Line::Attention);
			if(value & 0x80) bus_output_ |= Line::Reset;
			if(value & 0x08) bus_output_ |= Line::Busy;
			if(value & 0x04) bus_output_ |= Line::SelectTarget;

			/* bit 5 = differential enable if this were a 5381 */

			test_mode_ = value & 0x40;
			assert_data_bus_ = value & 0x01;
			update_control_output();
		} break;

		case 2:
			logger.info().append("[2] Set mode: %02x", value);
			mode_ = value;

			// bit 7: 1 = use block mode DMA mode (if DMA mode is also enabled)
			// bit 6: 1 = be a SCSI target; 0 = be an initiator
			// bit 5: 1 = check parity
			// bit 4: 1 = generate an interrupt if parity checking is enabled and an error is found
			// bit 3: 1 = generate an interrupt when an EOP is received from the DMA controller
			// bit 2: 1 = generate an interrupt and reset low 6 bits of register 1 if an unexpected loss of Line::Busy occurs
			// bit 1: 1 = use DMA mode
			// bit 0: 1 = begin arbitration mode (device ID should be in register 0)
			arbitration_in_progress_ = false;
			phase_mismatch_ = false;
			switch(mode_ & 0x3) {
				case 0x0:
					bus_output_ &= ~SCSI::Line::Busy;
					dma_request_ = false;
					set_execution_state(ExecutionState::None);
				break;

				case 0x1:
					arbitration_in_progress_ = true;
					set_execution_state(ExecutionState::WaitingForBusy);
					lost_arbitration_ = false;
				break;

				default:
					assert_data_bus_ = false;
					set_execution_state(ExecutionState::PerformingDMA);
					bus_.update_observers();
				break;
			}

			// "[The End of DMA Transfer] bit is reset when the DMA MODE bit
			// is reset (0) in the Mode Register".
			end_of_dma_ &= bool(value & 0x2);

			update_control_output();
		break;

		case 3: {
			logger.info().append("[3] Set target command: %02x", value);
			target_command_ = value;
			update_control_output();
		} break;

		case 4:
			logger.info().append("[4] Set select enabled: %02x", value);
		break;

		case 5:
			logger.info().append("[5] Start DMA send: %02x", value);
			dma_operation_ = DMAOperation::Send;
		break;

		case 6:
			logger.info().append("[6] Start DMA target receive: %02x", value);
			dma_operation_ = DMAOperation::TargetReceive;
		break;

		case 7:
			logger.info().append("[7] Start DMA initiator receive: %02x", value);
			dma_operation_ = DMAOperation::InitiatorReceive;
		break;
	}

	// Data is output only if the data bus is asserted.
	if(assert_data_bus_) {
		bus_output_ = (bus_output_ & ~SCSI::Line::Data) | data_bus_;
	} else {
		bus_output_ &= ~SCSI::Line::Data;
	}

	// In test mode, still nothing is output. Otherwise throw out
	// the current value of bus_output_.
	if(test_mode_) {
		bus_.set_device_output(device_id_, SCSI::DefaultBusState);
	} else {
		bus_.set_device_output(device_id_, bus_output_);
	}
}

uint8_t NCR5380::read(int address, bool) {
	switch(address & 7) {
		case 0:
			logger.info().append("[0] Get current SCSI bus state: %02x", (bus_.get_state() & 0xff));

			if(dma_request_ && dma_operation_ == DMAOperation::InitiatorReceive) {
				return dma_acknowledge();
			}
		return uint8_t(bus_.get_state());

		case 1:
			logger.info().append("[1] Initiator command register get: %c%c", arbitration_in_progress_ ? 'p' : '-', lost_arbitration_ ? 'l' : '-');
		return
			// Bits repeated as they were set.
			(initiator_command_ & ~0x60) |

			// Arbitration in progress.
			(arbitration_in_progress_ ? 0x40 : 0x00) |

			// Lost arbitration.
			(lost_arbitration_ ? 0x20 : 0x00);

		case 2:
			logger.info().append("[2] Get mode");
		return mode_;

		case 3:
			logger.info().append("[3] Get target command");
		return target_command_;

		case 4: {
			const auto bus_state = bus_.get_state();
			const uint8_t result =
				((bus_state & Line::Reset)			? 0x80 : 0x00) |
				((bus_state & Line::Busy)			? 0x40 : 0x00) |
				((bus_state & Line::Request)		? 0x20 : 0x00) |
				((bus_state & Line::Message)		? 0x10 : 0x00) |
				((bus_state & Line::Control)		? 0x08 : 0x00) |
				((bus_state & Line::Input)			? 0x04 : 0x00) |
				((bus_state & Line::SelectTarget)	? 0x02 : 0x00) |
				((bus_state & Line::Parity)			? 0x01 : 0x00);
			logger.info().append("[4] Get current bus state: %02x", result);
			return result;
		}

		case 5: {
			const auto bus_state = bus_.get_state();
			const uint8_t result =
				(end_of_dma_ ? 0x80 : 0x00) |
				((dma_request_ && state_ == ExecutionState::PerformingDMA) ? 0x40 : 0x00)	|
				/* b5 = parity error */
				(irq_ ? 0x10 : 0x00) |
				(phase_matches() ? 0x08 : 0x00)	|
				/* b2 = busy error */
				((bus_state & Line::Attention) ? 0x02 : 0x00) |
				((bus_state & Line::Acknowledge) ? 0x01 : 0x00);
			logger.info().append("[5] Get bus and status: %02x", result);
			return result;
		}

		case 6:
			logger.info().append("[6] Get input data");
		return 0xff;

		case 7:
			logger.info().append("[7] Reset parity/interrupt");
			irq_ = false;
		return 0xff;
	}
	return 0;
}

SCSI::BusState NCR5380::target_output() const {
	SCSI::BusState output = SCSI::DefaultBusState;
	if(target_command_ & 0x08) output |= Line::Request;
	if(target_command_ & 0x04) output |= Line::Message;
	if(target_command_ & 0x02) output |= Line::Control;
	if(target_command_ & 0x01) output |= Line::Input;
	return output;
}

void NCR5380::update_control_output() {
	bus_output_ &= ~(Line::Request | Line::Message | Line::Control | Line::Input | Line::Acknowledge | Line::Attention);
	if(mode_ & 0x40) {
		// This is a target; C/D, I/O, /MSG and /REQ are signalled on the bus.
		bus_output_ |= target_output();
	} else {
		// This is an initiator; /ATN and /ACK are signalled on the bus.
		if(
			(initiator_command_ & 0x10) ||
			(state_ == ExecutionState::PerformingDMA && dma_acknowledge_)
		) bus_output_ |= Line::Acknowledge;
		if(initiator_command_ & 0x02) bus_output_ |= Line::Attention;
	}
}

void NCR5380::scsi_bus_did_change(SCSI::Bus *, SCSI::BusState new_state, double time_since_change) {
	/*
		When connected as an Initiator with DMA Mode True,
		if the phase lines I//O, C//D, and /MSG do not match the
		phase bits in the Target Command Register, a phase mismatch
		interrupt is generated when /REQ goes active.
	*/
	if((mode_ & 0x42) == 0x02 && new_state & SCSI::Line::Request && !phase_matches()) {
		irq_ = true;
		phase_mismatch_ = true;
	}

	switch(state_) {
		default: break;

		/*
			Official documentation:

				Arbitration is accomplished using a bus-free filter to continuously monitor BSY.
				If BSY remains inactive for at least 400 nsec then the SCSI bus is considered free
				and arbitration may begin. Arbitration will begin if the bus is free, SEL is inactive
				and the ARBITRATION bit (port 2, bit 0) is active. Once arbitration has begun
				(BSY asserted), an arbitration delay of 2.2 /Lsec must elapse before the data bus
				can be examined to deter- mine if arbitration has been won. This delay must be
				implemented in the controlling software driver.

			Personal notes:

				I'm discounting that "arbitratation is accomplished" opening, and assuming that what needs
				to happen is:

					(i) wait for BSY to be inactive;
					(ii) count 400 nsec;
					(iii) check that BSY and SEL are inactive.
		*/

		case ExecutionState::WaitingForBusy:
			if(!(new_state & SCSI::Line::Busy) || time_since_change < SCSI::DeskewDelay) return;
			state_ = ExecutionState::WatchingBusy;
			[[fallthrough]];

		case ExecutionState::WatchingBusy:
			if(!(new_state & SCSI::Line::Busy)) {
				lost_arbitration_ = true;
				set_execution_state(ExecutionState::None);
			}

			// Check for having hit 400ns (more or less) since BSY was inactive.
			if(time_since_change >= SCSI::BusSettleDelay) {
//				arbitration_in_progress_ = false;
				if(new_state & SCSI::Line::SelectTarget) {
					lost_arbitration_ = true;
					set_execution_state(ExecutionState::None);
				} else {
					bus_output_ &= ~SCSI::Line::Busy;
					set_execution_state(ExecutionState::None);
				}
			}

			/* TODO: there's a bug here, given that the dropping of Busy isn't communicated onward. */
		break;

		case ExecutionState::PerformingDMA:
			if(time_since_change < SCSI::DeskewDelay) return;

			// Signal a DMA request if the request line is active, i.e. meaningful data is
			// on the bus, and this device hasn't yet acknowledged it.
			switch(new_state & (SCSI::Line::Request | SCSI::Line::Acknowledge)) {
				case 0:
					dma_request_ = false;
				break;
				case SCSI::Line::Request:
					// Don't issue a new DMA request if a phase mismatch has
					// been detected and this is an intiator receiving.
					// This is a bit of reading between the lines.
					// (i.e. guesswork, partly)
					dma_request_ =
						!phase_mismatch_ ||
						(dma_operation_ != DMAOperation::InitiatorReceive);
				break;
				case SCSI::Line::Request | SCSI::Line::Acknowledge:
					dma_request_ = false;
				break;
				case SCSI::Line::Acknowledge:
					dma_acknowledge_ = false;
					dma_request_ = false;
					update_control_output();
					bus_.set_device_output(device_id_, bus_output_);
				break;
			}
		break;
	}
}

void NCR5380::set_execution_state(ExecutionState state) {
	state_ = state;
	if(state != ExecutionState::PerformingDMA) dma_operation_ = DMAOperation::Ready;
}

size_t NCR5380::scsi_id() {
	return device_id_;
}

bool NCR5380::dma_request() {
	return dma_request_;
}

uint8_t NCR5380::dma_acknowledge() {
	const uint8_t bus_state = uint8_t(bus_.get_state());

	dma_acknowledge_ = true;
	dma_request_ = false;
	update_control_output();
	bus_.set_device_output(device_id_, bus_output_);

	return bus_state;
}

void NCR5380::dma_acknowledge(uint8_t value) {
	data_bus_ = value;

	dma_acknowledge_ = true;
	dma_request_ = false;
	update_control_output();
	bus_.set_device_output(device_id_, bus_output_);
}

bool NCR5380::phase_matches() const {
	const auto bus_state = bus_.get_state();
	return
		(target_output() & (Line::Message | Line::Control | Line::Input)) ==
		(bus_state & (Line::Message | Line::Control | Line::Input));
}

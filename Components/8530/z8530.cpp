//
//  8530.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "z8530.hpp"

#include "../../Outputs/Log.hpp"

using namespace Zilog::SCC;

void z8530::reset() {
	// TODO.
}

bool z8530::get_interrupt_line() const {
	return
		(master_interrupt_control_ & 0x8) &&
		(
			channels_[0].get_interrupt_line() ||
			channels_[1].get_interrupt_line()
		);
}

std::uint8_t z8530::read(int address) {
	if(address & 2) {
		// Read data register for channel
		return 0x00;
	} else {
		// Read control register for channel.
		uint8_t result = 0;

		switch(pointer_) {
			default:
				result = channels_[address & 1].read(address & 2, pointer_);
			break;

			case 2:		// Handled non-symmetrically between channels.
				if(address & 1) {
					LOG("[SCC] Unimplemented: register 2 status bits");
				} else {
					result = interrupt_vector_;

					// Modify the vector if permitted.
//					if(master_interrupt_control_ & 1) {
						for(int port = 0; port < 2; ++port) {
							// TODO: the logic below assumes that DCD is the only implemented interrupt. Fix.
							if(channels_[port].get_interrupt_line()) {
								const uint8_t shift = 1 + 3*((master_interrupt_control_ & 0x10) >> 4);
								const uint8_t mask = uint8_t(~(7 << shift));
								result = uint8_t(
									(result & mask) |
									((1 | ((port == 1) ? 4 : 0)) << shift)
								);
								break;
							}
						}
//					}
				}
			break;
		}

		pointer_ = 0;
		update_delegate();
		return result;
	}

	return 0x00;
}

void z8530::write(int address, std::uint8_t value) {
	if(address & 2) {
		// Write data register for channel.
	} else {
		// Write control register for channel.

		// Most registers are per channel, but a couple are shared; sever
		// them here.
		switch(pointer_) {
			default:
				channels_[address & 1].write(address & 2, pointer_, value);
			break;

			case 2:	// Interrupt vector register; shared between both channels.
				interrupt_vector_ = value;
				LOG("[SCC] Interrupt vector set to " << PADHEX(2) << int(value));
			break;

			case 9:	// Master interrupt and reset register; also shared between both channels.
				LOG("[SCC] Master interrupt and reset register: " << PADHEX(2) << int(value));
				master_interrupt_control_ = value;
			break;
		}

		// The pointer number resets to 0 after every access, but if it is zero
		// then crib at least the next set of pointer bits (which, similarly, are shared
		// between the two channels).
		if(pointer_) {
			pointer_ = 0;
		} else {
			// The lowest three bits are the lowest three bits of the pointer.
			pointer_ = value & 7;

			// If the command part of the byte is a 'point high', also set the
			// top bit of the pointer.
			if(((value >> 3)&7) == 1) {
				pointer_ |= 8;
			}
		}
	}
	update_delegate();
}

void z8530::set_dcd(int port, bool level) {
	channels_[port].set_dcd(level);
	update_delegate();
}

// MARK: - Channel implementations

uint8_t z8530::Channel::read(bool data, uint8_t pointer) {
	// If this is a data read, just return it.
	if(data) {
		return data_;
	} else {
		// Otherwise, this is a control read...
		switch(pointer) {
			default:
				LOG("[SCC] Unrecognised control read from register " << int(pointer));
			return 0x00;

			case 0:
			return dcd_ ? 0x8 : 0x0;

			case 0xf:
			return external_interrupt_status_;
		}
	}

	return 0x00;
}

void z8530::Channel::write(bool data, uint8_t pointer, uint8_t value) {
	if(data) {
		data_ = value;
		return;
	} else {
		switch(pointer) {
			default:
				LOG("[SCC] Unrecognised control write: " << PADHEX(2) << int(value) << " to register " << int(pointer));
			break;

			case 0x0:	// Write register 0 — CRC reset and other functions.
				// Decode CRC reset instructions.
				switch(value >> 6) {
					default:	/* Do nothing. */		break;
					case 1:
						LOG("[SCC] TODO: reset Rx CRC checker.");
					break;
					case 2:
						LOG("[SCC] TODO: reset Tx CRC checker.");
					break;
					case 3:
						LOG("[SCC] TODO: reset Tx underrun/EOM latch.");
					break;
				}

				// Decode command code.
				switch((value >> 3)&7) {
					default:	/* Do nothing. */		break;
					case 2:
//						LOG("[SCC] reset ext/status interrupts.");
						external_status_interrupt_ = false;
						external_interrupt_status_ = 0;
					break;
					case 3:
						LOG("[SCC] TODO: send abort (SDLC).");
					break;
					case 4:
						LOG("[SCC] TODO: enable interrupt on next Rx character.");
					break;
					case 5:
						LOG("[SCC] TODO: reset Tx interrupt pending.");
					break;
					case 6:
						LOG("[SCC] TODO: reset error.");
					break;
					case 7:
						LOG("[SCC] TODO: reset highest IUS.");
					break;
				}
			break;

			case 0x1:	// Write register 1 — Transmit/Receive Interrupt and Data Transfer Mode Definition.
				interrupt_mask_ = value;
			break;

			case 0x4:	// Write register 4 — Transmit/Receive Miscellaneous Parameters and Modes.
				// Bits 0 and 1 select parity mode.
				if(!(value&1)) {
					parity_ = Parity::Off;
				} else {
					parity_ = (value&2) ? Parity::Even : Parity::Odd;
				}

				// Bits 2 and 3 select stop bits.
				switch((value >> 2)&3) {
					default:	stop_bits_ = StopBits::Synchronous;			break;
					case 1:		stop_bits_ = StopBits::OneBit;				break;
					case 2:		stop_bits_ = StopBits::OneAndAHalfBits;		break;
					case 3:		stop_bits_ = StopBits::TwoBits;				break;
				}

				// Bits 4 and 5 pick a sync mode.
				switch((value >> 4)&3) {
					default:	sync_mode_ = Sync::Monosync;	break;
					case 1:		sync_mode_ = Sync::Bisync;		break;
					case 2:		sync_mode_ = Sync::SDLC;		break;
					case 3:		sync_mode_ = Sync::External;	break;
				}

				// Bits 6 and 7 select a clock rate multiplier, unless synchronous
				// mode is enabled (and this is ignored if sync mode is external).
				if(stop_bits_ == StopBits::Synchronous) {
					clock_rate_multiplier_ = 1;
				} else {
					switch((value >> 6)&3) {
						default:	clock_rate_multiplier_ = 1;		break;
						case 1:		clock_rate_multiplier_ = 16;	break;
						case 2:		clock_rate_multiplier_ = 32;	break;
						case 3:		clock_rate_multiplier_ = 64;	break;
					}
				}
			break;

			case 0xf:	// Write register 15 — External/Status Interrupt Control.
				external_interrupt_mask_ = value;
			break;
		}
	}
}

void z8530::Channel::set_dcd(bool level) {
	if(dcd_ == level) return;
	dcd_ = level;

	if(external_interrupt_mask_ & 0x8) {
		external_status_interrupt_ = true;
		external_interrupt_status_ |= 0x8;
	}
}

bool z8530::Channel::get_interrupt_line() const {
	return
		(interrupt_mask_ & 1) && external_status_interrupt_;
	// TODO: other potential causes of an interrupt.
}

void z8530::update_delegate() {
	const bool interrupt_line = get_interrupt_line();
	if(interrupt_line != previous_interrupt_line_) {
		previous_interrupt_line_ = interrupt_line;
		if(delegate_) delegate_->did_change_interrupt_status(this, interrupt_line);
	}
}

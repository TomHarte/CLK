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

bool z8530::get_interrupt_line() {
	// TODO.
	return false;
}

std::uint8_t z8530::read(int address) {
	const auto result = channels_[address & 1].read(address & 2, pointer_);
	pointer_ = 0;
	return result;
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
				LOG("[SCC] TODO: master interrupt and reset register " << PADHEX(2) << int(value));
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
}

uint8_t z8530::Channel::read(bool data, uint8_t pointer) {
	// If this is a data read, just return it.
	if(data) {
		return data_;
	} else {
		LOG("[SCC] Unrecognised control read from register " << int(pointer));
	}

	// Otherwise, this is a control read...
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
						LOG("[SCC] TODO: reset ext/status interrupts.");
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
				transfer_interrupt_mask_ = value;
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
				interrupt_mask_ = value;
			break;
		}
	}
}

void z8530::set_dcd(int port, bool level) {
	// TODO.
}

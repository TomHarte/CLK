//
//  8530.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "z8530.hpp"

#define LOG_PREFIX "[SCC] "
#include "../../Outputs/Log.hpp"

using namespace Zilog::SCC;

void z8530::reset() {
	// TODO.
}

bool z8530::get_interrupt_line() {
	return
		(master_interrupt_control_ & 0x8) &&
		(
			channels_[0].get_interrupt_line() ||
			channels_[1].get_interrupt_line()
		);
}

/*
	Per the standard defined in the header file, this implementation follows
	an addressing convention of:

	A0 = A/B		(i.e. channel select)
	A1 = C/D		(i.e. control or data)
*/

std::uint8_t z8530::read(int address) {
	if(address & 2) {
		// Read data register for channel.
		return channels_[address & 1].read(true, pointer_);
	} else {
		// Read control register for channel.
		uint8_t result = 0;

		switch(pointer_) {
			default:
				result = channels_[address & 1].read(false, pointer_);
			break;

			case 2:		// Handled non-symmetrically between channels.
				if(address & 1) {
					LOG("Unimplemented: register 2 status bits");
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

		// Cf. the two-step control register selection process in ::write. Since this
		// definitely wasn't a *write* to register 0, it follows that the next selected
		// control register will be 0.
		pointer_ = 0;

		update_delegate();
		return result;
	}

	return 0x00;
}

void z8530::write(int address, std::uint8_t value) {
	if(address & 2) {
		// Write data register for channel. This is completely independent
		// of whatever is going on over in the control realm.
		channels_[address & 1].write(true, pointer_, value);
	} else {
		// Write control register for channel; there's a two-step sequence
		// here for the programmer. Initially the selected register
		// (i.e. `pointer_`) is zero. That register includes a field to
		// set the next selected register. After any other register has
		// been written to, register 0 is selected again.

		// Most registers are per channel, but a couple are shared;
		// sever them here, send the rest to the appropriate chnanel.
		switch(pointer_) {
			default:
				channels_[address & 1].write(false, pointer_, value);
			break;

			case 2:	// Interrupt vector register; used only by Channel B.
					// So there's only one of these.
				interrupt_vector_ = value;
				LOG("Interrupt vector set to " << PADHEX(2) << int(value));
			break;

			case 9:	// Master interrupt and reset register; there is also only one of these.
				LOG("Master interrupt and reset register: " << PADHEX(2) << int(value));
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
			// top bit of the pointer. Channels themselves therefore need not
			// (/should not) respond to the point high command.
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
		LOG("Control read from register " << int(pointer));
		// Otherwise, this is a control read...
		switch(pointer) {
			default:
			return 0x00;

			case 0x0:	// Read Register 0; see p.37 (PDF p.45).
				// b0: Rx character available.
				// b1: zero count.
				// b2: Tx buffer empty.
				// b3: DCD.
				// b4: sync/hunt.
				// b5: CTS.
				// b6: Tx underrun/EOM.
				// b7: break/abort.
			return dcd_ ? 0x8 : 0x0;

			case 0x1:	// Read Register 1; see p.37 (PDF p.45).
				// b0: all sent.
				// b1: residue code 0.
				// b2: residue code 1.
				// b3: residue code 2.
				// b4: parity error.
				// b5: Rx overrun error.
				// b6: CRC/framing error.
				// b7: end of frame (SDLC).
			return 0x01;

			case 0x2:	// Read Register 2; see p.37 (PDF p.45).
				// Interrupt vector — modified by status information in B channel.
			return 0x00;

			case 0x3:	// Read Register 3; see p.37 (PDF p.45).
				// B channel: all bits are 0.
				// A channel:
				// b0: Channel B ext/status IP.
				// b1: Channel B Tx IP.
				// b2: Channel B Rx IP.
				// b3: Channel A ext/status IP.
				// b4: Channel A Tx IP.
				// b5: Channel A Rx IP.
				// b6, b7: 0.
			return 0x00;

			case 0xa:	// Read Register 10; see p.37 (PDF p.45).
				// b0: 0
				// b1: On loop.
				// b2: 0
				// b3: 0
				// b4: Loop sending.
				// b5: 0
				// b6: Two clocks missing.
				// b7: One clock missing.
			return 0x00;

			case 0xc:	// Read Register 12; see p.37 (PDF p.45).
				// Lower byte of time constant.
			return 0x00;

			case 0xd:	// Read Register 13; see p.38 (PDF p.46).
				// Upper byte of time constant.
			return 0x00;

			case 0xf:	// Read Register 15; see p.38 (PDF p.46).
				// External interrupt status:
				// b0: 0
				// b1: Zero count.
				// b2: 0
				// b3: DCD.
				// b4: Sync/hunt.
				// b5: CTS.
				// b6: Tx underrun/EOM.
				// b7: Break/abort.
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
		LOG("Control write: " << PADHEX(2) << int(value) << " to register " << int(pointer));
		switch(pointer) {
			default:
				LOG("Unrecognised control write: " << PADHEX(2) << int(value) << " to register " << int(pointer));
			break;

			case 0x0:	// Write register 0 — CRC reset and other functions.
				// Decode CRC reset instructions.
				switch(value >> 6) {
					default:	/* Do nothing. */		break;
					case 1:
						LOG("TODO: reset Rx CRC checker.");
					break;
					case 2:
						LOG("TODO: reset Tx CRC checker.");
					break;
					case 3:
						LOG("TODO: reset Tx underrun/EOM latch.");
					break;
				}

				// Decode command code.
				switch((value >> 3)&7) {
					default:	/* Do nothing. */		break;
					case 2:
//						LOG("reset ext/status interrupts.");
						external_status_interrupt_ = false;
						external_interrupt_status_ = 0;
					break;
					case 3:
						LOG("TODO: send abort (SDLC).");
					break;
					case 4:
						LOG("TODO: enable interrupt on next Rx character.");
					break;
					case 5:
						LOG("TODO: reset Tx interrupt pending.");
					break;
					case 6:
						LOG("TODO: reset error.");
					break;
					case 7:
						LOG("TODO: reset highest IUS.");
					break;
				}
			break;

			case 0x1:	// Write register 1 — Transmit/Receive Interrupt and Data Transfer Mode Definition.
				interrupt_mask_ = value;

				/*
					b7 = 0 => Wait/Request output is inactive; 1 => output is informative.
					b6 = Wait/request output is for...
						0 => wait: floating when inactive, low if CPU is attempting to transfer data the SCC isn't yet ready for.
						1 => request: high if inactive, low if SCC is ready to transfer data.
					b5 = 1 => wait/request is relative to read buffer; 0 => relative to write buffer.

					b4/b3:
						00 = disable receive interrupt
						01 = interrupt on first character or special condition
						10 = interrupt on all characters and special conditions
						11 = interrupt only upon special conditions.

					b2 = 1 => parity error is a special condition; 0 => it isn't.
					b1 = 1 => transmit buffer empty interrupt is enabled; 0 => it isn't.
					b0 = 1 => external interrupt is enabled; 0 => it isn't.
				*/
				LOG("Interrupt mask: " << PADHEX(2) << int(value));
			break;

			case 0x2:	// Write register 2 - interrupt vector.
			break;

			case 0x3: {	// Write register 3 — Receive Parameters and Control.
				// Get bit count.
				int receive_bit_count = 8;
				switch(value >> 6) {
					default:	receive_bit_count = 5;	break;
					case 1:		receive_bit_count = 7;	break;
					case 2:		receive_bit_count = 6;	break;
					case 3:		receive_bit_count = 8;	break;
				}
				LOG("Receive bit count: " << receive_bit_count);

				/*
					b7,b6:
						00 = 5 receive bits per character
						01 = 7 bits
						10 = 6 bits
						11 = 8 bits

					b5 = 1 => DCD and CTS outputs are set automatically; 0 => they're inputs to read register 0.
								(DCD is ignored in local loopback; CTS is ignored in both auto echo and local loopback).
					b4: enter hunt mode (if set to 1, presumably?)
					b3 = 1 => enable receiver CRC generation; 0 => don't.
					b2: address search mode (SDLC)
					b1: sync character load inhibit.
					b0: Rx enable.
				*/
			} break;

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

			case 0x5:
				// b7: DTR
				// b6/b5:
				//	00 = Tx 5 bits (or less) per character
				//	01 = Tx 7 bits per character
				//	10 = Tx 6 bits per character
				//	11 = Tx 8 bits per character
				// b4: send break.
				// b3: Tx enable.
				// b2: SDLC (if 0) / CRC-16 (if 1)
				// b1: RTS
				// b0: Tx CRC enable.
			break;

			case 0x6:
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

bool z8530::Channel::get_interrupt_line() {
	return
		(interrupt_mask_ & 1) && external_status_interrupt_;
	// TODO: other potential causes of an interrupt.
}

/*!
	Evaluates the new level of the interrupt line and notifies the delegate if
	both: (i) there is one; and (ii) the interrupt line has changed since last
	the delegate was notified.
*/
void z8530::update_delegate() {
	const bool interrupt_line = get_interrupt_line();
	if(interrupt_line != previous_interrupt_line_) {
		previous_interrupt_line_ = interrupt_line;
		if(delegate_) delegate_->did_change_interrupt_status(this, interrupt_line);
	}
}

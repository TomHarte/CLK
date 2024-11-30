//
//  I2C.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "I2C.hpp"

#include "../../Outputs/Log.hpp"

using namespace I2C;

namespace {

Log::Logger<Log::Source::I2C> logger;

}

void Bus::set_data(const bool pulled) {
	set_clock_data(clock_, pulled);
}
bool Bus::data() const {
	bool result = data_;
	if(peripheral_bits_) {
		result |= !(peripheral_response_ & 0x80);
	}
	return result;
}

void Bus::set_clock(const bool pulled) {
	set_clock_data(pulled, data_);
}
bool Bus::clock() const {
	return clock_;
}

void Bus::set_clock_data(const bool clock_pulled, const bool data_pulled) {
	// Proceed only if changes are evidenced.
	if(clock_pulled == clock_ && data_pulled == data_) {
		return;
	}

	const bool prior_clock = clock_;
	const bool prior_data = data_;
	clock_ = clock_pulled;
	data_ = data_pulled;

	// If currently serialising from a peripheral then shift onwards on
	// every clock trailing edge.
	if(peripheral_bits_) {
		// Trailing edge of clock => bit has been consumed.
		if(!prior_clock && clock_) {
			logger.info().append("<< %d", (peripheral_response_ >> 7) & 1);
			--peripheral_bits_;
			peripheral_response_ <<= 1;

			if(!peripheral_bits_) {
				signal(Event::FinishedOutput);
			}
		}
		return;
	}

	// Not currently serialising implies listening.
	if(!clock_ && prior_data != data_) {
		// A data transition outside of a clock cycle implies a start or stop.
		in_bit_ = false;
		if(data_) {
			logger.info().append("S");
			signal(Event::Start);
		} else {
			logger.info().append("W");
			signal(Event::Stop);
		}
	} else if(clock_ != prior_clock) {
		// Bits: wait until the falling edge of the cycle.
		if(!clock_) {
			// Rising edge: clock period begins.
			in_bit_ = true;
		} else if(in_bit_) {
			// Falling edge: clock period ends (assuming it began; otherwise this is a preparatory
			// clock transition only, immediately after a start bit).
			in_bit_ = false;

			if(data_) {
				logger.info().append("0");
				signal(Event::Zero);
			} else {
				logger.info().append("1");
				signal(Event::One);
			}
		}
	}
}

void Bus::signal(const Event event) {
	const auto capture_bit = [&]() {
		input_ = uint16_t((input_ << 1) | (event == Event::Zero ? 0 : 1));
		++input_count_;
	};

	const auto acknowledge = [&]() {
		// Post an acknowledgement bit.
		peripheral_response_ = 0;
		peripheral_bits_ = 1;
	};

	const auto set_state = [&](State state) {
		state_ = state;
		input_count_ = 0;
		input_ = 0;
	};

	const auto enqueue = [&](std::optional<uint8_t> next) {
		if(next) {
			peripheral_response_ = static_cast<uint16_t>(*next);
			peripheral_bits_ = 8;
			set_state(State::AwaitingByteAcknowledge);
		} else {
			set_state(State::AwaitingAddress);
		}
	};

	const auto stop = [&]() {
		set_state(State::AwaitingAddress);
		active_peripheral_ = nullptr;
	};

	// Allow start and stop conditions at any time.
	if(event == Event::Start) {
		set_state(State::CollectingAddress);
		active_peripheral_ = nullptr;
		return;
	}

	if(event == Event::Stop) {
		if(active_peripheral_) {
			active_peripheral_->stop();
		}
		stop();
		return;
	}

	switch(state_) {
		// While waiting for an address, don't respond to anything other than a
		// start bit, which is actually dealt with above.
		case State::AwaitingAddress:	break;

		// To collect an address: shift in eight bits, and if there's a device
		// at that address then acknowledge the address and segue into a read
		// or write loop.
		case State::CollectingAddress:
			capture_bit();
			if(input_count_ == 8) {
				auto pair = peripherals_.find(uint8_t(input_) & 0xfe);
				if(pair != peripherals_.end()) {
					active_peripheral_ = pair->second;
					active_peripheral_->start(input_ & 1);

					if(input_&1) {
						acknowledge();
						set_state(State::CompletingReadAcknowledge);
					} else {
						acknowledge();
						set_state(State::ReceivingByte);
					}
				} else {
					state_ = State::AwaitingAddress;
				}
			}
		break;

		// Receiving byte: wait until a scheduled acknowledgment has
		// happened, then collect eight bits, then see whether the
		// active peripheral will accept them. If so, acknowledge and repeat.
		// Otherwise fall silent.
		case State::ReceivingByte:
			if(event == Event::FinishedOutput) {
				return;
			}
			capture_bit();
			if(input_count_ == 8) {
				if(active_peripheral_->write(static_cast<uint8_t>(input_))) {
					acknowledge();
					set_state(State::ReceivingByte);
				} else {
					stop();
				}
			}
		break;

		// The initial state immediately after a peripheral has been started
		// in read mode and the address-select acknowledgement is still
		// being serialised.
		//
		// Once that is completed, enqueues the first byte from the peripheral.
		case State::CompletingReadAcknowledge:
			if(event != Event::FinishedOutput) {
				break;
			}
			enqueue(active_peripheral_->read());
		break;

		// Repeating state during reading; waits until the previous byte has
		// been fully serialised, and if the host acknowledged it then posts
		// the next. If the host didn't acknowledge, stops the connection.
		case State::AwaitingByteAcknowledge:
			if(event == Event::FinishedOutput) {
				break;
			}
			if(event != Event::Zero) {
				stop();
				break;
			}

			// Add a new byte if there is one.
			enqueue(active_peripheral_->read());
		break;
	}
}

void Bus::add_peripheral(Peripheral *const peripheral, const int address) {
	peripherals_[address] = peripheral;
}

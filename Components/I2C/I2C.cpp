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

void Bus::set_data(bool pulled) {
	set_clock_data(clock_, pulled);
}
bool Bus::data() {
	bool result = data_;
	if(peripheral_bits_) {
		result |= !(peripheral_response_ & 0x200);
	}
	return result;
}

void Bus::set_clock(bool pulled) {
	set_clock_data(pulled, data_);
}
bool Bus::clock() {
	return clock_;
}

void Bus::set_clock_data(bool clock_pulled, bool data_pulled) {
	if(clock_pulled == clock_ && data_pulled == data_) {
		return;
	}

	const bool prior_data = data_;
	clock_ = clock_pulled;
	data_ = data_pulled;

	if(clock_) {
		return;
	}

	if(prior_data != data_) {
		if(data_) {
//			logger.info().append("S");
			signal(Event::Start);
		} else {
//			logger.info().append("P");
			signal(Event::Stop);
		}
	} else {
		if(peripheral_bits_) {
			--peripheral_bits_;
			peripheral_response_ <<= 1;
		}

		if(data_) {
//			logger.info().append("0");
			signal(Event::Zero);
		} else {
//			logger.info().append("1");
			signal(Event::One);
		}
	}
}

void Bus::signal(Event event) {
	const auto capture_bit = [&]() {
		input_ = uint16_t((input_ << 1) | (event == Event::Zero ? 0 : 1));
		++input_count_;
	};

	const auto acknowledge = [&]() {
		// Post an acknowledgement bit.
		peripheral_response_ = 0;
		peripheral_bits_ = 2;
	};

	const auto set_state = [&](State state) {
		state_ = state;
		input_count_ = 0;
		input_ = 0;
	};

	const auto enqueue = [&](std::optional<uint8_t> next) {
		if(next) {
			peripheral_response_ = *next;
			peripheral_bits_ = 9;
			set_state(State::PostingByte);
		} else {
			acknowledge();
			set_state(State::AwaitingAddress);
		}
	};

	// Allow start and stop conditions at any time.
	if(event == Event::Start) {
		set_state(State::CollectingAddress);
		active_peripheral_ = nullptr;
		return;
	}

	if(event == Event::Stop) {
		set_state(State::AwaitingAddress);
		if(active_peripheral_) {
			active_peripheral_->stop();
		}
		active_peripheral_ = nullptr;
		return;
	}

	switch(state_) {
		case State::AwaitingAddress:	break;

		case State::CollectingAddress:
			capture_bit();

			if(input_count_ == 8) {
				auto pair = peripherals_.find(uint8_t(input_) & 0xfe);
				if(pair != peripherals_.end()) {
					active_peripheral_ = pair->second;
					active_peripheral_->start(input_ & 1);

					if(input_&1) {
						enqueue(active_peripheral_->read());
					} else {
						acknowledge();
						set_state(State::ReceivingByte);
					}
				} else {
					state_ = State::AwaitingAddress;
				}
			}
		break;

		case State::ReceivingByte:
			// Run down the clock on the acknowledge bit.
			if(peripheral_bits_) {
				return;
			}

			capture_bit();
			if(input_count_ == 8) {
				active_peripheral_->write(static_cast<uint8_t>(input_));
				acknowledge();
				set_state(State::ReceivingByte);
			}
		break;

		case State::PostingByte:
			// Finish whatever is enqueued.
			if(peripheral_bits_) {
				return;
			}

			enqueue(active_peripheral_->read());
		break;
	}
}

void Bus::add_peripheral(Peripheral *peripheral, int address) {
	peripherals_[address] = peripheral;
}

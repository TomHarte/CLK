//
//  SerialPort.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Line.hpp"

using namespace Serial;

void Line::set_writer_clock_rate(HalfCycles clock_rate) {
	clock_rate_ = clock_rate;
}

void Line::advance_writer(HalfCycles cycles) {
	if(cycles == HalfCycles(0)) return;

	const auto integral_cycles = cycles.as_integral();
	remaining_delays_ = std::max(remaining_delays_ - integral_cycles, Cycles::IntType(0));
	if(events_.empty()) {
		write_cycles_since_delegate_call_ += integral_cycles;
		if(transmission_extra_) {
			transmission_extra_ -= integral_cycles;
			if(transmission_extra_ <= 0) {
				transmission_extra_ = 0;
				update_delegate(level_);
			}
		}
	} else {
		while(!events_.empty()) {
			if(events_.front().delay <= integral_cycles) {
				cycles -= events_.front().delay;
				write_cycles_since_delegate_call_ += events_.front().delay;
				const auto old_level = level_;

				auto iterator = events_.begin() + 1;
				while(iterator != events_.end() && iterator->type != Event::Delay) {
					level_ = iterator->type == Event::SetHigh;
					++iterator;
				}
				events_.erase(events_.begin(), iterator);

				if(old_level != level_) {
					update_delegate(old_level);
				}

				// Book enough extra time for the read delegate to be posted
				// the final bit if one is attached.
				if(events_.empty()) {
					transmission_extra_ = minimum_write_cycles_for_read_delegate_bit();
				}
			} else {
				events_.front().delay -= integral_cycles;
				write_cycles_since_delegate_call_ += integral_cycles;
				break;
			}
		}
	}
}

void Line::write(bool level) {
	if(!events_.empty()) {
		events_.emplace_back();
		events_.back().type = level ? Event::SetHigh : Event::SetLow;
	} else {
		level_ = level;
		transmission_extra_ = minimum_write_cycles_for_read_delegate_bit();
	}
}

void Line::write(HalfCycles cycles, int count, int levels) {
	remaining_delays_ += count * cycles.as_integral();

	auto event = events_.size();
	events_.resize(events_.size() + size_t(count)*2);
	while(count--) {
		events_[event].type = Event::Delay;
		events_[event].delay = int(cycles.as_integral());
		events_[event+1].type = (levels&1) ? Event::SetHigh : Event::SetLow;
		levels >>= 1;
		event += 2;
	}
}

void Line::reset_writing() {
	remaining_delays_ = 0;
	events_.clear();
}

bool Line::read() const {
	return level_;
}

void Line::set_read_delegate(ReadDelegate *delegate, Storage::Time bit_length) {
	read_delegate_ = delegate;
	read_delegate_bit_length_ = bit_length;
	read_delegate_bit_length_.simplify();
	write_cycles_since_delegate_call_ = 0;
}

void Line::update_delegate(bool level) {
	// Exit early if there's no delegate, or if the delegate is waiting for
	// zero and this isn't zero.
	if(!read_delegate_) return;

	const int cycles_to_forward = write_cycles_since_delegate_call_;
	write_cycles_since_delegate_call_ = 0;
	if(level && read_delegate_phase_ == ReadDelegatePhase::WaitingForZero) return;

	// Deal with a transition out of waiting-for-zero mode by seeding time left
	// in bit at half a bit.
	if(read_delegate_phase_ == ReadDelegatePhase::WaitingForZero) {
		time_left_in_bit_ = read_delegate_bit_length_;
		time_left_in_bit_.clock_rate <<= 1;
		read_delegate_phase_ = ReadDelegatePhase::Serialising;
	}

	// Forward as many bits as occur.
	Storage::Time time_left(cycles_to_forward, int(clock_rate_.as_integral()));
	const int bit = level ? 1 : 0;
	int bits = 0;
	while(time_left >= time_left_in_bit_) {
		++bits;
		if(!read_delegate_->serial_line_did_produce_bit(this, bit)) {
			read_delegate_phase_ = ReadDelegatePhase::WaitingForZero;
			if(bit) return;
		}

		time_left -= time_left_in_bit_;
		time_left_in_bit_ = read_delegate_bit_length_;
	}
	time_left_in_bit_ -= time_left;
}

Cycles::IntType Line::minimum_write_cycles_for_read_delegate_bit() {
	if(!read_delegate_) return 0;
	return 1 + (read_delegate_bit_length_ * unsigned(clock_rate_.as_integral())).get<int>();
}

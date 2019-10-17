//
//  SerialPort.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "SerialPort.hpp"

using namespace Serial;

void Line::set_writer_clock_rate(int clock_rate) {
	clock_rate_ = clock_rate;
}

void Line::advance_writer(int cycles) {
	remaining_delays_ = std::max(remaining_delays_ - cycles, 0);
	while(!events_.empty()) {
		if(events_.front().delay < cycles) {
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
				if(read_delegate_) {
					read_delegate_->serial_line_did_change_output(this, Storage::Time(write_cycles_since_delegate_call_, clock_rate_), level_);
					write_cycles_since_delegate_call_ = 0;
				}
			}
		} else {
			events_.front().delay -= cycles;
			write_cycles_since_delegate_call_ += cycles;
			break;
		}
	}
}

void Line::write(bool level) {
	if(!events_.empty()) {
		events_.emplace_back();
		events_.back().type = level ? Event::SetHigh : Event::SetLow;
	} else {
		level_ = level;
	}
}

void Line::write(int cycles, int count, int levels) {
	remaining_delays_ += count*cycles;

	auto event = events_.size();
	events_.resize(events_.size() + size_t(count)*2);
	while(count--) {
		events_[event].type = Event::Delay;
		events_[event].delay = cycles;
		events_[event+1].type = (levels&1) ? Event::SetHigh : Event::SetLow;
		levels >>= 1;
		event += 2;
	}
}

int Line::write_data_time_remaining() {
	return remaining_delays_;
}

void Line::reset_writing() {
	remaining_delays_ = 0;
	events_.clear();
}

void Line::flush_writing() {
	remaining_delays_ = 0;
	for(const auto &event : events_) {
		bool new_level = level_;
		switch(event.type) {
			default: break;
			case Event::SetHigh:	new_level = true;	break;
			case Event::SetLow:		new_level = false;	break;
			case Event::Delay:
				write_cycles_since_delegate_call_ += event.delay;
			continue;
		}

		if(new_level != level_) {
			level_ = new_level;
			if(read_delegate_) {
				read_delegate_->serial_line_did_change_output(this, Storage::Time(write_cycles_since_delegate_call_, clock_rate_), level_);
				write_cycles_since_delegate_call_ = 0;
			}
		}
	}
	events_.clear();
}

bool Line::read() {
	return level_;
}

void Line::set_read_delegate(ReadDelegate *delegate) {
	read_delegate_ = delegate;
	write_cycles_since_delegate_call_ = 0;
}

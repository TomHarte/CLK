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
			auto iterator = events_.begin() + 1;
			while(iterator != events_.end() && iterator->type != Event::Delay) {
				level_ = iterator->type == Event::SetHigh;
				++iterator;
			}
			events_.erase(events_.begin(), iterator);
		} else {
			events_.front().delay -= cycles;
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
		event += 2;
	}
}

int Line::write_data_time_remaining() {
	return remaining_delays_;
}

void Line::reset_writing() {
	events_.clear();
}

void Line::flush_writing() {
	for(const auto &event : events_) {
		switch(event.type) {
			default: break;
			case Event::SetHigh:	level_ = true;	break;
			case Event::SetLow:		level_ = false;	break;
		}
	}
	events_.clear();
}

bool Line::read() {
	return level_;
}

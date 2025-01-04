//
//  SerialPort.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "SerialBus.hpp"

#include <cstdio>
#include <iostream>

using namespace Commodore::Serial;

const char *::Commodore::Serial::to_string(Line line) {
	switch(line) {
		case Line::ServiceRequest: return "Service request";
		case Line::Attention: return "Attention";
		case Line::Clock: return "Clock";
		case Line::Data: return "Data";
		case Line::Reset: return "Reset";
		default: return nullptr;
	}
}

const char *::Commodore::Serial::to_string(LineLevel level) {
	switch(level) {
		case LineLevel::High : return "high";
		case LineLevel::Low : return "low";
		default: return nullptr;
	}
}

void ::Commodore::Serial::attach(Port &port, Bus &bus) {
	port.set_bus(bus);
	bus.add_port(port);
}

void Bus::add_port(Port &port) {
	ports_.push_back(&port);
	for(int line = int(Line::ServiceRequest); line <= int(Line::Reset); line++) {
		// The addition of a new device may change the line output...
		set_line_output_did_change(Line(line));

		// ... but the new device will need to be told the current state regardless.
		port.set_input(Line(line), line_levels_[line]);
	}
}

void Bus::set_line_output_did_change(Line line) {
	// Treat lines as open collector.
	auto new_line_level = LineLevel::High;
	for(auto port : ports_) {
		new_line_level = LineLevel(bool(new_line_level) & bool(port->get_output(line)));
	}

	// Post an update only if one occurred.
	const auto index = size_t(line);
	if(new_line_level != line_levels_[index]) {
		line_levels_[index] = new_line_level;

		for(auto port : ports_) {
			port->set_input(line, new_line_level);
		}
	}
}

// MARK: - The debug port

void DebugPort::set_input(Line line, LineLevel value) {
	const auto index = size_t(line);
	input_levels_[index] = value;

	std::cout << "[Bus] " << to_string(line) << " is " << to_string(value);
	if(!incoming_count_) {
		incoming_count_ = (!input_levels_[size_t(Line::Clock)] && !input_levels_[size_t(Line::Data)]) ? 8 : 0;
	} else {
		if(line == Line::Clock && value) {
			incoming_byte_ = (incoming_byte_ >> 1) | (input_levels_[size_t(Line::Data)] ? 0x80 : 0x00);
		}
		incoming_count_--;
		if(incoming_count_ == 0) std::cout << "[Bus] Observed value " << std::hex << int(incoming_byte_);
	}
}

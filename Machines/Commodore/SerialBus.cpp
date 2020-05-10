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

const char *::Commodore::Serial::StringForLine(Line line) {
	switch(line) {
		case ServiceRequest: return "Service request";
		case Attention: return "Attention";
		case Clock: return "Clock";
		case Data: return "Data";
		case Reset: return "Reset";
		default: return nullptr;
	}
}

void ::Commodore::Serial::AttachPortAndBus(std::shared_ptr<Port> port, std::shared_ptr<Bus> bus) {
	port->set_serial_bus(bus);
	bus->add_port(port);
}

void Bus::add_port(std::shared_ptr<Port> port) {
	ports_.push_back(port);
	for(int line = int(ServiceRequest); line <= int(Reset); line++) {
		// the addition of a new device may change the line output...
		set_line_output_did_change(Line(line));

		// ... but the new device will need to be told the current state regardless
		port->set_input(Line(line), line_levels_[line]);
	}
}

void Bus::set_line_output_did_change(Line line) {
	// i.e. I believe these lines to be open collector
	LineLevel new_line_level = High;
	for(std::weak_ptr<Port> port : ports_) {
		std::shared_ptr<Port> locked_port = port.lock();
		if(locked_port) {
			new_line_level = (LineLevel)(bool(new_line_level) & bool(locked_port->get_output(line)));
		}
	}

	// post an update only if one occurred
	if(new_line_level != line_levels_[line]) {
		line_levels_[line] = new_line_level;

		for(std::weak_ptr<Port> port : ports_) {
			std::shared_ptr<Port> locked_port = port.lock();
			if(locked_port) {
				locked_port->set_input(line, new_line_level);
			}
		}
	}
}

// MARK: - The debug port

void DebugPort::set_input(Line line, LineLevel value) {
	input_levels_[line] = value;

	std::cout << "[Bus] " << StringForLine(line) << " is " << (value ? "high" : "low");
	if(!incoming_count_) {
		incoming_count_ = (!input_levels_[Line::Clock] && !input_levels_[Line::Data]) ? 8 : 0;
	} else {
		if(line == Line::Clock && value) {
			incoming_byte_ = (incoming_byte_ >> 1) | (input_levels_[Line::Data] ? 0x80 : 0x00);
		}
		incoming_count_--;
		if(incoming_count_ == 0) std::cout << "[Bus] Observed value " << std::hex << int(incoming_byte_);
	}
}

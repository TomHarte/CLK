//
//  SerialPort.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "SerialBus.hpp"

using namespace Commodore::Serial;

const char *::Commodore::Serial::StringForLine(Line line)
{
	switch(line)
	{
		case ServiceRequest: return "Service request";
		case Attention: return "Attention";
		case Clock: return "Clock";
		case Data: return "Data";
		case Reset: return "Reset";
	}
}

void ::Commodore::Serial::AttachPortAndBus(std::shared_ptr<Port> port, std::shared_ptr<Bus> bus)
{
	port->set_serial_bus(bus);
	bus->add_port(port);
}

void Bus::add_port(std::shared_ptr<Port> port)
{
	_ports.push_back(port);
	for(int line = (int)ServiceRequest; line <= (int)Reset; line++)
	{
		// the addition of a new device may change the line output...
		set_line_output_did_change((Line)line);

		// ... but the new device will need to be told the current state regardless
		port->set_input((Line)line, _line_levels[line]);
	}
}

void Bus::set_line_output_did_change(Line line)
{
	// i.e. I believe these lines to be open collector
	LineLevel new_line_level = High;
	for(std::weak_ptr<Port> port : _ports)
	{
		std::shared_ptr<Port> locked_port = port.lock();
		if(locked_port)
		{
			new_line_level = (LineLevel)((bool)new_line_level & (bool)locked_port->get_output(line));
		}
	}

	// post an update only if one occurred
	if(new_line_level != _line_levels[line])
	{
		_line_levels[line] = new_line_level;

		for(std::weak_ptr<Port> port : _ports)
		{
			std::shared_ptr<Port> locked_port = port.lock();
			if(locked_port)
			{
				locked_port->set_input(line, new_line_level);
			}
		}
	}
}

#pragma mark - The debug port

void DebugPort::set_input(Line line, LineLevel value)
{
	_input_levels[line] = value;

	printf("[Bus] %s is %s\n", StringForLine(line), value ? "high" : "low");
	if(!_incoming_count)
	{
		_incoming_count = (!_input_levels[Line::Clock] && !_input_levels[Line::Data]) ? 8 : 0;
	}
	else
	{
		if(line == Line::Clock && value)
		{
			_incoming_byte = (_incoming_byte >> 1) | (_input_levels[Line::Data] ? 0x80 : 0x00);
		}
		_incoming_count--;
		if(_incoming_count == 0) printf("[Bus] Observed %02x\n", _incoming_byte);
	}
}

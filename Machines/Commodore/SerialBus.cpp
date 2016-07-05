//
//  SerialPort.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "SerialBus.hpp"

using namespace Commodore::Serial;

void Bus::add_port(std::shared_ptr<Port> port)
{
	_ports.push_back(port);
}

void Bus::set_line_output_did_change(Line line)
{
	bool new_line_value = false;
	for(std::weak_ptr<Port> port : _ports)
	{
		std::shared_ptr<Port> locked_port = port.lock();
		if(locked_port)
		{
			new_line_value |= locked_port->get_output(line);
		}
	}

	if(new_line_value != _line_values[line])
	{
		_line_values[line] = new_line_value;

		for(std::weak_ptr<Port> port : _ports)
		{
			std::shared_ptr<Port> locked_port = port.lock();
			if(locked_port)
			{
				locked_port->set_input(line, new_line_value);
			}
		}
	}
}

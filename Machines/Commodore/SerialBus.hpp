//
//  SerialPort.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef SerialBus_hpp
#define SerialBus_hpp

#include <vector>

namespace Commodore {
namespace Serial {

	enum Line {
		ServiceRequest = 0,
		Attention,
		Clock,
		Data,
		Reset
	};

	enum LineLevel: bool {
		High = true,
		Low = false
	};

	class Port;
	class Bus;

	/*!
		Returns a C string giving a human-readable name for the supplied line.
	*/
	const char *StringForLine(Line line);

	/*!
		Calls both of the necessary methods to (i) set @c bus as the target for @c port outputs; and
		(ii) add @c port as one of the targets to which @c bus propagates line changes.
	*/
	void AttachPortAndBus(std::shared_ptr<Port> port, std::shared_ptr<Bus> bus);

	/*!
		A serial bus is responsible for retaining a weakly-held collection of attached ports and for deciding the
		current bus levels based upon the net result of each port's output, and for communicating changes in bus
		levels to every port.
	*/
	class Bus {
		public:
			Bus() : _line_levels{High, High, High, High, High} {}

			/*!
				Adds the supplied port to the bus.
			*/
			void add_port(std::shared_ptr<Port> port);

			/*!
				Communicates to the bus that one of its attached port has changed its output level for the given line.
				The bus will therefore recalculate bus state and propagate as necessary.
			*/
			void set_line_output_did_change(Line line);

		private:
			LineLevel _line_levels[5];
			std::vector<std::weak_ptr<Port>> _ports;
	};

	/*!
		A serial port is an endpoint on a serial bus; this class provides a direct setter for current line outputs and
		expects to be subclassed in order for specific port-housing devices to deal with input.
	*/
	class Port {
		public:
			Port() : _line_levels{High, High, High, High, High} {}

			/*!
				Sets the current level of an output line on this serial port.
			*/
			void set_output(Line line, LineLevel level) {
				if(_line_levels[line] != level)
				{
					_line_levels[line] = level;
					std::shared_ptr<Bus> bus = _serial_bus.lock();
					if(bus) bus->set_line_output_did_change(line);
				}
			}

			/*!
				Gets the previously set level of an output line.
			*/
			LineLevel get_output(Line line) {
				return _line_levels[line];
			}

			/*!
				Called by the bus to signal a change in any input line level. Subclasses should implement this.
			*/
			virtual void set_input(Line line, LineLevel value) = 0;

			/*!
				Sets the supplied serial bus as that to which line levels will be communicated.
			*/
			inline void set_serial_bus(std::shared_ptr<Bus> serial_bus) {
				_serial_bus = serial_bus;
			}

		private:
			std::weak_ptr<Bus> _serial_bus;
			LineLevel _line_levels[5];
	};

	/*!
		A debugging port, which makes some attempt to log bus activity. Incomplete. TODO: complete.
	*/
	class DebugPort: public Port {
		public:
			void set_input(Line line, LineLevel value);

			DebugPort() : _incoming_count(0) {}

		private:
			uint8_t _incoming_byte;
			int _incoming_count;
			LineLevel _input_levels[5];
	};

}
}

#endif /* SerialPort_hpp */

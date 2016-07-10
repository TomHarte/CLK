//
//  SerialPort.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef SerialBus_hpp
#define SerialBus_hpp

#import <vector>

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

	const char *StringForLine(Line line);

	class Port;

	class Bus {
		public:
			Bus() : _line_levels{High, High, High, High, High} {}

			void add_port(std::shared_ptr<Port> port);
			void set_line_output_did_change(Line line);

		private:
			LineLevel _line_levels[5];
			std::vector<std::weak_ptr<Port>> _ports;
	};

	class Port {
		public:
			Port() : _line_levels{High, High, High, High, High} {}

			void set_output(Line line, LineLevel level) {
				if(_line_levels[line] != level)
				{
					_line_levels[line] = level;
					std::shared_ptr<Bus> bus = _serial_bus.lock();
					if(bus) bus->set_line_output_did_change(line);
				}
			}

			LineLevel get_output(Line line) {
				return _line_levels[line];
			}

			virtual void set_input(Line line, LineLevel value) = 0;

			inline void set_serial_bus(std::shared_ptr<Bus> serial_bus) {
				_serial_bus = serial_bus;
			}

		private:
			std::weak_ptr<Bus> _serial_bus;
			LineLevel _line_levels[5];
	};

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

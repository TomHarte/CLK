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

	const char *StringForLine(Line line);

	class Port;

	class Bus {
		public:
			Bus() : _line_values{false, false, false, false, false} {}

			void add_port(std::shared_ptr<Port> port);
			void set_line_output_did_change(Line line);

		private:
			bool _line_values[5];
			std::vector<std::weak_ptr<Port>> _ports;
	};

	class Port {
		public:
			Port() : _line_values{false, false, false, false, false} {}

			void set_output(Line line, bool value) {
				_line_values[line] = value;
				std::shared_ptr<Bus> bus = _serial_bus.lock();
				if(bus) bus->set_line_output_did_change(line);
			}

			bool get_output(Line line) {
				return _line_values[line];
			}

			virtual void set_input(Line line, bool value) = 0;

			inline void set_serial_bus(std::shared_ptr<Bus> serial_bus) {
				_serial_bus = serial_bus;
			}

		private:
			std::weak_ptr<Bus> _serial_bus;
			bool _line_values[5];
	};

}
}

#endif /* SerialPort_hpp */

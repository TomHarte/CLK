//
//  i8255.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef i8255_hpp
#define i8255_hpp

namespace Intel {
namespace i8255 {

class PortHandler {
	public:
		void set_value(int port, uint8_t value) {}
		uint8_t get_value(int port) { return 0xff; }
};

// TODO: most of the implementation below. Right now it just blindly passes data in all directions,
// ignoring operation mode. But at least it establishes proper ownership and hand-off of decision making.
template <class T> class i8255 {
	public:
		i8255() : control_(0), outputs_{0, 0, 0} {}

		void set_register(int address, uint8_t value) {
			switch(address & 3) {
				case 0:	outputs_[0] = value; port_handler_.set_value(0, value);	break;
				case 1:	outputs_[1] = value; port_handler_.set_value(1, value);	break;
				case 2:	outputs_[2] = value; port_handler_.set_value(2, value);	break;
				case 3:
					if(value & 0x80) {
						control_ = value;
					} else {
						if(value & 1) {
							outputs_[2] |= 1 << ((value >> 1)&7);
						} else {
							outputs_[2] &= ~(1 << ((value >> 1)&7));
						}
					}
					update_outputs();
				break;
			}
		}

		uint8_t get_register(int address) {
			switch(address & 3) {
				case 0:	return port_handler_.get_value(0);
				case 1:	return port_handler_.get_value(1);
				case 2:	return port_handler_.get_value(2);
				case 3:	return control_;
			}
			return 0xff;
		}

	private:
		void update_outputs() {
			port_handler_.set_value(0, outputs_[0]);
			port_handler_.set_value(1, outputs_[1]);
			port_handler_.set_value(2, outputs_[2]);
		}

		uint8_t control_;
		uint8_t outputs_[3];
		T port_handler_;
};

}
}

#endif /* i8255_hpp */

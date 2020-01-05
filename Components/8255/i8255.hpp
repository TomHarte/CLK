//
//  i8255.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
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

// TODO: Modes 1 and 2.
template <class T> class i8255 {
	public:
		i8255(T &port_handler) : control_(0), outputs_{0, 0, 0}, port_handler_(port_handler) {}

		/*!
			Stores the value @c value to the register at @c address. If this causes a change in 8255 output
			then the PortHandler will be informed.
		*/
		void write(int address, uint8_t value) {
			switch(address & 3) {
				case 0:
					if(!(control_ & 0x10)) {
						// TODO: so what would output be when switching from input to output mode?
						outputs_[0] = value; port_handler_.set_value(0, value);
					}
				break;
				case 1:
					if(!(control_ & 0x02)) {
						outputs_[1] = value; port_handler_.set_value(1, value);
					}
				break;
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

		/*!
			Obtains the current value for the register at @c address. If this provides a reading
			of input then the PortHandler will be queried.
		*/
		uint8_t read(int address) {
			switch(address & 3) {
				case 0:	return (control_ & 0x10) ? port_handler_.get_value(0) : outputs_[0];
				case 1:	return (control_ & 0x02) ? port_handler_.get_value(1) : outputs_[1];
				case 2:	{
					if(!(control_ & 0x09)) return outputs_[2];
					uint8_t input = port_handler_.get_value(2);
					return ((control_ & 0x01) ? (input & 0x0f) : (outputs_[2] & 0x0f)) | ((control_ & 0x08) ? (input & 0xf0) : (outputs_[2] & 0xf0));
				}
				case 3:	return control_;
			}
			return 0xff;
		}

	private:
		void update_outputs() {
			if(!(control_ & 0x10)) port_handler_.set_value(0, outputs_[0]);
			if(!(control_ & 0x02)) port_handler_.set_value(1, outputs_[1]);
			port_handler_.set_value(2, outputs_[2]);
		}

		uint8_t control_;
		uint8_t outputs_[3];
		T &port_handler_;
};

}
}

#endif /* i8255_hpp */

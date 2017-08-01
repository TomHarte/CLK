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
};

template <class T> class i8255 {
	public:
		void set_register(int address, uint8_t value) {
			switch((address >> 8) & 3) {
				case 0:	printf("PSG data: %d\n", value);		break;
				case 1:	printf("Vsync, etc: %02x\n", value);	break;
				case 2:	printf("Key row, etc: %02x\n", value);	break;
				case 3:	printf("PIO control: %02x\n", value);	break;
			}
		}

		uint8_t get_register(int address) {
			switch((address >> 8) & 3) {
				case 0:	printf("[In] PSG data\n");		break;
				case 1:	printf("[In] Vsync, etc\n");	break;
				case 2:	printf("[In] Key row, etc\n");	break;
				case 3:	printf("[In] PIO control\n");	break;
			}
			return 0xff;
		}
};

}
}

#endif /* i8255_hpp */

//
//  RTC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef RTC_h
#define RTC_h

namespace PCCompatible {

class RTC {
	public:
		template <int address>
		void write(uint8_t value) {
			switch(address) {
				default: break;
				case 0:
					selected_ = value & 0x7f;
					// NMI not yet supported.
				break;
				case 1:
					// TODO.
				break;
			}
		}

		uint8_t read() {
			return 0x00;
		}

	private:
		int selected_;
};

}

#endif /* RTC_h */

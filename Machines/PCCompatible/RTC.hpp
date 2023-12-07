//
//  RTC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef RTC_h
#define RTC_h

#include <ctime>

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
					write_register(value);
				break;
			}
		}

		uint8_t read() {
			std::time_t now = std::time(NULL);
			std::tm *time_date = std::localtime(&now);

			switch(selected_) {
				default:
				return 0xff;

				case 0x00:	return bcd(time_date->tm_sec);			// Seconds [0-59]
				case 0x01:	return 0;	// Seconds alarm
				case 0x02:	return bcd(time_date->tm_min);			// Minutes [0-59]
				case 0x03:	return 0;	// Minutes alarm
				case 0x04:
					// Hours [1-12 or 0-23]
					if(statusB_ & 2) {
						return bcd(time_date->tm_hour);
					}
					return bcd(1 + (time_date->tm_hour + 11)%12);
				break;
				case 0x05:	return 0;	// Hours alarm
				case 0x06:	return bcd(time_date->tm_wday + 1);		// Day of the week [Sunday = 1]
				case 0x07:	return bcd(time_date->tm_mon);			// Date of the Month [1-31]
				case 0x08:	return bcd(time_date->tm_mon + 1);		// Month [1-12]
				case 0x09:	return bcd(time_date->tm_year % 100);	// Year [0-99]
				case 0x32:	return bcd(19 + time_date->tm_year / 100);	// Century

				case 0x0a:	return statusA_;
				case 0x0b:	return statusB_;
			}
		}

	private:
		int selected_;

		uint8_t statusA_ = 0x00;
		uint8_t statusB_ = 0x00;

		template <typename IntT>
		uint8_t bcd(IntT input) {
			// If calendar is in binary format, don't convert.
			if(statusB_ & 4) {
				return uint8_t(input);
			}

			// Convert a one or two digit number to BCD.
			return uint8_t(
				(input % 10) +
				((input / 10) * 16)
			);
		}

		void write_register(uint8_t value) {
			switch(selected_) {
				default: break;
				case 0x0a:	statusA_ = value;	break;
			}
		}
};

}

#endif /* RTC_h */

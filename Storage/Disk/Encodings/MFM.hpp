//
//  MFM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Disk_Encodings_MFM_hpp
#define Storage_Disk_Encodings_MFM_hpp

#include <cstdint>
#include <vector>
#import "../Disk.hpp"

namespace Storage {
namespace Encodings {
namespace MFM {

template <class T> class Shifter {
	public:
		virtual void add_byte(uint8_t input) = 0;
		virtual void add_index_address_mark() = 0;
		virtual void add_ID_address_mark() = 0;
		virtual void add_data_address_mark() = 0;
		virtual void add_deleted_data_address_mark() = 0;

	protected:
		// override me!
		void output_short(uint16_t value);
};

template <class T> class MFMShifter: public Shifter<T> {
	public:
		void add_byte(uint8_t input) {
			uint16_t spread_value =
				(uint16_t)(
					((input & 0x01) << 0) |
					((input & 0x02) << 1) |
					((input & 0x04) << 2) |
					((input & 0x08) << 3) |
					((input & 0x10) << 4) |
					((input & 0x20) << 5) |
					((input & 0x40) << 6) |
					((input & 0x80) << 7)
				);
			uint16_t or_bits = (uint16_t)((spread_value << 1) | (spread_value >> 1) | (output_ << 15));
			output_ = spread_value | ((~or_bits) & 0xaaaa);
			static_cast<T *>(this)->output_short(output_);
		}

		void add_index_address_mark() {
			static_cast<T *>(this)->output_short(output_ = 0x5224);
			add_byte(0xfc);
		}

		void add_ID_address_mark() {
			static_cast<T *>(this)->output_short(output_ = 0x4489);
			add_byte(0xfe);
		}

		void add_data_address_mark() {
			static_cast<T *>(this)->output_short(output_ = 0x4489);
			add_byte(0xfb);
		}

		void add_deleted_data_address_mark() {
			static_cast<T *>(this)->output_short(output_ = 0x4489);
			add_byte(0xf8);
		}

	private:
		uint16_t output_;
};

template <class T> class FMShifter: public Shifter<T> {
	public:
		void add_byte(uint8_t input) {
			static_cast<T *>(this)->output_short(
				(uint16_t)(
					((input & 0x01) << 1) |
					((input & 0x02) << 2) |
					((input & 0x04) << 3) |
					((input & 0x08) << 4) |
					((input & 0x10) << 5) |
					((input & 0x20) << 6) |
					((input & 0x40) << 7) |
					((input & 0x80) << 8) |
					0x5555
				));
		}

		void add_index_address_mark() {
			// data 0xfc, with clock 0xd7 => 1111 1100 with clock 1101 0111 => 1111 1011 1011 0101
			static_cast<T *>(this)->output_short(0xfbb5);
		}

		void add_ID_address_mark() {
			// data 0xfe, with clock 0xc7 => 1111 1110 with clock 1100 0111 => 1111 1010 1011 1101
			static_cast<T *>(this)->output_short(0xfabd);
		}

		void add_data_address_mark() {
			// data 0xfb, with clock 0xc7 => 1111 1011 with clock 1100 0111 => 1111 1010 1001 1111
			static_cast<T *>(this)->output_short(0xfa9f);
		}

		void add_deleted_data_address_mark() {
			// data 0xf8, with clock 0xc7 => 1111 1000 with clock 1100 0111 => 1111 1010 1001 0101
			static_cast<T *>(this)->output_short(0xfa95);
		}
};

struct Sector {
	uint8_t track, side, sector;
	std::vector<uint8_t> data;
};

std::shared_ptr<Storage::Disk::Track> GetMFMTrackWithSectors(const std::vector<Sector> &sectors);
std::shared_ptr<Storage::Disk::Track> GetFMTrackWithSectors(const std::vector<Sector> &sectors);

}
}
}

#endif /* MFM_hpp */

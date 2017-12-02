//
//  Cartridge.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Cartridge_hpp
#define Storage_Cartridge_hpp

#include <vector>
#include <memory>

namespace Storage {
namespace Cartridge {

/*!
	Provides a base class for cartridges; the bus provided to cartridges and therefore
	the interface they support is extremely machine-dependent so unlike disks and tapes,
	no model is imposed; this class seeks merely to be a base class for fully-descriptive
	summaries of the contents of emulator files that themselves describe cartridges.

	Consumers will almost certainly seek to dynamic_cast to something more appropriate,
	however some cartridge container formats have no exposition beyond the ROM dump,
	making the base class 100% descriptive.
*/
class Cartridge {
	public:
		struct Segment {
			Segment(size_t start_address, size_t end_address, std::vector<uint8_t> data) :
				start_address(start_address), end_address(end_address), data(std::move(data)) {}

			Segment(int start_address, std::vector<uint8_t> data) :
				Segment(start_address, start_address + data.size(), data) {}

			/// Indicates that an address is unknown.
			static const size_t UnknownAddress;

			/// The initial CPU-exposed starting address for this segment; may be @c UnknownAddress.
			size_t start_address;
			/*!
				The initial CPU-exposed ending address for this segment; may be @c UnknownAddress. Not necessarily equal
				to start_address + data_length due to potential paging.
			*/
			size_t end_address;

			/*!
				The data contents for this segment. If @c start_address and @c end_address are suppled then
				the first end_address - start_address bytes will be those initially visible. The size will
				not necessarily be the same as @c end_address - @c start_address due to potential paging.
			*/
			std::vector<uint8_t> data;
		};

		const std::vector<Segment> &get_segments() const {
			return segments_;
		}
		virtual ~Cartridge() {}

		Cartridge() {}
		Cartridge(const std::vector<Segment> &segments) : segments_(segments) {}

	protected:
		std::vector<Segment> segments_;
};

}
}

#endif /* ROM_hpp */

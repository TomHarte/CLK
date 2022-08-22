//
//  SCSICard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/08/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef SCSICard_hpp
#define SCSICard_hpp

#include "Card.hpp"
#include "../../ROMMachine.hpp"

#include "../../../Components/5380/ncr5380.hpp"

#include <array>

namespace Apple {
namespace II {

class SCSICard: public Card {
	public:
		static ROM::Request rom_request();
		SCSICard(ROM::Map &);

		void perform_bus_operation(Select select, bool is_read, uint16_t address, uint8_t *value) final;

	private:
		uint8_t *ram_pointer_ = nullptr;
		uint8_t *rom_pointer_ = nullptr;

		std::array<uint8_t, 8*1024> ram_;
		std::array<uint8_t, 16*1024> rom_;

		SCSI::Bus scsi_bus_;
		NCR::NCR5380::NCR5380 ncr5380_;

		// TODO: the rest of this.
};

}
}

#endif /* SCSICard_hpp */

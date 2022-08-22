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

namespace Apple {
namespace II {

class SCSICard: public Card {
	public:
		static ROM::Request rom_request();
		SCSICard(ROM::Map &);

		void perform_bus_operation(Select select, bool is_read, uint16_t address, uint8_t *value) final;
		void run_for(Cycles cycles, int stretches) final;

	private:
		// TODO.
};

}
}

#endif /* SCSICard_hpp */

//
//  Commodore1540.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Commodore1540_hpp
#define Commodore1540_hpp

namespace Commodore {
namespace C1540 {

/// Defines the type of drive this 1540 hardware is configured as.
enum class Personality {
	C1540,
	C1541
};

/*
	Implementation note: this is defined up here so that it precedes
	C1540Base.hpp below. The alternative option was to factor it out,
	but the whole point of the C1540.hpp/C1540Base.hpp split is supposed
	to be to create a single file of public interface.
*/

}
}

#include "../SerialBus.hpp"
#include "../../ROMMachine.hpp"
#include "../../../Storage/Disk/Disk.hpp"
#include "Implementation/C1540Base.hpp"

namespace Commodore {
namespace C1540 {

/*!
	Provides an emulation of the C1540.
*/
class Machine final: public MachineBase {
	public:
		Machine(Personality personality, const ROMMachine::ROMFetcher &rom_fetcher);

		/*!
			Sets the serial bus to which this drive should attach itself.
		*/
		void set_serial_bus(std::shared_ptr<::Commodore::Serial::Bus> serial_bus);

		/// Advances time.
		void run_for(const Cycles cycles);

		/// Inserts @c disk into the drive.
		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk);
};

}
}

#endif /* Commodore1540_hpp */

//
//  Commodore1540.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Commodore1540_hpp
#define Commodore1540_hpp

#include "../SerialBus.hpp"
#include "../../../Storage/Disk/Disk.hpp"
#include "Implementation/C1540Base.hpp"

namespace Commodore {
namespace C1540 {

/*!
	Provides an emulation of the C1540.
*/
class Machine: public MachineBase {
	public:
		/*!
			Sets the ROM image to use for this drive; it is asserted that the buffer provided is 16 kb in size.
		*/
		void set_rom(const std::vector<uint8_t> &rom);

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

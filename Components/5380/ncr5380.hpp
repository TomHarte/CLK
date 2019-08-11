//
//  ncr5380.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef ncr5380_hpp
#define ncr5380_hpp

#include <cstdint>

namespace NCR {
namespace NCR5380 {

/*!
	Models the NCR 5380, a SCSI interface chip.
*/
class NCR5380 {
	public:
		/*! Writes @c value to @c address.  */
		void write(int address, uint8_t value);

		/*! Reads from @c address. */
		uint8_t read(int address);
};

}
}

#endif /* ncr5380_hpp */

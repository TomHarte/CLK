//
//  AcornADF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef AcornADF_hpp
#define AcornADF_hpp

#include "MFMSectorDump.hpp"

namespace Storage {
namespace Disk {

/*!
	Provies a @c Disk containing an ADF disk image — a decoded sector dump of an Acorn ADFS disk.
*/
class AcornADF: public MFMSectorDump {
	public:
		/*!
			Construct an @c AcornADF containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotAcornADF if the file doesn't appear to contain an Acorn .ADF format image.
		*/
		AcornADF(const char *file_name);

		enum {
			ErrorNotAcornADF,
		};

		unsigned int get_head_position_count();
		unsigned int get_head_count();

	private:
		long get_file_offset_for_position(unsigned int head, unsigned int position);
};

}
}

#endif /* AcornADF_hpp */

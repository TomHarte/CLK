//
//  SonyDrive.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef SonyDrive_hpp
#define SonyDrive_hpp

#include "../../../Storage/Disk/Drive.hpp"

namespace Apple {
namespace Macintosh {

/*!
	Models one of the Sony drives found in an original Macintosh,
	specifically by providing automatic motor speed adjustment if
	this is an 800kb drive.
*/
class SonyDrive: public Storage::Disk::Drive {
	public:
		SonyDrive(int input_clock_rate, bool is_800k);

	private:
		void did_step(Storage::Disk::HeadPosition to_position) override;
		bool is_800k_;
};

}
}

#endif /* SonyDrive_hpp */

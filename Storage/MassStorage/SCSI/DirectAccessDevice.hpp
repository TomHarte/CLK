//
//  DirectAccessDevice.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef SCSI_DirectAccessDevice_hpp
#define SCSI_DirectAccessDevice_hpp

#include "Target.hpp"

namespace SCSI {

class DirectAccessDevice: public Target::Executor {
	public:
		bool read(const Target::CommandState &, Target::Responder &);
};

}

#endif /* SCSI_DirectAccessDevice_hpp */

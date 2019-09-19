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
#include "../MassStorageDevice.hpp"

#include <memory>

namespace SCSI {

class DirectAccessDevice: public Target::Executor {
	public:

		/*!
			Sets the backing storage exposed by this direct-access device.
		*/
		void set_storage(const std::shared_ptr<Storage::MassStorage::MassStorageDevice> &device);

		/* SCSI commands. */
		bool read(const Target::CommandState &, Target::Responder &);
		bool write(const Target::CommandState &, Target::Responder &);
		Inquiry inquiry_values();
		bool read_capacity(const Target::CommandState &, Target::Responder &);
		bool format_unit(const Target::CommandState &, Target::Responder &);

	private:
		std::shared_ptr<Storage::MassStorage::MassStorageDevice> device_;
};

}

#endif /* SCSI_DirectAccessDevice_hpp */

//
//  SCSICard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/08/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#pragma once

#include "Card.hpp"
#include "../../ROMMachine.hpp"

#include "../../../Components/5380/ncr5380.hpp"

#include "../../../Storage/MassStorage/SCSI/SCSI.hpp"
#include "../../../Storage/MassStorage/SCSI/DirectAccessDevice.hpp"
#include "../../../Storage/MassStorage/MassStorageDevice.hpp"

#include <array>
#include <memory>

namespace Apple::II {

class SCSICard: public Card {
	public:
		static ROM::Request rom_request();
		SCSICard(ROM::Map &, int clock_rate);

		void perform_bus_operation(Select select, bool is_read, uint16_t address, uint8_t *value) final;

		void set_storage_device(const std::shared_ptr<Storage::MassStorage::MassStorageDevice> &device);

		void run_for([[maybe_unused]] Cycles cycles, [[maybe_unused]] int stretches) final {
			scsi_bus_.run_for(cycles);
		}

		void set_activity_observer(Activity::Observer *observer) final;

	private:
		uint8_t *ram_pointer_ = nullptr;
		uint8_t *rom_pointer_ = nullptr;

		std::array<uint8_t, 8*1024> ram_;
		std::array<uint8_t, 16*1024> rom_;

		SCSI::Bus scsi_bus_;
		NCR::NCR5380::NCR5380 ncr5380_;
		SCSI::Target::Target<SCSI::DirectAccessDevice> storage_;
};

}

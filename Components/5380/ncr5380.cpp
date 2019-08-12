//
//  ncr5380.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "ncr5380.hpp"

#include "../../Outputs/Log.hpp"

using namespace NCR::NCR5380;

void NCR5380::write(int address, uint8_t value) {
	switch(address & 7) {
		case 0:
			LOG("[SCSI] Set current SCSI bus state to " << PADHEX(2) << value);
		break;

		case 1:
			LOG("[SCSI] Initiator command register set: " << PADHEX(2) << value);
		break;

		case 2:
			LOG("[SCSI] Set mode: " << PADHEX(2) << value);
			mode_ = value;
		break;

		case 3:
			LOG("[SCSI] Set target command: " << PADHEX(2) << value);
		break;

		case 4:
			LOG("[SCSI] Set select enabled: " << PADHEX(2) << value);
		break;

		case 5:
			LOG("[SCSI] Start DMA send: " << PADHEX(2) << value);
		break;

		case 6:
			LOG("[SCSI] Start DMA target receive: " << PADHEX(2) << value);
		break;

		case 7:
			LOG("[SCSI] Start DMA initiator receive: " << PADHEX(2) << value);
		break;
	}
}

uint8_t NCR5380::read(int address) {
	switch(address & 7) {
		case 0:
			LOG("[SCSI] Get current SCSI bus state");
		return 0xff;

		case 1:
			LOG("[SCSI] Initiator command register get");
		return 0xff;

		case 2:
			LOG("[SCSI] Get mode");
		return mode_;

		case 3:
			LOG("[SCSI] Get target command");
		return 0xff;

		case 4:
			LOG("[SCSI] Get current bus state");
		return 0xff;

		case 5:
			LOG("[SCSI] Get bus and status");
		return 0x03;

		case 6:
			LOG("[SCSI] Get input data");
		return 0xff;

		case 7:
			LOG("[SCSI] Reset parity/interrupt");
		return 0xff;
	}
	return 0;
}

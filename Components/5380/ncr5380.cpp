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
			LOG("[SCSI 0] Set current SCSI bus state to " << PADHEX(2) << int(value));
		break;

		case 1:
			LOG("[SCSI 1] Initiator command register set: " << PADHEX(2) << int(value));
		break;

		case 2:
			LOG("[SCSI 2] Set mode: " << PADHEX(2) << int(value));
			mode_ = value;
		break;

		case 3:
			LOG("[SCSI 3] Set target command: " << PADHEX(2) << int(value));
		break;

		case 4:
			LOG("[SCSI 4] Set select enabled: " << PADHEX(2) << int(value));
		break;

		case 5:
			LOG("[SCSI 5] Start DMA send: " << PADHEX(2) << int(value));
		break;

		case 6:
			LOG("[SCSI 6] Start DMA target receive: " << PADHEX(2) << int(value));
		break;

		case 7:
			LOG("[SCSI 7] Start DMA initiator receive: " << PADHEX(2) << int(value));
		break;
	}
}

uint8_t NCR5380::read(int address) {
	switch(address & 7) {
		case 0:
			LOG("[SCSI 0] Get current SCSI bus state");
		return 0xff;

		case 1:
			LOG("[SCSI 1] Initiator command register get");
		return 0xff;

		case 2:
			LOG("[SCSI 2] Get mode");
		return mode_;

		case 3:
			LOG("[SCSI 3] Get target command");
		return 0xff;

		case 4:
			LOG("[SCSI 4] Get current bus state");
		return 0xff;

		case 5:
			LOG("[SCSI 5] Get bus and status");
		return 0x03;

		case 6:
			LOG("[SCSI 6] Get input data");
		return 0xff;

		case 7:
			LOG("[SCSI 7] Reset parity/interrupt");
		return 0xff;
	}
	return 0;
}

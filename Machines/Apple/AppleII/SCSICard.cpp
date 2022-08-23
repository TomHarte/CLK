//
//  SCSICard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/08/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

// Per the documentation around the GGLabs Apple II SCSI card clone:
//
// A 5380 is mapped to the first eight bytes of slot IO:
//
//	$c0x0	R		current SCSI data register
//	$c0x0	W		output data register
//	$c0x1	R/W		initiator command register
//	$c0x2	R/W		mode select register
//	$c0x3	R/W		target command register
//	$c0x4	R		SCSI bus status
//	$c0x4	W		select enable register
//	$c0x5	R		bus and status register
//	$c0x6	R		input data register
//	$c0x7	R		reset parity and interrupts
//		(i.e. the 5380's standard registers in their usual order)
//
// The remaining eight are used for control functions:
//
//	$c0x8	R/W		PDMA/DACK
//	$c0x9	R		SCSI device ID
//	$c0xa	W		memory bank select register
//	$c0xb	W		reset 5380 SCSI chip
//	$c0xc	-		[unused]
//	$c0xd	W		PDMA mode enable
//	$c0xe	R		read DRQ status through bit 7
//	$c0xf	-		[unused]
//
// Further, per that card's schematic:
//
//	BANK REGISTER: bit 0..3 ROM Addr, 4..6 RAM Addr, 7 RSVD
//
// Which relates to the description:
//
//	The card is also equipped with 16K of ROM and 8K of RAM.
//	These are mapped in the $C800-$CFFF card memory using a banking
//	scheme. The $C0xA bank register selects the which bank of RAM
//	and ROM are mapped. RAM is always at $C800-$CBFF and ROM is
//	at $CC00-$CFFF. The boot code in the first 256 bytes of ROM
//	bank 0 is also mapped in the IOSEL space ($Cn00-$CnFF).
//

#include "SCSICard.hpp"

#include <cstring>

using namespace Apple::II;

ROM::Request SCSICard::rom_request() {
	return ROM::Request(ROM::Name::AppleIISCSICard);
}

// TODO: accept and supply real clock rate.
SCSICard::SCSICard(ROM::Map &map) : scsi_bus_(1), ncr5380_(scsi_bus_, 1) {
	// Grab a copy of the SCSI ROM.
	const auto rom = map.find(ROM::Name::AppleIISCSICard);
	if(rom == map.end()) {
		throw ROMMachine::Error::MissingROMs;
	}
	memcpy(rom_.data(), rom->second.data(), rom_.size());

	// Set up initial banking.
	rom_pointer_ = rom_.data();
	ram_pointer_ = ram_.data();
}

void SCSICard::perform_bus_operation(Select select, bool is_read, uint16_t address, uint8_t *value) {
	switch(select) {
		default: break;

		case Select::Device:
			if(is_read) {
				*value = rom_[address & 255];
			}
		break;

		case Select::IO:
			address &= 0xf;
			switch(address) {
				case 0x0:	case 0x1:	case 0x2:	case 0x3:
				case 0x4:	case 0x5:	case 0x6:	case 0x7:
					if(is_read) {
						*value = ncr5380_.read(address);
					} else {
						ncr5380_.write(address, *value);
					}
				break;

				case 0xa:
					// RAM and ROM select.
					if(!is_read) {
						const auto rom_base = size_t((*value & 0x0f) << 10);
						const auto ram_base = size_t((*value & 0x70) << 6);

						rom_pointer_ = &rom_[rom_base];
						ram_pointer_ = &ram_[ram_base];
					}
				break;

				default:
					printf("Unhandled: %04x %c %02x\n", address, is_read ? 'r' : 'w', *value);
				break;
			}
		break;

		case Select::C8Region:
			if(address & 0x400) {
				if(is_read) {
					*value = rom_pointer_[address & 0x3ff];
				}
			} else {
				if(is_read) {
					*value = ram_pointer_[address & 0x3ff];
				} else {
					ram_pointer_[address & 0x3ff] = *value;
				}
			}
		break;
	}
}

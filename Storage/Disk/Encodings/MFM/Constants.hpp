//
//  Constants.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Constants_h
#define Constants_h

#include "../../../Storage.hpp"

namespace Storage {
namespace Encodings {
namespace MFM {

const uint8_t IndexAddressByte			= 0xfc;
const uint8_t IDAddressByte				= 0xfe;
const uint8_t DataAddressByte			= 0xfb;
const uint8_t DeletedDataAddressByte	= 0xf8;

const uint16_t FMIndexAddressMark		= 0xf77a;	// data 0xfc, with clock 0xd7 => 1111 1100 with clock 1101 0111 => 1111 0111 0111 1010
const uint16_t FMIDAddressMark			= 0xf57e;	// data 0xfe, with clock 0xc7 => 1111 1110 with clock 1100 0111 => 1111 0101 0111 1110
const uint16_t FMDataAddressMark		= 0xf56f;	// data 0xfb, with clock 0xc7 => 1111 1011 with clock 1100 0111 => 1111 0101 0110 1111
const uint16_t FMDeletedDataAddressMark	= 0xf56a;	// data 0xf8, with clock 0xc7 => 1111 1000 with clock 1100 0111 => 1111 0101 0110 1010

const uint16_t MFMIndexSync				= 0x5224;	// data 0xc2, with a missing clock at 0x0080 => 0101 0010 1010 0100 without 1000 0000
const uint16_t MFMSync					= 0x4489;	// data 0xa1, with a missing clock at 0x0020 => 0100 0100 1010 1001 without 0010 0000
const uint16_t MFMPostSyncCRCValue		= 0xcdb4;	// the value the CRC generator should have after encountering three 0xa1s

const uint8_t MFMIndexSyncByteValue		= 0xc2;
const uint8_t MFMSyncByteValue			= 0xa1;

const Time MFMBitLength					= Time(1, 100000);
const Time FMBitLength					= Time(1, 50000);

}
}
}

#endif /* Constants_h */

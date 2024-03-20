//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Outputs/Log.hpp"

#include <cstdint>

namespace Archimedes {

struct Video {
	void write(uint32_t value) {
		const auto target = (value >> 24) & 0xfc;

		switch(target) {
			case 0x00:	case 0x04:	case 0x08:	case 0x0c:
			case 0x10:	case 0x14:	case 0x18:	case 0x1c:
			case 0x20:	case 0x24:	case 0x28:	case 0x2c:
			case 0x30:	case 0x34:	case 0x38:	case 0x3c:
				logger.error().append("TODO: Video palette logical colour %d to %03x", (target >> 2), value & 0x1fff);
			break;

			case 0x40:
				logger.error().append("TODO: Video border colour to %03x", value & 0x1fff);
			break;

			case 0x44:	case 0x48:	case 0x4c:
				logger.error().append("TODO: Cursor colour %d to %03x", (target - 0x44) >> 2, value & 0x1fff);
			break;

			case 0x60:	case 0x64:	case 0x68:	case 0x6c:
			case 0x70:	case 0x74:	case 0x78:	case 0x7c:
				logger.error().append("TODO: Stereo image register %d to %03x", (target - 0x60) >> 2, value & 0x7);
			break;

			case 0x80:
				logger.error().append("TODO: Video horizontal period: %d", (value >> 14) & 0x3ff);
			break;
			case 0x84:
				logger.error().append("TODO: Video horizontal sync width: %d", (value >> 14) & 0x3ff);
			break;
			case 0x88:
				logger.error().append("TODO: Video horizontal border start: %d", (value >> 14) & 0x3ff);
			break;
			case 0x8c:
				logger.error().append("TODO: Video horizontal display start: %d", (value >> 14) & 0x3ff);
			break;
			case 0x90:
				logger.error().append("TODO: Video horizontal display end: %d", (value >> 14) & 0x3ff);
			break;
			case 0x94:
				logger.error().append("TODO: Video horizontal border end: %d", (value >> 14) & 0x3ff);
			break;
			case 0x98:
				logger.error().append("TODO: Video horizontal cursor end: %d", (value >> 14) & 0x3ff);
			break;
			case 0x9c:
				logger.error().append("TODO: Video horizontal interlace: %d", (value >> 14) & 0x3ff);
			break;

			case 0xa0:
				logger.error().append("TODO: Video vertical period: %d", (value >> 14) & 0x3ff);
			break;
			case 0xa4:
				logger.error().append("TODO: Video vertical sync width: %d", (value >> 14) & 0x3ff);
			break;
			case 0xa8:
				logger.error().append("TODO: Video vertical border start: %d", (value >> 14) & 0x3ff);
			break;
			case 0xac:
				logger.error().append("TODO: Video vertical display start: %d", (value >> 14) & 0x3ff);
			break;
			case 0xb0:
				logger.error().append("TODO: Video vertical display end: %d", (value >> 14) & 0x3ff);
			break;
			case 0xb4:
				logger.error().append("TODO: Video vertical border end: %d", (value >> 14) & 0x3ff);
			break;
			case 0xb8:
				logger.error().append("TODO: Video vertical cursor start: %d", (value >> 14) & 0x3ff);
			break;
			case 0xbc:
				logger.error().append("TODO: Video vertical cursor end: %d", (value >> 14) & 0x3ff);
			break;

			case 0xc0:
				logger.error().append("TODO: Sound frequency: %d", value & 0x7f);
			break;

			case 0xe0:
				logger.error().append("TODO: video control: %08x", value);
			break;

			default:
				logger.error().append("TODO: unrecognised VIDC write of %08x", value);
			break;
		}
	}

private:
	Log::Logger<Log::Source::ARMIOC> logger;
};

}

//
//  Minterms.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include <concepts>

namespace Amiga {

/// @returns the result of applying the Amiga-format @c minterm to inputs @c a, @c b and @c c.
template <std::unsigned_integral IntT>
IntT apply_minterm(const IntT a, const IntT b, const IntT c, const uint8_t minterm) {

	// Quick implementation notes:
	//
	// This was created very lazily. I tried to enter as many logical combinations of
	// a, b and c as I could think of and had a test program match them up to the
	// Amiga minterm IDs, prioritising by simplicity.
	//
	// That got me most of the way; starting from the point indicated below I had run
	// out of good ideas and automatically generated the rest.
	//
	switch(minterm) {
		case 0x00:	return IntT(0);
		case 0xff:	return IntT(~0);

		case 0xf0:	return IntT(a);
		case 0xcc:	return IntT(b);
		case 0xaa:	return IntT(c);

		case 0x0f:	return IntT(~a);
		case 0x33:	return IntT(~b);
		case 0x55:	return IntT(~c);

		case 0xfc:	return IntT(a | b);
		case 0xfa:	return IntT(a | c);
		case 0xee:	return IntT(b | c);
		case 0xfe:	return IntT(a | b | c);

		case 0xf3:	return IntT(a | ~b);
		case 0xf5:	return IntT(a | ~c);
		case 0xdd:	return IntT(b | ~c);

		case 0xfd:	return IntT(a | b | ~c);
		case 0xfb:	return IntT(a | ~b | c);
		case 0xf7:	return IntT(a | ~b | ~c);

		case 0xcf:	return IntT(~a | b);
		case 0xaf:	return IntT(~a | c);
		case 0xbb:	return IntT(~b | c);

		case 0xef:	return IntT(~a | b | c);
		case 0xdf:	return IntT(~a | b | ~c);
		case 0x7f:	return IntT(~a | ~b | ~c);

		case 0x3c:	return IntT(a ^ b);
		case 0x5a:	return IntT(a ^ c);
		case 0x66:	return IntT(b ^ c);
		case 0x96:	return IntT(a ^ b ^ c);

		case 0xc3:	return IntT(~a ^ b);
		case 0xa5:	return IntT(~a ^ c);
		case 0x99:	return IntT(~b ^ c);
		case 0x69:	return IntT(~a ^ b ^ c);

		case 0xc0:	return IntT(a & b);
		case 0xa0:	return IntT(a & c);
		case 0x88:	return IntT(b & c);
		case 0x80:	return IntT(a & b & c);

		case 0x30:	return IntT(a & ~b);
		case 0x50:	return IntT(a & ~c);
		case 0x44:	return IntT(b & ~c);

		case 0x0c:	return IntT(~a & b);
		case 0x0a:	return IntT(~a & c);
		case 0x22:	return IntT(~b & c);

		case 0x40:	return IntT(a & b & ~c);
		case 0x20:	return IntT(a & ~b & c);
		case 0x08:	return IntT(~a & b & c);

		case 0x10:	return IntT(a & ~b & ~c);
		case 0x04:	return IntT(~a & b & ~c);
		case 0x02:	return IntT(~a & ~b & c);

		case 0x03:	return IntT(~a & ~b);
		case 0x05:	return IntT(~a & ~c);
		case 0x11:	return IntT(~b & ~c);
		case 0x01:	return IntT(~a & ~b & ~c);

		case 0x70:	return IntT(a & ~(b & c));
		case 0x4c:	return IntT(b & ~(a & c));
		case 0x2a:	return IntT(c & ~(a & b));

		case 0x07:	return IntT(~a & ~(b & c));
		case 0x13:	return IntT(~b & ~(a & c));
		case 0x15:	return IntT(~c & ~(a & b));

		case 0xe0:	return IntT(a & (b | c));
		case 0xc8:	return IntT(b & (a | c));
		case 0xa8:	return IntT(c & (a | b));

		case 0x0e:	return IntT(~a & (b | c));
		case 0x32:	return IntT(~b & (a | c));
		case 0x54:	return IntT(~c & (a | b));

		case 0x60:	return IntT(a & (b ^ c));
		case 0x48:	return IntT(b & (a ^ c));
		case 0x28:	return IntT(c & (a ^ b));

		case 0x06:	return IntT(~a & (b ^ c));
		case 0x12:	return IntT(~b & (a ^ c));
		case 0x14:	return IntT(~c & (a ^ b));

		case 0x90:	return IntT(a & ~(b ^ c));
		case 0x84:	return IntT(b & ~(a ^ c));
		case 0x82:	return IntT(c & ~(a ^ b));

		case 0x09:	return IntT(~a & ~(b ^ c));
		case 0x21:	return IntT(~b & ~(a ^ c));
		case 0x41:	return IntT(~c & ~(a ^ b));

		case 0xb0:	return IntT(a & (~b | c));
		case 0xd0:	return IntT(a & (b | ~c));
		case 0x0b:	return IntT(~a & (~b | c));
		case 0x0d:	return IntT(~a & (b | ~c));

		case 0xf6:	return IntT(a | (b ^ c));
		case 0xde:	return IntT(b | (a ^ c));
		case 0xbe:	return IntT(c | (a ^ b));

		case 0x6f:	return IntT(~a | (b ^ c));
		case 0x7b:	return IntT(~b | (a ^ c));
		case 0x7d:	return IntT(~c | (a ^ b));

		case 0x9f:	return IntT(~a | ~(b ^ c));
		case 0xb7:	return IntT(~b | ~(a ^ c));
		case 0xd7:	return IntT(~c | ~(a ^ b));

		case 0xf8:	return IntT(a | (b & c));
		case 0xec:	return IntT(b | (a & c));
		case 0xea:	return IntT(c | (a & b));

		case 0x8f:	return IntT(~a | (b & c));
		case 0xb3:	return IntT(~b | (a & c));
		case 0xd5:	return IntT(~c | (a & b));

		case 0xf1:	return IntT(a | ~(b | c));
		case 0xcd:	return IntT(b | ~(a | c));
		case 0xab:	return IntT(c | ~(a | b));

		case 0x1f:	return IntT(~a | ~(b | c));
		case 0x37:	return IntT(~b | ~(a | c));
		case 0x57:	return IntT(~c | ~(a | b));

		case 0x8c:	return IntT(b & (~a | c));
		case 0x8a:	return IntT(c & (~a | b));

		case 0xc4:	return IntT(b & (a | ~c));
		case 0xa2:	return IntT(c & (a | ~b));

		case 0x78:	return IntT(a ^ (b & c));
		case 0x6c:	return IntT(b ^ (a & c));
		case 0x6a:	return IntT(c ^ (a & b));

		case 0x87:	return IntT(~a ^ (b & c));
		case 0x93:	return IntT(~b ^ (a & c));
		case 0x95:	return IntT(~c ^ (a & b));

		case 0x1e:	return IntT(a ^ (b | c));
		case 0x36:	return IntT(b ^ (a | c));
		case 0x56:	return IntT(c ^ (a | b));

		case 0x2d:	return IntT(a ^ (b | ~c));
		case 0x4b:	return IntT(a ^ (~b | c));
		case 0xe1:	return IntT(a ^ ~(b | c));

		case 0x39:	return IntT(b ^ (a | ~c));
		case 0x63:	return IntT(b ^ (~a | c));
		case 0xc9:	return IntT(b ^ ~(a | c));

		case 0x59:	return IntT(c ^ (a | ~b));
		case 0x65:	return IntT(c ^ (~a | b));
		case 0xa9:	return IntT(c ^ ~(a | b));

		case 0x24:	return IntT((a ^ b) & (b ^ c));
		case 0x18:	return IntT((a ^ b) & (a ^ c));
		case 0x42:	return IntT((a ^ c) & (b ^ c));

		case 0xa6:	return IntT((a & b) ^ (b ^ c));
		case 0xc6:	return IntT((a & c) ^ (b ^ c));

		case 0x5c:	return IntT((a | b) ^ (a & c));
		case 0x74:	return IntT((a | b) ^ (b & c));
		case 0x72:	return IntT((a | c) ^ (b & c));
		case 0x4e:	return IntT((b | c) ^ (a & c));

		case 0x58:	return IntT((a | b) & (a ^ c));
		case 0x62:	return IntT((a | c) & (b ^ c));

		case 0x7e:	return IntT((a ^ b) | (a ^ c));

		case 0xca:	return IntT((a & b) | (~a & c));
		case 0xac:	return IntT((~a & b) | (a & c));
		case 0xa3:	return IntT((~a & ~b) | (a & c));

		case 0xf4:	return IntT(a | ((a ^ b) & (b ^ c)));
		case 0xf2:	return IntT(a | ((a ^ c) & (b ^ c)));
		case 0xdc:	return IntT(b | ((a ^ b) & (a ^ c)));
		case 0xce:	return IntT(b | ((a ^ c) & (b ^ c)));
		case 0xae:	return IntT(c | ((a ^ b) & (b ^ c)));
		case 0xba:	return IntT(c | ((a ^ b) & (a ^ c)));

		case 0x2f:	return IntT(~a | ((a ^ b) & (b ^ c)));
		case 0x4f:	return IntT(~a | ((a ^ c) & (b ^ c)));
		case 0x3b:	return IntT(~b | ((a ^ b) & (a ^ c)));
		case 0x73:	return IntT(~b | ((a ^ c) & (b ^ c)));
		case 0x75:	return IntT(~c | ((a ^ b) & (b ^ c)));
		case 0x5d:	return IntT(~c | ((a ^ b) & (a ^ c)));

		case 0x3f:	return IntT(~a | ~b | ((a ^ b) & (b ^ c)));
		case 0x77:	return IntT(~b | ~c | ((a ^ b) & (b ^ c)));

		case 0x27:	return IntT(~(a | b) | ((a ^ b) & (b ^ c)));
		case 0x47:	return IntT(~(a | c) | ((a ^ c) & (b ^ c)));
		case 0x53:	return IntT(~(b | c) | ((a ^ c) & (b ^ c)));
		case 0x43:	return IntT(~(a | b | c) | ((a ^ c) & (b ^ c)));

		case 0x7a:	return IntT((a & ~b) | (a ^ c));
		case 0x76:	return IntT((a & ~b) | (b ^ c));
		case 0x7c:	return IntT((a & ~c) | (a ^ b));

		case 0x5e:	return IntT((~a & b) | (a ^ c));
		case 0x6e:	return IntT((~a & b) | (b ^ c));
		case 0x3e:	return IntT((~a & c) | (a ^ b));

		case 0xad:	return IntT((~a & b) | ~(a ^ c));
		case 0xb5:	return IntT((a & ~b) | ~(a ^ c));
		case 0xcb:	return IntT((~a & c) | ~(a ^ b));
		case 0xd3:	return IntT((a & ~c) | ~(a ^ b));

		case 0x9b:	return IntT((~a & c) | ~(b ^ c));
		case 0xd9:	return IntT((a & ~c) | ~(b ^ c));
		case 0x9d:	return IntT((~a & b) | ~(b ^ c));
		case 0xb9:	return IntT((a & ~b) | ~(b ^ c));

		case 0x9e:	return IntT((~a & b) | (a ^ b ^ c));
		case 0xb6:	return IntT((a & ~b) | (a ^ b ^ c));
		case 0xd6:	return IntT((a & ~c) | (a ^ b ^ c));
		case 0xbf:	return IntT(~(a & b) | (a ^ b ^ c));

		case 0x6d:	return IntT((~a & b) | ~(a ^ b ^ c));
		case 0x79:	return IntT((a & ~b) | ~(a ^ b ^ c));
		case 0x6b:	return IntT((~a & c) | ~(a ^ b ^ c));
		case 0xe9:	return IntT((b & c) | ~(a ^ b ^ c));

		case 0xb8:	return IntT((a & ~b) | (c & b));
		case 0xd8:	return IntT((a & ~c) | (b & c));
		case 0xe4:	return IntT((b & ~c) | (a & c));
		case 0xe2:	return IntT((c & ~b) | (a & b));

		case 0x2c:	return IntT((~a & b) | ((a ^ b) & (b ^ c)));
		case 0x34:	return IntT((a & ~b) | ((a ^ b) & (b ^ c)));
		case 0x4a:	return IntT((~a & c) | ((a ^ c) & (b ^ c)));
		case 0x52:	return IntT((a & ~c) | ((a ^ c) & (b ^ c)));
		case 0x5f:	return IntT(~(a & c) | ((a ^ c) & (b ^ c)));

		case 0x16:	return IntT((a & ~(c | b)) | (c & ~(b | a)) | (b & ~(a | c)));
		case 0x81:	return IntT((a ^ ~(c | b)) & (c ^ ~(b | a)) & (b ^ ~(a | c)));

		case 0x2e:	return IntT((~a & (b | c)) | (~b & c));
		case 0x3a:	return IntT((~b & (a | c)) | (~a & c));

		case 0x8b:	return IntT((~a & ~b) | (c & b));
		case 0x8d:	return IntT((~a & ~c) | (b & c));
		case 0xb1:	return IntT((~b & ~c) | (a & c));
		case 0xd1:	return IntT((~c & ~b) | (a & b));

		case 0x98:	return IntT((a & ~(c | b)) | (b & c));
		case 0x8e:	return IntT((~a & (c | b)) | (b & c));

		case 0x46:	return IntT((~a | b) & (b ^ c));

		case 0xe6:	return IntT(((~a | b) & (b ^ c)) ^ (a & c));
		case 0xc2:	return IntT(((a | ~b) & (b ^ c)) ^ (a & c));

		case 0x85:	return IntT((~a | b) & ~(a ^ c));
		case 0x83:	return IntT((~a | c) & ~(a ^ b));
		case 0x89:	return IntT((~a | c) & ~(b ^ c));

		case 0xa1:	return IntT((a | ~b) & ~(a ^ c));
		case 0x91:	return IntT((a | ~b) & ~(b ^ c));
		case 0xc1:	return IntT((a | ~c) & ~(a ^ b));

		case 0x94:	return IntT((a | b) & (a ^ b ^ c));
		case 0x86:	return IntT((b | c) & (a ^ b ^ c));
		case 0x92:	return IntT((a | c) & (a ^ b ^ c));

		case 0x68:	return IntT((a | b) & ~(a ^ b ^ c));
		case 0x61:	return IntT((a | ~b) & ~(a ^ b ^ c));
		case 0x49:	return IntT((~a | b) & ~(a ^ b ^ c));
		case 0x29:	return IntT((~a | c) & ~(a ^ b ^ c));

		case 0x64:	return IntT((a & ~b & c) | (b & ~c));

		//
		// From here downwards functions were found automatically.
		// Neater versions likely exist of many of the functions below.
		//

		case 0xe8:	return IntT((a & b) | ((b | a) & c));
		case 0xd4:	return IntT((a & b) | ((b | a) & ~c));
		case 0xb2:	return IntT((a & ~b) | ((~b | a) & c));
		case 0x17:	return IntT((~a & ~b) | ((~b | ~a) & ~c));
		case 0x1b:	return IntT((~a & ~b) | (~b & ~c) | (~a & c));
		case 0x1d:	return IntT((~a & b) | ((~b | ~a) & ~c));
		case 0x2b:	return IntT((~a & ~b) | ((~b | ~a) & c));
		case 0x35:	return IntT((a & ~b) | ((~b | ~a) & ~c));
		case 0x4d:	return IntT((~a & b) | ((b | ~a) & ~c));
		case 0x71:	return IntT((a & ~b) | ((~b | a) & ~c));
		case 0xbd:	return IntT((~a & b) | (~b & ~c) | (a & c));
		case 0xc5:	return IntT((a & b) | ((b | ~a) & ~c));
		case 0xdb:	return IntT((a & b) | (~b & ~c) | (~a & c));
		case 0xe7:	return IntT((~a & ~b) | (b & ~c) | (a & c));


		case 0x1c:	return IntT((~a & b) | (a & ~b & ~c));
		case 0x23:	return IntT((~a & ~b) | (a & ~b & c));
		case 0x31:	return IntT((a & ~b) | (~a & ~b & ~c));
		case 0x38:	return IntT((a & ~b) | (~a & b & c));
		case 0x1a:	return IntT((~a & c) | (a & ~b & ~c));
		case 0x25:	return IntT((~a & ~c) | (a & ~b & c));
		case 0x45:	return IntT((~a & ~c) | (a & b & ~c));
		case 0x51:	return IntT((a & ~c) | (~a & ~b & ~c));
		case 0xa4:	return IntT((a & c) | (~a & b & ~c));
		case 0x19:	return IntT((~b & ~c) | (~a & b & c));
		case 0x26:	return IntT((~b & c) | (~a & b & ~c));

		case 0xc7:	return IntT((a & b) | (~a & (~b | ~c)));
		case 0x3d:	return IntT((a & ~b) | (~a & (b | ~c)));
		case 0xbc:	return IntT((~a & b) | (a & (~b | c)));
		case 0xe3:	return IntT((~a & ~b) | (a & (b | c)));
		case 0xa7:	return IntT((a & c) | (~a & (~b | ~c)));
		case 0x5b:	return IntT((a & ~c) | (~a & (~b | c)));
		case 0xda:	return IntT((~a & c) | (a & (b | ~c)));
		case 0xe5:	return IntT((~a & ~c) | (a & (b | c)));

		case 0x67:	return IntT((~a & ~b) | ((~a | b) & ~c) | (~b & c));
		case 0x97:	return IntT((~a & ~b) | ((~a | ~b) & ~c) | (a & b & c));

		case 0xb4:	return IntT((a & ~b) | (a & c) | (~a & b & ~c));
		case 0x9c:	return IntT((~a & b) | (b & c) | (a & ~b & ~c));

		case 0xd2:	return IntT(((~c | b) & a) | (~a & ~b & c));
		case 0x9a:	return IntT(((~a | b) & c) | (a & ~b & ~c));

		case 0xf9:	return IntT(a | IntT(~b & ~c) | (b & c));
		case 0xed:	return IntT(b | IntT(~a & ~c) | (a & c));
		case 0xeb:	return IntT(c | IntT(~a & ~b) | (a & b));
	}

	__builtin_unreachable();
}

}

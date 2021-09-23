//
//  Minterms.h
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Minterms_h
#define Minterms_h

namespace Amiga {

/// @returns the result of applying the Amiga-format @c minterm to inputs @c a, @c b and @c c.
template <typename IntT> IntT apply_minterm(IntT a, IntT b, IntT c, int minterm) {

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
		default:
		case 0x00:	return IntT(0);
		case 0xff:	return IntT(~0);


		case 0xf0:	return a;
		case 0xcc:	return b;
		case 0xaa:	return c;

		case 0x0f:	return ~a;
		case 0x33:	return ~b;
		case 0x55:	return ~c;

		case 0xfc:	return a | b;
		case 0xfa:	return a | c;
		case 0xee:	return b | c;
		case 0xfe:	return a | b | c;

		case 0xf3:	return a | ~b;
		case 0xf5:	return a | ~c;
		case 0xdd:	return b | ~c;

		case 0xfd:	return a | b | ~c;
		case 0xfb:	return a | ~b | c;
		case 0xf7:	return a | ~b | ~c;

		case 0xcf:	return ~a | b;
		case 0xaf:	return ~a | c;
		case 0xbb:	return ~b | c;

		case 0xef:	return ~a | b | c;
		case 0xdf:	return ~a | b | ~c;
		case 0x7f:	return ~a | ~b | ~c;


		case 0x3c:	return a ^ b;
		case 0x5a:	return a ^ c;
		case 0x66:	return b ^ c;
		case 0x96:	return a ^ b ^ c;

		case 0xc3:	return ~a ^ b;
		case 0xa5:	return ~a ^ c;
		case 0x99:	return ~b ^ c;
		case 0x69:	return ~a ^ b ^ c;


		case 0xc0:	return a & b;
		case 0xa0:	return a & c;
		case 0x88:	return b & c;
		case 0x80:	return a & b & c;

		case 0x30:	return a & ~b;
		case 0x50:	return a & ~c;
		case 0x44:	return b & ~c;

		case 0x0c:	return ~a & b;
		case 0x0a:	return ~a & c;
		case 0x22:	return ~b & c;

		case 0x40:	return a & b & ~c;
		case 0x20:	return a & ~b & c;
		case 0x08:	return ~a & b & c;

		case 0x10:	return a & ~b & ~c;
		case 0x04:	return ~a & b & ~c;
		case 0x02:	return ~a & ~b & c;

		case 0x03:	return ~a & ~b;
		case 0x05:	return ~a & ~c;
		case 0x11:	return ~b & ~c;
		case 0x01:	return ~a & ~b & ~c;

		case 0x70:	return a & ~(b & c);
		case 0x4c:	return b & ~(a & c);
		case 0x2a:	return c & ~(a & b);

		case 0x07:	return ~a & ~(b & c);
		case 0x13:	return ~b & ~(a & c);
		case 0x15:	return ~c & ~(a & b);


		case 0xe0:	return a & (b | c);
		case 0xc8:	return b & (a | c);
		case 0xa8:	return c & (a | b);

		case 0x0e:	return ~a & (b | c);
		case 0x32:	return ~b & (a | c);
		case 0x54:	return ~c & (a | b);

		case 0x60:	return a & (b ^ c);
		case 0x48:	return b & (a ^ c);
		case 0x28:	return c & (a ^ b);

		case 0x06:	return ~a & (b ^ c);
		case 0x12:	return ~b & (a ^ c);
		case 0x14:	return ~c & (a ^ b);

		case 0x90:	return a & ~(b ^ c);
		case 0x84:	return b & ~(a ^ c);
		case 0x82:	return c & ~(a ^ b);

		case 0x09:	return ~a & ~(b ^ c);
		case 0x21:	return ~b & ~(a ^ c);
		case 0x41:	return ~c & ~(a ^ b);

		case 0xb0:	return a & (~b | c);
		case 0xd0:	return a & (b | ~c);
		case 0x0b:	return ~a & (~b | c);
		case 0x0d:	return ~a & (b | ~c);


		case 0xf6:	return a | (b ^ c);
		case 0xde:	return b | (a ^ c);
		case 0xbe:	return c | (a ^ b);

		case 0x6f:	return ~a | (b ^ c);
		case 0x7b:	return ~b | (a ^ c);
		case 0x7d:	return ~c | (a ^ b);

		case 0x9f:	return ~a | ~(b ^ c);
		case 0xb7:	return ~b | ~(a ^ c);
		case 0xd7:	return ~c | ~(a ^ b);

		case 0xf8:	return a | (b & c);
		case 0xec:	return b | (a & c);
		case 0xea:	return c | (a & b);

		case 0x8f:	return ~a | (b & c);
		case 0xb3:	return ~b | (a & c);
		case 0xd5:	return ~c | (a & b);

		case 0xf1:	return a | ~(b | c);
		case 0xcd:	return b | ~(a | c);
		case 0xab:	return c | ~(a | b);

		case 0x1f:	return ~a | ~(b | c);
		case 0x37:	return ~b | ~(a | c);
		case 0x57:	return ~c | ~(a | b);

		case 0x8c:	return b & (~a | c);
		case 0x8a:	return c & (~a | b);

		case 0xc4:	return b & (a | ~c);
		case 0xa2:	return c & (a | ~b);


		case 0x78:	return a ^ (b & c);
		case 0x6c:	return b ^ (a & c);
		case 0x6a:	return c ^ (a & b);

		case 0x87:	return ~a ^ (b & c);
		case 0x93:	return ~b ^ (a & c);
		case 0x95:	return ~c ^ (a & b);


		case 0x1e:	return a ^ (b | c);
		case 0x36:	return b ^ (a | c);
		case 0x56:	return c ^ (a | b);

		case 0x2d:	return a ^ (b | ~c);
		case 0x4b:	return a ^ (~b | c);
		case 0xe1:	return a ^ ~(b | c);

		case 0x39:	return b ^ (a | ~c);
		case 0x63:	return b ^ (~a | c);
		case 0xc9:	return b ^ ~(a | c);

		case 0x59:	return c ^ (a | ~b);
		case 0x65:	return c ^ (~a | b);
		case 0xa9:	return c ^ ~(a | b);


		case 0x24:	return (a ^ b) & (b ^ c);
		case 0x18:	return (a ^ b) & (a ^ c);
		case 0x42:	return (a ^ c) & (b ^ c);

		case 0xa6:	return (a & b) ^ (b ^ c);
		case 0xc6:	return (a & c) ^ (b ^ c);

		case 0x5c:	return (a | b) ^ (a & c);
		case 0x74:	return (a | b) ^ (b & c);
		case 0x72:	return (a | c) ^ (b & c);
		case 0x4e:	return (b | c) ^ (a & c);

		case 0x58:	return (a | b) & (a ^ c);
		case 0x62:	return (a | c) & (b ^ c);

		case 0x7e:	return (a ^ b) | (a ^ c);

		case 0xca:	return (a & b) | (~a & c);
		case 0xac:	return (~a & b) | (a & c);
		case 0xa3:	return (~a & ~b) | (a & c);


		case 0xf4:	return a | ((a ^ b) & (b ^ c));
		case 0xf2:	return a | ((a ^ c) & (b ^ c));
		case 0xdc:	return b | ((a ^ b) & (a ^ c));
		case 0xce:	return b | ((a ^ c) & (b ^ c));
		case 0xae:	return c | ((a ^ b) & (b ^ c));
		case 0xba:	return c | ((a ^ b) & (a ^ c));

		case 0x2f:	return ~a | ((a ^ b) & (b ^ c));
		case 0x4f:	return ~a | ((a ^ c) & (b ^ c));
		case 0x3b:	return ~b | ((a ^ b) & (a ^ c));
		case 0x73:	return ~b | ((a ^ c) & (b ^ c));
		case 0x75:	return ~c | ((a ^ b) & (b ^ c));
		case 0x5d:	return ~c | ((a ^ b) & (a ^ c));

		case 0x3f:	return ~a | ~b | ((a ^ b) & (b ^ c));
		case 0x77:	return ~b | ~c | ((a ^ b) & (b ^ c));

		case 0x27:	return ~(a | b) | ((a ^ b) & (b ^ c));
		case 0x47:	return ~(a | c) | ((a ^ c) & (b ^ c));
		case 0x53:	return ~(b | c) | ((a ^ c) & (b ^ c));
		case 0x43:	return ~(a | b | c) | ((a ^ c) & (b ^ c));


		case 0x7a:	return (a & ~b) | (a ^ c);
		case 0x76:	return (a & ~b) | (b ^ c);
		case 0x7c:	return (a & ~c) | (a ^ b);

		case 0x5e:	return (~a & b) | (a ^ c);
		case 0x6e:	return (~a & b) | (b ^ c);
		case 0x3e:	return (~a & c) | (a ^ b);

		case 0xad:	return (~a & b) | ~(a ^ c);
		case 0xb5:	return (a & ~b) | ~(a ^ c);
		case 0xcb:	return (~a & c) | ~(a ^ b);
		case 0xd3:	return (a & ~c) | ~(a ^ b);

		case 0x9b:	return (~a & c) | ~(b ^ c);
		case 0xd9:	return (a & ~c) | ~(b ^ c);
		case 0x9d:	return (~a & b) | ~(b ^ c);
		case 0xb9:	return (a & ~b) | ~(b ^ c);

		case 0x9e:	return (~a & b) | (a ^ b ^ c);
		case 0xb6:	return (a & ~b) | (a ^ b ^ c);
		case 0xd6:	return (a & ~c) | (a ^ b ^ c);
		case 0xbf:	return ~(a & b) | (a ^ b ^ c);

		case 0x6d:	return (~a & b) | ~(a ^ b ^ c);
		case 0x79:	return (a & ~b) | ~(a ^ b ^ c);
		case 0x6b:	return (~a & c) | ~(a ^ b ^ c);
		case 0xe9:	return (b & c) | ~(a ^ b ^ c);

		case 0xb8:	return (a & ~b) | (c & b);
		case 0xd8:	return (a & ~c) | (b & c);
		case 0xe4:	return (b & ~c) | (a & c);
		case 0xe2:	return (c & ~b) | (a & b);


		case 0x2c:	return (~a & b) | ((a ^ b) & (b ^ c));
		case 0x34:	return (a & ~b) | ((a ^ b) & (b ^ c));
		case 0x4a:	return (~a & c) | ((a ^ c) & (b ^ c));
		case 0x52:	return (a & ~c) | ((a ^ c) & (b ^ c));
		case 0x5f:	return ~(a & c) | ((a ^ c) & (b ^ c));


		case 0x16:	return (a & ~(c | b)) | (c & ~(b | a)) | (b & ~(a | c));
		case 0x81:	return (a ^ ~(c | b)) & (c ^ ~(b | a)) & (b ^ ~(a | c));


		case 0x2e:	return (~a & (b | c)) | (~b & c);
		case 0x3a:	return (~b & (a | c)) | (~a & c);

		case 0x8b:	return (~a & ~b) | (c & b);
		case 0x8d:	return (~a & ~c) | (b & c);
		case 0xb1:	return (~b & ~c) | (a & c);
		case 0xd1:	return (~c & ~b) | (a & b);


		case 0x98:	return (a & ~(c | b)) | (b & c);
		case 0x8e:	return (~a & (c | b)) | (b & c);

		case 0x46:	return (~a | b) & (b ^ c);

		case 0xe6:	return ((~a | b) & (b ^ c)) ^ (a & c);
		case 0xc2:	return ((a | ~b) & (b ^ c)) ^ (a & c);

		case 0x85:	return (~a | b) & ~(a ^ c);
		case 0x83:	return (~a | c) & ~(a ^ b);
		case 0x89:	return (~a | c) & ~(b ^ c);

		case 0xa1:	return (a | ~b) & ~(a ^ c);
		case 0x91:	return (a | ~b) & ~(b ^ c);
		case 0xc1:	return (a | ~c) & ~(a ^ b);

		case 0x94:	return (a | b) & (a ^ b ^ c);
		case 0x86:	return (b | c) & (a ^ b ^ c);
		case 0x92:	return (a | c) & (a ^ b ^ c);

		case 0x68:	return (a | b) & ~(a ^ b ^ c);
		case 0x61:	return (a | ~b) & ~(a ^ b ^ c);
		case 0x49:	return (~a | b) & ~(a ^ b ^ c);
		case 0x29:	return (~a | c) & ~(a ^ b ^ c);

		case 0x64:	return (a & ~b & c) | (b & ~c);

		//
		// From here downwards functions were found automatically.
		// Neater versions likely exist of many of the functions below.
		//

		case 0xe8:	return (a & b) | ((b | a) & c);
		case 0xd4:	return (a & b) | ((b | a) & ~c);
		case 0xb2:	return (a & ~b) | ((~b | a) & c);
		case 0x17:	return (~a & ~b) | ((~b | ~a) & ~c);
		case 0x1b:	return (~a & ~b) | (~b & ~c) | (~a & c);
		case 0x1d:	return (~a & b) | ((~b | ~a) & ~c);
		case 0x2b:	return (~a & ~b) | ((~b | ~a) & c);
		case 0x35:	return (a & ~b) | ((~b | ~a) & ~c);
		case 0x4d:	return (~a & b) | ((b | ~a) & ~c);
		case 0x71:	return (a & ~b) | ((~b | a) & ~c);
		case 0xbd:	return (~a & b) | (~b & ~c) | (a & c);
		case 0xc5:	return (a & b) | ((b | ~a) & ~c);
		case 0xdb:	return (a & b) | (~b & ~c) | (~a & c);
		case 0xe7:	return (~a & ~b) | (b & ~c) | (a & c);


		case 0x1c:	return (~a & b) | (a & ~b & ~c);
		case 0x23:	return (~a & ~b) | (a & ~b & c);
		case 0x31:	return (a & ~b) | (~a & ~b & ~c);
		case 0x38:	return (a & ~b) | (~a & b & c);
		case 0x1a:	return (~a & c) | (a & ~b & ~c);
		case 0x25:	return (~a & ~c) | (a & ~b & c);
		case 0x45:	return (~a & ~c) | (a & b & ~c);
		case 0x51:	return (a & ~c) | (~a & ~b & ~c);
		case 0xa4:	return (a & c) | (~a & b & ~c);
		case 0x19:	return (~b & ~c) | (~a & b & c);
		case 0x26:	return (~b & c) | (~a & b & ~c);

		case 0xc7:	return (a & b) | (~a & (~b | ~c));
		case 0x3d:	return (a & ~b) | (~a & (b | ~c));
		case 0xbc:	return (~a & b) | (a & (~b | c));
		case 0xe3:	return (~a & ~b) | (a & (b | c));
		case 0xa7:	return (a & c) | (~a & (~b | ~c));
		case 0x5b:	return (a & ~c) | (~a & (~b | c));
		case 0xda:	return (~a & c) | (a & (b | ~c));
		case 0xe5:	return (~a & ~c) | (a & (b | c));

		case 0x67:	return (~a & ~b) | ((~a | b) & ~c) | (~b & c);
		case 0x97:	return (~a & ~b) | ((~a | ~b) & ~c) | (a & b & c);

		case 0xb4:	return (a & ~b) | (a & c) | (~a & b & ~c);
		case 0x9c:	return (~a & b) | (b & c) | (a & ~b & ~c);

		case 0xd2:	return ((~c | b) & a) | (~a & ~b & c);
		case 0x9a:	return ((~a | b) & c) | (a & ~b & ~c);

		case 0xf9:	return a | (~b & ~c) | (b & c);
		case 0xed:	return b | (~a & ~c) | (a & c);
		case 0xeb:	return c | (~a & ~b) | (a & b);
	}

	// Should be unreachable.
	return 0;
}

}

#endif /* Minterms_h */

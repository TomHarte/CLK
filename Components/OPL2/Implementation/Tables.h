//
//  Tables.h
//  Clock Signal
//
//  Created by Thomas Harte on 15/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Tables_h
#define Tables_h

namespace Yamaha {
namespace OPL {

/*
	These are the OPL's built-in log-sin and exponentiation tables, as recovered by
	Matthew Gambrell and Olli Niemitalo in 'OPLx decapsulated'. Despite the formulas
	being well known, I've elected not to generate these at runtime because even if I
	did, I'd just end up with the proper values laid out in full in a unit test, and
	they're very compact.
*/


struct LogSin {
	int logsin;
	int sign;
};
/*!
	@returns Negative log sin of x, assuming a 1024-unit circle.
*/
constexpr LogSin negative_log_sin(int x) {
	/// Defines the first quadrant of 1024-unit negative log to the base two of  sine (that conveniently misses sin(0)).
	///
	/// Expected branchless usage for a full 1024 unit output:
	///
	///	constexpr int multiplier[] = { 1, -1 };
	///	constexpr int mask[] = { 0, 255 };
	///
	/// value = exp( log_sin[angle & 255] ^ mask[(angle >> 8) & 1]) * multitplier[(angle >> 9) & 1]
	///
	/// ... where exp(x) = 2 ^ -x / 256
	constexpr int16_t log_sin[] = {
		2137,	1731,	1543,	1419,	1326,	1252,	1190,	1137,
		1091,	1050,	1013,	979,	949,	920,	894,	869,
		846,	825,	804,	785,	767,	749,	732,	717,
		701,	687,	672,	659,	646,	633,	621,	609,
		598,	587,	576,	566,	556,	546,	536,	527,
		518,	509,	501,	492,	484,	476,	468,	461,
		453,	446,	439,	432,	425,	418,	411,	405,
		399,	392,	386,	380,	375,	369,	363,	358,
		352,	347,	341,	336,	331,	326,	321,	316,
		311,	307,	302,	297,	293,	289,	284,	280,
		276,	271,	267,	263,	259,	255,	251,	248,
		244,	240,	236,	233,	229,	226,	222,	219,
		215,	212,	209,	205,	202,	199,	196,	193,
		190,	187,	184,	181,	178,	175,	172,	169,
		167,	164,	161,	159,	156,	153,	151,	148,
		146,	143,	141,	138,	136,	134,	131,	129,
		127,	125,	122,	120,	118,	116,	114,	112,
		110,	108,	106,	104,	102,	100,	98,		96,
		94,		92,		91,		89,		87,		85,		83,		82,
		80,		78,		77,		75,		74,		72,		70,		69,
		67,		66,		64,		63,		62,		60,		59,		57,
		56,		55,		53,		52,		51,		49,		48,		47,
		46,		45,		43,		42,		41,		40,		39,		38,
		37,		36,		35,		34,		33,		32,		31,		30,
		29,		28,		27,		26,		25,		24,		23,		23,
		22,		21,		20,		20,		19,		18,		17,		17,
		16,		15,		15,		14,		13,		13,		12,		12,
		11,		10,		10,		9,		9,		8,		8,		7,
		7,		7,		6,		6,		5,		5,		5,		4,
		4,		4,		3,		3,		3,		2,		2,		2,
		2,		1,		1,		1,		1,		1,		1,		1,
		0,		0,		0,		0,		0,		0,		0,		0
	};
	constexpr int16_t sign[] = { 1, -1 };
	constexpr int16_t mask[] = { 0, 255 };

	return {
		.logsin = log_sin[x & 255] ^ mask[(x >> 8) & 1],
		.sign = sign[(x >> 9) & 1]
	};
}

/*!
	@returns 2 ^ -x/256 in 0.10 fixed-point form.
*/
constexpr int power_two(int x) {
	/// A derivative of the exponent table in a real OPL2; mapped_exp[x] = (source[c ^ 0xff] << 1) | 0x800.
	///
	/// The ahead-of-time transformation represents fixed work the OPL2 does when reading its table
	/// independent on the input.
	///
	/// The original table is a 0.10 fixed-point representation of 2^x - 1 with bit 10 implicitly set, where x is
	/// in 0.8 fixed point.
	///
	/// Since the log_sin table represents sine in a negative base-2 logarithm, values from it would need
	/// to be negatived before being put into the original table. That's haned with the ^ 0xff. The | 0x800 is to
	/// set the implicit bit 10 (subject to the shift).
	///
	/// The shift by 1 is to allow the chip's exploitation of the recursive symmetry of the exponential table to
	/// be achieved more easily. Specifically, to convert a logarithmic attenuation to a linear one, just perform:
	///
	///	result = mapped_exp[x & 0xff] >> (x >> 8)
	constexpr int mapped_exp[] = {
		4084,	4074,	4062,	4052,	4040,	4030,	4020,	4008,
		3998,	3986,	3976,	3966,	3954,	3944,	3932,	3922,
		3912,	3902,	3890,	3880,	3870,	3860,	3848,	3838,
		3828,	3818,	3808,	3796,	3786,	3776,	3766,	3756,
		3746,	3736,	3726,	3716,	3706,	3696,	3686,	3676,
		3666,	3656,	3646,	3636,	3626,	3616,	3606,	3596,
		3588,	3578,	3568,	3558,	3548,	3538,	3530,	3520,
		3510,	3500,	3492,	3482,	3472,	3464,	3454,	3444,
		3434,	3426,	3416,	3408,	3398,	3388,	3380,	3370,
		3362,	3352,	3344,	3334,	3326,	3316,	3308,	3298,
		3290,	3280,	3272,	3262,	3254,	3246,	3236,	3228,
		3218,	3210,	3202,	3192,	3184,	3176,	3168,	3158,
		3150,	3142,	3132,	3124,	3116,	3108,	3100,	3090,
		3082,	3074,	3066,	3058,	3050,	3040,	3032,	3024,
		3016,	3008,	3000,	2992,	2984,	2976,	2968,	2960,
		2952,	2944,	2936,	2928,	2920,	2912,	2904,	2896,
		2888,	2880,	2872,	2866,	2858,	2850,	2842,	2834,
		2826,	2818,	2812,	2804,	2796,	2788,	2782,	2774,
		2766,	2758,	2752,	2744,	2736,	2728,	2722,	2714,
		2706,	2700,	2692,	2684,	2678,	2670,	2664,	2656,
		2648,	2642,	2634,	2628,	2620,	2614,	2606,	2600,
		2592,	2584,	2578,	2572,	2564,	2558,	2550,	2544,
		2536,	2530,	2522,	2516,	2510,	2502,	2496,	2488,
		2482,	2476,	2468,	2462,	2456,	2448,	2442,	2436,
		2428,	2422,	2416,	2410,	2402,	2396,	2390,	2384,
		2376,	2370,	2364,	2358,	2352,	2344,	2338,	2332,
		2326,	2320,	2314,	2308,	2300,	2294,	2288,	2282,
		2276,	2270,	2264,	2258,	2252,	2246,	2240,	2234,
		2228,	2222,	2216,	2210,	2204,	2198,	2192,	2186,
		2180,	2174,	2168,	2162,	2156,	2150,	2144,	2138,
		2132,	2128,	2122,	2116,	2110,	2104,	2098,	2092,
		2088,	2082,	2076,	2070,	2064,	2060,	2054,	2048,
	};

	return mapped_exp[x & 0xff] >> (x >> 8);
}

/*

	Credit for the fixed register lists goes to Nuke.YKT; I found them at:
	https://siliconpr0n.org/archive/doku.php?id=vendor:yamaha:opl2#ym2413_instrument_rom

	The arrays below begin with channel 1, then each line is a single channel defined
	in exactly the same terms as the OPL's user-defined channel.

*/

constexpr uint8_t opll_patch_set[] = {
	0x71, 0x61, 0x1e, 0x17, 0xd0, 0x78, 0x00, 0x17,
	0x13, 0x41, 0x1a, 0x0d, 0xd8, 0xf7, 0x23, 0x13,
	0x13, 0x01, 0x99, 0x00, 0xf2, 0xc4, 0x11, 0x23,
	0x31, 0x61, 0x0e, 0x07, 0xa8, 0x64, 0x70, 0x27,
	0x32, 0x21, 0x1e, 0x06, 0xe0, 0x76, 0x00, 0x28,
	0x31, 0x22, 0x16, 0x05, 0xe0, 0x71, 0x00, 0x18,
	0x21, 0x61, 0x1d, 0x07, 0x82, 0x81, 0x10, 0x07,
	0x23, 0x21, 0x2d, 0x14, 0xa2, 0x72, 0x00, 0x07,
	0x61, 0x61, 0x1b, 0x06, 0x64, 0x65, 0x10, 0x17,
	0x41, 0x61, 0x0b, 0x18, 0x85, 0xf7, 0x71, 0x07,
	0x13, 0x01, 0x83, 0x11, 0xfa, 0xe4, 0x10, 0x04,
	0x17, 0xc1, 0x24, 0x07, 0xf8, 0xf8, 0x22, 0x12,
	0x61, 0x50, 0x0c, 0x05, 0xc2, 0xf5, 0x20, 0x42,
	0x01, 0x01, 0x55, 0x03, 0xc9, 0x95, 0x03, 0x02,
	0x61, 0x41, 0x89, 0x03, 0xf1, 0xe4, 0x40, 0x13,
};

constexpr uint8_t vrc7_patch_set[] = {
	0x03, 0x21, 0x05, 0x06, 0xe8, 0x81, 0x42, 0x27,
	0x13, 0x41, 0x14, 0x0d, 0xd8, 0xf6, 0x23, 0x12,
	0x11, 0x11, 0x08, 0x08, 0xfa, 0xb2, 0x20, 0x12,
	0x31, 0x61, 0x0c, 0x07, 0xa8, 0x64, 0x61, 0x27,
	0x32, 0x21, 0x1e, 0x06, 0xe1, 0x76, 0x01, 0x28,
	0x02, 0x01, 0x06, 0x00, 0xa3, 0xe2, 0xf4, 0xf4,
	0x21, 0x61, 0x1d, 0x07, 0x82, 0x81, 0x11, 0x07,
	0x23, 0x21, 0x22, 0x17, 0xa2, 0x72, 0x01, 0x17,
	0x35, 0x11, 0x25, 0x00, 0x40, 0x73, 0x72, 0x01,
	0xb5, 0x01, 0x0f, 0x0f, 0xa8, 0xa5, 0x51, 0x02,
	0x17, 0xc1, 0x24, 0x07, 0xf8, 0xf8, 0x22, 0x12,
	0x71, 0x23, 0x11, 0x06, 0x65, 0x74, 0x18, 0x16,
	0x01, 0x02, 0xd3, 0x05, 0xc9, 0x95, 0x03, 0x02,
	0x61, 0x63, 0x0c, 0x00, 0x94, 0xc0, 0x33, 0xf6,
	0x21, 0x72, 0x0d, 0x00, 0xc1, 0xd5, 0x56, 0x06,
};

constexpr uint8_t percussion_patch_set[] = {
	0x01, 0x01, 0x18, 0x0f, 0xdf, 0xf8, 0x6a, 0x6d,
	0x01, 0x01, 0x00, 0x00, 0xc8, 0xd8, 0xa7, 0x48,
	0x05, 0x01, 0x00, 0x00, 0xf8, 0xaa, 0x59, 0x55,
};

}
}

#endif /* Tables_h */

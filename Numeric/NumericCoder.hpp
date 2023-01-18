//
//  NumericCoder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef NumericCoder_hpp
#define NumericCoder_hpp

namespace Numeric {

/// Stores and retrieves an arbitrary number of fields into an int with arbitrary modulos.
///
/// E.g. NumericCoder<8, 3, 14> establishes an encoder and decoder for three fields,
/// the first is modulo 8, the second is modulo 3, the third is modulo 14.
///
/// NumericCoder<8, 3, 14>::encode<2>(v, 9) will mutate v so that the third field
/// (i.e. field 2) has value 9.
///
/// The first given field will occupy the least significant part of the target int; e.g.
/// a NumericCoder<3, 4, 6> with ::encode<0>(v, 2), ::encode<1>(v, 1) and
/// ::encode<2>(v, 5) will have a final value of:
///
///		[value] 2 +
///		[value] 1 * [product of previous field sizes] 3 +
///		5 * 12
///				= 65
///
template <int... Sizes> class NumericCoder {
	public:
		/// Modifies @c target to hold @c value at @c index.
		template <int index> static void encode(int &target, int value) {
			static_assert(index < sizeof...(Sizes), "Index must be within range");
			NumericEncoder<Sizes...>::template encode<index>(target, value);
		}

		/// @returns The value from @c source at @c index.
		template <int index> static int decode(int source) {
			static_assert(index < sizeof...(Sizes), "Index must be within range");
			return NumericDecoder<Sizes...>::template decode<index>(source);
		}

	private:

		template <int size, int... Tail>
		struct NumericEncoder {
			template <int index, int i = 0, int divider = 1> static void encode(int &target, int value) {
				if constexpr (i == index) {
					const int suffix = target % divider;
					target /= divider;
					target -= target % size;
					target += value % size;
					target *= divider;
					target += suffix;
				} else {
					NumericEncoder<Tail...>::template encode<index, i+1, divider*size>(target, value);
				}
			}
		};

		template <int size, int... Tail>
		struct NumericDecoder {
			template <int index, int i = 0, int divider = 1> static int decode(int source) {
				if constexpr (i == index) {
					return (source / divider) % size;
				} else {
					return NumericDecoder<Tail...>::template decode<index, i+1, divider*size>(source);
				}
			}
		};
};

}

#endif /* NumericCoder_hpp */

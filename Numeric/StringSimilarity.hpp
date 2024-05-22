//
//  StringSimilarity.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/05/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#ifndef StringSimilarity_hpp
#define StringSimilarity_hpp

#include <cstdint>
#include <set>
#include <string>

namespace Numeric {

/// Seeks to implement algorithm as per http://www.catalysoft.com/articles/StrikeAMatch.html
///
/// @returns A number in the range 0.0 to 1.0 indicating the similarity between two strings;
/// 1.0 is most similar, 0.0 is least.
double similarity(std::string_view first, std::string_view second) {
	if(first.size() < 2 || second.size() < 2) {
		return 0.0;
	}

	const auto pairs = [](std::string_view source) -> std::set<uint16_t> {
		std::set<uint16_t> result;
		for(std::size_t c = 0; c < source.size() - 1; c++) {
			if(isalpha(source[c]) && isalpha(source[c+1])) {
				result.insert(static_cast<uint16_t>(
					(toupper(source[c]) << 8) |
					toupper(source[c+1])
				));
			}
		}
		return result;
	};

	const auto first_pairs = pairs(first);
	const auto second_pairs = pairs(second);

	const auto denominator = static_cast<double>(first_pairs.size() + second_pairs.size());

	std::size_t numerator = 0;
	auto first_it = first_pairs.begin();
	auto second_it = second_pairs.begin();
	while(first_it != first_pairs.end() && second_it != second_pairs.end()) {
		if(*first_it == *second_it) {
			++numerator;
			++first_it;
			++second_it;
		} else if(*first_it < *second_it) {
			++first_it;
		} else {
			++second_it;
		}
	}

	return static_cast<double>(numerator * 2) / denominator;
}

}

#endif /* StringSimilarity_h */

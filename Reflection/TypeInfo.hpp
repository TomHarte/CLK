//
//  TypeInfo.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/06/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef TypeInfo_hpp
#define TypeInfo_hpp

#define ForAllInts(x)	\
	x(uint8_t);			\
	x(int8_t);			\
	x(uint16_t);		\
	x(int16_t);			\
	x(uint32_t);		\
	x(int32_t);			\
	x(uint64_t);		\
	x(int64_t);

#define ForAllFloats(x)	\
	x(float);			\
	x(double);

namespace TypeInfo {

inline bool is_integral(const std::type_info *type) {
	return
		*type == typeid(uint8_t) || *type == typeid(int8_t) ||
		*type == typeid(uint16_t) || *type == typeid(int16_t) ||
		*type == typeid(uint32_t) || *type == typeid(int32_t) ||
		*type == typeid(uint64_t) || *type == typeid(int64_t);
}

inline bool is_floating_point(const std::type_info *type) {
	return *type == typeid(float) || *type == typeid(double);
}

inline bool is_signed(const std::type_info *type) {
	return
		*type == typeid(int8_t) ||
		*type == typeid(int16_t) ||
		*type == typeid(int32_t) ||
		*type == typeid(int64_t) ||
		*type == typeid(double) ||
		*type == typeid(float);
}

inline size_t size(const std::type_info *type) {
#define TestType(x)	if(*type == typeid(x)) return sizeof(x);
	ForAllInts(TestType);
	ForAllFloats(TestType);
	TestType(char *);
#undef TestType

	// This is some sort of struct or object type.
	return 0;
}

}


#endif /* TypeInfo_hpp */

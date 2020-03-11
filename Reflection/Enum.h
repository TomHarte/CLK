//
//  Enum.h
//  Clock Signal
//
//  Created by Thomas Harte on 17/02/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Enum_h
#define Enum_h

#include <cctype>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <unordered_map>

namespace Reflection {

#define ReflectableEnum(Name, Type, ...)	\
	enum class Name: Type { Mac128k, Mac512k, Mac512ke, MacPlus };	\
	constexpr static const char *__declaration##Name = #__VA_ARGS__;

#define EnumDeclaration(Name) __declaration##Name

class Enum {
	public:
		template <typename Type> static void declare(const char *declaration) {
			const char *d_ptr = declaration;

			std::vector<std::string> result;
			while(true) {
				// Skip non-alphas, and exit if the terminator is found.
				while(*d_ptr && !isalpha(*d_ptr)) ++d_ptr;
				if(!*d_ptr) break;

				// Note the current location and proceed for all alphas and digits.
				const auto start = d_ptr;
				while(isalpha(*d_ptr) || isdigit(*d_ptr)) ++d_ptr;

				// Add a string view.
				result.emplace_back(std::string(start, size_t(d_ptr - start)));
			}

			members_by_type_.emplace(std::make_pair(&typeid(Type), result));
		}

		template <typename Type> size_t size() {
			return size(typeid(Type));
		}

		size_t size(const std::type_info &type) {
			const auto entry = members_by_type_.find(&type);
			if(entry == members_by_type_.end()) return 0;
			return entry->second.size();
		}

		/*!
			@returns A @c string_view name for the enum value @c e.
		*/
		template <typename EnumType> static const std::string &toString(EnumType e) {
			const auto entry = members_by_type_.find(&typeid(EnumType));
			if(entry == members_by_type_.end()) return empty_string_;
			return entry->second[size_t(e)];
		}

		/*!
			@returns A value of @c EnumType name for the name @c str, or @c EnumType(-1) if
				the string is not found.
		*/
		template <typename Type> Type fromString(const std::string &str) {
			return Type(fromString(typeid(Type), str));
		}

		size_t fromString(const std::type_info &type, const std::string &str) {
			const auto entry = members_by_type_.find(&type);
			if(entry == members_by_type_.end()) return 0;
			const auto iterator = std::find(entry->second.begin(), entry->second.end(), str);
			if(iterator == entry->second.end()) return 0;
			return size_t(iterator - entry->second.begin());
		}

	private:
		static inline std::unordered_map<const std::type_info *, std::vector<std::string>> members_by_type_;
		static inline const std::string empty_string_;
};

}

#endif /* Enum_h */

//
//  Struct.h
//  Clock Signal
//
//  Created by Thomas Harte on 06/03/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Struct_h
#define Struct_h

#include <any>
#include <string>
#include <unordered_map>
#include <vector>

namespace Reflection {

class Struct {
	public:
		template <typename Type> const Type &get(const std::string &name) {
			return *std::any_cast<Type *>(contents_[name]);
		}

		template <typename Type> void set(const std::string &name, const Type &value) {
			*std::any_cast<Type *>(contents_[name]) = value;
		}

		const std::type_info &type_of(const std::string &name) {
			return contents_[name].type();
		}

		std::vector<std::string> all_keys() {
			std::vector<std::string> keys;
			for(const auto &pair: contents_) {
				keys.push_back(pair.first);
			}
			return keys;
		}

	protected:
		template <typename Type> void declare(Type *t, const std::string &name) {
			contents_[name] = t;
		}

	private:
		std::unordered_map<std::string, std::any> contents_;
};

}

#endif /* Struct_h */

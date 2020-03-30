//
//  MultiConfigurable.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MultiConfigurable.hpp"

#include <algorithm>

using namespace Analyser::Dynamic;

namespace {

class MultiStruct: public Reflection::Struct {
	public:
		MultiStruct(const std::vector<Configurable::Device *> &devices) : devices_(devices) {
			for(auto device: devices) {
				options_.emplace_back(device->get_options());
			}
		}

		void apply() {
			auto options = options_.begin();
			for(auto device: devices_) {
				device->set_options(*options);
				++options;
			}
		}

		std::vector<std::string> all_keys() const final {
			std::set<std::string> keys;
			for(auto &options: options_) {
				const auto new_keys = options->all_keys();
				keys.insert(new_keys.begin(), new_keys.end());
			}
			return std::vector<std::string>(keys.begin(), keys.end());
		}

		std::vector<std::string> values_for(const std::string &name) const final {
			std::set<std::string> values;
			for(auto &options: options_) {
				const auto new_values = options->values_for(name);
				values.insert(new_values.begin(), new_values.end());
			}
			return std::vector<std::string>(values.begin(), values.end());
		}

		const std::type_info *type_of(const std::string &name) const final {
			for(auto &options: options_) {
				auto info = options->type_of(name);
				if(info) return info;
			}
			return nullptr;
		}

		const void *get(const std::string &name) const final {
			for(auto &options: options_) {
				auto value = options->get(name);
				if(value) return value;
			}
			return nullptr;
		}

		void set(const std::string &name, const void *value) final {
			const auto safe_type = type_of(name);
			if(!safe_type) return;

			// Set this property only where the child's type is the same as that
			// which was returned from here for type_of.
			for(auto &options: options_) {
				const auto type = options->type_of(name);
				if(!type) continue;

				if(*type == *safe_type) {
					options->set(name, value);
				}
			}
		}

	private:
		const std::vector<Configurable::Device *> &devices_;
		std::vector<std::unique_ptr<Reflection::Struct>> options_;
};

}

MultiConfigurable::MultiConfigurable(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	for(const auto &machine: machines) {
		Configurable::Device *device = machine->configurable_device();
		if(device) devices_.push_back(device);
	}
}

void MultiConfigurable::set_options(const std::unique_ptr<Reflection::Struct> &str) {
	const auto options = dynamic_cast<MultiStruct *>(str.get());
	options->apply();
}

std::unique_ptr<Reflection::Struct> MultiConfigurable::get_options() {
	return std::make_unique<MultiStruct>(devices_);
}

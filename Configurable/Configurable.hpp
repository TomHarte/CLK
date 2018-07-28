//
//  Configurable.h
//  Clock Signal
//
//  Created by Thomas Harte on 17/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Configurable_h
#define Configurable_h

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Configurable {

/*!
	The Option class hierarchy provides a way for components, machines, etc, to provide a named
	list of typed options to which they can respond.
*/
struct Option {
	std::string long_name;
	std::string short_name;
	virtual ~Option() {}

	Option(const std::string &long_name, const std::string &short_name) : long_name(long_name), short_name(short_name) {}

	virtual bool operator==(const Option &rhs) {
		return long_name == rhs.long_name && short_name == rhs.short_name;
	}
};

struct BooleanOption: public Option {
	BooleanOption(const std::string &long_name, const std::string &short_name) : Option(long_name, short_name) {}
};

struct ListOption: public Option {
	std::vector<std::string> options;
	ListOption(const std::string &long_name, const std::string &short_name, const std::vector<std::string> &options) : Option(long_name, short_name), options(options) {}

	virtual bool operator==(const Option &rhs) {
		const ListOption *list_rhs = dynamic_cast<const ListOption *>(&rhs);
		if(!list_rhs) return false;
		return long_name == rhs.long_name && short_name == rhs.short_name && options == list_rhs->options;
	}
};

struct BooleanSelection;
struct ListSelection;

/*!
	Selections are responses to Options.
*/
struct Selection {
	virtual ~Selection() {}
	virtual ListSelection *list_selection() = 0;
	virtual BooleanSelection *boolean_selection() = 0;
};

struct BooleanSelection: public Selection {
	bool value;

	ListSelection *list_selection();
	BooleanSelection *boolean_selection();
	BooleanSelection(bool value) : value(value) {}
};

struct ListSelection: public Selection {
	std::string value;

	ListSelection *list_selection();
	BooleanSelection *boolean_selection();
	ListSelection(const std::string value) : value(value) {}
};

using SelectionSet = std::map<std::string, std::unique_ptr<Selection>>;

/*!
	A Configuratble provides the options that it responds to and allows selections to be set.
*/
struct Device {
	virtual std::vector<std::unique_ptr<Option>> get_options() = 0;
	virtual void set_selections(const SelectionSet &selection_by_option) = 0;
	virtual SelectionSet get_accurate_selections() = 0;
	virtual SelectionSet get_user_friendly_selections() = 0;
};

template <typename T> T *selection(const Configurable::SelectionSet &selections_by_option, const std::string &name) {
	auto selection = selections_by_option.find(name);
	if(selection == selections_by_option.end()) return nullptr;
	return dynamic_cast<T *>(selection->second.get());
}

}

#endif /* Configurable_h */

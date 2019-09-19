//
//  StandardOptions.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "StandardOptions.hpp"

namespace {

/*!
	Appends a Boolean selection of @c selection for option @c name to @c selection_set.
*/
void append_bool(Configurable::SelectionSet &selection_set, const std::string &name, bool selection) {
	selection_set[name] = std::unique_ptr<Configurable::Selection>(new Configurable::BooleanSelection(selection));
}

/*!
	Enquires for a Boolean selection for option @c name from @c selections_by_option, storing it to @c result if found.
*/
bool get_bool(const Configurable::SelectionSet &selections_by_option, const std::string &name, bool &result) {
	auto quickload = Configurable::selection<Configurable::BooleanSelection>(selections_by_option, "quickload");
	if(!quickload) return false;
	result = quickload->value;
	return true;
}

}

// MARK: - Standard option list builder
std::vector<std::unique_ptr<Configurable::Option>> Configurable::standard_options(Configurable::StandardOptions mask) {
	std::vector<std::unique_ptr<Configurable::Option>> options;
	if(mask & QuickLoadTape)				options.emplace_back(new Configurable::BooleanOption("Load Tapes Quickly", "quickload"));
	if(mask & (DisplayRGB | DisplayCompositeColour | DisplayCompositeMonochrome | DisplaySVideo)) {
		std::vector<std::string> display_options;
		if(mask & DisplayCompositeColour)		display_options.emplace_back("composite");
		if(mask & DisplayCompositeMonochrome)	display_options.emplace_back("composite-mono");
		if(mask & DisplaySVideo)				display_options.emplace_back("svideo");
		if(mask & DisplayRGB)					display_options.emplace_back("rgb");
		options.emplace_back(new Configurable::ListOption("Display", "display", display_options));
	}
	if(mask & AutomaticTapeMotorControl)	options.emplace_back(new Configurable::BooleanOption("Automatic Tape Motor Control", "autotapemotor"));
	if(mask & QuickBoot)					options.emplace_back(new Configurable::BooleanOption("Boot Quickly", "quickboot"));
	return options;
}

// MARK: - Selection appenders
void Configurable::append_quick_load_tape_selection(Configurable::SelectionSet &selection_set, bool selection) {
	append_bool(selection_set, "quickload", selection);
}

void Configurable::append_automatic_tape_motor_control_selection(SelectionSet &selection_set, bool selection) {
	append_bool(selection_set, "autotapemotor", selection);
}

void Configurable::append_display_selection(Configurable::SelectionSet &selection_set, Display selection) {
	std::string string_selection;
	switch(selection) {
		default:
		case Display::RGB:					string_selection = "rgb";				break;
		case Display::SVideo:				string_selection = "svideo";			break;
		case Display::CompositeMonochrome:	string_selection = "composite-mono";	break;
		case Display::CompositeColour:		string_selection = "composite";			break;
	}
	selection_set["display"] = std::unique_ptr<Configurable::Selection>(new Configurable::ListSelection(string_selection));
}

void Configurable::append_quick_boot_selection(Configurable::SelectionSet &selection_set, bool selection) {
	append_bool(selection_set, "quickboot", selection);
}

// MARK: - Selection parsers
bool Configurable::get_quick_load_tape(const Configurable::SelectionSet &selections_by_option, bool &result) {
	return get_bool(selections_by_option, "quickload", result);
}

bool Configurable::get_automatic_tape_motor_control_selection(const SelectionSet &selections_by_option, bool &result) {
	return get_bool(selections_by_option, "autotapemotor", result);
}

bool Configurable::get_display(const Configurable::SelectionSet &selections_by_option, Configurable::Display &result) {
	auto display = Configurable::selection<Configurable::ListSelection>(selections_by_option, "display");
	if(display) {
		if(display->value == "rgb") {
			result = Configurable::Display::RGB;
			return true;
		}
		if(display->value == "svideo") {
			result = Configurable::Display::SVideo;
			return true;
		}
		if(display->value == "composite") {
			result = Configurable::Display::CompositeColour;
			return true;
		}
		if(display->value == "composite-mono") {
			result = Configurable::Display::CompositeMonochrome;
			return true;
		}
	}
	return false;
}

bool Configurable::get_quick_boot(const Configurable::SelectionSet &selections_by_option, bool &result) {
	return get_bool(selections_by_option, "quickboot", result);
}

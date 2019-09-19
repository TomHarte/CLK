//
//  StandardOptions.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef StandardOptions_hpp
#define StandardOptions_hpp

#include "Configurable.hpp"

namespace Configurable {

enum StandardOptions {
	DisplayRGB					= (1 << 0),
	DisplaySVideo				= (1 << 1),
	DisplayCompositeColour		= (1 << 2),
	DisplayCompositeMonochrome	= (1 << 3),
	QuickLoadTape				= (1 << 4),
	AutomaticTapeMotorControl	= (1 << 5),
	QuickBoot					= (1 << 6),
};

enum class Display {
	RGB,
	SVideo,
	CompositeColour,
	CompositeMonochrome
};

/*!
	@returns An option list comprised of the standard names for all the options indicated by @c mask.
*/
std::vector<std::unique_ptr<Option>> standard_options(StandardOptions mask);

/*!
	Appends to @c selection_set a selection of @c selection for QuickLoadTape.
*/
void append_quick_load_tape_selection(SelectionSet &selection_set, bool selection);

/*!
	Appends to @c selection_set a selection of @c selection for AutomaticTapeMotorControl.
*/
void append_automatic_tape_motor_control_selection(SelectionSet &selection_set, bool selection);

/*!
	Appends to @c selection_set a selection of @c selection for DisplayRGBComposite.
*/
void append_display_selection(SelectionSet &selection_set, Display selection);

/*!
	Appends to @c selection_set a selection of @c selection for QuickBoot.
*/
void append_quick_boot_selection(SelectionSet &selection_set, bool selection);

/*!
	Attempts to discern a QuickLoadTape selection from @c selections_by_option.
 
	@param selections_by_option The user selections.
	@param result The location to which the selection will be stored if found.
	@returns @c true if a selection is found; @c false otherwise.
*/
bool get_quick_load_tape(const SelectionSet &selections_by_option, bool &result);

/*!
	Attempts to discern an AutomaticTapeMotorControl selection from @c selections_by_option.
 
	@param selections_by_option The user selections.
	@param result The location to which the selection will be stored if found.
	@returns @c true if a selection is found; @c false otherwise.
*/
bool get_automatic_tape_motor_control_selection(const SelectionSet &selections_by_option, bool &result);

/*!
	Attempts to discern a display RGB/composite selection from @c selections_by_option.
 
	@param selections_by_option The user selections.
	@param result The location to which the selection will be stored if found.
	@returns @c true if a selection is found; @c false otherwise.
*/
bool get_display(const SelectionSet &selections_by_option, Display &result);

/*!
	Attempts to QuickBoot a QuickLoadTape selection from @c selections_by_option.

	@param selections_by_option The user selections.
	@param result The location to which the selection will be stored if found.
	@returns @c true if a selection is found; @c false otherwise.
*/
bool get_quick_boot(const SelectionSet &selections_by_option, bool &result);

}

#endif /* StandardOptions_hpp */

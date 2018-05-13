//
//  Configurable.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Configurable.hpp"

using namespace Configurable;

ListSelection *BooleanSelection::list_selection() {
	return new ListSelection(value ? "yes" : "no");
}

ListSelection *ListSelection::list_selection() {
	return new ListSelection(value);
}

BooleanSelection *ListSelection::boolean_selection() {
	return new BooleanSelection(value != "no" && value != "n");
}

BooleanSelection *BooleanSelection::boolean_selection() {
	return new BooleanSelection(value);
}

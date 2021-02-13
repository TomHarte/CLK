//
//  Mouse.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Mouse.hpp"

using namespace Apple::ADB;

Mouse::Mouse(Bus &bus) : ReactiveDevice(bus, 3) {}

void Mouse::perform_command(const Command &) {
}

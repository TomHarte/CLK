//
//  ROMFetcher.h
//  Clock Signal
//
//  Created by Thomas Harte on 01/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "ROMMachine.hpp"

ROMMachine::ROMFetcher CSROMFetcher(ROM::Request *missing = nullptr);

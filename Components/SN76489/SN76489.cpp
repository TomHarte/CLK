//
//  SN76489.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "SN76489.hpp"

using namespace TI;

SN76489::SN76489(Concurrency::DeferringAsyncTaskQueue &task_queue) : task_queue_(task_queue) {}

void SN76489::write(uint8_t value) {
}

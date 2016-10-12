//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Oric;

VideoOutput::VideoOutput(uint8_t *memory) : _ram(memory)
{
}

void VideoOutput::set_crt(std::shared_ptr<Outputs::CRT::CRT> crt)
{
}

void VideoOutput::run_for_cycles(int number_of_cycles)
{
}

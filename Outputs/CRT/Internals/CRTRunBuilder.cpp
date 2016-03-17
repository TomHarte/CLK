//
//  CRTFrameBuilder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"
#include "CRTOpenGL.hpp"

using namespace Outputs::CRT;

/*void CRTRunBuilder::reset()
{
	number_of_vertices = 0;
	uploaded_vertices = 0;
	duration = 0;
}

uint8_t *CRTRunBuilder::get_next_run(size_t number_of_vertices_in_run)
{
	// get a run from the allocated list, allocating more if we're about to overrun
	if((number_of_vertices + number_of_vertices_in_run) * _vertex_size >= _runs.size())
	{
		_runs.resize(_runs.size() + _vertex_size * 100);
	}

	uint8_t *next_run = &_runs[number_of_vertices * _vertex_size];
	number_of_vertices += number_of_vertices_in_run;

	return next_run;
}
*/
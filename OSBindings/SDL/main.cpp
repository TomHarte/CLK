//
//  main.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include <iostream>
#include <SDL2/SDL.h>

#include "../../StaticAnalyser/StaticAnalyser.hpp"

int main(int argc, char *argv[]) {
	SDL_Window *window = nullptr;

	if(argc < 2) {
		std::cerr << "Usage: " << argv[0] << " [file]" << std::endl;
		return -1;
	}
	
	std::list<StaticAnalyser::Target> targets = StaticAnalyser::GetTargets(argv[1]);
	if(targets.empty()) {
		std::cerr << "Cannot open " << argv[1] << std::endl;
		return -1;
	}

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
		return -1;
	}

	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	window = SDL_CreateWindow(	"Clock Signal",
								SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
								400, 300,
								SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	if(!window)
	{
		std::cerr << "Could not create window" << std::endl;
		return -1;
	}

	bool should_quit = false;
	while(!should_quit) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_QUIT:	should_quit = true;	break;
				
				case SDL_KEYDOWN:
				break;
				case SDL_KEYUP:
				break;
			}
		}
	}
	
	SDL_DestroyWindow( window );
	SDL_Quit();

	return 0;
}

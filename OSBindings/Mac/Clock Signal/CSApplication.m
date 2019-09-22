//
//  Application.m
//  Clock Signal
//
//  Created by Thomas Harte on 21/09/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import "CSApplication.h"

@implementation CSApplication

- (void)sendEvent:(NSEvent *)event {
	// The only reason to capture events here rather than at the window or view
	// is to divert key combinations such as command+w or command+q in the few
	// occasions when the user would expect those to affect a running machine
	// rather than to affect application state.
	//
	// Most obviously: when emulating a Macintosh, you'd expect command+q to
	// quit the application inside the Macintosh, not to quit the emulator.

	switch(event.type) {
		case NSEventTypeKeyUp:
		case NSEventTypeKeyDown:
		case NSEventTypeFlagsChanged:
			if(self.keyboardEventDelegate) {
				[self.keyboardEventDelegate sendEvent:event];
				return;
			}

		default:
			[super sendEvent:event];
	}

}

@end

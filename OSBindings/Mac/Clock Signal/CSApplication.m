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
	// Send the event unless an event delegate says otherwise.
	if(!self.eventDelegate || [self.eventDelegate application:self shouldSendEvent:event]) {
		[super sendEvent:event];
	}
}

@end

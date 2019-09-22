//
//  CSApplication.h
//  Clock Signal
//
//  Created by Thomas Harte on 22/09/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef CSApplication_h
#define CSApplication_h

#import <Cocoa/Cocoa.h>

@protocol CSApplicationKeyboardEventDelegate
- (void)sendEvent:(nonnull NSEvent *)event;
@end

/*!
	CSApplication differs from NSApplication in only one regard: it supports a keyboardEventDelegate.

	If a keyboardEventDelegate is installed, all keyboard events — @c NSEventTypeKeyUp,
	@c NSEventTypeKeyDown and @c NSEventTypeFlagsChanged — will be diverted to it
	rather than passed through the usual processing. As a result keyboard shortcuts and assistive
	dialogue navigations won't work.
*/
@interface CSApplication: NSApplication
@property(nonatomic, weak, nullable) id<CSApplicationKeyboardEventDelegate> keyboardEventDelegate;
@end


#endif /* CSApplication_h */

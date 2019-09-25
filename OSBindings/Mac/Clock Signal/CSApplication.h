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

@class CSApplication;

@protocol CSApplicationEventDelegate
- (BOOL)application:(nonnull CSApplication *)application shouldSendEvent:(nonnull NSEvent *)event;
@end

/*!
	CSApplication differs from NSApplication in only one regard: it supports an eventDelegate.

	If conected, an eventDelegate will be offered all application events prior to their propagation
	into the application proper. It may opt to remove those events from the queue. This primarily
	provides a way to divert things like the command key that will otherwise trigger menu
	shortcuts, for periods when it is appropriate to do so.
*/
@interface CSApplication: NSApplication
@property(nonatomic, weak, nullable) id<CSApplicationEventDelegate> eventDelegate;
@end


#endif /* CSApplication_h */

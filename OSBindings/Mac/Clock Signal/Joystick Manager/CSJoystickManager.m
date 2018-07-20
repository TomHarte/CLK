//
//  CSJoystickManager.m
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#import "CSJoystickManager.h"

@import IOKit;
#include <IOKit/hid/IOHIDLib.h>

static void DeviceMatched(void *context, IOReturn result, void *sender, IOHIDDeviceRef device) {
	NSLog(@"Matched");
}

static void DeviceRemoved(void *context, IOReturn result, void *sender, IOHIDDeviceRef device) {
	NSLog(@"Removed");
}

@implementation CSJoystickManager {
	IOHIDManagerRef _hidManager;
}

- (instancetype)init {
	self = [super init];
	if(self) {
		_hidManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
		if(!_hidManager) return nil;

		NSArray<NSDictionary<NSString *, NSNumber *> *> *const multiple = @[
			@{ @kIOHIDDeviceUsagePageKey: @(kHIDPage_GenericDesktop), @kIOHIDDeviceUsageKey: @(kHIDUsage_GD_Joystick) },
			@{ @kIOHIDDeviceUsagePageKey: @(kHIDPage_GenericDesktop), @kIOHIDDeviceUsageKey: @(kHIDUsage_GD_GamePad) },
			@{ @kIOHIDDeviceUsagePageKey: @(kHIDPage_GenericDesktop), @kIOHIDDeviceUsageKey: @(kHIDUsage_GD_MultiAxisController) },
		];

		IOHIDManagerSetDeviceMatchingMultiple(_hidManager, (__bridge CFArrayRef)multiple);
		IOHIDManagerRegisterDeviceMatchingCallback(_hidManager, DeviceMatched, (__bridge void *)self);
		IOHIDManagerRegisterDeviceRemovalCallback(_hidManager, DeviceRemoved, (__bridge void *)self);
		IOHIDManagerScheduleWithRunLoop(_hidManager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);

		if(IOHIDManagerOpen(_hidManager, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
			NSLog(@"Failed to open HID manager");
			// something
			return nil;
		}
	}

	return self;
}

- (void)dealloc {
	IOHIDManagerUnscheduleFromRunLoop(_hidManager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
	IOHIDManagerClose(_hidManager, kIOHIDOptionsTypeNone);
	CFRelease(_hidManager);
}

@end

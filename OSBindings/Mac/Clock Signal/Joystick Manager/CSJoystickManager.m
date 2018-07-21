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

@interface CSJoystickManager ()
- (void)deviceMatched:(IOHIDDeviceRef)device result:(IOReturn)result sender:(void *)sender;
- (void)deviceRemoved:(IOHIDDeviceRef)device result:(IOReturn)result sender:(void *)sender;
@end

static void DeviceMatched(void *context, IOReturn result, void *sender, IOHIDDeviceRef device) {
	[(__bridge  CSJoystickManager *)context deviceMatched:device result:result sender:sender];
}

static void DeviceRemoved(void *context, IOReturn result, void *sender, IOHIDDeviceRef device) {
	[(__bridge  CSJoystickManager *)context deviceRemoved:device result:result sender:sender];
}

@implementation CSJoystickManager {
	IOHIDManagerRef _hidManager;
	NSMutableSet<NSValue *> *_activeDevices;
}

- (instancetype)init {
	self = [super init];
	if(self) {
		_activeDevices = [[NSMutableSet alloc] init];

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

- (void)deviceMatched:(IOHIDDeviceRef)device result:(IOReturn)result sender:(void *)sender {
	NSValue *const deviceKey = [NSValue valueWithPointer:device];
	if([_activeDevices containsObject:deviceKey]) {
		return;
	}

	[_activeDevices addObject:deviceKey];
	NSLog(@"Matched");

	const CFArrayRef elements = IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone);
	for(CFIndex index = 0; index < CFArrayGetCount(elements); ++index) {
		const IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, index);

		// Check that this element is either on the generic desktop page or else is a button.
		const uint32_t usagePage = IOHIDElementGetUsagePage(element);
		if(usagePage != kHIDPage_GenericDesktop && usagePage != kHIDPage_Button) continue;

		// Then inspect the usage and type.
		const IOHIDElementType type = IOHIDElementGetType(element);

		// IOHIDElementGetCookie

		switch(type) {
			default: break;
			case kIOHIDElementTypeInput_Button:
				// Add a buton
			break;

			case kIOHIDElementTypeInput_Misc:
			case kIOHIDElementTypeInput_Axis: {
				const uint32_t usage = IOHIDElementGetUsage(element);
				// Add something depending on usage...
			} break;

			case kIOHIDElementTypeCollection:
				// TODO: recurse.
			break;
		}
	}

	CFRelease(elements);
}

- (void)deviceRemoved:(IOHIDDeviceRef)device result:(IOReturn)result sender:(void *)sender {
	NSValue *const deviceKey = [NSValue valueWithPointer:device];
	if(![_activeDevices containsObject:deviceKey]) {
		return;
	}

	[_activeDevices removeObject:deviceKey];
	NSLog(@"Removed");
}

@end

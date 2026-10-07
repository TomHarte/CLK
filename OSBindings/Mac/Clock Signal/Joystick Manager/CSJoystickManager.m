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
@import GameController;

#pragma mark - CSJoystickButton

@implementation CSJoystickButton {
	@package
	bool _isPressed;
}

- (instancetype)initWithIndex:(NSInteger)index {
	if (self = [super init]) {
		_index = index;
	}
	return self;
}

@end

#pragma mark - CSJoystickAxis

@implementation CSJoystickAxis {
	@package
	float _position;
}

- (instancetype)initWithType:(CSJoystickAxisType)type
{
	if (self = [super init]) {
		_type = type;
	}
	return self;
}

@end

#pragma mark - CSJoystickHat

@implementation CSJoystickHat {
	@package
	CSJoystickHatDirection _direction;
}

@end

#pragma mark - CSJoystick

@implementation CSJoystick {
	@package
	NSArray<CSJoystickButton *> *_buttons;
	NSArray<CSJoystickAxis *> *_axes;
	NSArray<CSJoystickHat *> *_hats;
}

- (void)update {
	//subclass!
}

@end

#pragma mark - GameController subclasses

API_AVAILABLE(macos(11.0))
@interface CSGCJoystickHat: CSJoystickHat
@property (readonly, strong) GCDeviceDirectionPad *directionPad;
@end

API_AVAILABLE(macos(11.0))
@interface CSGCJoystickAxis: CSJoystickAxis
@property (readonly, strong) GCDeviceAxisInput *axis;
@end

API_AVAILABLE(macos(11.0))
@interface CSGCJoystickButton: CSJoystickButton
@property (readonly, strong) GCDeviceButtonInput *button;
@end

API_AVAILABLE(macos(11.0))
@interface CSGCJoystick: CSJoystick
@property (readonly, strong) GCController *device;
@end

@implementation CSGCJoystickHat

- (instancetype)initWithDirectionPad:(GCDeviceDirectionPad*)dPad {
	if (self = [super init]) {
		_directionPad = dPad;
	}
	return self;
}

- (NSString *)description {
	return [NSString stringWithFormat:@"<CSGCJoystickHat: %p>; direction %ld", self, (long)self.direction];
}

- (void)setDirection:(CSJoystickHatDirection)direction {
	_direction = direction;
}

@end

@implementation CSGCJoystickAxis

- (instancetype)initWithAxis:(GCDeviceAxisInput*)element type:(CSJoystickAxisType)type {
	self = [super initWithType:type];
	if(self) {
		_axis = element;
		_position = 0.5f;
	}
	return self;
}

- (NSString *)description {
	return [NSString stringWithFormat:@"<CSGCJoystickAxis: %p>; type %d, value %0.2f", self, (int)self.type, self.position];
}

- (void)setPosition:(float)position {
	_position = position;
}

@end

@implementation CSGCJoystickButton

- (instancetype)initWithButton:(GCDeviceButtonInput*)element index:(NSInteger)index {
	self = [super initWithIndex:index];
	if(self) {
		_button = element;
	}
	return self;
}

- (NSString *)description {
	return [NSString stringWithFormat:@"<CSGCJoystickButton: %p>; button %ld, %@", self, (long)self.index, self.isPressed ? @"pressed" : @"released"];
}

- (void)setIsPressed:(bool)isPressed {
	_isPressed = isPressed;
}

@end

@implementation CSGCJoystick

- (instancetype)initWithButtons:(NSArray<CSJoystickButton *> *)buttons
	axes:(NSArray<CSJoystickAxis *> *)axes
	hats:(NSArray<CSJoystickHat *> *)hats
	device:(GCController *)device {
	if (self = [super init]) {
		// Sort buttons by index.
		_buttons = [buttons sortedArrayUsingDescriptors:@[[NSSortDescriptor sortDescriptorWithKey:@"index" ascending:YES]]];

		// Sort axes by enum value.
		_axes = [axes sortedArrayUsingDescriptors:@[[NSSortDescriptor sortDescriptorWithKey:@"type" ascending:YES]]];

		// Hats have no guaranteed ordering.
		_hats = hats;

		// Keep hold of the device.
		_device = device;
	}
	return self;
}

- (void)update {
	// Update buttons.
	for(CSGCJoystickButton *button in _buttons) {
		// This assumes that the values provided by GCDeviceButtonInput are
		// digital. This might not always be the case.
		button.isPressed = button.button.pressed;
	}
	for(CSGCJoystickAxis *axis in _axes) {
		float val = axis.axis.value;
		val += 1;
		val /= 2;
		if(axis.type == CSJoystickAxisTypeY) {
			axis.position = 1 - val;
		} else {
			axis.position = val;
		}
	}
	for(CSGCJoystickHat *hat in _hats) {
		// This assumes that the values provided by GCDeviceDirectionPad are
		// digital. this might not always be the case.
		CSJoystickHatDirection hatDir = 0;
		if (hat.directionPad.down.pressed) {
			hatDir |= CSJoystickHatDirectionDown;
		}
		if (hat.directionPad.up.pressed) {
			hatDir |= CSJoystickHatDirectionUp;
		}
		if (hat.directionPad.left.pressed) {
			hatDir |= CSJoystickHatDirectionLeft;
		}
		if (hat.directionPad.right.pressed) {
			hatDir |= CSJoystickHatDirectionRight;
		}
		// There shouldn't be any conflicting directions.
		hat.direction = hatDir;
	}
}

- (NSString *)description {
	return [NSString stringWithFormat:@"<CSGCJoystick: %p>; buttons %@, axes %@, hats %@", self, self.buttons, self.axes, self.hats];
}

@end

#pragma mark - CSJoystickManager

@interface CSJoystickManager ()
- (void)controllerDidConnect:(NSNotification *)note API_AVAILABLE(macos(11.0));
- (void)controllerDidDisconnect:(NSNotification *)note API_AVAILABLE(macos(11.0));
@end

@implementation CSJoystickManager {
	IOHIDManagerRef _hidManager;
	NSMutableArray<CSJoystick *> *_joysticks;
}

- (instancetype)init {
	self = [super init];
	if(self) {
		_joysticks = [[NSMutableArray alloc] init];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(controllerDidConnect:) name:GCControllerDidConnectNotification object:nil];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(controllerDidDisconnect:) name:GCControllerDidDisconnectNotification object:nil];
	}

	return self;
}

- (void)controllerDidConnect:(NSNotification *)note {
	GCController *controller = note.object;

	// Double check this joystick isn't already known.
	for(CSGCJoystick *joystick in _joysticks) {
		if([joystick.device isEqual:controller]) return;
	}

	// Prepare to collate a list of buttons, axes and hats for the new device.
	NSMutableArray<CSJoystickButton *> *buttons = [[NSMutableArray alloc] init];
	NSMutableArray<CSJoystickAxis *> *axes = [[NSMutableArray alloc] init];
	NSMutableArray<CSJoystickHat *> *hats = [[NSMutableArray alloc] init];

	if (controller.extendedGamepad) {
		GCExtendedGamepad *gp = controller.extendedGamepad;
		// Let's go a b x y
		//          1 2 3 4
		[buttons addObject:[[CSGCJoystickButton alloc] initWithButton:gp.buttonA index:1]];
		[buttons addObject:[[CSGCJoystickButton alloc] initWithButton:gp.buttonB index:2]];
		[buttons addObject:[[CSGCJoystickButton alloc] initWithButton:gp.buttonX index:3]];
		[buttons addObject:[[CSGCJoystickButton alloc] initWithButton:gp.buttonY index:4]];

		[hats addObject:[[CSGCJoystickHat alloc] initWithDirectionPad:gp.dpad]];

		[axes addObject:[[CSGCJoystickAxis alloc] initWithAxis:gp.leftThumbstick.xAxis type:CSJoystickAxisTypeX]];
		[axes addObject:[[CSGCJoystickAxis alloc] initWithAxis:gp.leftThumbstick.yAxis type:CSJoystickAxisTypeY]];
		[axes addObject:[[CSGCJoystickAxis alloc] initWithAxis:gp.rightThumbstick.xAxis type:CSJoystickAxisTypeZ]];
	} else {
		return;
	}

	// Add this joystick to the list.
	[_joysticks addObject:[[CSGCJoystick alloc] initWithButtons:buttons axes:axes hats:hats device:controller]];
}

- (void)controllerDidDisconnect:(NSNotification *)note {
	GCController *controller = note.object;

	// If this joystick was recorded, remove it.
	for(CSGCJoystick *joystick in [_joysticks copy]) {
		if (![joystick isKindOfClass:[CSGCJoystick class]]) {
			continue;
		}
		if([joystick.device isEqual:controller]) {
			[_joysticks removeObject:joystick];
			return;
		}
	}
}

- (void)dealloc {
	[[NSNotificationCenter defaultCenter] removeObserver:self name:GCControllerDidConnectNotification object:nil];
	[[NSNotificationCenter defaultCenter] removeObserver:self name:GCControllerDidDisconnectNotification object:nil];
}

- (void)update {
	[self.joysticks makeObjectsPerformSelector:@selector(update)];
}

- (NSArray<CSJoystick *> *)joysticks {
	return [_joysticks copy];
}

@end

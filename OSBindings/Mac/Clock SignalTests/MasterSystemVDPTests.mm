//
//  MasterSystemVDPTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 09/10/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>
#import <OpenGL/OpenGL.h>

#include "9918.hpp"

@interface MasterSystemVDPTests : XCTestCase
@end

@implementation MasterSystemVDPTests {
	NSOpenGLContext *_openGLContext;
}

- (void)setUp {
    [super setUp];

	// Create a valid OpenGL context, so that a VDP can be constructed.
	NSOpenGLPixelFormatAttribute attributes[] =
	{
		NSOpenGLPFAOpenGLProfile,	NSOpenGLProfileVersion3_2Core,
		0
	};

	NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	_openGLContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
	[_openGLContext makeCurrentContext];
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    _openGLContext = nil;

    [super tearDown];
}

- (void)testLineInterrupt {
	TI::TMS::TMS9918 vdp(TI::TMS::Personality::SMSVDP);

	// Disable end-of-frame interrupts, enable line interrupts.
	vdp.set_register(1, 0x00);
	vdp.set_register(1, 0x81);

	vdp.set_register(1, 0x10);
	vdp.set_register(1, 0x80);

	// Set a line interrupt to occur in five lines.
	vdp.set_register(1, 5);
	vdp.set_register(1, 0x8a);

	// Get time until interrupt.
	int time_until_interrupt = vdp.get_time_until_interrupt().as_int() - 1;

	// Check interrupt flag isn't set prior to the reported time.
	vdp.run_for(HalfCycles(time_until_interrupt));
	NSAssert(!vdp.get_interrupt_line(), @"Interrupt line went active early [1]");

	// Check interrupt flag is set at the reported time.
	vdp.run_for(HalfCycles(1));
	NSAssert(vdp.get_interrupt_line(), @"Interrupt line wasn't set when promised [1]");

	// Read the status register to clear interrupt status.
	vdp.get_register(1);
	NSAssert(!vdp.get_interrupt_line(), @"Interrupt wasn't reset by status read");

	// Check interrupt flag isn't set prior to the reported time.
	time_until_interrupt = vdp.get_time_until_interrupt().as_int() - 1;
	vdp.run_for(HalfCycles(time_until_interrupt));
	NSAssert(!vdp.get_interrupt_line(), @"Interrupt line went active early [2]");

	// Check interrupt flag is set at the reported time.
	vdp.run_for(HalfCycles(1));
	NSAssert(vdp.get_interrupt_line(), @"Interrupt line wasn't set when promised [2]");
}

@end

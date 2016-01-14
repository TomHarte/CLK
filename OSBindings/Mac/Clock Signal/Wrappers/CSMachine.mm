//
//  CSMachine.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSMachine+Subclassing.h"

struct CRTDelegate: public Outputs::CRT::Delegate {
	__weak CSMachine *machine;
	void crt_did_end_frame(Outputs::CRT *crt, CRTFrame *frame, bool did_detect_vsync) {
		[machine crt:crt didEndFrame:frame didDetectVSync:did_detect_vsync];
	}
};

typedef NS_ENUM(NSInteger, CSAtari2600RunningState) {
	CSMachineRunningStateRunning,
	CSMachineRunningStateStopped
};

@implementation CSMachine {
	CRTDelegate _crtDelegate;

	dispatch_queue_t _serialDispatchQueue;
	NSConditionLock *_runningLock;
}

- (void)perform:(dispatch_block_t)action {
	dispatch_async(_serialDispatchQueue, action);
}

- (void)crt:(Outputs::CRT *)crt didEndFrame:(CRTFrame *)frame didDetectVSync:(BOOL)didDetectVSync {
	if([self.view pushFrame:frame]) crt->return_frame();
}

- (void)runForNumberOfCycles:(int)cycles {
	if([_runningLock tryLockWhenCondition:CSMachineRunningStateStopped]) {
		[_runningLock unlockWithCondition:CSMachineRunningStateRunning];
		dispatch_async(_serialDispatchQueue, ^{
			[_runningLock lockWhenCondition:CSMachineRunningStateRunning];
			[self doRunForNumberOfCycles:cycles];
			[_runningLock unlockWithCondition:CSMachineRunningStateStopped];
		});
	}
}

- (instancetype)init {
	self = [super init];

	if (self) {
		_crtDelegate.machine = self;
		[self setCRTDelegate:&_crtDelegate];
		[self setSpeakerDelegate:nil];
		_serialDispatchQueue = dispatch_queue_create("Machine queue", DISPATCH_QUEUE_SERIAL);
		_runningLock = [[NSConditionLock alloc] initWithCondition:CSMachineRunningStateStopped];
	}

	return self;
}

- (void)setCRTDelegate:(Outputs::CRT::Delegate *)delegate {}
- (void)setSpeakerDelegate:(Outputs::Speaker::Delegate *)delegate {}
- (void)doRunForNumberOfCycles:(int)numberOfCycles {}

@end

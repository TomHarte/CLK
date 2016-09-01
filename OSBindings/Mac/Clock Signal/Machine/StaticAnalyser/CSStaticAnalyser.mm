//
//  CSStaticAnalyser.m
//  Clock Signal
//
//  Created by Thomas Harte on 31/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSStaticAnalyser.h"

#import "CSMachine.h"
#import "CSMachine+Target.h"
#import "Clock_Signal-Swift.h"
#include "StaticAnalyser.hpp"

@implementation CSStaticAnalyser
{
	StaticAnalyser::Target _target;
}

- (instancetype)initWithFileAtURL:(NSURL *)url
{
	self = [super init];
	if(self)
	{
		std::list<StaticAnalyser::Target> targets = StaticAnalyser::GetTargets([url fileSystemRepresentation]);
		if(!targets.size()) return nil;
		_target = targets.front();
	}
	return self;
}

- (Class)documentClass
{
	switch(_target.machine)
	{
		case StaticAnalyser::Target::Electron:	return [ElectronDocument class];
		case StaticAnalyser::Target::Vic20:		return [Vic20Document class];
		case StaticAnalyser::Target::Atari2600:	return [Atari2600Document class];
	}

	return nil;
}

- (void)applyToMachine:(CSMachine *)machine
{
	[machine applyTarget:_target];
}

@end

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
#import "CSMachine+Subclassing.h"

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

		// TODO: can this better be supplied by the analyser?
		_displayName = [[url pathComponents] lastObject];
	}
	return self;
}

- (NSString *)optionsPanelNibName
{
	switch(_target.machine)
	{
		case StaticAnalyser::Target::Electron:	return @"ElectronOptions";
		case StaticAnalyser::Target::Vic20:		return @"Vic20Options";
		case StaticAnalyser::Target::Atari2600:	return @"Atari2600Options";
	}

	return nil;
}

- (CSMachine *)newMachine
{
	switch(_target.machine)
	{
		case StaticAnalyser::Target::Electron:	return [[CSElectron alloc] init];
		case StaticAnalyser::Target::Vic20:		return [[CSVic20 alloc] init];
		case StaticAnalyser::Target::Atari2600:	return [[CSAtari2600 alloc] init];
	}
}

- (void)applyToMachine:(CSMachine *)machine
{
	[machine applyTarget:_target];
}

@end

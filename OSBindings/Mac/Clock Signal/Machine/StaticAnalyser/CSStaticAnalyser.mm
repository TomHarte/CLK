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
#import "CSMachine+Subclassing.h"

#include "StaticAnalyser.hpp"

#import "CSAtari2600.h"
#import "CSElectron.h"
#import "CSOric.h"
#import "CSVic20.h"
#import "CSZX8081.h"

#import "Clock_Signal-Swift.h"

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
		case StaticAnalyser::Target::Atari2600:	return @"Atari2600Options";
		case StaticAnalyser::Target::Electron:	return @"ElectronOptions";
		case StaticAnalyser::Target::Oric:		return @"OricOptions";
		case StaticAnalyser::Target::Vic20:		return @"Vic20Options";
		case StaticAnalyser::Target::ZX8081:	return @"ZX8081Options";
		default: return nil;
	}
}

- (CSMachine *)newMachine
{
	switch(_target.machine)
	{
		case StaticAnalyser::Target::Atari2600:	return [[CSAtari2600 alloc] init];
		case StaticAnalyser::Target::Electron:	return [[CSElectron alloc] init];
		case StaticAnalyser::Target::Oric:		return [[CSOric alloc] init];
		case StaticAnalyser::Target::Vic20:		return [[CSVic20 alloc] init];
		case StaticAnalyser::Target::ZX8081:	return [[CSZX8081 alloc] init];
		default: return nil;
	}
}

- (void)applyToMachine:(CSMachine *)machine
{
	[machine applyTarget:_target];
}

@end

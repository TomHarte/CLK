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

#import "Clock_Signal-Swift.h"

@implementation CSStaticAnalyser {
	std::vector<StaticAnalyser::Target> _targets;
}

- (instancetype)initWithFileAtURL:(NSURL *)url {
	self = [super init];
	if(self) {
		_targets = StaticAnalyser::GetTargets([url fileSystemRepresentation]);
		if(!_targets.size()) return nil;

		// TODO: could this better be supplied by the analyser? A hypothetical file format might
		// provide a better name for it contents than the file name?
		_displayName = [[url pathComponents] lastObject];
	}
	return self;
}

- (NSString *)optionsPanelNibName {
	switch(_targets.front().machine) {
		case StaticAnalyser::Target::AmstradCPC:	return nil;
		case StaticAnalyser::Target::Atari2600:		return @"Atari2600Options";
		case StaticAnalyser::Target::Electron:		return @"QuickLoadCompositeOptions";
		case StaticAnalyser::Target::MSX:			return @"QuickLoadCompositeOptions";
		case StaticAnalyser::Target::Oric:			return @"OricOptions";
		case StaticAnalyser::Target::Vic20:			return @"Vic20Options";
		case StaticAnalyser::Target::ZX8081:		return @"ZX8081Options";
		default: return nil;
	}
}

- (void)applyToMachine:(CSMachine *)machine {
	[machine applyTarget:_targets.front()];
}

- (std::vector<StaticAnalyser::Target> &)targets {
	return _targets;
}

@end

@implementation CSMediaSet {
	StaticAnalyser::Media _media;
}

- (instancetype)initWithFileAtURL:(NSURL *)url {
	self = [super init];
	if(self) {
		_media = StaticAnalyser::GetMedia([url fileSystemRepresentation]);
	}
	return self;
}

- (void)applyToMachine:(CSMachine *)machine {
	[machine applyMedia:_media];
}

@end

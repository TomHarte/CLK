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

#include "StaticAnalyser.hpp"

#import "Clock_Signal-Swift.h"

@implementation CSStaticAnalyser {
	std::vector<std::unique_ptr<Analyser::Static::Target>> _targets;
}

- (instancetype)initWithFileAtURL:(NSURL *)url {
	self = [super init];
	if(self) {
		_targets = Analyser::Static::GetTargets([url fileSystemRepresentation]);
		if(!_targets.size()) return nil;

		// TODO: could this better be supplied by the analyser? A hypothetical file format might
		// provide a better name for it contents than the file name?
		_displayName = [[url pathComponents] lastObject];
	}
	return self;
}

- (NSString *)optionsPanelNibName {
	switch(_targets.front()->machine) {
		case Analyser::Machine::AmstradCPC:	return nil;
		case Analyser::Machine::Atari2600:	return @"Atari2600Options";
		case Analyser::Machine::Electron:	return @"QuickLoadCompositeOptions";
		case Analyser::Machine::MSX:		return @"QuickLoadCompositeOptions";
		case Analyser::Machine::Oric:		return @"OricOptions";
		case Analyser::Machine::Vic20:		return @"QuickLoadCompositeOptions";
		case Analyser::Machine::ZX8081:		return @"ZX8081Options";
		default: return nil;
	}
}

- (std::vector<std::unique_ptr<Analyser::Static::Target>> &)targets {
	return _targets;
}

@end

@implementation CSMediaSet {
	Analyser::Static::Media _media;
}

- (instancetype)initWithFileAtURL:(NSURL *)url {
	self = [super init];
	if(self) {
		_media = Analyser::Static::GetMedia([url fileSystemRepresentation]);
	}
	return self;
}

- (void)applyToMachine:(CSMachine *)machine {
	[machine applyMedia:_media];
}

@end

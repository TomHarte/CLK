//
//  CSElectron.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSElectron.h"

#include "Electron.hpp"
#include "StaticAnalyser.hpp"

#import "CSMachine+Subclassing.h"
#import "NSData+StdVector.h"
#import "NSBundle+DataResource.h"

@implementation CSElectron {
	std::unique_ptr<Electron::Machine> _electron;
}

- (instancetype)init {
	Electron::Machine *machine = Electron::Machine::Electron();

	self = [super initWithMachine:machine];
	if(self) {
		_electron.reset(machine);

		[self setOSROM:[self rom:@"os"]];
		[self setBASICROM:[self rom:@"basic"]];
		[self setDFSROM:[self rom:@"DFS-1770-2.20"]];

		NSMutableData *adfs = [[self rom:@"ADFS-E00_1"] mutableCopy];
		[adfs appendData:[self rom:@"ADFS-E00_2"]];
		[self setADFSROM:adfs];
	}
	return self;
}

- (NSData *)rom:(NSString *)name {
	return [[NSBundle mainBundle] dataForResource:name withExtension:@"rom" subdirectory:@"ROMImages/Electron"];
}

#pragma mark - ROM setting

- (void)setOSROM:(nonnull NSData *)rom		{	[self setROM:rom slot:Electron::ROMSlotOS];		}
- (void)setBASICROM:(nonnull NSData *)rom	{	[self setROM:rom slot:Electron::ROMSlotBASIC];	}
- (void)setADFSROM:(nonnull NSData *)rom	{	[self setROM:rom slot:Electron::ROMSlotADFS];	}
- (void)setDFSROM:(nonnull NSData *)rom		{	[self setROM:rom slot:Electron::ROMSlotDFS];	}

- (void)setROM:(nonnull NSData *)rom slot:(int)slot {
	if(rom)
	{
		@synchronized(self) {
			_electron->set_rom((Electron::ROMSlot)slot, rom.stdVector8, false);
		}
	}
}

- (NSString *)userDefaultsPrefix {	return @"electron";	}

#pragma mark - Options

- (void)setUseFastLoadingHack:(BOOL)useFastLoadingHack {
	@synchronized(self) {
		_useFastLoadingHack = useFastLoadingHack;
		_electron->set_use_fast_tape_hack(useFastLoadingHack ? true : false);
	}
}

- (void)setUseTelevisionOutput:(BOOL)useTelevisionOutput {
	@synchronized(self) {
		_useTelevisionOutput = useTelevisionOutput;
		_electron->get_crt()->set_output_device(useTelevisionOutput ? Outputs::CRT::Television : Outputs::CRT::Monitor);
	}
}
@end

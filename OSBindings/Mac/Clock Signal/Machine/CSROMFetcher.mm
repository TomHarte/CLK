//
//  CSROMFetcher.m
//  Clock Signal
//
//  Created by Thomas Harte on 01/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#include "CSROMFetcher.hpp"

#import "NSBundle+DataResource.h"
#import "NSData+StdVector.h"

#include <string>

ROMMachine::ROMFetcher CSROMFetcher(ROM::Request *missing) {
	return [missing] (const ROM::Request &roms) -> ROM::Map {
		NSArray<NSURL *> *const supportURLs = [[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];

		ROM::Map results;
		for(const auto &description: roms.all_descriptions()) {
			for(const auto &file_name: description.file_names) {
				NSData *fileData;
				NSString *const subdirectory = [@"ROMImages/" stringByAppendingString:[NSString stringWithUTF8String:description.machine_name.c_str()]];

				// Check for this file first within the application support directories.
				for(NSURL *supportURL in supportURLs) {
					NSURL *const fullURL = [[supportURL URLByAppendingPathComponent:subdirectory]
								URLByAppendingPathComponent:[NSString stringWithUTF8String:file_name.c_str()]];
					fileData = [NSData dataWithContentsOfURL:fullURL];
					if(fileData) break;
				}

				// Failing that, check inside the application bundle.
				if(!fileData) {
					fileData = [[NSBundle mainBundle]
						dataForResource:[NSString stringWithUTF8String:file_name.c_str()]
						withExtension:nil
						subdirectory:subdirectory];
				}

				// Store an appropriate result.
				if(fileData) {
					results[description.name] = fileData.stdVector8;
				}
			}
		}

		// TODO: sever all found ROMs from roms and store to missing, if provided.

		return results;
	};
}

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
#import "NSData+CRC32.h"

#include <string>

namespace {

NSString *directoryFor(const ROM::Description &description) {
	return [@"ROMImages/" stringByAppendingString:[NSString stringWithUTF8String:description.machine_name.c_str()]];
}

NSArray<NSURL *> *urlsFor(const ROM::Description &description, const std::string &file_name) {
	NSMutableArray<NSURL *> *const urls = [[NSMutableArray alloc] init];
	NSArray<NSURL *> *const supportURLs = [[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];
	NSString *const subdirectory = directoryFor(description);

	for(NSURL *supportURL in supportURLs) {
		[urls addObject:[[supportURL URLByAppendingPathComponent:subdirectory]
								URLByAppendingPathComponent:[NSString stringWithUTF8String:file_name.c_str()]]];
	}

	return urls;
}

}

BOOL CSInstallROM(NSURL *url) {
	NSData *const data = [NSData dataWithContentsOfURL:url];
	if(!data) return NO;

	// Try for a direct CRC match.
	std::optional<ROM::Description> target_description;
	target_description = ROM::Description::from_crc(uint32_t(data.crc32.integerValue));

	// See whether there's an acceptable trimming that creates a CRC match.
	if(!target_description) {
		const std::vector<ROM::Description> descriptions = ROM::all_descriptions();
		for(const auto &description: descriptions) {
			if(description.size > data.length) continue;

			NSData *const trimmedData = [data subdataWithRange:NSMakeRange(0, description.size)];
			if(description.crc32s.find(uint32_t(trimmedData.crc32.unsignedIntValue)) != description.crc32s.end()) {
				target_description = description;
				break;
			}
		}
	}

	// If no destination was found, stop.
	if(!target_description) {
		return NO;
	}

	// Copy the data to its destination and report success.
	NSURL *const targetURL = [urlsFor(*target_description, target_description->file_names[0]) firstObject];
	[[NSFileManager defaultManager] createDirectoryAtPath:targetURL.URLByDeletingLastPathComponent.path withIntermediateDirectories:YES attributes:nil error:nil];
	[data writeToURL:targetURL atomically:NO];

	return YES;
}

ROMMachine::ROMFetcher CSROMFetcher(ROM::Request *missing) {
	return [missing] (const ROM::Request &roms) -> ROM::Map {
		ROM::Map results;
		for(const auto &description: roms.all_descriptions()) {
			for(const auto &file_name: description.file_names) {
				NSData *fileData;

				// Check for this file first within the application support directories.
				for(NSURL *fileURL in urlsFor(description, file_name)) {
					fileData = [NSData dataWithContentsOfURL:fileURL];
					if(fileData) break;
				}

				// Failing that, check inside the application bundle.
				if(!fileData) {
					fileData = [[NSBundle mainBundle]
						dataForResource:[NSString stringWithUTF8String:file_name.c_str()]
						withExtension:nil
						subdirectory:directoryFor(description)];
				}

				// Store an appropriate result.
				if(fileData) {
					results[description.name] = fileData.stdVector8;
				}
			}
		}

		if(missing) {
			*missing = roms.subtract(results);
		}

		return results;
	};
}

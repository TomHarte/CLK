//
//  CSROMReceiverView.m
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import "CSROMReceiverView.h"

@interface CSROMReceiverView () <NSDraggingDestination>
@end


@implementation CSROMReceiverView

- (void)awakeFromNib {
	[super awakeFromNib];

	// Accept file URLs by drag and drop.
	[self registerForDraggedTypes:@[(__bridge NSString *)kUTTypeFileURL]];
}

#pragma mark - NSDraggingDestination

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender {
	// Just forward the URLs.
	for(NSPasteboardItem *item in [[sender draggingPasteboard] pasteboardItems]) {
		NSURL *URL = [NSURL URLWithString:[item stringForType:(__bridge NSString *)kUTTypeFileURL]];
		[self.delegate romReceiverView:self didReceiveFileAtURL:URL];
	}
	return YES;
}

- (NSDragOperation)draggingEntered:(id < NSDraggingInfo >)sender {
	// The emulator will take a copy of any deposited ROM files.
	return NSDragOperationCopy;
}

@end

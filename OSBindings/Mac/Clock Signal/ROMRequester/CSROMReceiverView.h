//
//  CSROMReceiverView.h
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class CSROMReceiverView;

@protocol CSROMReciverViewDelegate
/*!
	Announces receipt of a file by drag and drop to the delegate.
	@param view The view making the request.
	@param URL The file URL of the received file.
*/
- (void)romReceiverView:(nonnull CSROMReceiverView *)view didReceiveFileAtURL:(nonnull NSURL *)URL;

@end

@interface CSROMReceiverView : NSView

@property(nonatomic, weak, nullable) id <CSROMReciverViewDelegate> delegate;

@end

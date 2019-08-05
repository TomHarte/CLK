//
//  AppDelegate.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {

	func applicationDidFinishLaunching(_ aNotification: Notification) {
		// Insert code here to initialize your application.
	}

	func applicationWillTerminate(_ aNotification: Notification) {
		// Insert code here to tear down your application.
	}

	func applicationShouldOpenUntitledFile(_ sender: NSApplication) -> Bool {
		// Decline to show the 'New...' selector by default, preferring to offer
		// an 'Open...' dialogue.
		//
		// Obiter: I dislike it when other applications do this for me, but it
		// seems to be the new norm, and I've had user feedback that showing
		// nothing is confusing. So here it is.
		NSDocumentController.shared.openDocument(self)
		return false
	}
}

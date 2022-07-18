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

	func applicationDidFinishLaunching(_ notification: Notification) {
		// Check for at least one Metal-capable GPU; this check
		// will become unnecessary if/when the minimum OS version
		// that this project supports reascends to 10.14.
		if (MTLCopyAllDevices().count == 0) {
			let alert = NSAlert()
			alert.messageText = "This application requires a Metal-capable GPU"
			alert.addButton(withTitle: "Exit")
			alert.runModal()

			let application = notification.object as! NSApplication
			application.terminate(self)
		}
	}

	private var hasShownOpenDocument = false
	func applicationShouldOpenUntitledFile(_ sender: NSApplication) -> Bool {
		// Decline to show the 'New...' selector by default; the 'Open...'
		// dialogue has already been shown if this application was started
		// without a file.
		//
		// Obiter: I dislike it when other applications do this for me, but it
		// seems to be the new norm, and I've had user feedback that showing
		// nothing is confusing. So here it is.
		if !hasShownOpenDocument {
			NSDocumentController.shared.openDocument(self)
			hasShownOpenDocument = true
		}
		return false
	}
}

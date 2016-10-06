//
//  DocumentController.swift
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa

class DocumentController: NSDocumentController {
	override func newDocument(_ sender: Any?) {
		let window = NSWindow(contentViewController: NewDocumentViewController())
		window.title = "Choose a Machine"
		window.makeKeyAndOrderFront(self)
	}
}

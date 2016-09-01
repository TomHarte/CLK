//
//  DocumentController.swift
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa

class DocumentController: NSDocumentController {
	override func documentForURL(url: NSURL) -> NSDocument? {
		return super.documentForURL(url)
	}
//	override func newDocument(sender: AnyObject?) {
//		let window = NSWindow(contentViewController: NewDocumentViewController())
//		window.title = "Choose a Machine"
//		window.makeKeyAndOrderFront(self)
//	}
}

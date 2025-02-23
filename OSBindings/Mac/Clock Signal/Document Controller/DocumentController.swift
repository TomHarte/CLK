//
//  DocumentController.swift
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

import Cocoa

class DocumentController: NSDocumentController {
	let joystickManager = CSJoystickManager()

	override func document(for url: URL) -> NSDocument? {
		Swift.print("Document for?")
		return super.document(for: url)
	}
}

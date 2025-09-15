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

	// This causes a new NSDocument to be opened for every open request;
	// the emulator won't check whether the file is already open and,
	// if so, bring that window to the front, it'll just open a new window
	// each time. My thinking at the time of implementation is that I want
	// to implement internal networking on some of the more advanced machines
	// which will likely mean letting them load multiple instances of the
	// same network software, e.g. loading Stunt Car Racer onto multiple STs.
	//
	// I also think this is probably closer to expectations for an emulator,
	// where current application state branches from the document loaded but
	// then can go in a multitude of directions.
	override func document(for url: URL) -> NSDocument? {
		return nil
	}
}

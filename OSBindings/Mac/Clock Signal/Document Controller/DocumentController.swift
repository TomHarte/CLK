//
//  DocumentController.swift
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa

class DocumentController: NSDocumentController {
	override func makeDocumentWithContentsOfURL(url: NSURL, ofType typeName: String) throws -> NSDocument {
		if let analyser = CSStaticAnalyser(fileAtURL: url) {
			if let documentClass = analyser.documentClass as? NSDocument.Type {
				let document = documentClass.init()
				if let machineDocument = document as? MachineDocument {
					machineDocument.configureAs(analyser)
					return machineDocument
				}
			}
		}

		return try! super.makeDocumentWithContentsOfURL(url, ofType: typeName)
	}
}

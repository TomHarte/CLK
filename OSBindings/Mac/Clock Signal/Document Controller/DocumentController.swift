//
//  DocumentController.swift
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa

class DocumentController: NSDocumentController {
	override func makeDocument(withContentsOf url: URL, ofType typeName: String) throws -> NSDocument {
		if let analyser = CSStaticAnalyser(fileAt: url) {
			if let documentClass = analyser.documentClass as? NSDocument.Type {
				let document = documentClass.init()
				if let machineDocument = document as? MachineDocument {
					machineDocument.displayName = analyser.displayName
					machineDocument.configureAs(analyser)
					return machineDocument
				}
			}
		}

		return try super.makeDocument(withContentsOf: url, ofType: typeName)
	}
}

//
//  Vic20Document.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Foundation

class Vic20Document: MachineDocument {

	private lazy var vic20 = CSVic20()
	override func machine() -> CSMachine! {
		return vic20
	}

	// MARK: NSDocument overrides
	override init() {
		super.init()
		self.intendedCyclesPerSecond = 1022727
		// TODO: or 1108405 for PAL; see http://www.antimon.org/dl/c64/code/stable.txt

		if let kernel = rom("kernel"), basic = rom("basic"), characters = rom("characters-english") {
			vic20.setKernelROM(kernel)
			vic20.setBASICROM(basic)
			vic20.setCharactersROM(characters)
		}
	}

	override class func autosavesInPlace() -> Bool {
		return true
	}

	override var windowNibName: String? {
		return "Vic20Document"
	}

	// MARK: machine setup
	private func rom(name: String) -> NSData? {
		return dataForResource(name, ofType: "bin", inDirectory: "ROMImages/Vic20")
	}

	override func readFromData(data: NSData, ofType typeName: String) throws {
		vic20.setPRG(data)
	}
}

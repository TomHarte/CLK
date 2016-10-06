//
//  Vic20Document.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Foundation

class Vic20Document: MachineDocument {

	fileprivate lazy var vic20 = CSVic20()
	override var machine: CSMachine! {
		get {
			return vic20
		}
	}
	override var name: String! {
		get {
			return "vic20"
		}
	}

	// MARK: NSDocument overrides
	override init() {
		super.init()

		if let kernel = rom("kernel-ntsc"), let basic = rom("basic"), let characters = rom("characters-english") {
			vic20.setKernelROM(kernel)
			vic20.setBASICROM(basic)
			vic20.setCharactersROM(characters)
		}

		if let drive = dataForResource("1541", ofType: "bin", inDirectory: "ROMImages/Commodore1540") {
			vic20.setDriveROM(drive)
		}
	}

	override class func autosavesInPlace() -> Bool {
		return true
	}

	override var windowNibName: String? {
		return "Vic20Document"
	}

	override func read(from url: URL, ofType typeName: String) throws {
		if let pathExtension = url.pathExtension {
			switch pathExtension.lowercased() {
				case "tap":
					vic20.openTAP(at: url)
					return
				default: break;
			}
		}

		let fileWrapper = try FileWrapper(url: url, options: FileWrapper.ReadingOptions(rawValue: 0))
		try self.read(from: fileWrapper, ofType: typeName)
	}

	// MARK: machine setup
	fileprivate func rom(_ name: String) -> Data? {
		return dataForResource(name, ofType: "bin", inDirectory: "ROMImages/Vic20")
	}

	override func read(from data: Data, ofType typeName: String) throws {
		vic20.setPRG(data)
	}
}

//
//  ElectronDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Foundation
import AudioToolbox

class ElectronDocument: MachineDocument {

	fileprivate lazy var electron = CSElectron()
	override var machine: CSMachine! {
		get {
			return electron
		}
	}
	override var name: String! {
		get {
			return "electron"
		}
	}

	override func aspectRatio() -> NSSize {
		return NSSize(width: 11.0, height: 10.0)
	}

	fileprivate func rom(_ name: String) -> Data? {
		return dataForResource(name, ofType: "rom", inDirectory: "ROMImages/Electron")
	}

	override init() {
		super.init();

		if let os = rom("os"), let basic = rom("basic") {
			self.electron.setOSROM(os)
			self.electron.setBASICROM(basic)
		}
		if let dfs = rom("DFS-1770-2.20") {
			self.electron.setDFSROM(dfs)
		}
		if let adfs1 = rom("ADFS-E00_1"), let adfs2 = rom("ADFS-E00_2") {
			var fullADFS = adfs1
			fullADFS.append(adfs2)
			self.electron.setADFSROM(fullADFS as Data)
		}
	}

	override var windowNibName: String? {
		return "ElectronDocument"
	}

/*	override func readFromURL(url: NSURL, ofType typeName: String) throws {
		if let pathExtension = url.pathExtension {
			switch pathExtension.lowercaseString {
				case "uef":
					electron.openUEFAtURL(url)
					return
				default: break;
			}
		}

		let fileWrapper = try NSFileWrapper(URL: url, options: NSFileWrapperReadingOptions(rawValue: 0))
		try self.readFromFileWrapper(fileWrapper, ofType: typeName)
	}*/

	// MARK: IBActions
	@IBOutlet var displayTypeButton: NSPopUpButton?
	@IBAction func setDisplayType(_ sender: NSPopUpButton!) {
		electron.useTelevisionOutput = (sender.indexOfSelectedItem == 1)
		UserDefaults.standard.set(sender.indexOfSelectedItem, forKey: self.displayTypeUserDefaultsKey)
	}

	fileprivate let displayTypeUserDefaultsKey = "electron.displayType"
	override func establishStoredOptions() {
		super.establishStoredOptions()
		let standardUserDefaults = UserDefaults.standard
		standardUserDefaults.register(defaults: [
			displayTypeUserDefaultsKey: 0,
		])

		let displayType = standardUserDefaults.integer(forKey: self.displayTypeUserDefaultsKey)
		electron.useTelevisionOutput = (displayType == 1)
		self.displayTypeButton?.selectItem(at: displayType)
	}
}

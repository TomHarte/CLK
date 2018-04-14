//
//  MachinePicker.swift
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

import Cocoa

class MachinePicker: NSObject {
	@IBOutlet var machineSelector: NSTabView?

	// MARK: - Electron properties
	@IBOutlet var electronDFSButton: NSButton?
	@IBOutlet var electronADFSButton: NSButton?

	// MARK: - CPC properties
	@IBOutlet var cpcModelTypeButton: NSPopUpButton?

	// MARK: - MSX properties
	@IBOutlet var msxHasDiskDriveButton: NSButton?

	// MARK: - Oric properties
	@IBOutlet var oricModelTypeButton: NSPopUpButton?
	@IBOutlet var oricHasMicrodriveButton: NSButton?

	// MARK: - Vic-20 properties
	@IBOutlet var vic20RegionButton: NSPopUpButton?
	@IBOutlet var vic20MemorySizeButton: NSPopUpButton?
	@IBOutlet var vic20HasC1540Button: NSButton?

	// MARK: - ZX80 properties
	@IBOutlet var zx80MemorySizeButton: NSPopUpButton?
	@IBOutlet var zx80UsesZX81ROMButton: NSButton?

	// MARK: - ZX81 properties
	@IBOutlet var zx81MemorySizeButton: NSPopUpButton?

	// MARK: - Preferences
	func establishStoredOptions() {
		let standardUserDefaults = UserDefaults.standard

		// Machine type
		if let machineIdentifier = standardUserDefaults.string(forKey: "new.machine") {
			machineSelector?.selectTabViewItem(withIdentifier: machineIdentifier as Any)
		}

		// Electron settings
		electronDFSButton?.state = standardUserDefaults.bool(forKey: "new.electronDFS") ? .on : .off
		electronADFSButton?.state = standardUserDefaults.bool(forKey: "new.electronADFS") ? .on : .off

		// CPC settings
		cpcModelTypeButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.cpcModel"))

		// MSX settings
		msxHasDiskDriveButton?.state = standardUserDefaults.bool(forKey: "new.msxDiskDrive") ? .on : .off

		// Oric settings
		oricHasMicrodriveButton?.state = standardUserDefaults.bool(forKey: "new.oricMicrodrive") ? .on : .off
		oricModelTypeButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.oricModel"))

		// Vic-20 settings
		vic20RegionButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.vic20Region"))
		vic20MemorySizeButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.vic20MemorySize"))
		vic20HasC1540Button?.state = standardUserDefaults.bool(forKey: "new.vic20C1540") ? .on : .off

		// ZX80
		zx80MemorySizeButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.zx80MemorySize"))
		zx80UsesZX81ROMButton?.state = standardUserDefaults.bool(forKey: "new.zx80UsesZX81ROM") ? .on : .off

		// ZX81
		zx81MemorySizeButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.zx81MemorySize"))
	}

	fileprivate func storeOptions() {
		let standardUserDefaults = UserDefaults.standard

		// Machine type
		standardUserDefaults.set(machineSelector!.selectedTabViewItem!.identifier as! String, forKey: "new.machine")

		// Electron settings
		standardUserDefaults.set(electronDFSButton!.state == .on, forKey: "new.electronDFS")
		standardUserDefaults.set(electronADFSButton!.state == .on, forKey: "new.electronADFS")

		// CPC settings
		standardUserDefaults.set(cpcModelTypeButton!.selectedTag(), forKey: "new.cpcModel")

		// MSX settings
		standardUserDefaults.set(msxHasDiskDriveButton?.state == .on, forKey: "new.msxDiskDrive")

		// Oric settings
		standardUserDefaults.set(oricHasMicrodriveButton?.state == .on, forKey: "new.oricMicrodrive")
		standardUserDefaults.set(oricModelTypeButton!.selectedTag(), forKey: "new.oricModel")

		// Vic-20 settings
		standardUserDefaults.set(vic20RegionButton!.selectedTag(), forKey: "new.vic20Region")
		standardUserDefaults.set(vic20MemorySizeButton!.selectedTag(), forKey: "new.vic20MemorySize")
		standardUserDefaults.set(vic20HasC1540Button?.state == .on, forKey: "new.vic20C1540")

		// ZX80
		standardUserDefaults.set(zx80MemorySizeButton!.selectedTag(), forKey: "new.zx80MemorySize")
		standardUserDefaults.set(zx80UsesZX81ROMButton?.state == .on, forKey: "new.zx80UsesZX81ROM")

		// ZX81
		standardUserDefaults.set(zx81MemorySizeButton!.selectedTag(), forKey: "new.zx81MemorySize")
	}

	// MARK: - Machine builder
	func selectedMachine() -> CSStaticAnalyser {
		storeOptions()

		switch machineSelector!.selectedTabViewItem!.identifier as! String {
			case "electron":
				return CSStaticAnalyser(electronDFS: electronDFSButton!.state == .on, adfs: electronADFSButton!.state == .on)!

			case "appleii":
				return CSStaticAnalyser(appleII: ())

			case "cpc":
				switch cpcModelTypeButton!.selectedItem!.tag {
					case 464:	return CSStaticAnalyser(amstradCPCModel: .model464)
					case 664:	return CSStaticAnalyser(amstradCPCModel: .model664)
					case 6128:	fallthrough
					default:	return CSStaticAnalyser(amstradCPCModel: .model6128)
				}

			case "msx":
				return CSStaticAnalyser(msxHasDiskDrive: msxHasDiskDriveButton!.state == .on)

			case "oric":
				let hasMicrodrive = oricHasMicrodriveButton!.state == .on
				switch oricModelTypeButton!.selectedItem!.tag {
					case 1:		return CSStaticAnalyser(oricModel: .oric1, hasMicrodrive: hasMicrodrive)
					default:	return CSStaticAnalyser(oricModel: .oricAtmos, hasMicrodrive: hasMicrodrive)
				}

			case "vic20":
				let memorySize = Kilobytes(vic20MemorySizeButton!.selectedItem!.tag)
				let hasC1540 = vic20HasC1540Button!.state == .on
				switch vic20RegionButton!.selectedItem?.tag {
					case 1:
						return CSStaticAnalyser(vic20Region: .american, memorySize: memorySize, hasC1540: hasC1540)
					case 2:
						return CSStaticAnalyser(vic20Region: .danish, memorySize: memorySize, hasC1540: hasC1540)
					case 3:
						return CSStaticAnalyser(vic20Region: .swedish, memorySize: memorySize, hasC1540: hasC1540)
					case 4:
						return CSStaticAnalyser(vic20Region: .japanese, memorySize: memorySize, hasC1540: hasC1540)
					case 0: fallthrough
					default:
						return CSStaticAnalyser(vic20Region: .european, memorySize: memorySize, hasC1540: hasC1540)
				}

			case "zx80":
				return CSStaticAnalyser(zx80MemorySize: Kilobytes(zx80MemorySizeButton!.selectedItem!.tag), useZX81ROM: zx80UsesZX81ROMButton!.state == .on)

			case "zx81":
				return CSStaticAnalyser(zx81MemorySize: Kilobytes(zx81MemorySizeButton!.selectedItem!.tag))

			default: return CSStaticAnalyser()
		}
	}
}

//
//  MachinePicker.swift
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

import Cocoa

class MachinePicker: NSObject {
	@IBOutlet var machineSelector: NSTabView?

	// MARK: - Apple II properties
	@IBOutlet var appleIIModelButton: NSPopUpButton?
	@IBOutlet var appleIIDiskControllerButton: NSPopUpButton?

	// MARK: - Electron properties
	@IBOutlet var electronDFSButton: NSButton?
	@IBOutlet var electronADFSButton: NSButton?

	// MARK: - CPC properties
	@IBOutlet var cpcModelTypeButton: NSPopUpButton?

	// MARK: - Macintosh properties
	@IBOutlet var macintoshModelTypeButton: NSPopUpButton?

	// MARK: - MSX properties
	@IBOutlet var msxRegionButton: NSPopUpButton?
	@IBOutlet var msxHasDiskDriveButton: NSButton?

	// MARK: - Oric properties
	@IBOutlet var oricModelTypeButton: NSPopUpButton?
	@IBOutlet var oricDiskInterfaceButton: NSPopUpButton?

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

		// Apple II settings
		appleIIModelButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.appleIIModel"))
		appleIIDiskControllerButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.appleIIDiskController"))

		// Electron settings
		electronDFSButton?.state = standardUserDefaults.bool(forKey: "new.electronDFS") ? .on : .off
		electronADFSButton?.state = standardUserDefaults.bool(forKey: "new.electronADFS") ? .on : .off

		// CPC settings
		cpcModelTypeButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.cpcModel"))

		// Macintosh settings
		macintoshModelTypeButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.macintoshModel"))

		// MSX settings
		msxRegionButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.msxRegion"))
		msxHasDiskDriveButton?.state = standardUserDefaults.bool(forKey: "new.msxDiskDrive") ? .on : .off

		// Oric settings
		oricDiskInterfaceButton?.selectItem(withTag: standardUserDefaults.integer(forKey: "new.oricDiskInterface"))
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

		// Apple II settings
		standardUserDefaults.set(appleIIModelButton!.selectedTag(), forKey: "new.appleIIModel")
		standardUserDefaults.set(appleIIDiskControllerButton!.selectedTag(), forKey: "new.appleIIDiskController")

		// Electron settings
		standardUserDefaults.set(electronDFSButton!.state == .on, forKey: "new.electronDFS")
		standardUserDefaults.set(electronADFSButton!.state == .on, forKey: "new.electronADFS")

		// CPC settings
		standardUserDefaults.set(cpcModelTypeButton!.selectedTag(), forKey: "new.cpcModel")

		// Macintosh settings
		standardUserDefaults.set(macintoshModelTypeButton!.selectedTag(), forKey: "new.macintoshModel")

		// MSX settings
		standardUserDefaults.set(msxRegionButton!.selectedTag(), forKey: "new.msxRegion")
		standardUserDefaults.set(msxHasDiskDriveButton?.state == .on, forKey: "new.msxDiskDrive")

		// Oric settings
		standardUserDefaults.set(oricDiskInterfaceButton!.selectedTag(), forKey: "new.oricDiskInterface")
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
				var model: CSMachineAppleIIModel = .appleII
				switch appleIIModelButton!.selectedTag() {
					case 1:		model = .appleIIPlus
					case 2:		model = .appleIIe
					case 3:		model = .appleEnhancedIIe
					case 0:		fallthrough
					default:	model = .appleII
				}

				var diskController: CSMachineAppleIIDiskController = .none
				switch appleIIDiskControllerButton!.selectedTag() {
					case 13:	diskController = .thirteenSector
					case 16:	diskController = .sixteenSector
					case 0:		fallthrough
					default:	diskController = .none
				}

				return CSStaticAnalyser(appleIIModel: model, diskController: diskController)

			case "cpc":
				switch cpcModelTypeButton!.selectedItem!.tag {
					case 464:	return CSStaticAnalyser(amstradCPCModel: .model464)
					case 664:	return CSStaticAnalyser(amstradCPCModel: .model664)
					case 6128:	fallthrough
					default:	return CSStaticAnalyser(amstradCPCModel: .model6128)
				}

			case "mac":
				switch macintoshModelTypeButton!.selectedItem!.tag {
					case 0:		return CSStaticAnalyser(macintoshModel: .model128k)
					case 1:		return CSStaticAnalyser(macintoshModel: .model512k)
					case 2:		return CSStaticAnalyser(macintoshModel: .model512ke)
					case 3:		fallthrough
					default:	return CSStaticAnalyser(macintoshModel: .modelPlus)
				}

			case "msx":
				let hasDiskDrive = msxHasDiskDriveButton!.state == .on
				switch msxRegionButton!.selectedItem?.tag {
					case 2:
						return CSStaticAnalyser(msxRegion: .japanese, hasDiskDrive: hasDiskDrive)
					case 1:
						return CSStaticAnalyser(msxRegion: .american, hasDiskDrive: hasDiskDrive)
					case 0: fallthrough
					default:
						return CSStaticAnalyser(msxRegion: .european, hasDiskDrive: hasDiskDrive)
				}

			case "oric":
				var diskInterface: CSMachineOricDiskInterface = .none
				switch oricDiskInterfaceButton!.selectedTag() {
					case 1:		diskInterface = .microdisc
					case 2:		diskInterface = .pravetz
					default:	break;

				}
				var model: CSMachineOricModel = .oric1
				switch oricModelTypeButton!.selectedItem!.tag {
					case 1:		model = .oricAtmos
					case 2:		model = .pravetz
					default:	break;
				}

				return CSStaticAnalyser(oricModel: model, diskInterface: diskInterface)

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

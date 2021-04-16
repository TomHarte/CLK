//
//  MachinePicker.swift
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

import Cocoa

// Quick note on structure below: I have an NSTableView to contain machine names,
// programmatically manipulating the selected tab in an NSTabView. I admit that this
// is odd. It's partly for historical reasons — this was purely an NSTabView until
// the number of options got too large — and partly because it makes designing things
// in the interface builder easier.
//
// I accept that I'll have to rethink this again if the machine list keeps growing.
class MachinePicker: NSObject, NSTableViewDataSource, NSTableViewDelegate {
	@IBOutlet var machineSelector: NSTabView!
	@IBOutlet var machineNameTable: NSTableView!

	// MARK: - Apple II properties
	@IBOutlet var appleIIModelButton: NSPopUpButton!
	@IBOutlet var appleIIDiskControllerButton: NSPopUpButton!

	// MARK: - Apple IIgs properties
	@IBOutlet var appleIIgsModelButton: NSPopUpButton!
	@IBOutlet var appleIIgsMemorySizeButton: NSPopUpButton!

	// MARK: - Electron properties
	@IBOutlet var electronDFSButton: NSButton!
	@IBOutlet var electronADFSButton: NSButton!
	@IBOutlet var electronAP6Button: NSButton!
	@IBOutlet var electronSidewaysRAMButton: NSButton!

	// MARK: - CPC properties
	@IBOutlet var cpcModelTypeButton: NSPopUpButton!

	// MARK: - Macintosh properties
	@IBOutlet var macintoshModelTypeButton: NSPopUpButton!

	// MARK: - MSX properties
	@IBOutlet var msxRegionButton: NSPopUpButton!
	@IBOutlet var msxHasDiskDriveButton: NSButton!

	// MARK: - Oric properties
	@IBOutlet var oricModelTypeButton: NSPopUpButton!
	@IBOutlet var oricDiskInterfaceButton: NSPopUpButton!

	// MARK: - Spectrum properties
	@IBOutlet var spectrumModelTypeButton: NSPopUpButton!

	// MARK: - Vic-20 properties
	@IBOutlet var vic20RegionButton: NSPopUpButton!
	@IBOutlet var vic20MemorySizeButton: NSPopUpButton!
	@IBOutlet var vic20HasC1540Button: NSButton!

	// MARK: - ZX80 properties
	@IBOutlet var zx80MemorySizeButton: NSPopUpButton!
	@IBOutlet var zx80UsesZX81ROMButton: NSButton!

	// MARK: - ZX81 properties
	@IBOutlet var zx81MemorySizeButton: NSPopUpButton!

	// MARK: - Preferences
	func establishStoredOptions() {
		let standardUserDefaults = UserDefaults.standard

		// Set up data soure.

		// TEMPORARY: remove the Apple IIgs option. It's not yet a fully-working machine; no need to publicise it.
		let appleIIgsTabIndex = machineSelector.indexOfTabViewItem(withIdentifier: "appleiigs")
		machineSelector.removeTabViewItem(machineSelector.tabViewItem(at: appleIIgsTabIndex))
		machineNameTable.reloadData()

		// Machine type
		if let machineIdentifier = standardUserDefaults.string(forKey: "new.machine") {
			// If I've changed my mind about visible tabs between versions, there may not be one that corresponds
			// to the stored identifier. Make sure not to raise an NSRangeException in that scenario.
			let index = machineSelector.indexOfTabViewItem(withIdentifier: machineIdentifier as Any)
			if index != NSNotFound {
				machineSelector.selectTabViewItem(at: index)
				machineNameTable.selectRowIndexes(IndexSet(integer: index), byExtendingSelection: false)
				machineNameTable.scrollRowToVisible(index)
			}
		}

		// Apple II settings
		appleIIModelButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.appleIIModel"))
		appleIIDiskControllerButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.appleIIDiskController"))

		// Apple IIgs settings
		appleIIgsModelButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.appleIIgsModel"))
		appleIIgsMemorySizeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.appleIIgsMemorySize"))

		// Electron settings
		electronDFSButton.state = standardUserDefaults.bool(forKey: "new.electronDFS") ? .on : .off
		electronADFSButton.state = standardUserDefaults.bool(forKey: "new.electronADFS") ? .on : .off
		electronAP6Button.state = standardUserDefaults.bool(forKey: "new.electronAP6") ? .on : .off
		electronSidewaysRAMButton.state = standardUserDefaults.bool(forKey: "new.electronSidewaysRAM") ? .on : .off

		// CPC settings
		cpcModelTypeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.cpcModel"))

		// Macintosh settings
		macintoshModelTypeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.macintoshModel"))

		// MSX settings
		msxRegionButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.msxRegion"))
		msxHasDiskDriveButton.state = standardUserDefaults.bool(forKey: "new.msxDiskDrive") ? .on : .off

		// Oric settings
		oricDiskInterfaceButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.oricDiskInterface"))
		oricModelTypeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.oricModel"))

		// Spectrum settings
		spectrumModelTypeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.spectrumModel"))

		// Vic-20 settings
		vic20RegionButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.vic20Region"))
		vic20MemorySizeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.vic20MemorySize"))
		vic20HasC1540Button.state = standardUserDefaults.bool(forKey: "new.vic20C1540") ? .on : .off

		// ZX80
		zx80MemorySizeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.zx80MemorySize"))
		zx80UsesZX81ROMButton.state = standardUserDefaults.bool(forKey: "new.zx80UsesZX81ROM") ? .on : .off

		// ZX81
		zx81MemorySizeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.zx81MemorySize"))
	}

	fileprivate func storeOptions() {
		let standardUserDefaults = UserDefaults.standard

		// Machine type
		standardUserDefaults.set(machineSelector.selectedTabViewItem!.identifier as! String, forKey: "new.machine")

		// Apple II settings
		standardUserDefaults.set(appleIIModelButton.selectedTag(), forKey: "new.appleIIModel")
		standardUserDefaults.set(appleIIDiskControllerButton.selectedTag(), forKey: "new.appleIIDiskController")

		// Apple IIgs settings
		standardUserDefaults.set(appleIIgsModelButton.selectedTag(), forKey: "new.appleIIgsModel")
		standardUserDefaults.set(appleIIgsMemorySizeButton.selectedTag(), forKey: "new.appleIIgsMemorySize")

		// Electron settings
		standardUserDefaults.set(electronDFSButton.state == .on, forKey: "new.electronDFS")
		standardUserDefaults.set(electronADFSButton.state == .on, forKey: "new.electronADFS")
		standardUserDefaults.set(electronAP6Button.state == .on, forKey: "new.electronAP6")
		standardUserDefaults.set(electronSidewaysRAMButton.state == .on, forKey: "new.electronSidewaysRAM")

		// CPC settings
		standardUserDefaults.set(cpcModelTypeButton.selectedTag(), forKey: "new.cpcModel")

		// Macintosh settings
		standardUserDefaults.set(macintoshModelTypeButton.selectedTag(), forKey: "new.macintoshModel")

		// MSX settings
		standardUserDefaults.set(msxRegionButton.selectedTag(), forKey: "new.msxRegion")
		standardUserDefaults.set(msxHasDiskDriveButton.state == .on, forKey: "new.msxDiskDrive")

		// Oric settings
		standardUserDefaults.set(oricDiskInterfaceButton.selectedTag(), forKey: "new.oricDiskInterface")
		standardUserDefaults.set(oricModelTypeButton.selectedTag(), forKey: "new.oricModel")

		// Spectrum settings
		standardUserDefaults.set(spectrumModelTypeButton.selectedTag(), forKey: "new.spectrumModel")

		// Vic-20 settings
		standardUserDefaults.set(vic20RegionButton.selectedTag(), forKey: "new.vic20Region")
		standardUserDefaults.set(vic20MemorySizeButton.selectedTag(), forKey: "new.vic20MemorySize")
		standardUserDefaults.set(vic20HasC1540Button.state == .on, forKey: "new.vic20C1540")

		// ZX80
		standardUserDefaults.set(zx80MemorySizeButton.selectedTag(), forKey: "new.zx80MemorySize")
		standardUserDefaults.set(zx80UsesZX81ROMButton.state == .on, forKey: "new.zx80UsesZX81ROM")

		// ZX81
		standardUserDefaults.set(zx81MemorySizeButton.selectedTag(), forKey: "new.zx81MemorySize")
	}

	// MARK: - Table view handling
	func numberOfRows(in tableView: NSTableView) -> Int {
		return machineSelector.numberOfTabViewItems
	}

	func tableView(_ tableView: NSTableView, objectValueFor tableColumn: NSTableColumn?, row: Int) -> Any? {
		return machineSelector.tabViewItem(at: row).label
	}

	func tableViewSelectionDidChange(_ notification: Notification) {
		machineSelector.selectTabViewItem(at: machineNameTable.selectedRow)
	}

	func tableView(_ tableView: NSTableView, heightOfRow row: Int) -> CGFloat {
		let font = NSFont.systemFont(ofSize: NSFont.systemFontSize)

		// YUCK. TODO: find a way to use cells with vertically-centred text.
		// Likely that means not using NSTextFieldCell.
		return font.ascender - font.descender + 2.5
	}

	// MARK: - Machine builder
	func selectedMachine() -> CSStaticAnalyser {
		storeOptions()

		switch machineSelector.selectedTabViewItem!.identifier as! String {
			case "electron":
				return CSStaticAnalyser(
					electronDFS: electronDFSButton.state == .on,
					adfs: electronADFSButton.state == .on,
					ap6: electronAP6Button.state == .on,
					sidewaysRAM: electronSidewaysRAMButton.state == .on)

			case "appleii":
				var model: CSMachineAppleIIModel = .appleII
				switch appleIIModelButton.selectedTag() {
					case 1:		model = .appleIIPlus
					case 2:		model = .appleIIe
					case 3:		model = .appleEnhancedIIe
					case 0:		fallthrough
					default:	model = .appleII
				}

				var diskController: CSMachineAppleIIDiskController = .none
				switch appleIIDiskControllerButton.selectedTag() {
					case 13:	diskController = .thirteenSector
					case 16:	diskController = .sixteenSector
					case 0:		fallthrough
					default:	diskController = .none
				}

				return CSStaticAnalyser(appleIIModel: model, diskController: diskController)

			case "appleiigs":
				var model: CSMachineAppleIIgsModel = .ROM00
				switch appleIIgsModelButton.selectedTag() {
					case 1:		model = .ROM01
					case 2:		model = .ROM03
					case 0:		fallthrough
					default:	model = .ROM00
				}

				let memorySize = Kilobytes(appleIIgsMemorySizeButton.selectedItem!.tag)
				return CSStaticAnalyser(appleIIgsModel: model, memorySize: memorySize)

			case "atarist":
				return CSStaticAnalyser(atariSTModel: .model512k)

			case "cpc":
				switch cpcModelTypeButton.selectedItem!.tag {
					case 464:	return CSStaticAnalyser(amstradCPCModel: .model464)
					case 664:	return CSStaticAnalyser(amstradCPCModel: .model664)
					case 6128:	fallthrough
					default:	return CSStaticAnalyser(amstradCPCModel: .model6128)
				}

			case "mac":
				switch macintoshModelTypeButton.selectedItem!.tag {
					case 0:		return CSStaticAnalyser(macintoshModel: .model128k)
					case 1:		return CSStaticAnalyser(macintoshModel: .model512k)
					case 2:		return CSStaticAnalyser(macintoshModel: .model512ke)
					case 3:		fallthrough
					default:	return CSStaticAnalyser(macintoshModel: .modelPlus)
				}

			case "msx":
				let hasDiskDrive = msxHasDiskDriveButton.state == .on
				switch msxRegionButton.selectedItem!.tag {
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
				switch oricDiskInterfaceButton.selectedTag() {
					case 1:		diskInterface = .microdisc
					case 2:		diskInterface = .pravetz
					case 3:		diskInterface = .jasmin
					case 4:		diskInterface = .BD500
					default:	break

				}
				var model: CSMachineOricModel = .oric1
				switch oricModelTypeButton.selectedItem!.tag {
					case 1:		model = .oricAtmos
					case 2:		model = .pravetz
					default:	break
				}

				return CSStaticAnalyser(oricModel: model, diskInterface: diskInterface)

			case "spectrum":
				var model: CSMachineSpectrumModel = .plus2a
				switch spectrumModelTypeButton.selectedItem!.tag {
					case 16:	model = .sixteenK
					case 48:	model = .fortyEightK
					case 128:	model = .oneTwoEightK
					case 2:		model = .plus2
					case 21:	model = .plus2a
					case 3:		model = .plus3
					default:	break
				}

				return CSStaticAnalyser(spectrumModel: model)

			case "vic20":
				let memorySize = Kilobytes(vic20MemorySizeButton.selectedItem!.tag)
				let hasC1540 = vic20HasC1540Button.state == .on
				switch vic20RegionButton.selectedItem!.tag {
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
				return CSStaticAnalyser(zx80MemorySize: Kilobytes(zx80MemorySizeButton.selectedItem!.tag), useZX81ROM: zx80UsesZX81ROMButton.state == .on)

			case "zx81":
				return CSStaticAnalyser(zx81MemorySize: Kilobytes(zx81MemorySizeButton.selectedItem!.tag))

			default: return CSStaticAnalyser()
		}
	}
}

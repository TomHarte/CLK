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

	// MARK: - Amiga properties
	@IBOutlet var amigaChipRAMButton: NSPopUpButton!
	@IBOutlet var amigaFastRAMButton: NSPopUpButton!

	// MARK: - Apple II properties
	@IBOutlet var appleIIModelButton: NSPopUpButton!
	@IBOutlet var appleIIDiskControllerButton: NSPopUpButton!

	// MARK: - Apple IIgs properties
	@IBOutlet var appleIIgsModelButton: NSPopUpButton!
	@IBOutlet var appleIIgsMemorySizeButton: NSPopUpButton!

	// MARK: - Atari ST properties
	@IBOutlet var atariSTMemorySizeButton: NSPopUpButton!

	// MARK: - CPC properties
	@IBOutlet var cpcModelTypeButton: NSPopUpButton!

	// MARK: - Electron properties
	@IBOutlet var electronDFSButton: NSButton!
	@IBOutlet var electronADFSButton: NSButton!
	@IBOutlet var electronAP6Button: NSButton!
	@IBOutlet var electronSidewaysRAMButton: NSButton!

	// MARK: - Enterprise properties
	@IBOutlet var enterpriseModelButton: NSPopUpButton!
	@IBOutlet var enterpriseSpeedButton: NSPopUpButton!
	@IBOutlet var enterpriseEXOSButton: NSPopUpButton!
	@IBOutlet var enterpriseBASICButton: NSPopUpButton!
	@IBOutlet var enterpriseDOSButton: NSPopUpButton!

	// MARK: - Macintosh properties
	@IBOutlet var macintoshModelTypeButton: NSPopUpButton!

	// MARK: - MSX properties
	@IBOutlet var msxModelButton: NSPopUpButton!
	@IBOutlet var msxRegionButton: NSPopUpButton!
	@IBOutlet var msxHasDiskDriveButton: NSButton!
	@IBOutlet var msxHasMSXMUSICButton: NSButton!

	// MARK: - Oric properties
	@IBOutlet var oricModelTypeButton: NSPopUpButton!
	@IBOutlet var oricDiskInterfaceButton: NSPopUpButton!

	// MARK: - PC compatible properties
	@IBOutlet var pcVideoAdaptorButton: NSPopUpButton!
	@IBOutlet var pcSpeedButton: NSPopUpButton!

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

		// TEMPORARY: remove the Apple IIgs and PC compatible options.
		// Neither is yet a fully-working machine.
		#if !DEBUG
		for hidden in ["appleiigs"] {
			let tabIndex = machineSelector.indexOfTabViewItem(withIdentifier: hidden)
			machineSelector.removeTabViewItem(machineSelector.tabViewItem(at: tabIndex))
		}
		machineNameTable.reloadData()
		#endif

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

		// Amiga settings
		amigaChipRAMButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.amigaChipRAM"))
		amigaFastRAMButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.amigaFastRAM"))

		// Apple II settings
		appleIIModelButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.appleIIModel"))
		appleIIDiskControllerButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.appleIIDiskController"))

		// Apple IIgs settings
		appleIIgsModelButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.appleIIgsModel"))
		appleIIgsMemorySizeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.appleIIgsMemorySize"))

		// Atari ST settings
		atariSTMemorySizeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.atariSTMemorySize"))

		// CPC settings
		cpcModelTypeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.cpcModel"))

		// Electron settings
		electronDFSButton.state = standardUserDefaults.bool(forKey: "new.electronDFS") ? .on : .off
		electronADFSButton.state = standardUserDefaults.bool(forKey: "new.electronADFS") ? .on : .off
		electronAP6Button.state = standardUserDefaults.bool(forKey: "new.electronAP6") ? .on : .off
		electronSidewaysRAMButton.state = standardUserDefaults.bool(forKey: "new.electronSidewaysRAM") ? .on : .off

		// Enterprise settings
		enterpriseModelButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.enterpriseModel"))
		enterpriseSpeedButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.enterpriseSpeed"))
		enterpriseEXOSButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.enterpriseEXOSVersion"))
		enterpriseBASICButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.enterpriseBASICVersion"))
		enterpriseDOSButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.enterpriseDOS"))

		// Macintosh settings
		macintoshModelTypeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.macintoshModel"))

		// MSX settings
		msxModelButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.msxModel"))
		msxRegionButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.msxRegion"))
		msxHasDiskDriveButton.state = standardUserDefaults.bool(forKey: "new.msxDiskDrive") ? .on : .off
		msxHasMSXMUSICButton.state = standardUserDefaults.bool(forKey: "new.msxMSXMUSIC") ? .on : .off

		// Oric settings
		oricDiskInterfaceButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.oricDiskInterface"))
		oricModelTypeButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.oricModel"))

		// PC settings
		pcVideoAdaptorButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.pcVideoAdaptor"))
		pcSpeedButton.selectItem(withTag: standardUserDefaults.integer(forKey: "new.pcSpeed"))

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

		// Amiga settings
		standardUserDefaults.set(amigaChipRAMButton.selectedTag(), forKey: "new.amigaChipRAM")
		standardUserDefaults.set(amigaFastRAMButton.selectedTag(), forKey: "new.amigaFastRAM")

		// Apple II settings
		standardUserDefaults.set(appleIIModelButton.selectedTag(), forKey: "new.appleIIModel")
		standardUserDefaults.set(appleIIDiskControllerButton.selectedTag(), forKey: "new.appleIIDiskController")

		// Apple IIgs settings
		standardUserDefaults.set(appleIIgsModelButton.selectedTag(), forKey: "new.appleIIgsModel")
		standardUserDefaults.set(appleIIgsMemorySizeButton.selectedTag(), forKey: "new.appleIIgsMemorySize")

		// Atari ST settings
		standardUserDefaults.set(atariSTMemorySizeButton.selectedTag(), forKey: "new.atariSTMemorySize")

		// CPC settings
		standardUserDefaults.set(cpcModelTypeButton.selectedTag(), forKey: "new.cpcModel")

		// Electron settings
		standardUserDefaults.set(electronDFSButton.state == .on, forKey: "new.electronDFS")
		standardUserDefaults.set(electronADFSButton.state == .on, forKey: "new.electronADFS")
		standardUserDefaults.set(electronAP6Button.state == .on, forKey: "new.electronAP6")
		standardUserDefaults.set(electronSidewaysRAMButton.state == .on, forKey: "new.electronSidewaysRAM")

		// Enterprise settings
		standardUserDefaults.set(enterpriseModelButton.selectedTag(), forKey: "new.enterpriseModel")
		standardUserDefaults.set(enterpriseSpeedButton.selectedTag(), forKey: "new.enterpriseSpeed")
		standardUserDefaults.set(enterpriseEXOSButton.selectedTag(), forKey: "new.enterpriseEXOSVersion")
		standardUserDefaults.set(enterpriseBASICButton.selectedTag(), forKey: "new.enterpriseBASICVersion")
		standardUserDefaults.set(enterpriseDOSButton.selectedTag(), forKey: "new.enterpriseDOS")

		// Macintosh settings
		standardUserDefaults.set(macintoshModelTypeButton.selectedTag(), forKey: "new.macintoshModel")

		// MSX settings
		standardUserDefaults.set(msxModelButton.selectedTag(), forKey: "new.msxModel")
		standardUserDefaults.set(msxRegionButton.selectedTag(), forKey: "new.msxRegion")
		standardUserDefaults.set(msxHasDiskDriveButton.state == .on, forKey: "new.msxDiskDrive")
		standardUserDefaults.set(msxHasMSXMUSICButton.state == .on, forKey: "new.msxMSXMUSIC")

		// Oric settings
		standardUserDefaults.set(oricDiskInterfaceButton.selectedTag(), forKey: "new.oricDiskInterface")
		standardUserDefaults.set(oricModelTypeButton.selectedTag(), forKey: "new.oricModel")

		// PC settings
		standardUserDefaults.set(pcVideoAdaptorButton.selectedTag(), forKey: "new.pcVideoAdaptor")
		standardUserDefaults.set(pcSpeedButton.selectedTag(), forKey: "new.pcSpeed")

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

			case "amiga":
				return CSStaticAnalyser(amigaModel: .A500, chipMemorySize: Kilobytes(amigaChipRAMButton.selectedTag()), fastMemorySize: Kilobytes(amigaFastRAMButton.selectedTag()))

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

				let memorySize = Kilobytes(appleIIgsMemorySizeButton.selectedTag())
				return CSStaticAnalyser(appleIIgsModel: model, memorySize: memorySize)

			case "atarist":
				let memorySize = Kilobytes(atariSTMemorySizeButton.selectedTag())
				return CSStaticAnalyser(atariSTModel: .model512k, memorySize: memorySize)

			case "cpc":
				switch cpcModelTypeButton.selectedTag() {
					case 464:	return CSStaticAnalyser(amstradCPCModel: .model464)
					case 664:	return CSStaticAnalyser(amstradCPCModel: .model664)
					case 6128:	fallthrough
					default:	return CSStaticAnalyser(amstradCPCModel: .model6128)
				}

			case "electron":
				return CSStaticAnalyser(
					electronDFS: electronDFSButton.state == .on,
					adfs: electronADFSButton.state == .on,
					ap6: electronAP6Button.state == .on,
					sidewaysRAM: electronSidewaysRAMButton.state == .on)

			case "enterprise":
				var model: CSMachineEnterpriseModel = .model128
				switch enterpriseModelButton.selectedTag() {
					case 64:	model = .model64
					case 256:	model = .model256
					case 128:	fallthrough
					default:	model = .model128
				}

				var speed: CSMachineEnterpriseSpeed = .speed4MHz
				switch enterpriseSpeedButton.selectedTag() {
					case 6:		speed = .speed6MHz
					case 4:		fallthrough
					default:	speed = .speed4MHz
				}

				var exos: CSMachineEnterpriseEXOS = .version21
				switch enterpriseEXOSButton.selectedTag() {
					case 10:	exos = .version10
					case 20:	exos = .version20
					case 21:	fallthrough
					default:	exos = .version21
				}

				var basic: CSMachineEnterpriseBASIC = .version21
				switch enterpriseBASICButton.selectedTag() {
					case 0:		basic = .none
					case 10:	basic = .version10
					case 11:	basic = .version11
					case 21:	fallthrough
					default:	basic = .version21
				}

				var dos: CSMachineEnterpriseDOS = .dosNone
				switch enterpriseDOSButton.selectedTag() {
					case 1:		dos = .DOSEXDOS
					case 0:		fallthrough
					default:	dos = .dosNone
				}

				return CSStaticAnalyser(enterpriseModel: model, speed: speed, exosVersion: exos, basicVersion: basic, dos: dos)

			case "mac":
				switch macintoshModelTypeButton.selectedTag() {
					case 0:		return CSStaticAnalyser(macintoshModel: .model128k)
					case 1:		return CSStaticAnalyser(macintoshModel: .model512k)
					case 2:		return CSStaticAnalyser(macintoshModel: .model512ke)
					case 3:		fallthrough
					default:	return CSStaticAnalyser(macintoshModel: .modelPlus)
				}

			case "msx":
				let hasDiskDrive = msxHasDiskDriveButton.state == .on
				let hasMSXMUSIC = msxHasMSXMUSICButton.state == .on
				var region: CSMachineMSXRegion
				switch msxRegionButton.selectedTag() {
					case 2:		region = .japanese
					case 1:		region = .american
					case 0:		fallthrough
					default:	region = .european
				}
				var model: CSMachineMSXModel
				switch msxModelButton.selectedTag() {
					case 2:		model = .MSX2
					case 1:		fallthrough
					default:	model = .MSX1
				}
				return CSStaticAnalyser(msxModel: model, region: region, hasDiskDrive: hasDiskDrive, hasMSXMUSIC: hasMSXMUSIC)

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
				switch oricModelTypeButton.selectedTag() {
					case 1:		model = .oricAtmos
					case 2:		model = .pravetz
					default:	break
				}

				return CSStaticAnalyser(oricModel: model, diskInterface: diskInterface)

			case "pc":
				var videoAdaptor: CSPCCompatibleVideoAdaptor = .MDA
				switch pcVideoAdaptorButton.selectedTag() {
					case 1:		videoAdaptor = .CGA
					default:	break
				}
				var speed: CSPCCompatibleSpeed = .original
				switch pcSpeedButton.selectedTag() {
					case 80286:	speed = .turbo
					default:	break
				}
				return CSStaticAnalyser(pcCompatibleSpeed: speed, videoAdaptor: videoAdaptor)

			case "spectrum":
				var model: CSMachineSpectrumModel = .plus2a
				switch spectrumModelTypeButton.selectedTag() {
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
				let memorySize = Kilobytes(vic20MemorySizeButton.selectedTag())
				let hasC1540 = vic20HasC1540Button.state == .on
				switch vic20RegionButton.selectedTag() {
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
				return CSStaticAnalyser(zx80MemorySize: Kilobytes(zx80MemorySizeButton.selectedTag()), useZX81ROM: zx80UsesZX81ROMButton.state == .on)

			case "zx81":
				return CSStaticAnalyser(zx81MemorySize: Kilobytes(zx81MemorySizeButton.selectedTag()))

			default: return CSStaticAnalyser()
		}
	}
}

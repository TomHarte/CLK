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
class MachinePicker: NSObject, NSTableViewDataSource, NSTableViewDelegate, NSPathControlDelegate {
	@IBOutlet var machineSelector: NSTabView!
	@IBOutlet var machineNameTable: NSTableView!

	// MARK: - Amiga properties
	@IBOutlet var amigaChipRAMButton: NSPopUpButton!
	@IBOutlet var amigaFastRAMButton: NSPopUpButton!

	// MARK: - Apple II properties
	@IBOutlet var appleIIModelButton: NSPopUpButton!
	@IBOutlet var appleIIDiskControllerButton: NSPopUpButton!
	@IBOutlet var appleIIMockingboardButton: NSButton!

	// MARK: - Apple IIgs properties
	@IBOutlet var appleIIgsModelButton: NSPopUpButton!
	@IBOutlet var appleIIgsMemorySizeButton: NSPopUpButton!

	// MARK: - Atari ST properties
	@IBOutlet var atariSTMemorySizeButton: NSPopUpButton!

	// MARK: - BBC Micro properties
	@IBOutlet var bbcDFSButton: NSButton!
	@IBOutlet var bbcADFSButton: NSButton!
	@IBOutlet var bbcSidewaysRAMButton: NSButton!
	@IBOutlet var bbcBeebSIDButton: NSButton!
	@IBOutlet var bbcSecondProcessorButton: NSPopUpButton!

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

	@IBOutlet var enterpriseExposePathButton: NSButton!
	@IBOutlet var enterprisePathControl: NSPathControl!

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

	// MARK: - Plus 4 properties
	@IBOutlet var plus4HasC1541Button: NSButton!

	// MARK: - PC compatible properties
	@IBOutlet var pcVideoAdaptorButton: NSPopUpButton!
	@IBOutlet var pcSpeedButton: NSPopUpButton!

	// MARK: - Spectrum properties
	@IBOutlet var spectrumModelTypeButton: NSPopUpButton!

	// MARK: - Thomson
	@IBOutlet var thomsonDiskButton: NSButton!

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

	private func applyToControls(_ function: (_: NSControl, _: String) -> Void) {
		// Amiga settings
		function(amigaChipRAMButton!, "new.amigaChipRAM")
		function(amigaFastRAMButton!, "new.amigaFastRAM")

		// Apple II settings
		function(appleIIModelButton!, "new.appleIIModel")
		function(appleIIDiskControllerButton!, "new.appleIIDiskController")
		function(appleIIMockingboardButton!, "new.appleIIMockingboard")

		// Apple IIgs settings
		function(appleIIgsModelButton!, "new.appleIIgsModel")
		function(appleIIgsMemorySizeButton!, "new.appleIIgsMemorySize")

		// Atari ST settings
		function(atariSTMemorySizeButton!, "new.atariSTMemorySize")

		// BBC Micro settings
		function(bbcDFSButton!, "new.bbcDFS")
		function(bbcADFSButton!, "new.bbcADFS")
		function(bbcSidewaysRAMButton!, "new.bbcSidewaysRAM")
		function(bbcBeebSIDButton!, "new.bbcBeebSID")
		function(bbcSecondProcessorButton!, "new.bbcSecondProcessor")

		// CPC settings
		function(cpcModelTypeButton!, "new.cpcModel")

		// Electron settings
		function(electronDFSButton!, "new.electronDFS")
		function(electronADFSButton!, "new.electronADFS")
		function(electronAP6Button!, "new.electronAP6")
		function(electronSidewaysRAMButton!, "new.electronSidewaysRAM")

		// Enterprise settings
		function(enterpriseModelButton!, "new.enterpriseModel")
		function(enterpriseSpeedButton!, "new.enterpriseSpeed")
		function(enterpriseEXOSButton!, "new.enterpriseEXOSVersion")
		function(enterpriseBASICButton!, "new.enterpriseBASICVersion")
		function(enterpriseDOSButton!, "new.enterpriseDOS")

		function(enterpriseExposePathButton!, "new.enterpriseExposeLocalPath")
		function(enterprisePathControl!, "new.enterpriseExposedLocalPath")

		// Macintosh settings
		function(macintoshModelTypeButton!, "new.macintoshModel")

		// MSX settings
		function(msxModelButton!, "new.msxModel")
		function(msxRegionButton!, "new.msxRegion")
		function(msxHasDiskDriveButton!, "new.msxDiskDrive")
		function(msxHasMSXMUSICButton!, "new.msxMSXMUSIC")

		// Oric settings
		function(oricDiskInterfaceButton!, "new.oricDiskInterface")
		function(oricModelTypeButton!, "new.oricModel")

		// Plus 4 settings
		function(plus4HasC1541Button!, "new.plus4C1541")

		// PC settings
		function(pcVideoAdaptorButton!, "new.pcVideoAdaptor")
		function(pcSpeedButton!, "new.pcSpeed")

		// Spectrum settings
		function(spectrumModelTypeButton!, "new.spectrumModel")

		// Thomson settings
		function(thomsonDiskButton!, "new.thomsonDiskDrive")

		// Vic-20 settings
		function(vic20RegionButton!, "new.vic20Region")
		function(vic20MemorySizeButton!, "new.vic20MemorySize")
		function(vic20HasC1540Button!, "new.vic20C1540")

		// ZX80
		function(zx80MemorySizeButton!, "new.zx80MemorySize")
		function(zx80UsesZX81ROMButton!, "new.zx80UsesZX81ROM")

		// ZX81
		function(zx81MemorySizeButton!, "new.zx81MemorySize")
	}

	func establishStoredOptions() {
		let standardUserDefaults = UserDefaults.standard

		#if !DEBUG
		// Remove options that are not yet fully working, except in debug builds.
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
			let index = machineSelector.indexOfTabViewItem(withIdentifier: machineIdentifier)
			if index != NSNotFound {
				machineSelector.selectTabViewItem(at: index)
				machineNameTable.selectRowIndexes(IndexSet(integer: index), byExtendingSelection: false)
				machineNameTable.scrollRowToVisible(index)
			}
		}

		// Per-machine controls.
		self.applyToControls { (control: NSControl, name: String) in
			if let popUp = control as? NSPopUpButton {
				popUp.selectItem(withTag: standardUserDefaults.integer(forKey: name))
				return
			}

			if let button = control as? NSButton {
				button.state = standardUserDefaults.bool(forKey: name) ? .on : .off
				return
			}

			if let pathControl = control as? NSPathControl {
				establishPathControl(pathControl, userDefaultsKey: name)
				return
			}

			Swift.print("Unable to establish stored state for control \(control)")
		}
	}

	fileprivate func storeOptions() {
		let standardUserDefaults = UserDefaults.standard

		// Machine type
		standardUserDefaults.set(machineSelector.selectedTabViewItem!.identifier as! String, forKey: "new.machine")

		// Per-machine controls.
		self.applyToControls { (control: NSControl, name: String) in
			if let popUp = control as? NSPopUpButton {
				standardUserDefaults.set(popUp.selectedTag(), forKey: name)
				return
			}

			if let button = control as? NSButton {
				standardUserDefaults.set(button.state == .on, forKey: name)
				return
			}

			if let pathControl = control as? NSPathControl {
				storePathControl(pathControl, userDefaultsKey: name)
				return
			}

			Swift.print("Unable to store state for control \(control)")
		}
	}

	// MARK: - NSTableViewDataSource and NSTableViewDelegate

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
				return CSStaticAnalyser(
					amigaModel: .A500,
					chipMemorySize: Kilobytes(amigaChipRAMButton.selectedTag()),
					fastMemorySize: Kilobytes(amigaFastRAMButton.selectedTag())
				)

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

				return CSStaticAnalyser(
					appleIIModel: model,
					diskController: diskController,
					hasMockingboard: appleIIMockingboardButton.state == .on
				)

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

			case "archimedes":
				return CSStaticAnalyser(archimedesModel: .firstGeneration)

			case "atarist":
				let memorySize = Kilobytes(atariSTMemorySizeButton.selectedTag())
				return CSStaticAnalyser(atariSTMemorySize: memorySize)

			case "bbcmicro":
				var secondProcessor: CSMachineBBCMicroSecondProcessor = .processorNone
				switch bbcSecondProcessorButton.selectedTag() {
					case 6502:	secondProcessor = .processor65C02
					case 80:	secondProcessor = .processorZ80
					case 0:		fallthrough
					default:	secondProcessor = .processorNone
				}
				return CSStaticAnalyser(
					bbcMicroDFS: bbcDFSButton.state == .on,
					adfs: bbcADFSButton.state == .on,
					sidewaysRAM: bbcSidewaysRAMButton.state == .on,
					beebSID: bbcBeebSIDButton.state == .on,
					secondProcessor: secondProcessor)

			case "c16plus4":
				let hasC1541 = plus4HasC1541Button.state == .on
				return CSStaticAnalyser(commodoreTEDModel: .C16, hasC1541: hasC1541)

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

				return CSStaticAnalyser(
					enterpriseModel: model,
					speed: speed,
					exosVersion: exos,
					basicVersion: basic,
					dos: dos,
					exposedLocalPath: enterpriseExposePathButton.state == .on ? enterprisePathControl.url : nil
				)

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
				return CSStaticAnalyser(
					msxModel: model,
					region: region,
					hasDiskDrive: hasDiskDrive,
					hasMSXMUSIC: hasMSXMUSIC
				)

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

			case "thomsonmo":
				return CSStaticAnalyser(thomsonMOHasDiskDrive: thomsonDiskButton.state == .on)

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
				return CSStaticAnalyser(
					zx80MemorySize: Kilobytes(zx80MemorySizeButton.selectedTag()),
					useZX81ROM: zx80UsesZX81ROMButton.state == .on
				)

			case "zx81":
				return CSStaticAnalyser(zx81MemorySize: Kilobytes(zx81MemorySizeButton.selectedTag()))

			default: return CSStaticAnalyser()
		}
	}

	// MARK: - NSPathControlDelegate (and paths in general)

	func pathControl(_ pathControl: NSPathControl, willDisplay openPanel: NSOpenPanel) {
		openPanel.canChooseFiles = false
		openPanel.canChooseDirectories = true
	}

	func pathControl(_ pathControl: NSPathControl, validateDrop info: any NSDraggingInfo) -> NSDragOperation {
		// Accept only directories.
		if let url = NSURL(from: info.draggingPasteboard) {
			if url.hasDirectoryPath {
				return NSDragOperation.link
			}
		}
		return []
	}

	func establishPathControl(_ pathControl: NSPathControl, userDefaultsKey: String) {
		pathControl.url = FileManager.default.homeDirectoryForCurrentUser
		if let bookmarkData = UserDefaults.standard.data(forKey: userDefaultsKey) {
			var isStale: Bool = false
			if let url = try? URL(resolvingBookmarkData: bookmarkData, bookmarkDataIsStale: &isStale) {
				enterprisePathControl.url = url
			}
		}
	}

	func storePathControl(_ pathControl: NSPathControl, userDefaultsKey: String) {
		let url = pathControl.url
		if let bookmarkData = try? url?.bookmarkData(options: [.withSecurityScope]) {
			UserDefaults.standard.set(bookmarkData, forKey: userDefaultsKey)
		}
	}
}

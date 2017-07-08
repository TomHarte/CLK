//
//  ZX8081OptionsPanel.swift
//  Clock Signal
//
//  Created by Thomas Harte on 08/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import Cocoa

class ZX8081OptionsPanel: MachinePanel {

	@IBOutlet var tapeControlButton: NSButton!
	@IBAction func setAutomaticTapeConrol(_ sender: NSButton!) {
		Swift.print("tape control \(tapeControlButton)")
	}
	@IBAction func playOrPauseTape(_ sender: NSButton!) {
		Swift.print("play/pause")
	}
}

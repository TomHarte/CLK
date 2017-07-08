//
//  ZX8081OptionsPanel.swift
//  Clock Signal
//
//  Created by Thomas Harte on 08/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

class ZX8081OptionsPanel: MachinePanel {
	var zx8081: CSZX8081! {
		get {
			return self.machine as! CSZX8081
		}
	}

	@IBOutlet var automaticTapeMotorControlButton: NSButton!
	var automaticTapeMotorControlDefaultsKey: String {
		get { return prefixedUserDefaultsKey("automaticTapeMotorControl") }
	}
	@IBAction func setAutomaticTapeMotorConrol(_ sender: NSButton!) {
		let isEnabled = sender.state == NSOnState
		UserDefaults.standard.set(isEnabled, forKey: self.automaticTapeMotorControlDefaultsKey)
		self.playOrPauseTapeButton.isEnabled = !isEnabled
		self.zx8081.useAutomaticTapeMotorControl = isEnabled
	}

	@IBOutlet var playOrPauseTapeButton: NSButton!
	@IBAction func playOrPauseTape(_ sender: NSButton!) {
		self.zx8081.tapeIsPlaying = !self.zx8081.tapeIsPlaying
		self.playOrPauseTapeButton.title = self.zx8081.tapeIsPlaying
			? NSLocalizedString("Stop Tape", comment: "Text for a button that will stop tape playback")
			: NSLocalizedString("Play Tape", comment: "Text for a button that will start tape playback")
	}

	// MARK: option restoration
	override func establishStoredOptions() {
		super.establishStoredOptions()

		let standardUserDefaults = UserDefaults.standard
		standardUserDefaults.register(defaults: [
			self.automaticTapeMotorControlDefaultsKey: true
		])

		let automaticTapeMotorControlIsEnabled = standardUserDefaults.bool(forKey: self.automaticTapeMotorControlDefaultsKey)
		self.automaticTapeMotorControlButton.state = automaticTapeMotorControlIsEnabled ? NSOnState : NSOffState
		self.playOrPauseTapeButton.isEnabled = !automaticTapeMotorControlIsEnabled
	}
}

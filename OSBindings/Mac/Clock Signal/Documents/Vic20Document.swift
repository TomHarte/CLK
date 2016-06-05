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
	}
}

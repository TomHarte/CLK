//
//  Bundle+DataForResource.swift
//  Clock Signal
//
//  Created by Thomas Harte on 02/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Foundation

extension Bundle {
	func dataForResource(_ name : String, ofType type: String, inDirectory directory: String) -> Data? {
		if let path = self.path(forResource: name, ofType: type, inDirectory: directory) {
			return try? Data(contentsOf: URL(fileURLWithPath: path))
		}
		return nil
	}

}

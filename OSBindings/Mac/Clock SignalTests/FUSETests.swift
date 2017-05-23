//
//  FUSETests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 21/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class FUSETests: XCTestCase {

	func testFUSE() {
		if	let inputFilename = Bundle(for: type(of: self)).path(forResource: "tests", ofType: "in"),
			let outputFilename = Bundle(for: type(of: self)).path(forResource: "tests", ofType: "expected") {
			if	let input = try? String(contentsOf: URL(fileURLWithPath: inputFilename), encoding: .utf8),
				let output = try? String(contentsOf: URL(fileURLWithPath: outputFilename), encoding: .utf8) {

				let inputScanner = Scanner(string: input)
				let outputScanner = Scanner(string: output)

				while !inputScanner.isAtEnd {
					autoreleasepool {
						var name: NSString?
						inputScanner.scanUpToCharacters(from: CharacterSet.newlines, into: &name)
						if let name = name {
							let machine = CSTestMachineZ80()

							var af: UInt32 = 0, bc: UInt32 = 0, de: UInt32 = 0, hl: UInt32 = 0
							var afDash: UInt32 = 0, bcDash: UInt32 = 0, deDash: UInt32 = 0, hlDash: UInt32 = 0
							var ix: UInt32 = 0, iy: UInt32 = 0, sp: UInt32 = 0, pc: UInt32 = 0
							var i: UInt32 = 0, r: UInt32 = 0, iff1: UInt32 = 0, iff2: UInt32 = 0, interruptMode: UInt32 = 0
							var isHalted: UInt32 = 0, tStates: UInt32 = 0

							inputScanner.scanHexInt32(&af)
							inputScanner.scanHexInt32(&bc)
							inputScanner.scanHexInt32(&de)
							inputScanner.scanHexInt32(&hl)
							inputScanner.scanHexInt32(&afDash)
							inputScanner.scanHexInt32(&bcDash)
							inputScanner.scanHexInt32(&deDash)
							inputScanner.scanHexInt32(&hlDash)
							inputScanner.scanHexInt32(&ix)
							inputScanner.scanHexInt32(&iy)
							inputScanner.scanHexInt32(&sp)
							inputScanner.scanHexInt32(&pc)
							inputScanner.scanHexInt32(&i)
							inputScanner.scanHexInt32(&r)
							inputScanner.scanHexInt32(&iff1)
							inputScanner.scanHexInt32(&iff2)
							inputScanner.scanHexInt32(&interruptMode)
							inputScanner.scanHexInt32(&isHalted)
							inputScanner.scanHexInt32(&tStates)

							print("\(name)")
							machine.setValue(UInt16(af), for: .AF)
							machine.setValue(UInt16(bc), for: .BC)
							machine.setValue(UInt16(de), for: .DE)
							machine.setValue(UInt16(hl), for: .HL)
							machine.setValue(UInt16(afDash), for: .afDash)
							machine.setValue(UInt16(bcDash), for: .bcDash)
							machine.setValue(UInt16(deDash), for: .deDash)
							machine.setValue(UInt16(hlDash), for: .hlDash)
							machine.setValue(UInt16(ix), for: .IX)
							machine.setValue(UInt16(iy), for: .IY)
							machine.setValue(UInt16(sp), for: .stackPointer)
							machine.setValue(UInt16(pc), for: .programCounter)
							machine.setValue(UInt16(i), for: .I)
							machine.setValue(UInt16(r), for: .R)
							machine.setValue(UInt16(iff1), for: .IFF1)
							machine.setValue(UInt16(iff2), for: .IFF2)
							machine.setValue(UInt16(interruptMode), for: .IM)
							// TODO: isHalted

							while true {
								var address: UInt32 = 0
								var negative: Int = 0
								if inputScanner.scanHexInt32(&address) {
									while true {
										var value: UInt32 = 0
										if inputScanner.scanHexInt32(&value) {
											machine.setValue(UInt8(value), atAddress: UInt16(address))
											address = address + 1
										} else {
											inputScanner.scanInt(&negative)
											break
										}
									}
								} else {
									inputScanner.scanInt(&negative)
									break
								}
							}

							machine.captureBusActivity = true
							machine.runForNumber(ofCycles: Int32(tStates))
							machine.runToNextInstruction()

							print("\(machine.busOperationCaptures)")
						}
					}
				}
			}
		}
	}
}

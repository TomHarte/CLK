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

	struct RegisterState {
		var af: UInt16 = 0,		bc: UInt16 = 0,		de: UInt16 = 0,		hl: UInt16 = 0
		var afDash: UInt16 = 0, bcDash: UInt16 = 0, deDash: UInt16 = 0, hlDash: UInt16 = 0
		var ix: UInt16 = 0,		iy: UInt16 = 0,		sp: UInt16 = 0,		pc: UInt16 = 0
		var i: UInt8 = 0,		r: UInt8 = 0
		var	iff1: Bool = false,	iff2: Bool = false,	interruptMode: Int = 0
		var isHalted: Bool = false
		var tStates: Int = 0

		func set(onMachine machine: CSTestMachineZ80) {
			machine.setValue(af, for: .AF)
			machine.setValue(bc, for: .BC)
			machine.setValue(de, for: .DE)
			machine.setValue(hl, for: .HL)
			machine.setValue(afDash, for: .afDash)
			machine.setValue(bcDash, for: .bcDash)
			machine.setValue(deDash, for: .deDash)
			machine.setValue(hlDash, for: .hlDash)
			machine.setValue(ix, for: .IX)
			machine.setValue(iy, for: .IY)
			machine.setValue(sp, for: .stackPointer)
			machine.setValue(pc, for: .programCounter)
			machine.setValue(UInt16(i), for: .I)
			machine.setValue(UInt16(r), for: .R)
			machine.setValue(iff1 ? 1 : 0, for: .IFF1)
			machine.setValue(iff2 ? 1 : 0, for: .IFF2)
			machine.setValue(UInt16(interruptMode), for: .IM)
			// TODO: isHalted
		}

		fileprivate func readHexInt16(from scanner: Scanner) -> UInt16 {
			var temporary: UInt32 = 0
			scanner.scanHexInt32(&temporary)
			return UInt16(temporary)
		}

		fileprivate func readHexInt8(from scanner: Scanner) -> UInt8 {
			var temporary: UInt32 = 0
			scanner.scanHexInt32(&temporary)
			return UInt8(temporary)
		}

		init(scanner: Scanner) {
			af = readHexInt16(from: scanner)
			bc = readHexInt16(from: scanner)
			de = readHexInt16(from: scanner)
			hl = readHexInt16(from: scanner)

			afDash = readHexInt16(from: scanner)
			bcDash = readHexInt16(from: scanner)
			deDash = readHexInt16(from: scanner)
			hlDash = readHexInt16(from: scanner)

			ix = readHexInt16(from: scanner)
			iy = readHexInt16(from: scanner)

			sp = readHexInt16(from: scanner)
			pc = readHexInt16(from: scanner)

			i = readHexInt8(from: scanner)
			r = readHexInt8(from: scanner)

			iff1 = readHexInt8(from: scanner) == 1
			iff2 = readHexInt8(from: scanner) == 1

			var temporary: UInt32 = 0
			scanner.scanHexInt32(&temporary)
			interruptMode = Int(temporary)

			isHalted = readHexInt8(from: scanner) == 1

			scanner.scanHexInt32(&temporary)
			tStates = Int(temporary)
		}
	}

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
							let state = RegisterState(scanner: inputScanner)
							state.set(onMachine: machine)

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

							print("\(name)")

							machine.captureBusActivity = true
							machine.runForNumber(ofCycles: Int32(state.tStates))
							machine.runToNextInstruction()

							print("\(machine.busOperationCaptures)")
						}
					}
				}
			}
		}
	}
}

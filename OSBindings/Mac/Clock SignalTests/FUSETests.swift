//
//  FUSETests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 21/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

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

fileprivate struct RegisterState {
	let af: UInt16,		bc: UInt16,		de: UInt16,		hl: UInt16
	let afDash: UInt16, bcDash: UInt16, deDash: UInt16, hlDash: UInt16
	let ix: UInt16,		iy: UInt16,		sp: UInt16,		pc: UInt16
	let i: UInt8,		r: UInt8
	let	iff1: Bool,		iff2: Bool,		interruptMode: Int
	let isHalted: Bool
	let tStates: Int

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

	init(dictionary: [String: Any]) {
		af = UInt16(dictionary["af"] as! NSNumber)
		bc = UInt16(dictionary["bc"] as! NSNumber)
		de = UInt16(dictionary["de"] as! NSNumber)
		hl = UInt16(dictionary["hl"] as! NSNumber)

		afDash = UInt16(dictionary["afDash"] as! NSNumber)
		bcDash = UInt16(dictionary["bcDash"] as! NSNumber)
		deDash = UInt16(dictionary["deDash"] as! NSNumber)
		hlDash = UInt16(dictionary["hlDash"] as! NSNumber)

		ix = UInt16(dictionary["ix"] as! NSNumber)
		iy = UInt16(dictionary["iy"] as! NSNumber)

		sp = UInt16(dictionary["sp"] as! NSNumber)
		pc = UInt16(dictionary["pc"] as! NSNumber)

		i = UInt8(dictionary["i"] as! NSNumber)
		r = UInt8(dictionary["r"] as! NSNumber)

		iff1 = (dictionary["iff1"] as! NSNumber).boolValue
		iff2 = (dictionary["iff2"] as! NSNumber).boolValue

		interruptMode = (dictionary["im"] as! NSNumber).intValue
		isHalted = (dictionary["halted"] as! NSNumber).boolValue

		tStates = (dictionary["tStates"] as! NSNumber).intValue
	}

	init(machine: CSTestMachineZ80) {
		af = machine.value(for: .AF)
		bc = machine.value(for: .BC)
		de = machine.value(for: .DE)
		hl = machine.value(for: .HL)

		afDash = machine.value(for: .afDash)
		bcDash = machine.value(for: .bcDash)
		deDash = machine.value(for: .deDash)
		hlDash = machine.value(for: .hlDash)

		ix = machine.value(for: .IX)
		iy = machine.value(for: .IY)

		sp = machine.value(for: .stackPointer)
		pc = machine.value(for: .programCounter)

		i = UInt8(machine.value(for: .I))
		r = UInt8(machine.value(for: .R))

		iff1 = machine.value(for: .IFF1) != 0
		iff2 = machine.value(for: .IFF2) != 0

		interruptMode = Int(machine.value(for: .IM))

		isHalted = false	// TODO
		tStates = 0			// TODO
	}
}

extension RegisterState: Equatable {}

fileprivate func ==(lhs: RegisterState, rhs: RegisterState) -> Bool {
	return	lhs.af == rhs.af &&
			lhs.bc == rhs.bc &&
			lhs.de == rhs.de &&
			lhs.hl == rhs.hl &&
			lhs.afDash == rhs.afDash &&
			lhs.bcDash == rhs.bcDash &&
			lhs.deDash == rhs.deDash &&
			lhs.hlDash == rhs.hlDash &&
			lhs.ix == rhs.ix &&
			lhs.iy == rhs.iy &&
			lhs.sp == rhs.sp &&
			lhs.pc == rhs.pc &&
			lhs.i == rhs.i &&
			lhs.r == rhs.r &&
			lhs.iff1 == rhs.iff1 &&
			lhs.iff2 == rhs.iff2 &&
			lhs.interruptMode == rhs.interruptMode
}

class FUSETests: XCTestCase {

	func testFUSE() {
		let inputFilename: String! = Bundle(for: type(of: self)).path(forResource: "tests.in", ofType: "json")
		let outputFilename: String! = Bundle(for: type(of: self)).path(forResource: "tests.expected", ofType: "json")

		XCTAssert(inputFilename != nil && outputFilename != nil)

		let inputData: Data! = try? Data(contentsOf: URL(fileURLWithPath: inputFilename))
		let outputData: Data! = try? Data(contentsOf: URL(fileURLWithPath: outputFilename))

		XCTAssert(inputData != nil && outputData != nil)

		let inputArray: [Any]! = try! JSONSerialization.jsonObject(with: inputData, options: []) as? [Any]
		let outputArray: [Any]! = try! JSONSerialization.jsonObject(with: outputData, options: []) as? [Any]

		XCTAssert(inputArray != nil && outputArray != nil)

		var index = 0
//		var failures = 0
		for item in inputArray {
			let itemDictionary = item as! [String: Any]
			let outputDictionary = outputArray[index] as! [String: Any]
			index = index + 1

			let name = itemDictionary["name"] as! String

//			if name != "ddcb00" {
//				continue;
//			}

			let initialState = RegisterState(dictionary: itemDictionary["state"] as! [String: Any])
			let targetState = RegisterState(dictionary: outputDictionary["state"] as! [String: Any])

			let machine = CSTestMachineZ80()
			initialState.set(onMachine: machine)

			let memoryGroups = itemDictionary["memory"] as? [Any]
			if let memoryGroups = memoryGroups {
				for group in memoryGroups {
					let groupDictionary = group as! [String: Any]
					var address = UInt16(groupDictionary["address"] as! NSNumber)
					let data = groupDictionary["data"] as! [NSNumber]
					for value in data {
						machine.setValue(UInt8(value), atAddress: address)
						address = address + 1
					}
				}
			}

			machine.runForNumber(ofCycles: Int32(targetState.tStates))

			let finalState = RegisterState(machine: machine)

			XCTAssert(finalState == targetState, "Failed \(name)")
//			if finalState != targetState {
//				failures = failures + 1
//				if failures == 5 {
//					return
//				}
//			}

			// TODO compare bus operations and final memory state

		}
	}
}

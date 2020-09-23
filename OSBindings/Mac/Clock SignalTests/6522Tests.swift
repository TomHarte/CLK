//
//  6522Tests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class MOS6522Tests: XCTestCase {

	private var m6522: MOS6522Bridge!

	override func setUp() {
		m6522 = MOS6522Bridge()
	}

	// MARK: Timer tests

	func testTimerCount() {
		// set timer 1 to a value of m652200a
		m6522.setValue(10, forRegister: 4)
		m6522.setValue(0, forRegister: 5)

		// complete the setting cycle
		m6522.run(forHalfCycles: 2)

		// run for 5 cycles
		m6522.run(forHalfCycles: 10)

		// check that the timer has gone down by 5
		XCTAssert(m6522.value(forRegister: 4) == 5, "Low order byte should be 5; was \(m6522.value(forRegister: 4))")
		XCTAssert(m6522.value(forRegister: 5) == 0, "High order byte should be 0; was \(m6522.value(forRegister: 5))")
	}

	func testTimerLatches() {
		// set timer 2 to $1020
		m6522.setValue(0x10, forRegister: 8)
		m6522.setValue(0x20, forRegister: 9)

		// change the low-byte latch
		m6522.setValue(0x40, forRegister: 8)

		// complete the cycle
		m6522.run(forHalfCycles: 2)

		// chek that the new latched value hasn't been copied
		XCTAssert(m6522.value(forRegister: 8) == 0x10, "Low order byte should be 0x10; was \(m6522.value(forRegister: 8))")
		XCTAssert(m6522.value(forRegister: 9) == 0x20, "High order byte should be 0x20; was \(m6522.value(forRegister: 9))")

		// write the low-byte latch
		m6522.setValue(0x50, forRegister: 9)

		// complete the cycle
		m6522.run(forHalfCycles: 2)

		// chek that the latched value has been copied
		XCTAssert(m6522.value(forRegister: 8) == 0x40, "Low order byte should be 0x50; was \(m6522.value(forRegister: 8))")
		XCTAssert(m6522.value(forRegister: 9) == 0x50, "High order byte should be 0x40; was \(m6522.value(forRegister: 9))")
	}

	func testTimerReload() {
		// set timer 1 to a value of m6522010, enable repeating mode
		m6522.setValue(16, forRegister: 4)
		m6522.setValue(0, forRegister: 5)
		m6522.setValue(0x40, forRegister: 11)
		m6522.setValue(0x40 | 0x80, forRegister: 14)

		// complete the cycle to set initial values
		m6522.run(forHalfCycles: 2)

		// run for 16 cycles
		m6522.run(forHalfCycles: 32)

		// check that the timer has gone down to 0 but not yet triggered an interrupt
		XCTAssert(m6522.value(forRegister: 4) == 0, "Low order byte should be 0; was \(m6522.value(forRegister: 4))")
		XCTAssert(m6522.value(forRegister: 5) == 0, "High order byte should be 0; was \(m6522.value(forRegister: 5))")
		XCTAssert(!m6522.irqLine, "IRQ should not yet be active")

		// check that two half-cycles later the timer is $ffff but IRQ still hasn't triggered
		m6522.run(forHalfCycles: 2)
		XCTAssert(m6522.value(forRegister: 4) == 0xff, "Low order byte should be 0xff; was \(m6522.value(forRegister: 4))")
		XCTAssert(m6522.value(forRegister: 5) == 0xff, "High order byte should be 0xff; was \(m6522.value(forRegister: 5))")
		XCTAssert(!m6522.irqLine, "IRQ should not yet be active")

		// check that one half-cycle later the timer is still $ffff and IRQ has triggered...
		m6522.run(forHalfCycles: 1)
		XCTAssert(m6522.irqLine, "IRQ should be active")
		XCTAssert(m6522.value(forRegister: 4) == 0xff, "Low order byte should be 0xff; was \(m6522.value(forRegister: 4))")
		XCTAssert(m6522.value(forRegister: 5) == 0xff, "High order byte should be 0xff; was \(m6522.value(forRegister: 5))")

		// ... but that reading the timer cleared the interrupt
		XCTAssert(!m6522.irqLine, "IRQ should be active")

		// check that one half-cycles later the timer has reloaded
		m6522.run(forHalfCycles: 1)
		XCTAssert(m6522.value(forRegister: 4) == 0x10, "Low order byte should be 0x10; was \(m6522.value(forRegister: 4))")
		XCTAssert(m6522.value(forRegister: 5) == 0x00, "High order byte should be 0x00; was \(m6522.value(forRegister: 5))")
	}


	// MARK: PB7 timer 1 tests
	// These follow the same logic and check for the same results as the VICE VIC-20 via_pb7 tests.

	// Perfoms:
	//
	//	(1) establish initial ACR and port B output value, and grab port B input value.
	//	(2) start timer 1, grab port B input value.
	//	(3) set final ACR, grab port B input value.
	//	(4) allow timer 1 to expire, grab port B input value.
	private func runTest(startACR: UInt8, endACR: UInt8, portBOutput: UInt8) -> [UInt8] {
		var result: [UInt8] = []

		// Clear all register values.
		for n: UInt in 0...15 {
			m6522.setValue(0, forRegister: n)
		}
		m6522.run(forHalfCycles: 2)

		// Set data direction and current port B value.
		m6522.setValue(0xff, forRegister: 2)
		m6522.run(forHalfCycles: 2)
		m6522.setValue(portBOutput, forRegister: 0)
		m6522.run(forHalfCycles: 2)

		// Set initial ACR and grab the current port B value.
		m6522.setValue(startACR, forRegister: 0xb)
		m6522.run(forHalfCycles: 2)
		result.append(m6522.value(forRegister: 0))
		m6522.run(forHalfCycles: 2)

		// Start timer 1 and grab the value.
		m6522.setValue(1, forRegister: 0x5)
		m6522.run(forHalfCycles: 2)
		result.append(m6522.value(forRegister: 0))
		m6522.run(forHalfCycles: 2)

		// Set the final ACR value and grab value.
		m6522.setValue(endACR, forRegister: 0xb)
		m6522.run(forHalfCycles: 2)
		result.append(m6522.value(forRegister: 0))
		m6522.run(forHalfCycles: 2)

		// Make sure timer 1 has expired.
		m6522.run(forHalfCycles: 512)

		// Grab the final value.
		result.append(m6522.value(forRegister: 0))

		return result
	}

	func testTimer1PB7() {
		// Original top row. [original Vic-20 screen output in comments on the right]
		XCTAssertEqual(runTest(startACR: 0x00, endACR: 0x00, portBOutput: 0x00), [0x00, 0x00, 0x00, 0x00])	// @@@@	(i.e. 0, 0, 0, 0)
		XCTAssertEqual(runTest(startACR: 0x00, endACR: 0x40, portBOutput: 0x00), [0x00, 0x00, 0x00, 0x00])	// @@@@	(i.e. 0, 0, 0, 0)
		XCTAssertEqual(runTest(startACR: 0x00, endACR: 0x80, portBOutput: 0x00), [0x00, 0x00, 0x80, 0x00])	// @@b@	(i.e. 0, 0, 1, 0)
		XCTAssertEqual(runTest(startACR: 0x00, endACR: 0xc0, portBOutput: 0x00), [0x00, 0x00, 0x80, 0x00])	// @@b@	(i.e. 0, 0, 1, 0)

		XCTAssertEqual(runTest(startACR: 0x00, endACR: 0x00, portBOutput: 0xff), [0xff, 0xff, 0xff, 0xff])	// cccc	(i.e. 1, 1, 1, 1)
		XCTAssertEqual(runTest(startACR: 0x00, endACR: 0x40, portBOutput: 0xff), [0xff, 0xff, 0xff, 0xff])	// cccc	(i.e. 1, 1, 1, 1)
		XCTAssertEqual(runTest(startACR: 0x00, endACR: 0x80, portBOutput: 0xff), [0xff, 0xff, 0xff, 0x7f])	// ccca	(i.e. 1, 1, 1, 0)
		XCTAssertEqual(runTest(startACR: 0x00, endACR: 0xc0, portBOutput: 0xff), [0xff, 0xff, 0xff, 0x7f])	// ccca	(i.e. 1, 1, 1, 0)

		// Second row.	[same output as first row]
		XCTAssertEqual(runTest(startACR: 0x40, endACR: 0x00, portBOutput: 0x00), [0x00, 0x00, 0x00, 0x00])	// @@@@
		XCTAssertEqual(runTest(startACR: 0x40, endACR: 0x40, portBOutput: 0x00), [0x00, 0x00, 0x00, 0x00])	// @@@@
		XCTAssertEqual(runTest(startACR: 0x40, endACR: 0x80, portBOutput: 0x00), [0x00, 0x00, 0x80, 0x00])	// @@b@
		XCTAssertEqual(runTest(startACR: 0x40, endACR: 0xc0, portBOutput: 0x00), [0x00, 0x00, 0x80, 0x00])	// @@b@

		XCTAssertEqual(runTest(startACR: 0x40, endACR: 0x00, portBOutput: 0xff), [0xff, 0xff, 0xff, 0xff])	// cccc
		XCTAssertEqual(runTest(startACR: 0x40, endACR: 0x40, portBOutput: 0xff), [0xff, 0xff, 0xff, 0xff])	// cccc
		XCTAssertEqual(runTest(startACR: 0x40, endACR: 0x80, portBOutput: 0xff), [0xff, 0xff, 0xff, 0x7f])	// ccca
		XCTAssertEqual(runTest(startACR: 0x40, endACR: 0xc0, portBOutput: 0xff), [0xff, 0xff, 0xff, 0x7f])	// ccca

		// Third row.
		XCTAssertEqual(runTest(startACR: 0x80, endACR: 0x00, portBOutput: 0x00), [0x80, 0x00, 0x00, 0x00])	// b@@@	(i.e. 1, 0, 0, 0)
		XCTAssertEqual(runTest(startACR: 0x80, endACR: 0x40, portBOutput: 0x00), [0x80, 0x00, 0x00, 0x00])	// b@@@	(i.e. 1, 0, 0, 0)
		XCTAssertEqual(runTest(startACR: 0x80, endACR: 0x80, portBOutput: 0x00), [0x80, 0x00, 0x00, 0x80])	// b@@b	(i.e. 1, 0, 0, 1)
		XCTAssertEqual(runTest(startACR: 0x80, endACR: 0xc0, portBOutput: 0x00), [0x80, 0x00, 0x00, 0x80])	// b@@b	(i.e. 1, 0, 0, 1)

		XCTAssertEqual(runTest(startACR: 0x80, endACR: 0x00, portBOutput: 0xff), [0xff, 0x7f, 0xff, 0xff])	// cacc	(i.e. 1, 0, 1, 1)
		XCTAssertEqual(runTest(startACR: 0x80, endACR: 0x40, portBOutput: 0xff), [0xff, 0x7f, 0xff, 0xff])	// cacc	(i.e. 1, 0, 1, 1)
		XCTAssertEqual(runTest(startACR: 0x80, endACR: 0x80, portBOutput: 0xff), [0xff, 0x7f, 0x7f, 0xff])	// caac	(i.e. 1, 0, 0, 1)
		XCTAssertEqual(runTest(startACR: 0x80, endACR: 0xc0, portBOutput: 0xff), [0xff, 0x7f, 0x7f, 0xff])	// caac	(i.e. 1, 0, 0, 1)

		// Final row.	[same output as third row]
		XCTAssertEqual(runTest(startACR: 0xc0, endACR: 0x00, portBOutput: 0x00), [0x80, 0x00, 0x00, 0x00])	// b@@@
		XCTAssertEqual(runTest(startACR: 0xc0, endACR: 0x40, portBOutput: 0x00), [0x80, 0x00, 0x00, 0x00])	// b@@@
		XCTAssertEqual(runTest(startACR: 0xc0, endACR: 0x80, portBOutput: 0x00), [0x80, 0x00, 0x00, 0x80])	// b@@b
		XCTAssertEqual(runTest(startACR: 0xc0, endACR: 0xc0, portBOutput: 0x00), [0x80, 0x00, 0x00, 0x80])	// b@@b

		XCTAssertEqual(runTest(startACR: 0xc0, endACR: 0x00, portBOutput: 0xff), [0xff, 0x7f, 0xff, 0xff])	// cacc
		XCTAssertEqual(runTest(startACR: 0xc0, endACR: 0x40, portBOutput: 0xff), [0xff, 0x7f, 0xff, 0xff])	// cacc
		XCTAssertEqual(runTest(startACR: 0xc0, endACR: 0x80, portBOutput: 0xff), [0xff, 0x7f, 0x7f, 0xff])	// caac
		XCTAssertEqual(runTest(startACR: 0xc0, endACR: 0xc0, portBOutput: 0xff), [0xff, 0x7f, 0x7f, 0xff])	// caac

		// Conclusions:
		//
		//	after inital ACR and port B value:	[original data if not in PB7 output mode, otherwise 1]
		//	after starting timer 1: 			[original data if not in PB7 output mode, otherwise 0]
		//	after final ACR value:				[original data if not in PB7 output mode, 1 if has transitioned to PB7 mode, 0 if was already in PB7 mode]
		//	after timer 1 expiry:				[original data if not in PB7 mode, 1 if timer has expired while in PB7 mode]
		//
		//	i.e.
		//		(1)	there is separate storage for the programmer-set PB7 and the timer output;
		//		(2)	the timer output is reset upon a timer write only if PB7 output is enabled;
		//		(3)	expiry toggles the output.
	}


	// MARK: Data direction tests
	func testDataDirection() {
		// set low four bits of register B as output, the top four as input
		m6522.setValue(0xf0, forRegister: 2)

		// ask to output 0x8c
		m6522.setValue(0x8c, forRegister: 0)

		// complete the cycle
		m6522.run(forHalfCycles: 2)

		// set current input as 0xda
		m6522.portBInput = 0xda

		// test that the result of reading register B is therefore 0x8a
		XCTAssert(m6522.value(forRegister: 0) == 0x8a, "Data direction register should mix input and output; got \(m6522.value(forRegister: 0))")
	}

	func testShiftDisabled() {
		/*
			Mode 0 disables the Shift Register. In this mode the microprocessor can
			write or read the SR and the SR will shift on each CB1 positive edge
			shifting in the value on CB2. In this mode the SR Interrupt Flag is
			disabled (held to a logic 0).
		*/
	}

	func testShiftInUnderT2() {
		/*
			In mode 1, the shifting rate is controlled by the low order 8 bits of T2
			(Figure 22). Shift pulses are generated on the CB1 pin to control shifting
			in external devices. The time between transitions of this output clock is a
			function of the system clock period and the contents of the low order T2
			latch (N).

			The shifting operation is triggered by the read or write of the SR if the
			SR flag is set in the IFR. Otherwise the first shift will occur at the next
			time-out of T2 after a read or write of the SR. Data is shifted first into
			the low order bit of SR and is then shifted into the next higher order bit
			of the shift register on the negative-going edge of each clock pulse. The
			input data should change before the positive-going edge of the CB1 clock
			pulse. This data is shifted into shift register during the 02 clock cycle
			following the positive-going edge of the CB1 clock pulse. After 8 CB1 clock
			pulses, the shift register interrupt flag will set and IRQ will go low.
		*/
	}

	func testShiftInUnderPhase2() {
		/*
			In mode 2, the shift rate is a direct function of the system clock
			frequency (Figure 23). CB1 becomes an output which generates shift pulses
			for controlling external devices. Timer 2 operates as an independent
			interval timer and has no effect on SR. The shifting operation is triggered
			by reading or writing the Shift Register. Data is shifted, first into bit 0
			and is then shifted into the next higher order bit of the shift register on
			the trailing edge of each 02 clock pulse. After 8 clock pulses, the shift
			register interrupt flag will be set, and the output clock pulses on CB1
			will stop.
		*/
	}

	func testShiftInUnderCB1() {
		/*
			In mode 3, external pin CB1 becomes an input (Figure 24). This allows an
			external device to load the shift register at its own pace. The shift
			register counter will interrupt the processor each time 8 bits have been
			shifted in. However the shift register counter does not stop the shifting
			operation; it acts simply as a pulse counter. Reading or writing the Shift
			Register resets the Interrupt Flag and initializes the SR counter to count
			another 8 pulses.

			Note that the data is shifted during the first system clock cycle
			following the positive-going edge of the CB1 shift pulse. For this reason,
			data must be held stable during the first full cycle following CB1 going
			high.
		*/
	}

	func testShiftOutUnderT2FreeRunning() {
		/*
			Mode 4 is very similar to mode 5 in which the shifting rate is set by T2.
			However, in mode 4 the SR Counter does not stop the shifting operation
			(Figure 25). Since the Shift Register bit 7 (SR7) is recirculated back into
			bit 0, the 8 bits loaded into the Shift Register will be clocked onto CB2
			repetitively. In this mode the Shift Register Counter is disabled.
		*/
	}

	func testShiftOutUnderT2() {
		/*
			In mode 5, the shift rate is controlled by T2 (as in mode 4). The shifting
			operation is triggered by the read or write of the SR if the SR flag is set
			in the IFR (Figure 26). Otherwise the first shift will occur at the next
			time-out of T2 after a read or write of the SR. However, with each read or
			write of the shift register the SR Counter is reset and 8 bits are shifted
			onto CB2. At the same time, 8 shift pulses are generated on CB1 to control
			shifting in external devices. After the 8 shift pulses, the shifting is
			disabled, the SR Interrupt Flag is set and CB2 remains at the last data
			level.
		*/
	}

	func testShiftOutUnderPhase2() {
		/*
			In mode 6, the shift rate is controlled by the 02 system clock (Figure 27).

			(... and I'm assuming the same behaviour as shift out under control of T2
			otherwise, based on original context)
		*/
		// Set the shift register to a non-zero something.
		m6522.setValue(0xaa, forRegister: 10)

		// Set shift register mode 6.
		m6522.setValue(6 << 2, forRegister: 11)

		// Make sure the shift register's interrupt bit is set.
		m6522.run(forHalfCycles: 16)
		XCTAssertEqual(m6522.value(forRegister: 13) & 0x04, 0x04)

		// Test that output is now inhibited: CB2 should remain unchanged.
		let initialOutput = m6522.value(forControlLine: .two, port: .B)
		for _ in 1...8 {
			m6522.run(forHalfCycles: 2)
			XCTAssertEqual(m6522.value(forControlLine: .two, port: .B), initialOutput)
		}

		// Set a new value to the shift register.
		m6522.setValue(0x16, forRegister: 10)

		// Test that the new value is shifted out.
		var output = 0
		for _ in 1..<8 {
			m6522.run(forHalfCycles: 2)
			output = (output << 1) | (m6522.value(forControlLine: .two, port: .B) ? 1 : 0)
		}
		XCTAssertEqual(output, 0x16)
	}

	func testShiftOutUnderCB1() {
		/*
			In mode 7, shifting is controlled by pulses applied to the CB1 pin by an
			external device (Figure 28). The SR counter sets the SR Interrupt Flag each
			time it counts 8 pulses but it does not disable the shifting function. Each
			time the microprocessor, writes or reads the shift register, the SR
			Interrupt Flag is reset and the SR counter is initialized to begin counting
			the next 8 shift pulses on pin CB1. After 8 shift pulses, the Interrupt
			Flag is set. The microprocessor can then load the shift register with the
			next byte of data.
		*/
	}
}

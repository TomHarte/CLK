//
//  68000ArithmeticTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 28/06/2019.
//
//  Largely ported from the tests of the Portable 68k Emulator.
//

#import <XCTest/XCTest.h>

#include "68000.hpp"
#include "68000Mk2.hpp"

#include <array>

namespace {

struct Transaction {
	uint8_t function_code = 0;
	uint32_t address = 0;
	uint16_t value = 0;
	bool address_strobe = false;
	bool data_strobe = false;

	bool operator !=(const Transaction &rhs) {
		return false;
	}
};

struct BusHandler {
	template <typename Microcycle> HalfCycles perform_bus_operation(const Microcycle &cycle, bool is_supervisor) {
		Transaction transaction;

		// Fill all of the transaction record except the data field; will do that after
		// any potential read.
		if(cycle.operation & Microcycle::InterruptAcknowledge) {
			transaction.function_code = 0b111;
		} else {
			transaction.function_code = is_supervisor ? 0x4 : 0x0;
			transaction.function_code |= (cycle.operation & Microcycle::IsData) ? 0x1 : 0x2;
		}
		transaction.address_strobe = cycle.operation & (Microcycle::NewAddress | Microcycle::SameAddress);
		transaction.data_strobe = cycle.operation & (Microcycle::SelectByte | Microcycle::SelectWord);
		if(cycle.address) transaction.address = *cycle.address & 0xffff'ff;

		// TODO: generate a random value if this is a read from an address not yet written to;
		// use a shared store in order to ensure that both devices get the same random values.

		// Do the operation...
		const uint32_t address = cycle.address ? (*cycle.address & 0xffff'ff) : 0;
		switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
			default: break;

			case Microcycle::SelectWord | Microcycle::Read:
				cycle.set_value16((ram[address] << 8) | ram[address + 1]);
			break;
			case Microcycle::SelectByte | Microcycle::Read:
				if(address & 1) {
					cycle.set_value8_low(ram[address]);
				} else {
					cycle.set_value8_high(ram[address]);
				}
			break;
			case Microcycle::SelectWord:
				ram[address] = cycle.value8_high();
				ram[address+1] = cycle.value8_low();
			break;
			case Microcycle::SelectByte:
				ram[address] = (address & 1) ? cycle.value8_low() : cycle.value8_high();
			break;
		}


		// Add the data value if relevant.
		if(transaction.data_strobe) {
			transaction.value = cycle.value16();
		}

		// Push back only if interesting.
		if(transaction.address_strobe || transaction.data_strobe || transaction.function_code == 7) {
			transactions.push_back(transaction);
		}

		return HalfCycles(0);
	}

	void flush() {}

	std::vector<Transaction> transactions;
	std::array<uint8_t, 16*1024*1024> ram;

	void set_default_vectors() {
		// Establish that all exception vectors point to 1024-byte blocks of memory.
		for(int c = 0; c < 256; c++) {
			const uint32_t target = (c + 1) << 10;
			ram[(c << 2) + 0] = uint8_t(target >> 24);
			ram[(c << 2) + 1] = uint8_t(target >> 16);
			ram[(c << 2) + 2] = uint8_t(target >> 8);
			ram[(c << 2) + 3] = uint8_t(target >> 0);
		}
	}
};

using OldProcessor = CPU::MC68000::Processor<BusHandler, true>;
using NewProcessor = CPU::MC68000Mk2::Processor<BusHandler, true, true>;

template <typename M68000> struct Tester {
	Tester() : processor(bus_handler) {
	}

	void advance(int cycles) {
		processor.run_for(HalfCycles(cycles << 1));
	}

	void reset_with_opcode(uint16_t opcode) {
		bus_handler.transactions.clear();
		bus_handler.set_default_vectors();

		bus_handler.ram[(2 << 10) + 0] = uint8_t(opcode >> 8);
		bus_handler.ram[(2 << 10) + 1] = uint8_t(opcode >> 0);

		processor.reset();
	}

	BusHandler bus_handler;
	M68000 processor;
};

}

@interface M68000OldVsNewTests : XCTestCase
@end

@implementation M68000OldVsNewTests {
}

- (void)testOldVsNew {
	auto oldTester = std::make_unique<Tester<OldProcessor>>();
	auto newTester = std::make_unique<Tester<NewProcessor>>();
	InstructionSet::M68k::Predecoder<InstructionSet::M68k::Model::M68000> decoder;

	for(int c = 0; c < 65536; c++) {
		// Test only defined opcodes.
		const auto instruction = decoder.decode(uint16_t(c));
		if(instruction.operation == InstructionSet::M68k::Operation::Undefined) {
			continue;
		}

		// Test each 1000 times.
		for(int test = 0; test < 1000; test++) {
			oldTester->reset_with_opcode(c);
			newTester->reset_with_opcode(c);

			// For arbitrary resons, only run for 200 cycles.
			newTester->advance(200);
			oldTester->advance(200);

			// Compare bus activity.
			const auto &oldTransactions = oldTester->bus_handler.transactions;
			const auto &newTransactions = newTester->bus_handler.transactions;
			if(
				oldTransactions.size() != newTransactions.size()
			) {
				printf("Size mismatch: %zu vs %zu\n", newTransactions.size(), oldTransactions.size());
				continue;
			}

			auto newIt = newTransactions.begin();
			auto oldIt = oldTransactions.begin();
			while(newIt != newTransactions.end()) {
//				if(*newIt != *oldIt) {
//					printf("Peekaboo!");
//				}

				++newIt;
				++oldIt;
			}
		}
	}
}

@end

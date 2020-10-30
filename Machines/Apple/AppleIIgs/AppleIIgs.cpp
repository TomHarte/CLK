//
//  AppleIIgs.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

#include "AppleIIgs.hpp"

#include "../../MachineTypes.hpp"
#include "../../../Processors/65816/65816.hpp"

#include "../../../Analyser/Static/AppleIIgs/Target.hpp"
#include "MemoryMap.hpp"

#include "../../../Components/8530/z8530.hpp"
#include "../../../Components/AppleClock/AppleClock.hpp"
#include "../../../Components/DiskII/IWM.hpp"

#include <cassert>
#include <array>

namespace Apple {
namespace IIgs {

class ConcreteMachine:
	public Apple::IIgs::Machine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public CPU::MOS6502Esque::BusHandler<uint32_t> {

	public:
		ConcreteMachine(const Analyser::Static::AppleIIgs::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			m65816_(*this) {

			set_clock_rate(14318180.0);

			using Target = Analyser::Static::AppleIIgs::Target;
			std::vector<ROMMachine::ROM> rom_descriptions;
			const std::string machine_name = "AppleIIgs";
			switch(target.model) {
				case Target::Model::ROM00:
					/* TODO */
				case Target::Model::ROM01:
					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM01", "apple2gs.rom", 128*1024, 0x42f124b0);
				break;

				case Target::Model::ROM03:
					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM03", "apple2gs.rom2", 256*1024, 0xde7ddf29);
				break;
			}
			const auto roms = rom_fetcher(rom_descriptions);
			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}
			rom_ = *roms[0];

			size_t ram_size = 0;
			switch(target.memory_model) {
				case Target::MemoryModel::TwoHundredAndFiftySixKB:
					ram_size = 256;
				break;

				case Target::MemoryModel::OneMB:
					ram_size = 128 + 1024;
				break;

				case Target::MemoryModel::EightMB:
					ram_size = 128 + 8 * 1024;
				break;
			}
			ram_.resize(ram_size * 1024);

			memory_.set_storage(ram_, rom_);

			// Sync up initial values.
			memory_.set_speed_register(speed_register_);
		}

		void run_for(const Cycles cycles) override {
			m65816_.run_for(cycles);
		}

		void set_scan_target(Outputs::Display::ScanTarget *) override {
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return Outputs::Display::ScanStatus();
		}

		forceinline Cycles perform_bus_operation(const CPU::WDC65816::BusOperation operation, const uint32_t address, uint8_t *const value) {
			const auto &region = MemoryMapRegion(memory_, address);

			// TODO: potentially push time to clock_.

			if(region.flags & MemoryMap::Region::IsIO) {
				// Ensure classic auxiliary and language card accesses have effect.
				const bool is_read = isReadOperation(operation);
				memory_.access(uint16_t(address), is_read);

				switch(address & 0xffff) {

					// New video register.
					case 0xc029:
						if(is_read) {
							*value = 0;
						} else {
							printf("New video: %02x\n", *value);
							// TODO: this bit should affect memory bank selection, somehow?
							// Cf. Page 90.
						}
					break;

					// Shadow register.
					case 0xc035:
						if(is_read) {
							*value = memory_.get_shadow_register();
						} else {
							memory_.set_shadow_register(*value);
						}
					break;

					// Clock data.
					case 0xc033:
						if(is_read) {
							*value = clock_.get_data();
						} else {
							clock_.set_data(*value);
						}
					break;

					// Clock and border control.
					case 0xc034:
						if(is_read) {
							*value = clock_.get_control();
						} else {
							clock_.set_control(*value);
							// TODO: also set border colour.
						}
					break;

					// Speed register.
					case 0xc036:
						if(is_read) {
							*value = speed_register_;
						} else {
							memory_.set_speed_register(*value);
							speed_register_ = *value;
							printf("[Unimplemented] most of speed register: %02x\n", *value);
						}
					break;

					// [Memory] State register.
					case 0xc068:
						if(is_read) {
							*value = memory_.get_state_register();
						} else {
							memory_.set_state_register(*value);
						}
					break;

					// Various independent memory switch reads [TODO: does the IIe-style keyboard the low seven?].
#define SwitchRead(s) *value = memory_.s ? 0x80 : 0x00
#define LanguageRead(s) SwitchRead(language_card_switches().state().s)
#define AuxiliaryRead(s) SwitchRead(auxiliary_switches().switches().s)
					case 0xc011:	LanguageRead(bank1);						break;
					case 0xc012:	LanguageRead(read);							break;
					case 0xc013:	AuxiliaryRead(read_auxiliary_memory);		break;
					case 0xc014:	AuxiliaryRead(write_auxiliary_memory);		break;
					case 0xc015:	AuxiliaryRead(internal_CX_rom);				break;
					case 0xc016:	AuxiliaryRead(alternative_zero_page);		break;
					case 0xc017:	AuxiliaryRead(slot_C3_rom);					break;
#undef AuxiliaryRead
#undef LanguageRead
#undef SwitchRead

					// The SCC.
					case 0xc038: case 0xc039: case 0xc03a: case 0xc03b:
						if(isReadOperation(operation)) {
							*value = scc_.read(int(address));
						} else {
							scc_.write(int(address), *value);
						}
					break;

					// These were all dealt with by the call to memory_.access.
					// TODO: subject to read data? Does vapour lock apply?
					case 0xc000: case 0xc001: case 0xc002: case 0xc003: case 0xc004: case 0xc005:
					case 0xc006: case 0xc007: case 0xc008: case 0xc009: case 0xc00a: case 0xc00b:
					case 0xc054: case 0xc055: case 0xc056: case 0xc057:
					break;

					default:
						if((address & 0xffff) < 0xc100) {
							// TODO: all other IO accesses.
							printf("Unhandled IO: %04x\n", address & 0xffff);
							assert(false);
						} else {
							// Card IO. Not implemented!
							if(isReadOperation(operation)) {
								*value = 0xff;
							}
						}
				}
			} else {
				if(isReadOperation(operation)) {
					MemoryMapRead(region, address, value);
				} else {
					MemoryMapWrite(memory_, region, address, value);
				}
			}

			printf("%06x [%02x] %c\n", address, *value, operation == CPU::WDC65816::BusOperation::ReadOpcode ? '*' : ' ');

			Cycles duration = Cycles(5);

			// TODO: determine the cost of this access.
//			if((mapping.flags & BankMapping::Is1Mhz) || ((mapping.flags & BankMapping::IsShadowed) && !isReadOperation(operation))) {
//				// TODO: (i) get into phase; (ii) allow for the 1Mhz bus length being sporadically 16 rather than 14.
//				duration = Cycles(14);
//			} else {
//				// TODO: (i) get into phase; (ii) allow for collisions with the refresh cycle.
//				duration = Cycles(5);
//			}
			fast_access_phase_ = (fast_access_phase_ + duration.as<int>()) % 5;		// TODO: modulo something else, to allow for refresh.
			slow_access_phase_ = (slow_access_phase_ + duration.as<int>()) % 14;	// TODO: modulo something else, to allow for stretched cycles.
			return duration;
		}

	private:
		CPU::WDC65816::Processor<ConcreteMachine, false> m65816_;
		MemoryMap memory_;
		Apple::Clock::ParallelClock clock_;

		int fast_access_phase_ = 0;
		int slow_access_phase_ = 0;

		uint8_t speed_register_ = 0x00;

		// MARK: - Memory storage.

		std::vector<uint8_t> ram_;
		std::vector<uint8_t> rom_;

		// MARK: - Other components.
 		Zilog::SCC::z8530 scc_;
};

}
}

using namespace Apple::IIgs;

Machine *Machine::AppleIIgs(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(*dynamic_cast<const Analyser::Static::AppleIIgs::Target *>(target), rom_fetcher);
}

Machine::~Machine() {}

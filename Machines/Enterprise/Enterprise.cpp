//
//  Enterprise.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Enterprise.hpp"

#include "Dave.hpp"
#include "EXDos.hpp"
#include "HostFSHandler.hpp"
#include "Keyboard.hpp"
#include "Nick.hpp"

#include "Machines/MachineTypes.hpp"
#include "Machines/Utility/Typer.hpp"

#include "Activity/Source.hpp"
#include "Analyser/Static/Enterprise/Target.hpp"
#include "ClockReceiver/JustInTime.hpp"
#include "Outputs/Log.hpp"
#include "Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "Processors/Z80/Z80.hpp"

#include <unordered_set>

namespace {
using Logger = Log::Logger<Log::Source::Enterprise>;
}

namespace Enterprise {

/*
	Notes to self on timing:

	Nick divides each line into 57 windows; each window lasts 16 cycles and dedicates the
	first 10 of those to VRAM accesses, leaving the final six for a Z80 video RAM access
	if one has been requested.

	The Z80 has a separate, asynchronous 4Mhz clock. That's that.

	The documentation is also very forward in emphasising that Nick generates phaselocked
	(i.e. in-phase) PAL video.

	So: 57*16 = 912 cycles/line.

	A standard PAL line lasts 64µs and during that time outputs 283.7516 colour cycles.

	I shall _guess_ that the Enterprise stretches each line to 284 colour cycles rather than
	reducing it to 283.

	Therefore 912 cycles occurs in 284/283.7516 * 64 µs.

	So one line = 181760000 / 2837516 µs = 45440000 / 709379 µs
	=> one cycle = 45440000 / 709379*912 = 45440000 / 646953648 = 2840000 / 40434603 µs
	=> clock rate of 40434603 / 2840000 Mhz

	And, therefore, the ratio to a 4Mhz Z80 clock is:

		40434603 / (2840000 * 4)
		= 40434603 / 11360000
		i.e. roughly 3.55 Nick cycles per Z80 cycle.

	If that's true then the 6-cycle window is around 1.69 Z80 cycles long. Given that the Z80
	clock in an Enterprise can be stopped in half-cycle increments only, the Z80 can only be
	guaranteed to have around a 1.19 cycle minimum for its actual access. I'm therefore further
	postulating that the clock stoppage takes place so as to align the final cycle of a relevant
	access over the available window.

*/

template <bool has_disk_controller, bool is_6mhz> class ConcreteMachine:
	public Activity::Source,
	public Configurable::Device,
	public CPU::Z80::BusHandler,
	public HostFSHandler::MemoryAccessor,
	public Machine,
	public MachineTypes::AudioProducer,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaTarget,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public Utility::TypeRecipient<CharacterMapper> {
private:
	constexpr uint8_t min_ram_slot(const Analyser::Static::Enterprise::Target &target) {
		const auto ram_size = [&] {
			switch(target.model) {
				case Analyser::Static::Enterprise::Target::Model::Enterprise64:		return 64*1024;
				default:
				case Analyser::Static::Enterprise::Target::Model::Enterprise128:	return 128*1024;
				case Analyser::Static::Enterprise::Target::Model::Enterprise256:	return 256*1024;
			}
		}();
		return uint8_t(0x100 - ram_size / 0x4000);
	}

	static constexpr double clock_rate = is_6mhz ? 6'000'000.0 : 4'000'000.0;
	using NickType =
		std::conditional_t<is_6mhz,
			JustInTimeActor<Nick, HalfCycles, 13478201, 5680000>,
			JustInTimeActor<Nick, HalfCycles, 40434603, 11360000>>;

public:
	ConcreteMachine(const Analyser::Static::Enterprise::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
		min_ram_slot_(min_ram_slot(target)),
		z80_(*this),
		host_fs_(*this),
		nick_(ram_.end() - 65536),
		dave_audio_(audio_queue_),
		speaker_(dave_audio_) {

		// Request a clock of 4Mhz; this'll be mapped upwards for Nick and downwards for Dave elsewhere.
		set_clock_rate(clock_rate);
		speaker_.set_input_rate(float(clock_rate) / float(dave_divider));

		ROM::Request request;
		using Target = Analyser::Static::Enterprise::Target;

		// Pick one or more EXOS ROMs.
		switch(target.exos_version) {
			case Target::EXOSVersion::v10:	request = request && ROM::Request(ROM::Name::EnterpriseEXOS10);	break;
			case Target::EXOSVersion::v20:	request = request && ROM::Request(ROM::Name::EnterpriseEXOS20);	break;
			case Target::EXOSVersion::v21:	request = request && ROM::Request(ROM::Name::EnterpriseEXOS21);	break;
			case Target::EXOSVersion::v23:	request = request && ROM::Request(ROM::Name::EnterpriseEXOS23);	break;
			case Target::EXOSVersion::Any:
				request =
					request && (
						ROM::Request(ROM::Name::EnterpriseEXOS10) || ROM::Request(ROM::Name::EnterpriseEXOS20) ||
						ROM::Request(ROM::Name::EnterpriseEXOS21) || ROM::Request(ROM::Name::EnterpriseEXOS23)
					);
			break;

			default: break;
		}

		// Similarly pick one or more BASIC ROMs.
		switch(target.basic_version) {
			case Target::BASICVersion::v10:
				request = request && (
					ROM::Request(ROM::Name::EnterpriseBASIC10) ||
					(
						ROM::Request(ROM::Name::EnterpriseBASIC10Part1) &&
						ROM::Request(ROM::Name::EnterpriseBASIC10Part2)
					)
				);
			break;
			case Target::BASICVersion::v11:
				request = request && (
					ROM::Request(ROM::Name::EnterpriseBASIC11) ||
					ROM::Request(ROM::Name::EnterpriseBASIC11Suffixed)
				);
			break;
			case Target::BASICVersion::v21:
				request = request && ROM::Request(ROM::Name::EnterpriseBASIC21);
			break;
			case Target::BASICVersion::Any:
				request =
					request && (
						ROM::Request(ROM::Name::EnterpriseBASIC10) ||
						(
							ROM::Request(ROM::Name::EnterpriseBASIC10Part1) &&
							ROM::Request(ROM::Name::EnterpriseBASIC10Part2)
						) ||
						ROM::Request(ROM::Name::EnterpriseBASIC11) ||
						ROM::Request(ROM::Name::EnterpriseBASIC21)
					);
			break;

			default: break;
		}

		// Possibly add in a DOS.
		switch(target.dos) {
			case Target::DOS::EXDOS:
				request = request && ROM::Request(ROM::Name::EnterpriseEXDOS);
			break;
			default: break;
		}

		// Get and validate ROMs.
		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		// Extract the appropriate EXOS ROM.
		exos_.fill(0xff);
		for(const auto rom_name: {
			ROM::Name::EnterpriseEXOS10, ROM::Name::EnterpriseEXOS20,
			ROM::Name::EnterpriseEXOS21, ROM::Name::EnterpriseEXOS23
		}) {
			const auto exos = roms.find(rom_name);
			if(exos != roms.end()) {
				memcpy(exos_.data(), exos->second.data(), std::min(exos_.size(), exos->second.size()));
				break;
			}
		}

		// Extract the appropriate BASIC ROM[s] (if any).
		basic_.fill(0xff);
		bool has_basic = false;
		for(const auto rom_name: {
			ROM::Name::EnterpriseBASIC10, ROM::Name::EnterpriseBASIC11,
			ROM::Name::EnterpriseBASIC11Suffixed, ROM::Name::EnterpriseBASIC21
		}) {
			const auto basic = roms.find(rom_name);
			if(basic != roms.end()) {
				memcpy(basic_.data(), basic->second.data(), std::min(basic_.size(), basic->second.size()));
				has_basic = true;
				break;
			}
		}
		if(!has_basic) {
			const auto basic1 = roms.find(ROM::Name::EnterpriseBASIC10Part1);
			const auto basic2 = roms.find(ROM::Name::EnterpriseBASIC10Part2);
			if(basic1 != roms.end() && basic2 != roms.end()) {
				memcpy(&basic_[0x0000], basic1->second.data(), std::min(size_t(8192), basic1->second.size()));
				memcpy(&basic_[0x2000], basic2->second.data(), std::min(size_t(8192), basic2->second.size()));
			}
		}

		// Extract the appropriate DOS ROMs.
		epdos_rom_.fill(0xff);
		const auto epdos = roms.find(ROM::Name::EnterpriseEPDOS);
		if(epdos != roms.end()) {
			memcpy(epdos_rom_.data(), epdos->second.data(), std::min(epdos_rom_.size(), epdos->second.size()));
		}
		exdos_rom_.fill(0xff);
		const auto exdos = roms.find(ROM::Name::EnterpriseEXDOS);
		if(exdos != roms.end()) {
			memcpy(exdos_rom_.data(), exdos->second.data(), std::min(exdos_rom_.size(), exdos->second.size()));
		}

		// Possibly install the host FS ROM.
		host_fs_rom_.fill(0xff);
		if(!target.media.file_bundles.empty()) {
			const auto rom = host_fs_.rom();
			std::copy(rom.begin(), rom.end(), host_fs_rom_.begin());
			find_host_fs_hooks();
		}

		// Seed key state.
		clear_all_keys();

		// Take a reasonable guess at the initial memory configuration:
		// put EXOS into the first bank since this is a Z80 and therefore
		// starts from address 0; the third instruction in EXOS is a jump
		// to $c02e so it's reasonable to assume EXOS is in the highest bank
		// too, and it appears to act correctly if it's the first 16kb that's
		// in the highest bank. From there I guess: all banks are initialised
		// to 0.
		page<0>(0x00);
		page<1>(0x00);
		page<2>(0x00);
		page<3>(0x00);

		// Pass on any media.
		insert_media(target.media);
		if(!target.loading_command.empty()) {
			type_string(target.loading_command);
		}

		// Ensure the splash screen is automatically skipped if any media has been provided.
		if(!target.media.empty()) {
			should_skip_splash_screen_ = !target.media.empty();
			typer_delay_ = 2;
		}
	}

	~ConcreteMachine() {
		audio_queue_.lock_flush();
	}

	// MARK: - Z80::BusHandler.
	forceinline void advance_nick(const HalfCycles duration) {
		if(nick_ += duration) {
			const auto nick = nick_.last_valid();
			const bool nick_interrupt_line = nick->get_interrupt_line();
			if(nick_interrupt_line && !previous_nick_interrupt_line_) {
				set_interrupts(uint8_t(Dave::Interrupt::Nick), nick_.last_sequence_point_overrun());
			}
			previous_nick_interrupt_line_ = nick_interrupt_line;
		}
	}

	forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
		using PartialMachineCycle = CPU::Z80::PartialMachineCycle;
		const uint16_t address = cycle.address ? *cycle.address : 0x0000;

		// Calculate an access penalty, if applicable.
		//
		// Rule applied here, which is slightly inferred:
		//
		//	Non-video reads and writes are delayed by exactly a cycle or not delayed at all,
		//	depending on the programmer's configuration of Dave.
		//
		//	Video reads and writes, and Nick port accesses, are delayed so that the last
		//	clock cycle of the machine cycle falls wholly inside the designated Z80 access
		//	window, per Nick.
		//
		// The switch statement below just attempts to implement that logic.
		//
		HalfCycles penalty;
		switch(cycle.operation) {
			default: break;

			// For non-video pauses, insert during the initial part of the bus cycle.
			case PartialMachineCycle::ReadStart:
			case PartialMachineCycle::WriteStart:
				if(!is_video_[address >> 14] && wait_mode_ == WaitMode::OnAllAccesses) {
					penalty = dave_delay_;
				}
			break;
			case PartialMachineCycle::ReadOpcodeStart: {
				if(is_video_[address >> 14]) {
					// Query Nick for the amount of delay that would occur with one cycle left
					// in this read opcode.
					const auto delay_time = nick_.time_since_flush(HalfCycles(2));
					const auto delay = nick_.last_valid()->get_time_until_z80_slot(delay_time);
					penalty = nick_.back_map(delay, delay_time);
				} else if(wait_mode_ != WaitMode::None) {
					penalty = dave_delay_;
				}
			} break;

			// Video pauses: insert right at the end of the bus cycle.
			case PartialMachineCycle::Write:
				// Ensure all video that should have been collected prior to
				// this write has been.
				if(is_video_[address >> 14]) {
					nick_.flush();
				}
				[[fallthrough]];

			case PartialMachineCycle::Read:
				if(is_video_[address >> 14]) {
					// Get delay, in Nick cycles, for a Z80 access that occurs in 0.5
					// cycles from now (i.e. with one cycle left to run).
					const auto delay_time = nick_.time_since_flush(HalfCycles(1));
					const auto delay = nick_.last_valid()->get_time_until_z80_slot(delay_time);
					penalty = nick_.back_map(delay, delay_time);
				}
			break;

			case PartialMachineCycle::Input:
			case PartialMachineCycle::Output: {
				if((address & 0xf0) == 0x80) {
					// Get delay, in Nick cycles, for a Z80 access that occurs in 0.5
					// cycles from now (i.e. with one cycle left to run).
					const auto delay_time = nick_.time_since_flush(HalfCycles(1));
					const auto delay = nick_.last_valid()->get_time_until_z80_slot(delay_time);
					penalty = nick_.back_map(delay, delay_time);
				}
			}
		}

		const HalfCycles full_length = cycle.length + penalty;
		time_since_audio_update_ += full_length;
		advance_nick(full_length);
		if(dave_timer_ += full_length) {
			set_interrupts(dave_timer_.last_valid()->get_new_interrupts(), dave_timer_.last_sequence_point_overrun());
		}

		// The WD/etc runs at a nominal 8Mhz.
		if constexpr (has_disk_controller) {
			exdos_.run_for(Cycles(full_length.as_integral()));
		}

		switch(cycle.operation) {
			default: break;

			case PartialMachineCycle::Interrupt:
				*cycle.value = 0xff;
			break;

			case PartialMachineCycle::Input:
				switch(address & 0xff) {
					default:
						Logger::error().append("Unhandled input from %02x", address & 0xff);
						*cycle.value = 0xff;
					break;

					case 0x10:	case 0x11:	case 0x12:	case 0x13:
					case 0x14:	case 0x15:	case 0x16:	case 0x17:
						if constexpr (has_disk_controller) {
							*cycle.value = exdos_.read(address);
						} else {
							*cycle.value = 0xff;
						}
					break;
					case 0x18:	case 0x19:	case 0x1a:	case 0x1b:
					case 0x1c:	case 0x1d:	case 0x1e:	case 0x1f:
						if constexpr (has_disk_controller) {
							*cycle.value = exdos_.get_control_register();
						} else {
							*cycle.value = 0xff;
						}
					break;

					case 0x80:	case 0x81:	case 0x82:	case 0x83:
					case 0x84:	case 0x85:	case 0x86:	case 0x87:
					case 0x88:	case 0x89:	case 0x8a:	case 0x8b:
					case 0x8c:	case 0x8d:	case 0x8e:	case 0x8f:
						*cycle.value = nick_->read();
					break;

					case 0xb0:	*cycle.value = pages_[0];	break;
					case 0xb1:	*cycle.value = pages_[1];	break;
					case 0xb2:	*cycle.value = pages_[2];	break;
					case 0xb3:	*cycle.value = pages_[3];	break;

					case 0xb4:
						*cycle.value =
							(nick_->get_interrupt_line() ? 0x10 : 0x00) |
							dave_timer_->get_divider_state() |
							interrupt_state_;
					break;
					case 0xb5:
						if(active_key_line_ < key_lines_.size()) {
							*cycle.value = key_lines_[active_key_line_];
						} else {
							*cycle.value = 0xff;
						}
					break;
					case 0xb6: {
						// TODO: selected keyboard row, 0 to 9, should return one bit of joystick
						// input. That being the case:
						//
						//	b0:		joystick input
						//	b1, b2:	unused (in theory read from control port, but not used by any hardware)
						//	b3:		0 = printer ready; 1 = not ready
						//	b4:		serial, data in
						//	b5:		serial, status in
						//	b6:		tape input volume level, 0 = high, 1 = low
						//	b7:		tape data input
						*cycle.value = 0xff;
					} break;
				}
			break;

			case PartialMachineCycle::Output:
				switch(address & 0xff) {
					default:
						Logger::error().append("Unhandled output: %02x to %02x", *cycle.value, address & 0xff);
					break;

					case 0x10:	case 0x11:	case 0x12:	case 0x13:
					case 0x14:	case 0x15:	case 0x16:	case 0x17:
						if constexpr(has_disk_controller) {
							exdos_.write(address, *cycle.value);
						}
					break;
					case 0x18:	case 0x19:	case 0x1a:	case 0x1b:
					case 0x1c:	case 0x1d:	case 0x1e:	case 0x1f:
						if constexpr(has_disk_controller) {
							exdos_.set_control_register(*cycle.value);
						}
					break;

					case 0x80:	case 0x81:	case 0x82:	case 0x83:
					case 0x84:	case 0x85:	case 0x86:	case 0x87:
					case 0x88:	case 0x89:	case 0x8a:	case 0x8b:
					case 0x8c:	case 0x8d:	case 0x8e:	case 0x8f:
						nick_->write(address, *cycle.value);
					break;

					case 0xb0:	page<0>(*cycle.value);	break;
					case 0xb1:	page<1>(*cycle.value);	break;
					case 0xb2:	page<2>(*cycle.value);	break;
					case 0xb3:	page<3>(*cycle.value);	break;

					case 0xbf:
						switch((*cycle.value >> 2)&3) {
							default:	wait_mode_ = WaitMode::None;			break;
							case 0:		wait_mode_ = WaitMode::OnAllAccesses;	break;
							case 1:		wait_mode_ = WaitMode::OnM1;			break;
						}

						// Dave delays (i.e. those affecting memory areas not associated with Nick)
						// are one cycle in 8Mhz mode, two cycles in 12Mhz mode.
						dave_delay_ = HalfCycles(2 + ((*cycle.value)&2));

						[[fallthrough]];

					case 0xa0:	case 0xa1:	case 0xa2:	case 0xa3:
					case 0xa4:	case 0xa5:	case 0xa6:	case 0xa7:
					case 0xa8:	case 0xa9:	case 0xaa:	case 0xab:
					case 0xac:	case 0xad:	case 0xae:	case 0xaf:
						update_audio();
						dave_audio_.write(address, *cycle.value);
						dave_timer_->write(address, *cycle.value);
					break;

					case 0xb4:
						interrupt_mask_ = *cycle.value & 0x55;
						interrupt_state_ &= ~*cycle.value;
						update_interrupts();
					break;
					case 0xb5:
						// Logic here: the ROM scans the keyboard by checking ascending
						// lines. It also seems to provide a line of 0 when using port B5
						// for non-keyboard uses.
						//
						// So: use the rollover from line 9 back to line 0 as a trigger to
						// spot that a scan of the keyboard just finished. Which makes it
						// time to enqueue the next keypress.
						//
						// Re: should_skip_splash_screen_ and typer_delay_, assume that a
						// single keypress is necessary to get past the Enterprise splash
						// screen, then a pause in keypressing while BASIC or whatever
						// starts up, then presses can resume.
						if(active_key_line_ == 9 && !(*cycle.value & 0xf) && (should_skip_splash_screen_ || typer_)) {
							if(should_skip_splash_screen_) {
								set_key_state(uint16_t(Key::Space), typer_delay_);
								if(typer_delay_) {
									--typer_delay_;
								} else {
									typer_delay_ = 60;
									should_skip_splash_screen_ = false;
								}
							} else {
								if(!typer_delay_) {
									if(!typer_->type_next_character()) {
										clear_all_keys();
										typer_ = nullptr;
									}
								} else {
									--typer_delay_;
								}
							}
						}

						active_key_line_ = *cycle.value & 0xf;
						// TODO:
						//
						//	b4:	strobe output for printer
						//	b5:	tape sound control (?)
						//	b6:	tape motor control 1, 1 = on
						//	b7: tape motor control 2, 1 = on
					break;
					case 0xb6:
						// Just 8 bits of printer data.
						Logger::info().append("TODO: printer output: %02x", *cycle.value);
					break;
					case 0xb7:
						// b0 = serial data out
						// b1 = serial status out
						Logger::info().append("TODO: serial output: %02x", *cycle.value);
					break;
				}
			break;

			case PartialMachineCycle::ReadOpcode:
				{
					static bool print_opcode = false;
					if(print_opcode) {
						printf("%04x: %02x\n", address, read_pointers_[address >> 14][address]);
					}
				}

				// Potential segue for the host FS. I'm relying on branch prediction to
				// avoid this cost almost always.
				if(test_host_fs_traps_ && (address >> 14) == 3) [[unlikely]] {
					const auto is_trap = host_fs_traps_.contains(address);

					if(is_trap) {
						using Register = CPU::Z80::Register;

						uint8_t a = uint8_t(z80_.value_of(Register::A));
						uint16_t bc = z80_.value_of(Register::BC);
						uint16_t de = z80_.value_of(Register::DE);

						// Grab function code from where the PC actually is, and return a NOP
						host_fs_.perform(read_pointers_[address >> 14][address], a, bc, de);
						*cycle.value = 0x00;	// i.e. NOP.

						z80_.set_value_of(Register::A, a);
						z80_.set_value_of(Register::BC, bc);
						z80_.set_value_of(Register::DE, de);

						break;
					}
				}
				[[fallthrough]];

			case PartialMachineCycle::Read:
				if(read_pointers_[address >> 14]) {
					*cycle.value = read_pointers_[address >> 14][address];
				} else {
					*cycle.value = 0xff;
				}
			break;

			case PartialMachineCycle::Write:
				if(write_pointers_[address >> 14]) {
					write_pointers_[address >> 14][address] = *cycle.value;
				}
			break;
		}

		return penalty;
	}

	void flush_output(const int outputs) final {
		if(outputs & Output::Video) {
			nick_.flush();
		}
		if(outputs & Output::Audio) {
			update_audio();
			audio_queue_.perform();
		}
	}

private:
	// MARK: - Memory layout

	std::array<uint8_t, 256 * 1024> ram_{};
	std::array<uint8_t, 64 * 1024> exos_;
	std::array<uint8_t, 16 * 1024> basic_;
	std::array<uint8_t, 16 * 1024> exdos_rom_;
	std::array<uint8_t, 32 * 1024> epdos_rom_;
	std::array<uint8_t, 16 * 1024> host_fs_rom_;
	const uint8_t min_ram_slot_;

	uint8_t *ram_segment(const uint8_t page) {
		if(page < min_ram_slot_) return nullptr;
		const auto ram_floor = (0x100 << 14) - ram_.size();
			// Each segment is 2^14 bytes long and there are 256 of them. So the Enterprise has a 22-bit address space.
			// RAM is at the end of that range; `ram_floor` is the 22-bit address at which RAM starts.
		return &ram_[(page << 14) - ram_floor];
	}

	const uint8_t *read_pointers_[4] = {nullptr, nullptr, nullptr, nullptr};
	uint8_t *write_pointers_[4] = {nullptr, nullptr, nullptr, nullptr};
	uint8_t pages_[4] = {0x80, 0x80, 0x80, 0x80};

	template <size_t slot, typename RomT>
	bool page_rom(const uint8_t offset, const uint8_t location, const RomT &source) {
		if(offset < location || offset >= location + source.size() / 0x4000) {
			return false;
		}

		page<slot>(&source[(offset - location) * 0x4000], nullptr);
		is_video_[slot] = false;
		return true;
	}

	template <size_t slot> void page(const uint8_t offset) {
		pages_[slot] = offset;

		if constexpr (slot == 3) {
			test_host_fs_traps_ = false;
		}

		if(page_rom<slot>(offset, 0, exos_)) return;
		if(page_rom<slot>(offset, 16, basic_)) return;
		if(page_rom<slot>(offset, 32, exdos_rom_)) return;
		if(page_rom<slot>(offset, 48, epdos_rom_)) return;
		if(page_rom<slot>(offset, 64, host_fs_rom_)) {
			if constexpr (slot == 3) {
				test_host_fs_traps_ = true;
			}
			return;
		}

		// Of whatever size of RAM I've declared above, use only the final portion.
		// This correlated with Nick always having been handed the final 64kb and,
		// at least while the RAM is the first thing declared above, does a little
		// to benefit data locality. Albeit not in a useful sense.
		if(offset >= min_ram_slot_) {
			is_video_[slot] = offset >= 0xfc;	// TODO: this hard-codes a 64kb video assumption.
			auto pointer = ram_segment(offset);
			page<slot>(pointer, pointer);
			return;
		}

		page<slot>(nullptr, nullptr);
	}

	template <size_t slot> void page(const uint8_t *const read, uint8_t *const write) {
		read_pointers_[slot] = read ? read - (slot * 0x4000) : nullptr;
		write_pointers_[slot] = write ? write - (slot * 0x4000) : nullptr;
	}

	// MARK: - Memory Timing
	// The wait mode affects all memory accesses _outside of the video area_.
	enum class WaitMode {
		None,
		OnM1,
		OnAllAccesses
	} wait_mode_ = WaitMode::OnAllAccesses;
	bool is_video_[4]{};

	// MARK: - ScanProducer

	void set_scan_target(Outputs::Display::ScanTarget *const scan_target) override {
		nick_.last_valid()->set_scan_target(scan_target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const override {
		return nick_.last_valid()->get_scaled_scan_status();
	}

	void set_display_type(const Outputs::Display::DisplayType display_type) final {
		nick_.last_valid()->set_display_type(display_type);
	}

	Outputs::Display::DisplayType get_display_type() const final {
		return nick_.last_valid()->get_display_type();
	}

	// MARK: - AudioProducer

	Outputs::Speaker::Speaker *get_speaker() final {
		return &speaker_;
	}

	// MARK: - TimedMachine
	void run_for(const Cycles cycles) override {
		z80_.run_for(cycles);
	}

	// MARK: - KeyboardMachine
	Enterprise::KeyboardMapper keyboard_mapper_;
	KeyboardMapper *get_keyboard_mapper() final {
		return &keyboard_mapper_;
	}

	uint8_t active_key_line_ = 0;
	std::array<uint8_t, 10> key_lines_;
	void set_key_state(const uint16_t key, const bool is_pressed) final {
		if(is_pressed) {
			key_lines_[key >> 8] &= ~uint8_t(key);
		} else {
			key_lines_[key >> 8] |= uint8_t(key);
		}
	}

	void clear_all_keys() final {
		key_lines_.fill(0xff);
	}

	// MARK: - Utility::TypeRecipient
	void type_string(const std::string &string) final {
		Utility::TypeRecipient<CharacterMapper>::add_typer(string);

		if(z80_.get_is_resetting()) {
			should_skip_splash_screen_ = true;
			typer_delay_ = 1;
		} else {
			should_skip_splash_screen_ = false;
			typer_delay_ = 0;
		}
	}

	bool can_type(const char c) const final {
		return Utility::TypeRecipient<CharacterMapper>::can_type(c);
	}

	bool should_skip_splash_screen_ = false;
	int typer_delay_ = 30;

	// MARK: - MediaTarget
	bool insert_media(const Analyser::Static::Media &media) final {
		if constexpr (has_disk_controller) {
			if(!media.disks.empty()) {
				exdos_.set_disk(media.disks.front(), 0);
			}
		}

		if(!media.file_bundles.empty()) {
			host_fs_.set_file_bundle(media.file_bundles.front());
		}

		return true;
	}

	// MARK: - Interrupts
	uint8_t interrupt_mask_ = 0x00, interrupt_state_ = 0x00;
	void set_interrupts(const uint8_t mask, const HalfCycles offset = HalfCycles(0)) {
		interrupt_state_ |= uint8_t(mask);
		update_interrupts(offset);
	}
	void update_interrupts(const HalfCycles offset = HalfCycles(0)) {
		z80_.set_interrupt_line((interrupt_state_ >> 1) & interrupt_mask_, offset);
	}

	// MARK: - Chips.
	CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
	NickType nick_;
	bool previous_nick_interrupt_line_ = false;
	// Cf. timing guesses above.

	Concurrency::AsyncTaskQueue<false> audio_queue_;
	Dave::Audio dave_audio_;
	Outputs::Speaker::PullLowpass<Dave::Audio> speaker_;
	HalfCycles time_since_audio_update_;

	HalfCycles dave_delay_ = HalfCycles(2);

	// The divider supplied to the JustInTimeActor and the manual divider used in
	// update_audio() should match.
	static constexpr int dave_divider = 8;
	JustInTimeActor<Dave::TimedInterruptSource, HalfCycles, 1, dave_divider> dave_timer_;
	inline void update_audio() {
		speaker_.run_for(audio_queue_, time_since_audio_update_.divide_cycles(Cycles(dave_divider)));
	}

	// MARK: - EXDos card.

	EXDos exdos_;

	// MARK: - Host FS.

	HostFSHandler host_fs_;
	std::unordered_set<uint16_t> host_fs_traps_;
	bool test_host_fs_traps_ = false;

	uint8_t hostfs_read(const uint16_t address) override {
		if(read_pointers_[address >> 14]) {
			return read_pointers_[address >> 14][address];
		} else {
			return 0xff;
		}
	}

	void hostfs_user_write(const uint16_t address, const uint8_t value) override {
		// "User" writes go to to wherever the user last had paged;
		// per 5.4 System Segment Usage those pages are stored in memory from
		// 0xbffc, so grab from there.
		const auto page_id = address >> 14;
		const auto page = read_pointers_[0xbffc >> 14] ? read_pointers_[0xbffc >> 14][0xbffc + page_id] : 0xff;
		const auto offset = address & 0x3fff;
		ram_segment(page)[offset] = value;
	}

	void find_host_fs_hooks() {
		static constexpr uint8_t syscall[] = {
			0xed, 0xfe, 0xfe
		};

		auto begin = host_fs_rom_.begin();
		while(true) {
			begin = std::search(
				begin, host_fs_rom_.end(),
				std::begin(syscall), std::end(syscall)
			);

			if(begin == host_fs_rom_.end()) {
				break;
			}

			const auto offset = begin - host_fs_rom_.begin() + 0xc000;	// ROM will be paged in slot 3, i.e. at $c000.
			host_fs_traps_.insert(offset);

			// Move function code up to where this trap was, and NOP out the tail.
			begin[0] = begin[3];
			begin[1] = begin[2] = begin[3] = 0x00;
			begin += 4;
		}
	}

	// MARK: - Activity Source

	void set_activity_observer([[maybe_unused]] Activity::Observer *const observer) final {
		if constexpr (has_disk_controller) {
			exdos_.set_activity_observer(observer);
		}
	}

	// MARK: - Configuration options.

	std::unique_ptr<Reflection::Struct> get_options() const final {
		auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
		options->output = get_video_signal_configurable();
		return options;
	}

	void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
		const auto options = dynamic_cast<Options *>(str.get());
		set_video_signal_configurable(options->output);
	}
};

}

using namespace Enterprise;

namespace {

template <bool has_disk_controller>
std::unique_ptr<Machine> machine(
	const Analyser::Static::Enterprise::Target &target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	if(target.speed == Analyser::Static::Enterprise::Target::Speed::SixMHz) {
		return std::make_unique<Enterprise::ConcreteMachine<has_disk_controller, true>>(target, rom_fetcher);
	} else {
		return std::make_unique<Enterprise::ConcreteMachine<has_disk_controller, false>>(target, rom_fetcher);
	}
}

}

std::unique_ptr<Machine> Machine::Enterprise(
	const Analyser::Static::Target *const target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	using Target = Analyser::Static::Enterprise::Target;
	const Target &enterprise_target = *dynamic_cast<const Target *>(target);

	if(enterprise_target.dos != Target::DOS::None) {
		return machine<true>(enterprise_target, rom_fetcher);
	} else {
		return machine<false>(enterprise_target, rom_fetcher);
	}
}

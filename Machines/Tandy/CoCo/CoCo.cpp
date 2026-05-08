//
//  CoCo.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CoCo.hpp"

#include "Keyboard.hpp"

#include "Processors/6809/6809.hpp"
#include "Components/6821/6821.hpp"
#include "Components/6847/6847.hpp"

#include "Activity/Source.hpp"
#include "Analyser/Static/TandyCoCo/Target.hpp"
#include "ClockReceiver/JustInTime.hpp"
#include "Machines/MachineTypes.hpp"
#include "Machines/Utility/MemoryFuzzer.hpp"

using namespace Tandy::CoCo;

namespace {

struct MemoryMap {
	uint8_t read(const uint16_t address) const {
		return read_[address >> 13] ? read_[address >> 13][address] : 0xff;
	}

	void write(const uint16_t address, uint8_t value) {
		if(!write_[address >> 13]) return;
		write_[address >> 13][address] = value;
	}

	void set_read(const uint16_t begin, const uint16_t end, const uint8_t *data) {
		assert(!(begin & 0x1fff));
		assert(!(end & 0x1fff));
		for(int page = begin >> 13; page < end >> 13; page++) {
			read_[page] = data - begin;
		}
	}

	void set_readwrite(const uint16_t begin, const uint16_t end, uint8_t *data) {
		assert(!(begin & 0x1fff));
		assert(!(end & 0x1fff));
		for(int page = begin >> 13; page < end >> 13; page++) {
			read_[page] = write_[page] = data - begin;
		}
	}

private:
	uint8_t *write_[8]{};
	const uint8_t *read_[8]{};
};

struct Joystick: public Inputs::ConcreteJoystick {
	Joystick() :
		ConcreteJoystick({
			Input(Input::Horizontal),
			Input(Input::Vertical),
			Input(Input::Fire, 0),
			Input(Input::Fire, 1)
		}) {}

	void did_set_input(const Input &input, const float value) final {
		const size_t index = input.type == Input::Type::Vertical;
		axes[index] = std::clamp<uint8_t>(uint8_t(value * 63.0), 0, 63);
	}

	void did_set_input(const Input &input, const bool value) final {
		if(input.type != Input::Type::Fire) {
			return;
		}
		buttons[input.info.control.index] = value;
	}

	uint8_t axes[2]{32, 32};
	bool buttons[2]{};
};

static constexpr double ClockRate = 1'789'772.5;

}

namespace TandyCoCo {

class ConcreteMachine:
	public Activity::Source,
	public Configurable::Device,
	public Machine,
	public MachineTypes::JoystickMachine,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaChangeObserver,
	public MachineTypes::MediaTarget,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine
{
public:
	ConcreteMachine(const Analyser::Static::TandyCoCo::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
		m6809_(*this),
		pia0_handler_(*this),
		pia0_(pia0_handler_),
		pia1_handler_(*this),
		pia1_(pia1_handler_),
		sam_(memory_),
		m6847_(sam_),
		tape_player_(int(ClockRate))
	{
		set_clock_rate(ClockRate);
		construct_joysticks();

		const auto BasicROM = ROM::Name::TandyCoCoColourBasic12;
		auto request = ROM::Request(BasicROM);

		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		{
			auto rom = roms.find(BasicROM)->second;
			std::copy_n(rom.begin(), 8 * 1024, colour_basic_.begin());
		}

		memory_.set_readwrite(0x0000, 0x8000, ram_.data());
		memory_.set_read(0xa000, 0xc000, colour_basic_.data());
		Memory::Fuzz(ram_);

		insert_media(target.media);
	}

	template <
		CPU::M6809::BusPhase bus_phase,
		CPU::M6809::LIC lic,
		CPU::M6809::ReadWrite read_write,
		CPU::M6809::BusState bus_state,
		typename AddressT
	>
	Cycles perform(
		const AddressT address,
		CPU::M6809::data_t<read_write> value
	) {
		// TODO, maybe: pull the switch inside this SAM call outside the loop?
		const auto delay = sam_.cycle_cost<bus_phase, read_write>(address);
		const auto duration = delay + CPU::M6809::duration<Cycles>(bus_phase);
		if(m6847_ += duration) {
			pia0_.set<Motorola::MC6821::Control::CA1>(m6847_.get()->hsync());
			pia0_.set<Motorola::MC6821::Control::CB1>(m6847_.get()->vsync());
		}
		tape_player_.run_for(duration);

		using namespace CPU::M6809;
		if(address >> 8 == 0xff) {
			if constexpr (read_write != ReadWrite::NoData) {
				switch(address) {
					default: printf("Unhandled at %04x\n", +address);	break;

					case 0xff00:	case 0xff04:	case 0xff08:	case 0xff0c:
					case 0xff10:	case 0xff14:	case 0xff18:	case 0xff1c:
						access<0xff00, read_write>(pia0_, value);
					break;
					case 0xff01:	case 0xff05:	case 0xff09:	case 0xff0d:
					case 0xff11:	case 0xff15:	case 0xff19:	case 0xff1d:
						access<0xff01, read_write>(pia0_, value);
					break;
					case 0xff02:	case 0xff06:	case 0xff0a:	case 0xff0e:
					case 0xff12:	case 0xff16:	case 0xff1a:	case 0xff1e:
						access<0xff02, read_write>(pia0_, value);
					break;
					case 0xff03:	case 0xff07:	case 0xff0b:	case 0xff0f:
					case 0xff13:	case 0xff17:	case 0xff1b:	case 0xff1f:
						access<0xff03, read_write>(pia0_, value);
					break;

					case 0xff20:	case 0xff24:	case 0xff28:	case 0xff2c:
					case 0xff30:	case 0xff34:	case 0xff38:	case 0xff3c:
						access<0xff20, read_write>(pia1_, value);
					break;
					case 0xff21:	case 0xff25:	case 0xff29:	case 0xff2d:
					case 0xff31:	case 0xff35:	case 0xff39:	case 0xff3d:
						access<0xff21, read_write>(pia1_, value);
					break;
					case 0xff22:	case 0xff26:	case 0xff2a:	case 0xff2e:
					case 0xff32:	case 0xff36:	case 0xff3a:	case 0xff3e:
						access<0xff22, read_write>(pia1_, value);
					break;
					case 0xff23:	case 0xff27:	case 0xff2b:	case 0xff2f:
					case 0xff33:	case 0xff37:	case 0xff3b:	case 0xff3f:
						access<0xff23, read_write>(pia1_, value);
					break;

					case 0xffc0:	sam_.access<0xffc0>();	break;		case 0xffc1:	sam_.access<0xffc1>();	break;
					case 0xffc2:	sam_.access<0xffc2>();	break;		case 0xffc3:	sam_.access<0xffc3>();	break;
					case 0xffc4:	sam_.access<0xffc4>();	break;		case 0xffc5:	sam_.access<0xffc5>();	break;
					case 0xffc6:	sam_.access<0xffc6>();	break;		case 0xffc7:	sam_.access<0xffc7>();	break;
					case 0xffc8:	sam_.access<0xffc8>();	break;		case 0xffc9:	sam_.access<0xffc9>();	break;
					case 0xffca:	sam_.access<0xffca>();	break;		case 0xffcb:	sam_.access<0xffcb>();	break;
					case 0xffcc:	sam_.access<0xffcc>();	break;		case 0xffcd:	sam_.access<0xffcd>();	break;
					case 0xffce:	sam_.access<0xffce>();	break;		case 0xffcf:	sam_.access<0xffcf>();	break;
					case 0xffd0:	sam_.access<0xffd0>();	break;		case 0xffd1:	sam_.access<0xffd1>();	break;
					case 0xffd2:	sam_.access<0xffd2>();	break;		case 0xffd3:	sam_.access<0xffd3>();	break;
					case 0xffd4:	sam_.access<0xffd4>();	break;		case 0xffd5:	sam_.access<0xffd5>();	break;
					case 0xffd6:	sam_.access<0xffd6>();	break;		case 0xffd7:	sam_.access<0xffd7>();	break;
					case 0xffd8:	sam_.access<0xffd8>();	break;		case 0xffd9:	sam_.access<0xffd9>();	break;
					case 0xffda:	sam_.access<0xffda>();	break;		case 0xffdb:	sam_.access<0xffdb>();	break;
					case 0xffdc:	sam_.access<0xffdc>();	break;		case 0xffdd:	sam_.access<0xffdd>();	break;
					case 0xffde:	sam_.access<0xffde>();	break;		case 0xffdf:	sam_.access<0xffdf>();	break;

					case 0xfff2:	case 0xfff3:	case 0xfff4:	case 0xfff5:
					case 0xfff6:	case 0xfff7:	case 0xfff8:	case 0xfff9:
					case 0xfffa:	case 0xfffb:	case 0xfffc:	case 0xfffd:
					case 0xfffe:	case 0xffff:
						if constexpr (is_read(read_write)) {
							value = memory_.read(address - 0x4000);
						}
					break;
				}
			}
		} else {
			if constexpr (is_read(read_write)) {
				value = memory_.read(address);

				if constexpr (lic == CPU::M6809::LIC::InstructionFetch) {
					if(allow_fast_tape_loading_ && tape_player_.motor_control() && address == 0xa75d) {
						// To reproduce:
						//
						// LA75D 	CLR CPERTM		; RESET PERIOD TIMER
						//			TST CBTPHA		; CHECK TO SEE IF SYNC’ED ON THE HI-LO TRANSITION OR LO-HI
						//			BNE LA773		; BRANCH ON HI-LO TRANSITION
						//
						// * LO - HI TRANSITION
						// LA763	BSR LA76C		; READ CASSETTE INPUT BIT					8D 07;		7 cycles
						//			BCS LA763		; LOOP UNTIL IT IS LO						25 FC;		3 cycles
						// LA767	BSR LA76C		; READ CASSETTE INPUT DATA
						//			BCC LA767		; WAIT UNTIL IT GOES HI
						//			RTS
						//
						// * READ CASSETTE INPUT BIT OF THE PIA
						// LA76C 	INC CPERTM		; INCREMENT PERIOD TIMER					0C 83;		6 cycles
						//			LDB PIA1		; GET CASSETTE INPUT BIT					F6 FF 20;	5 cycles
						//			RORB			; PUT CASSETTE BIT INTO THE CARRY FLAG		56;			2 cycles
						//			RTS															39;			5 cycles
						//
						// * WAIT FOR HI - LO TRANSITION
						// LA773 	BSR LA76C		; READ CASSETTE INPUT DATA
						//			BCC LA773		; LOOP UNTIL IT IS HI
						// LA777 	BSR LA76C		; READ CASSETTE INPUT
						//			BCS LA777		; LOOP UNTIL IT IS LO
						//			RTS
						//
						// where: CPERTM = $0083; CBTPHA = $0084.
						//
						// The value of B is immediately discared by the caller so all that's needed is to set
						// CPERTM to the amount of time taken to observe the two transitions required, divided
						// by 28 cycles = 28/894886.25 s.
						//

						bool polarity = memory_.read(0x0084);

						Cycles::IntType total = 0;
						for(int c = 0; c < 2; c++) {
							while(tape_player_.input() != polarity) {
								total += tape_player_.get_cycles_until_next_event();
								tape_player_.run_for_input_pulse();
							}
							polarity ^= true;
						}

						memory_.write(0x0083, uint8_t(total / 56));
						value = 0x39;
					}
				}
			}

			if constexpr (is_write(read_write)) {
				// TODO: could do better with the test below by having SAM calculate a range?
				const uint16_t video_start = sam_.graphics_address();
				if(address >= video_start && address < video_start + 6144) {
					m6847_.flush();
				}

				memory_.write(address, value);
			}
		}

		return duration;
	}

private:
	struct M6809Traits {
		static constexpr bool uses_mrdy = false;
		static constexpr auto pause_precision = CPU::M6809::PausePrecision::BetweenInstructions;
		using BusHandlerT = ConcreteMachine;
	};
	CPU::M6809::Processor<M6809Traits> m6809_;
	MemoryMap memory_;
	std::array<uint8_t, 8 * 1024> colour_basic_;
	std::array<uint8_t, 64 * 1024> ram_;

	// MARK: - TimedMachine.

	void run_for(const Cycles cycles) final {
		m6809_.run_for(cycles);
	}

	void flush_output(const int outputs) final {
		if(outputs & Output::Video) {
			m6847_.flush();
		}
	}

	// MARK: - PIAs.

	friend struct PIA0Handler;
	struct PIA0Handler {
		PIA0Handler(ConcreteMachine &machine) : machine_(machine) {
			clear_all_keys();
		}

		//
		// Port A:
		//
		//	b7: joystick comparison input
		//	b0–b6: keyboard row [input?]
		//	b0–b1, b2–b3: joystick button inputs;
		//		0 and 2 = right joystick; 1 and 3 = left joystick;
		//		0 and 1 = switch 1; 2 and 3 = switch 2.
		//
		//	CA1: hsync
		//	CA2: select line LSB of joystick MUX

		//
		// Port B:
		//
		//	b0-b7: keyboard column [output?]
		//
		//	CB1: vsync
		//	CB2: select line MSB of joystick MUX

		// Interrupt outputs both connected to IRQ.

		template <Motorola::MC6821::Port port>
		uint8_t input() {
			if constexpr (port == Motorola::MC6821::Port::A) {
				return
					(keyboard_column_ & 0x80 ? 0xff : keys_[7]) &
					(keyboard_column_ & 0x40 ? 0xff : keys_[6]) &
					(keyboard_column_ & 0x20 ? 0xff : keys_[5]) &
					(keyboard_column_ & 0x10 ? 0xff : keys_[4]) &
					(keyboard_column_ & 0x08 ? 0xff : keys_[3]) &
					(keyboard_column_ & 0x04 ? 0xff : keys_[2]) &
					(keyboard_column_ & 0x02 ? 0xff : keys_[1]) &
					(keyboard_column_ & 0x01 ? 0xff : keys_[0]) &
					(
						machine_.dac_level_ > machine_.joystick(mux_ >> 1).axes[mux_ & 1]
							? 0x7f : 0xff
					) &
					(machine_.joystick(1).buttons[1] ? 0xf7 : 0xff) &
					(machine_.joystick(0).buttons[1] ? 0xfb : 0xff) &
					(machine_.joystick(1).buttons[0] ? 0xfd : 0xff) &
					(machine_.joystick(0).buttons[0] ? 0xfe : 0xff);
			}

			if constexpr (port == Motorola::MC6821::Port::B) {
				return 0xff;
			}

			__builtin_unreachable();
		}

		template <Motorola::MC6821::Port port>
		void output(const uint8_t value) {
			if constexpr (port == Motorola::MC6821::Port::B) {
				keyboard_column_ = value;
			}
		}

		template <Motorola::MC6821::IRQ irq>
		void set(const bool active) {
			if constexpr (irq == Motorola::MC6821::IRQ::A) {
				machine_.set_irq<0>(active);
			}

			if constexpr (irq == Motorola::MC6821::IRQ::B) {
				machine_.set_irq<1>(active);
			}
		}

		template <Motorola::MC6821::Control control>
		void observe(const bool value) {
			if constexpr (control == Motorola::MC6821::Control::CA2) {
				mux_ = (mux_ & 1) | (value ? 2 : 0);
			}
			if constexpr (control == Motorola::MC6821::Control::CB2) {
				mux_ = (mux_ & 2) | (value ? 1 : 0);
			}
		}

		void set_key_pressed(const int column, const int row, bool is_pressed) {
			const auto mask = uint8_t(0xff ^ (1 << column));
			keys_[row] &= mask;
			if(!is_pressed) keys_[row] |= ~mask;
		}

		void clear_all_keys() {
			std::fill(std::begin(keys_), std::end(keys_), 0xff);
		}

	private:
		ConcreteMachine &machine_;
		uint8_t keyboard_column_ = 0xff;
		uint8_t keys_[8]{};
		uint8_t mux_ = 0;
	};
	PIA0Handler pia0_handler_;
	Motorola::MC6821::MC6821<PIA0Handler> pia0_;

	bool irqs_[2]{};
	template <int slot>
	void set_irq(const bool active) {
		irqs_[slot] = active;
		m6809_.set<CPU::M6809::Line::IRQ>(irqs_[0] || irqs_[1]);
	}

	friend struct PIA1Handler;
	struct PIA1Handler {
		PIA1Handler(ConcreteMachine &machine) : machine_(machine) {}

		//
		// Port A:
		//
		//	b2-b7: 6-bit DAC output
		//	b1: RS232 data output
		//	b0: tape input
		//
		//	CA1: RS232 carrier detect
		//	CA2: tape motor control

		//
		// Port B:
		//
		//	b7: 6847 alpha/graphics select (0 = alphanumeric)
		//	b4–b6: VDG GM inputs; also b5 = 6847 invert; b4 = 6847 shift toggle
		//	b3: colour set select (and RGB monitor detecting input? Probably CoCo3)
		//	b2: ram size input
		//	b1: 1-bit sound/tape output
		//	b0: RS232 data input
		//
		//	CB1: cartridge interrupt input
		//	CB2: sound enable

		// Interrupt output connected to FIRQ.

		template <Motorola::MC6821::Port port>
		uint8_t input() {
			if constexpr (port == Motorola::MC6821::Port::A) {
				return
					0xfe |
					(machine_.tape_player_.input() ? 0x01 : 0x00);
			}

			if constexpr (port == Motorola::MC6821::Port::B) {
				return 0xff;
			}

			__builtin_unreachable();
		}

		template <Motorola::MC6821::Port port>
		void output(const uint8_t value) {
			if constexpr (port == Motorola::MC6821::Port::A) {
				machine_.dac_level_ = (value >> 2) & 0b111111;
			}

			if constexpr (port == Motorola::MC6821::Port::B) {
				machine_.m6847_->set_mode(
					value & 0x80,		// Alpha/graphics.
					value & 0x80,		// Graphics/semigraphics.
					false,				// External ROM.
					value & 0x20,		// Invert.
					(value >> 4) & 7,	// Graphics mode.
					value & 0x80		// Colour select.
				);
			}
		}

		template <Motorola::MC6821::IRQ irq>
		void set(const bool) {
			if constexpr (irq == Motorola::MC6821::IRQ::A) {
			}

			if constexpr (irq == Motorola::MC6821::IRQ::B) {
			}
		}

		template <Motorola::MC6821::Control control>
		void observe(const bool value) {
			if constexpr (control == Motorola::MC6821::Control::CA2) {
				machine_.tape_player_.set_motor_control(value);
			}

			if constexpr (control == Motorola::MC6821::Control::CB2) {
			}
		}

	private:
		ConcreteMachine &machine_;
	};
	PIA1Handler pia1_handler_;
	Motorola::MC6821::MC6821<PIA1Handler> pia1_;

	uint8_t dac_level_ = 0;

	// MARK: - SAM.

	struct SAM {
		SAM(MemoryMap &memory) : memory_(memory) {}

		uint8_t operator[](const uint16_t address) {
			// TODO: this whole thing is phoney; all I'm doing is taking the full 6847-generated
			// address and adding an offset. The real SAM does a minimal reimplementation of 6847-style
			// address counting. Do that.
			//
			// This is very much bootstrapping stuff.
			//
			// TODO: this shouldn't be able to go into ROM, possibly?
			return memory_.read(address + graphics_address_);

			// TODO: ... in the CoCo the most significant VDG data bit is hardwired to the VDG's '*alpha/semi_g' inputs.
			// This allows it to automatically switch between alphanumeric mode and semi-graphics mode based on the MS
			// data bit,which allows mixed text and block-graphics on the same screen.
			//
			// That can be achieved here; set semigraphics as per the top bit.
			//
			//	https://web.archive.org/web/20210214054301/http://www.cs.unc.edu/~yakowenk/coco/text/semigraphics.html
		}

		template <uint16_t address> void access() {
			const auto set = [&](auto &target, auto bit) {
				target = target & ~bit;
				target |= address & 1 ? bit : 0;
			};

			switch((address >> 1) & 0xf) {
				default:
					printf("Unhandled SAM access %04x\n", address);
				break;

				case 0:	set(graphics_mode_, 1);	break;
				case 1:	set(graphics_mode_, 2);	break;
				case 2:	set(graphics_mode_, 4);	break;

				case 3:	set(graphics_address_, 0x0200);	break;
				case 4:	set(graphics_address_, 0x0400);	break;
				case 5:	set(graphics_address_, 0x0800);	break;
				case 6:	set(graphics_address_, 0x1000);	break;
				case 7:	set(graphics_address_, 0x2000);	break;
				case 8:	set(graphics_address_, 0x4000);	break;
				case 9:	set(graphics_address_, 0x8000);	break;

				case 10:
					page1_ = address & 1;
					if(page1_) {
						printf("TODO: RAM page 1\n");
						// TODO: communicate to memory map.
					}
				break;

				case 11: { int tc = int(speed_); set(tc, 1); speed_ = ClockSpeed(tc); }	break;
				case 12: { int tc = int(speed_); set(tc, 2); speed_ = ClockSpeed(tc); }	break;

				// Likely these are to effect refresh; TODO: look into this.
				case 13: set(ram_size_, 1);	break;
				case 14: set(ram_size_, 2);	break;

				case 15:
					all_ram_ = address & 1;
					if(all_ram_) {
						printf("TODO: all-RAM mode\n");
						// TODO: communicate to memory map.
					}
				break;
			}
		}

		template <
			CPU::M6809::BusPhase bus_phase,
			CPU::M6809::ReadWrite read_write,
			typename AddressT
		> Cycles cycle_cost(const AddressT address) {
			switch(speed_) {
				case ClockSpeed::Full1:
				case ClockSpeed::Full2:
				return Cycles(0);

				case ClockSpeed::Half:
				return duration<Cycles>(bus_phase);

				case ClockSpeed::HalfInRAM:
					if constexpr (read_write == CPU::M6809::ReadWrite::NoData) {
						return Cycles(0);
					} else {
						if(address < 0x8000 || all_ram_) {
							return duration<Cycles>(bus_phase);
						} else {
							return Cycles(0);
						}
					}
				break;

				default: break;
			}
			__builtin_unreachable();
		}

		uint16_t graphics_address() const {
			return graphics_address_;
		}

	private:
		MemoryMap &memory_;

		enum class ClockSpeed {
			Half,
			HalfInRAM,
			Full1,
			Full2,
		};

		int graphics_mode_ = 0;
		uint16_t graphics_address_ = 0;
		ClockSpeed speed_ = ClockSpeed::Half;
		bool all_ram_ = false;
		bool page1_ = false;
		int ram_size_ = 0;
	};
	SAM sam_;

	// MARK: - Video and ScanProducer.

	JustInTimeActor<
		Motorola::MC6847::MC6847<
			Motorola::MC6847::FrameTiming::NTSC,
			SAM
		>,
		Cycles,
		4
	> m6847_;

	void set_scan_target(Outputs::Display::ScanTarget *const target) final {
		m6847_.get()->set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return m6847_.get()->get_scaled_scan_status() / 2.0;
	}

	// MARK: - MappedKeyboardMachine.

	Tandy::CoCo::Keyboard::KeyboardMapper keyboard_mapper_;
	KeyboardMapper *keyboard_mapper() final {
		return &keyboard_mapper_;
	}

	void set_key_state(const uint16_t key, const bool is_pressed) final {
		pia0_handler_.set_key_pressed(Keyboard::column(key), Keyboard::row(key), is_pressed);
	}

	void clear_all_keys() final {
		pia0_handler_.clear_all_keys();
	}

	// MARK: - MediaTarget and MediaChangeObserver.

	Storage::Tape::BinaryTapePlayer tape_player_;
	bool allow_fast_tape_loading_ = true;

	bool insert_media(const Analyser::Static::Media &media) override {
		if(!media.tapes.empty()) {
			tape_player_.set_tape(media.tapes.front(), TargetPlatform::ThomsonMO);
		}

		return !media.tapes.empty();
	}

	ChangeEffect effect_for_file_did_change(const std::string &) const override {
		return ChangeEffect::RestartMachine;
	}

	// MARK: - Configuration options.

	std::unique_ptr<Reflection::Struct> get_options() const final {
		auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
		options->quick_load = allow_fast_tape_loading_;
		return options;
	}

	void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
		const auto options = dynamic_cast<Options *>(str.get());
		allow_fast_tape_loading_ = options->quick_load;
	}

	// MARK: - Activity Source.

	void set_activity_observer(Activity::Observer *const observer) override {
		tape_player_.set_activity_observer(observer);
	}

	// MARK: - Joysticks.

	std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;

	void construct_joysticks() {
		joysticks_.emplace_back(new Joystick);
		joysticks_.emplace_back(new Joystick);
	}

	const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
		return joysticks_;
	}

	Joystick &joystick(const size_t index) {
		return *static_cast<Joystick *>(joysticks_[index].get());
	}

};

}

std::unique_ptr<Machine> Machine::create(
	const Analyser::Static::Target &target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	const auto &coco_target = static_cast<const Analyser::Static::TandyCoCo::Target &>(target);
	return std::make_unique<TandyCoCo::ConcreteMachine>(coco_target, rom_fetcher);
}

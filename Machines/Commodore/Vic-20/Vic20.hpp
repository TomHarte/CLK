//
//  Vic20.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Vic20_hpp
#define Vic20_hpp

#include "../../ConfigurationTarget.hpp"
#include "../../CRTMachine.hpp"
#include "../../Typer.hpp"

#include "../../../Processors/6502/6502.hpp"
#include "../../../Components/6560/6560.hpp"
#include "../../../Components/6522/6522.hpp"

#include "../SerialBus.hpp"
#include "../1540/C1540.hpp"

#include "../../../Storage/Tape/Tape.hpp"
#include "../../../Storage/Disk/Disk.hpp"

namespace Commodore {
namespace Vic20 {

enum ROMSlot {
	Kernel,
	BASIC,
	Characters,
	Drive
};

enum MemorySize {
	Default,
	ThreeKB,
	ThirtyTwoKB
};

enum Region {
	NTSC,
	PAL
};

#define key(line, mask) (((mask) << 3) | (line))

enum Key: uint16_t {
	Key2		= key(7, 0x01),		Key4		= key(7, 0x02),		Key6			= key(7, 0x04),		Key8		= key(7, 0x08),
	Key0		= key(7, 0x10),		KeyDash		= key(7, 0x20),		KeyHome			= key(7, 0x40),		KeyF7		= key(7, 0x80),
	KeyQ		= key(6, 0x01),		KeyE		= key(6, 0x02),		KeyT			= key(6, 0x04),		KeyU		= key(6, 0x08),
	KeyO		= key(6, 0x10),		KeyAt		= key(6, 0x20),		KeyUp			= key(6, 0x40),		KeyF5		= key(6, 0x80),
	KeyCBM		= key(5, 0x01),		KeyS		= key(5, 0x02),		KeyF			= key(5, 0x04),		KeyH		= key(5, 0x08),
	KeyK		= key(5, 0x10),		KeyColon	= key(5, 0x20),		KeyEquals		= key(5, 0x40),		KeyF3		= key(5, 0x80),
	KeySpace	= key(4, 0x01),		KeyZ		= key(4, 0x02),		KeyC			= key(4, 0x04),		KeyB		= key(4, 0x08),
	KeyM		= key(4, 0x10),		KeyFullStop	= key(4, 0x20),		KeyRShift		= key(4, 0x40),		KeyF1		= key(4, 0x80),
	KeyRunStop	= key(3, 0x01),		KeyLShift	= key(3, 0x02),		KeyX			= key(3, 0x04),		KeyV		= key(3, 0x08),
	KeyN		= key(3, 0x10),		KeyComma	= key(3, 0x20),		KeySlash		= key(3, 0x40),		KeyDown		= key(3, 0x80),
	KeyControl	= key(2, 0x01),		KeyA		= key(2, 0x02),		KeyD			= key(2, 0x04),		KeyG		= key(2, 0x08),
	KeyJ		= key(2, 0x10),		KeyL		= key(2, 0x20),		KeySemicolon	= key(2, 0x40),		KeyRight	= key(2, 0x80),
	KeyLeft		= key(1, 0x01),		KeyW		= key(1, 0x02),		KeyR			= key(1, 0x04),		KeyY		= key(1, 0x08),
	KeyI		= key(1, 0x10),		KeyP		= key(1, 0x20),		KeyAsterisk		= key(1, 0x40),		KeyReturn	= key(1, 0x80),
	Key1		= key(0, 0x01),		Key3		= key(0, 0x02),		Key5			= key(0, 0x04),		Key7		= key(0, 0x08),
	Key9		= key(0, 0x10),		KeyPlus		= key(0, 0x20),		KeyGBP			= key(0, 0x40),		KeyDelete	= key(0, 0x80),

	TerminateSequence = 0xffff,	NotMapped = 0xfffe
};

enum JoystickInput {
	Up = 0x04,
	Down = 0x08,
	Left = 0x10,
	Right = 0x80,
	Fire = 0x20
};

class UserPortVIA: public MOS::MOS6522<UserPortVIA>, public MOS::MOS6522IRQDelegate {
	public:
		UserPortVIA();
		using MOS6522IRQDelegate::set_interrupt_status;

		uint8_t get_port_input(Port port);
		void set_control_line_output(Port port, Line line, bool value);
		void set_serial_line_state(::Commodore::Serial::Line line, bool value);
		void set_joystick_state(JoystickInput input, bool value);
		void set_port_output(Port port, uint8_t value, uint8_t mask);

		void set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort);
		void set_tape(std::shared_ptr<Storage::Tape::BinaryTapePlayer> tape);

	private:
		uint8_t port_a_;
		std::weak_ptr<::Commodore::Serial::Port> serial_port_;
		std::shared_ptr<Storage::Tape::BinaryTapePlayer> tape_;
};

class KeyboardVIA: public MOS::MOS6522<KeyboardVIA>, public MOS::MOS6522IRQDelegate {
	public:
		KeyboardVIA();
		using MOS6522IRQDelegate::set_interrupt_status;

		void set_key_state(uint16_t key, bool isPressed);
		void clear_all_keys();

		// to satisfy MOS::MOS6522
		uint8_t get_port_input(Port port);

		void set_port_output(Port port, uint8_t value, uint8_t mask);
		void set_control_line_output(Port port, Line line, bool value);

		void set_joystick_state(JoystickInput input, bool value);

		void set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort);

	private:
		uint8_t port_b_;
		uint8_t columns_[8];
		uint8_t activation_mask_;
		std::weak_ptr<::Commodore::Serial::Port> serial_port_;
};

class SerialPort : public ::Commodore::Serial::Port {
	public:
		void set_input(::Commodore::Serial::Line line, ::Commodore::Serial::LineLevel level);
		void set_user_port_via(std::shared_ptr<UserPortVIA> userPortVIA);

	private:
		std::weak_ptr<UserPortVIA> user_port_via_;
};

class Vic6560: public MOS::MOS6560<Vic6560> {
	public:
		inline void perform_read(uint16_t address, uint8_t *pixel_data, uint8_t *colour_data) {
			*pixel_data = video_memory_map[address >> 10] ? video_memory_map[address >> 10][address & 0x3ff] : 0xff; // TODO
			*colour_data = colour_memory[address & 0x03ff];
		}

		uint8_t *video_memory_map[16];
		uint8_t *colour_memory;
};

class Machine:
	public CPU::MOS6502::Processor<Machine>,
	public CRTMachine::Machine,
	public MOS::MOS6522IRQDelegate::Delegate,
	public Utility::TypeRecipient,
	public Storage::Tape::BinaryTapePlayer::Delegate,
	public ConfigurationTarget::Machine {

	public:
		Machine();
		~Machine();

		void set_rom(ROMSlot slot, size_t length, const uint8_t *data);
		void configure_as_target(const StaticAnalyser::Target &target);

		void set_key_state(uint16_t key, bool isPressed) { keyboard_via_->set_key_state(key, isPressed); }
		void clear_all_keys() { keyboard_via_->clear_all_keys(); }
		void set_joystick_state(JoystickInput input, bool isPressed) {
			user_port_via_->set_joystick_state(input, isPressed);
			keyboard_via_->set_joystick_state(input, isPressed);
		}

		void set_memory_size(MemorySize size);
		void set_region(Region region);

		inline void set_use_fast_tape_hack(bool activate) { use_fast_tape_hack_ = activate; }

		// to satisfy CPU::MOS6502::Processor
		unsigned int perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value);
		void flush() { mos6560_->flush(); }

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio);
		virtual void close_output();
		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt() { return mos6560_->get_crt(); }
		virtual std::shared_ptr<Outputs::Speaker> get_speaker() { return mos6560_->get_speaker(); }
		virtual void run_for_cycles(int number_of_cycles) { CPU::MOS6502::Processor<Machine>::run_for_cycles(number_of_cycles); }

		// to satisfy MOS::MOS6522::Delegate
		virtual void mos6522_did_change_interrupt_status(void *mos6522);

		// for Utility::TypeRecipient
		uint16_t *sequence_for_character(Utility::Typer *typer, char character);

		// for Tape::Delegate
		virtual void tape_did_change_input(Storage::Tape::BinaryTapePlayer *tape);

	private:
		uint8_t character_rom_[0x1000];
		uint8_t basic_rom_[0x2000];
		uint8_t kernel_rom_[0x2000];
		uint8_t expansion_ram_[0x8000];

		uint8_t *rom_;
		uint16_t rom_address_, rom_length_;

		uint8_t user_basic_memory_[0x0400];
		uint8_t screen_memory_[0x1000];
		uint8_t colour_memory_[0x0400];
		std::vector<uint8_t> drive_rom_;

		uint8_t *processor_read_memory_map_[64];
		uint8_t *processor_write_memory_map_[64];
		void write_to_map(uint8_t **map, uint8_t *area, uint16_t address, uint16_t length);

		Region region_;

		std::unique_ptr<Vic6560> mos6560_;
		std::shared_ptr<UserPortVIA> user_port_via_;
		std::shared_ptr<KeyboardVIA> keyboard_via_;
		std::shared_ptr<SerialPort> serial_port_;
		std::shared_ptr<::Commodore::Serial::Bus> serial_bus_;

		// Tape
		std::shared_ptr<Storage::Tape::BinaryTapePlayer> tape_;
		bool use_fast_tape_hack_;
		bool is_running_at_zero_cost_;

		// Disk
		std::shared_ptr<::Commodore::C1540::Machine> c1540_;
		void install_disk_rom();
};

}
}

#endif /* Vic20_hpp */

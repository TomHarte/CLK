//
//  Cartridge.h
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_Cartridge_hpp
#define Atari2600_Cartridge_hpp

#include "../../../../Processors/6502/6502.hpp"
#include "../Bus.hpp"

namespace Atari2600 {
namespace Cartridge {

class BusExtender: public CPU::MOS6502::BusHandler {
	public:
		BusExtender(uint8_t *rom_base, std::size_t rom_size) : rom_base_(rom_base), rom_size_(rom_size) {}

		void advance_cycles(int) {}

	protected:
		uint8_t *rom_base_;
		std::size_t rom_size_;
};

template<class T> class Cartridge:
	public CPU::MOS6502::BusHandler,
	public Bus {

	public:
		Cartridge(const std::vector<uint8_t> &rom) :
			m6502_(*this),
			rom_(rom),
			bus_extender_(rom_.data(), rom.size()) {
			// The above works because bus_extender_ is declared after rom_ in the instance storage list;
			// consider doing something less fragile.
		}

		void run_for(const Cycles cycles) override	{
			// Horizontal counter resets are used as a proxy for whether this really is an Atari 2600
			// title. Random memory accesses are likely to trigger random counter resets.
			horizontal_counter_resets_ = 0;
			cycle_count_ = cycles;
			m6502_.run_for(cycles);
		}

		/*!
			Adjusts @c confidence_counter according to the results of the most recent run_for.
		*/
		void apply_confidence(Analyser::Dynamic::ConfidenceCounter &confidence_counter) override {
			if(cycle_count_.as_integral() < 200) return;
			if(horizontal_counter_resets_ > 10)
				confidence_counter.add_miss();
		}

		void set_reset_line(bool state) override	{ m6502_.set_reset_line(state);	}

		// to satisfy CPU::MOS6502::Processor
		Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			uint8_t returnValue = 0xff;
			int cycles_run_for = 3;

			// this occurs as a feedback loop: the 2600 requests ready, then performs the cycles_run_for
			// leap to the end of ready only once ready is signalled because on a 6502 ready doesn't take
			// effect until the next read; therefore it isn't safe to assume that signalling ready immediately
			// skips to the end of the line.
			if(operation == CPU::MOS6502::BusOperation::Ready)
				cycles_run_for = tia_.get_cycles_until_horizontal_blank(cycles_since_video_update_);

			cycles_since_speaker_update_ += Cycles(cycles_run_for);
			cycles_since_video_update_ += Cycles(cycles_run_for);
			cycles_since_6532_update_ += Cycles(cycles_run_for / 3);
			bus_extender_.advance_cycles(cycles_run_for / 3);

			if(isAccessOperation(operation)) {
				// give the cartridge a chance to respond to the bus access
				bus_extender_.perform_bus_operation(operation, address, value);

				// check for a RIOT RAM access
				if((address&0x1280) == 0x80) {
					if(isReadOperation(operation)) {
						returnValue &= mos6532_.get_ram(address);
					} else {
						mos6532_.set_ram(address, *value);
					}
				}

				// check for a TIA access
				if(!(address&0x1080)) {
					if(isReadOperation(operation)) {
						const uint16_t decodedAddress = address & 0xf;
						switch(decodedAddress) {
							case 0x00:		// missile 0 / player collisions
							case 0x01:		// missile 1 / player collisions
							case 0x02:		// player 0 / playfield / ball collisions
							case 0x03:		// player 1 / playfield / ball collisions
							case 0x04:		// missile 0 / playfield / ball collisions
							case 0x05:		// missile 1 / playfield / ball collisions
							case 0x06:		// ball / playfield collisions
							case 0x07:		// player / player, missile / missile collisions
								returnValue &= tia_.get_collision_flags(decodedAddress);
							break;

							case 0x08:
							case 0x09:
							case 0x0a:
							case 0x0b:
								// TODO: pot ports
								returnValue &= 0;
							break;

							case 0x0c:
							case 0x0d:
								returnValue &= tia_input_value_[decodedAddress - 0x0c];
							break;
						}
					} else {
						const uint16_t decodedAddress = address & 0x3f;
						switch(decodedAddress) {
							case 0x00:	update_video(); tia_.set_sync(*value & 0x02);		break;
							case 0x01:	update_video();	tia_.set_blank(*value & 0x02);		break;

							case 0x02:	m6502_.set_ready_line(true);						break;
							case 0x03:
								update_video();
								tia_.reset_horizontal_counter();
								horizontal_counter_resets_++;
							break;
								// TODO: audio will now be out of synchronisation. Fix.

							case 0x04:
							case 0x05:	update_video();	tia_.set_player_number_and_size(decodedAddress - 0x04, *value);	break;
							case 0x06:
							case 0x07:	update_video();	tia_.set_player_missile_colour(decodedAddress - 0x06, *value);		break;
							case 0x08:	update_video();	tia_.set_playfield_ball_colour(*value);								break;
							case 0x09:	update_video();	tia_.set_background_colour(*value);									break;
							case 0x0a:	update_video();	tia_.set_playfield_control_and_ball_size(*value);					break;
							case 0x0b:
							case 0x0c:	update_video();	tia_.set_player_reflected(decodedAddress - 0x0b, !((*value)&8));	break;
							case 0x0d:
							case 0x0e:
							case 0x0f:	update_video();	tia_.set_playfield(decodedAddress - 0x0d, *value);					break;
							case 0x10:
							case 0x11:	update_video(); tia_.set_player_position(decodedAddress - 0x10);					break;
							case 0x12:
							case 0x13:	update_video(); tia_.set_missile_position(decodedAddress - 0x12);					break;
							case 0x14:	update_video();	tia_.set_ball_position();											break;
							case 0x1b:
							case 0x1c:	update_video(); tia_.set_player_graphic(decodedAddress - 0x1b, *value);				break;
							case 0x1d:
							case 0x1e:	update_video(); tia_.set_missile_enable(decodedAddress - 0x1d, (*value)&2);			break;
							case 0x1f:	update_video(); tia_.set_ball_enable((*value)&2);									break;
							case 0x20:
							case 0x21:	update_video(); tia_.set_player_motion(decodedAddress - 0x20, *value);				break;
							case 0x22:
							case 0x23:	update_video(); tia_.set_missile_motion(decodedAddress - 0x22, *value);				break;
							case 0x24:	update_video(); tia_.set_ball_motion(*value);										break;
							case 0x25:
							case 0x26:	tia_.set_player_delay(decodedAddress - 0x25, (*value)&1);							break;
							case 0x27:	tia_.set_ball_delay((*value)&1);													break;
							case 0x28:
							case 0x29:	update_video(); tia_.set_missile_position_to_player(decodedAddress - 0x28, (*value)&2);		break;
							case 0x2a:	update_video(); tia_.move();														break;
							case 0x2b:	update_video(); tia_.clear_motion();												break;
							case 0x2c:	update_video(); tia_.clear_collision_flags();										break;

							case 0x15:
							case 0x16:	update_audio(); tia_sound_.set_control(decodedAddress - 0x15, *value);				break;
							case 0x17:
							case 0x18:	update_audio(); tia_sound_.set_divider(decodedAddress - 0x17, *value);				break;
							case 0x19:
							case 0x1a:	update_audio(); tia_sound_.set_volume(decodedAddress - 0x19, *value);				break;
						}
					}
				}

				// check for a PIA access
				if((address&0x1280) == 0x280) {
					update_6532();
					if(isReadOperation(operation)) {
						returnValue &= mos6532_.read(address);
					} else {
						mos6532_.write(address, *value);
					}
				}

				if(isReadOperation(operation)) {
					*value &= returnValue;
				}
			}

			if(!tia_.get_cycles_until_horizontal_blank(cycles_since_video_update_)) m6502_.set_ready_line(false);

			return Cycles(cycles_run_for / 3);
		}

		void flush() override {
			update_audio();
			update_video();
			audio_queue_.perform();
		}

	protected:
		CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, Cartridge<T>, true> m6502_;
		std::vector<uint8_t> rom_;

	private:
		T bus_extender_;
		int horizontal_counter_resets_ = 0;
		Cycles cycle_count_;

};

}
}

#endif /* Atari2600_Cartridge_hpp */

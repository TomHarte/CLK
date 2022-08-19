//
//  Blitter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Blitter_hpp
#define Blitter_hpp

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "BlitterSequencer.hpp"
#include "DMADevice.hpp"

namespace Amiga {

/*!
	If @c record_bus is @c true then all bus interactions will be recorded
	and can subsequently be retrieved. This is included for testing purposes.
*/
template <bool record_bus = false> class Blitter: public DMADevice<4, 4> {
	public:
		using DMADevice::DMADevice;

		template <int id, int shift> void set_pointer(uint16_t value) {
			if(get_status() & 0x4000) {
				printf(">>>");
			}
			DMADevice<4, 4>::set_pointer<id, shift>(value);
		}

		// Various setters; it's assumed that address decoding is handled externally.
		//
		// In all cases where a channel is identified numerically, it's taken that
		// 0 = A, 1 = B, 2 = C, 3 = D.
		void set_control(int index, uint16_t value);
		void set_first_word_mask(uint16_t value);
		void set_last_word_mask(uint16_t value);

		void set_size(uint16_t value);
		void set_minterms(uint16_t value);
//		void set_vertical_size(uint16_t value);
//		void set_horizontal_size(uint16_t value);
		void set_data(int channel, uint16_t value);

		uint16_t get_status();

		template <bool complete_immediately> bool advance_dma();

		struct Transaction {
			enum class Type {
				SkippedSlot,
				ReadA,
				ReadB,
				ReadC,
				AddToPipeline,
				WriteFromPipeline
			} type = Type::SkippedSlot;

			uint32_t address = 0;
			uint16_t value = 0;

			Transaction() {}
			Transaction(Type type) : type(type) {}
			Transaction(Type type, uint32_t address, uint16_t value) : type(type), address(address), value(value) {}

			std::string to_string() const {
				std::string result;

				switch(type) {
					case Type::SkippedSlot:			result = "SkippedSlot";			break;
					case Type::ReadA:				result = "ReadA";				break;
					case Type::ReadB:				result = "ReadB";				break;
					case Type::ReadC:				result = "ReadC";				break;
					case Type::AddToPipeline:		result = "AddToPipeline";		break;
					case Type::WriteFromPipeline:	result = "WriteFromPipeline";	break;
				}

				result += " address:" + std::to_string(address) + " value:" + std::to_string(value);
				return result;
			}
		};
		std::vector<Transaction> get_and_reset_transactions();

	private:
		int width_ = 0, height_ = 0;
		int shifts_[2]{};
		uint16_t a_mask_[2] = {0xffff, 0xffff};

		bool line_mode_ = false;
		bool one_dot_ = false;
		int line_direction_ = 0;
		int line_sign_ = 1;

		uint32_t direction_ = 1;
		bool inclusive_fill_ = false;
		bool exclusive_fill_ = false;
		bool fill_carry_ = false;

		uint8_t minterms_ = 0;
		uint32_t a32_ = 0, b32_ = 0;
		uint16_t a_data_ = 0, b_data_ = 0, c_data_ = 0;

		bool not_zero_flag_ = false;

		BlitterSequencer sequencer_;
		uint32_t write_address_ = 0xffff'ffff;
		uint16_t write_value_ = 0;
		enum WritePhase {
			Starting, Full
		} write_phase_;
		int y_, x_;
		uint16_t transient_a_mask_;
		bool busy_ = false;
		int loop_index_ = -1;

		int error_ = 0;
		bool draw_ = false;
		bool has_c_data_ = false;

		void add_modulos();
		std::vector<Transaction> transactions_;
};

}


#endif /* Blitter_hpp */

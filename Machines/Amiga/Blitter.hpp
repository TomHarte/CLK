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
#include <vector>

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "DMADevice.hpp"

namespace Amiga {

/*!
	Statefully provides the next access the Blitter should make.

	TODO: determine the actual logic here, rather than
	relying on tables.
*/
class BlitterSequencer {
	public:
		enum class Channel {
			/// Tells the caller to calculate and load a new piece of output
			/// into the output pipeline.
			///
			/// If any inputs are enabled then a one-slot output pipeline applies:
			/// output will rest in the pipeline for one write phase before being written.
			Write,
			/// Indicates that a write should occur if anything is in the pipeline, otherwise
			/// no activity should occur.
			FlushPipeline,
			/// The caller should read from channel C.
			C,
			/// The caller should read from channel B.
			B,
			/// The caller should read from channel A.
			A,
			/// Indicates an unused DMA slot.
			None
		};

		/// Sets the current control value, which indicates which
		/// channels are enabled.
		void set_control(int control) {
			control_ = control & 0xf;
			index_ = 0;	// TODO: this probably isn't accurate; case caught is a change
						// of control values during a blit.
		}

		/// Indicates that blitting should conclude after this step, i.e.
		/// whatever is being fetched now is part of the final set of input data;
		/// this is safe to call following a fetch request on any channel.
		void complete() {
			next_phase_ =
				(control_ == 0x9 || control_ == 0xb || control_ == 0xd) ?
					Phase::PauseAndComplete : Phase::Complete;
		}

		/// Begins a blit operation.
		void begin() {
			phase_ = next_phase_ = Phase::Ongoing;
			index_ = loop_ = 0;
		}

		/// Provides the next channel to fetch from, or that a write is required,
		/// along with a count of complete channel iterations so far completed.
		std::pair<Channel, int> next() {
			switch(phase_) {
				default: break;

				case Phase::Complete:
				return std::make_pair(Channel::FlushPipeline, loop_);

				case Phase::PauseAndComplete:
					phase_ = Phase::Complete;
				return std::make_pair(Channel::None, loop_);
			}

			Channel next = Channel::None;

			switch(control_) {
				default: break;

				case 0: next = next_channel(pattern0); break;
				case 1: next = next_channel(pattern1); break;
				case 2: next = next_channel(pattern2); break;
				case 3: next = next_channel(pattern3); break;
				case 4: next = next_channel(pattern4); break;
				case 5: next = next_channel(pattern5); break;
				case 6: next = next_channel(pattern6); break;
				case 7: next = next_channel(pattern7); break;
				case 8: next = next_channel(pattern8); break;
				case 9: next = next_channel(pattern9); break;
				case 10: next = next_channel(patternA); break;
				case 11: next = next_channel(patternB); break;
				case 12: next = next_channel(patternC); break;
				case 13: next = next_channel(patternD); break;
				case 14: next = next_channel(patternE); break;
				case 15: next = next_channel(patternF); break;
			}

			return std::make_pair(next, loop_);
		}

		template <int channel> bool channel_enabled() {
			return control_ & (8 >> channel);
		}

	private:
		static constexpr std::array<Channel, 1> pattern0 = { Channel::None };
		static constexpr std::array<Channel, 2> pattern1 = { Channel::Write, Channel::None };
		static constexpr std::array<Channel, 2> pattern2 = { Channel::C, Channel::None };
		static constexpr std::array<Channel, 3> pattern3 = { Channel::C, Channel::Write, Channel::None };
		static constexpr std::array<Channel, 3> pattern4 = { Channel::B, Channel::None, Channel::None };
		static constexpr std::array<Channel, 3> pattern5 = { Channel::B, Channel::Write, Channel::None };
		static constexpr std::array<Channel, 3> pattern6 = { Channel::B, Channel::C, Channel::None };
		static constexpr std::array<Channel, 4> pattern7 = { Channel::B, Channel::C, Channel::Write, Channel::None };
		static constexpr std::array<Channel, 2> pattern8 = { Channel::A, Channel::None };
		static constexpr std::array<Channel, 2> pattern9 = { Channel::A, Channel::Write };
		static constexpr std::array<Channel, 2> patternA = { Channel::A, Channel::C };
		static constexpr std::array<Channel, 3> patternB = { Channel::A, Channel::C, Channel::Write };
		static constexpr std::array<Channel, 3> patternC = { Channel::A, Channel::B, Channel::None };
		static constexpr std::array<Channel, 3> patternD = { Channel::A, Channel::B, Channel::Write };
		static constexpr std::array<Channel, 3> patternE = { Channel::A, Channel::B, Channel::C };
		static constexpr std::array<Channel, 4> patternF = { Channel::A, Channel::B, Channel::C, Channel::Write };
		template <typename ArrayT> Channel next_channel(const ArrayT &list) {
			loop_ += index_ / list.size();
			index_ %= list.size();
			const Channel result = list[index_];
			++index_;
			if(index_ == list.size()) {
				phase_ = next_phase_;
			}
			return result;
		}

		// Current control flags, i.e. which channels are enabled.
		int control_ = 0;

		// Index into the pattern table for this blit.
		size_t index_ = 0;

		// Number of times the entire pattern table has been completed.
		int loop_ = 0;

		enum class Phase {
			/// Return the next thing in the pattern table and advance.
			/// If looping from the end of the pattern table to the start,
			/// set phase_ to next_phase_.
			Ongoing,
			/// Return a Channel::None and advancce to phase_ = Phase::Complete.
			PauseAndComplete,
			/// Return Channel::Write indefinitely.
			Complete
		};

		// Current sequencer pahse.
		Phase phase_ = Phase::Complete;
		// Phase to assume at the end of this iteration of the sequence table.
		Phase next_phase_ = Phase::Complete;
};

/*!
	If @c record_bus is @c true then all bus interactions will be recorded
	and can subsequently be retrieved. This is included for testing purposes.
*/
template <bool record_bus = false> class Blitter: public DMADevice<4, 4> {
	public:
		using DMADevice::DMADevice;

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

		bool advance_dma();

		struct Transaction {
			enum class Type {
				SkippedSlot,
				ReadA,
				ReadB,
				ReadC,
				AddToPipeline,
				WriteFromPipeline
			} type;

			uint32_t address = 0;
			uint16_t value = 0;

			Transaction(Type type) : type(type) {}
			Transaction(Type type, uint32_t address, uint16_t value) : type(type), address(address), value(value) {}
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

		void add_modulos();
		std::vector<Transaction> transactions_;
};

}


#endif /* Blitter_hpp */

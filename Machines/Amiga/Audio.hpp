//
//  Audio.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include <atomic>
#include <cstdint>

#include "DMADevice.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

namespace Amiga {

class Audio: public DMADevice<4> {
	public:
		Audio(Chipset &chipset, uint16_t *ram, size_t word_size, float output_rate);

		/// Idiomatic call-in for DMA scheduling; indicates that this class may
		/// perform a DMA access for the stated channel now.
		bool advance_dma(int channel);

		/// Advances output by one DMA window, which is implicitly two cycles
		/// at the output rate that was specified to the constructor.
		void output();

		/// Sets the total number of words to fetch for the given channel.
		void set_length(int channel, uint16_t);

		/// Sets the number of DMA windows between each 8-bit output,
		/// in the same time base as @c ticks_per_line.
		void set_period(int channel, uint16_t);

		/// Sets the output volume for the given channel; if bit 6 is set
		/// then output is maximal; otherwise bits 0–5 select
		/// a volume of [0–63]/64, on a logarithmic scale.
		void set_volume(int channel, uint16_t);

		/// Sets the next two samples of audio to output.
		template <bool is_external = true> void set_data(int channel, uint16_t);

		/// Provides a copy of the DMA enable flags, for the purpose of
		/// determining which channels are enabled for DMA.
		void set_channel_enables(uint16_t);

		/// Sets which channels, if any, modulate period or volume of
		/// their neighbours.
		void set_modulation_flags(uint16_t);

		/// Sets which interrupt requests are currently active.
		void set_interrupt_requests(uint16_t);

		/// Obtains the output source.
		Outputs::Speaker::Speaker *get_speaker() {
			return &speaker_;
		}

	private:
		struct Channel {
			// The data latch plus a count of unused samples
			// in the latch, which will always be 0, 1 or 2.
			uint16_t data = 0x0000;
			bool wants_data = false;
			uint16_t data_latch = 0x0000;

			// The DMA address; unlike most of the Amiga Chipset,
			// the user posts a value to feed a pointer, rather
			// than having access to the pointer itself.
			bool should_reload_address = false;
			uint32_t data_address = 0x0000'0000;

			// Number of words remaining in DMA data.
			uint16_t length = 0;
			uint16_t length_counter = 0;

			// Number of ticks between each sample, plus the
			// current counter, which counts downward.
			uint16_t period = 0;
			uint16_t period_counter = 0;

			// Modulation / attach flags.
			bool attach_period = false;
			bool attach_volume = false;

			// Output volume, [0, 64].
			uint8_t volume = 0;
			uint8_t volume_latch = 0;

			// Indicates whether DMA is enabled for this channel.
			bool dma_enabled = false;

			// Records whether this audio interrupt is pending.
			bool interrupt_pending = false;
			bool will_request_interrupt = false;

			// Replicates the Hardware Reference Manual state machine;
			// comments indicate which of the documented states each
			// label refers to.
			enum class State {
				Disabled,			// 000
				WaitingForDummyDMA,	// 001
				WaitingForDMA,		// 101
				PlayingHigh,		// 010
				PlayingLow,			// 011
			} state = State::Disabled;

			/// Dispatches to the appropriate templatised output for the current state.
			/// @param moduland The channel to modulate, if modulation is enabled.
			/// @returns @c true if an interrupt should be posted; @c false otherwise.
			bool output(Channel *moduland);

			/// Applies dynamic logic for @c state, mostly testing for potential state transitions.
			/// @param moduland The channel to modulate, if modulation is enabled.
			/// @returns @c true if an interrupt should be posted; @c false otherwise.
			template <State state> bool output(Channel *moduland);

			/// Transitions from @c begin to @c end, calling the appropriate @c begin_state
			/// and taking any steps specific to that particular transition.
			/// @param moduland The channel to modulate, if modulation is enabled.
			/// @returns @c true if an interrupt should be posted; @c false otherwise.
			template <State begin, State end> bool transit(Channel *moduland);

			/// Begins @c state, performing all fixed logic that would otherwise have to be
			/// repeated endlessly in the relevant @c output.
			/// @param moduland The channel to modulate, if modulation is enabled.
			template <State state> void begin_state(Channel *moduland);

			/// Provides the common length-decrementing logic used when transitioning
			/// between PlayingHigh and PlayingLow in either direction.
			void decrement_length();

			// Output state.
			int8_t output_level = 0;
			uint8_t output_phase = 0;
			bool output_enabled = false;

			void reset_output_phase() {
				output_phase = 0;
				output_enabled = (volume_latch > 0) && !attach_period && !attach_volume;
			}
		} channels_[4];

		// Transient output state, and its destination.
		Outputs::Speaker::PushLowpass<true> speaker_;
		Concurrency::AsyncTaskQueue<true> queue_;

		using AudioBuffer = std::array<int16_t, 4096>;
		static constexpr int BufferCount = 3;
		AudioBuffer buffer_[BufferCount];
		std::atomic<bool> buffer_available_[BufferCount];
		size_t buffer_pointer_ = 0, sample_pointer_ = 0;
};

}

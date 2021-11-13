//
//  Audio.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Audio_hpp
#define Audio_hpp

#include "DMADevice.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"

namespace Amiga {

class Audio: public DMADevice<4> {
	public:
		Audio(
			Chipset &chipset, uint16_t *ram, size_t word_size,
			[[maybe_unused]] double output_rate) :
			DMADevice<4>(chipset, ram, word_size) {}

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
		void set_data(int channel, uint16_t);

		/// Provides a copy of the DMA enable flags, for the purpose of
		/// determining which channels are enabled for DMA.
		void set_channel_enables(uint16_t);

		/// Sets which channels, if any, modulate period or volume of
		/// their neighbours.
		void set_modulation_flags(uint16_t);

		/// Sets which interrupt requests are currently active.
		void set_interrupt_requests(uint16_t);

	private:
		struct Channel {
			// The data latch plus a count of unused samples
			// in the latch, which will always be 0, 1 or 2.
			uint16_t data = 0x0000;
			bool has_data = false;

			// Number of words remaining in DMA data.
			uint16_t length = 0;

			// Number of ticks between each sample, plus the
			// current counter, which counts downward.
			uint16_t period = 0;
			uint16_t period_counter = 0;

			// Output volume, [0, 64].
			uint8_t volume;

			// Indicates whether DMA is enabled for this channel.
			bool dma_enabled = false;

			// Records whether this audio interrupt is pending.
			bool interrupt_pending = false;

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
		} channels_[4];
};

}

#endif /* Audio_hpp */

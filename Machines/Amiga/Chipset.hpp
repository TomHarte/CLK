//
//  Chipset.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Chipset_hpp
#define Chipset_hpp

#include <cstddef>
#include <cstdint>

#include "../../Processors/68000/68000.hpp"
#include "../../Outputs/CRT/CRT.hpp"

#include "DMADevice.hpp"
#include "Blitter.hpp"

namespace Amiga {

enum class InterruptFlag: uint16_t {
	SerialPortTransmit		= 1 << 0,
	DiskBlock				= 1 << 1,
	Software				= 1 << 2,
	IOPortsAndTimers		= 1 << 3,	// i.e. CIA A.
	Copper					= 1 << 4,
	VerticalBlank			= 1 << 5,
	Blitter					= 1 << 6,
	AudioChannel0			= 1 << 7,
	AudioChannel1			= 1 << 8,
	AudioChannel2			= 1 << 9,
	AudioChannel3			= 1 << 10,
	SerialPortReceive		= 1 << 11,
	DiskSyncMatch			= 1 << 12,
	External				= 1 << 13,	// i.e. CIA B.
};

enum class DMAFlag: uint16_t {
	AudioChannel0			= 1 << 0,
	AudioChannel1			= 1 << 1,
	AudioChannel2			= 1 << 2,
	AudioChannel3			= 1 << 3,
	Disk					= 1 << 4,
	Sprites					= 1 << 5,
	Blitter					= 1 << 6,
	Copper					= 1 << 7,
	Bitplane				= 1 << 8,
	AllBelow				= 1 << 9,
	BlitterPriority			= 1 << 10,
	BlitterZero				= 1 << 13,
	BlitterBusy				= 1 << 14,
};

class Chipset {
	public:
		Chipset(uint16_t *ram, size_t word_size);

		struct Changes {
			int hsyncs = 0;
			int vsyncs = 0;
			int interrupt_level = 0;
			HalfCycles duration;

			Changes &operator += (const Changes &rhs) {
				hsyncs += rhs.hsyncs;
				vsyncs += rhs.vsyncs;
				duration += rhs.duration;
				return *this;
			}
		};

		/// Advances the stated amount of time.
		Changes run_for(HalfCycles);

		/// Advances to the next available CPU slot.
		Changes run_until_cpu_slot();

		/// Performs the provided microcycle, which the caller guarantees to be a memory access.
		void perform(const CPU::MC68000::Microcycle &);

		/// Sets the current state of the CIA interrupt lines.
		void set_cia_interrupts(bool cia_a, bool cia_b);

		/// Provides the chipset's current interrupt level.
		int get_interrupt_level() {
			return interrupt_level_;
		}

		// The standard CRT set.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);
		Outputs::Display::ScanStatus get_scaled_scan_status() const;
		void set_display_type(Outputs::Display::DisplayType);
		Outputs::Display::DisplayType get_display_type() const;

	private:
		friend class DMADeviceBase;

		// MARK: - Interrupts.

		uint16_t interrupt_enable_ = 0;
		uint16_t interrupt_requests_ = 0;
		int interrupt_level_ = 0;

		void update_interrupts();
		void posit_interrupt(InterruptFlag);

		// MARK: - Scheduler.

		template <bool stop_on_cpu> Changes run(HalfCycles duration = HalfCycles::max());
		template <bool stop_on_cpu> int advance_slots(int, int);
		template <int cycle, bool stop_if_cpu> bool perform_cycle();
		template <int cycle> void output();

		// MARK: - DMA Control, Scheduler and Blitter.

		uint16_t dma_control_ = 0;
		Blitter blitter_;

		// MARK: - Sprites.

		struct Sprite {
			void set_pointer(int shift, uint16_t value);
			void set_start_position(uint16_t value);
			void set_stop_and_control(uint16_t value);
			void set_image_data(int slot, uint16_t value);
		} sprites_[8];

		// MARK: - Raster position and state.

		// Definitions related to PAL/NTSC.
		int line_length_ = 227;
		int frame_height_ = 312;
		int vertical_blank_height_ = 29;

		// Current raster position.
		int line_cycle_ = 0, y_ = 0;

		// Parameters affecting bitplane collection and output.
		uint16_t display_window_start_[2] = {0, 0};
		uint16_t display_window_stop_[2] = {0, 0};
		uint16_t fetch_window_[2] = {0, 0};

		// Ephemeral bitplane collection state.
		bool fetch_vertical_ = false, fetch_horizontal_ = false;
		bool display_horizontal_ = false;
		bool did_fetch_ = false;

		// Output state.
		uint16_t border_colour_ = 0;
		bool is_border_ = true;
		int zone_duration_ = 0;
		uint16_t *pixels_ = nullptr;
		void flush_output();

		struct BitplaneData: public std::array<uint16_t, 6> {
			BitplaneData &operator >>= (int c) {
				(*this)[0] >>= c;
				(*this)[1] >>= c;
				(*this)[2] >>= c;
				(*this)[3] >>= c;
				(*this)[4] >>= c;
				(*this)[5] >>= c;
				return *this;
			}
		};

		class Bitplanes: public DMADevice<6> {
			public:
				using DMADevice::DMADevice;

				template <bool is_odd> bool advance();
				void do_end_of_line();
				void set_control(uint16_t);

			private:
				bool is_high_res_ = false;
				int collection_offset_ = 0;
				int plane_count_ = 0;

				BitplaneData next;
		} bitplanes_;

		void post_bitplanes(const BitplaneData &data);

		BitplaneData current_bitplanes_, next_bitplanes_;
//		std::array<uint8_t, 912> even_playfield_;
//		std::array<uint8_t, 912> odd_playfield_;
		int odd_delay_ = 0, even_delay_ = 0;

		// MARK: - Copper.

		class Copper: public DMADevice<2> {
			public:
				using DMADevice<2>::DMADevice;

				/// Offers a DMA slot to the Copper, specifying the current beam position.
				///
				/// @returns @c true if the slot was used; @c false otherwise.
				bool advance(uint16_t position);

				/// Forces a reload of address @c id (i.e. 0 or 1) and restarts the Copper.
				template <int id> void reload() {
					address_ = pointer_[id];
					state_ = State::FetchFirstWord;
				}

				/// Sets the Copper control word.
				void set_control(uint16_t c) {
					control_ = c;
				}

				/// Forces the Copper into the stopped state.
				void stop() {
					state_ = State::Stopped;
				}

			private:
				uint32_t address_ = 0;
				uint16_t control_ = 0;

				enum class State {
					FetchFirstWord, FetchSecondWord, Waiting, Stopped,
				} state_ = State::Stopped;
				bool skip_next_ = false;
				uint16_t instruction_[2]{};
				uint16_t position_mask_ = 0xffff;
		} copper_;

		// MARK: - Serial port.

		class SerialPort {
			public:
				void set_control(uint16_t) {}

			private:
				uint16_t value = 0, reload = 0;
				uint16_t shift = 0, receive_shift = 0;
				uint16_t status;
		} serial_;

		// MARK: - Disk drives.

		class DiskDMA: public DMADevice<1> {
			public:
				using DMADevice::DMADevice;

				void set_length(uint16_t value) {
					dma_enable_ = value & 0x8000;
					write_ = value & 0x4000;
					length_ = value & 0x3fff;

					if(dma_enable_) {
						printf("Not yet implemented: disk DMA [%s of %d to %06x]\n", write_ ? "write" : "read", length_, pointer_[0]);
					}
				}

				bool advance();

			private:
				uint16_t length_;
				bool dma_enable_ = false;
				bool write_ = false;
		} disk_;

		// MARK: - Pixel output.

		Outputs::CRT::CRT crt_;
		uint16_t palette_[32]{};
		uint16_t swizzled_palette_[32]{};
};

}

#endif /* Chipset_hpp */

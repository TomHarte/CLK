//
//  Chipset.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Chipset_hpp
#define Chipset_hpp

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "../../Activity/Source.hpp"
#include "../../ClockReceiver/ClockingHintSource.hpp"
#include "../../Components/6526/6526.hpp"
#include "../../Outputs/CRT/CRT.hpp"
#include "../../Processors/68000/68000.hpp"
#include "../../Storage/Disk/Controller/DiskController.hpp"
#include "../../Storage/Disk/Drive.hpp"

#include "Blitter.hpp"
#include "Copper.hpp"
#include "DMADevice.hpp"
#include "Flags.hpp"
#include "MemoryMap.hpp"

namespace Amiga {

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

class Chipset: private ClockingHint::Observer {
	public:
		Chipset(MemoryMap &memory_map, int input_clock_rate);

		struct Changes {
			int interrupt_level = 0;
			HalfCycles duration;

			Changes &operator += (const Changes &rhs) {
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

		/// Inserts the disks provided.
		/// @returns @c true if anything was inserted; @c false otherwise.
		bool insert(const std::vector<std::shared_ptr<Storage::Disk::Disk>> &disks);

		// The standard CRT set.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);
		Outputs::Display::ScanStatus get_scaled_scan_status() const;
		void set_display_type(Outputs::Display::DisplayType);
		Outputs::Display::DisplayType get_display_type() const;

		// Activity observation.
		void set_activity_observer(Activity::Observer *observer) {
			cia_a_handler_.set_activity_observer(observer);
			disk_controller_.set_activity_observer(observer);
		}

	private:
		friend class DMADeviceBase;

		// MARK: - E Clock follow along.

		HalfCycles cia_divider_;

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

		class Sprite: public DMADevice<1> {
			public:
				using DMADevice::DMADevice;

				void set_start_position(uint16_t value);
				void set_stop_and_control(uint16_t value);
				void set_image_data(int slot, uint16_t value);

				bool advance(int y);
				void reset_dma();

			private:
				uint16_t v_start_ = 0, h_start_ = 0, v_stop_ = 0;
				uint16_t data_[2]{};
				bool attached_ = false;
				bool active_ = false;

				enum class DMAState {
					FetchStart,
					FetchStopAndControl,
					WaitingForStart,
					FetchData1,
					FetchData0,
					Stopped
				} dma_state_ = DMAState::FetchStart;
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
		bool horizontal_is_last_ = false;
		bool display_horizontal_ = false;
		bool did_fetch_ = false;

		// Output state.
		uint16_t border_colour_ = 0;
		bool is_border_ = true;
		int zone_duration_ = 0;
		uint16_t *pixels_ = nullptr;
		void flush_output();

		struct BitplaneData: public std::array<uint16_t, 6> {
			BitplaneData &operator <<= (int c) {
				(*this)[0] <<= c;
				(*this)[1] <<= c;
				(*this)[2] <<= c;
				(*this)[3] <<= c;
				(*this)[4] <<= c;
				(*this)[5] <<= c;
				return *this;
			}

			void clear() {
				std::fill(begin(), end(), 0);
			}
		};

		class Bitplanes: public DMADevice<6> {
			public:
				using DMADevice::DMADevice;

				bool advance(int cycle);
				void do_end_of_line();
				void set_control(uint16_t);

			private:
				bool is_high_res_ = false;
				int plane_count_ = 0;

				BitplaneData next;
		} bitplanes_;

		void post_bitplanes(const BitplaneData &data);
		BitplaneData previous_bitplanes_;

		struct SixteenPixels: public std::array<uint64_t, 2> {
			void set(
				const BitplaneData &previous,
				const BitplaneData &next,
				int odd_delay,
				int even_delay);

			SixteenPixels &operator <<= (int c) {
				(*this)[1] = ((*this)[1] << c) | ((*this)[0] >> (64 - c));
				(*this)[0] <<= c;
				return *this;
			}

			int operator >> (int c) {
				assert(c >= 96);
				return int((*this)[1] >> (c - 64));
			}
		} bitplane_pixels_;

		int odd_delay_ = 0, even_delay_ = 0;
		bool is_high_res_ = false;

		// MARK: - Copper.

		Copper copper_;

		// MARK: - Serial port.

		class SerialPort {
			public:
				void set_control(uint16_t) {}

			private:
				uint16_t value = 0, reload = 0;
				uint16_t shift = 0, receive_shift = 0;
				uint16_t status;
		} serial_;

		// MARK: - Pixel output.

		Outputs::CRT::CRT crt_;
		uint16_t palette_[32]{};
		uint16_t swizzled_palette_[32]{};

		// MARK: - CIAs
	private:
		class DiskController;

		class CIAAHandler: public MOS::MOS6526::PortHandler {
			public:
				CIAAHandler(MemoryMap &map, DiskController &controller);
				void set_port_output(MOS::MOS6526::Port port, uint8_t value);
				uint8_t get_port_input(MOS::MOS6526::Port port);
				void set_activity_observer(Activity::Observer *observer);

			private:
				MemoryMap &map_;
				DiskController &controller_;
				Activity::Observer *observer_ = nullptr;
				inline static const std::string led_name = "Power";
		} cia_a_handler_;

		class CIABHandler: public MOS::MOS6526::PortHandler {
			public:
				CIABHandler(DiskController &controller);
				void set_port_output(MOS::MOS6526::Port port, uint8_t value);
				uint8_t get_port_input(MOS::MOS6526::Port);

			private:
				DiskController &controller_;
		} cia_b_handler_;

	public:
		using CIAA = MOS::MOS6526::MOS6526<CIAAHandler, MOS::MOS6526::Personality::P8250>;
		using CIAB = MOS::MOS6526::MOS6526<CIABHandler, MOS::MOS6526::Personality::P8250>;

		// CIAs are provided for direct access; it's up to the caller properly
		// to distinguish relevant accesses.
		CIAA cia_a;
		CIAB cia_b;

	private:
		// MARK: - Disk drives.

		class DiskDMA: public DMADevice<1> {
			public:
				using DMADevice::DMADevice;

				void set_length(uint16_t value);
				bool advance();

				void enqueue(uint16_t value, bool matches_sync);

			private:
				uint16_t length_;
				bool dma_enable_ = false;
				bool write_ = false;
				uint16_t last_set_length_ = 0;

				std::array<uint16_t, 4> buffer_;
				size_t buffer_read_ = 0, buffer_write_ = 0;
		} disk_;

		class DiskController: public Storage::Disk::Controller {
			public:
				DiskController(Cycles clock_rate, Chipset &chipset, DiskDMA &disk_dma, CIAB &cia);

				void set_mtr_sel_side_dir_step(uint8_t);
				uint8_t get_rdy_trk0_wpro_chng();

				void run_for(Cycles duration) {
					Storage::Disk::Controller::run_for(duration);
				}

				bool insert(const std::shared_ptr<Storage::Disk::Disk> &disk, size_t drive);
				void set_activity_observer(Activity::Observer *);

				void set_sync_word(uint16_t);
				void set_control(uint16_t);

			private:
				void process_input_bit(int value) final;
				void process_index_hole() final;

				// Implement the Amiga's drive ID shift registers
				// directly in the controller for now.
				uint32_t drive_ids_[4]{};
				uint32_t previous_select_ = 0;

				uint16_t data_ = 0;
				int bit_count_ = 0;
				uint16_t sync_word_ = 0x4489;	// TODO: confirm or deny guess.
				bool sync_with_word_ = false;

				Chipset &chipset_;
				DiskDMA &disk_dma_;
				CIAB &cia_;

		} disk_controller_;
		friend DiskController;

		void set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) final;
		bool disk_controller_is_sleeping_ = false;
		uint16_t paula_disk_control_ = 0;
};

}

#endif /* Chipset_hpp */

//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Atari_ST_Video_hpp
#define Atari_ST_Video_hpp

#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../ClockReceiver/DeferredQueue.hpp"

#include <vector>

// Testing hook; not for any other user.
class VideoTester;

namespace Atari {
namespace ST {

struct LineLength {
	int length = 1024;
	int hsync_start = 1024;
	int hsync_end = 1024;
};

/*!
	Models a combination of the parts of the GLUE, MMU and Shifter that in net
	form the video subsystem of the Atari ST. So not accurate to a real chip, but
	(hopefully) to a subsystem.
*/
class Video {
	public:
		Video();

		/*!
			Sets the memory pool that provides video, and its size.
		*/
		void set_ram(uint16_t *, size_t size);

		/*!
			Sets the target device for video data.
		*/
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);

		/// Gets the current scan status.
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

		/*!
			Sets the type of output.
		*/
		void set_display_type(Outputs::Display::DisplayType);

		/*!
			Produces the next @c duration period of pixels.
		*/
		void run_for(HalfCycles duration);


		/*!
			@returns the number of cycles until there is next a change in the hsync,
			vsync or display_enable outputs.
		*/
		HalfCycles get_next_sequence_point();

		/*!
			@returns @c true if the horizontal sync output is currently active; @c false otherwise.

			@discussion On an Atari ST, this generates a VPA-style interrupt, which is often erroneously
			documented as being triggered by horizontal blank.
		*/
		bool hsync();

		/*!
			@returns @c true if the vertical sync output is currently active; @c false otherwise.

			@discussion On an Atari ST, this generates a VPA-style interrupt, which is often erroneously
			documented as being triggered by vertical blank.
		*/
		bool vsync();

		/*!
			@returns @c true if the display enabled output is currently active; @c false otherwise.

			@discussion On an Atari ST this is fed to the MFP. The documentation that I've been able to
			find implies a total 28-cycle delay between the real delay enabled signal changing and its effect
			on the 68000 interrupt input via the MFP. As I have yet to determine how much delay is caused
			by the MFP a full 28-cycle delay is applied by this class. This should be dialled down when the
			MFP's responsibility is clarified.
		*/
		bool display_enabled();

		/// @returns the effect of reading from @c address; only the low 6 bits are decoded.
		uint16_t read(int address);

		/// Writes @c value to @c address, of which only the low 6 bits are decoded.
		void write(int address, uint16_t value);

		/// Used internally to track state.
		enum class FieldFrequency {
			Fifty = 0, Sixty = 1, SeventyTwo = 2
		};

		struct RangeObserver {
			/// Indicates to the observer that the memory access range has changed.
			virtual void video_did_change_access_range(Video *) = 0;
		};

		/// Sets a range observer, which is an actor that will be notified if the memory access range changes.
		void set_range_observer(RangeObserver *);

		struct Range {
			uint32_t low_address, high_address;
		};
		/*!
			@returns the range of addresses that the video might read from.
		*/
		Range get_memory_access_range();

	private:
		void advance(HalfCycles duration);
		DeferredQueuePerformer<HalfCycles> deferrer_;

		Outputs::CRT::CRT crt_;
		RangeObserver *range_observer_ = nullptr;

		uint16_t raw_palette_[16];
		uint16_t palette_[16];
		int base_address_ = 0;
		int previous_base_address_ = 0;
		int current_address_ = 0;

		uint16_t *ram_ = nullptr;
		uint16_t line_buffer_[256];

		int x_ = 0, y_ = 0, next_y_ = 0;
		int next_load_toggle_ = -1;
		bool load_ = false;
		int load_base_ = 0;

		uint16_t video_mode_ = 0;
		uint16_t sync_mode_ = 0;

		FieldFrequency field_frequency_ = FieldFrequency::Fifty;
		enum class OutputBpp {
			One, Two, Four
		} output_bpp_ = OutputBpp::Four;
		void update_output_mode();

		struct HorizontalState {
			bool enable = false;
			bool blank = false;
			bool sync = false;
		} horizontal_;
		struct VerticalState {
			bool enable = false;
			bool blank = false;

			enum class SyncSchedule {
				/// No sync events this line.
				None,
				/// Sync should begin during this horizontal line.
				Begin,
				/// Sync should end during this horizontal line.
				End,
			} sync_schedule = SyncSchedule::None;
			bool sync = false;
		} vertical_, next_vertical_;
		LineLength line_length_;

		int data_latch_position_ = 0;
		int data_latch_read_position_ = 0;
		uint16_t data_latch_[128];
		void push_latched_data();

		void reset_fifo();

		/*!
			Provides a target for control over the output video stream, which is considered to be
			a permanently shifting shifter, that you need to reload when appropriate, which can be
			overridden by the blank and sync levels.

			This stream will automatically insert a colour burst.
		*/
		class VideoStream {
			public:
				VideoStream(Outputs::CRT::CRT &crt, uint16_t *palette) : crt_(crt), palette_(palette) {}

				enum class OutputMode {
					Sync, Blank, ColourBurst, Pixels,
				};

				/// Sets the current data format for the shifter. Changes in output BPP flush the shifter.
				void set_bpp(OutputBpp bpp);

				/// Outputs signal of type @c mode for @c duration.
				void output(int duration, OutputMode mode);

				/// Warns the video stream that the border colour, included in the palette that it holds a pointer to,
				/// will change momentarily. This should be called after the relevant @c output() updates, and
				/// is used to help elide border-regio output.
				void will_change_border_colour();

				/// Loads 64 bits into the Shifter. The shifter shifts continuously. If you also declare
				/// a pixels region then whatever is being shifted will reach the display, in a form that
				/// depends on the current output BPP.
				void load(uint64_t value);

			private:
				// The target CRT and the palette to use.
				Outputs::CRT::CRT &crt_;
				uint16_t *palette_ = nullptr;

				// Internal stateful processes.
				void generate(int duration, OutputMode mode, bool is_terminal);

				void flush_border();
				void flush_pixels();
				void shift(int duration);
				void output_pixels(int duration);

				// Internal state that is a function of output intent.
				int duration_ = 0;
				OutputMode output_mode_ = OutputMode::Sync;
				OutputBpp bpp_ = OutputBpp::Four;
				union {
					uint64_t output_shifter_;
					uint32_t shifter_halves_[2];
				};

				// Internal state for handling output serialisation.
				uint16_t *pixel_buffer_ = nullptr;
				int pixel_pointer_ = 0;
		} video_stream_;

		/// Contains copies of the various observeable fields, after the relevant propagation delay.
		struct PublicState {
			bool display_enable = false;
			bool hsync = false;
			bool vsync = false;
		} public_state_;

		struct Event {
			int delay;
			enum class Type {
				SetDisplayEnable, ResetDisplayEnable,
				SetHsync, ResetHsync,
				SetVsync, ResetVsync,
			} type;

			Event(Type type, int delay) : delay(delay), type(type) {}

			void apply(PublicState &state) {
				apply(type, state);
			}

			static void apply(Type type, PublicState &state) {
				switch(type) {
					default:
					case Type::SetDisplayEnable:	state.display_enable = true;	break;
					case Type::ResetDisplayEnable:	state.display_enable = false;	break;
					case Type::SetHsync:			state.hsync = true;				break;
					case Type::ResetHsync:			state.hsync = false;			break;
					case Type::SetVsync:			state.vsync = true;				break;
					case Type::ResetVsync:			state.vsync = false;			break;
				}
			}
		};

/*		std::vector<Event> pending_events_;
		void add_event(int delay, Event::Type type) {
			// Apply immediately if there's no delay (or a negative delay).
			if(delay <= 0) {
				Event::apply(type, public_state_);
				return;
			}

			if(!pending_events_.empty()) {
				// Otherwise enqueue, having subtracted the delay for any preceding events,
				// and subtracting from the subsequent, if any.
				auto insertion_point = pending_events_.begin();
				while(insertion_point != pending_events_.end() && insertion_point->delay < delay) {
					delay -= insertion_point->delay;
					++insertion_point;
				}
				if(insertion_point != pending_events_.end()) {
					insertion_point->delay -= delay;
				}

				pending_events_.emplace(insertion_point, type, delay);
			} else {
				pending_events_.emplace_back(type, delay);
			}
		}*/

		friend class ::VideoTester;
};

}
}

#endif /* Atari_ST_Video_hpp */

//
//  VideoSwitches.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef VideoSwitches_h
#define VideoSwitches_h

#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../ClockReceiver/DeferredQueue.hpp"

namespace Apple {
namespace II {

// Enumerates all Apple II, IIe and IIgs display modes.
enum class GraphicsMode {
	Text = 0,
	DoubleText,
	HighRes,
	DoubleHighRes,
	LowRes,
	DoubleLowRes,
	FatLowRes
};
constexpr bool is_text_mode(GraphicsMode m) { return m <= GraphicsMode::DoubleText; }
constexpr bool is_double_mode(GraphicsMode m) { return int(m) & 1; }

template <typename TimeUnit> class VideoSwitches {
	public:
		/*!
			Constructs a new instance of VideoSwitches in which changes to the switch
			affect the video mode only after @c delay cycles.
		*/
		VideoSwitches(TimeUnit delay, std::function<void(TimeUnit)> &&target) : delay_(delay), deferrer_(std::move(target)) {}

		/*!
			Advances @c cycles.
		*/
		void run_for(TimeUnit cycles) {
			deferrer_.run_for(cycles);
		}

		/*
			Descriptions for the setters below are taken verbatim from
			the Apple IIe Technical Reference. Addresses are the conventional
			locations within the Apple II memory map. Only those which affect
			video output are implemented here.

			Those registers which don't exist on a II/II+ are marked.
		*/

		/*!
			Setter for ALTCHAR ($C00E/$C00F; triggers on write only):

			* Off: display text using primary character set.
			* On: display text using alternate character set.

			Doesn't exist on a II/II+.
		*/
		void set_alternative_character_set(bool alternative_character_set) {
			external_.alternative_character_set = alternative_character_set;
			deferrer_.defer(delay_, [this, alternative_character_set] {
				internal_.alternative_character_set = alternative_character_set;
				did_set_annunciator_3(alternative_character_set);
			});
		}
		bool get_alternative_character_set() const {
			return external_.alternative_character_set;
		}

		/*!
			Setter for 80COL ($C00C/$C00D; triggers on write only).

			* Off: display 40 columns.
			* On: display 80 columns.

			Doesn't exist on a II/II+.
		*/
		void set_80_columns(bool columns_80) {
			external_.columns_80 = columns_80;
			deferrer_.defer(delay_, [this, columns_80] {
				internal_.columns_80 = columns_80;
			});
		}
		bool get_80_columns() const {
			return external_.columns_80;
		}

		/*!
			Setter for 80STORE ($C000/$C001; triggers on write only).

			* Off: cause PAGE2 to select auxiliary RAM.
			* On: cause PAGE2 to switch main RAM areas.

			Doesn't exist on a II/II+.
		*/
		void set_80_store(bool store_80) {
			external_.store_80 = internal_.store_80 = store_80;
		}
		bool get_80_store() const {
			return external_.store_80;
		}

		/*!
			Setter for PAGE2 ($C054/$C055; triggers on read or write).

			* Off: select Page 1.
			* On: select Page 2 or, if 80STORE on, Page 1 in auxiliary memory.

			80STORE doesn't exist on a II/II+; therefore this always selects
			either Page 1 or Page 2 on those machines.
		*/
		void set_page2(bool page2) {
			external_.page2 = internal_.page2 = page2;
		}
		bool get_page2() const {
			return external_.page2;
		}

		/*!
			Setter for TEXT ($C050/$C051; triggers on read or write).

			* Off: display graphics or, if MIXED on, mixed.
			* On: display text.
		*/
		void set_text(bool text) {
			external_.text = text;
			deferrer_.defer(delay_, [this, text] {
				internal_.text = text;
			});
		}
		bool get_text() const {
			return external_.text;
		}

		/*!
			Setter for MIXED ($C052/$C053; triggers on read or write).

			* Off: display only text or only graphics.
			* On: if TEXT off, display text and graphics.
		*/
		void set_mixed(bool mixed) {
			external_.mixed = mixed;
			deferrer_.defer(delay_, [this, mixed] {
				internal_.mixed = mixed;
			});
		}
		bool get_mixed() const {
			return external_.mixed;
		}

		/*!
			Setter for HIRES ($C056/$C057; triggers on read or write).

			* Off: if TEXT off, display low-resolution graphics.
			* On: if TEXT off, display high-resolution or, if DHIRES on, double high-resolution graphics.

			DHIRES doesn't exist on a II/II+; therefore this always selects
			either high- or low-resolution graphics on those machines.

			Despite Apple's documentation, the IIe also supports double low-resolution
			graphics, which are the 80-column analogue to ordinary low-resolution 40-column
			low-resolution graphics.
		*/
		void set_high_resolution(bool high_resolution) {
			external_.high_resolution = high_resolution;
			deferrer_.defer(delay_, [this, high_resolution] {
				internal_.high_resolution = high_resolution;
			});
		}
		bool get_high_resolution() const {
			return external_.high_resolution;
		}

		/*!
			Setter for annunciator 3.

			* On: turn on annunciator 3.
			* Off: turn off annunciator 3.

			This exists on both the II/II+ and the IIe, but has no effect on
			video on the older machines. It's intended to be used on the IIe
			to confirm double-high resolution mode but has side effects in
			selecting mixed mode output and discarding high-resolution
			delay bits.
		*/
		void set_annunciator_3(bool annunciator_3) {
			external_.annunciator_3 = annunciator_3;
			deferrer_.defer(delay_, [this, annunciator_3] {
				internal_.annunciator_3 = annunciator_3;
				did_set_alternative_character_set(annunciator_3);
			});
		}
		bool get_annunciator_3() const {
			return external_.annunciator_3;
		}

	protected:
		GraphicsMode graphics_mode(int row) const {
			if(
				internal_.text ||
				(internal_.mixed && row >= 160 && row < 192)
			) return internal_.columns_80 ? GraphicsMode::DoubleText : GraphicsMode::Text;
			if(internal_.high_resolution) {
				return (internal_.annunciator_3 && internal_.columns_80) ? GraphicsMode::DoubleHighRes : GraphicsMode::HighRes;
			} else {
				if(internal_.columns_80) return GraphicsMode::DoubleLowRes;
				if(internal_.annunciator_3) return GraphicsMode::FatLowRes;
				return GraphicsMode::LowRes;
			}
		}

		int video_page() const {
			return (internal_.store_80 || !internal_.page2) ? 0 : 1;
		}

		uint16_t get_row_address(int row) const {
			const int character_row = row >> 3;
			const int pixel_row = row & 7;
			const uint16_t row_address = uint16_t((character_row >> 3) * 40 + ((character_row&7) << 7));

			const GraphicsMode pixel_mode = graphics_mode(row);
			return ((pixel_mode == GraphicsMode::HighRes) || (pixel_mode == GraphicsMode::DoubleHighRes)) ?
				uint16_t(((video_page()+1) * 0x2000) + row_address + ((pixel_row&7) << 10)) :
				uint16_t(((video_page()+1) * 0x400) + row_address);
		}

		virtual void did_set_annunciator_3(bool) = 0;
		virtual void did_set_alternative_character_set(bool) = 0;

	private:
		// Maintain a DeferredQueue for delayed mode switches.
		const TimeUnit delay_;
		DeferredQueuePerformer<TimeUnit> deferrer_;

		struct Switches {
			bool alternative_character_set = false;
			bool columns_80 = false;
			bool store_80 = false;
			bool page2 = false;
			bool text = true;
			bool mixed = false;
			bool high_resolution = false;
			bool annunciator_3 = false;
		} external_, internal_;
};

}
}

#endif /* VideoSwitches_h */

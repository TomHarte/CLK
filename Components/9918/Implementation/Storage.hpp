//
//  Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Storage_h
#define Storage_h

#include "LineBuffer.hpp"
#include "YamahaCommands.hpp"

namespace TI {
namespace TMS {

/// A container for personality-specific storage; see specific instances below.
template <Personality personality, typename Enable = void> struct Storage {
};

template <> struct Storage<Personality::TMS9918A> {
	using AddressT = uint16_t;

	void begin_line(ScreenMode, bool, bool) {}
};

// Yamaha-specific storage.
template <Personality personality> struct Storage<personality, std::enable_if_t<is_yamaha_vdp(personality)>> {
	using AddressT = uint32_t;

	int selected_status_ = 0;

	int indirect_register_ = 0;
	bool increment_indirect_register_ = false;

	std::array<uint32_t, 16> palette_{};
	uint8_t new_colour_ = 0;
	uint8_t palette_entry_ = 0;
	bool palette_write_phase_ = false;

	uint8_t mode_ = 0;

	uint8_t vertical_offset_ = 0;

	/// Describes an _observable_ memory access event. i.e. anything that it is safe
	/// (and convenient) to treat as atomic in between external slots.
	struct Event {
		/// Offset of the _beginning_ of the event. Not completely arbitrarily: this is when
		/// external data must be ready by in order to take part in those slots.
		int offset = 1368;
		enum class Type {
			External,
			DataBlock,
			SpriteY,
			SpriteContents,
		} type = Type::External;

		constexpr Event(int offset, Type type) noexcept :
			offset(grauw_to_internal(offset)),
			type(type) {}

		constexpr Event(int offset) noexcept :
			offset(grauw_to_internal(offset)) {}

		constexpr Event() noexcept {}
	};

	// State that tracks fetching position within a line.
	const Event *next_event_ = nullptr;
	int data_block_ = 0;
	int sprite_block_ = 0;

	/// Resets line-ephemeral state for a new line.
	void begin_line([[maybe_unused]] ScreenMode mode, bool is_refresh, [[maybe_unused]] bool sprites_enabled) {
		// TODO: reinstate upon completion of the Yamaha pipeline.
//		assert(mode < ScreenMode::YamahaText80 || next_event_ == nullptr || next_event_->offset == 1368);

		data_block_ = 0;
		sprite_block_ = 0;

		if(is_refresh) {
			next_event_ = refresh_events;
			return;
		}

		// TODO: obey sprites_enabled flag, at least.
		next_event_ = no_sprites_events;
	}

	// Command engine state.
	CommandContext command_context_;
	std::unique_ptr<Command> command_ = nullptr;

	enum class CommandStep {
		None,

		ReadSourcePixel,
		ReadDestinationPixel,
		WritePixel,

		ReadSourceByte,
		WriteByte,
	};
	CommandStep next_command_step_ = CommandStep::None;
	int minimum_command_column_ = 0;
	uint8_t command_latch_ = 0;

	void update_command_step(int current_column) {
		if(!command_) {
			next_command_step_ = CommandStep::None;
			return;
		}
		if(command_->done()) {
			command_ = nullptr;
			next_command_step_ = CommandStep::None;
			return;
		}

		minimum_command_column_ = current_column + command_->cycles;
		switch(command_->access) {
			case Command::AccessType::CopyPoint:
				next_command_step_ = CommandStep::ReadSourcePixel;
			break;
			case Command::AccessType::PlotPoint:
				next_command_step_ = CommandStep::ReadDestinationPixel;
			break;

			case Command::AccessType::WaitForColourReceipt:
				// i.e. nothing to do until a colour is received.
				next_command_step_ = CommandStep::None;
			break;

			case Command::AccessType::CopyByte:
				next_command_step_ = CommandStep::ReadSourceByte;
			break;
			case Command::AccessType::WriteByte:
				next_command_step_ = CommandStep::WriteByte;
			break;
		}
	}

	Storage() noexcept {
		// Perform sanity checks on the event lists.
#ifndef NDEBUG
		const Event *lists[] = { no_sprites_events, refresh_events, nullptr };
		const Event **list = lists;
		while(*list) {
			const Event *cursor = *list;
			++list;

			while(cursor[1].offset != 1368) {
				assert(cursor[1].offset > cursor[0].offset);
				++cursor;
			}
		}
#endif

		// Seed to _something_ meaningful.
		//
		// TODO: this is a workaround [/hack], in effect, for the main TMS' habit of starting
		// in a randomised position, which means that start-of-line isn't announced.
		//
		// Do I really want that behaviour?
		next_event_ = refresh_events;
	}

	private:
		// This emulator treats position 0 as being immediately after the standard pixel area.
		// i.e. offset 1282 on Grauw's http://map.grauw.nl/articles/vdp-vram-timing/vdp-timing.png
		constexpr static int grauw_to_internal(int offset) {
			return (offset + 1368 - 1282) % 1368;
		}

		static constexpr Event refresh_events[] = {
			Event(1284),	Event(1292),	Event(1300),	Event(1308),	Event(1316),	Event(1324),
			Event(1334),	Event(1344),	Event(1352),	Event(1360),	Event(0),		Event(8),
			Event(16),		Event(24),		Event(32),		Event(40),		Event(48),		Event(56),
			Event(64),		Event(72),		Event(80),		Event(88),		Event(96),		Event(104),
			Event(112),		Event(120),

			Event(164),		Event(172),		Event(180),		Event(188),		Event(196),		Event(204),
			Event(212),		Event(220),		Event(228),		Event(236),		Event(244),		Event(252),
			Event(260),		Event(268),		Event(276),		/* Refresh. */	Event(292),		Event(300),
			Event(308),		Event(316),		Event(324),		Event(332),		Event(340),		Event(348),
			Event(356),		Event(364),		Event(372),		Event(380),		Event(388),		Event(396),
			Event(404),		/* Refresh. */	Event(420),		Event(428),		Event(436),		Event(444),
			Event(452),		Event(460),		Event(468),		Event(476),		Event(484),		Event(492),
			Event(500),		Event(508),		Event(516),		Event(524),		Event(532),		/* Refresh. */
			Event(548),		Event(556),		Event(564),		Event(570),		Event(580),		Event(588),
			Event(596),		Event(604),		Event(612),		Event(620),		Event(628),		Event(636),
			Event(644),		Event(652),		Event(660),		/* Refresh. */	Event(676),		Event(684),
			Event(692),		Event(700),		Event(708),		Event(716),		Event(724),		Event(732),
			Event(740),		Event(748),		Event(756),		Event(764),		Event(772),		Event(780),
			Event(788),		/* Refresh. */	Event(804),		Event(812),		Event(820),		Event(828),
			Event(836),		Event(844),		Event(852),		Event(860),		Event(868),		Event(876),
			Event(884),		Event(892),		Event(900),		Event(908),		Event(916),		/* Refresh. */
			Event(932),		Event(940),		Event(948),		Event(956),		Event(964),		Event(972),
			Event(980),		Event(988),		Event(996),		Event(1004),	Event(1012),	Event(1020),
			Event(1028),	Event(1036),	Event(1044),	/* Refresh. */	Event(1060),	Event(1068),
			Event(1076),	Event(1084),	Event(1092),	Event(1100),	Event(1108),	Event(1116),
			Event(1124),	Event(1132),	Event(1140),	Event(1148),	Event(1156),	Event(1164),
			Event(1172),	/* Refresh. */	Event(1188),	Event(1196),	Event(1204),	Event(1212),
			Event(1220),	Event(1228),

			Event(1268),	Event(1276),

			Event()
		};

		static constexpr Event no_sprites_events[] = {
			Event(1282),	Event(1290),	Event(1298),	Event(1306),
			Event(1314),	Event(1322),	Event(1332),	Event(1342),
			Event(1350),	Event(1358),	Event(1366),

			Event(6),		Event(14),		Event(22),		Event(30),
			Event(38),		Event(46),		Event(54),		Event(62),
			Event(70),		Event(78),		Event(86),		Event(94),
			Event(102),		Event(110),		Event(118),

			Event(162),		Event(170),		Event(182),		Event(188),
			// Omitted: dummy data block. Is not observable.
			Event(214),		Event(220),

			Event(226, Event::Type::DataBlock),		Event(246),		Event(252),
			Event(258, Event::Type::DataBlock),		Event(278),		// Omitted: refresh.
			Event(290, Event::Type::DataBlock),		Event(310),		Event(316),
			Event(322, Event::Type::DataBlock),		Event(342),		Event(348),
			Event(354, Event::Type::DataBlock),		Event(374),		Event(380),
			Event(386, Event::Type::DataBlock),		Event(406),		// Omitted: refresh.
			Event(418, Event::Type::DataBlock),		Event(438),		Event(444),
			Event(450, Event::Type::DataBlock),		Event(470),		Event(476),

			Event(482, Event::Type::DataBlock),		Event(502),		Event(508),
			Event(514, Event::Type::DataBlock),		Event(534),		// Omitted: refresh.
			Event(546, Event::Type::DataBlock),		Event(566),		Event(572),
			Event(578, Event::Type::DataBlock),		Event(598),		Event(604),
			Event(610, Event::Type::DataBlock),		Event(630),		Event(636),
			Event(642, Event::Type::DataBlock),		Event(662),		// Omitted: refresh.
			Event(674, Event::Type::DataBlock),		Event(694),		Event(700),
			Event(706, Event::Type::DataBlock),		Event(726),		Event(732),

			Event(738, Event::Type::DataBlock),		Event(758),		Event(764),
			Event(770, Event::Type::DataBlock),		Event(790),		// Omitted: refresh.
			Event(802, Event::Type::DataBlock),		Event(822),		Event(828),
			Event(834, Event::Type::DataBlock),		Event(854),		Event(860),
			Event(866, Event::Type::DataBlock),		Event(886),		Event(892),
			Event(898, Event::Type::DataBlock),		Event(918),		// Omitted: refresh.
			Event(930, Event::Type::DataBlock),		Event(950),		Event(956),
			Event(962, Event::Type::DataBlock),		Event(982),		Event(988),

			Event(994, Event::Type::DataBlock),		Event(1014),	Event(1020),
			Event(1026, Event::Type::DataBlock),	Event(1046),	// Omitted: refresh.
			Event(1058, Event::Type::DataBlock),	Event(1078),	Event(1084),
			Event(1090, Event::Type::DataBlock),	Event(1110),	Event(1116),
			Event(1122, Event::Type::DataBlock),	Event(1142),	Event(1148),
			Event(1154, Event::Type::DataBlock),	Event(1174),	// Omitted: refresh.
			Event(1186, Event::Type::DataBlock),	Event(1206),	Event(1212),
			Event(1218, Event::Type::DataBlock),

			Event(1266),
			Event(1274),

			Event()
		};
};

// Master System-specific storage.
template <Personality personality> struct Storage<personality, std::enable_if_t<is_sega_vdp(personality)>> {
	using AddressT = uint16_t;

	// The SMS VDP has a programmer-set colour palette, with a dedicated patch of RAM. But the RAM is only exactly
	// fast enough for the pixel clock. So when the programmer writes to it, that causes a one-pixel glitch; there
	// isn't the bandwidth for the read both write to occur simultaneously. The following buffer therefore keeps
	// track of pending collisions, for visual reproduction.
	struct CRAMDot {
		LineBufferPointer location;
		uint32_t value;
	};
	std::vector<CRAMDot> upcoming_cram_dots_;

	// The Master System's additional colour RAM.
	uint32_t colour_ram_[32];
	bool cram_is_selected_ = false;

	// Programmer-set flags.
	bool vertical_scroll_lock_ = false;
	bool horizontal_scroll_lock_ = false;
	bool hide_left_column_ = false;
	bool shift_sprites_8px_left_ = false;
	bool mode4_enable_ = false;
	uint8_t horizontal_scroll_ = 0;
	uint8_t vertical_scroll_ = 0;

	// Holds the vertical scroll position for this frame; this is latched
	// once and cannot dynamically be changed until the next frame.
	uint8_t latched_vertical_scroll_ = 0;

	// Various resource addresses with VDP-version-specific modifications
	// built in.
	AddressT pattern_name_address_;
	AddressT sprite_attribute_table_address_;
	AddressT sprite_generator_table_address_;

	void begin_line(ScreenMode, bool, bool) {}
};

}
}

#endif /* Storage_h */

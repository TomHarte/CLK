//
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

#include <optional>

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
			SpriteLocation,
			SpritePattern,
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
			next_event_ = refresh_events.data();
			return;
		}

		// TODO: obey sprites_enabled flag, at least.
		next_event_ = no_sprites_events.data();
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
		const Event *lists[] = { no_sprites_events.data(), refresh_events.data(), nullptr };
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
		next_event_ = refresh_events.data();
	}

	private:
		// This emulator treats position 0 as being immediately after the standard pixel area.
		// i.e. offset 1282 on Grauw's http://map.grauw.nl/articles/vdp-vram-timing/vdp-timing.png
		static constexpr int ZeroAsGrauwIndex = 1282;
		constexpr static int grauw_to_internal(int offset) {
			return (offset + 1368 - ZeroAsGrauwIndex) % 1368;
		}
		constexpr static int internal_to_grauw(int offset) {
			return (offset + ZeroAsGrauwIndex) % 1368;
		}

		template <typename GeneratorT> static constexpr size_t events_size() {
			size_t size = 0;
			for(int c = 0; c < 1368; c++) {
				const auto event_type = GeneratorT::event(internal_to_grauw(c));
				size += event_type.has_value();
			}
			return size + 1;
		}

		template <typename GeneratorT, size_t size = events_size<GeneratorT>()>
		static constexpr std::array<Event, size> events() {
			std::array<Event, size> result{};
			size_t index = 0;
			for(int c = 0; c < 1368; c++) {
				const auto event_type = GeneratorT::event(internal_to_grauw(c));
				if(!event_type) {
					continue;
				}
				// TODO: once all lists are using generators, remove implicit grauw_to_internal
				// from Event constructor and just supply c here.
				result[index] = Event(internal_to_grauw(c), *event_type);
				++index;
			}
			result[index] = Event();
			return result;
		}

		struct StandardGenerators {
			static constexpr std::optional<typename Event::Type> external_every_eight(int index) {
				if(index & 7) return std::nullopt;
				return Event::Type::External;
			}
		};

		struct RefreshGenerator {
			static constexpr std::optional<typename Event::Type> event(int grauw_index) {
				// From 0 to 126: CPU/CMD slots at every cycle divisible by 8.
				if(grauw_index < 126) {
					return StandardGenerators::external_every_eight(grauw_index - 0);
				}

				// From 164 to 1234: eight-cycle windows, the first 15 of each 16 being
				// CPU/CMD and the final being refresh.
				if(grauw_index >= 164 && grauw_index < 1234) {
					const int offset = grauw_index - 164;
					if(offset & 7) return std::nullopt;
					if(((offset >> 3) & 15) == 15) return std::nullopt;
					return Event::Type::External;
				}

				// From 1268 to 1330: CPU/CMD slots at every cycle divisible by 8.
				if(grauw_index >= 1268 && grauw_index < 1330) {
					return StandardGenerators::external_every_eight(grauw_index - 1268);
				}

				// A CPU/CMD at 1334.
				if(grauw_index == 1334) {
					return Event::Type::External;
				}

				// From 1344 to 1366: CPU/CMD slots every cycle divisible by 8.
				if(grauw_index >= 1344 && grauw_index < 1366) {
					return StandardGenerators::external_every_eight(grauw_index - 1344);
				}

				// Otherwise: nothing.
				return std::nullopt;
			}
		};
		static constexpr auto refresh_events = events<RefreshGenerator>();

		struct NoSpriteEventsGenerator {
			static constexpr std::optional<typename Event::Type> event(int grauw_index) {
				// Various standard zones of one-every-eight external slots.
				if(grauw_index < 124) {
					return StandardGenerators::external_every_eight(grauw_index + 2);
				}
				if(grauw_index > 1266) {
					return StandardGenerators::external_every_eight(grauw_index - 1266);
				}
				if(grauw_index >= 162 && grauw_index < 176) {
					return StandardGenerators::external_every_eight(grauw_index - 162);
				}

				// Everywhere else the pattern is:
				//
				//	external, external, data block
				//
				// Subject to caveats:
				//
				//	1)	the first data block is just a dummy fetch with no side effects,
				//		so this emulator declines to record it; and
				//	2)	every fourth block, the second external is actually a refresh.
				//
				if(grauw_index >= 182 && grauw_index < 1238) {
					const int offset = grauw_index - 182;
					const int block = offset / 32;
					const int sub_block = offset & 31;

					switch(sub_block) {
						default:	return std::nullopt;
						case 0:		return Event::Type::External;
						case 6:
							if((block & 3) != 3) {
								return Event::Type::External;
							}
						break;
						case 12:
							if(block) {
								return Event::Type::DataBlock;
							}
						break;
					}
				}

				return std::nullopt;
			}
		};
		static constexpr auto no_sprites_events = events<NoSpriteEventsGenerator>();
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

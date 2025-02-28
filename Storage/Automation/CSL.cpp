//
//  CSL.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/06/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "CSL.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <set>

#include "Machines/AmstradCPC/Keyboard.hpp"

using namespace Storage::Automation;

namespace {

bool append_typed(std::vector<Storage::Automation::CSL::KeyEvent> &down, std::vector<Storage::Automation::CSL::KeyEvent> &up, std::istringstream &stream) {
	const auto press = [&](uint16_t key) {
		CSL::KeyEvent event;
		event.key = key;
		event.down = true;
		down.push_back(event);
		event.down = false;
		up.push_back(event);
	};

	const auto shift = [&](uint16_t key) {
		CSL::KeyEvent event;
		event.key = AmstradCPC::Key::KeyShift;
		event.down = true;
		down.push_back(event);
		press(key);
		event.down = false;
		up.push_back(event);
	};

	const auto next = stream.get();
	if(stream.eof()) return false;

	switch(next) {
		default: throw CSL::InvalidArgument;
		case '\'': return false;
		case '}': return false;

		case 'A':	press(AmstradCPC::Key::KeyA);		break;
		case 'B':	press(AmstradCPC::Key::KeyB);		break;
		case 'C':	press(AmstradCPC::Key::KeyC);		break;
		case 'D':	press(AmstradCPC::Key::KeyD);		break;
		case 'E':	press(AmstradCPC::Key::KeyE);		break;
		case 'F':	press(AmstradCPC::Key::KeyF);		break;
		case 'G':	press(AmstradCPC::Key::KeyG);		break;
		case 'H':	press(AmstradCPC::Key::KeyH);		break;
		case 'I':	press(AmstradCPC::Key::KeyI);		break;
		case 'J':	press(AmstradCPC::Key::KeyJ);		break;
		case 'K':	press(AmstradCPC::Key::KeyK);		break;
		case 'L':	press(AmstradCPC::Key::KeyL);		break;
		case 'M':	press(AmstradCPC::Key::KeyM);		break;
		case 'N':	press(AmstradCPC::Key::KeyN);		break;
		case 'O':	press(AmstradCPC::Key::KeyO);		break;
		case 'P':	press(AmstradCPC::Key::KeyP);		break;
		case 'Q':	press(AmstradCPC::Key::KeyQ);		break;
		case 'R':	press(AmstradCPC::Key::KeyR);		break;
		case 'S':	press(AmstradCPC::Key::KeyS);		break;
		case 'T':	press(AmstradCPC::Key::KeyT);		break;
		case 'U':	press(AmstradCPC::Key::KeyU);		break;
		case 'V':	press(AmstradCPC::Key::KeyV);		break;
		case 'W':	press(AmstradCPC::Key::KeyW);		break;
		case 'X':	press(AmstradCPC::Key::KeyX);		break;
		case 'Y':	press(AmstradCPC::Key::KeyY);		break;
		case 'Z':	press(AmstradCPC::Key::KeyZ);		break;
		case ' ':	press(AmstradCPC::Key::KeySpace);	break;
		case '0':	press(AmstradCPC::Key::Key0);		break;
		case '1':	press(AmstradCPC::Key::Key1);		break;
		case '2':	press(AmstradCPC::Key::Key2);		break;
		case '3':	press(AmstradCPC::Key::Key3);		break;
		case '4':	press(AmstradCPC::Key::Key4);		break;
		case '5':	press(AmstradCPC::Key::Key5);		break;
		case '6':	press(AmstradCPC::Key::Key6);		break;
		case '7':	press(AmstradCPC::Key::Key7);		break;
		case '8':	press(AmstradCPC::Key::Key8);		break;
		case '9':	press(AmstradCPC::Key::Key9);		break;

		case '"':	shift(AmstradCPC::Key::Key2);		break;

		case '\\': {
			if(stream.peek() != '(') {
				press(AmstradCPC::Key::KeyBackSlash);
				break;
			}
			stream.get();

			std::string name;
			while(stream.peek() != ')') {
				name.push_back(char(stream.get()));
			}
			stream.get();

			static const std::unordered_map<std::string, uint16_t> names = {
				{"ESC", AmstradCPC::Key::KeyEscape},
				{"TAB", AmstradCPC::Key::KeyTab},
				{"CAP", AmstradCPC::Key::KeyCapsLock},
				{"SHI", AmstradCPC::Key::KeyShift},
				{"CTR", AmstradCPC::Key::KeyControl},
				{"COP", AmstradCPC::Key::KeyCopy},
				{"CLR", AmstradCPC::Key::KeyClear},
				{"DEL", AmstradCPC::Key::KeyDelete},
				{"RET", AmstradCPC::Key::KeyReturn},
				{"ENT", AmstradCPC::Key::KeyEnter},
				{"ARL", AmstradCPC::Key::KeyLeft},
				{"ARR", AmstradCPC::Key::KeyRight},
				{"ARU", AmstradCPC::Key::KeyUp},
				{"ARD", AmstradCPC::Key::KeyDown},
				{"FN0", AmstradCPC::Key::KeyF0},
				{"FN1", AmstradCPC::Key::KeyF1},
				{"FN2", AmstradCPC::Key::KeyF2},
				{"FN3", AmstradCPC::Key::KeyF3},
				{"FN4", AmstradCPC::Key::KeyF4},
				{"FN5", AmstradCPC::Key::KeyF5},
				{"FN6", AmstradCPC::Key::KeyF6},
				{"FN7", AmstradCPC::Key::KeyF7},
				{"FN8", AmstradCPC::Key::KeyF8},
				{"FN9", AmstradCPC::Key::KeyF9},
				//TODO: { } \ ' KOF
			};
			const auto name_pair = names.find(name);
			if(name_pair == names.end()) {
				throw CSL::InvalidArgument;
			}
			press(name_pair->second);
		} break;

		case '{':
			while(append_typed(down, up, stream));
		break;
	}

	return true;
}

}

std::vector<CSL::Instruction> CSL::parse(const std::string &file_name) {
	std::vector<Instruction> instructions;
	std::ifstream file;
	file.open(file_name);

	using Type = Instruction::Type;
	static const std::unordered_map<std::string, Type> keywords = {
		{"csl_version", Type::Version},
		{"reset", Type::Reset},
		{"crtc_select", Type::CRTCSelect},
		{"disk_insert", Type::DiskInsert},
		{"disk_dir", Type::SetDiskDir},
		{"tape_insert", Type::TapeInsert},
		{"tape_dir", Type::SetTapeDir},
		{"tape_play", Type::TapeInsert},
		{"tape_stop", Type::TapeStop},
		{"tape_rewind", Type::TapeRewind},
		{"snapshot_load", Type::LoadSnapshot},
		{"snapshot_dir", Type::SetSnapshotDir},
		{"key_delay", Type::KeyDelay},
		{"key_output", Type::KeyOutput},
		{"key_from_file", Type::KeyFromFile},
		{"wait", Type::Wait},
		{"wait_driveonoff", Type::WaitDriveOnOff},
		{"wait_ssm0000", Type::WaitSSM0000},
		{"screenshot_name", Type::SetScreenshotName},
		{"screenshot_dir", Type::SetScreenshotDir},
		{"screenshot", Type::Screenshot},
		{"snapshot_name", Type::SetSnapshotDir},
		{"csl_load", Type::LoadCSL},
	};

	for(std::string line; std::getline(file, line); ) {
		// Ignore comments and empty lines.
		if(line.empty() || line[0] == ';') {
			continue;
		}

		std::istringstream stream(line);
		std::string keyword;
		stream >> keyword;

		// Second way for a line to be empty: purely whitespace.
		if(keyword.empty()) {
			continue;
		}

		const auto key_pair = keywords.find(keyword);
		if(key_pair == keywords.end()) {
			throw InvalidKeyword;
		}

		Instruction instruction;
		instruction.type = key_pair->second;

		// TODO: strings are encoded specially in order to capture whitespace.
		// They're surrounded in single quotes with some special keys escaped.
		const auto require = [&](auto &&target) {
			stream >> target;
			if(stream.fail()) {
				throw InvalidArgument;
			}
		};

		switch(instruction.type) {
			// Keywords with no argument.
			case Type::TapePlay:
			case Type::TapeStop:
			case Type::TapeRewind:
			case Type::WaitVsyncOnOff:
			case Type::WaitSSM0000:
			break;

			// Keywords with a single string mandatory argument
			// that can be directly captured as a string.
			case Type::Version: {
				std::string argument;
				require(argument);
				instruction.argument = argument;
			} break;

			// Keywords with a single string mandatory argument
			// that is within quotes but otherwise directly usable
			// as a string.
			case Type::LoadCSL:
			case Type::SetScreenshotDir:
			case Type::SetScreenshotName:
			case Type::SetSnapshotDir:
			case Type::SetSnapshotName:
			case Type::LoadSnapshot:
			case Type::SetTapeDir:
			case Type::TapeInsert:
			case Type::SetDiskDir:
			case Type::KeyFromFile: {
				std::string argument;

				char next;
				stream >> next;
				if(next != '\'') {
					throw InvalidArgument;
				}

				while(true) {
					next = static_cast<char>(stream.get());
					if(stream.eof()) break;

					// Take a bit of a random guess about what's escaped
					// in regular string arguments.
					if(next == '\\' && stream.peek() == '(') {
						stream.get();
						if(stream.peek() != '\'') {
							argument.push_back('\\');
							argument.push_back('(');
							continue;
						}
					}

					if(next == '\'') {
						break;
					}
					argument.push_back(next);
				}
				instruction.argument = argument;
			} break;

			// Keywords with a single number mandatory argument.
			case Type::WaitDriveOnOff:
			case Type::Wait: {
				uint64_t argument;
				require(argument);
				instruction.argument = argument;
			} break;

			// Miscellaneous:
			case Type::Snapshot:
			case Type::Screenshot: {
				std::string vsync;
				stream >> vsync;
				if(stream.fail()) {
					instruction.argument = ScreenshotOrSnapshot::Now;
					break;
				}
				if(vsync != "vsync") {
					throw InvalidArgument;
				}
				instruction.argument = ScreenshotOrSnapshot::WaitForVSync;
			} break;

			case Type::Reset: {
				std::string type;
				stream >> type;
				if(!stream.fail()) {
					if(type != "soft" && type != "hard") {
						throw InvalidArgument;
					}
					instruction.argument = (type == "soft") ? Reset::Soft : Reset::Hard;
				}
			} break;

			case Type::CRTCSelect: {
				std::string type;
				require(type);

				static const std::set<std::string> allowed_types = {
					"0", "1", "1A", "1B", "2", "3", "4",
				};
				if(allowed_types.find(type) == allowed_types.end()) {
					throw InvalidArgument;
				}

				instruction.argument = static_cast<uint64_t>(std::stoi(type));
			} break;

			case Type::DiskInsert: {
				std::string name;
				require(name);

				// Crop the assumed opening and closing quotes.
				name.erase(name.end() - 1);
				name.erase(name.begin());

				DiskInsert argument;
				if(name.size() == 1) {
					argument.drive = toupper(name[0]) - 'A';
					require(name);
				}

				argument.file = name;
				instruction.argument = argument;
			} break;

			case Type::KeyOutput: {
				std::vector<KeyEvent> argument;

				char next;
				stream >> next;
				if(next != '\'') {
					throw InvalidArgument;
				}

				std::vector<KeyEvent> down;
				std::vector<KeyEvent> up;
				while(append_typed(down, up, stream)) {
					std::copy(down.begin(), down.end(), std::back_inserter(argument));
					std::copy(up.begin(), up.end(), std::back_inserter(argument));
					down.clear();
					up.clear();
				}
				instruction.argument = argument;
			} break;

			case Type::KeyDelay: {
				KeyDelay argument;
				require(argument.press_delay);

				uint64_t interpress_delay;
				stream >> interpress_delay;
				if(!stream.fail()) {
					argument.interpress_delay = argument.press_delay;
				}

				uint64_t carriage_return_delay;
				stream >> carriage_return_delay;
				if(!stream.fail()) {
					argument.carriage_return_delay = carriage_return_delay;
				}
				instruction.argument = argument;
			} break;
		}

		instructions.push_back(std::move(instruction));
	}

	return instructions;
}

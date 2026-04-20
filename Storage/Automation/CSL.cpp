//
//  CSL.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/06/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "CSL.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>

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
		event.key = AmstradCPC::Key::Key::Shift;
		event.down = true;
		down.push_back(event);
		press(key);
		event.down = false;
		up.push_back(event);
	};

	const auto next = stream.get();
	if(stream.eof()) return false;

	switch(next) {
		using enum AmstradCPC::Key::Key;

		default: throw CSL::InvalidArgument;
		case '\'': return false;
		case '}': return false;

		case 'A':	press(A);		break;
		case 'B':	press(B);		break;
		case 'C':	press(C);		break;
		case 'D':	press(D);		break;
		case 'E':	press(E);		break;
		case 'F':	press(F);		break;
		case 'G':	press(G);		break;
		case 'H':	press(H);		break;
		case 'I':	press(I);		break;
		case 'J':	press(J);		break;
		case 'K':	press(K);		break;
		case 'L':	press(L);		break;
		case 'M':	press(M);		break;
		case 'N':	press(N);		break;
		case 'O':	press(O);		break;
		case 'P':	press(P);		break;
		case 'Q':	press(Q);		break;
		case 'R':	press(R);		break;
		case 'S':	press(S);		break;
		case 'T':	press(T);		break;
		case 'U':	press(U);		break;
		case 'V':	press(V);		break;
		case 'W':	press(W);		break;
		case 'X':	press(X);		break;
		case 'Y':	press(Y);		break;
		case 'Z':	press(Z);		break;
		case ' ':	press(Space);	break;
		case '0':	press(k0);		break;
		case '1':	press(k1);		break;
		case '2':	press(k2);		break;
		case '3':	press(k3);		break;
		case '4':	press(k4);		break;
		case '5':	press(k5);		break;
		case '6':	press(k6);		break;
		case '7':	press(k7);		break;
		case '8':	press(k8);		break;
		case '9':	press(k9);		break;

		case '"':	shift(k2);		break;

		case '\\': {
			if(stream.peek() != '(') {
				press(BackSlash);
				break;
			}
			stream.get();

			std::string name;
			while(stream.peek() != ')') {
				name.push_back(char(stream.get()));
			}
			stream.get();

			static const std::unordered_map<std::string, uint16_t> names = {
				{"ESC", Escape},
				{"TAB", Tab},
				{"CAP", CapsLock},
				{"SHI", Shift},
				{"CTR", Control},
				{"COP", Copy},
				{"CLR", Clear},
				{"DEL", Delete},
				{"RET", Return},
				{"ENT", Enter},
				{"ARL", Left},
				{"ARR", Right},
				{"ARU", Up},
				{"ARD", Down},
				{"FN0", F0},
				{"FN1", F1},
				{"FN2", F2},
				{"FN3", F3},
				{"FN4", F4},
				{"FN5", F5},
				{"FN6", F6},
				{"FN7", F7},
				{"FN8", F8},
				{"FN9", F9},
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
					std::ranges::copy(down, std::back_inserter(argument));
					std::ranges::copy(up, std::back_inserter(argument));
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

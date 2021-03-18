//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "../../../Storage/Tape/Parsers/Spectrum.hpp"
#include "Target.hpp"

namespace {

bool IsSpectrumTape(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	using Parser = Storage::Tape::ZXSpectrum::Parser;
	Parser parser(Parser::MachineType::ZXSpectrum);

	while(true) {
		const auto block = parser.find_block(tape);
		if(!block) break;

		// Check for a Spectrum header block.
		if(block->type == 0x00) {
			return true;
		}
	}

	return false;
}

}

Analyser::Static::TargetList Analyser::Static::ZXSpectrum::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	TargetList destination;
	auto target = std::make_unique<Target>();
	target->confidence = 0.5;

	if(!media.tapes.empty()) {
		bool has_spectrum_tape = false;
		for(auto &tape: media.tapes) {
			has_spectrum_tape |= IsSpectrumTape(tape);
		}

		if(has_spectrum_tape) {
			target->media.tapes = media.tapes;
		}
	}

	// If any media survived, add the target.
	if(!target->media.empty())
		destination.push_back(std::move(target));

	return destination;
}

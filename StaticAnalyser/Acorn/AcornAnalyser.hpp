//
//  AcornAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef AcornAnalyser_hpp
#define AcornAnalyser_hpp

#include "../StaticAnalyser.hpp"

namespace StaticAnalyser {
namespace Acorn {

void AddTargets(
	const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
	const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
	const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
	std::list<Target> &destination
);

}
}

#endif /* AcornAnalyser_hpp */

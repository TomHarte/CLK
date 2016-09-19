//
//  Disk.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Acorn_Disk_hpp
#define StaticAnalyser_Acorn_Disk_hpp

#include "File.hpp"
#include "../../Storage/Disk/Disk.hpp"

namespace StaticAnalyser {
namespace Acorn {

struct Catalogue {
	std::string name;
	std::list<File> files;
	enum class BootOption {
		None,
		LoadBOOT,
		RunBOOT,
		ExecBOOT
	} bootOption;
};

std::unique_ptr<Catalogue> GetDFSCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk);
std::unique_ptr<Catalogue> GetADFSCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk);

}
}

#endif /* Disk_hpp */

//
//  DiskController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Components/1770/1770.hpp"

namespace Tandy::CoCo {

class DiskController final: public WD::WD1770 {
public:
	DiskController();

};

}

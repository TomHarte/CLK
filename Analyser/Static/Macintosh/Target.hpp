//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Macintosh_Target_h
#define Analyser_Static_Macintosh_Target_h

#include "../../../Reflection/Enum.h"

ReflectiveEnum(MacintoshModel, int, Mac128k, Mac512k, Mac128ke, MacPlus);

namespace Analyser {
namespace Static {
namespace Macintosh {


struct Target: public ::Analyser::Static::Target {
	MacintoshModel model = MacintoshModel::MacPlus;
};

}
}
}

#endif /* Analyser_Static_Macintosh_Target_h */

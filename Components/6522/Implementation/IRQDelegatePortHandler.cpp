//
//  IRQDelegatePortHandler.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "../6522.hpp"

using namespace MOS::MOS6522;

void IRQDelegatePortHandler::set_interrupt_delegate(Delegate *delegate) {
	delegate_ = delegate;
}

void IRQDelegatePortHandler::set_interrupt_status(bool) {
	if(delegate_) delegate_->mos6522_did_change_interrupt_status(this);
}

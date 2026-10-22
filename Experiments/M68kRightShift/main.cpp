// SPDX-License-Identifier: MIT
#include "Cases.hpp"
#include "InstructionSets/M68k/Perform.hpp"
#include <iostream>

using namespace InstructionSet::M68k;
struct Timing {
    int count=-1, calls=0;
    template<typename T> void did_shift(int n) { count=n; ++calls; }
};
template<typename T, Operation asr, Operation lsr> bool test(const RightShiftTest::Case &c) {
    T result=T(c.value);
    Status status; status.set_status(uint16_t(0x2700 | c.initial_ccr));
    Timing timing;
    if(c.logical) Primitive::shift<lsr>(c.count,result,status,timing);
    else Primitive::shift<asr>(c.count,result,status,timing);
    return result==T(c.result) && status.ccr()==c.ccr &&
        (status.status() & 0xffe0)==0x2700 && timing.calls==1 && timing.count==int(c.count&63);
}
int main() {
    unsigned total=0, failures=0;
    for(bool logical : {false,true}) for(const auto &c : RightShiftTest::cases(logical)) {
        ++total;
        const bool ok = c.width==8 ? test<uint8_t,Operation::ASRb,Operation::LSRb>(c) :
            c.width==16 ? test<uint16_t,Operation::ASRw,Operation::LSRw>(c) :
            test<uint32_t,Operation::ASRl,Operation::LSRl>(c);
        if(!ok) ++failures;
    }
    std::cout << "Right-shift conformance: " << failures << '/' << total << " failures\n";
    return failures ? 1 : 0;
}

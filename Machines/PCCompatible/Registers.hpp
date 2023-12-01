//
//  Registers.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Registers_hpp
#define Registers_hpp

namespace PCCompatible {

struct Registers {
	public:
		static constexpr bool is_32bit = false;

		uint8_t &al()	{	return ax_.halves.low;	}
		uint8_t &ah()	{	return ax_.halves.high;	}
		uint16_t &ax()	{	return ax_.full;		}

		CPU::RegisterPair16 &axp()	{	return ax_;	}

		uint8_t &cl()	{	return cx_.halves.low;	}
		uint8_t &ch()	{	return cx_.halves.high;	}
		uint16_t &cx()	{	return cx_.full;		}

		uint8_t &dl()	{	return dx_.halves.low;	}
		uint8_t &dh()	{	return dx_.halves.high;	}
		uint16_t &dx()	{	return dx_.full;		}

		uint8_t &bl()	{	return bx_.halves.low;	}
		uint8_t &bh()	{	return bx_.halves.high;	}
		uint16_t &bx()	{	return bx_.full;		}

		uint16_t &sp()	{	return sp_;				}
		uint16_t &bp()	{	return bp_;				}
		uint16_t &si()	{	return si_;				}
		uint16_t &di()	{	return di_;				}

		uint16_t &ip()	{	return ip_;				}

		uint16_t &es()		{	return es_;			}
		uint16_t &cs()		{	return cs_;			}
		uint16_t &ds()		{	return ds_;			}
		uint16_t &ss()		{	return ss_;			}
		uint16_t es() const	{	return es_;			}
		uint16_t cs() const	{	return cs_;			}
		uint16_t ds() const	{	return ds_;			}
		uint16_t ss() const	{	return ss_;			}

		void reset() {
			cs_ = 0xffff;
			ip_ = 0;
		}

	private:
		CPU::RegisterPair16 ax_;
		CPU::RegisterPair16 cx_;
		CPU::RegisterPair16 dx_;
		CPU::RegisterPair16 bx_;

		uint16_t sp_;
		uint16_t bp_;
		uint16_t si_;
		uint16_t di_;
		uint16_t es_, cs_, ds_, ss_;
		uint16_t ip_;
};

}

#endif /* Registers_hpp */

#include <iostream>
#include <cstdlib>

/*
	Converter for FUSE-style tests.expected that writes JSON out. Hacky, barely tested,
	not reliable, but seemed to work long enough to produce a JSON object, for which
	robust parsers are widely available.
	
	Intended usage: expectedconverter < tests.in > tests.in.json
*/

int main(void) {
	std::cout << '[';

	bool isFirstObject = true;
	while(true) {
		// Name is always present.
		std::string name;
		std::cin >> name;

		// Exit if a whole test wasn't found.
		// SUPER HACK HERE. I can't be bothered working out why
		if(std::cin.eof() || name == "5505") break;

		// Close previous object, if there was one and output the name.
		if(!isFirstObject) std::cout << "}," << std::endl;
		isFirstObject = false;
		std::cout << "{" << std::endl;
		std::cout << "\t\"name\" : \"" << name << "\"," << std::endl;

		// There are now arbitrarily many events, and at least one.
		//
		// I suspect you're supposed to distinguish the end of events by indentation,
		// but I'm going to do it by string length. Hack attack!
		std::cout << "\t\"busActivity\" : [" << std::endl;
		std::string time;
		bool isFirstEvent = true;
		while(true) {
			std::cin >> time;
			if(time.size() == 4) break;

			std::string type, address, data;
			std::cin >> type >> address;

			if(!isFirstEvent) std::cout << "," << std::endl;
			isFirstEvent = false;

			std::cout << "\t\t{ \"time\" : " << time << ", ";	// Arbitrarily, FUSE switches to base 10 for these numbers.
			std::cout << "\"type\" : \"" << type << "\", ";
			std::cout << "\"address\" : " << strtol(address.c_str(), nullptr, 16);

			// Memory type can be used to determine whether there's a value at the end.
			if(type == "MR" || type == "MW" || type == "PR" || type == "PW") {
				std::string value;
				std::cin >> value;
				std::cout << ", \"value\" : " << strtol(value.c_str(), nullptr, 16);
			}
			std::cout << " }";
		}
		std::cout << std::endl << "\t]," << std::endl;

		// Okay, now for the closing machine state.
		std::string af, bc, de, hl, afDash, bcDash, deDash, hlDash, ix, iy, sp, pc, memptr;
		std::string i, r, iff1, iff2, im, halted, tStates;

		af = time;
		std::cin >> bc >> de >> hl >> afDash >> bcDash >> deDash >> hlDash >> ix >> iy >> sp >> pc >> memptr;
		std::cin >> i >> r >> iff1 >> iff2 >> im >> halted >> tStates;

		// Output the state
		std::cout << "\t\"state\" : {" << std::endl;

#define OUTPUTnbr(name)	std::cout << "\t\t\"" << #name << "\" : " << strtol(name.c_str(), nullptr, 16)
#define OUTPUT(name)	OUTPUTnbr(name) << "," << std::endl
#define OUTPUTb(name)	std::cout << "\t\t\"" << #name << "\" : " << ((name == "0") ? "false" : "true") << "," << std::endl

		OUTPUT(af);
		OUTPUT(bc);
		OUTPUT(de);
		OUTPUT(hl);
		OUTPUT(afDash);
		OUTPUT(bcDash);
		OUTPUT(deDash);
		OUTPUT(hlDash);
		OUTPUT(ix);
		OUTPUT(iy);
		OUTPUT(sp);
		OUTPUT(pc);
		OUTPUT(memptr);
		OUTPUT(i);
		OUTPUT(r);
		OUTPUTb(iff1);
		OUTPUTb(iff2);
		OUTPUT(im);
		OUTPUTb(halted);
		OUTPUTnbr(tStates) << std::endl;
		
#undef OUTPUTb
#undef OUTPUT
#undef OUTPUTnbr
		std::cout << "\t}";


		// A memory list may or may not follow. If it does it'll be terminated
		// in the usual way. If not, it just won't be there. Hassle!
		char nextChar;
		std::cin.rdbuf()->sbumpc();
		nextChar = std::cin.rdbuf()->sgetc();
		if(nextChar == '\n') {
			std::cout << std::endl;
			continue;
		}

		// Parse and transcode the memory list.
		// There's only ever one memory block.
		std::cout << "," << std::endl;
		std::cout << "\t\"memory\" : [" << std::endl;

		std::string address;
		std::cin >> address;

		std::cout << "\t\t{ \"address\" : " << strtol(address.c_str(), nullptr, 16) << ", \"data\" : [";
		bool isFirstValue = true;
		while(true) {
			std::string value;
			std::cin >> value;
			if(value == "-1") break;

			if(!isFirstValue) std::cout << ", ";
			isFirstValue = false;

			std::cout << strtol(value.c_str(), nullptr, 16);
		}
		std::cout << "] }";

		// Close the object.
		std::cout << std::endl << "\t]" << std::endl;
	}

	std::cout << "}]" << std::endl;
	
	return 0;
}

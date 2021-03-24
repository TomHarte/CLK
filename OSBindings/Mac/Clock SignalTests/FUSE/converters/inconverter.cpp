#include <iostream>
#include <cstdlib>

/*
	Converter for FUSE-style tests.in that writes JSON out. Hacky, barely tested, not reliable,
	but seemed to work long enough to produce a JSON object, for which robust parsers are
	widely available.
	
	Intended usage: inconverter < tests.in > tests.in.json
*/

int main(void) {
	std::cout << '[';

	bool isFirstObject = true;
	while(true) {
		std::string name;
		std::string af, bc, de, hl, afDash, bcDash, deDash, hlDash, ix, iy, sp, pc, memptr;
		std::string i, r, iff1, iff2, im, halted, tStates;

		// Read the fixed part of this test.
		std::cin >> name;
		std::cin >> af >> bc >> de >> hl >> afDash >> bcDash >> deDash >> hlDash >> ix >> iy >> sp >> pc >> memptr;
		std::cin >> i >> r >> iff1 >> iff2 >> im >> halted >> tStates;

		// Exit if a whole test wasn't found.		
		if(std::cin.eof()) break;

		if(!isFirstObject) std::cout << "}," << std::endl;
		isFirstObject = false;

		// Output that much.
		std::cout << "{" << std::endl;
		std::cout << "\t\"name\" : \"" << name << "\"," << std::endl;
		std::cout << "\t\"state\" : {" << std::endl;

#define OUTPUTnbr(name)	std::cout << "\t\t\"" << #name << "\" : " << strtol(name.c_str(), nullptr, 16)
#define OUTPUT10(name)	std::cout << "\t\t\"" << #name << "\" : " << name
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
		OUTPUT10(tStates) << std::endl;
		
#undef OUTPUTb
#undef OUTPUT
#undef OUTPUTnbr

		std::cout << "\t}," << std::endl;
		
		// Parse and transcode the memory list.
		std::cout << "\t\"memory\" : [" << std::endl;

		bool isFirstBlock = true;
		while(true) {
			std::string address;
			std::cin >> address;
			if(address == "-1") break;
			
			if(!isFirstBlock) std::cout << "," << std::endl;
			isFirstBlock = false;
			
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
		}

		// Close the object.
		std::cout << std::endl << "\t]" << std::endl;
	}

	std::cout << "}]" << std::endl;
	
	return 0;
}

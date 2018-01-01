//
//  AtariStaticAnalyserTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 11/03/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#import <CommonCrypto/CommonDigest.h>
#include "../../../StaticAnalyser/StaticAnalyser.hpp"

@interface MSXROMRecord : NSObject
@property(nonatomic, readonly) StaticAnalyser::MSXCartridgeType cartridgeType;
+ (instancetype)recordWithCartridgeType:(StaticAnalyser::MSXCartridgeType)cartridgeType;
@end

@implementation MSXROMRecord
+ (instancetype)recordWithCartridgeType:(StaticAnalyser::MSXCartridgeType)cartridgeType {
	MSXROMRecord *record = [[MSXROMRecord alloc] init];
	record->_cartridgeType = cartridgeType;
	return record;
}
@end

#define Record(sha, type) sha : [MSXROMRecord recordWithCartridgeType:StaticAnalyser::MSXCartridgeType::type],
static NSDictionary<NSString *, MSXROMRecord *> *romRecordsBySHA1 = @{
	Record(@"da397e783d677d1a78fff222d9d6cb48b915dada", RType) // 1942 (1986)(ASCII)(JP).rom
	Record(@"0733cd627467a866846e15caf1770a5594eaf4cc", RType) // 1942 (1986)(ASCII)(JP)[a].rom
	Record(@"ba07b1b585386f887d4c7e457210b3fce819709a", RType) // 1942 (1987)(Zemina)(KR).rom
	Record(@"dd1e87a16e5fb38d9d729ef7edc6da21146a99fe", RType) // 1942 (1987)(Zemina)(KR)[a].rom
	Record(@"6bde4e6761286a2909858ecef04155e17072996e", RType) // A Life M36 Planet - MotherBrain has Been Aliving (1987)(Pixel)(JP).rom
	Record(@"937464eb371c68add2236bcef91d24a8ce7c4ed1", RType) // A1 Spirit - The Way to Formula 1 (1987)(Konami)(JP).rom
	Record(@"2639792df6f7c7cfaffc2616b0e1849f18897ace", RType) // Aliens. Alien 2 (1987)(Square)(JP).rom
	Record(@"0d9c472cf7687b86f3fe2e5af6545a26a0efd5fc", RType) // Aliens. Alien 2 (1987)(Square)(JP)[a2].rom
	Record(@"5380e913d8dac23470446844cab21f6921101af8", RType) // Aliens. Alien 2 (1987)(Square)(JP)[a].rom
	Record(@"92a0ca682a367cbc1c6c5f96f8b84619a43e9f12", RType) // Aliens. Alien 2 (1987)(Square)(JP)[b].rom
	Record(@"db33011d006201b3bd3bbc4c7c952da2990f36e4", RType) // Animal Land Murder Case (1987)(Enix)(JP).rom
	Record(@"495876c504bdc4de24860ea15b25dda8f3b06c49", RType) // Animal Land Murder Case (1987)(Enix)(JP)[a].rom
	Record(@"72815a7213f899a454bcf76733834ba499d35cd8", RType) // Astro Marine Corps (1989)(Dinamic Software)(ES).rom
	Record(@"709fb35338f21897e275237cc4c5615d0a5c2753", RType) // Batman (1986)(Pack In Video)(JP).rom
	Record(@"3739d841abfef971db76ba10915b19b9df833476", RType) // Batman (1986)(Pack In Video)(JP)[a].rom
	Record(@"2588b9ade775b93f03fd4b17fd3f78ba70b556d6", RType) // Black Onyx II, The - Search for the Fire Crystal (1986)(ASCII)(JP).rom
	Record(@"482cd650220e6931f85ee8532c61dac561365e30", RType) // Bomber King (1988)(Hudson Soft)(JP).rom
	Record(@"16c3ced0fb2e360bc7c43d372a0a30eb6bd3963d", RType) // Borfesu. Bolu Fez and Five Evil Spirits (1987)(XTalSoft)(JP).rom
	Record(@"bcfdb43b275c62a76856aab94fb1b53bd92b2db2", RType) // Break In (1987)(Jaleco)(JP).rom
	Record(@"16c692a2c9babfdadd8408d2f0f8fae3a8d96fd5", RType) // Cosmic Soldier 2 - Psychic War (1987)(Kogado)(JP).rom
	Record(@"a731d3d3b5badf33c7602febd32cc4e6ec98c646", RType) // Craze (1988)(Heart Soft)(JP).rom
	Record(@"9e0312e72f30a20f556b64fe37dbbfe0d4471823", RType) // Craze (1988)(Heart Soft)(JP)[a].rom
	Record(@"f0d74d951a3781c7a2680e1d7ab809d2afe1fd18", RType) // Crazy Train (1983)(Sony)(JP)[o].rom
	Record(@"25b28cfe8d6d51f619d182c774f6ceb05b577eeb", RType) // Cross Blaim (1986)(dB-Soft)(JP).rom
	Record(@"bb902e82a2bdda61101a9b3646462adecdd18c8d", RType) // Cross Blaim (1986)(dB-Soft)(JP)[b].rom
	Record(@"1833ffc252d43d3d8239e57d5ac2c015b4367988", RType) // Daiva Story 4 - Asura's Bloodfeud (1987)(T&E Soft)(JP).rom
	Record(@"2418c0302abbce8b0f8556b63169c60a849f60ee", RType) // Daiva Story 4 - Asura's Bloodfeud (1987)(T&E Soft)(JP)[a].rom
	Record(@"6856a13b67750d36c4c466d30472dc5465879bbf", RType) // Dawn Patrol (1986)(Pony Canyon)(JP).rom
	Record(@"e6419519c2d3247ea395e4feaa494a2e23e469ce", RType) // Deep Dungeon (1988)(Scaptrust)(JP).rom
	Record(@"d82135a5e28b750c44995af116db890a15f6428a", RType) // Deep Dungeon II (1988)(Scaptrust)(JP).rom
	Record(@"63d4e39c59f24f880809caa534d7a46ae83f4c9f", RType) // Demon Crystal Saga II - Knither Special (1987)(Radio Wave Newspaper Publisher)(JP).rom
	Record(@"d5b164797bc969b55c1a6f4006a4535c3fb03cf0", RType) // Demonia (1986)(Microids)(GB)(fr).rom
	Record(@"7e9a9ce7c18206b325830e9cdcbb27179118de96", RType) // Dragon Quest (1986)(Enix)(JP).rom
	Record(@"7b94a728a5945a53d518c18994e1e09a09ec3c1b", RType) // Dragon Quest (1986)(Enix)(JP)[a].rom
	Record(@"3df7c19f739d74d6efdfd8151343e5a55d4ac842", RType) // Dragon Quest II (1988)(Enix)(JP).rom
	Record(@"68691348a29ce59046f993e9abaf3c8651bdda3c", RType) // Dragon Quest II (1988)(Enix)(JP)[a2].rom
	Record(@"d7b46aece68c924e09f07e3df45711f337d35d6a", RType) // Dragon Quest II (1988)(Enix)(JP)[a].rom
	Record(@"6c1814c70d69a50ec60e39ef281f0b8cd7bf8598", RType) // Dragon Slayer 2 - Xanadu (1987)(Falcom)(JP).rom
	Record(@"fcdbc5e15dd6b973e0f1112f4599dad985f48042", RType) // Dragon Slayer 2 - Xanadu (1987)(Zemina)(KR).rom
	Record(@"2f0db48fbcf3444f52b9c7c76ba9c4bd38bc2a15", RType) // Dragon Slayer 3 - Romancia. Dragon Slayer Jr (1987)(Falcom)(JP).rom
	Record(@"a52c37c1f16ba13d3f39bb5403c82c0187cbff51", RType) // Dragon Slayer 3 - Romancia. Dragon Slayer Jr (1987)(Falcom)(JP)[b].rom
	Record(@"f100a76117e95ab0335e89a901e47d844bbc0ab6", RType) // Dragon Slayer 4 - Drasle Family (1987)(Falcom)(JP).rom
	Record(@"d1fbdbdf2e830139584d7dc796806aa3327720dd", RType) // Dungeon Hunter (1989)(ASCII)(JP).rom
	Record(@"2799d221d5486d48226bbbd3941207e1fc7c985e", RType) // Dustin (1987)(Dinamic Software)(ES).rom
	Record(@"d7109cf20a22558f923c833ff0b4e2311340acb1", RType) // Dynamite Bowl (1988)(Toshiba-EMI)(JP).rom
	Record(@"e92baa5fdfb2715e68700024d964098ef35704d9", RType) // Eggerland Mystery 2. Meikyushinwa. Labyrinth Myth (1986)(HAL Laboratory)(JP).rom
	Record(@"98b7a6ac44b82ccfc45eb51595e2905adabac1c7", RType) // Eggerland Mystery 2. Meikyushinwa. Labyrinth Myth (1986)(HAL Laboratory)(JP)[a].rom
	Record(@"c335e8382fee4ba1bfbd2f4cdd99b8222f3e910a", RType) // Eggerland Mystery 2. Meikyushinwa. Labyrinth Myth (1986)(HAL Laboratory)(JP)[b].rom
	Record(@"cf344e0f58fd918c089c4d4575caec8843944ff6", RType) // Eggerland Mystery 2. Meikyushinwa. Labyrinth Myth (1986)(HAL Laboratory)(JP)[h THEVMA].rom
	Record(@"ff72a1788d47f9876d9fabd720f6a289fb409090", RType) // F-1 Spirit - The Way to Formula 1 (1987)(Konami)(JP)[a][RC-752].rom
	Record(@"42fbb18722df3e34e5b0f935a2dc0ce0d85099e9", RType) // F-1 Spirit - The Way to Formula 1 (1987)(Konami)(JP)[RC-752].rom
	Record(@"3880b064dcb851ca221ff67e435137a7cf1141f8", RType) // F-1 Spirit - The Way to Formula 1 (1987)(Konami)(JP)[SCC][RC-752].rom
	Record(@"6e5acdfb1c1610257a7aabf3d5aa858866dbcf2e", RType) // F-1 Spirit - The Way to Formula 1 (1987)(Zemina)(KR)[RC-752].rom
	Record(@"84566b5f37ab4f04a2e5b950c5beecbd27b88ea0", RType) // Fairyland Story, The (1987)(Hot-B)(JP).rom
	Record(@"e11afc03db4e1d03976d02796b29da9c65d4ff3d", RType) // Fairyland Story, The (1987)(Hot-B)(JP)[a2].rom
	Record(@"2bb837a9051277ba574d8351a9f91f9e57033074", RType) // Fairyland Story, The (1987)(Hot-B)(JP)[a].rom
	Record(@"048737f995eecb1dd8dd341d750efd005267796f", RType) // Fantasy Zone (1986)(Pony Canyon)(JP).rom
	Record(@"442a39b196f19a22fc7fb8f14bf17386a292b60e", RType) // Fantasy Zone (1987)(Zemina)(KR).rom
	Record(@"a553c3f204c105c23b227a1e5aeb290671ccdbeb", RType) // Final Zone Wolf (1986)(Telenet Japan)(JP).rom
	Record(@"90ea059c57f011a4fb33a558e868ee639882fe5e", RType) // Final Zone Wolf (1986)(Zemina)(KR).rom
	Record(@"488f01314da5644fbf2ca938c6f9720ef2b0d0f9", RType) // Flight Deck II (1986)(Pony Canyon)(JP).rom
	Record(@"3eee7c72362522c27866d4367a8cf1e22932db32", RType) // Flight Deck II (1986)(Pony Canyon)(JP)[a].rom
	Record(@"2bd311a4baf59cb85b839cca1f55b7462aa96952", RType) // Flight Simulator (1986)(subLOGIC)(JP).rom
	Record(@"453eac7568d5d04c8cf7da86f2e8dc79777343da", RType) // Flight Simulator (1986)(subLOGIC)(JP)[a].rom
	Record(@"abe52920e895c148851649ecb9c029fdc41a275f", RType) // Gall Force - Defense of Chaos (1986)(Sony)(JP).rom
	Record(@"e8ab46785e75e19dac497c4a82ac2f5b37ac0580", RType) // Gall Force - Defense of Chaos (1986)(Sony)(JP)[a2].rom
	Record(@"3425eea336140da03d7d7f09a94fd928d70e2212", RType) // Gall Force - Defense of Chaos (1986)(Sony)(JP)[a].rom
	Record(@"89073c052b0fe29b6de077c8bdf5373474081edf", RType) // Gambler Jikichushinpa (1988)(Game Arts)(JP).rom
	Record(@"12b3c31f0fd10ff5823dcc8bf6dfeb785a8af2f7", RType) // Genghis Khan (1986)(Koei)(JP).rom
	Record(@"0413bb3aeacb0c28429b8c85b42796dbe48bef6d", RType) // Gofer no Yabou Episode II. Nemesis 3 - The Eve of Destruction (1988)(Konami)(JP)[a2][RC-764].rom
	Record(@"7393f677e0fae5fc83071c6b74756117b7d75e2d", RType) // Gofer no Yabou Episode II. Nemesis 3 - The Eve of Destruction (1988)(Konami)(JP)[a][RC-764].rom
	Record(@"40a0f35dccc7572ae53bcd4be70abfe477d49bc9", RType) // Gofer no Yabou Episode II. Nemesis 3 - The Eve of Destruction (1988)(Konami)(JP)[o][b][RC-764].rom
	Record(@"5692e41b3a4c5e767cf290fd6c24942d0fd7b2e3", RType) // Gofer no Yabou Episode II. Nemesis 3 - The Eve of Destruction (1988)(Konami)(JP)[RC-764].rom
	Record(@"f416b424fb913ca067fe75279c924a20fac5c6a1", RType) // Gofer no Yabou Episode II. Nemesis 3 - The Eve of Destruction (1988)(Konami)(JP)[SCC][RC-764].rom
	Record(@"520739caa1e0aac1b8eabc4305556aa75f3f5a3b", RType) // Gofer no Yabou Episode II. Nemesis 3 - The Eve of Destruction (1988)(Konami)(JP)[t][RC-764].rom
	Record(@"7a4126934f9e68c34bf00dd3d9a9e753c05ee73f", RType) // Golvellius (1987)(Compile)(JP)[a].rom
	Record(@"91505fccdcc43230550d101967010bed27f9b573", RType) // Golvellius (1987)(Compile)(JP)[o].rom
	Record(@"930df58762e7b5bcf2d362462f03335bad732398", RType) // Golvellius (1987)(Compile).rom
	Record(@"4c2f015685a17db7a8c3893e868e0a84a8dbf1e5", RType) // Gradius 2. Nemesis 2 (1987)(Konami)(beta)[a][RC-751].rom
	Record(@"6844758ff5c2c410115d4d7cf12498c42e931732", RType) // Gradius 2. Nemesis 2 (1987)(Konami)(beta)[RC-751].rom
	Record(@"d63e20369f98487767810a0c57603bef6a2a07e5", RType) // Gradius 2. Nemesis 2 (1987)(Konami)(JP)[a][RC-751].rom
	Record(@"c66483cd0d83292e4f2b54a3e89bd96b8bf9abb2", RType) // Gradius 2. Nemesis 2 (1987)(Konami)(JP)[penguin version][RC-751].rom
	Record(@"ab30cdeaacbdf14e6366d43d881338178fc665cb", RType) // Gradius 2. Nemesis 2 (1987)(Konami)(JP)[RC-751].rom
	Record(@"fd7b23a4f1c2058b966b5ddd52cf7ae44a0eebb0", RType) // Gradius 2. Nemesis 2 (1987)(Konami)(JP)[SCC][RC-751].rom
	Record(@"4127844955388f812e33437f618936dc98944c0a", RType) // Gradius 2. Nemesis 2 (1987)(Konami)(JP)[t][RC-751].rom
	Record(@"50efb7040339632cf8bddbc1d3eaae1fb2e2188f", RType) // Gradius. Nemesis (1986)(Konami)(JP)[a][RC-742].rom
	Record(@"f0e4168ea18188fca2581526c2503223b9a28581", RType) // Gradius. Nemesis (1986)(Konami)(JP)[a][SCC][RC-742].rom
	Record(@"f2854b7198f325691a5ad3129bd05b44d6120771", RType) // Gradius. Nemesis (1986)(Konami)(JP)[b][SCC][RC-742].rom
	Record(@"1434386454c776800cba3805c4041b0105c2f71f", RType) // Gradius. Nemesis (1986)(Konami)(JP)[SCC+][RC-742].rom
	Record(@"e31ac6520e912c27ce96431a1dfb112bf71cb7b9", RType) // Gradius. Nemesis (1986)(Konami)(JP)[SCC][RC-742].rom
	Record(@"98748c364e7bff50cf073c1a421ebe5b5d8b7025", RType) // Gradius. Nemesis (1986)(Konami)(JP)[t][SCC][RC-742].rom
	Record(@"44b23a175d04ca21236a5fef18600b01b12aaf4d", RType) // Haja no Fuin (1987)(Kogado)(JP).rom
	Record(@"a3de07612da7986387a4f5c41bbbc7e3b244e077", RType) // Harry Fox MSX Special (1986)(Micro Cabin)(JP).rom
	Record(@"3626b5dd3188ec2a16e102d05c79f8f242fbd892", RType) // Harry Fox Yki no Maoh (1985)(Micro Cabin)(JP).rom
	Record(@"26a7b0118d158e9bd3ea947fbe68f57b54e0b847", RType) // Head over Heels (1987)(Ocean Software)(GB).rom
	Record(@"da4b44c734029f60388b7cea4ab97c3d5c6a09e9", RType) // Hydlide II - Shine of Darkness (1986)(T&E Soft)(JP).rom
	Record(@"a3537934a4d9dfbf27aca5aaf42e0f18e4975366", RType) // Hydlide II - Shine of Darkness (1986)(T&E Soft)(JP)[a].rom
	Record(@"b18d36cc60d0e3b325138bb98472b685cca89f90", RType) // Hydlide II - Shine of Darkness (1986)(T&E Soft)(JP)[b].rom
	Record(@"74e9ea381e2fed07d989d1056002de5737125aaf", RType) // Hydlide III - The Space Memories (1987)(T&E Soft)(JP).rom
	Record(@"5d2062ecc7176ca2b100724c2a17f877878d721d", RType) // Hydlide III - The Space Memories (1987)(Zemina)(KR).rom
	Record(@"842009e0f7d0e977e47be7a56fe60707f477ed93", RType) // Jagur (1987)(Hudson Soft)(JP).rom
	Record(@"6fefbe448674b6ea846d0c6b9c8a0d57a11aa410", RType) // Jagur (1987)(Hudson Soft)(JP)[a2].rom
	Record(@"1aeae1180471e9a6e8e866993031b881a341f921", RType) // Jagur (1987)(Hudson Soft)(JP)[a3].rom
	Record(@"3b6200f88561a59ae5d2b3e94515b89cdb96044b", RType) // Jagur (1987)(Hudson Soft)(JP)[a].rom
	Record(@"1c462c3629d43297a006ba9055b39a2dccba9f6c", RType) // Karuizawa Kidnapping Guidance, The (1986)(Enix)(JP).rom
	Record(@"46062d3393c49884f84c2dc437ff27854e9d2e49", RType) // King's Knight (1986)(Square)(JP).rom
	Record(@"122f659250a0ae10ce0be0dde626dd3e384affa7", RType) // King's Knight (1986)(Square)(JP)[a2].rom
	Record(@"a2ca7e6e216f8b450eb8db10a4120f0353275b6b", RType) // King's Knight (1986)(Square)(JP)[a3].rom
	Record(@"438bbb3367db4938b3d90fa9d2cfb1f08c072bb7", RType) // King's Knight (1986)(Square)(JP)[a].rom
	Record(@"3af846020be32c20d705c4fbfb6f51d24a094be3", RType) // King's Knight (1986)(Square)(JP)[b].rom
	Record(@"cfd872d005b7bd4cdd6e06c4c0162191f0b0415d", RType) // King's Valley II - The Seal of El Giza (1988)(Konami)(JP)[RC-760].rom
	Record(@"f3306e6f25d111da21ce66db3404f5f48acb25a1", RType) // King's Valley II - The Seal of El Giza (1988)(Konami)(JP)[SCC][RC-760].rom
	Record(@"ee60e88ae409ddd93d4259b79586dacb2e5ee372", RType) // Knightmare II - The Maze of Galious (1987)(Konami)(JP)[a][RC-749].rom
	Record(@"4d51d3c5036311392b173a576bc7d91dc9fed6cb", RType) // Knightmare II - The Maze of Galious (1987)(Konami)(JP)[RC-749].rom
	Record(@"7a786959d8fc0b518b35341422f096dd6019468d", RType) // Knightmare II - The Maze of Galious (1987)(Zemina)(KR)[RC-749].rom
	Record(@"f999e0187023413d839a67337d0595e150b2398f", RType) // Knightmare III - Shalom (1987)(Konami)(JP)[a2][RC-754].rom
	Record(@"3b6f140c99cba5bfa510200df49d1e573d687e4d", RType) // Knightmare III - Shalom (1987)(Konami)(JP)[a3][RC-754].rom
	Record(@"240e15fb5d918aa821f226002772dc800d9f20a4", RType) // Knightmare III - Shalom (1987)(Konami)(JP)[a][RC-754].rom
	Record(@"25f5adeca8a2ddb754d25eb18cff0d84e5b003bc", RType) // Knightmare III - Shalom (1987)(Konami)(JP)[RC-754].rom
	Record(@"8c609b8bee4245a1bb81e37d888ac5efb66533cf", RType) // Knightmare III - Shalom (1987)(Konami)(JP)[tr pt R. Bittencourt][RC-754].rom
	Record(@"6a98d5787dd0e76f04283ce0aec55d45ba81565d", RType) // Legendly Knight (1988)(Topia)(KR).rom
	Record(@"2cea9cdd501d77f2b6bd78ae2ae9a63aba64cfed", RType) // Legendly Knight (1988)(Topia)(KR)[a].rom
	Record(@"0442dd29ea0f7b78c9cd5849ec7ef5bb21ec0bf5", RType) // Light Corridor, The (1990)(Infogrames)(FR).rom
	Record(@"3243a5cc562451de527917e9ca85657286b2852f", RType) // Magunam. Kiki Ippatsu. Magnum Prohibition 1931 (1988)(Toshiba-EMI)(JP).rom
	Record(@"9c5c1c40ec30c34b1b436cf8cc494c0b509e81fc", RType) // Magunam. Kiki Ippatsu. Magnum Prohibition 1931 (1988)(Toshiba-EMI)(JP)[a].rom
	Record(@"510e1fa5cdd845082c1881d83ed8cf7c4ae70a3c", RType) // Magunam. Kiki Ippatsu. Magnum Prohibition 1931 (1988)(Toshiba-EMI)(JP)[b].rom
	Record(@"f7dd6841d280cbffa9f0f2da7af3549f23270ddb", RType) // Marchen Veil (1986)(System Sacom)(JP).rom
	Record(@"8f92ea214a7a11394c512080d3b65a7afa9a92ed", RType) // Mashou no Yakata Gabalin. Goblin (1987)(Pony Canyon)(JP).rom
	Record(@"e4d61ee74867066b04b0f6aba35d98d72665e6e0", RType) // Mashou no Yakata Gabalin. Goblin (1987)(Pony Canyon)(JP)[a].rom
	Record(@"0cb11c766bd357d203879bd6bee041a4690cc3df", RType) // Meikyuu no Tobira. Gate of Labyrinth (1987)(Radio Wave Newspaper Publisher)(JP).rom
	Record(@"7abf89652396a648a84ae06e6dabc09735a75798", RType) // Mirai. Future (1987)(Xain)(JP).rom
	Record(@"7341efc039394ec159feebcfaa9d4a61ebf08a18", RType) // Mitsume ga Tooru. The Three-Eyed One Comes Here (1989)(Natsume)(JP).rom
	Record(@"176ec8e65a9fdbf59edc245b9e8388cc94195db9", RType) // Mitsume ga Tooru. The Three-Eyed One Comes Here (1989)(Natsume)(JP)[a2].rom
	Record(@"0ec71916791e05d207d7fe0a461a79a76eab52c5", RType) // Mitsume ga Tooru. The Three-Eyed One Comes Here (1989)(Natsume)(JP)[a].rom
	Record(@"03b42b77b1a7412f1d7bd0998cf8f2f003f77d0a", RType) // Monogatari Megami Tensei. Digital Devil Story (1987)(Telenet Japan)(JP).rom
	Record(@"7dc7f7e3966943280f34836656a7d1bd3ace67cd", RType) // Monogatari Megami Tensei. Digital Devil Story (1987)(Telenet Japan)(JP)[a].rom
	Record(@"e6de8bbd60123444de2a90928853985ceb0b4cbf", RType) // Nobunaga no Yabou - Zenkoku Han (1987)(Koei)(JP).rom
	Record(@"75d3b72d9ceeaa55c76223d935629a30ae4124d6", RType) // Parodius - Tako Saves Earth (1988)(Konami)(JP)[a][RC-759].rom
	Record(@"7c066cb763f7a4fec0474b5a09e3ef43bbf9248b", RType) // Parodius - Tako Saves Earth (1988)(Konami)(JP)[a][SCC][RC-759].rom
	Record(@"2220363ae56ef707ab2471fcdb36f4816ad1d32c", RType) // Parodius - Tako Saves Earth (1988)(Konami)(JP)[RC-759].rom
	Record(@"0b02dda5316a318a7f75a811aa54200ddd7abc30", RType) // Parodius - Tako Saves Earth (1988)(Konami)(JP)[SCC][RC-759].rom
	Record(@"2111cec4b5ea698d772bb80664f6be690b47391c", RType) // Parodius - Tako Saves Earth (1988)(Konami)(JP)[t][RC-759].rom
	Record(@"c95f8edb24edca9f5d38c26d0e8a34c6b61efb0c", RType) // Pinball Blaster (1988)(Eurosoft)(NL).rom
	Record(@"37bd4680a36c3a1a078e2bc47b631d858d9296b8", RType) // R-Type (1988)(IREM)(JP).rom
	Record(@"9c886fff02779267041efe45dadefc5fd7f4b9a2", RType) // R-Type v2 (1988)(IREM)(JP).rom
	Record(@"0b379610cb7085005a56b24a0890b79dd5a7e817", RType) // R-Type v2 (1988)(IREM)(JP)[a].rom
	Record(@"3856d9d6b7af383a821bd564396fef5f13154e10", RType) // readme.txt
	Record(@"74ae85d44cb8ef1bae428c90200cb74be6d56d3a", RType) // Relics (1986)(Bothtec)(JP).rom
	Record(@"46c98a3143f5a80b2090311a770f9b73000881c0", RType) // Robowres 2001 (1987)(Micronet)(JP).rom
	Record(@"3bc699c0b789939140f6cc35f2732080f80feaa3", RType) // Robowres 2001 (1988)(Zemina)(KR)[b].rom
	Record(@"0d459788b6c464b50cbc2436e67a2cef248e0c4a", RType) // Salamander - Operation X (1987)(Konami)(JP)[RC-758].rom
	Record(@"0d76a069726fec7326541f75b809b8b72148ed3a", RType) // Salamander - Operation X (1987)(Konami)(JP)[SCC][RC-758].rom
	Record(@"655b15e8fd81866492bcf2b1d6609211b30efce1", RType) // Salamander - Operation X (1987)(Konami)(JP)[t][RC-758].rom
	Record(@"d30c17109fa1c4a81e39dca57b79464b0aa0b7c2", RType) // Salamander - Operation X (1988)(Zemina)(KR)[RC-758].rom
	Record(@"96e2a7163c755fbea77abfd9d09a798687a5a993", RType) // Sangokushi. Romance of Three Kingdoms (1986)(Koei)(JP).rom
	Record(@"3f741ba2ab08c5e9fb658882b36b8e3d01682f58", RType) // Sangokushi. Romance of Three Kingdoms (1986)(Koei)(JP)[a2].rom
	Record(@"7ad40ae512bbf4ba688467ba23e16354b8421d0a", RType) // Sangokushi. Romance of Three Kingdoms (1986)(Koei)(JP)[a3].rom
	Record(@"14ff6fe464362c6b7dbb47b2ecda3a8c5f05ef79", RType) // Sangokushi. Romance of Three Kingdoms (1986)(Koei)(JP)[a].rom
	Record(@"2b255c3e5615b2f5e2365419a35e4115a060e93c", RType) // Senjyo no Ookami. Wolf's Battlefield. Commando (1987)(ASCII)(JP).rom
	Record(@"5a12439f74ca5d1c2664f01ff6a8302d3ce907a8", RType) // Sofia (1988)(Radio Wave Newspaper Publisher)(JP).rom
	Record(@"3caeec19423a960d98e4719b301a3d276339e5ae", RType) // Sofia (1988)(Radio Wave Newspaper Publisher)(JP)[o].rom
	Record(@"9bafce699964f4aabc6b60c71a71c9ff5b0cc82d", RType) // Super Boy III (1991)(Zemina)(KR).rom
	Record(@"2dfbca8f5cc3a9e9d151382ebe0da410f5393eaf", RType) // Super Laydock - Mission Striker (1987)(T&E Soft)(JP).rom
	Record(@"244399d67d7851f3daa9bb87a14f5b8ef6d8c160", RType) // Super Laydock - Mission Striker (1988)(Zemina)(KR).rom
	Record(@"0d2f86dbb70f4b4a4e4dc1bc95df232f48856037", RType) // Super Pierrot (1988)(Nidecom)(JP).rom
	Record(@"16351e6a7d7fc38c23b54ee15ac6f0275621ba96", RType) // Syougun. Shigun (1987)(Nippon Dexter)(JP).rom
	Record(@"d147f2cac0600527ce49bfffc865c54eb783e5e5", RType) // Toobin' (1989)(Domark)(GB).rom
	Record(@"bfe8046f8ccc6d6016d7752c04f0654420ef81e7", RType) // Tumego 120 (1987)(Champion Soft)(JP).rom
	Record(@"39a91558bba92b7310455cd7513276d487de0f9d", RType) // Twin Bee (1986)(Konami)(JP)[a][SCC+][RC-740].rom
	Record(@"b813123219446f16cd10b916cc18ec4a8d7c69de", RType) // Twin Bee (1986)(Konami)(JP)[SCC+][RC-740].rom
	Record(@"2b10234debd2a6a9a02e0750ba6563768bc4a2f3", RType) // Valis - The Fantasm Soldier (1986)(Telenet Japan)(JP).rom
	Record(@"dccdb2d18c70a94e48b3ec5e0cb986c5d708bbc9", RType) // Valis - The Fantasm Soldier (1987)(Zemina)(KR).rom
	Record(@"4340a2580c949d498d2c8e71699fff860214e9ea", RType) // Vaxol (1987)(Heart Soft)(JP).rom
	Record(@"818d91505ad39bba2eaf7f4857c7d41e95fcb233", RType) // Wing Man 2 (1987)(Enix)(JP).rom
	Record(@"b6a5552effcee708b665fa74e5ce7b0fa2541c03", RType) // Young Sherlock - The Legacy of Doyle (1985)(Pack In Video)(JP).rom
	Record(@"97e173dac64dbde7d6a60de7606cba0c860813db", RType) // Young Sherlock - The Legacy of Doyle (1985)(Pack In Video)(JP)[a].rom
	Record(@"d0706fd10e418eba2929d515cc0994f49376a63f", RType) // Yume Tairiku Adventure. Penguin Adventure (1986)(Konami)(JP)[a2][RC-743].rom
	Record(@"d53e0c8bcd98820afe820f756af35cc97911bfe4", RType) // Yume Tairiku Adventure. Penguin Adventure (1986)(Konami)(JP)[a][RC-743].rom
	Record(@"898bda19f882c6d1dffdb2173db84a97dc21f7d6", RType) // Yume Tairiku Adventure. Penguin Adventure (1986)(Konami)(JP)[cr Screen][RC-743].rom
	Record(@"fa6c059e14092d023b1f9f2df28e482f966287db", RType) // Yume Tairiku Adventure. Penguin Adventure (1986)(Konami)(JP)[RC-743].rom
	Record(@"d3d411c8b7891aef9c59cbc20bb4fa3ff0ca03ea", RType) // Yume Tairiku Adventure. Penguin Adventure (1987)(Zemina)(KR)[RC-743].rom
};
#undef Record

@interface MSXStaticAnalyserTests : XCTestCase
@end

@implementation MSXStaticAnalyserTests

- (void)testROMs {
	NSString *basePath = [[[NSBundle bundleForClass:[self class]] resourcePath] stringByAppendingPathComponent:@"MSX ROMs"];
	for(NSString *testFile in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:basePath error:nil]) {
		NSString *fullPath = [basePath stringByAppendingPathComponent:testFile];

		// get a SHA1 for the file
		NSData *fileData = [NSData dataWithContentsOfFile:fullPath];
		uint8_t sha1Bytes[CC_SHA1_DIGEST_LENGTH];
		CC_SHA1([fileData bytes], (CC_LONG)[fileData length], sha1Bytes);
		NSMutableString *sha1 = [[NSMutableString alloc] init];
		for(int c = 0; c < CC_SHA1_DIGEST_LENGTH; c++) [sha1 appendFormat:@"%02x", sha1Bytes[c]];

		// get an analysis of the file
		std::list<StaticAnalyser::Target> targets = StaticAnalyser::GetTargets([fullPath UTF8String]);

		// grab the ROM record
		MSXROMRecord *romRecord = romRecordsBySHA1[sha1];
		if(!romRecord) {
			printf("\tRecord(@\"%s\", RType) // %s\n", sha1.UTF8String, testFile.UTF8String);
			continue;
		}

		// assert equality
		XCTAssert(targets.front().msx.paging_model == romRecord.cartridgeType, @"%@; should be %d, is %d", testFile, romRecord.cartridgeType, targets.front().msx.paging_model);
	}
}

@end

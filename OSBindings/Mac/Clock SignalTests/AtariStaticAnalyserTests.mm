//
//  AtariStaticAnalyserTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 11/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#import <CommonCrypto/CommonDigest.h>
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../../Analyser/Static/Atari2600/Target.hpp"

using PagingModel = Analyser::Static::Atari2600::Target::PagingModel;

@interface AtariROMRecord : NSObject
@property(nonatomic, readonly) PagingModel pagingModel;
@property(nonatomic, readonly) BOOL usesSuperchip;
+ (instancetype)recordWithPagingModel:(PagingModel)pagingModel usesSuperchip:(BOOL)usesSuperchip;
@end

@implementation AtariROMRecord
+ (instancetype)recordWithPagingModel:(PagingModel)pagingModel usesSuperchip:(BOOL)usesSuperchip
{
	AtariROMRecord *record = [[AtariROMRecord alloc] init];
	record->_pagingModel = pagingModel;
	record->_usesSuperchip = usesSuperchip;
	return record;
}
@end

#define Record(sha, model, uses) sha : [AtariROMRecord recordWithPagingModel:PagingModel::model usesSuperchip:uses],
static NSDictionary<NSString *, AtariROMRecord *> *romRecordsBySHA1 = @{
	Record(@"58dbcbdffbe80be97746e94a0a75614e64458fdc", None, NO)			// 4kraVCS
	Record(@"9967a76efb68017f793188f691159f04e6bb4447", None, NO)			// 'X'Mission
	Record(@"21d983f2f52b84c22ecae84b0943678ae2c31c10", None, NO)			// 3d Tic-Tac-Toe
	Record(@"d7c62df8300a68b21ce672cfaa4d0f2f4b3d0ce1", Atari16k, NO)		// Acid Drop
	Record(@"924ca836aa08eeffc141d487ac6b9b761b2f8ed5", None, NO)			// Action Force
	Record(@"e07e48d463d30321239a8acc00c490f27f1f7422", None, NO)			// Adventure
	Record(@"03a495c7bfa0671e24aa4d9460d232731f68cb43", None, NO)			// Adventures of Tron
	Record(@"6e420544bf91f603639188824a2b570738bb7e02", None, NO)			// Adventures On GX12
	Record(@"3b02e7dacb418c44d0d3dc77d60a9663b90b0fbc", None, NO)			// Air Raid
	Record(@"29f5c73d1fe806a4284547274dd73f9972a7ed70", None, NO)			// Air Raiders
	Record(@"af5b9f33ccb7778b42957da4f20f2bc000992366", None, NO)			// Air-Sea Battle
	Record(@"0376c242819b785310b8af43c03b1d1156bd5f02", None, NO)			// Airlock
	Record(@"fb870ec3d51468fa4cf40e0efae9617e60c1c91c", None, NO)			// AKA Space Adventure
	Record(@"01d99bf307262825db58631e8002dd008a42cb1e", None, NO)			// Alien
	Record(@"a1f660827ce291f19719a5672f2c5d277d903b03", Atari8k, NO)		// Alpha Beam with Ernie
	Record(@"b89a5ac6593e83fbebee1fe7d4cec81a7032c544", None, NO)			// Amidar
	Record(@"ac58ac94ceab78725a1182cc7b907376c011b0c8", None, NO)			// Angriff der Luftflotten
	Record(@"7d132ab776ff755b86bf4f204165aa54e9e1f1cf", Atari8k, NO)		// Aquaventure
	Record(@"9b6a54969240baf64928118741c3affee148d721", None, NO)			// Armor Ambush
	Record(@"8c249e9eaa83fc6be16039f05ec304efdf987beb", Atari8k, NO)		// Artillery Duel
	Record(@"0c03eba97df5178eec5d4d0aea4a6fe2f961c88f", None, NO)			// Assault
	Record(@"1a094f92e46a8127d9c29889b5389865561c0a6f", Atari8k, NO)		// Asterix (NTSC)
	Record(@"f14408429a911854ec76a191ad64231cc2ed7d11", Atari8k, NO)		// Asterix (PAL)
	Record(@"8f4a00cb4ab6a6f809be0e055d97e8fe17f19e7d", None, NO)			// Asteroid Fire
	Record(@"8423f99092b454aed89f89f5d7da658caf7af016", Atari8k, NO)		// Asteroids
	Record(@"b850bd72d18906d9684e1c7251cb699588cbcf64", None, NO)			// Astroblast
	Record(@"d1563c24208766cf8d28de7af995021a9f89d7e1", None, NO)			// Atari Video Cube
	Record(@"f4e838de9159c149ac080ab85e4f830d5b299963", None, NO)			// Atlantis II
	Record(@"c6b1dcdb2f024ab682316db45763bacc6949c33c", None, NO)			// Atlantis
	Record(@"75e7efa861f7e7d8e367c09bf7c0cc351b472f03", None, NO)			// Bachelor Party
	Record(@"b88ca823aaa10a7a4a3d325023881b2de969c156", None, NO)			// Bachelorette Party
	Record(@"9b1da7fbd0bf6fcadf1b60c11eeb31b6a61a03c3", None, NO)			// Backgammon
	Record(@"80d4020575b14e130f28146bf45921e001f9f649", None, NO)			// Bank Heist
	Record(@"372663097b419ced64f44ef743fe8d0af4317f46", None, NO)			// Barnstorming
	Record(@"d0bdd609ebc6e69fb351ba469ff322406bcbab50", None, NO)			// Base Attack
	Record(@"6c56fad688b2e9bb783f8a5a2360c80ad2338e47", None, NO)			// Basic Programming
	Record(@"bffe99454cb055552e5d612f0dba25470137328d", None, NO)			// Basketball
	Record(@"e4134a3b4a065c856802bc935c12fa7e9868110a", Atari8k, NO)		// Battlezone
	Record(@"47619edb352f7f955f811cbb03a00746c8e099b1", Atari8k, NO)		// Beamrider
	Record(@"fad0c97331a525a4aeba67987552ba324629a7a0", None, NO)			// Beany Bopper
	Record(@"e2c29d0a73a4575028b62dca745476a17f07c8f0", None, NO)			// Beat 'Em & Eat 'Em
	Record(@"c3afd7909b72b49ca7d4485465b622d5e55f8913", Atari8k, NO)		// Berenstain Bears
	Record(@"fcad0e5130de24f06b98fb86a7c3214841ca42e2", None, NO)			// Bermuda Triangle
	Record(@"08bcbc8954473e8f0242b881315b0af4466998ae", None, NO)			// Berzerk
	Record(@"5e4517db83c061926130ab65975e3b83d9401cc9", Atari8k, NO)		// Big Bird's Egg Catch
	Record(@"512e4d047f1f813bc805c8d2a5f7cbdb34b9ea46", None, NO)			// bin00016
	Record(@"f6a41507b8cf890ab7c59bb1424f0500534385ce", Atari8k, NO)		// Bionic Breakthrough
	Record(@"edfd905a34870196f8acb2a9cd41f79f4326f88d", None, NO)			// Blackjack
	Record(@"0fadef01ce28192880f745b23a5fbb64c5a96efe", Atari8k, NO)		// Blueprint
	Record(@"ff25ed062dcc430448b358d2ac745787410e1169", Atari16k, NO)		// BMX Air Master
	Record(@"50e26688fdd3eadcfa83240616267a8f60216c25", None, NO)			// Bobby is Going Home
	Record(@"282cad17482f5f87805065d1a62e49e662d5b4bb", None, NO)			// Bogey Blaster
	Record(@"d106bb41a38ed222dead608d839e8a3f0d0ecc18", None, NO)			// Boing!
	Record(@"cf6ce244b3edaad7ad5e9ca5f01668135c2f93d0", None, NO)			// Bowling
	Record(@"14b9cd91188c7fb0d4566442d639870f8d6f174d", None, NO)			// Boxing
	Record(@"238915cafd26f69bc8a3b9aa7d880dde59f6f12d", None, NO)			// Brain Games
	Record(@"8d473b87b70e26890268e6c417c0bb7f01e402eb", None, NO)			// Breakout
	Record(@"2873eb6effd35003d13e2f8f997b76dbc85d0f64", None, NO)			// Bridge
	Record(@"a65dea2d9790f3eb308c048a01566e35e8c24549", Atari8k, NO)		// Buck Rogers — Planet of Zoom
	Record(@"9c0e13af336a986c271fe828fafdca250afba647", Atari8k, NO)		// Bugs Bunny
	Record(@"67387d0d3d48a44800c44860bf15339a81f41aa9", None, NO)			// Bugs
	Record(@"1819ef408c1216c83dcfeceec28d13f6ea5ca477", MNetwork, NO)		// Bump 'n' Jump
	Record(@"6c199782c79686dc0cbce6d5fe805f276a86a3f5", None, NO)			// Bumper Bash
	Record(@"49e01b8048ae344cb65838f6b1c1de0e1f416f29", MNetwork, NO)		// BurgerTime
	Record(@"b233c37aa5164a54e2e7cc3dc621b331ddc6e55b", None, NO)			// Burning Desire
	Record(@"3f1f17cf620f462355009f5302cddffa730fa2fa", None, NO)			// Cakewalk
	Record(@"609c20365c3a71ce45cb277c66ec3ce6b2c50980", Atari16k, NO)		// California Games
	Record(@"b89443a0029e765c2716774fe2582be37650115c", None, NO)			// Canyon Bomber
	Record(@"e1acf7a845b56e4b3d18192a75a81c7afa6f341a", None, NO)			// Carnival
	Record(@"54ed2864f58ef3768579ec96cca445ee62078521", None, NO)			// cart
	Record(@"08598101e38756916613f37581ef1b61c719016f", None, NO)			// Casino
	Record(@"e979de719cecab2115affd9c0552c6c596b1999a", None, NO)			// Cat Trax
	Record(@"6adf70e0b7b5dab74cf4778f56000de7605e8713", None, NO)			// Cathouse Blues
	Record(@"0b5914bc1526a9beaf54d7fd11408175cd8fcc72", Atari8k, NO)		// Centipede
	Record(@"b2b1bd165b3c10cde5316ed0f9f05a509aac828d", None, NO)			// Challenge (Zellers)
	Record(@"ac9b0c62ba0ca7a975d08fabbbc7c7448ecdf18d", None, NO)			// Challenge of… Nexar
	Record(@"e81b5e49cfbb283edba2c8f21f31a8148d8645a1", None, NO)			// Challenge
	Record(@"872b2f9aa7edbcbb2368de0db3696c90998ff016", None, NO)			// Chase the Chuckwagon
	Record(@"39b5bb27a6c4cb6532bd9d4cc520415c59dac653", None, NO)			// Checkers
	Record(@"0b1bb76769ae3f8b4936f0f95f4941d276791bde", None, NO)			// China Syndrome
	Record(@"51a53bbfdbcc22925515ae0af79df434df6ee68a", None, NO)			// Chopper Command
	Record(@"8a91ecdbd8bf9d412da051c3422abb004eab8603", None, NO)			// Circus
	Record(@"3f56d1a376702b64b3992b2d5652a3842c56ffad", None, NO)			// Coco Nuts
	Record(@"137bd3d3f36e2549c6e1cc3a60f2a7574f767775", None, NO)			// Codebreaker
	Record(@"53c324ae736afa92a83d619b04e4fe72182281a6", None, NO)			// Color Bar Generator
	Record(@"66014de1f8e9f39483ee3f97ca0d97d026ffc3bb", Atari8k, NO)		// Combat Two
	Record(@"ce7580059e8b41cb4a1e734c9b35ce3774bf777a", None, NO)			// Combat
	Record(@"8dad05085657e95e567f47836502be515b42f66b", None, NO)			// Commando Raid
	Record(@"68a7cb3ff847cd987a551f3dd9cda5f90ce0a3bf", Atari16k, NO)		// Commando
	Record(@"dbc0c0451dee44425810e04df8f1d26d1c2d3993", None, NO)			// Computer Chess
	Record(@"5512a0ed4306edc007a78bb52dbcf492adf798ec", None, NO)			// Confrontation
	Record(@"3a77db43b6583e8689435f0f14aa04b9e57bdded", Atari8k, NO)		// Congo Bongo
	Record(@"f4a62ba0ff59803c5f40d59eeed1e126fe37979b", Atari8k, NO)		// Cookie Monster Munch
	Record(@"187983fd14d37498437d0ef8f3fbd05675feb6ae", None, NO)			// Cosmic Ark
	Record(@"3717c97bbb0f547e4389db8fc954d1bad992444c", None, NO)			// Cosmic Commuter
	Record(@"8b9dfef6c6757a6a59e01d783b751e4ab9541d9e", None, NO)			// Cosmic Corridor
	Record(@"22ff281b1e698e8a5d7a6f6173c86c46d3cd8561", None, NO)			// Cosmic Creeps
	Record(@"c01354760f2ca8d6e4d01b230f31611973c6ae2d", None, NO)			// Cosmic Swarm
	Record(@"6ee0a26af4643ff250198dfc1c2b7c6568b4f207", None, NO)			// Crackpots
	Record(@"73d68f32d1fb73883ceb183d5150bff5f1065de4", None, NO)			// Crash Dive
	Record(@"70e723aa67d68f8549d9bd8f96d8b1262cbdac3c", Atari8k, NO)		// Crazy Climber
	Record(@"dd385886fdd20727c060bad6c92541938661e2b4", None, NO)			// Crazy Valet
	Record(@"b1b3d8d6afe94b73a43c36668cc756c5b6fdc1c3", None, NO)			// Cross Force
	Record(@"5da3d089ccda960ce244adb855975877c670e615", Atari16k, NO)		// Crossbow
	Record(@"a57062f44e7ac793d4c39d1350521dc5bc2a665f", None, NO)			// Crypts of Chaos
	Record(@"2e4ee5ee040b08be1fe568602d1859664e607efb", Atari16k, YES)		// Crystal Castles
	Record(@"0dd72a3461b4167f2d68c93511ed4985d97e6adc", None, NO)			// Cubicolor
	Record(@"4b3d02b59e17520b4d60236568d5cb50a4e6aeb3", None, NO)			// Custer's Revenge
	Record(@"07e94a7d357e859dcff77180981713ce8119324e", None, NO)			// Dancing Plate
	Record(@"0ae2fc87f87a5cc199c3b9a17444bf3c2f6a829b", None, NO)			// Dark Cavern
	Record(@"fbb4814973fcb4e101521515e04daa6424c45f5c", Atari16k, YES)		// Dark Chambers
	Record(@"c5af53c4b64c3db552d4717b8583d6fe8d3e7952", Atari8k, NO)		// Dark Mage
	Record(@"68de291d5e9cbebfed72d2f9039e60581b6dbdc5", None, NO)			// Deadly Duck
	Record(@"2ad9db4b5aec2da36ecc3178599b02619c3c462e", ParkerBros, NO)		// Death Star Battle
	Record(@"5f710a1148740760b4ebcc42861a1f9c3384799e", None, NO)			// Death Trap
	Record(@"717656f561823edaa69240471c3106963f5c307e", ActivisionStack, NO)// Decathlon
	Record(@"d7b506b84f28e1b917a2978753d5a40eb197537a", Atari8k, YES)		// Defender 2
	Record(@"79facc1bf70e642685057999f5c2b8e94b102439", None, NO)			// Defender
	Record(@"5aae618292a728b55ad7f00242d870736b5356d3", None, NO)			// Demolition Herby
	Record(@"a580d886c191a069f6b9036c3f879e83e09500c2", None, NO)			// Demon Attack
	Record(@"b45582de81c48b04c2bb758d69021e8088c70ce7", None, NO)			// Demons to Diamonds
	Record(@"ccea2d5095441d7e1b1468e3879a6ab556dc8b7a", Atari16k, YES)		// Desert Falcon
	Record(@"538108ca9821265e23f06fa7672965631bdf8175", None, NO)			// Diagnostic Test Cartridge 2.6
	Record(@"81022ef30e682183d51b18bff145ce425c6f924e", None, NO)			// Dice Puzzle
	Record(@"79e746524520da546249149c33614fc23a4f2a51", Atari16k, YES)		// Dig Dug
	Record(@"7485cf55201ef98ded201aec73c4141f9f74f863", None, NO)			// Dishaster
	Record(@"157117df23cb5229386d06bbdb3af20a208722e0", None, NO)			// Dodge 'Em
	Record(@"e3985d759f8a8f4705f543ce7eb5e93bf63722b5", None, NO)			// Dolphin
	Record(@"4606c0751f560200aede6598ec9c8e6249a105f5", Atari8k, NO)		// Donald Duck's Speedboat
	Record(@"359e662da02bf0a2184472e25d05bc597b6c497a", None, NO)			// Donkey Kong (1983) (CBS Electronics) (PAL) [!]
	Record(@"98f98ac0728c68de66afda6500cafbdffe8ab50a", Atari8k, NO)		// Donkey Kong Junior
	Record(@"6e6e37ec8d66aea1c13ed444863e3db91497aa35", None, NO)			// Donkey Kong
	Record(@"251e02ac583d84eb43f1451d55b62c7c70e9644f", Atari16k, NO)		// Double Dragon
	Record(@"8e2ea320b23994dc87abe69d61249489f3a0fccc", Atari16k, NO)		// Double Dunk
	Record(@"b446381fe480156077b0b3c51747d156e5dde89f", None, NO)			// Dragon Treasures
	Record(@"944c52de85464070a946813b050518977750e939", None, NO)			// Dragster
	Record(@"9caf114c9582d5e0c14396b13d2bd1a89cad90b1", None, NO)			// Dukes of Hazzard (1980)
	Record(@"c061d753435dcb7275a8764f4ad003b05fa100ed", Atari16k, NO)		// Dukes of Hazzard (1983)
	Record(@"d16eba13ab1313f375e86b488181567f846f1dc4", Atari8k, NO)		// Dumbo's Flying Circus
	Record(@"9e34f9ca51573c92918720f8a259b9449a0cd65e", Atari8k, NO)		// E.T. — The Extra-Terrestrial
	Record(@"68cbfadf097ae2d1e838f315c7cc7b70bbf2ccc8", None, NO)			// Eggomania
	Record(@"bab872ee41695cefe41d88e4932132eca6c4e69c", Atari8k, YES)		// Elevator Action
	Record(@"475fc2b23c0ee273388539a4eeafa34f8f8d3fd8", None, NO)			// Eli's Ladder
	Record(@"3983e109fc0b38c0b559a09a001f3e5f2bb1dc2a", Atari8k, NO)		// Elk Attack
	Record(@"205af4051ea39fb5a038a8545c78bff91df321b7", None, NO)			// Encounter at L-5
	Record(@"82e9b2dd6d99f15381506a76ef958a1773a7ba21", None, NO)			// Enduro
	Record(@"7905aee90a6dd64d9538e0b8e772f833ba9feb83", None, NO)			// Entombed
	Record(@"27d925d482553deff23f0889b3051091977d6920", Tigervision, NO)	// Espial
	Record(@"c17801c0190ade27f438e2aa98dde81b3ae66267", None, NO)			// Exocet
	Record(@"6297dd336a6343f98cd142d1d3d76ce84770a488", None, NO)			// Fantastic Voyage
	Record(@"a5614c751f29118ddb3dec9794612b98a0f00b98", None, NO)			// Fast Eddie
	Record(@"c62a70645939480b184e3b2e378ec4bcbd484bc7", None, NO)			// Fast Food
	Record(@"d0bb58ea1fc37e929e5f7cdead037bb14a166451", Atari32k, YES)		// Fatal Run
	Record(@"686427cc47b69980d292d04597270347942773ff", Atari8k, NO)		// Fathom
	Record(@"684275b22f2bac7d577cf48cf42fa14fa6f69678", Atari16k, NO)		// Fighter Pilot
	Record(@"ba9a8ccfeb552dd756c660ea843a39619d3c77e9", None, NO)			// Final Approach
	Record(@"f76cc14afd7aef367c5a5defbd84f3bbb2f98ba3", None, NO)			// Fire Fighter
	Record(@"df5420eb0f71e681e7222ede8e211a7601e7a327", None, NO)			// Fire Fly
	Record(@"531e995aef6cd47b0efea72ae3e56aeee449d798", None, NO)			// Fishing Derby
	Record(@"ac05f05f3365f5e348e1e618410065a1c2a88ee4", None, NO)			// Flag Capture
	Record(@"c6fe4ce24bc1ebd538258d98cfe829963323acca", None, NO)			// Football
	Record(@"c6023bf73818c78b2e477a9c6dac411cdbf9c0aa", None, NO)			// Frankenstein's Monster
	Record(@"91cc7e5cd6c0d4a6f42ed66353b7ee7bb972fa3f", None, NO)			// Freeway
	Record(@"de6fc1b51d41b34dcda92f579b2aa4df8eccf586", Atari8k, NO)		// Frog Pond
	Record(@"f344d5a8dc895c5a2ae0288f3c6cb66650e49167", None, NO)			// Frogflys
	Record(@"6b9e591cc53844795725fc66c564f0364d1fbe40", ParkerBros, NO)		// Frogger II
	Record(@"e859b935a36494f3c4b4bf5547392600fb9c96f0", None, NO)			// Frogger
	Record(@"cf32bfcd7f2c3b7d2a6ad2f298aea2dfad8242e7", Atari8k, NO)		// Front Line
	Record(@"b9e60437e7691d5ef9002cfc7d15ae95f1c03a12", None, NO)			// Frostbite
	Record(@"5cc4010eb2858afe8ce77f53a89d37c7584e15b4", None, NO)			// Fun with Numbers
	Record(@"9cfb6288a5c2dae63ee6f5e9325200ccd21a3055", None, NO)			// G.I. Joe — Cobra Strike
	Record(@"8e708e0067d3302327900fa322aeb8e2df2022d7", Atari8k, NO)		// Galaxian Enhanced Graphics
	Record(@"b081b327ac32d951c36cb4b3ff812be95685d52f", Atari8k, NO)		// Galaxian
	Record(@"8cf49d43bd62308df788cfacbfcd80e9226c7590", None, NO)			// Gangster Alley
	Record(@"bc0d1edc251d8d4db3d5234ec83dee171642a547", Atari16k, NO)		// Garfield
	Record(@"0da1f2de5a9b5a6604ccdb0f30b9da4e5f799b40", None, NO)			// Gas Hog
	Record(@"73adae38d86d50360b1a247244df05892e33da46", None, NO)			// Gauntlet
	Record(@"3b1fb93342c7f014a28dddf6f16895d11ac7d6f0", None, NO)			// General Re-Treat
	Record(@"4b533776dcd9d538f9206ad1e28b30116d08df1e", Atari8k, NO)		// Ghost Manor
	Record(@"1bcf03e1129015a46ad7028e0e74253653944e86", Atari16k, NO)		// Ghostbusters II (alternate)
	Record(@"e032876305647a95b622e5c4971f7096ef72acdb", Atari16k, NO)		// Ghostbusters II
	Record(@"5ed0b2cb346d20720e3c526da331551aa16a23a4", Atari8k, NO)		// Ghostbusters
	Record(@"b64ed2d5a2f8fdac4ff0ce56939ba72e343fec33", None, NO)			// Gigolo
	Record(@"3a3d7206afee36786026d6287fe956c2ebc80ea7", None, NO)			// Glacier Patrol
	Record(@"7bca0f7a0f992782e4e4c90772bac976ca963a6d", None, NO)			// Glib
	Record(@"f78c478aacf6536522e8d37a3888a975e1a383cd", None, NO)			// Go Go Home Monster (2)
	Record(@"4c41379f0dd9880384fcbb46bad9fbaaf109a477", None, NO)			// Go Go Home Monster
	Record(@"a25d52770408314dec6f41aaf5f9f0a2a3e2c18f", None, NO)			// Golf
	Record(@"97fb489ba4ce0f8a306563063563617321352cfb", None, NO)			// Gopher
	Record(@"35f8341c73c7e6e896cb065977427b3f98ae9f08", None, NO)			// Gorf
	Record(@"24fab817728216582b6d95558c361ace66abf96f", None, NO)			// Grand Prix
	Record(@"a372d4dd3d95b3866553cae2336e4565e00cc25b", Atari8k, NO)		// Gravitar
	Record(@"7a027329309e018b0d51adcb6ae13c9d13e54f4a", Atari8k, NO)		// Gremlins
	Record(@"c90acaee066f97efc6a520deb7fa3e5760a471fa", Atari8k, NO)		// Grover's Music Maker
	Record(@"7d30ff565ad7b2a3143d049c5b39e4a6ac3f9cd5", None, NO)			// Guardian
	Record(@"45f3f98735798e19427a9100a9000d97917b932f", None, NO)			// Gunfight (NTSC)
	Record(@"7ac6356224cc718ee5731d1ce14aea6fb2335439", None, NO)			// Gunfight (PAL)
	Record(@"4bd87ba8b3b6d7850e3ea41b4d494c3b12659f27", ParkerBros, NO)		// Gyruss
	Record(@"282f94817401e3725c622b73a0c05685ce761783", Atari8k, NO)		// H.E.R.O.
	Record(@"4c72cec151f219866bf870fa7ac749a19ca501c9", None, NO)			// Halloween
	Record(@"561bccf508e162bc70c42d85c170cf0d1d4691a3", None, NO)			// Hangman
	Record(@"d6db71da02ae194140bf812be34d6e8a6785d138", None, NO)			// Harbor Escape
	Record(@"1476c869619075b551b20f2c7f95b11e0d16aec1", None, NO)			// Haunted House
	Record(@"8196209ef7048c5494dbdc932adbf1c7abf79f4e", Atari8k, NO)		// Holey Moley
	Record(@"f362d2b3a50e5ae3c2b412b6c08ecdcfee47a688", None, NO)			// Home Run
	Record(@"d4b0b2aa379893356c72414ee0065a3a91cf9f97", None, NO)			// Human Cannonball
	Record(@"a6e42c63138a2fd527cdbe9b7e60f5feabdd55c8", None, NO)			// Hunt & Score — Memory Match
	Record(@"f7e782214b5f9227e34c00f590be50534f1fda91", None, NO)			// I Want my Mommy
	Record(@"21de0f034e5dad03fa91eb7ae6cc081c142be35c", None, NO)			// Ice Hockey
	Record(@"e5b9c3a3638bd42f96a26b651463da96a9432315", Atari16k, NO)		// Ikari Warriors
	Record(@"620ab88d63cdd3f8ce67deac00a22257c7205c8b", None, NO)			// Indy 500
	Record(@"922cd171ef132bf6c5bed00ad01410ada4b20729", None, NO)			// Infiltrate
	Record(@"19fc37f2a24e31a59a17f9cbf3cc03416a8bab9a", None, NO)			// Invaders
	Record(@"2bbc124cead9aa49b364268735dad8cb1eb6594f", ParkerBros, NO)		// James Bond 007
	Record(@"ea5c827052886908c0deaa0a03d6f8da8e4f298d", None, NO)			// Jammed Demo
	Record(@"af4d6867a8bc4818fc6bb701a765a3c907feb628", None, NO)			// Jaw Breaker
	Record(@"36b9edc7150311203f375c1be10d0510efde6476", None, NO)			// Jedi Arena
	Record(@"0d94c1caacb862d9e0b4c2dda121cd4d74a1cced", None, NO)			// John K Harvey's Equalizer
	Record(@"928eaa424b36d98078f9251d67fb13a8fddfafbd", None, NO)			// Journey Escape
	Record(@"cb94dc316cba282a0036871db2417257e960786b", Atari8k, NO)		// Joust
	Record(@"cd2cf245d6e924ff2100cc93d20223c4a231e160", Atari16k, YES)		// Jr. Pac-Man
	Record(@"9a0ee845d9928d4db003b07b927bb2c1f628e725", None, NO)			// Jungle Fever
	Record(@"83a32a2d686355438c915540cfe0bb13b76c1113", Atari8k, NO)		// Jungle Hunt
	Record(@"ce8ac88b799c282567495ce509402a5a4c2c4d82", None, NO)			// Kabobber
	Record(@"40d4df4f8e4a69a299ae7678c17e72bedeb70105", None, NO)			// Kaboom!
	Record(@"a82aaeef44ad88de605c50d23fb4f6cec73f3ab4", None, NO)			// Kamikaze Saucers
	Record(@"01fd30311e028944eafb6d14bb001035f816ced7", Atari8k, NO)		// Kangaroo
	Record(@"c0db7d295e2ce5e00e00b8a83075b1103688ea15", None, NO)			// Karate
	Record(@"3eefc193dec3b242bcfd43f5a4d9f023e55378a4", None, NO)			// Keystone Kapers
	Record(@"839e13ffedcbed22d51a24c001900c3474a078f2", None, NO)			// King Kong
	Record(@"3162259c6dbfbb57a2ea41d849155702151ee39b", Atari16k, YES)		// Klax
	Record(@"759597d1d779cfdfd7aa30fd28a59acc58ca2533", None, NO)			// Knight on the Town
	Record(@"2f550743e237f6dc8c75c389a01b02e9a396fdad", None, NO)			// Kool-Aid Man
	Record(@"4bdf1cf73316bdb0002606facf11b6ddcb287207", Atari8k, NO)		// Krull
	Record(@"1637b6b9cd1a918339ec054cf95b924e7ce4789a", Atari8k, NO)		// Kung Fu Superkicks
	Record(@"3b93a34ba2a6b7db387ea588c48d939eee5d71a1", Atari8k, NO)		// Kung-Fu Master
	Record(@"6d59dfea26b7a06545a817f03f62a59be8993587", None, NO)			// Lady in Wading
	Record(@"ea8ecc2f6818e1c9479f55c0a3356edcf7a4d657", None, NO)			// Laser Blast
	Record(@"cdf55b73b4322428a001e545019eaa591d3479cf", None, NO)			// Laser Gates
	Record(@"afab795719386a776b5fb2165fc84f4858e16e05", None, NO)			// Laser Volley
	Record(@"fe208ad775cbf9523e7a99632b9f10f2c9c7aa87", None, NO)			// Lochjaw
	Record(@"fc3d75d46d917457aa1701bf47844817d0ba96c3", None, NO)			// Lock 'n' Chase
	Record(@"f92b0b83db3cd840d16ee2726011f5f0144103d5", None, NO)			// London Blitz
	Record(@"ef02fdb94ac092247bfcd5f556e01a68c06a4832", ParkerBros, NO)		// Lord of the Rings
	Record(@"e8492fe9d62750df682358fe59a4d4272655eb96", None, NO)			// Lost Luggage
	Record(@"dcd96913a1c840c8b57848986242eeb928bfd2ff", None, NO)			// M*A*S*H
	Record(@"d6e2b7765a9d30f91c9b2b8d0adf61ec5dc2b30a", None, NO)			// M.A.D.
	Record(@"4c66b84ab0d25e46729bbcf23f985d59ca8520ad", CommaVid, NO)		// MagiCard
	Record(@"cdc7e65d965a7a00adda1e8bedfbe6200e349497", None, NO)			// Malagai
	Record(@"ee8f9bf7cdb55f25f4d99e1a23f4c90106fadc39", None, NO)			// Mangia
	Record(@"249a11bb4872a24f22dff1027ff256c1408140c2", None, NO)			// Marauder
	Record(@"dd9e94ca96c75a212f1414aa511fd99ecdadaf44", None, NO)			// Marine Wars
	Record(@"49425ff154b92ca048abb4ce5e8d485c24935035", Atari8k, NO)		// Mario Bros.
	Record(@"fbe7a78764407743b43a91136903ede65306f4e7", None, NO)			// Master Builder
	Record(@"6db8fa65755db86438ada3d90f4c39cc288dcf84", MNetwork, NO)		// Masters of the Universe
	Record(@"18fac606400c08a0469aebd9b071ae3aec2a3cf2", None, NO)			// Math Gran Prix
	Record(@"aba25089d87cd6fee8d206b880baa5d938aae255", None, NO)			// Maze Craze
	Record(@"0ae118373c7bda97da2f8d9c113e1e09ea7e49e1", None, NO)			// Mega Force
	Record(@"46977baf0e1ee6124b524258879c46f80d624fae", MegaBoy, NO)		// MegaBoy
	Record(@"9c5748b38661dbadcbc9cd1ec6a6b0c550b0e3da", None, NO)			// MegaMania
	Record(@"debb1572eadb20beb0e4cd2df8396def8eb02098", None, NO)			// Meltdown
	Record(@"7fcf95459ea597a332bf5b6f56c8f891307b45b4", Atari16k, NO)		// Midnight Magic
	Record(@"0616f0dde6d697816dda92ed9e5a4c3d77a39408", Atari16k, YES)		// Millipede
	Record(@"5edbf8a24fcba9763983befe20e2311f61b986d4", Tigervision, NO)	// Miner 2049er Volume 2
	Record(@"0e56b48e88f69d405eabf544e57663bd180b3b1e", Tigervision, NO)	// Miner 2049er
	Record(@"34773998d7740e1e8c206b3b22a19e282ca132e1", None, NO)			// Mines of Minos
	Record(@"be24b42e3744a81fb217c86c4ed5ce51bff28e65", None, NO)			// Miniature Golf
	Record(@"f721d1f750e19b9e1788eed5e3872923ab46a91d", Atari8k, NO)		// Miss Piggy's Wedding
	Record(@"faa06bb0643dbf556b13591c31917d277a83110b", None, NO)			// Missile Command
	Record(@"224e7a310afdb91c6915743e72b7b53b38eb5754", None, NO)			// Missile Control
	Record(@"999dc390a7a3f7be7c88022506c70bd4208b26d8", None, NO)			// Mission 3,000 AD
	Record(@"93520821ce406a7aa6cc30472f76bca543805fd4", None, NO)			// Mission Survive
	Record(@"0b74a90a22a7a16f9c2131fabd76b7742de0473e", None, NO)			// Mogul Maniac
	Record(@"81a4d56820b1e00130e368a3532c409929aff5fb", Atari8k, NO)		// Monstercise
	Record(@"7dfeb1a8ec863c1e0f297113a1cc4185c215e81c", ParkerBros, NO)		// Montezuma's Revenge
	Record(@"dce778f397a325113f035722b7769492645d69eb", Atari8k, NO)		// Moon Patrol
	Record(@"05ab04dc30eae31b98ebf6f43fec6793a53e0a23", Atari8k, NO)		// Moonsweeper
	Record(@"c4d495d42ea5bd354af04e1f2b68cce0fb43175d", Atari8k, NO)		// Motocross Racer
	Record(@"ece97bda734faffcf847a8bcdfa474789c377d8d", Atari16k, NO)		// MotoRodeo
	Record(@"ef4112e86d6a3e8f7b8e482d294a5917f162b38c", CBSRamPlus, NO)		// Mountain King
	Record(@"cf6347dedcfec213c28dd92111ec6f41e74b6f64", None, NO)			// Mouse Trap
	Record(@"e4c912199779bba25f1b9950007f14dca3d19c84", Atari8k, NO)		// Mr Do!
	Record(@"330c2c67399e07c40f4101f9e18670fef070475e", ParkerBros, NO)		// Mr. Do!'s Castle
	Record(@"62b933cdd8844bb1816ce57889203954fe782603", Atari8k, NO)		// Ms. Pac-Man
	Record(@"b2df23b1bf6df9d253ad0705592d3fce352a837b", Atari8k, NO)		// My Golf
	Record(@"2b4a0535ca83b963906eb0a5d60ce0e21f07905d", None, NO)			// Name This Game
	Record(@"372771aeb4e2fb2cd1dead5497e3821e4236d5fc", None, NO)			// Night Driver
	Record(@"26e1309bc848cf5880b831d7566488ec5b3db58c", None, NO)			// Night Stalker (2)
	Record(@"281ff7e55c27656522b144b84cba08eb148e2f0a", None, NO)			// Night Stalker
	Record(@"e2e8750b8856dd44d914c43a7d277188cc148e5c", None, NO)			// No Escape!
	Record(@"771a2a87b3b457c0b83f556ce00d1e9c54caeabc", None, NO)			// Nothing
	Record(@"be3f4beeb322cddc7223d6d77e17302aa811e43a", Atari8k, NO)		// Obelix
	Record(@"1f8d06b99db94b0aa8ca320c7cb212639ac9591f", None, NO)			// Ocean City
	Record(@"3dcfe93399044148561586056288c6f8e5c96e2b", Atari16k, YES)		// Off the Wall
	Record(@"7ad74a7c36318f1304f5dc454401cf257fa60d7a", None, NO)			// Off Your Rocker
	Record(@"ea674cf2c90d407b8f8b96eac692690b602b73f9", None, NO)			// Oink!
	Record(@"7bd1cbddefcf3bd24da570be015234d0c444a7e5", None, NO)			// Okie Dokie
	Record(@"dcaab259e7617c7ac7d349893451896a9ca0e292", CBSRamPlus, NO)		// Omega Race
	Record(@"cd968c2a983f9adf4d8d8d56823923b31c33980f", None, NO)			// Open Sesame
	Record(@"7905709fcc85cbcfc28ca2ed543ffa737a5483ae", Atari8k, NO)		// Oscar's Trash Race
	Record(@"cbecf1a32d9366a3dd4ad643916cd59cdc820a8b", None, NO)			// Othello
	Record(@"344d6942723513c376a7a844779804e10f357b85", None, NO)			// Out of Control
	Record(@"f8eeaaf4635ac39b4bdf7ded1348bce46313ef9f", None, NO)			// Outlaw
	Record(@"412e8a2438379878ee4de5c6bf4e5a9ee2707c8b", None, NO)			// Oystron
	Record(@"923c969d31cef450075932436f03f1404e1cab0e", None, NO)			// Pac-Kong
	Record(@"0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1", None, NO)			// Pac-Man
	Record(@"b529c1664ed1abc8f5f962a1fed65c0e4440219c", None, NO)			// Peek-A-Boo
	Record(@"832283530f5dee332f29cf8c4854dd554f2030a0", None, NO)			// Pele's Soccer
	Record(@"461c2ea3e4d24f86ec02215c1f4743d250796c11", Atari8k, NO)		// Pengo (prototype)
	Record(@"89b991a7a251f78f422bcdf9cf7d4475fdf33e97", Atari8k, NO)		// Pengo
	Record(@"c5317035e73f60e959c123d89600c81b7c45701f", None, NO)			// Pepsi Invaders
	Record(@"19c3ad034466c0433501a415a996ed7155d6063a", Atari16k, NO)		// Pete Rose Baseball
	Record(@"959aca4b44269b1e5ac58791fc3c7c461a6a4a17", None, NO)			// Phantom Tank
	Record(@"b299df2792c5cca73118925dff85695b73a16228", None, NO)			// Philly Flasher
	Record(@"010d51e3f522ba60f021d56819437d7c85897cdd", Atari8k, NO)		// Phoenix
	Record(@"a5917537cf1093aa350903d85d9e271e8a11d2cf", Atari16k, NO)		// Pick 'n' Pile
	Record(@"483fc907471c5c358fb3e624097861a2fc9c1e45", None, NO)			// Picnic
	Record(@"57774193081acea010bd935a0449bc8f53157128", None, NO)			// Piece o' Cake
	Record(@"d08b30ca2e5e351cac3bd3fb760b87a1a30aa300", Atari8k, NO)		// Pigs in Space
	Record(@"920cfbd517764ad3fa6a7425c031bd72dc7d927c", Pitfall2, NO)		// Pitfall II
	Record(@"d0ec08b88d032627701ad72337524d91b26c656b", None, NO)			// Pitfall! (PAL)
	Record(@"8d525480445d48cc48460dc666ebad78c8fb7b73", None, NO)			// Pitfall! (NTSC)
	Record(@"dcca30e4ae58c85a070f0c6cfaa4d27be2970d61", None, NO)			// Planet of the Apes
	Record(@"ccfcbf52815a441158977292b719f7c5ed80c515", None, NO)			// Planet Patrol
	Record(@"103398dd35ebd39450c5cac760fa332aac3f9458", None, NO)			// Plaque Attack
	Record(@"2410931a8a18b915993b6982fbabab0f437967a4", Tigervision, NO)	// Polaris
	Record(@"9d334da07352a9399cbbd9b41c6923232d0cdcd3", Atari8k, NO)		// Pole Position
	Record(@"c0af0188028cd899c49ba18f52bd1678e573bff2", None, NO)			// Polo
	Record(@"954d2980ea8f8d9a76921612c378889f24c35639", None, NO)			// Pompeii
	Record(@"b7a002025c24ab2ec4a03f62212db7b96c0e5ffd", None, NO)			// Pooyan
	Record(@"1772a22df3e9a1f3842387ac63eeddff7f04b01c", ParkerBros, NO)		// Popeye
	Record(@"70afc2cc870be546dc976fa0c6811f7e01ebc471", Atari8k, NO)		// Porky's
	Record(@"8b001373be485060f88182e9a7afcf55b4d07a57", Atari8k, NO)		// Pressure Cooker
	Record(@"1ea6bea907a6b5607c76f222730f812a99cd1015", Atari8k, NO)		// Private Eye
	Record(@"feb6bd37e5d722bd080433587972b980afff5fa5", None, NO)			// Pumruckl I
	Record(@"a61be3702437b5d16e19c0d2cd92393515d42f23", ParkerBros, NO)		// Q-Bert's Qubes
	Record(@"f3ef9787b4287a32e4d9ac7b9c3358edc16315b2", None, NO)			// Q-Bert
	Record(@"1e634a8733cbc50462d363562b80013343d2fac3", Atari8k, NO)		// Quadrun
	Record(@"d83c740d2968343e6401828d62f58be6aea8e858", Atari8k, NO)		// Quest for Quintana Roo
	Record(@"33a47f79610c4525802c9881f67ad1f3f8c1b55d", None, NO)			// Quick Step!
	Record(@"7bf945ea667e683ec24a4ed779e88bbe55dc4b26", Atari8k, NO)		// Rabbit Transit
	Record(@"4af6008152f1d38626d84016a7ef753406b48b46", None, NO)			// Racquetball
	Record(@"33f016c941fab01e1e2d0d7ba7930e3bcd8feaa3", Atari16k, YES)		// Radar Lock
	Record(@"5f1f2b5b407b0624b59409e02060a3a9e8eed8fc", None, NO)			// Radar
	Record(@"a79f6e0f4fd76878e5c3ba6b52d17e88acdbe9f6", None, NO)			// Raft Rider
	Record(@"7ae70783969709318e56f189cf03da92320a6aba", Atari8k, NO)		// Raiders of the Lost Ark
	Record(@"fd0a69c06eb3f7c9328951c890644f93c4bad6ad", None, NO)			// Ram It
	Record(@"7bb7df255829d5fbbee0d944915e50f89a5e7075", Atari16k, NO)		// Rampage!
	Record(@"5adf9b530321472380ebceb2539de2ffbb0310bc", None, NO)			// Reactor
	Record(@"ace97b89b8b6ab947434dbfd263951c6c0b349ac", Atari8k, NO)		// RealSports Baseball
	Record(@"bc2e6bdaa950bc06be040899dfeb9ad0938f4e98", Atari8k, NO)		// RealSports Basketball
	Record(@"22dedbfce6cc9055a6c4caec013ca80200e51971", Atari16k, NO)		// RealSports Boxing
	Record(@"200d04c1e7f41a5a3730287ed0c3f9293628f195", Atari8k, NO)		// RealSports Football
	Record(@"e3d964d918b7f2c420776acd3370ec1ee62744ea", Atari8k, NO)		// RealSports Soccer
	Record(@"702c1c7d985d0d22f935265bd284d1ed50df2527", Atari8k, NO)		// RealSports Tennis
	Record(@"2025e1d868595fad36e5d9e7384ffd24c206208d", None, NO)			// RealSports Volleyball
	Record(@"94e94810bf6c72eee49157f9218c3c170b65c836", None, NO)			// Rescue Terra I
	Record(@"f8a9dd46f9bad232f74d1ee2671ccb26ea1b3029", None, NO)			// Revenge of the Beefsteak Tomatoes
	Record(@"acb2430b4e6c72ce13f321d9d3a38986dc4768ef", None, NO)			// Riddle of the Sphinx
	Record(@"6715493dce54b22362741229078815b3360988ae", Tigervision, NO)	// River Patrol
	Record(@"40329780402f8247f294fe884ffc56cc3da0c62d", None, NO)			// River Raid copy
	Record(@"a08c3eae3368334c937a5e03329782e95f7b57c7", Atari16k, NO)		// River Raid II
	Record(@"325a2374800b2cb78ab7ff9e4017759865109d7d", None, NO)			// River Raid
	Record(@"7f9c2321c9f22cf2cdbcf1b3f0e563a1c53f68ca", Atari8k, NO)		// Robin Hood
	Record(@"f45dfcd6db0dae5458e1c0ae8eeaa75b553cdfec", Atari16k, NO)		// Road Runner
	Record(@"21a3ee57cb622f410ffd51986ab80acadb8d44b7", ActivisionStack, NO)// Robot Tank
	Record(@"0abf0a292d4a24df5a5ebe19a9729f3a8f883c8b", Atari8k, NO)		// Roc 'n Rope
	Record(@"fd243c480e769b20b7bf3e74bcd86e4ac99dab19", None, NO)			// Room of Doom
	Record(@"85752ac6eb7045a9083425cd166609882a1c2c58", Atari8k, NO)		// Saboteur
	Record(@"ecd8ef49ae23ddd3e10ec60839b95c8e7764ea27", Atari16k, YES)		// Save Mary!
	Record(@"e566d7b1f4eb6c2b110eb4fc676eb0ce9e90fe1e", None, NO)			// SCIScide 1.32
	Record(@"8e9b320d8966315a8b07f1babc0ba2662f761102", None, NO)			// SCSIcide 1.30
	Record(@"e8f5ae861ca1f410c6a9af116a96ed65d9a3abb2", None, NO)			// Scuba Diver
	Record(@"e5558ae30acc1fa5b4ffe782ae480622586a32ca", None, NO)			// Sea Hunt
	Record(@"4ec6982b2da25b29840428fd993a391e63f53730", None, NO)			// Seahawk
	Record(@"7324a1ebc695a477c8884718ffcad27732a98ab0", None, NO)			// Seaquest
	Record(@"1914f57ab0a6f221f2ad344b244a3cdd7b1d991a", None, NO)			// Secret Agent
	Record(@"af11f1666d345267196a1c35223727e2ef93483a", Atari16k, YES)		// Secret Quest
	Record(@"6518078b3786ac26f75067f0646aef4e83f2db15", None, NO)			// Self_Portrait_by_Tjoppen
	Record(@"fcf5f8a7d6e59a339c2002e3d4084d87deb670fe", Atari16k, NO)		// Sentinel
	Record(@"6e91759756c34f40a2c26936df6c0ca1a3850e80", None, NO)			// Shark Attack
	Record(@"cfb4b41e318c7cd0070e75e412f67c973e124d8e", None, NO)			// Shootin' Gallery
	Record(@"6e6daa34878d3e331c630359c7125a4ffba1b22d", Atari16k, YES)		// Shooting Arcade
	Record(@"08952192ea6bf0ef94373520a7e855f58bae6179", None, NO)			// Shuttle Orbiter
	Record(@"242fc23def80da96da22c2c7238d48635489abb0", Atari8k, NO)		// Sinistar
	Record(@"e9fa52f8e7f747cd9685ddb18bdeed2f66255324", Atari8k, NO)		// Sir Lancelot
	Record(@"a26fe0b5a43fe8116ab0ae6656d6b11644d871ec", Atari8k, NO)		// Skate Boardin'
	Record(@"5ea6d2eb27c76e85f477ba6c799deb7c416ebbc3", None, NO)			// Skeet Shoot
	Record(@"6581846f983b50cffb75d1c1b902238ba7dd4e92", None, NO)			// Skiing
	Record(@"4dde18d4abc139562fdd7a9d2fd49a1f00a9e64a", None, NO)			// Sky Diver
	Record(@"105f722dcf9a89b683c10ddd7f684c5966c8e1db", None, NO)			// Sky Jinks
	Record(@"fc5f1e30db3b2469c9701dadfa95f3268fd1e4cb", Atari8k, NO)		// Sky Patrol
	Record(@"ef0a7ecfe8f3b5d1e67a736552a0cdc472803be9", None, NO)			// Sky Skipper
	Record(@"7239d1c64f3dfc2a1613be325cce13803dd2baa5", None, NO)			// Slot Machine
	Record(@"a2b13017d759346174e3d8dd53b6347222d3b85d", None, NO)			// Slot Racers
	Record(@"530c7883fed4c5b9d78e35d48770b56e328999a3", Atari8k, NO)		// Smurfs: Rescue in Gargamel's Castle
	Record(@"c0ae3965fcfab0294f770af0af57d7d1adc17750", Atari8k, NO)		// Smurfs Save the Day
	Record(@"e7bf450cf3a3f40de9d24d89968a4bc89b53cb18", None, NO)			// Snail Against Squirrel
	Record(@"843e3c2fc71af2db3f2ae98eb350fde26334cfd1", None, NO)			// Sneak 'n Peak
	Record(@"972bc0a77e76f3e4e1270ec1c2fc395e9826bc07", Atari8k, NO)		// Snoopy and the Red Baron
	Record(@"9d725002e94b04e29d8cbce3c71d3bb2a84352fa", None, NO)			// Soccer
	Record(@"09ea74f14db8d21ea785d0c8209ed670e4ce88be", Atari8k, NO)		// Solar Fox
	Record(@"ec65ef9e47239a7d15db9bca7e625b166e8ac242", None, NO)			// Solar Storm
	Record(@"33b16fbc95c2cdc52d84d98ca471f10dae3f9dbf", Atari16k, NO)		// Solaris
	Record(@"ae3009e921f23254bb71f67c8cb2d7d6de2845a5", Atari8k, NO)		// Sorceror's Apprentice
	Record(@"70e912379060d834aa9fb2baa2e6a438f3b5d3b6", None, NO)			// Sorceror
	Record(@"560563613bc309a532d611f11a1cf2b9af1e2f16", None, NO)			// Space Attack
	Record(@"26c6c47e9b654e81f47875c5fcb4e6212125f329", None, NO)			// Space Canyon
	Record(@"b757b883ee114054c650027f3b9a8f15548cbf32", None, NO)			// Space Cavern
	Record(@"31d9668fe5812c3d2e076987ca327ac6b2e280bf", None, NO)			// Space Invaders
	Record(@"5bdd8af54020fa43065750bd4239a497695d403b", None, NO)			// Space Jockey
	Record(@"bcec5a66f8dff1a751769626b0fce305fab44ca2", Atari8k, NO)		// Space Shuttle
	Record(@"ce4432bb48921a3565d996b80b65fdf73bbfc39b", None, NO)			// Space Tunnel
	Record(@"23510ba617431097668eaf104aa1e36233173093", None, NO)			// Space War
	Record(@"b356294e35827bf81add95fee5453b0ca0f497ad", None, NO)			// Spacechase
	Record(@"983b1aff97ab1243e283ba62d3a6a75ad186d225", None, NO)			// SpaceMaster X-7
	Record(@"06820ad3c957913847f9849d920bc8725f535f11", None, NO)			// Spider Fighter
	Record(@"5d6f918bba4bd046e85b707da3b7d643cc2e1f1f", None, NO)			// Spider Maze
	Record(@"60af23a860b33e1a85081b8de779d2ddfe36b19a", None, NO)			// Spider Monster
	Record(@"912c5f5571ac59a6782da412183cdd6277345816", None, NO)			// Spider-Man
	Record(@"904118b0c1be484782ec2a60a24436059608b36d", None, NO)			// Spiderdroid
	Record(@"205241a12778829981e9281d9c6fa137f11e1376", Atari8k, NO)		// Spike's Peak
	Record(@"165de0ebca628eb1e9f564390c9eedfe289c7a1d", None, NO)			// Spitfire Attack
	Record(@"6da0aa8aa40cd9c78dc014deb9074529688d91d0", Tigervision, NO)	// Springer
	Record(@"c0e29b86fc1cc41a1c8afa37572c3c5698ae70b2", Atari16k, YES)		// Sprint Master
	Record(@"1d0acf064d06a026a04b6028285db78c834e9854", Atari8k, NO)		// Spy Hunter
	Record(@"033148faebc97d4ed3a86c97fe0cdee21bd261f7", None, NO)			// Squeeze Box
	Record(@"11a9dd44787f011ec540159248377cb27fb8f7bb", None, NO)			// Squoosh
	Record(@"46aabde3074acded8890a2efa5586d6b8bd76b5d", None, NO)			// Sssnake
	Record(@"277184c4e61ced14393049a21a304e941d05993f", None, NO)			// Stampede
	Record(@"1b95e07437ddc1523d7ec21c460273e91dbf36c7", None, NO)			// Star Fox
	Record(@"3359aa7a6a5fa25beaa3ae5868d0034d52de9882", None, NO)			// Star Gunner
	Record(@"e10cce1a438c82bd499e1eb31a3f07d7254198f5", Atari8k, NO)		// Star Raiders
	Record(@"878e78ed46e29c44949d0904a2198826e412ed81", None, NO)			// Star Ship
	Record(@"de05d1ca8ad1e7a85df3faf25b1aa90b159afded", None, NO)			// Star Strike
	Record(@"61a3ebbffa0bfb761295c66e189b62915f4818d9", Atari8k, NO)		// Star Trek — Strategic Operations Simulator
	Record(@"ccc5b829c4aa71acb7976e741fdbf59c8ef9eb55", None, NO)			// Star Voyager
	Record(@"c9d201935bbe6373793241ba9c03cc02f1df31c9", ParkerBros, NO)		// Star Wars — Ewok Adventure
	Record(@"8823fe3d8e3aeadc6b61ca51914e3b15aa13801c", ParkerBros, NO)		// Star Wars — The Arcade Game
	Record(@"ad5b2c9df558ab23ad2954fe49ed5b37a06009bf", None, NO)			// Star Wars — The Empire Strikes Back
	Record(@"4f87be0ef16a1d0389226d1fbda9b4c16b06e13e", Atari8k, YES)		// Stargate
	Record(@"814876ed270114912363e4718a84123dee213b6f", None, NO)			// StarMaster
	Record(@"e56ef1c0313d6d04e25446c4e34f9bb7eda8efac", None, NO)			// Steeplechase copy
	Record(@"bd7f0005fa018f13ed7e942c83c1751fb746a317", None, NO)			// Steeplechase
	Record(@"de2c146fa7a701d6c37728f5415563ce923a3e5d", None, NO)			// Stella_Lives!
	Record(@"7d97d014c22a2ed3a5bc4b310f5a7be1b1d3520f", None, NO)			// Stellar Track
	Record(@"9d6decda6e8ab263f7380ff662c814b8cb8caf34", None, NO)			// Strategy X
	Record(@"7ca8f9cd74cfa505c493757ff37bf127ff467bb4", None, NO)			// Strawberry Shortcake — Musical Match-Ups
	Record(@"bffb3d41916c83398624151eb00aa2a3acd23ab8", None, NO)			// Street Racer
	Record(@"2cfe280fdbb6b5c8cda8a4620df12a5154e123be", None, NO)			// Stronghold
	Record(@"2e19d7e16cf17682b043baaa30e345e6fa4540e5", None, NO)			// Stunt Cycle
	Record(@"3aec7ea8af72bbe105b9d2903a92f5ad2b37bddb", None, NO)			// Stunt Man
	Record(@"ccd75f0141b917656ef2b86c068fba3238d18a0c", None, NO)			// Sub-Scan
	Record(@"b22ba7cbde60a21ecbbe3953cc4a5c0bf007cc26", None, NO)			// Submarine Commander
	Record(@"2abc6bbcab27985f19e42915530fd556b6b1ae23", Atari8k, NO)		// Subterrenea
	Record(@"65f4a708e6af565f1f75d0fbdc8942cb149cf299", Atari16k, NO)		// Summer Games
	Record(@"b066a60ea1df1db0a55271c7608b0e19e4d18a1e", Atari16k, NO)		// Super Baseball
	Record(@"e380e243c671e954e86aa1a3a0bfeb36d5e0c3e2", None, NO)			// Super Breakout
	Record(@"dfce4d6436f91d8d385f8b01f0d8e3488400407b", None, NO)			// Super Challenge Baseball
	Record(@"5c1338ec76828cfa4a85b5bd8db1c00c8095c330", None, NO)			// Super Challenge Football
	Record(@"bac0a0256509f8fd1feea93d74ba4c7d82c1edc6", ParkerBros, NO)		// Super Cobra
	Record(@"eaca6b474fd552ab4aaf75526618828165a91934", Atari16k, YES)		// Super Football
	Record(@"b9dee027c8d7dd2a46be111ab0b8363c1becc081", None, NO)			// Superman
	Record(@"cf84e21ada55730d689cfac7d26e2295317222bc", Atari8k, NO)		// Surf's Up
	Record(@"e754c8985ca7f5780c23a856656099b710e89919", None, NO)			// Surfer's Paradise
	Record(@"b7988373b81992d08056560d15d3e32d9d3888bc", None, NO)			// Surround
	Record(@"6c993b4c70cfed390f1f436fdbaa1f81495be18e", None, NO)			// Survival Run
	Record(@"e2b3b43cadf2f2c91c1ec615651ff9b1e295d065", None, NO)			// sv2k12
	Record(@"0d59545b22e15019a33de16999a57dae1f998283", None, NO)			// Swordfight
	Record(@"05db3d09fa3dac80c70aae2e39f1ad7c31c62f02", Atari8k, NO)		// SwordQuest — EarthWorld
	Record(@"5c3cf976edbea5ded66634a284787f965616d97e", Atari8k, NO)		// SwordQuest — FireWorld
	Record(@"569fcb67ca1674b48e2f3a2e7af7077a374402de", Atari8k, NO)		// SwordQuest — WaterWorld
	Record(@"55e98fe14b07460734781a6aa2f4f1646830c0af", None, NO)			// Tac-Scan
	Record(@"13a9d86cbde32a1478ef0c7ef412427b13bd6222", None, NO)			// Tanks But No Tanks
	Record(@"ee8bc1710a67c33e9f95bb05cc3d8f841093fde2", None, NO)			// Tapeworm
	Record(@"e986e1818e747beb9b33ce4dff1cdc6b55bdb620", Atari8k, NO)		// Tapper
	Record(@"bae73700ba6532e9e6415b6471d115bdb7805464", None, NO)			// Task Force
	Record(@"7aaf6be610ba6ea1205bdd5ed60838ccb8280d57", Atari8k, NO)		// Tax Avoiders
	Record(@"476f0c565f54accecafd72c63b0464f469ed20ea", Atari8k, NO)		// Taz
	Record(@"7efc0ebe334dde84e25fa020ecde4fddcbea9e8f", Atari8k, NO)		// Telepathy
	Record(@"bf4d570c1c738a4d6d00237e25c62e9c3225f98f", Atari8k, NO)		// Tempest
	Record(@"3d30e7ed617168d852923def2000c9c0a8b728c6", None, NO)			// Tennis
	Record(@"53413577afe7def1d390e3892c45822405513c07", Atari8k, NO)		// The A-Team
	Record(@"1f834923eac271bf04c18621ac2aada68d426917", None, NO)			// The Earth Dies Screaming
	Record(@"5a641caa2ab3c7c0cd5deb027acbc58efccf8d6a", None, NO)			// The Music Machine
	Record(@"717122a4184bc8db41e65ab7c369c40b21c048d9", None, NO)			// The Texas Chainsaw Massacre
	Record(@"49bebad3e2eb210591be709a6ec7e4f0864265ab", None, NO)			// This Planet Sucks
	Record(@"9a52fa88bd7455044f00548e9615452131d1c711", None, NO)			// Threshold
	Record(@"09608cfaa7c6e9638f12a1cff9dd5036c9effa43", Atari16k, NO)		// Thrust
	Record(@"3cc8bcc0ff5164303433f469aa4da2eb256d1ad0", None, NO)			// Thunderground
	Record(@"53ee70d4b35ee3df3ffb95fa360bddb4f2f56ab2", ActivisionStack, NO)// Thwocker
	Record(@"387358514964d0b6b55f9431576a59b55869f7ab", Atari8k, NO)		// Time Pilot
	Record(@"979d9b0b0f32b40c0a0568be65a0bc5ef36ca6d0", Atari8k, NO)		// Title Match Pro Wrestling
	Record(@"fcd339065a012c9fe47efbec62969cbc32f3fbf0", Atari8k, NO)		// Tomarc the Barbarian
	Record(@"d82ac7237df54cc8688e3074b58433a7dd6b7d11", ParkerBros, NO)		// Tooth Protectors
	Record(@"b344b3e042447afbb3e40292dc4ca063d5d1110d", None, NO)			// Towering Inferno
	Record(@"005a6a53f5a856f0bdbca519af1ef236aaa1494d", Atari16k, NO)		// Track and Field
	Record(@"9a9917d82362c77b4d396f56966219fc248edf47", None, NO)			// Treasure Below
	Record(@"86c563db11db9afbffbd73c55e9fae9b2f69be4f", None, NO)			// Trick Shot
	Record(@"0ec58a3a5a27d1b82a5f9aabab02f9a8387b6956", None, NO)			// TRON — Deadly Discs
	Record(@"fc1a0b58765a7dcbd8e33562e1074ddd9e0ac624", CBSRamPlus, NO)		// Tunnel Runner
	Record(@"1162fe46977f01b4d25efab813e0d05ec90aeadc", None, NO)			// Turmoil
	Record(@"a4d6bac854a70d2c55946932f1511cc62db7d4aa", ParkerBros, NO)		// Tutankham
	Record(@"bd0ca4884c85db2323f5a4be5266aabb99d84542", None, NO)			// TVNoise.bin
	Record(@"286106fb973530bc3e2af13240f28c4bcb37e642", None, NO)			// Universal Chaos
	Record(@"6bde671a50330af154ed15e73fdba3fa55f23d87", Atari8k, NO)		// Up 'n Down
	Record(@"01475d037cb7a2a892be09d67083102fa9159216", Atari8k, NO)		// Vanguard
	Record(@"dce98883e813d77e03a5de975d4c52bfb34e7f77", None, NO)			// Vault Assault
	Record(@"17626ae7bfd10bcde14903040baee0923ecf41dd", None, NO)			// Venture II
	Record(@"0305dfc99bf192e53452a1e0408ccc148940afcd", None, NO)			// Venture
	Record(@"babae88a832b76d8c5af6ea63b8f10a0da5bb992", None, NO)			// Video Checkers
	Record(@"043ef523e4fcb9fc2fc2fda21f15671bf8620fc3", None, NO)			// Video Chess
	Record(@"1554b146d076b64776bf49136cea01f60eeba4c1", None, NO)			// Video Jogger
	Record(@"3b18db73933747851eba9a0ffa3c12b9f602a95c", CommaVid, NO)		// Video Life
	Record(@"1ffe89d79d55adabc0916b95cc37e18619ef7830", None, NO)			// Video Olympics
	Record(@"2c16c1a6374c8e22275d152d93dd31ffba26271f", None, NO)			// Video Pinball
	Record(@"dec2a3e9b366dce2b63dc1c13662d3f22420a22e", None, NO)			// Video Reflex
	Record(@"a345a5696f1d63a879e7bb7e3a74c825e97ef7c3", None, NO)			// Video Simon
	Record(@"2ebd0f43ee76833f75759ac1bbb45a8e0c3b86e9", None, NO)			// Vulture Attack
	Record(@"73072295721453065d62d9136343b81310a4d225", None, NO)			// Wabbit
	Record(@"482bd349222b8c702e125c27fd516e73af13967b", None, NO)			// Wall Ball
	Record(@"6de42bbc4766b26301e291ba00f7f7a9ac748639", None, NO)			// Wall Break
	Record(@"2d7563d337cbc0cdf4fc14f69853ab6757697788", None, NO)			// Warlords
	Record(@"232a370519a7fcce121e15f850d0d3671909f8b8", None, NO)			// Warplock
	Record(@"e325c5c0501ff527f06e6518526f9eefed309e89", None, NO)			// Waring Worms
	Record(@"472215fcb46cec905576d539efc8043488efc4ed", None, NO)			// Westward Ho
	Record(@"16df34446af2e6035ca871a00e1e8a008cfb8df4", Atari8k, NO)		// Wing War
	Record(@"6850d329e8ab403bdae38850665a2eff91278e92", Atari16k, NO)		// Winter Games
	Record(@"326e5e63a54ec6a0231fd38e62e352004d4719fe", None, NO)			// Wizard of Wor
	Record(@"e4b0f68abff3273cdd2b973639d607ae4a700adc", None, NO)			// Wizard
	Record(@"806c5a8a7b042a1a3ada1b6f29451a3446f93da3", None, NO)			// Word Zapper
	Record(@"36a1e73eb5aa5c3cd0b01af5117d19b8c36071e4", None, NO)			// Worm War 1
	Record(@"1c5d151e86c0a0bbdf3b33ef153888c6be78c36b", None, NO)			// X-Man
	Record(@"160b6e36437ad6acbc2686fbde1002e2fa88c5fb", Atari16k, NO)		// Xenophobe
	Record(@"73133b81196e5cbc1cec99eefc1223ddb8f4ca83", Atari8k, NO)		// Xevious
	Record(@"6a1e0142c6886a6589a58e029e5aec6b72f7d27f", None, NO)			// Yahtzee
	Record(@"e2cd8996c1cf929e29130690024d1ec23d3b0bde", None, NO)			// Yars' Revenge
	Record(@"58c2f6abc5599cd35c0e722f24bcc128ac8f9a30", Atari8k, NO)		// Zaxxon
};
#undef Record

@interface AtariStaticAnalyserTests : XCTestCase
@end

@implementation AtariStaticAnalyserTests

- (void)testROMsOfSize:(NSInteger)size
{
	NSString *basePath = [[[NSBundle bundleForClass:[self class]] resourcePath] stringByAppendingPathComponent:@"Atari ROMs"];
	for(NSString *testFile in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:basePath error:nil])
	{
		NSString *fullPath = [basePath stringByAppendingPathComponent:testFile];

		// get a SHA1 for the file
		NSData *fileData = [NSData dataWithContentsOfFile:fullPath];
		if(size > 0 && [fileData length] != (NSUInteger)size) continue;
		uint8_t sha1Bytes[CC_SHA1_DIGEST_LENGTH];
		CC_SHA1([fileData bytes], (CC_LONG)[fileData length], sha1Bytes);
		NSMutableString *sha1 = [[NSMutableString alloc] init];
		for(int c = 0; c < CC_SHA1_DIGEST_LENGTH; c++) [sha1 appendFormat:@"%02x", sha1Bytes[c]];

		// get an analysis of the file
		auto targets = Analyser::Static::GetTargets([fullPath UTF8String]);

		// grab the ROM record
		AtariROMRecord *romRecord = romRecordsBySHA1[sha1];
		if(!romRecord) continue;

		// assert equality
		auto *const atari_target = dynamic_cast<Analyser::Static::Atari2600::Target *>(targets.front().get());
		XCTAssert(atari_target != nullptr);
		XCTAssert(atari_target->paging_model == romRecord.pagingModel, @"%@; should be %d, is %d", testFile, romRecord.pagingModel, atari_target->paging_model);
		XCTAssert(atari_target->uses_superchip == romRecord.usesSuperchip, @"%@; should be %@", testFile, romRecord.usesSuperchip ? @"true" : @"false");
	}
}

- (void)testAtariROMs	{	[self testROMsOfSize:-1];		}	// This will duplicate all tests below, but also catch anything that isn't 2, 4, 8, 12, 16 or 32kb in size.
- (void)test2kROMs		{	[self testROMsOfSize:2048];		}
- (void)test4kROMs		{	[self testROMsOfSize:4096];		}
- (void)test8kROMs		{	[self testROMsOfSize:8192];		}
- (void)test12kROMs		{	[self testROMsOfSize:12288];	}
- (void)test16kROMs		{	[self testROMsOfSize:16384];	}
- (void)test32kROMs		{	[self testROMsOfSize:32768];	}

@end

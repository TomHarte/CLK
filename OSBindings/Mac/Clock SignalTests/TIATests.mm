//
//  TIATests.m
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "TIA.hpp"

static uint8_t *line;
static void receive_line(uint8_t *next_line)
{
	line = next_line;
}

@interface TIATests : XCTestCase
@end

@implementation TIATests {
	std::unique_ptr<Atari2600::TIA> _tia;
}

- (void)setUp
{
	[super setUp];
	std::function<void(uint8_t *)> function = receive_line;
	_tia = std::make_unique<Atari2600::TIA>(function);
	line = nullptr;

	_tia->set_playfield(0, 0x00);
	_tia->set_playfield(1, 0x00);
	_tia->set_playfield(2, 0x00);
	_tia->set_player_graphic(0, 0x00);
	_tia->set_player_graphic(1, 0x00);
	_tia->set_ball_enable(false);
	_tia->set_missile_enable(0, false);
	_tia->set_missile_enable(1, false);
}

- (void)testReflectedPlayfield
{
	// set reflected, bit pattern 1000
	_tia->set_playfield_control_and_ball_size(1);
	_tia->set_playfield(0, 0x10);
	_tia->set_playfield(1, 0xf0);
	_tia->set_playfield(2, 0x0e);
	_tia->run_for(Cycles(228));

	XCTAssert(line != nullptr, @"228 cycles should have ended the line");

	uint8_t expected_line[] = {
		1, 1, 1, 1,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		1, 1, 1, 1
	};
	XCTAssert(!memcmp(expected_line, line, sizeof(expected_line)));
}

- (void)testRepeatedPlayfield
{
	// set reflected, bit pattern 1000
	_tia->set_playfield_control_and_ball_size(0);
	_tia->set_playfield(0, 0x10);
	_tia->set_playfield(1, 0xf0);
	_tia->set_playfield(2, 0x0e);

	_tia->run_for(Cycles(228));
	XCTAssert(line != nullptr, @"228 cycles should have ended the line");

	uint8_t expected_line[] = {
		1, 1, 1, 1,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		1, 1, 1, 1,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		1, 1, 1, 1,		1, 1, 1, 1,		1, 1, 1, 1,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
	};
	XCTAssert(!memcmp(expected_line, line, sizeof(expected_line)));
}

- (void)testSinglePlayer
{
	// set a player graphic, reset position so that it'll appear from column 1
	_tia->set_player_graphic(0, 0xff);
	_tia->set_player_position(0);

	_tia->run_for(Cycles(228));
	uint8_t first_expected_line[] = {
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
	};
	XCTAssert(line != nullptr, @"228 cycles should have ended the line");
	XCTAssert(!memcmp(first_expected_line, line, sizeof(first_expected_line)));
	line = nullptr;

	_tia->run_for(Cycles(228));
	uint8_t second_expected_line[] = {
		0, 4, 4, 4,		4, 4, 4, 4,		4, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,
	};
	XCTAssert(line != nullptr, @"228 cycles should have ended the line");
	XCTAssert(!memcmp(second_expected_line, line, sizeof(second_expected_line)));
}

@end

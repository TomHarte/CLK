//
//  ArrayBuilderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 19/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "ArrayBuilderTests.h"
#include "ArrayBuilder.hpp"

static NSData *inputData, *outputData;

static void setData(bool is_input, uint8_t *data, size_t size)
{
	NSData *dataObject = [NSData dataWithBytes:data length:size];
	if(is_input) inputData = dataObject; else outputData = dataObject;
}

@implementation ArrayBuilderTests

+ (void)setUp
{
	inputData = nil;
	outputData = nil;
}

- (void)assertMonotonicForInputSize:(size_t)inputSize outputSize:(size_t)outputSize
{
	XCTAssert(inputData != nil, @"Should have received some input data");
	XCTAssert(outputData != nil, @"Should have received some output data");

	XCTAssert(inputData.length == inputSize, @"Input data should be %lu bytes long, was %lu", inputSize, (unsigned long)inputData.length);
	XCTAssert(outputData.length == outputSize, @"Output data should be %lu bytes long, was %lu", outputSize, (unsigned long)outputData.length);

	if(inputData.length == inputSize && outputData.length == outputSize)
	{
		uint8_t *input = (uint8_t *)inputData.bytes;
		uint8_t *output = (uint8_t *)outputData.bytes;

		for(int c = 0; c < inputSize; c++) XCTAssert(input[c] == c, @"Input item %d should be %d, was %d", c, c, input[c]);
		for(int c = 0; c < outputSize; c++) XCTAssert(output[c] == c + 0x80, @"Output item %d should be %d, was %d", c, c+0x80, output[c]);
	}
}

- (std::function<void(uint8_t *input, size_t input_size, uint8_t *output, size_t output_size)>)emptyFlushFunction
{
	return [=] (uint8_t *input, size_t input_size, uint8_t *output, size_t output_size) {};
}

- (void)testSingleWriteSingleFlush
{
	Outputs::CRT::ArrayBuilder arrayBuilder(200, 100, setData);

	uint8_t *input = arrayBuilder.get_input_storage(5);
	uint8_t *output = arrayBuilder.get_output_storage(3);

	for(int c = 0; c < 5; c++) input[c] = c;
	for(int c = 0; c < 3; c++) output[c] = c + 0x80;

	arrayBuilder.flush(self.emptyFlushFunction);
	arrayBuilder.submit();

	[self assertMonotonicForInputSize:5 outputSize:3];
}

- (void)testDoubleWriteSingleFlush
{
	Outputs::CRT::ArrayBuilder arrayBuilder(200, 100, setData);
	uint8_t *input;
	uint8_t *output;

	input = arrayBuilder.get_input_storage(2);
	output = arrayBuilder.get_output_storage(2);

	for(int c = 0; c < 2; c++) input[c] = c;
	for(int c = 0; c < 2; c++) output[c] = c + 0x80;

	input = arrayBuilder.get_input_storage(2);
	output = arrayBuilder.get_output_storage(2);

	for(int c = 0; c < 2; c++) input[c] = c+2;
	for(int c = 0; c < 2; c++) output[c] = c+2 + 0x80;

	arrayBuilder.flush(self.emptyFlushFunction);
	arrayBuilder.submit();

	[self assertMonotonicForInputSize:4 outputSize:4];
}

- (void)testSubmitWithoutFlush
{
	Outputs::CRT::ArrayBuilder arrayBuilder(200, 100, setData);

	arrayBuilder.get_input_storage(5);
	arrayBuilder.get_input_storage(8);
	arrayBuilder.get_output_storage(6);
	arrayBuilder.get_input_storage(12);
	arrayBuilder.get_output_storage(3);

	arrayBuilder.submit();

	XCTAssert(inputData.length == 0, @"No input data should have been received; %lu bytes were received", (unsigned long)inputData.length);
	XCTAssert(outputData.length == 0, @"No output data should have been received; %lu bytes were received", (unsigned long)outputData.length);

	arrayBuilder.flush(self.emptyFlushFunction);
	arrayBuilder.submit();

	XCTAssert(inputData.length == 25, @"All input data should have been received; %lu bytes were received", (unsigned long)inputData.length);
	XCTAssert(outputData.length == 9, @"All output data should have been received; %lu bytes were received", (unsigned long)outputData.length);
}

- (void)testSubmitContinuity
{
	Outputs::CRT::ArrayBuilder arrayBuilder(200, 100, setData);

	arrayBuilder.get_input_storage(5);
	arrayBuilder.get_output_storage(5);

	arrayBuilder.flush(self.emptyFlushFunction);

	uint8_t *input = arrayBuilder.get_input_storage(5);
	uint8_t *output = arrayBuilder.get_output_storage(5);

	arrayBuilder.submit();

	for(int c = 0; c < 5; c++) input[c] = c;
	for(int c = 0; c < 5; c++) output[c] = c + 0x80;

	arrayBuilder.flush(self.emptyFlushFunction);
	arrayBuilder.submit();

	[self assertMonotonicForInputSize:5 outputSize:5];
}

@end

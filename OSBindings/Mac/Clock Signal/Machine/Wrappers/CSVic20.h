//
//  CSVic20.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSKeyboardMachine.h"
#import "CSFastLoading.h"

typedef NS_ENUM(NSInteger, CSVic20Country)
{
	CSVic20CountryDanish,
	CSVic20CountryEuropean,
	CSVic20CountryJapanese,
	CSVic20CountrySwedish,
	CSVic20CountryAmerican
};

typedef NS_ENUM(NSInteger, CSVic20MemorySize)
{
	CSVic20MemorySize5Kb,
	CSVic20MemorySize8Kb,
	CSVic20MemorySize32Kb,
};

@interface CSVic20 : CSMachine <CSKeyboardMachine, CSFastLoading>

- (instancetype)init;

@property (nonatomic, assign) BOOL useFastLoadingHack;
@property (nonatomic, assign) CSVic20Country country;
@property (nonatomic, assign) CSVic20MemorySize memorySize;

@end

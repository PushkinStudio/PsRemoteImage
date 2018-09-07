// Copyright 2018 Pushkin Studio. All Rights Reserved.

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "CoreMinimal.h"

// You should place include statements to your module's private header files here.  You only need to
// add includes for headers that are used in most of your module's source files though.
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPsRemoteImage, Log, All);

#define VA_FUNC (FString(__FUNCTION__))								// Current Class Name + Function Name where this is called
#define VA_LINE (FString::FromInt(__LINE__))						// Current Line Number in the code where this is called
#define VA_FUNC_LINE (VA_FUNC + "(" + VA_LINE + ")")				// Current Class and Line Number where this is called!

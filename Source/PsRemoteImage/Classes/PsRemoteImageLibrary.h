// Copyright 2015-2018 Mail.Ru Group. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "PsRemoteImageLibrary.generated.h"

UCLASS()
class PSREMOTEIMAGE_API UPsRemoteImageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	
public:
	/** Get cached directory path */
	static const FString& GetCacheDirectory();
	
	/** Clear images cache */
	UFUNCTION(BlueprintCallable, Category = PSRemoteImage)
	static void ClearImageCache();
};

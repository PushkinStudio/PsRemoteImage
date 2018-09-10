// Copyright 2015-2018 Mail.Ru Group. All Rights Reserved.

#include "PsRemoteImagePrivatePCH.h"
#include "PsRemoteImageLibrary.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

UPsRemoteImageLibrary::UPsRemoteImageLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FString& UPsRemoteImageLibrary::GetCacheDirectory()
{
	static const FString Path = FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir()), TEXT("PSRemoteImage"), TEXT("Cache"));
	return Path;
}

void UPsRemoteImageLibrary::ClearImageCache()
{
	const auto& Path = GetCacheDirectory();
	FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*Path);
}

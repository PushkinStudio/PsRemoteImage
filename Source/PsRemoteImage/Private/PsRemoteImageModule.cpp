// Copyright 2015-2018 Mail.Ru Group. All Rights Reserved.

#include "PsRemoteImagePrivatePCH.h"
#include "PsRemoteImageModule.h"

#define LOCTEXT_NAMESPACE "PsRemoteImageModule"

void PsRemoteImageModule::StartupModule()
{
}

void PsRemoteImageModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(PsRemoteImageModule, PsRemoteImage);

DEFINE_LOG_CATEGORY(LogPsRemoteImage);

#undef LOCTEXT_NAMESPACE

// Copyright 2015-2018 Mail.Ru Group. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
    public class PsRemoteImage : ModuleRules
    {
        public PsRemoteImage(ReadOnlyTargetRules Target) : base(Target)
        {
            //PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

            PrivateIncludePaths.AddRange(
                new string[] {
                    "PsRemoteImage/Private",
                });

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine"
                });

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "ImageWrapper",
                    "HTTP",
                    "Slate",
                    "SlateCore",
                    "UMG"
                }
            );
        }
    }
}
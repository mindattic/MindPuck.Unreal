using UnrealBuildTool;
using System.Collections.Generic;

public class MindPuckTarget : TargetRules
{
	public MindPuckTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MindPuck");
	}
}

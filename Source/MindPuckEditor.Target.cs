using UnrealBuildTool;
using System.Collections.Generic;

public class MindPuckEditorTarget : TargetRules
{
	public MindPuckEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MindPuck");
	}
}

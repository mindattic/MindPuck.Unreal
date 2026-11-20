using UnrealBuildTool;

public class MindPuck : ModuleRules
{
	public MindPuck(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// The rules in Core/ are a port of the web game's TypeScript core. Strict IEEE arithmetic
		// keeps its results as close to the original's as the two math libraries allow.
		FPSemantics = FPSemanticsMode.Precise;

		// So that "Core/Level.h" and "Game/MindPuckTable.h" resolve from anywhere in the module.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"ProceduralMeshComponent",
			"MindAtticEyeTracker",
		});
	}
}

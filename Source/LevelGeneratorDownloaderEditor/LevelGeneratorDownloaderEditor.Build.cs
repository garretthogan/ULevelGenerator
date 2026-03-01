using UnrealBuildTool;

public class LevelGeneratorDownloaderEditor : ModuleRules
{
	public LevelGeneratorDownloaderEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetTools",
				"DesktopPlatform",
				"EditorStyle",
				"HTTP",
				"InputCore",
				"InterchangeCore",
				"InterchangeEngine",
				"InterchangePipelines",
				"LevelEditor",
				"MeshMergeUtilities",
				"PropertyEditor",
				"Projects",
				"ToolMenus",
				"UnrealEd",
				"WebBrowser"
			});
	}
}

using UnrealBuildTool;

public class MaterialLayoutPro : ModuleRules
{
	public MaterialLayoutPro(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"UnrealEd",
				"ToolMenus",
				"Projects",
				"RenderCore",
				"RHI",
				"MaterialEditor",
				"EditorWidgets",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"PropertyEditor",
				"ApplicationCore",
				"LevelEditor",
				"EditorSubsystem",
				"AssetRegistry",
				"DesktopPlatform",
				"ContentBrowser",
			"DeveloperSettings",
			"AppFramework",
			"Json",
			}
		);

		if ((Target.Version.MajorVersion == 4 && Target.Version.MinorVersion == 26)
		    || Target.Version.MajorVersion >= 5)
		{
			PublicDefinitions.Add("MLP_WITH_MATERIAL_EDITOR=1");
		}
		else
		{
			PublicDefinitions.Add("MLP_WITH_MATERIAL_EDITOR=0");
		}
	}
}

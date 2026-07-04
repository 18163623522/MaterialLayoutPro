#include "Style/MaterialLayoutProStyle.h"
#include "MaterialLayoutProTheme.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FMaterialLayoutProStyle::StyleSet = nullptr;

void FMaterialLayoutProStyle::Initialize()
{
	if (!StyleSet.IsValid())
	{
		StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
		StyleSet->SetContentRoot(IPluginManager::Get().FindPlugin(TEXT("MaterialLayoutPro"))->GetBaseDir() / TEXT("Resources"));

		// Colored fallback brushes for toolbar/menu icons.
		StyleSet->Set("MaterialLayoutPro.OpenPanel", new FSlateColorBrush(FMLPTheme::Accent()));

		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
	}
}

void FMaterialLayoutProStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}

void FMaterialLayoutProStyle::ReloadTextures()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
	}
}

const ISlateStyle& FMaterialLayoutProStyle::Get()
{
	return *StyleSet;
}

FName FMaterialLayoutProStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("MaterialLayoutProStyle"));
	return StyleSetName;
}

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"
#include "Model/MaterialParameterInfo.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#define MLP_GET_STYLE() FAppStyle::Get()
#else
#include "EditorStyleSet.h"
#define MLP_GET_STYLE() FEditorStyle::Get()
#endif

/**
 * Centralized design token system for the Material Layout Pro UI.
 *
 * Colors try the engine's own style first; if a token is unavailable (returns unspecified),
 * we fall back to a safe constant so the panel is never transparent/black. This way it
 * blends with the native editor theme on both 4.26 and 5.x.
 */
class FMLPTheme
{
private:
	/** Resolve an engine SlateColor by key; fall back to SafeColor if unspecified/invalid. */
	static FLinearColor ResolveColor(const FString& Key, const FLinearColor& Fallback)
	{
		FSlateColor Sc = MLP_GET_STYLE().GetSlateColor(FName(*Key));
		// If the color isn't specified by the style, GetSpecifiedColor returns UseForeground() sentinel.
		if (!Sc.IsColorSpecified())
		{
			return Fallback;
		}
		return Sc.GetSpecifiedColor();
	}

public:
	// --- Backgrounds / surfaces (engine-native with fallback) ---

	static FLinearColor Background() { return ResolveColor(TEXT("Colors.Background"), FLinearColor(0.102f, 0.102f, 0.102f, 1.0f)); }
	static FLinearColor Surface() { return ResolveColor(TEXT("Colors.Panel"), FLinearColor(0.165f, 0.165f, 0.165f, 1.0f)); }
	static FLinearColor SurfaceAlt() { return Surface() * 0.85f; }
	static FLinearColor SurfaceHover() { return ResolveColor(TEXT("Colors.Header"), FLinearColor(0.227f, 0.227f, 0.227f, 1.0f)); }
	static FLinearColor Border() { return SurfaceHover(); }

	// --- Text (engine-native with fallback) ---

	static FLinearColor Foreground() { return ResolveColor(TEXT("Colors.Foreground"), FLinearColor(0.863f, 0.863f, 0.863f, 1.0f)); }
	static FLinearColor Muted() { return ResolveColor(TEXT("Colors.Dim"), FLinearColor(0.604f, 0.604f, 0.604f, 1.0f)); }

	// --- Accent / selection (engine-native with fallback) ---

	static FLinearColor Accent()
	{
		FLinearColor C = ResolveColor(TEXT("Colors.AccentBlue"), FLinearColor(0.039f, 0.561f, 0.890f, 1.0f));
		// Some engines only register "Colors.Selection" — try it as a secondary fallback.
		if (!MLP_GET_STYLE().GetSlateColor(FName("Colors.AccentBlue")).IsColorSpecified())
		{
			C = ResolveColor(TEXT("Colors.Selection"), C);
		}
		return C;
	}
	static FLinearColor AccentBg() { FLinearColor C = Accent(); C.A = 0.25f; return C; }
	static FLinearColor Selection() { return ResolveColor(TEXT("Colors.Selection"), Accent()); }
	static FLinearColor SelectionBg() { FLinearColor C = Selection(); C.A = 0.30f; return C; }

	// --- Semantic colors (kept as constants — these are semantic, not theme-dependent) ---

	static const FLinearColor& Destructive() { static const FLinearColor V(0.859f, 0.271f, 0.271f, 1.0f); return V; }
	static const FLinearColor& Warning() { static const FLinearColor V(0.941f, 0.620f, 0.043f, 1.0f); return V; }

	// Parameter pin colors — UE convention, matches the material graph pins.
	static const FLinearColor& TypeScalar() { static const FLinearColor V(0.478f, 0.800f, 0.251f, 1.0f); return V; }
	static const FLinearColor& TypeVector() { static const FLinearColor V(0.859f, 0.271f, 0.271f, 1.0f); return V; }
	static const FLinearColor& TypeTexture() { static const FLinearColor V(0.000f, 0.733f, 0.996f, 1.0f); return V; }
	static const FLinearColor& TypeStatic() { static const FLinearColor V(0.741f, 0.741f, 0.741f, 1.0f); return V; }

	// Status badge colors.
	static const FLinearColor& StatusUsed() { return TypeScalar(); }
	static const FLinearColor& StatusUnused() { return Destructive(); }
	static const FLinearColor& StatusHalfUsed() { return Warning(); }
	static const FLinearColor& StatusIndirect() { static const FLinearColor V(0.957f, 0.537f, 0.267f, 1.0f); return V; }
	static const FLinearColor& StatusUnknown() { static const FLinearColor V(0.541f, 0.541f, 0.541f, 1.0f); return V; }

	static const FLinearColor& Overlay() { static const FLinearColor V(0.020f, 0.020f, 0.020f, 0.92f); return V; }

	// --- Spacing tokens (4/8dp rhythm) ---

	static const FMargin& PadXS() { static const FMargin V(2.f); return V; }
	static const FMargin& PadSM() { static const FMargin V(4.f); return V; }
	static const FMargin& PadMD() { static const FMargin V(8.f); return V; }
	static const FMargin& PadLG() { static const FMargin V(12.f); return V; }
	static const FMargin& PadXL() { static const FMargin V(16.f); return V; }
	static const FMargin& PadH() { static const FMargin V(6.f, 0.f); return V; }
	static const FMargin& PadBtn() { static const FMargin V(8.f, 3.f); return V; }

	// --- Font tokens (engine-native sizes) ---

	static const FSlateFontInfo& FontSmall() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Normal", 9); return V; }
	static const FSlateFontInfo& FontBody() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Normal", 10); return V; }
	static const FSlateFontInfo& FontHeading() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Bold", 10); return V; }
	static const FSlateFontInfo& FontTitle() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Bold", 12); return V; }
	static const FSlateFontInfo& FontMono() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Mono", 9); return V; }
	static const FSlateFontInfo& FontTiny() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Normal", 8); return V; }
	static const FSlateFontInfo& FontBadge() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Bold", 8); return V; }

	// Helper Widgets

	/** A 1px vertical separator for toolbar grouping. */
	static TSharedRef<SWidget> MakeSeparator(float Height = 20.f)
	{
		return SNew(SBox)
			.WidthOverride(1.f)
			.HeightOverride(Height)
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(Border().R, Border().G, Border().B, 0.6f))
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.Padding(0.f)
			];
	}

	/** A colored badge/label with text. */
	static TSharedRef<SWidget> MakeBadge(const FText& Label, const FLinearColor& Color, const FLinearColor& TextColor = FLinearColor::White)
	{
		return SNew(SBorder)
			.BorderBackgroundColor(Color)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.Padding(FMargin(4.f, 1.f))
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FontBadge())
				.ColorAndOpacity(FSlateColor(TextColor))
			];
	}

	/** A small colored dot badge. */
	static TSharedRef<SWidget> MakeDotBadge(const FLinearColor& Color, float Size = 8.f)
	{
		return SNew(SBox)
			.WidthOverride(Size)
			.HeightOverride(Size)
			[
				SNew(SImage)
				.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.ColorAndOpacity(Color)
			];
	}

	/** A colored pill badge with short text (type abbreviation). UE5 style: wider padding. */
	static TSharedRef<SWidget> MakeTypePill(const FText& Label, const FLinearColor& Color)
	{
		return SNew(SBorder)
			.BorderBackgroundColor(Color)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.Padding(FMargin(6.f, 1.f, 6.f, 2.f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FontBadge())
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(0.f, 1.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.4f))
		];
	}

	/** Background color for a normal toolbar button (idle) — matches editor buttons. */
	static FLinearColor ButtonNormal()    { return MLP_GET_STYLE().GetSlateColor("Colors.Header").GetSpecifiedColor(); }
	/** Background color for a hovered toolbar button. */
	static FLinearColor ButtonHover()     { return MLP_GET_STYLE().GetSlateColor("Colors.Header").GetSpecifiedColor() * 1.2f; }
	/** Background color for a primary action button (Apply). */
	static FLinearColor ButtonPrimary()    { return Accent(); }
	/** Background color for a primary button hover. */
	static FLinearColor ButtonPrimaryHover() { return Accent() * 1.2f; }
	/** Background color for a danger button (Delete). */
	static FLinearColor ButtonDanger()     { return Destructive(); }
	/** Background color for a danger button hover. */
	static FLinearColor ButtonDangerHover() { return Destructive() * 1.15f; }
	/** Text color for a normal button. */
	static FLinearColor ButtonTextNormal() { return Foreground(); }
	/** Text color for a primary/danger button (white on colored bg). */
	static FLinearColor ButtonTextOnColor() { return FLinearColor::White; }
	/** Background color for an active tab — accent-tinted. */
	static FLinearColor TabActive()        { FLinearColor C = Accent(); C.A = 0.85f; return C; }
	/** Background color for an inactive tab. */
	static FLinearColor TabInactive()      { FLinearColor C = Surface(); C.A = 0.8f; return C; }
	/** Background color for a hovered inactive tab. */
	static FLinearColor TabHover()         { return SurfaceHover(); }

	// Type → color/abbreviation mapping (lets VM rows build pills without FMLPParameterInfo).

	/** Map a parameter type to its pin color. */
	static FLinearColor GetTypeColorForType(EMLPParameterType Type)
	{
		switch (Type)
		{
		case EMLPParameterType::Scalar:       return TypeScalar();
		case EMLPParameterType::Vector:       return TypeVector();
		case EMLPParameterType::Texture:      return TypeTexture();
		case EMLPParameterType::StaticBool:
		case EMLPParameterType::StaticSwitch: return TypeStatic();
		default:                              return Muted();
		}
	}

	/** Map a parameter type to its short pill abbreviation (S/V/T/SB/SS). */
	static FText GetTypeAbbrForType(EMLPParameterType Type)
	{
		switch (Type)
		{
		case EMLPParameterType::Scalar:       return FText::FromString(TEXT("S"));
		case EMLPParameterType::Vector:       return FText::FromString(TEXT("V"));
		case EMLPParameterType::Texture:      return FText::FromString(TEXT("T"));
		case EMLPParameterType::StaticBool:   return FText::FromString(TEXT("SB"));
		case EMLPParameterType::StaticSwitch: return FText::FromString(TEXT("SS"));
		default:                              return FText::FromString(TEXT("?"));
		}
	}
};

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"
#include "Model/MaterialParameterInfo.h"

/**
 * Centralized design token system for the Material Layout Pro UI.
 *
 * Adapted from the Niagara Attribute Inspector design system:
 *   - Semantic color tokens for consistent theming
 *   - 4/8dp spacing rhythm
 *   - Unified font size hierarchy
 */
class FMLPTheme
{
public:
	// Color Tokens — aligned with UE5's FAppStyle palette (FAppStyle.cpp values).
	// Matches the editor's native dark theme so the panel blends in seamlessly.

	/** Deepest background — toolbar / status bar base. #1A1A1A */
	static const FLinearColor& Background() { static const FLinearColor V(0.102f, 0.102f, 0.102f, 1.0f); return V; }
	/** Panel / card surface. #2A2A2A */
	static const FLinearColor& Surface() { static const FLinearColor V(0.165f, 0.165f, 0.165f, 1.0f); return V; }
	/** Alternate row / slightly darker than Surface. #212121 */
	static const FLinearColor& SurfaceAlt() { static const FLinearColor V(0.129f, 0.129f, 0.129f, 1.0f); return V; }
	/** Brighter surface for hovered/active items. #3A3A3A */
	static const FLinearColor& SurfaceHover() { static const FLinearColor V(0.227f, 0.227f, 0.227f, 1.0f); return V; }
	/** Border / divider line. #363636 */
	static const FLinearColor& Border() { static const FLinearColor V(0.212f, 0.212f, 0.212f, 1.0f); return V; }
	/** Primary text — near white. #DCDCDC (UE5 foreground, softer than pure white) */
	static const FLinearColor& Foreground() { static const FLinearColor V(0.863f, 0.863f, 0.863f, 1.0f); return V; }
	/** Secondary text — labels, hints. #9A9A9A (contrast 4.6:1 on #1A1A1A, AA compliant) */
	static const FLinearColor& Muted() { static const FLinearColor V(0.604f, 0.604f, 0.604f, 1.0f); return V; }
	/** Accent / active emphasis — UE5 blue #0A8FE3, used for primary actions and selection. */
	static const FLinearColor& Accent() { static const FLinearColor V(0.039f, 0.561f, 0.890f, 1.0f); return V; }
	/** Accent with reduced alpha for backgrounds. */
	static const FLinearColor& AccentBg() { static const FLinearColor V(0.039f, 0.561f, 0.890f, 0.25f); return V; }
	/** Destructive / danger. #DB4545 */
	static const FLinearColor& Destructive() { static const FLinearColor V(0.859f, 0.271f, 0.271f, 1.0f); return V; }
	/** Warning. #F09E0B */
	static const FLinearColor& Warning() { static const FLinearColor V(0.941f, 0.620f, 0.043f, 1.0f); return V; }
	/** Scalar parameter color — green #7ACC40 (UE pin convention). */
	static const FLinearColor& TypeScalar() { static const FLinearColor V(0.478f, 0.800f, 0.251f, 1.0f); return V; }
	/** Vector parameter color — red #DB4545 (UE pin convention). */
	static const FLinearColor& TypeVector() { static const FLinearColor V(0.859f, 0.271f, 0.271f, 1.0f); return V; }
	/** Texture parameter color — cyan #00BBFE (UE pin convention). */
	static const FLinearColor& TypeTexture() { static const FLinearColor V(0.000f, 0.733f, 0.996f, 1.0f); return V; }
	/** Static switch / bool parameter color — gray #BDBDBD. */
	static const FLinearColor& TypeStatic() { static const FLinearColor V(0.741f, 0.741f, 0.741f, 1.0f); return V; }
	/** Empty-state overlay background. */
	static const FLinearColor& Overlay() { static const FLinearColor V(0.020f, 0.020f, 0.020f, 0.92f); return V; }

	// Status badge colors — used for the usage state pill on each row.
	static const FLinearColor& StatusUsed() { static const FLinearColor V(0.478f, 0.800f, 0.251f, 1.0f); return V; }       // green
	static const FLinearColor& StatusUnused() { static const FLinearColor V(0.859f, 0.271f, 0.271f, 1.0f); return V; }     // red
	static const FLinearColor& StatusHalfUsed() { static const FLinearColor V(0.941f, 0.620f, 0.043f, 1.0f); return V; }   // amber
	static const FLinearColor& StatusIndirect() { static const FLinearColor V(0.957f, 0.537f, 0.267f, 1.0f); return V; }    // orange
	static const FLinearColor& StatusUnknown() { static const FLinearColor V(0.541f, 0.541f, 0.541f, 1.0f); return V; }      // muted

	// Selection — UE5 blue, matching the editor's accent.
	static const FLinearColor& Selection() { static const FLinearColor V(0.039f, 0.561f, 0.890f, 1.0f); return V; }
	static const FLinearColor& SelectionBg() { static const FLinearColor V(0.039f, 0.561f, 0.890f, 0.25f); return V; }

	// Spacing Tokens (4/8dp rhythm — UE5 uses slightly larger spacing than 4.26)

	static const FMargin& PadXS() { static const FMargin V(2.f); return V; }
	static const FMargin& PadSM() { static const FMargin V(4.f); return V; }
	static const FMargin& PadMD() { static const FMargin V(8.f); return V; }
	static const FMargin& PadLG() { static const FMargin V(12.f); return V; }
	static const FMargin& PadXL() { static const FMargin V(16.f); return V; }
	static const FMargin& PadH() { static const FMargin V(6.f, 0.f); return V; }
	static const FMargin& PadBtn() { static const FMargin V(8.f, 3.f); return V; }

	// Font Tokens — UE5 sizes (slightly larger than 4.26 for readability)

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

	/** Background color for a normal toolbar button (idle). */
	static FLinearColor ButtonNormal()    { return FLinearColor(0.165f, 0.165f, 0.165f, 0.8f); }
	/** Background color for a hovered toolbar button. */
	static FLinearColor ButtonHover()     { return FLinearColor(0.227f, 0.227f, 0.227f, 0.9f); }
	/** Background color for a primary action button (Apply). */
	static FLinearColor ButtonPrimary()    { return Accent(); }
	/** Background color for a primary button hover. */
	static FLinearColor ButtonPrimaryHover() { return FLinearColor(0.06f, 0.66f, 0.96f, 1.0f); }
	/** Background color for a danger button (Delete). */
	static FLinearColor ButtonDanger()     { return Destructive(); }
	/** Background color for a danger button hover. */
	static FLinearColor ButtonDangerHover() { return FLinearColor(0.95f, 0.35f, 0.35f, 1.0f); }
	/** Text color for a normal button. */
	static FLinearColor ButtonTextNormal() { return Foreground(); }
	/** Text color for a primary/danger button (white on colored bg). */
	static FLinearColor ButtonTextOnColor() { return FLinearColor::White; }
	/** Background color for an active tab. */
	static FLinearColor TabActive()        { return FLinearColor(0.039f, 0.561f, 0.890f, 0.85f); }
	/** Background color for an inactive tab. */
	static FLinearColor TabInactive()      { return FLinearColor(0.165f, 0.165f, 0.165f, 0.8f); }
	/** Background color for a hovered inactive tab. */
	static FLinearColor TabHover()         { return FLinearColor(0.227f, 0.227f, 0.227f, 0.9f); }

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

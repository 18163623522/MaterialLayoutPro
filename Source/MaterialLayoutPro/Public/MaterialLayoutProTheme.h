#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"

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
	// Color Tokens (Dark Mode — slate based palette)

	/** Deepest background — toolbar / status bar base. */
	static const FLinearColor& Background() { static const FLinearColor V(0.058f, 0.090f, 0.165f, 1.0f); return V; }
	/** Panel / card surface. */
	static const FLinearColor& Surface() { static const FLinearColor V(0.118f, 0.161f, 0.231f, 1.0f); return V; }
	/** Alternate row / slightly darker than Surface. */
	static const FLinearColor& SurfaceAlt() { static const FLinearColor V(0.055f, 0.071f, 0.137f, 1.0f); return V; }
	/** Brighter surface for hovered/active items. */
	static const FLinearColor& SurfaceHover() { static const FLinearColor V(0.141f, 0.204f, 0.278f, 1.0f); return V; }
	/** Border / divider line. */
	static const FLinearColor& Border() { static const FLinearColor V(0.200f, 0.255f, 0.333f, 1.0f); return V; }
	/** Primary text — near white. */
	static const FLinearColor& Foreground() { static const FLinearColor V(0.973f, 0.980f, 0.988f, 1.0f); return V; }
	/** Secondary text — labels, hints. */
	static const FLinearColor& Muted() { static const FLinearColor V(0.580f, 0.639f, 0.721f, 1.0f); return V; }
	/** Accent / active emphasis. */
	static const FLinearColor& Accent() { static const FLinearColor V(0.133f, 0.773f, 0.369f, 1.0f); return V; }
	/** Accent with reduced alpha for backgrounds. */
	static const FLinearColor& AccentBg() { static const FLinearColor V(0.133f, 0.773f, 0.369f, 0.18f); return V; }
	/** Destructive / danger. */
	static const FLinearColor& Destructive() { static const FLinearColor V(0.937f, 0.267f, 0.267f, 1.0f); return V; }
	/** Warning. */
	static const FLinearColor& Warning() { static const FLinearColor V(0.961f, 0.620f, 0.043f, 1.0f); return V; }
	/** Scalar parameter color. */
	static const FLinearColor& TypeScalar() { static const FLinearColor V(0.329f, 0.827f, 0.984f, 1.0f); return V; }
	/** Vector parameter color. */
	static const FLinearColor& TypeVector() { static const FLinearColor V(0.937f, 0.443f, 0.957f, 1.0f); return V; }
	/** Texture parameter color. */
	static const FLinearColor& TypeTexture() { static const FLinearColor V(0.992f, 0.792f, 0.353f, 1.0f); return V; }
	/** Static switch / bool parameter color. */
	static const FLinearColor& TypeStatic() { static const FLinearColor V(0.741f, 0.741f, 0.741f, 1.0f); return V; }
	/** Empty-state overlay background. */
	static const FLinearColor& Overlay() { static const FLinearColor V(0.020f, 0.030f, 0.060f, 0.92f); return V; }

	// Status badge colors
	static const FLinearColor& StatusUsed() { static const FLinearColor V(0.133f, 0.773f, 0.369f, 1.0f); return V; }
	static const FLinearColor& StatusUnused() { static const FLinearColor V(0.937f, 0.267f, 0.267f, 1.0f); return V; }
	static const FLinearColor& StatusHalfUsed() { static const FLinearColor V(0.961f, 0.620f, 0.043f, 1.0f); return V; }
	static const FLinearColor& StatusIndirect() { static const FLinearColor V(0.957f, 0.537f, 0.267f, 1.0f); return V; }
	static const FLinearColor& StatusUnknown() { static const FLinearColor V(0.580f, 0.639f, 0.721f, 1.0f); return V; }

	// Selection color
	static const FLinearColor& Selection() { static const FLinearColor V(0.133f, 0.420f, 0.773f, 1.0f); return V; }
	static const FLinearColor& SelectionBg() { static const FLinearColor V(0.133f, 0.420f, 0.773f, 0.18f); return V; }

	// Spacing Tokens (4/8dp rhythm)

	static const FMargin& PadXS() { static const FMargin V(2.f); return V; }
	static const FMargin& PadSM() { static const FMargin V(4.f); return V; }
	static const FMargin& PadMD() { static const FMargin V(8.f); return V; }
	static const FMargin& PadLG() { static const FMargin V(12.f); return V; }
	static const FMargin& PadXL() { static const FMargin V(16.f); return V; }
	static const FMargin& PadH() { static const FMargin V(4.f, 0.f); return V; }
	static const FMargin& PadBtn() { static const FMargin V(4.f, 2.f, 4.f, 2.f); return V; }

	// Font Tokens

	static const FSlateFontInfo& FontSmall() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Normal", 8); return V; }
	static const FSlateFontInfo& FontBody() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Normal", 9); return V; }
	static const FSlateFontInfo& FontHeading() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Bold", 9); return V; }
	static const FSlateFontInfo& FontTitle() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Bold", 11); return V; }
	static const FSlateFontInfo& FontMono() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Mono", 8); return V; }
	static const FSlateFontInfo& FontTiny() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Normal", 7); return V; }
	static const FSlateFontInfo& FontBadge() { static const FSlateFontInfo V = FCoreStyle::GetDefaultFontStyle("Bold", 7); return V; }

	// Helper Widgets

	/** A 1px vertical separator for toolbar grouping. */
	static TSharedRef<SWidget> MakeSeparator(float Height = 16.f)
	{
		return SNew(SBox)
			.WidthOverride(1.f)
			.HeightOverride(Height)
			[
				SNew(SBorder)
				.BorderBackgroundColor(Border())
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
};

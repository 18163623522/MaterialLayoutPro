#include "Widgets/SMaterialParameterRow.h"
#include "MaterialLayoutProTheme.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Styling/CoreStyle.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SMaterialParameterRow"

void SMaterialParameterRow::Construct(const FArguments& InArgs)
{
	Item = InArgs._Item;
	bSelected = InArgs._bSelected;
	OnClicked = InArgs._OnClicked;
	OnGroupChanged = InArgs._OnGroupChanged;
	OnPriorityChanged = InArgs._OnPriorityChanged;

	if (!Item.IsValid())
	{
		ChildSlot
		[
			SNew(STextBlock).Text(LOCTEXT("InvalidRow", "Invalid"))
		];
		return;
	}

	const FSlateColor TypeColor(Item->GetTypeColor());
	const FSlateColor StatusColor(Item->GetUsageColor());
	const FLinearColor BgColor = bSelected ? FMLPTheme::SelectionBg() : FMLPTheme::Surface();

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(BgColor)
		.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
		.Padding(FMLPTheme::PadSM())
		.OnMouseButtonDown(this, &SMaterialParameterRow::OnRowClicked)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.05f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
					SNew(SBox)
					.WidthOverride(8.f)
					.HeightOverride(8.f)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("WhiteBrush"))
						.ColorAndOpacity(TypeColor)
					]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.30f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(STextBlock)
				.Text(FText::FromName(Item->Name))
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Foreground())
				.ToolTipText(FText::FromString(Item->ValueString))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.15f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(STextBlock)
				.Text(Item->GetDisplayTypeName())
				.Font(FMLPTheme::FontSmall())
				.ColorAndOpacity(TypeColor)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.25f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(SEditableTextBox)
				.Text(FText::FromName(Item->Group))
				.Font(FMLPTheme::FontBody())
				.OnTextCommitted(this, &SMaterialParameterRow::OnGroupTextCommitted)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.12f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(SNumericEntryBox<int32>)
					.Value(TOptional<int32>(Item->SortPriority))
					.Font(FMLPTheme::FontBody())
					.OnValueCommitted(this, &SMaterialParameterRow::OnPriorityValueCommitted)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.13f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
					SNew(SBorder)
					.BorderBackgroundColor(Item->GetUsageBgColor())
					.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
					.Padding(FMargin(4.f, 1.f))
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(Item->GetUsageLabel())
						.Font(FMLPTheme::FontBadge())
						.ColorAndOpacity(StatusColor)
					]
			]
		]
	];
}

FReply SMaterialParameterRow::OnRowClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnClicked.ExecuteIfBound(Item, MouseEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SMaterialParameterRow::OnGroupTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter && CommitType != ETextCommit::OnUserMovedFocus)
	{
		return;
	}

	if (Item.IsValid())
	{
		FName NewGroup(*NewText.ToString());
		if (NewGroup != Item->Group)
		{
			OnGroupChanged.ExecuteIfBound(Item, NewGroup);
		}
	}
}

void SMaterialParameterRow::OnPriorityValueCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
	if (Item.IsValid() && NewValue != Item->SortPriority)
	{
		OnPriorityChanged.ExecuteIfBound(Item, NewValue);
	}
}

#undef LOCTEXT_NAMESPACE
#include "Widgets/SMaterialParameterRow.h"
#include "MaterialLayoutProTheme.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Styling/CoreStyle.h"
#if ENGINE_MAJOR_VERSION >= 5
#define MLP_STYLE FAppStyle
#else
#define MLP_STYLE FEditorStyle
#endif
#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif
#include "Framework/Application/SlateApplication.h"
#include "Model/MaterialParameterDragDrop.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Widgets/Colors/SColorPicker.h"

#define LOCTEXT_NAMESPACE "SMaterialParameterRow"

void SMaterialParameterRow::Construct(const FArguments& InArgs)
{
	Item = InArgs._Item;
	bSelected = InArgs._bSelected;
	bIsRenaming = InArgs._bIsRenaming;
	bInstanceViewMode = InArgs._bInstanceViewMode;
	OnClicked = InArgs._OnClicked;
	OnDoubleClicked = InArgs._OnDoubleClicked;
	OnGroupChanged = InArgs._OnGroupChanged;
	OnPriorityChanged = InArgs._OnPriorityChanged;
	OnRenamed = InArgs._OnRenamed;
	OnDragDetectedDelegate = InArgs._OnDragDetected;
	OnDroppedDelegate = InArgs._OnDropped;
	OnScalarChangedDelegate = InArgs._OnScalarChanged;
	OnVectorChangedDelegate = InArgs._OnVectorChanged;
	OnTextureChangedDelegate = InArgs._OnTextureChanged;
	OnBoolChangedDelegate = InArgs._OnBoolChanged;

	if (!Item.IsValid())
	{
		ChildSlot
		[
			SNew(STextBlock).Text(LOCTEXT("InvalidRow", "无效"))
		];
		return;
	}

	const FSlateColor TypeColor(Item->GetTypeColor());
	const FSlateColor StatusColor(Item->GetUsageColor());

	// Dynamic background color — reads Item->bSelected (data layer) so it updates
	// without rebuilding the widget tree. Just needs an Invalidate to repaint.
	auto GetBgColor = [this]() -> FLinearColor
	{
		const bool bSel = Item.IsValid() && Item->bSelected;
		return bSel
			? FLinearColor(FMLPTheme::SelectionBg().R, FMLPTheme::SelectionBg().G, FMLPTheme::SelectionBg().B, 0.35f)
			: FLinearColor(FMLPTheme::Surface().R, FMLPTheme::Surface().G, FMLPTheme::Surface().B, 0.6f);
	};

	// Name widget: editable text when renaming, otherwise a read-only label.
	// Name tinted amber when a duplicate-name conflict is detected (diagnostic alert).
	const FLinearColor NameColor = Item->bHasDuplicateName ? FMLPTheme::Warning() : FMLPTheme::Foreground();

	TSharedRef<SWidget> NameWidget = bIsRenaming
		? StaticCastSharedRef<SWidget>(
			SNew(SEditableTextBox)
			.Text(FText::FromName(Item->Name))
			.Font(FMLPTheme::FontBody())
			.SelectAllTextWhenFocused(true)
			.OnTextCommitted(this, &SMaterialParameterRow::OnNameTextCommitted))
		: StaticCastSharedRef<SWidget>(
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor::Transparent)
			.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
			.Padding(FMargin(0.f))
			[
				SNew(STextBlock)
				.Text(FText::FromName(Item->Name))
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(NameColor)
				.ToolTipText(MakeDiagnosticTooltip())
			]);

	// Group widget: editable text in Layout mode, read-only label in Instance mode.
	TSharedRef<SWidget> GroupWidget = bInstanceViewMode
		? StaticCastSharedRef<SWidget>(
			SNew(STextBlock)
			.Text(FText::FromName(Item->Group))
			.Font(FMLPTheme::FontSmall())
			.ColorAndOpacity(FMLPTheme::Muted()))
		: StaticCastSharedRef<SWidget>(
			SNew(SEditableTextBox)
			.Text(FText::FromName(Item->Group))
			.Font(FMLPTheme::FontBody())
			.OnTextCommitted(this, &SMaterialParameterRow::OnGroupTextCommitted));

	// Column widths shift in Instance mode: no Priority column, wider Name/Value.
	const float NameWidth = bInstanceViewMode ? 0.30f : 0.22f;
	const float ValueWidth = bInstanceViewMode ? 0.35f : 0.22f;
	const float GroupWidth = bInstanceViewMode ? 0.18f : 0.20f;
	const float StatusWidth = bInstanceViewMode ? 0.12f : 0.13f;

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor_Lambda(GetBgColor)
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMargin(8.f, 3.f, 6.f, 3.f))
		.OnMouseButtonDown(this, &SMaterialParameterRow::OnRowClicked)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				FMLPTheme::MakeTypePill(Item->GetTypeAbbreviation(), Item->GetTypeColor())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(NameWidth)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				NameWidget
			]
			+ SHorizontalBox::Slot()
			.FillWidth(ValueWidth)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				BuildValueWidget()
			]
			+ SHorizontalBox::Slot()
			.FillWidth(GroupWidth)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				GroupWidget
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				// Priority column only in Layout mode.
				bInstanceViewMode
					? StaticCastSharedRef<SWidget>(SNew(SBox))
					: StaticCastSharedRef<SWidget>(
						SNew(SBox)
						.WidthOverride(60.f)
						[
							SNew(SNumericEntryBox<int32>)
							.Value(TOptional<int32>(Item->SortPriority))
							.Font(FMLPTheme::FontBody())
							.OnValueCommitted(this, &SMaterialParameterRow::OnPriorityValueCommitted)
						])
			]
			+ SHorizontalBox::Slot()
			.FillWidth(StatusWidth)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
					SNew(SBorder)
					.BorderBackgroundColor(Item->GetUsageBgColor())
					.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
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
		const double Now = FSlateApplication::Get().GetCurrentTime();
		const bool bIsDoubleClick = (Now - LastClickTime) < 0.5;
		LastClickTime = Now;

		if (bIsDoubleClick)
		{
			OnDoubleClicked.ExecuteIfBound(Item, MouseEvent);
		}
		else
		{
			OnClicked.ExecuteIfBound(Item, MouseEvent);
		}
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

void SMaterialParameterRow::OnNameTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter && CommitType != ETextCommit::OnUserMovedFocus)
	{
		return;
	}

	if (Item.IsValid())
	{
		const FString NewName = NewText.ToString();
		if (!NewName.IsEmpty() && NewName != Item->Name.ToString())
		{
			OnRenamed.ExecuteIfBound(Item, NewName);
		}
	}
}

TSharedRef<SWidget> SMaterialParameterRow::BuildValueWidget()
{
	if (!Item.IsValid() || !Item->Expression.IsValid())
	{
		return SNew(STextBlock).Text(FText::GetEmpty());
	}

	UMaterialExpression* Expression = Item->Expression.Get();

	switch (Item->Type)
	{
	case EMLPParameterType::Scalar:
	{
		TWeakObjectPtr<UMaterialExpressionScalarParameter> WeakScalar = Cast<UMaterialExpressionScalarParameter>(Expression);
		auto GetScalarValue = [WeakScalar]() -> TOptional<float>
		{
			if (auto S = WeakScalar.Get()) return TOptional<float>(S->DefaultValue);
			return TOptional<float>();
		};
		return SNew(SNumericEntryBox<float>)
			.Value_Lambda(GetScalarValue)
			.Font(FMLPTheme::FontBody())
			.AllowSpin(true)
			.MinValue(TOptional<float>())
			.MaxValue(TOptional<float>())
			.MinSliderValue(TOptional<float>())
			.MaxSliderValue(TOptional<float>())
			.OnValueChanged(this, &SMaterialParameterRow::OnScalarValueDragged)
			.OnValueCommitted(this, &SMaterialParameterRow::OnScalarValueCommitted);
	}
	case EMLPParameterType::Vector:
	{
		TWeakObjectPtr<UMaterialExpressionVectorParameter> WeakVector = Cast<UMaterialExpressionVectorParameter>(Expression);
		auto GetVectorColor = [WeakVector]() -> FLinearColor
		{
			if (auto V = WeakVector.Get()) return V->DefaultValue;
			return FLinearColor::White;
		};
		return SNew(SColorBlock)
			.Color_Lambda(GetVectorColor)
			.OnMouseButtonDown_Lambda([this, WeakVector](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply
			{
				if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
				auto Vec = WeakVector.Get();
				if (!Vec) return FReply::Unhandled();
				FColorPickerArgs Args;
				Args.bUseAlpha = true;
#if ENGINE_MAJOR_VERSION >= 5
				Args.InitialColor = Vec->DefaultValue;
#else
				Args.InitialColorOverride = Vec->DefaultValue;
#endif
				Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this, WeakVector](FLinearColor NewColor)
				{
					if (auto V = WeakVector.Get())
					{
						V->DefaultValue = NewColor;
					}
					if (Item.IsValid())
					{
						OnVectorChangedDelegate.ExecuteIfBound(Item, NewColor);
					}
				});
				OpenColorPicker(Args);
				return FReply::Handled();
			});
	}
	case EMLPParameterType::Texture:
	{
		// Handle both TextureSampleParameter and TextureObjectParameter.
		UTexture* CurrentTexture = nullptr;
		if (UMaterialExpressionTextureSampleParameter* TexSample = Cast<UMaterialExpressionTextureSampleParameter>(Expression))
		{
			CurrentTexture = TexSample->Texture;
		}
		else if (UMaterialExpressionTextureObjectParameter* TexObj = Cast<UMaterialExpressionTextureObjectParameter>(Expression))
		{
			CurrentTexture = TexObj->Texture;
		}
		const FString DisplayText = CurrentTexture ? CurrentTexture->GetName() : TEXT("（无）");
		return SNew(SButton)
			.Text(FText::FromString(DisplayText))
			.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
			.ContentPadding(FMargin(2.f, 0.f))
			.HAlign(HAlign_Left)
			.OnClicked_Lambda([this]() -> FReply
			{
				if (!Item.IsValid()) return FReply::Handled();
				FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				FAssetPickerConfig Config;
#if ENGINE_MAJOR_VERSION >= 5
				Config.Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Texture2D")));
#else
				Config.Filter.ClassNames.Add(TEXT("Texture2D"));
#endif
				Config.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& AssetData) -> void
				{
					if (Item.IsValid() && AssetData.IsValid())
					{
						UObject* Asset = AssetData.GetAsset();
						if (!Asset)
						{
							// Fallback: try loading via package path (4.26/5.x compatible).
							const FString Path = AssetData.PackageName.ToString() / AssetData.AssetName.ToString();
							Asset = LoadObject<UObject>(nullptr, *Path);
						}
						if (Asset)
						{
							OnTextureChangedDelegate.ExecuteIfBound(Item, Asset);
						}
					}
					FSlateApplication::Get().DismissAllMenus();
				});
				Config.bAllowNullSelection = false;
				Config.InitialAssetViewType = EAssetViewType::List;

				TSharedRef<SWidget> Picker = CB.Get().CreateAssetPicker(Config);
				FSlateApplication::Get().PushMenu(
					AsShared(),
					FWidgetPath(),
					Picker,
					FSlateApplication::Get().GetCursorPos(),
					FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
				return FReply::Handled();
			});
	}
	case EMLPParameterType::StaticBool:
	case EMLPParameterType::StaticSwitch:
	{
		bool bChecked = false;
		if (UMaterialExpressionStaticBoolParameter* Bool = Cast<UMaterialExpressionStaticBoolParameter>(Expression))
		{
			bChecked = Bool->DefaultValue;
		}
		else if (UMaterialExpressionStaticSwitchParameter* Sw = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
		{
			bChecked = Sw->DefaultValue;
		}
		return SNew(SCheckBox)
			.IsChecked(bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
			{
				bool bNew = (State == ECheckBoxState::Checked);
				if (Item.IsValid() && Item->Expression.IsValid())
				{
					if (UMaterialExpressionStaticBoolParameter* Bool = Cast<UMaterialExpressionStaticBoolParameter>(Item->Expression.Get()))
					{
						Bool->DefaultValue = bNew;
					}
					else if (UMaterialExpressionStaticSwitchParameter* Sw = Cast<UMaterialExpressionStaticSwitchParameter>(Item->Expression.Get()))
					{
						Sw->DefaultValue = bNew;
					}
					OnBoolChangedDelegate.ExecuteIfBound(Item, bNew);
				}
			});
	}
	default:
		return SNew(STextBlock)
			.Text(FText::FromString(Item->ValueString))
			.Font(FMLPTheme::FontSmall())
			.ColorAndOpacity(FMLPTheme::Muted());
	}
}

void SMaterialParameterRow::OnScalarValueCommitted(float NewValue, ETextCommit::Type CommitType)
{
	if (Item.IsValid())
	{
		// Value already written by OnScalarValueDragged during drag.
		// Notify panel to trigger PostEditChange + MarkPackageDirty (undo support).
		OnScalarChangedDelegate.ExecuteIfBound(Item, NewValue);
	}
}

void SMaterialParameterRow::OnScalarValueDragged(float NewValue)
{
	if (Item.IsValid() && Item->Expression.IsValid())
	{
		// Write value directly to expression.
		if (UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Item->Expression.Get()))
		{
			Scalar->DefaultValue = NewValue;
		}
		// Notify panel — panel will throttle PostEditChange to avoid excessive recompiles.
		OnScalarChangedDelegate.ExecuteIfBound(Item, NewValue);
	}
}

FText SMaterialParameterRow::MakeDiagnosticTooltip() const
{
	if (!Item.IsValid())
	{
		return FText::GetEmpty();
	}

	FString Tip = FString::Printf(TEXT("Name: %s"), *Item->Name.ToString());
	Tip += FString::Printf(TEXT("\nType: %s"), *Item->GetDisplayTypeName().ToString());
	Tip += FString::Printf(TEXT("\nGroup: %s"), *Item->Group.ToString());
	Tip += FString::Printf(TEXT("\nSort Priority: %d"), Item->SortPriority);
	Tip += FString::Printf(TEXT("\nUsage: %s"), *Item->GetUsageLabel().ToString());

	if (Item->Usage == EMLPParameterUsage::Unused)
	{
		Tip += TEXT("\n\nWarning: this parameter is not connected to any output.");
	}
	if (Item->Usage == EMLPParameterUsage::HalfUsed)
	{
		Tip += TEXT("\n\nNote: half-used — connected via MaterialAttributes but not a direct output.");
	}
	if (Item->bHasDuplicateName)
	{
		Tip += TEXT("\n\nConflict: another parameter shares this name.");
	}
	if (!Item->ValueString.IsEmpty())
	{
		Tip += FString::Printf(TEXT("\n\nValue: %s"), *Item->ValueString);
	}

	return FText::FromString(Tip);
}

FReply SMaterialParameterRow::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && Item.IsValid())
	{
		// Ask the panel to gather the dragged payload (all selected rows travel together).
		if (OnDragDetectedDelegate.IsBound())
		{
			return OnDragDetectedDelegate.Execute(Item, MyGeometry, MouseEvent);
		}
	}
	return FReply::Unhandled();
}

FReply SMaterialParameterRow::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FMLPParameterDragDrop> DragOp = DragDropEvent.GetOperationAs<FMLPParameterDragDrop>();
	if (!DragOp.IsValid() || !DragOp->IsValid() || !Item.IsValid())
	{
		return FReply::Unhandled();
	}

	// Don't drop onto self or onto a parameter that's already in the dragged set.
	bool bDraggingSelf = false;
	for (const TSharedPtr<FMLPParameterInfo>& Dragged : DragOp->Parameters)
	{
		if (Dragged.IsValid() && Dragged == Item)
		{
			bDraggingSelf = true;
			break;
		}
	}
	if (bDraggingSelf)
	{
		return FReply::Unhandled();
	}

	OnDroppedDelegate.ExecuteIfBound(DragOp->Parameters, Item);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
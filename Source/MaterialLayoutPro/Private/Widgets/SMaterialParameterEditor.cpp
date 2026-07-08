#include "Widgets/SMaterialParameterEditor.h"
#include "MaterialLayoutProTheme.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialInstance.h"
#include "Engine/Texture.h"
#include "ScopedTransaction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

#if ENGINE_MAJOR_VERSION >= 5
#define MLP_STYLE FAppStyle
#include "Styling/AppStyle.h"
#else
#define MLP_STYLE FEditorStyle
#endif

#define LOCTEXT_NAMESPACE "SMaterialParameterEditor"

void SMaterialParameterEditor::Construct(const FArguments& InArgs)
{
	TargetMaterial = InArgs._TargetMaterial;
	TargetInstance = InArgs._TargetInstance;
	OnApplied = InArgs._OnApplied;

	InitWorkParameters(InArgs._Parameters);
	InitDefaultTabs();

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "参数编辑器"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(720.f, 600.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FMLPTheme::Background())
			.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
			.Padding(FMargin(8.f, 6.f, 8.f, 6.f))
			[
				SNew(SVerticalBox)
				// Target info
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(TargetInstance.IsValid()
							? FText::Format(LOCTEXT("TargetFmtInst", "正在编辑：{0}（实例）"), FText::FromString(TargetInstance->GetName()))
							: (TargetMaterial.IsValid()
								? FText::Format(LOCTEXT("TargetFmtMat", "正在编辑：{0}"), FText::FromString(TargetMaterial->GetName()))
								: LOCTEXT("NoTarget", "无目标")))
						.Font(FMLPTheme::FontHeading())
						.ColorAndOpacity(FMLPTheme::Foreground())
					]
				]
				// Tab bar
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
				[
					SAssignNew(TabBarContainer, SVerticalBox)
				]
				// Content area
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
				[
					SNew(SBorder)
					.BorderBackgroundColor(FMLPTheme::Surface())
					.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
					.Padding(1.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(ContentContainer, SVerticalBox)
						]
					]
				]
				// Bottom buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SBox)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMLPTheme::PadH())
					[
					SNew(SButton)
					.Text(LOCTEXT("ApplyToMaterial", "应用到材质"))
					.ToolTipText(LOCTEXT("ApplyToMaterialTooltip", "将分组、排序优先级和参数值写回材质"))
					.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
					.ButtonColorAndOpacity(FMLPTheme::ButtonPrimary())
					.ForegroundColor(FMLPTheme::ButtonTextOnColor())
					.ContentPadding(FMLPTheme::PadBtn())
					.OnClicked(this, &SMaterialParameterEditor::OnApplyToMaterialClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMLPTheme::PadH())
					[
					SNew(SButton)
					.Text(LOCTEXT("ApplyToInstance", "应用到实例"))
					.ToolTipText(LOCTEXT("ApplyToInstanceTooltip", "将参数值覆盖写回材质实例"))
					.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
					.ButtonColorAndOpacity(FMLPTheme::ButtonPrimary())
					.ForegroundColor(FMLPTheme::ButtonTextOnColor())
					.ContentPadding(FMLPTheme::PadBtn())
					.IsEnabled(TargetInstance.IsValid())
					.OnClicked(this, &SMaterialParameterEditor::OnApplyToInstanceClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("Cancel", "Cancel"))
						.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
						.ContentPadding(FMLPTheme::PadBtn())
						.OnClicked(this, &SMaterialParameterEditor::OnCancelClicked)
					]
				]
			]
		]);

	RebuildUI();
}

void SMaterialParameterEditor::InitWorkParameters(const TArray<TSharedPtr<FMLPParameterInfo>>& Source)
{
	WorkParameters.Reset();
	for (const TSharedPtr<FMLPParameterInfo>& Src : Source)
	{
		if (!Src.IsValid())
		{
			continue;
		}
		// Deep copy — share Expression pointer but own our own Group/SortPriority/Value copies.
		TSharedPtr<FMLPParameterInfo> Copy = MakeShared<FMLPParameterInfo>(*Src);
		WorkParameters.Add(Copy);
	}
}

void SMaterialParameterEditor::InitDefaultTabs()
{
	VirtualTabs.Reset();
	TMap<FName, TSharedPtr<FVirtualTab>> TabMap;

	for (const TSharedPtr<FMLPParameterInfo>& Param : WorkParameters)
	{
		if (!Param.IsValid())
		{
			continue;
		}
		FName GroupName = Param->Group.IsNone() ? FName(TEXT("(None)")) : Param->Group;
		TSharedPtr<FVirtualTab>* Found = TabMap.Find(GroupName);
		if (!Found)
		{
			TSharedPtr<FVirtualTab> NewTab = MakeShared<FVirtualTab>();
			NewTab->Name = GroupName;
			TabMap.Add(GroupName, NewTab);
			VirtualTabs.Add(NewTab);
			Found = &TabMap.FindChecked(GroupName);
		}
		(*Found)->Parameters.Add(Param);
	}

	VirtualTabs.Sort([](const TSharedPtr<FVirtualTab>& A, const TSharedPtr<FVirtualTab>& B)
	{
		return A->Name.ToString() < B->Name.ToString();
	});

	if (VirtualTabs.Num() > 0)
	{
		ActiveTabIndex = 0;
	}
}

void SMaterialParameterEditor::RebuildUI()
{
	if (TabBarContainer.IsValid())
	{
		TabBarContainer->ClearChildren();
		TabBarContainer->AddSlot()
			.AutoHeight()
			[
				BuildTabBar()
			];
	}
	if (ContentContainer.IsValid())
	{
		ContentContainer->ClearChildren();
		if (VirtualTabs.IsValidIndex(ActiveTabIndex))
		{
			ContentContainer->AddSlot()
				.FillHeight(1.0f)
				[
					BuildTabContent()
				];
		}
	}
}

TSharedRef<SWidget> SMaterialParameterEditor::BuildTabBar()
{
	TSharedRef<SHorizontalBox> HBox = SNew(SHorizontalBox);

	for (int32 i = 0; i < VirtualTabs.Num(); ++i)
	{
		const TSharedPtr<FVirtualTab>& Tab = VirtualTabs[i];
		if (!Tab.IsValid())
		{
			continue;
		}
		const bool bIsActive = (i == ActiveTabIndex);

		HBox->AddSlot()
		.AutoWidth()
		.Padding(FMargin(0.f, 0.f, 2.f, 0.f))
		[
			SNew(SButton)
			.Text(FText::Format(LOCTEXT("TabLabel", "{0} ({1})"), FText::FromName(Tab->Name), FText::AsNumber(Tab->Parameters.Num())))
			.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
			.ButtonColorAndOpacity(bIsActive ? FMLPTheme::TabActive() : FMLPTheme::TabInactive())
			.ForegroundColor(bIsActive ? FMLPTheme::ButtonTextOnColor() : FMLPTheme::ButtonTextNormal())
			.ContentPadding(FMargin(10.f, 3.f))
			.OnClicked(this, &SMaterialParameterEditor::SelectTab, i)
		];
	}

	// "+" button to add a new tab.
	HBox->AddSlot()
	.AutoWidth()
	.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
	[
		SNew(SButton)
		.Text(FText::FromString(TEXT("+")))
		.ToolTipText(LOCTEXT("AddTabTooltip", "创建新的虚拟标签页"))
		.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
		.ContentPadding(FMargin(8.f, 3.f))
		.OnClicked(this, &SMaterialParameterEditor::OnAddTabClicked)
	];

	return HBox;
}

FReply SMaterialParameterEditor::SelectTab(int32 TabIndex)
{
	if (VirtualTabs.IsValidIndex(TabIndex))
	{
		ActiveTabIndex = TabIndex;
		RebuildUI();
	}
	return FReply::Handled();
}

FReply SMaterialParameterEditor::OnAddTabClicked()
{
	// Simple inline: create a "New Tab N" name. User can rename later via material Group.
	int32 NewNum = VirtualTabs.Num() + 1;
	FName NewName(*FString::Printf(TEXT("New Tab %d"), NewNum));
	TSharedPtr<FVirtualTab> NewTab = MakeShared<FVirtualTab>();
	NewTab->Name = NewName;
	VirtualTabs.Add(NewTab);
	ActiveTabIndex = VirtualTabs.Num() - 1;
	RebuildUI();
	return FReply::Handled();
}

TSharedRef<SWidget> SMaterialParameterEditor::BuildTabContent()
{
	if (!VirtualTabs.IsValidIndex(ActiveTabIndex))
	{
		return SNew(STextBlock)
			.Text(LOCTEXT("NoTab", "未选择标签页"))
			.ColorAndOpacity(FMLPTheme::Muted());
	}

	const TSharedPtr<FVirtualTab>& Tab = VirtualTabs[ActiveTabIndex];
	TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox);

	// Tab header
	VBox->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(FMLPTheme::SurfaceHover().R, FMLPTheme::SurfaceHover().G, FMLPTheme::SurfaceHover().B, 0.5f))
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMargin(10.f, 5.f, 8.f, 5.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("TabHeader", "{0}"), FText::FromName(Tab->Name)))
				.Font(FMLPTheme::FontTitle())
				.ColorAndOpacity(FMLPTheme::Foreground())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("TabParamCount", "{0} 个参数"), FText::AsNumber(Tab->Parameters.Num())))
				.Font(FMLPTheme::FontSmall())
				.ColorAndOpacity(FMLPTheme::Muted())
			]
		]
	];

	if (Tab->Parameters.Num() == 0)
	{
		VBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 30.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EmptyTab", "This tab has no parameters.\nMove parameters here using the dropdown on each row."))
			.Font(FMLPTheme::FontBody())
			.ColorAndOpacity(FMLPTheme::Muted())
			.Justification(ETextJustify::Center)
		];
		return VBox;
	}

	for (const TSharedPtr<FMLPParameterInfo>& Param : Tab->Parameters)
	{
		VBox->AddSlot()
		.AutoHeight()
		[
			BuildParamRow(Param)
		];
	}

	return VBox;
}

TSharedRef<SWidget> SMaterialParameterEditor::BuildParamRow(TSharedPtr<FMLPParameterInfo> Param)
{
	if (!Param.IsValid())
	{
		return SNew(STextBlock).Text(LOCTEXT("InvalidRow", "无效"));
	}

	// Build the value editor based on type.
	TSharedRef<SWidget> ValueWidget =
		SNew(STextBlock)
		.Text(FText::GetEmpty())
		.Font(FMLPTheme::FontSmall())
		.ColorAndOpacity(FMLPTheme::Muted());
	if (Param->Expression.IsValid())
	{
		UMaterialExpression* Expr = Param->Expression.Get();

		if (UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expr))
		{
			TWeakObjectPtr<UMaterialExpressionScalarParameter> WeakScalar = Scalar;
			auto GetVal = [WeakScalar]() -> TOptional<float>
			{
				if (auto S = WeakScalar.Get()) return TOptional<float>(S->DefaultValue);
				return TOptional<float>();
			};
			ValueWidget = SNew(SBox)
				.WidthOverride(180.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda(GetVal)
					.AllowSpin(true)
					.MinValue(TOptional<float>())
					.MaxValue(TOptional<float>())
					.MinSliderValue(TOptional<float>())
					.MaxSliderValue(TOptional<float>())
					.OnValueChanged_Lambda([this, WeakScalar, Param](float NewVal)
					{
						if (auto S = WeakScalar.Get()) S->DefaultValue = NewVal;
					})
					.OnValueCommitted_Lambda([this, WeakScalar, Param](float NewVal, ETextCommit::Type)
					{
						if (auto S = WeakScalar.Get()) S->DefaultValue = NewVal;
						if (TargetMaterial.IsValid()) { TargetMaterial->PostEditChange(); TargetMaterial->MarkPackageDirty(); }
					})
				];
		}
		else if (UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expr))
		{
			TWeakObjectPtr<UMaterialExpressionVectorParameter> WeakVector = Vector;
			auto GetColor = [WeakVector]() -> FLinearColor
			{
				if (auto V = WeakVector.Get()) return V->DefaultValue;
				return FLinearColor::White;
			};
			ValueWidget = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(24.f)
					.HeightOverride(18.f)
					[
						SNew(SColorBlock)
						.Color_Lambda(GetColor)
						.OnMouseButtonDown_Lambda([this, WeakVector](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply
						{
							if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
							auto V = WeakVector.Get();
							if (!V) return FReply::Unhandled();
							FColorPickerArgs Args;
							Args.bUseAlpha = true;
#if ENGINE_MAJOR_VERSION >= 5
							Args.InitialColor = V->DefaultValue;
#else
							Args.InitialColorOverride = V->DefaultValue;
#endif
							Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this, WeakVector](FLinearColor NewColor)
							{
								if (auto V = WeakVector.Get()) V->DefaultValue = NewColor;
								if (TargetMaterial.IsValid()) { TargetMaterial->PostEditChange(); TargetMaterial->MarkPackageDirty(); }
							});
							OpenColorPicker(Args);
							return FReply::Handled();
						})
					]
				];
		}
		else if (UMaterialExpressionTextureSampleParameter* Tex = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
		{
			TWeakObjectPtr<UMaterialExpressionTextureSampleParameter> WeakTex = Tex;
			ValueWidget = SNew(SButton)
				.Text_Lambda([WeakTex]() -> FText
				{
					if (auto T = WeakTex.Get()) return T->Texture ? FText::FromString(T->Texture->GetName()) : FText::FromString(TEXT("（无）"));
					return FText::FromString(TEXT("（无）"));
				})
				.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
				.ContentPadding(FMargin(4.f, 0.f))
				.HAlign(HAlign_Left)
				.OnClicked_Lambda([this, Param]() -> FReply
				{
					FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
					FAssetPickerConfig Config;
#if ENGINE_MAJOR_VERSION >= 5
					Config.Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Texture2D")));
#else
					Config.Filter.ClassNames.Add(TEXT("Texture2D"));
#endif
					Config.OnAssetSelected = FOnAssetSelected::CreateLambda([this, Param](const FAssetData& AssetData)
					{
						if (AssetData.IsValid())
						{
							OnTextureChanged(Param, AssetData.GetAsset());
							if (TargetMaterial.IsValid()) TargetMaterial->PostEditChange();
						}
						FSlateApplication::Get().DismissAllMenus();
					});
					Config.bAllowNullSelection = false;
					TSharedRef<SWidget> Picker = CB.Get().CreateAssetPicker(Config);
					FSlateApplication::Get().PushMenu(AsShared(), FWidgetPath(), Picker,
						FSlateApplication::Get().GetCursorPos(),
						FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
					return FReply::Handled();
				});
		}
		else if (Cast<UMaterialExpressionStaticBoolParameter>(Expr) || Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
		{
			bool bChecked = false;
			if (UMaterialExpressionStaticBoolParameter* Bool = Cast<UMaterialExpressionStaticBoolParameter>(Expr)) bChecked = Bool->DefaultValue;
			else if (UMaterialExpressionStaticSwitchParameter* Sw = Cast<UMaterialExpressionStaticSwitchParameter>(Expr)) bChecked = Sw->DefaultValue;
			ValueWidget = SNew(SCheckBox)
				.IsChecked(bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([this, Param](ECheckBoxState State)
				{
					OnBoolChanged(Param, State == ECheckBoxState::Checked);
					if (TargetMaterial.IsValid()) TargetMaterial->PostEditChange();
				});
		}
	}

	// "Move to tab" combo — builds a menu listing all virtual tabs.
	TSharedRef<SVerticalBox> MoveMenuContent = SNew(SVerticalBox);
	for (int32 i = 0; i < VirtualTabs.Num(); ++i)
	{
		const int32 TargetIdx = i;
		MoveMenuContent->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
			.Text(FText::Format(LOCTEXT("MoveToTabItem", "→ {0}"), FText::FromName(VirtualTabs[i]->Name)))
			.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
			.ContentPadding(FMargin(8.f, 2.f))
			.HAlign(HAlign_Left)
			.OnClicked_Lambda([this, Param, TargetIdx]() -> FReply
			{
				MoveParameterToTab(Param, TargetIdx);
				return FReply::Handled();
			})
		];
	}
	TSharedRef<SWidget> MoveCombo = SNew(SComboButton)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MoveToTabBtn", "移动"))
			.Font(FMLPTheme::FontSmall())
			.ColorAndOpacity(FMLPTheme::Muted())
		]
		.MenuContent()
		[
			MoveMenuContent
		];

	return SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(FMLPTheme::Surface().R, FMLPTheme::Surface().G, FMLPTheme::Surface().B, 0.6f))
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMargin(8.f, 3.f, 6.f, 3.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				FMLPTheme::MakeTypePill(Param->GetTypeAbbreviation(), Param->GetTypeColor())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.25f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromName(Param->Name))
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Foreground())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.35f)
			.VAlign(VAlign_Center)
			.Padding(FMargin(6.f, 0.f))
			[
				ValueWidget
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.15f)
			.VAlign(VAlign_Center)
			.Padding(FMargin(6.f, 0.f))
			[
				SNew(SEditableTextBox)
				.Text(FText::FromName(Param->Group))
				.Font(FMLPTheme::FontBody())
				.OnTextCommitted_Lambda([this, Param](const FText& NewText, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
					{
						Param->Group = FName(*NewText.ToString());
						// Don't RebuildUI — it kills focus when Tab-moving to next field.
					}
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(6.f, 0.f))
			[
				SNew(SBox).WidthOverride(50.f)
				[
					SNew(SNumericEntryBox<int32>)
					.Value(TOptional<int32>(Param->SortPriority))
					.OnValueCommitted_Lambda([this, Param](int32 NewVal, ETextCommit::Type) { Param->SortPriority = NewVal; })
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				MoveCombo
			]
		];
}

void SMaterialParameterEditor::MoveParameterToTab(TSharedPtr<FMLPParameterInfo> Param, int32 TargetTabIndex)
{
	if (!Param.IsValid() || !VirtualTabs.IsValidIndex(TargetTabIndex))
	{
		return;
	}
	// Remove from all tabs first.
	for (TSharedPtr<FVirtualTab>& Tab : VirtualTabs)
	{
		Tab->Parameters.RemoveAll([&](const TSharedPtr<FMLPParameterInfo>& P) { return P == Param; });
	}
	VirtualTabs[TargetTabIndex]->Parameters.Add(Param);
	RebuildUI();
}

void SMaterialParameterEditor::OnScalarChanged(TSharedPtr<FMLPParameterInfo> Param, float NewValue)
{
	if (Param.IsValid() && Param->Expression.IsValid())
	{
		if (UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Param->Expression.Get()))
		{
			Scalar->DefaultValue = NewValue;
		}
		Param->ValueString = FString::SanitizeFloat(NewValue);
	}
}

void SMaterialParameterEditor::OnVectorChanged(TSharedPtr<FMLPParameterInfo> Param, const FLinearColor& NewColor)
{
	if (Param.IsValid() && Param->Expression.IsValid())
	{
		if (UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Param->Expression.Get()))
		{
			Vector->DefaultValue = NewColor;
		}
		Param->ValueString = NewColor.ToString();
	}
}

void SMaterialParameterEditor::OnTextureChanged(TSharedPtr<FMLPParameterInfo> Param, UObject* NewTexture)
{
	if (Param.IsValid() && Param->Expression.IsValid() && NewTexture)
	{
		if (UMaterialExpressionTextureSampleParameter* Tex = Cast<UMaterialExpressionTextureSampleParameter>(Param->Expression.Get()))
		{
			Tex->Texture = Cast<UTexture>(NewTexture);
		}
		Param->ValueString = NewTexture->GetName();
		RebuildUI();
	}
}

void SMaterialParameterEditor::OnBoolChanged(TSharedPtr<FMLPParameterInfo> Param, bool bNewValue)
{
	if (Param.IsValid() && Param->Expression.IsValid())
	{
		if (UMaterialExpressionStaticBoolParameter* Bool = Cast<UMaterialExpressionStaticBoolParameter>(Param->Expression.Get()))
		{
			Bool->DefaultValue = bNewValue;
		}
		else if (UMaterialExpressionStaticSwitchParameter* Sw = Cast<UMaterialExpressionStaticSwitchParameter>(Param->Expression.Get()))
		{
			Sw->DefaultValue = bNewValue;
		}
		Param->ValueString = bNewValue ? TEXT("true") : TEXT("false");
	}
}

FReply SMaterialParameterEditor::OnApplyToMaterialClicked()
{
	if (!TargetMaterial.IsValid())
	{
		return FReply::Handled();
	}

	UMaterial* Material = TargetMaterial.Get();
	const FScopedTransaction Transaction(LOCTEXT("ApplyParameterEditor", "将参数编辑器应用到材质"));
	Material->Modify();

	// Values were already written directly to the expressions by the inline editors.
	// Here we only need to sync Group/SortPriority and trigger PostEditChange.
	for (const TSharedPtr<FMLPParameterInfo>& Param : WorkParameters)
	{
		if (!Param.IsValid() || !Param->Expression.IsValid())
		{
			continue;
		}
		if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Param->Expression.Get()))
		{
			ParamExpr->Modify();
			ParamExpr->Group = Param->Group;
			ParamExpr->SortPriority = Param->SortPriority;
		}
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();
	OnApplied.ExecuteIfBound();
	RequestDestroyWindow();
	return FReply::Handled();
}

FReply SMaterialParameterEditor::OnApplyToInstanceClicked()
{
	if (!TargetInstance.IsValid())
	{
		return FReply::Handled();
	}

	UMaterialInstance* Instance = TargetInstance.Get();
	const FScopedTransaction Transaction(LOCTEXT("ApplyParameterEditorToInstance", "将参数编辑器应用到实例"));
	Instance->Modify();

	// Read current values directly from the expressions (already updated by inline editors).
	for (const TSharedPtr<FMLPParameterInfo>& Param : WorkParameters)
	{
		if (!Param.IsValid() || !Param->Expression.IsValid())
		{
			continue;
		}
		const FName& ParamName = Param->Name;
		UMaterialExpression* Expr = Param->Expression.Get();

		switch (Param->Type)
		{
		case EMLPParameterType::Scalar:
		{
			float Value = 0.f;
			if (UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expr)) Value = Scalar->DefaultValue;
			bool bFound = false;
			for (FScalarParameterValue& SV : Instance->ScalarParameterValues)
			{
				if (SV.ParameterInfo.Name == ParamName) { SV.ParameterValue = Value; bFound = true; break; }
			}
			if (!bFound)
			{
				FScalarParameterValue NewSV;
				NewSV.ParameterInfo.Name = ParamName;
				NewSV.ParameterValue = Value;
				Instance->ScalarParameterValues.Add(NewSV);
			}
			break;
		}
		case EMLPParameterType::Vector:
		{
			FLinearColor Value = FLinearColor::White;
			if (UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expr)) Value = Vector->DefaultValue;
			bool bFound = false;
			for (FVectorParameterValue& VV : Instance->VectorParameterValues)
			{
				if (VV.ParameterInfo.Name == ParamName) { VV.ParameterValue = Value; bFound = true; break; }
			}
			if (!bFound)
			{
				FVectorParameterValue NewVV;
				NewVV.ParameterInfo.Name = ParamName;
				NewVV.ParameterValue = Value;
				Instance->VectorParameterValues.Add(NewVV);
			}
			break;
		}
		case EMLPParameterType::Texture:
		{
			UTexture* Value = nullptr;
			if (UMaterialExpressionTextureSampleParameter* Tex = Cast<UMaterialExpressionTextureSampleParameter>(Expr)) Value = Tex->Texture;
			if (!Value) break;
			bool bFound = false;
			for (FTextureParameterValue& TV : Instance->TextureParameterValues)
			{
				if (TV.ParameterInfo.Name == ParamName) { TV.ParameterValue = Value; bFound = true; break; }
			}
			if (!bFound)
			{
				FTextureParameterValue NewTV;
				NewTV.ParameterInfo.Name = ParamName;
				NewTV.ParameterValue = Value;
				Instance->TextureParameterValues.Add(NewTV);
			}
			break;
		}
		default:
			break;
		}
	}

	Instance->PostEditChange();
	Instance->MarkPackageDirty();
	OnApplied.ExecuteIfBound();
	RequestDestroyWindow();
	return FReply::Handled();
}

FReply SMaterialParameterEditor::OnCancelClicked()
{
	RequestDestroyWindow();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

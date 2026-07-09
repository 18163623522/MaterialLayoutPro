#include "Widgets/SMaterialParameterEditor.h"
#include "MaterialLayoutProTheme.h"
#include "Widgets/SMaterialParameterRow.h"
#include "Model/MaterialLayoutViewModel.h"

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
#include "Engine/Texture.h"
#include "ScopedTransaction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"

#if ENGINE_MAJOR_VERSION >= 5
#define MLP_STYLE FAppStyle
#include "Styling/AppStyle.h"
#else
#define MLP_STYLE FEditorStyle
#endif

#define LOCTEXT_NAMESPACE "SMaterialParameterEditor"

void SMaterialParameterEditor::Construct(const FArguments& InArgs)
{
	Session = InArgs._Session;
	TargetInstance = InArgs._TargetInstance;
	OnApplied = InArgs._OnApplied;

	InitDefaultTabs();

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "参数编辑器"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(760.f, 620.f))
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
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() -> FText
						{
							if (!Session.IsValid() || !Session->TargetMaterial.IsValid())
								return LOCTEXT("NoTarget", "无目标");
							FText Name = FText::FromString(Session->TargetMaterial->GetName());
							return TargetInstance.IsValid()
								? FText::Format(LOCTEXT("TargetFmtInst", "正在编辑：{0}（实例）"), Name)
								: FText::Format(LOCTEXT("TargetFmtMat", "正在编辑：{0}"), Name);
						})
						.Font(FMLPTheme::FontHeading())
						.ColorAndOpacity(FMLPTheme::Foreground())
					]
				]
				// Tab bar
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
				[
					SAssignNew(TabBarContainer, SVerticalBox)
				]
				// Content area
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(FMargin(0.f, 0.f, 0.f, 6.f))
				[
					SNew(SBorder)
					.BorderBackgroundColor(FMLPTheme::Surface())
					.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
					.Padding(1.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot() [ SAssignNew(ContentContainer, SVerticalBox) ]
					]
				]
				// Bottom buttons
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f) [ SNew(SBox) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(FMLPTheme::PadH())
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
					+ SHorizontalBox::Slot().AutoWidth().Padding(FMLPTheme::PadH())
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
					+ SHorizontalBox::Slot().AutoWidth()
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

void SMaterialParameterEditor::InitDefaultTabs()
{
	VirtualTabs.Reset();
	if (!Session.IsValid()) return;

	// One virtual tab per VM group (initial mapping mirrors the material's Group).
	TMap<FName, TSharedPtr<FVirtualTab>> TabMap;
	for (const TSharedPtr<FMLPGroupVM>& Group : Session->Groups)
	{
		if (!Group.IsValid()) continue;
		FName GroupName = Group->Name.IsNone() ? FName(TEXT("(None)")) : Group->Name;
		TSharedPtr<FVirtualTab> Tab = MakeShared<FVirtualTab>();
		Tab->Name = GroupName;
		Tab->Parameters = Group->Parameters; // share the VM pointers
		TabMap.Add(GroupName, Tab);
		VirtualTabs.Add(Tab);
	}

	VirtualTabs.Sort([](const TSharedPtr<FVirtualTab>& A, const TSharedPtr<FVirtualTab>& B)
	{
		return A->Name.ToString() < B->Name.ToString();
	});

	ActiveTabIndex = VirtualTabs.Num() > 0 ? 0 : INDEX_NONE;
}

void SMaterialParameterEditor::RebuildUI()
{
	if (TabBarContainer.IsValid())
	{
		TabBarContainer->ClearChildren();
		TabBarContainer->AddSlot().AutoHeight() [ BuildTabBar() ];
	}
	if (ContentContainer.IsValid())
	{
		ContentContainer->ClearChildren();
		if (VirtualTabs.IsValidIndex(ActiveTabIndex))
		{
			ContentContainer->AddSlot().FillHeight(1.0f) [ BuildTabContent() ];
		}
	}
}

TSharedRef<SWidget> SMaterialParameterEditor::BuildTabBar()
{
	TSharedRef<SHorizontalBox> HBox = SNew(SHorizontalBox);

	for (int32 i = 0; i < VirtualTabs.Num(); ++i)
	{
		const TSharedPtr<FVirtualTab>& Tab = VirtualTabs[i];
		if (!Tab.IsValid()) continue;
		const bool bIsActive = (i == ActiveTabIndex);

		HBox->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 2.f, 0.f))
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
	HBox->AddSlot().AutoWidth().Padding(FMargin(4.f, 0.f, 0.f, 0.f))
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
	int32 NewNum = VirtualTabs.Num() + 1;
	TSharedPtr<FVirtualTab> NewTab = MakeShared<FVirtualTab>();
	NewTab->Name = FName(*FString::Printf(TEXT("New Tab %d"), NewNum));
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
	VBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(FMLPTheme::SurfaceHover().R, FMLPTheme::SurfaceHover().G, FMLPTheme::SurfaceHover().B, 0.5f))
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMargin(10.f, 5.f, 8.f, 5.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromName(Tab->Name))
				.Font(FMLPTheme::FontTitle())
				.ColorAndOpacity(FMLPTheme::Foreground())
			]
			+ SHorizontalBox::Slot().AutoWidth()
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
		VBox->AddSlot().AutoHeight().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.f, 30.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EmptyTab", "此标签页无参数。\n用每行的「移动」按钮把参数移到这里。"))
			.Font(FMLPTheme::FontBody())
			.ColorAndOpacity(FMLPTheme::Muted())
			.Justification(ETextJustify::Center)
		];
		return VBox;
	}

	for (const TSharedPtr<FMLPParamVM>& Param : Tab->Parameters)
	{
		VBox->AddSlot().AutoHeight() [ BuildParamRow(Param) ];
	}

	return VBox;
}

TSharedRef<SWidget> SMaterialParameterEditor::BuildParamRow(TSharedPtr<FMLPParamVM> Param)
{
	if (!Param.IsValid())
	{
		return SNew(STextBlock).Text(LOCTEXT("InvalidRow", "无效"));
	}

	// "Move to tab" combo — builds a menu listing all virtual tabs.
	TSharedRef<SVerticalBox> MoveMenuContent = SNew(SVerticalBox);
	for (int32 i = 0; i < VirtualTabs.Num(); ++i)
	{
		const int32 TargetIdx = i;
		MoveMenuContent->AddSlot().AutoHeight()
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
		.MenuContent() [ MoveMenuContent ];

	// The value editor reuses SMaterialParameterRow (bound to the VM, detail mode on
	// so group + priority editors are inline). Wrapping it in a border for consistent spacing.
	return SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(FMLPTheme::Surface().R, FMLPTheme::Surface().G, FMLPTheme::Surface().B, 0.6f))
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMargin(0.f, 1.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(SMaterialParameterRow)
				.ParamVM(Param)
				.Session(Session)
				.bDetailMode(true)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f, 0.f))
			[
				MoveCombo
			]
		];
}

void SMaterialParameterEditor::MoveParameterToTab(TSharedPtr<FMLPParamVM> Param, int32 TargetTabIndex)
{
	if (!Param.IsValid() || !VirtualTabs.IsValidIndex(TargetTabIndex)) return;
	for (TSharedPtr<FVirtualTab>& Tab : VirtualTabs)
	{
		Tab->Parameters.RemoveAll([&](const TSharedPtr<FMLPParamVM>& P) { return P == Param; });
	}
	VirtualTabs[TargetTabIndex]->Parameters.Add(Param);
	RebuildUI();
}

FReply SMaterialParameterEditor::OnApplyToMaterialClicked()
{
	if (Session.IsValid())
	{
		// Sync each VM's Group back from its virtual tab membership, then flush.
		// (Virtual tabs are a UI grouping; the material Group field follows the tab Name.)
		for (const TSharedPtr<FVirtualTab>& Tab : VirtualTabs)
		{
			if (!Tab.IsValid()) continue;
			for (const TSharedPtr<FMLPParamVM>& Param : Tab->Parameters)
			{
				if (Param.IsValid() && Param->Group != Tab->Name)
				{
					Param->Group = Tab->Name;
					Param->bDirty = true;
				}
			}
		}
		Session->PushDirty();
		OnApplied.ExecuteIfBound();
	}
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

	// Read values from the VM snapshots and write them as instance overrides.
	if (Session.IsValid())
	{
		for (const TSharedPtr<FMLPGroupVM>& Group : Session->Groups)
		{
			if (!Group.IsValid()) continue;
			for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
			{
				if (!Param.IsValid()) continue;
				const FName& ParamName = Param->Name;

				switch (Param->Type)
				{
				case EMLPParameterType::Scalar:
				{
					bool bFound = false;
					for (FScalarParameterValue& SV : Instance->ScalarParameterValues)
					{
						if (SV.ParameterInfo.Name == ParamName) { SV.ParameterValue = Param->ScalarValue; bFound = true; break; }
					}
					if (!bFound)
					{
						FScalarParameterValue NewSV;
						NewSV.ParameterInfo.Name = ParamName;
						NewSV.ParameterValue = Param->ScalarValue;
						Instance->ScalarParameterValues.Add(NewSV);
					}
					break;
				}
				case EMLPParameterType::Vector:
				{
					bool bFound = false;
					for (FVectorParameterValue& VV : Instance->VectorParameterValues)
					{
						if (VV.ParameterInfo.Name == ParamName) { VV.ParameterValue = Param->VectorValue; bFound = true; break; }
					}
					if (!bFound)
					{
						FVectorParameterValue NewVV;
						NewVV.ParameterInfo.Name = ParamName;
						NewVV.ParameterValue = Param->VectorValue;
						Instance->VectorParameterValues.Add(NewVV);
					}
					break;
				}
				case EMLPParameterType::Texture:
				{
					UTexture* Tex = Param->TextureValue.Get();
					if (!Tex) break;
					bool bFound = false;
					for (FTextureParameterValue& TV : Instance->TextureParameterValues)
					{
						if (TV.ParameterInfo.Name == ParamName) { TV.ParameterValue = Tex; bFound = true; break; }
					}
					if (!bFound)
					{
						FTextureParameterValue NewTV;
						NewTV.ParameterInfo.Name = ParamName;
						NewTV.ParameterValue = Tex;
						Instance->TextureParameterValues.Add(NewTV);
					}
					break;
				}
				default:
					break;
				}
			}
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

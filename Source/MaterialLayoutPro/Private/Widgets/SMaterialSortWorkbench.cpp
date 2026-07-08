#include "Widgets/SMaterialSortWorkbench.h"
#include "MaterialLayoutProTheme.h"
#include "Styling/CoreStyle.h"
#if ENGINE_MAJOR_VERSION >= 5
#define MLP_STYLE FAppStyle
#else
#define MLP_STYLE FEditorStyle
#endif

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "ScopedTransaction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "SMaterialSortWorkbench"

void SMaterialSortWorkbench::Construct(const FArguments& InArgs)
{
	TargetMaterial = InArgs._TargetMaterial;
	WorkParameters = InArgs._Parameters;
	OnApplied = InArgs._OnApplied;

	// Snapshot the original Group/SortPriority so Reset can restore them.
	for (TSharedPtr<FMLPParameterInfo>& Param : WorkParameters)
	{
		// WorkParameters shares the same FMLPParameterInfo pointers as the panel.
		// We edit Group/SortPriority on them directly (working copy), and only
		// write back to the material on Apply.
	}

	RebuildWorkingGroups();

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "排序工作台"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(640.f, 560.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FMLPTheme::Background())
			.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
			.Padding(FMLPTheme::PadMD())
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.f, 0.f, 0.f, 8.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(TargetMaterial.IsValid()
							? FText::Format(LOCTEXT("TargetFmt", "正在排序：{0}"), FText::FromString(TargetMaterial->GetName()))
							: LOCTEXT("NoTarget", "无材质"))
						.Font(FMLPTheme::FontHeading())
						.ColorAndOpacity(FMLPTheme::Foreground())
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
						.ContentPadding(FMLPTheme::PadBtn())
						.Text(LOCTEXT("Reset", "重置为材质"))
						.ToolTipText(LOCTEXT("ResetTooltip", "放弃所有工作更改并从材质重新加载"))
						.OnClicked(this, &SMaterialSortWorkbench::OnResetClicked)
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(FMargin(0.f, 0.f, 0.f, 8.f))
				[
					SNew(SBorder)
					.BorderBackgroundColor(FMLPTheme::Surface())
					.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(ListContainer, SVerticalBox)
						]
					]
				]
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
						.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
						.ButtonColorAndOpacity(FMLPTheme::ButtonPrimary())
						.ForegroundColor(FMLPTheme::ButtonTextOnColor())
						.ContentPadding(FMLPTheme::PadBtn())
						.Text(LOCTEXT("Apply", "应用更改"))
						.ToolTipText(LOCTEXT("ApplyTooltip", "以单次撤销步骤将分组和排序优先级写回材质"))
						.OnClicked(this, &SMaterialSortWorkbench::OnApplyClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
						.ContentPadding(FMLPTheme::PadBtn())
						.Text(LOCTEXT("Cancel", "Cancel"))
						.OnClicked(this, &SMaterialSortWorkbench::OnCancelClicked)
					]
				]
			]
		]);

	RebuildList();
}

void SMaterialSortWorkbench::RebuildWorkingGroups()
{
	WorkGroups.Reset();

	TMap<FName, TSharedPtr<FWorkGroup>> GroupMap;
	for (TSharedPtr<FMLPParameterInfo>& Param : WorkParameters)
	{
		if (!Param.IsValid())
		{
			continue;
		}
		FName GroupName = Param->Group.IsNone() ? FName(TEXT("(None)")) : Param->Group;
		TSharedPtr<FWorkGroup>* Found = GroupMap.Find(GroupName);
		if (!Found)
		{
			TSharedPtr<FWorkGroup> NewGroup = MakeShared<FWorkGroup>();
			NewGroup->Name = GroupName;
			GroupMap.Add(GroupName, NewGroup);
			WorkGroups.Add(NewGroup);
			Found = &GroupMap.FindChecked(GroupName);
		}
		(*Found)->Parameters.Add(Param);
	}

	WorkGroups.Sort([](const TSharedPtr<FWorkGroup>& A, const TSharedPtr<FWorkGroup>& B)
	{
		return A->Name.ToString() < B->Name.ToString();
	});

	for (TSharedPtr<FWorkGroup>& Group : WorkGroups)
	{
		Group->Parameters.Sort([](const TSharedPtr<FMLPParameterInfo>& A, const TSharedPtr<FMLPParameterInfo>& B)
		{
			if (A->SortPriority != B->SortPriority)
			{
				return A->SortPriority < B->SortPriority;
			}
			return A->Name.ToString() < B->Name.ToString();
		});
	}
}

void SMaterialSortWorkbench::RebuildList()
{
	if (!ListContainer.IsValid())
	{
		return;
	}
	ListContainer->ClearChildren();

	for (TSharedPtr<FWorkGroup>& Group : WorkGroups)
	{
		ListContainer->AddSlot()
			.AutoHeight()
			[
				BuildGroupHeader(Group)
			];

		if (Group->bExpanded)
		{
			for (TSharedPtr<FMLPParameterInfo>& Param : Group->Parameters)
			{
				ListContainer->AddSlot()
					.AutoHeight()
					[
						BuildRow(Param, Group)
					];
			}
		}
	}
}

TSharedRef<SWidget> SMaterialSortWorkbench::BuildGroupHeader(TSharedPtr<FWorkGroup> Group)
{
	FText ExpandedText = Group->bExpanded ? FText::FromString(TEXT("\u25BC")) : FText::FromString(TEXT("\u25B6"));

	return SNew(SBorder)
		.BorderBackgroundColor(FMLPTheme::SurfaceAlt())
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMLPTheme::PadSM())
		.OnMouseButtonDown_Lambda([this, Group](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				ToggleGroup(Group);
				return FReply::Handled();
			}
			return FReply::Unhandled();
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(ExpandedText)
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Muted())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(STextBlock)
				.Text(FText::FromName(Group->Name))
				.Font(FMLPTheme::FontHeading())
				.ColorAndOpacity(FMLPTheme::Foreground())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(Group->Parameters.Num()))
				.Font(FMLPTheme::FontSmall())
				.ColorAndOpacity(FMLPTheme::Muted())
			]
		];
}

TSharedRef<SWidget> SMaterialSortWorkbench::BuildRow(TSharedPtr<FMLPParameterInfo> Param, TSharedPtr<FWorkGroup> Group)
{
	if (!Param.IsValid())
	{
		return SNew(STextBlock).Text(LOCTEXT("InvalidRow", "无效"));
	}

	return SNew(SBorder)
		.BorderBackgroundColor(FMLPTheme::Surface())
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMLPTheme::PadSM())
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
			.FillWidth(0.35f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(STextBlock)
				.Text(FText::FromName(Param->Name))
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Foreground())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.30f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(SEditableTextBox)
				.Text(FText::FromName(Param->Group))
				.Font(FMLPTheme::FontBody())
				.OnTextCommitted_Lambda([this, Param](const FText& NewText, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
					{
						if (Param.IsValid())
						{
							Param->Group = FName(*NewText.ToString());
							RebuildWorkingGroups();
							RebuildList();
						}
					}
				})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.15f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(SNumericEntryBox<int32>)
				.Value(TOptional<int32>(Param->SortPriority))
				.Font(FMLPTheme::FontBody())
				.OnValueCommitted_Lambda([this, Param](int32 NewValue, ETextCommit::Type CommitType)
				{
					if (Param.IsValid())
					{
						Param->SortPriority = NewValue;
						RebuildWorkingGroups();
						RebuildList();
					}
				})
			]
		];
}

void SMaterialSortWorkbench::ToggleGroup(TSharedPtr<FWorkGroup> Group)
{
	if (Group.IsValid())
	{
		Group->bExpanded = !Group->bExpanded;
		RebuildList();
	}
}

FReply SMaterialSortWorkbench::OnApplyClicked()
{
	if (!TargetMaterial.IsValid())
	{
		RequestDestroyWindow();
		return FReply::Handled();
	}

	UMaterial* Material = TargetMaterial.Get();
	const FScopedTransaction Transaction(LOCTEXT("ApplySortWorkbench", "应用排序工作台更改"));
	Material->Modify();

	// Write Group + SortPriority back for every parameter.
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

FReply SMaterialSortWorkbench::OnCancelClicked()
{
	// Restore the original Group/SortPriority from the material so the shared
	// FMLPParameterInfo pointers reflect the material's true state.
	if (TargetMaterial.IsValid())
	{
		for (const TSharedPtr<FMLPParameterInfo>& Param : WorkParameters)
		{
			if (Param.IsValid() && Param->Expression.IsValid())
			{
				if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Param->Expression.Get()))
				{
					Param->Group = ParamExpr->Group;
					Param->SortPriority = ParamExpr->SortPriority;
				}
			}
		}
	}
	RequestDestroyWindow();
	return FReply::Handled();
}

FReply SMaterialSortWorkbench::OnResetClicked()
{
	// Reload Group/SortPriority from the material.
	if (TargetMaterial.IsValid())
	{
		for (const TSharedPtr<FMLPParameterInfo>& Param : WorkParameters)
		{
			if (Param.IsValid() && Param->Expression.IsValid())
			{
				if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Param->Expression.Get()))
				{
					Param->Group = ParamExpr->Group;
					Param->SortPriority = ParamExpr->SortPriority;
				}
			}
		}
	}
	RebuildWorkingGroups();
	RebuildList();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

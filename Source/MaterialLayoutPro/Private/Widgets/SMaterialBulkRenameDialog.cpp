#include "Widgets/SMaterialBulkRenameDialog.h"
#include "MaterialLayoutProTheme.h"
#include "Styling/CoreStyle.h"
#if ENGINE_MAJOR_VERSION >= 5
#define MLP_STYLE FAppStyle
#else
#define MLP_STYLE FEditorStyle
#endif

#include "Internationalization/Regex.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "ScopedTransaction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif

#define LOCTEXT_NAMESPACE "SMaterialBulkRenameDialog"

void SMaterialBulkRenameDialog::Construct(const FArguments& InArgs)
{
	TargetMaterial = InArgs._TargetMaterial;
	Parameters = InArgs._Parameters;
	OnRenamed = InArgs._OnRenamed;

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("BulkRenameTitle", "批量重命名参数"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(500.f, 400.f))
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
					SNew(STextBlock)
					.Text(LOCTEXT("BulkRenameDesc", "使用查找与替换重命名选中的参数。"))
					.Font(FMLPTheme::FontBody())
					.ColorAndOpacity(FMLPTheme::Muted())
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("FindLabel", "查找："))
						.Font(FMLPTheme::FontBody())
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(FindTextBox, SEditableTextBox)
						.Font(FMLPTheme::FontBody())
						.OnTextChanged(this, &SMaterialBulkRenameDialog::UpdatePreview)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ReplaceLabel", "替换："))
						.Font(FMLPTheme::FontBody())
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(ReplaceTextBox, SEditableTextBox)
						.Font(FMLPTheme::FontBody())
						.OnTextChanged(this, &SMaterialBulkRenameDialog::UpdatePreview)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.f, 0.f, 0.f, 8.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(RegexCheckBox, SCheckBox)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RegexLabel", "使用正则表达式"))
						.Font(FMLPTheme::FontBody())
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
						SAssignNew(PreviewList, SListView<TSharedPtr<FPreviewItem>>)
						.ListItemsSource(&PreviewData)
						.OnGenerateRow(this, &SMaterialBulkRenameDialog::OnGeneratePreviewRow)
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
						.Text(LOCTEXT("RenameButton", "重命名"))
						.OnClicked(this, &SMaterialBulkRenameDialog::OnRenameClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
						.ContentPadding(FMLPTheme::PadBtn())
						.Text(LOCTEXT("CancelButton", "Cancel"))
						.OnClicked(this, &SMaterialBulkRenameDialog::OnCancelClicked)
					]
				]
			]
		]);

	UpdatePreview();
}

void SMaterialBulkRenameDialog::UpdatePreview(const FText& NewText)
{
	PreviewData.Reset();

	for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (!Param.IsValid()) continue;

		FString OldName = Param->Name.ToString();
		FString NewName = ComputeNewName(OldName);
		if (NewName != OldName)
		{
			TSharedPtr<FPreviewItem> Item = MakeShared<FPreviewItem>();
			Item->Param = Param;
			Item->NewName = NewName;
			PreviewData.Add(Item);
		}
	}

	if (PreviewList.IsValid())
	{
		PreviewList->RequestListRefresh();
	}
}

FString SMaterialBulkRenameDialog::ComputeNewName(const FString& OldName) const
{
	FString Find = FindTextBox.IsValid() ? FindTextBox->GetText().ToString() : FString();
	FString Replace = ReplaceTextBox.IsValid() ? ReplaceTextBox->GetText().ToString() : FString();
	bool bRegex = RegexCheckBox.IsValid() ? RegexCheckBox->IsChecked() : false;

	if (Find.IsEmpty())
	{
		return OldName;
	}

	if (bRegex)
	{
		FRegexPattern Pattern(Find);
		FRegexMatcher Matcher(Pattern, OldName);
		FString Result;
		int32 LastPos = 0;
		bool bFoundAny = false;
		while (Matcher.FindNext())
		{
			bFoundAny = true;
			int32 Start = Matcher.GetMatchBeginning();
			int32 End = Matcher.GetMatchEnding();
			Result += OldName.Mid(LastPos, Start - LastPos);
			Result += Replace;
			LastPos = End;
		}
		if (bFoundAny)
		{
			Result += OldName.Mid(LastPos);
			return Result;
		}
		return OldName;
	}
	else
	{
		FString Result = OldName;
		Result.ReplaceInline(*Find, *Replace);
		return Result;
	}
}

FReply SMaterialBulkRenameDialog::OnRenameClicked()
{
	if (!TargetMaterial.IsValid() || PreviewData.Num() == 0)
	{
		RequestDestroyWindow();
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("BulkRenameParameters", "批量重命名参数"));
	UMaterial* Material = TargetMaterial.Get();
	Material->Modify();

	for (TSharedPtr<FPreviewItem>& Item : PreviewData)
	{
		if (!Item->Param.IsValid() || !Item->Param->Expression.IsValid()) continue;

		UMaterialExpression* Expression = Item->Param->Expression.Get();
		UMaterialExpressionParameter* ParamExpression = Cast<UMaterialExpressionParameter>(Expression);
		if (!ParamExpression) continue;

		ParamExpression->Modify();
		ParamExpression->ParameterName = FName(*Item->NewName);
		Item->Param->Name = ParamExpression->ParameterName;
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();

	OnRenamed.ExecuteIfBound();

	RequestDestroyWindow();
	return FReply::Handled();
}

FReply SMaterialBulkRenameDialog::OnCancelClicked()
{
	RequestDestroyWindow();
	return FReply::Handled();
}

TSharedRef<ITableRow> SMaterialBulkRenameDialog::OnGeneratePreviewRow(TSharedPtr<FPreviewItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FPreviewItem>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Param->Name.ToString()))
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Foreground())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("→")))
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Muted())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->NewName))
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Accent())
			]
		];
}

#undef LOCTEXT_NAMESPACE

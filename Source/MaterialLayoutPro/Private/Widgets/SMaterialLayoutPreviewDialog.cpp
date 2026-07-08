#include "Widgets/SMaterialLayoutPreviewDialog.h"
#include "MaterialLayoutProTheme.h"
#include "Styling/CoreStyle.h"
#if ENGINE_MAJOR_VERSION >= 5
#define MLP_STYLE FAppStyle
#else
#define MLP_STYLE FEditorStyle
#endif

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif

#define LOCTEXT_NAMESPACE "SMaterialLayoutPreviewDialog"

void SMaterialLayoutPreviewDialog::Construct(const FArguments& InArgs)
{
	// Convert raw changes into shared pointers for the list view.
	for (const FPreviewChange& Change : InArgs._Changes)
	{
		PendingChanges.Add(MakeShared<FPreviewChange>(Change));
	}

	SWindow::Construct(SWindow::FArguments()
		.Title(InArgs._Title)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(520.f, 420.f))
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
					.Text(InArgs._Description)
					.Font(FMLPTheme::FontBody())
					.ColorAndOpacity(FMLPTheme::Muted())
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(FMargin(0.f, 0.f, 0.f, 8.f))
				[
					SNew(SBorder)
					.BorderBackgroundColor(FMLPTheme::Surface())
					.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
					[
						SNew(SListView<TSharedPtr<FPreviewChange>>)
						.ListItemsSource(&PendingChanges)
						.OnGenerateRow(this, &SMaterialLayoutPreviewDialog::OnGenerateChangeRow)
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
						.Text(LOCTEXT("Confirm", "应用更改"))
						.OnClicked(this, &SMaterialLayoutPreviewDialog::OnConfirmClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
						.ContentPadding(FMLPTheme::PadBtn())
						.Text(LOCTEXT("Cancel", "Cancel"))
						.OnClicked(this, &SMaterialLayoutPreviewDialog::OnCancelClicked)
					]
				]
			]
		]);
}

FReply SMaterialLayoutPreviewDialog::OnConfirmClicked()
{
	bConfirmed = true;
	RequestDestroyWindow();
	return FReply::Handled();
}

FReply SMaterialLayoutPreviewDialog::OnCancelClicked()
{
	bConfirmed = false;
	RequestDestroyWindow();
	return FReply::Handled();
}

TSharedRef<ITableRow> SMaterialLayoutPreviewDialog::OnGenerateChangeRow(TSharedPtr<FPreviewChange> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FPreviewChange>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(STextBlock)
				.Text(FText::FromName(Item->Param->Name))
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Foreground())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(STextBlock)
				.Text(FText::FromName(Item->OldGroup))
				.Font(FMLPTheme::FontSmall())
				.ColorAndOpacity(FMLPTheme::Muted())
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
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(STextBlock)
				.Text(FText::FromName(Item->NewGroup))
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Accent())
			]
		];
}

#undef LOCTEXT_NAMESPACE

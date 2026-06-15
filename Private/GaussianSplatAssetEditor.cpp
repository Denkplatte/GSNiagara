#include "GaussianSplatAssetEditor.h"

#if WITH_EDITOR

#include "GaussianSplatAsset.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Math/UnrealMathUtility.h"


const FName FGaussianSplatAssetEditor::SummaryTabId(TEXT("NiagaraGS_SummaryTab"));

// ── Toolkit identity ──────────────────────────────────────────────────────────

FName FGaussianSplatAssetEditor::GetToolkitFName() const
{
    return FName(TEXT("GaussianSplatAssetEditor"));
}

FText FGaussianSplatAssetEditor::GetBaseToolkitName() const
{
    return FText::FromString(TEXT("Gaussian Splat Editor"));
}

FText FGaussianSplatAssetEditor::GetToolkitName() const
{
    return FText::FromString(Asset ? Asset->GetName() : TEXT("Gaussian Splat"));
}

FText FGaussianSplatAssetEditor::GetToolkitToolTipText() const
{
    return FText::FromString(TEXT("Gaussian Splat Asset"));
}

FString FGaussianSplatAssetEditor::GetWorldCentricTabPrefix() const
{
    return TEXT("GaussianSplat ");
}

FLinearColor FGaussianSplatAssetEditor::GetWorldCentricTabColorScale() const
{
    return FLinearColor(0.0f, 0.74f, 0.74f, 1.0f);  // teal to match asset icon
}

// ── Tab registration ──────────────────────────────────────────────────────────

void FGaussianSplatAssetEditor::RegisterTabSpawners(
    const TSharedRef<FTabManager>& TM)
{
    FAssetEditorToolkit::RegisterTabSpawners(TM);

    TM->RegisterTabSpawner(SummaryTabId,
        FOnSpawnTab::CreateSP(this, &FGaussianSplatAssetEditor::SpawnSummaryTab))
        .SetDisplayName(FText::FromString(TEXT("Summary")))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory());
}

void FGaussianSplatAssetEditor::UnregisterTabSpawners(
    const TSharedRef<FTabManager>& TM)
{
    FAssetEditorToolkit::UnregisterTabSpawners(TM);
    TM->UnregisterTabSpawner(SummaryTabId);
}

// ── Init ──────────────────────────────────────────────────────────────────────

void FGaussianSplatAssetEditor::InitEditor(
    const EToolkitMode::Type        Mode,
    const TSharedPtr<IToolkitHost>& InitToolkitHost,
    UGaussianSplatAsset* InAsset)
{
    Asset = InAsset;

    // Minimal layout: one tab, the summary panel
    const TSharedRef<FTabManager::FLayout> Layout =
        FTabManager::NewLayout("NiagaraGS_AssetEditor_v1")
        ->AddArea(
            FTabManager::NewPrimaryArea()
            ->SetOrientation(Orient_Vertical)
            ->Split(
                FTabManager::NewStack()
                ->AddTab(SummaryTabId, ETabState::OpenedTab)
                ->SetHideTabWell(true)
            )
        );

    InitAssetEditor(
        Mode,
        InitToolkitHost,
        FName(TEXT("NiagaraGSAssetEditor")),
        Layout,
        /*bCreateDefaultStandaloneMenu=*/ true,
        /*bCreateDefaultToolbar=*/        true,
        InAsset
    );
}

// ── Summary tab ───────────────────────────────────────────────────────────────

TSharedRef<SDockTab> FGaussianSplatAssetEditor::SpawnSummaryTab(
    const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::PanelTab)
        [
            BuildSummaryWidget().ToSharedRef()
        ];
}

TSharedPtr<SWidget> FGaussianSplatAssetEditor::BuildSummaryWidget()
{
    if (!Asset)
    {
        return SNew(STextBlock).Text(FText::FromString(TEXT("No asset loaded.")));
    }

    // Compute quick stats from the splat array without iterating in the UI
    const int32 Count = Asset->SplatData.Num();

    FVector3f BoundsMin(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector3f BoundsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    float OpacitySum = 0.0f;

    for (const FGaussianSplatData& S : Asset->SplatData)
    {
        BoundsMin.X = FMath::Min(BoundsMin.X, S.Position.X);
        BoundsMin.Y = FMath::Min(BoundsMin.Y, S.Position.Y);
        BoundsMin.Z = FMath::Min(BoundsMin.Z, S.Position.Z);
        BoundsMax.X = FMath::Max(BoundsMax.X, S.Position.X);
        BoundsMax.Y = FMath::Max(BoundsMax.Y, S.Position.Y);
        BoundsMax.Z = FMath::Max(BoundsMax.Z, S.Position.Z);
        OpacitySum += S.Opacity;
    }

    const FVector3f Extent = (Count > 0) ? (BoundsMax - BoundsMin) : FVector3f::ZeroVector;
    const float     AvgOpacity = (Count > 0) ? (OpacitySum / Count) : 0.0f;

    // Helper to build one label+value row
    auto Row = [](const FString& Label, const FString& Value) -> TSharedRef<SWidget>
        {
            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(8.0f, 4.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(Label))
                        .MinDesiredWidth(180.0f)
                        .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(8.0f, 4.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(Value))
                ];
        };

    return SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SVerticalBox)

                // ── Header ──────────────────────────────────────────────
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.0f, 12.0f, 8.0f, 4.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Gaussian Splat Asset")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                ]

                + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)
                [SNew(SSeparator)]

                // ── Source ──────────────────────────────────────────────
                + SVerticalBox::Slot().AutoHeight()
                [Row(TEXT("Source File"), Asset->SourceFilePath)]

                + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)
                [SNew(SSeparator)]

                // ── Stats ───────────────────────────────────────────────
                + SVerticalBox::Slot().AutoHeight()
                [Row(TEXT("Splat Count"), FString::FormatAsNumber(Count))]

                + SVerticalBox::Slot().AutoHeight()
                [Row(TEXT("Bounds Min (cm)"),
                    FString::Printf(TEXT("X=%.1f  Y=%.1f  Z=%.1f"),
                        BoundsMin.X, BoundsMin.Y, BoundsMin.Z))]

                + SVerticalBox::Slot().AutoHeight()
                [Row(TEXT("Bounds Max (cm)"),
                    FString::Printf(TEXT("X=%.1f  Y=%.1f  Z=%.1f"),
                        BoundsMax.X, BoundsMax.Y, BoundsMax.Z))]

                + SVerticalBox::Slot().AutoHeight()
                [Row(TEXT("Extent (cm)"),
                    FString::Printf(TEXT("X=%.1f  Y=%.1f  Z=%.1f"),
                        Extent.X, Extent.Y, Extent.Z))]

                + SVerticalBox::Slot().AutoHeight()
                [Row(TEXT("Avg Opacity"), FString::Printf(TEXT("%.3f"), AvgOpacity))]
        ];
}

#endif // WITH_EDITOR
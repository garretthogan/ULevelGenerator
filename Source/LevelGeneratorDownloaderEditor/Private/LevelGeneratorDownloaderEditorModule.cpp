#include "LevelGeneratorDownloaderEditorModule.h"

#include "Async/Async.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DesktopPlatformModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IDesktopPlatform.h"
#include "Interfaces/IPluginManager.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "MeshMergeModule.h"
#include "InterchangeManager.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericMaterialPipeline.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeSourceData.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "LevelEditor.h"
#include "Engine/MeshMerging.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "SWebBrowser.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FLevelGeneratorDownloaderEditorModule"

const FName FLevelGeneratorDownloaderEditorModule::PluginTabName(TEXT("Level Generator"));

class SLevelGeneratorDownloaderPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLevelGeneratorDownloaderPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		bCombineStaticMeshes = true;
		bUseSingleMaterialAsset = true;
		bAutoImportDownloads = true;
		bIsDownloading = false;
		bIsImporting = false;
		bShowSuccessIndicator = false;

		RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateSP(this, &SLevelGeneratorDownloaderPanel::OnAutoImportTick));

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor::Black)
			.Padding(0.0f)
			[
				SAssignNew(RootSwitcher, SWidgetSwitcher)
				.WidgetIndex(0)
				+ SWidgetSwitcher::Slot()
				[
					SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(8.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor::Black)
					[
						SAssignNew(WebBrowserWidget, SWebBrowser)
						.InitialURL(TEXT("https://garretthogan.github.io/level-generator/?unreal-engine=true"))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f)
				[
					SNew(SBorder)
					.Visibility(this, &SLevelGeneratorDownloaderPanel::GetManualImportVisibility)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(1.0f, 0.0f, 1.0f, 1.0f))
					.Padding(2.0f)
					[
						SNew(SButton)
						.ButtonColorAndOpacity(FLinearColor::Black)
						.OnClicked(this, &SLevelGeneratorDownloaderPanel::OnImportLocalGlbClicked)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ImportLocalButton", "Import"))
							.Font(GetIosevkaFont(10))
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 4.0f)
				[
					SAssignNew(AutoImportDownloadsCheckBox, SCheckBox)
					.IsChecked(ECheckBoxState::Checked)
					.OnCheckStateChanged(this, &SLevelGeneratorDownloaderPanel::OnAutoImportDownloadsCheckStateChanged)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AutoImportDownloadsLabel", "Auto import new .glb files from Downloads"))
						.Font(GetIosevkaFont())
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 4.0f)
				[
					SAssignNew(CombineMeshesCheckBox, SCheckBox)
					.IsChecked(ECheckBoxState::Checked)
					.OnCheckStateChanged(this, &SLevelGeneratorDownloaderPanel::OnCombineMeshesCheckStateChanged)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CombineMeshesLabel", "Combine all imported meshes into a single static mesh"))
						.Font(GetIosevkaFont())
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 6.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FloorMaterialLabel", "Floor Material"))
					.Font(GetIosevkaFont())
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 3.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UMaterialInterface::StaticClass())
					.ObjectPath(this, &SLevelGeneratorDownloaderPanel::GetFloorMaterialObjectPath)
					.OnObjectChanged(this, &SLevelGeneratorDownloaderPanel::OnFloorMaterialChanged)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 6.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("WallMaterialLabel", "Wall Material"))
					.Font(GetIosevkaFont())
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 3.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UMaterialInterface::StaticClass())
					.ObjectPath(this, &SLevelGeneratorDownloaderPanel::GetWallMaterialObjectPath)
					.OnObjectChanged(this, &SLevelGeneratorDownloaderPanel::OnWallMaterialChanged)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 6.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OpeningMaterialLabel", "Openings Material"))
					.Font(GetIosevkaFont())
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 3.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UMaterialInterface::StaticClass())
					.ObjectPath(this, &SLevelGeneratorDownloaderPanel::GetOpeningMaterialObjectPath)
					.OnObjectChanged(this, &SLevelGeneratorDownloaderPanel::OnOpeningMaterialChanged)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 8.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DestinationLabel", "Destination Content Path"))
					.Font(GetIosevkaFont())
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 4.0f)
				[
					SAssignNew(DestinationPathTextBox, SEditableTextBox)
					.BackgroundColor(FLinearColor::Black)
					.Font(GetIosevkaFont())
					.Text(FText::FromString(TEXT("/Game/LevelGeneratorImports")))
				]
				// Status indicator removed per UI request
				]
				+ SWidgetSwitcher::Slot()
				[
					SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(SThrobber)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					.HAlign(HAlign_Center)
					[
						SAssignNew(LoadingText, STextBlock)
						.ColorAndOpacity(FLinearColor::White)
						.Font(GetIosevkaFont())
						.Text(LOCTEXT("LoadingStatus", "Importing GLB..."))
					]
				]
				]
				+ SWidgetSwitcher::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SButton)
						.ButtonColorAndOpacity(FLinearColor(0.0f, 0.35f, 0.0f, 1.0f))
						.ContentPadding(FMargin(24.0f, 14.0f))
						.OnClicked(this, &SLevelGeneratorDownloaderPanel::OnSuccessIndicatorClicked)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ImportSuccessIndicatorLabel", "Import successful - click to dismiss"))
							.Font(GetIosevkaFont(12))
						]
					]
				]
			]
		];

		InitializeAutoImportBaseline();
		SetStatus(TEXT("Auto import enabled. Watching Downloads for new .glb files."));
	}

private:
	static FSlateFontInfo GetIosevkaFont(const int32 Size = 10)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("LevelGeneratorDownloader"));
		if (Plugin.IsValid())
		{
			const FString FontPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Fonts/Iosevka-Regular.ttc"));
			if (FPaths::FileExists(FontPath))
			{
				return FSlateFontInfo(FontPath, Size);
			}
		}

		FSlateFontInfo Fallback = FAppStyle::GetFontStyle(TEXT("NormalFont"));
		Fallback.Size = Size;
		return Fallback;
	}

	EVisibility GetManualImportVisibility() const
	{
		return bAutoImportDownloads ? EVisibility::Collapsed : EVisibility::Visible;
	}

	FReply OnSuccessIndicatorClicked()
	{
		bShowSuccessIndicator = false;
		SetImportingState(bIsImporting);
		return FReply::Handled();
	}

	void OnCombineMeshesCheckStateChanged(ECheckBoxState NewState)
	{
		bCombineStaticMeshes = (NewState == ECheckBoxState::Checked);
	}

	void OnAutoImportDownloadsCheckStateChanged(ECheckBoxState NewState)
	{
		bAutoImportDownloads = (NewState == ECheckBoxState::Checked);
		if (bAutoImportDownloads)
		{
			InitializeAutoImportBaseline();
			SetStatus(TEXT("Auto import enabled. Watching Downloads for new .glb files."));
		}
		else
		{
			PendingDownloadSizes.Reset();
			KnownDownloadTimestamps.Reset();
			SetDownloadingState(false);
			SetStatus(TEXT("Auto import disabled."));
		}
	}

	void OnCombineMaterialsCheckStateChanged(ECheckBoxState NewState)
	{
		bUseSingleMaterialAsset = (NewState == ECheckBoxState::Checked);
	}

	void OnFloorMaterialChanged(const FAssetData& AssetData)
	{
		FloorMaterialObjectPath = AssetData.IsValid() ? AssetData.GetObjectPathString() : FString();
	}

	void OnWallMaterialChanged(const FAssetData& AssetData)
	{
		WallMaterialObjectPath = AssetData.IsValid() ? AssetData.GetObjectPathString() : FString();
	}

	void OnOpeningMaterialChanged(const FAssetData& AssetData)
	{
		OpeningMaterialObjectPath = AssetData.IsValid() ? AssetData.GetObjectPathString() : FString();
	}

	FString GetFloorMaterialObjectPath() const
	{
		return FloorMaterialObjectPath;
	}

	FString GetWallMaterialObjectPath() const
	{
		return WallMaterialObjectPath;
	}

	FString GetOpeningMaterialObjectPath() const
	{
		return OpeningMaterialObjectPath;
	}

	FReply OnImportLocalGlbClicked()
	{
		if (bIsImporting)
		{
			SetStatus(TEXT("Import already in progress."));
			return FReply::Handled();
		}

		if (!DestinationPathTextBox.IsValid())
		{
			SetStatus(TEXT("UI is not initialized."));
			return FReply::Handled();
		}

		FString DestinationPath = DestinationPathTextBox->GetText().ToString().TrimStartAndEnd();
		if (!DestinationPath.StartsWith(TEXT("/Game")))
		{
			SetStatus(TEXT("Destination path must start with /Game."));
			return FReply::Handled();
		}

		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (!DesktopPlatform)
		{
			SetStatus(TEXT("Desktop platform services unavailable."));
			return FReply::Handled();
		}

		TArray<FString> SelectedFiles;
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		const bool bPicked = DesktopPlatform->OpenFileDialog(
			ParentWindowHandle,
			TEXT("Select GLB File"),
			FPaths::ProjectDir(),
			TEXT(""),
			TEXT("GLB Files (*.glb)|*.glb"),
			EFileDialogFlags::None,
			SelectedFiles);

		if (!bPicked || SelectedFiles.Num() == 0)
		{
			SetStatus(TEXT("No file selected."));
			return FReply::Handled();
		}

		ImportGlb(SelectedFiles[0], DestinationPath);
		return FReply::Handled();
	}

	void ImportGlb(const FString& LocalFilePath, const FString& DestinationPath)
	{
		if (bIsImporting)
		{
			SetStatus(TEXT("Import already in progress."));
			return;
		}

		if (!FPaths::FileExists(LocalFilePath))
		{
			SetStatus(TEXT("GLB file does not exist."));
			return;
		}

		if (!LocalFilePath.EndsWith(TEXT(".glb"), ESearchCase::IgnoreCase))
		{
			SetStatus(TEXT("Selected file is not a .glb file."));
			return;
		}

		UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
		UInterchangeSourceData* SourceData = InterchangeManager.CreateSourceData(LocalFilePath);
		if (!SourceData)
		{
			SetStatus(TEXT("Failed to create Interchange source data."));
			return;
		}

		FImportAssetParameters ImportParameters;
		ImportParameters.bIsAutomated = true;
		ImportParameters.bReplaceExisting = true;

		UInterchangeGenericAssetsPipeline* PipelineOverride = NewObject<UInterchangeGenericAssetsPipeline>(GetTransientPackage());
		if (ensure(PipelineOverride && PipelineOverride->MeshPipeline))
		{
			PipelineOverride->MeshPipeline->bCombineStaticMeshes = bCombineStaticMeshes;

			if (PipelineOverride->MaterialPipeline)
			{
				PipelineOverride->MaterialPipeline->bImportMaterials = false;
			}

			PipelineOverride->AddToRoot();
			ActivePipelineOverrides.Add(PipelineOverride);
			ImportParameters.OverridePipelines.Add(FSoftObjectPath(PipelineOverride));
		}

		UE::Interchange::FAssetImportResultRef ImportResult = InterchangeManager.ImportAssetAsync(
			DestinationPath,
			SourceData,
			ImportParameters);

		SetDownloadingState(false);
		SetImportingState(true, FString::Printf(TEXT("Importing %s..."), *FPaths::GetCleanFilename(LocalFilePath)));

		UE::Interchange::FAssetImportResultPtr ImportResultPtr = ImportResult;
		ActiveImports.Add(ImportResultPtr);

		TWeakPtr<SLevelGeneratorDownloaderPanel> WeakPanel = StaticCastSharedRef<SLevelGeneratorDownloaderPanel>(AsShared());
		const bool bPostImportSingleMaterial = true;
		ImportResult->OnDone([WeakPanel, ImportResultPtr, DestinationPath, PipelineOverride, bPostImportSingleMaterial](UE::Interchange::FImportResult& Result)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakPanel, ImportResultPtr, DestinationPath, PipelineOverride, bPostImportSingleMaterial]()
			{
				if (!WeakPanel.IsValid())
				{
					if (PipelineOverride)
					{
						PipelineOverride->RemoveFromRoot();
					}
					return;
				}

				TSharedPtr<SLevelGeneratorDownloaderPanel> Pinned = WeakPanel.Pin();
				Pinned->ActiveImports.RemoveSingleSwap(ImportResultPtr);

				TArray<UObject*> ImportedObjects = ImportResultPtr.IsValid() ? ImportResultPtr->GetImportedObjects() : TArray<UObject*>();
				FString SplitStatus;
				if (!Pinned->bCombineStaticMeshes)
				{
					Pinned->CombineStructuralMeshesIfNeeded(ImportedObjects, DestinationPath, SplitStatus);
				}

				FString LevelInstanceStatus;
				if (!Pinned->bCombineStaticMeshes)
				{
					Pinned->CreateLevelInstanceFromCombinedMeshes(ImportedObjects, LevelInstanceStatus);
				}

				FString PostImportStatus;
				if (bPostImportSingleMaterial)
				{
					Pinned->ApplySingleMaterialAsset(ImportedObjects, DestinationPath, PostImportStatus);
					if (!SplitStatus.IsEmpty())
					{
						PostImportStatus = PostImportStatus.IsEmpty() ? SplitStatus : FString::Printf(TEXT("%s %s"), *SplitStatus, *PostImportStatus);
					}
					if (!LevelInstanceStatus.IsEmpty())
					{
						PostImportStatus = PostImportStatus.IsEmpty() ? LevelInstanceStatus : FString::Printf(TEXT("%s %s"), *PostImportStatus, *LevelInstanceStatus);
					}
				}

				if (PipelineOverride)
				{
					Pinned->ActivePipelineOverrides.RemoveSingleSwap(PipelineOverride);
					PipelineOverride->RemoveFromRoot();
				}

				const int32 ImportedCount = ImportResultPtr.IsValid() ? ImportResultPtr->GetImportedObjects().Num() : 0;
				if (ImportedCount > 0)
				{
					Pinned->bShowSuccessIndicator = true;
					if (!PostImportStatus.IsEmpty())
					{
						Pinned->SetStatus(FString::Printf(TEXT("Interchange imported %d asset(s) to %s. %s"), ImportedCount, *DestinationPath, *PostImportStatus));
					}
					else
					{
						Pinned->SetStatus(FString::Printf(TEXT("Interchange imported %d asset(s) to %s"), ImportedCount, *DestinationPath));
					}
				}
				else
				{
					Pinned->bShowSuccessIndicator = false;
					Pinned->SetStatus(TEXT("Interchange import completed with no imported assets."));
				}

				Pinned->SetImportingState(false);
			});
		});
	}

	void CombineStructuralMeshesIfNeeded(TArray<UObject*>& InOutImportedObjects, const FString& DestinationPath, FString& OutStatus)
	{
		TArray<UStaticMesh*> FloorMeshes;
		TArray<UStaticMesh*> WallMeshes;
		TArray<UStaticMesh*> OpeningMeshes;

		for (UObject* ImportedObject : InOutImportedObjects)
		{
			if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(ImportedObject))
			{
				const FString MeshNameLower = StaticMesh->GetName().ToLower();
				if (MeshNameLower.Contains(TEXT("floor")))
				{
					FloorMeshes.Add(StaticMesh);
				}
				else if (MeshNameLower.Contains(TEXT("wall")))
				{
					WallMeshes.Add(StaticMesh);
				}
				else if (MeshNameLower.Contains(TEXT("opening")))
				{
					OpeningMeshes.Add(StaticMesh);
				}
			}
		}

		if (FloorMeshes.Num() <= 1 && WallMeshes.Num() <= 1 && OpeningMeshes.Num() <= 1)
		{
			return;
		}

		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			OutStatus = TEXT("Could not merge floor/wall meshes: no editor world available.");
			return;
		}

		auto MergeGroup = [&](const TArray<UStaticMesh*>& GroupMeshes, const FString& OutputAssetName) -> UStaticMesh*
		{
			if (GroupMeshes.Num() == 0)
			{
				return nullptr;
			}

			if (GroupMeshes.Num() == 1)
			{
				return GroupMeshes[0];
			}

			AActor* TempActor = World->SpawnActor<AActor>();
			if (!TempActor)
			{
				return nullptr;
			}

			USceneComponent* RootComponent = NewObject<USceneComponent>(TempActor);
			RootComponent->RegisterComponentWithWorld(World);
			TempActor->SetRootComponent(RootComponent);

			TArray<UPrimitiveComponent*> ComponentsToMerge;
			TArray<UStaticMeshComponent*> CreatedComponents;
			for (UStaticMesh* SourceMesh : GroupMeshes)
			{
				if (!SourceMesh)
				{
					continue;
				}

				UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(TempActor);
				MeshComponent->SetStaticMesh(SourceMesh);
				MeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
				MeshComponent->RegisterComponentWithWorld(World);
				MeshComponent->SetWorldTransform(FTransform::Identity);

				CreatedComponents.Add(MeshComponent);
				ComponentsToMerge.Add(MeshComponent);
			}

			if (ComponentsToMerge.Num() == 0)
			{
				TempActor->Destroy();
				return nullptr;
			}

			FMeshMergingSettings MergeSettings;
			MergeSettings.bMergeMaterials = false;
			MergeSettings.bPivotPointAtZero = true;

			const FString BasePackageName = DestinationPath / OutputAssetName;
			TArray<UObject*> AssetsToSync;
			FVector MergedActorLocation = FVector::ZeroVector;

			IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::LoadModuleChecked<IMeshMergeModule>(TEXT("MeshMergeUtilities")).GetUtilities();
			MeshMergeUtilities.MergeComponentsToStaticMesh(
				ComponentsToMerge,
				World,
				MergeSettings,
				nullptr,
				nullptr,
				BasePackageName,
				AssetsToSync,
				MergedActorLocation,
				1.0f,
				true);

			for (UStaticMeshComponent* CreatedComponent : CreatedComponents)
			{
				if (CreatedComponent)
				{
					CreatedComponent->UnregisterComponent();
				}
			}

			TempActor->Destroy();

			for (UObject* Asset : AssetsToSync)
			{
				if (UStaticMesh* MergedMesh = Cast<UStaticMesh>(Asset))
				{
					return MergedMesh;
				}
			}

			return nullptr;
		};

		UStaticMesh* MergedFloorMesh = MergeGroup(FloorMeshes, TEXT("SM_LevelGenerator_Floor"));
		UStaticMesh* MergedWallMesh = MergeGroup(WallMeshes, TEXT("SM_LevelGenerator_Wall"));
		UStaticMesh* MergedOpeningMesh = MergeGroup(OpeningMeshes, TEXT("SM_LevelGenerator_Opening"));

		auto MoveChildrenToSubfolder = [&](const TArray<UStaticMesh*>& GroupMeshes, UStaticMesh* MergedMesh)
		{
			TArray<FAssetRenameData> AssetsToRename;
			const FString ChildrenPath = DestinationPath / TEXT("Children");

			for (UStaticMesh* Mesh : GroupMeshes)
			{
				if (Mesh && Mesh != MergedMesh)
				{
					AssetsToRename.Emplace(Mesh, ChildrenPath, Mesh->GetName());
				}
			}

			if (AssetsToRename.Num() > 0)
			{
				FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
				AssetToolsModule.Get().RenameAssets(AssetsToRename);
			}
		};

		MoveChildrenToSubfolder(FloorMeshes, MergedFloorMesh);
		MoveChildrenToSubfolder(WallMeshes, MergedWallMesh);
		MoveChildrenToSubfolder(OpeningMeshes, MergedOpeningMesh);

		InOutImportedObjects.RemoveAll([&](UObject* ImportedObject)
		{
			if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(ImportedObject))
			{
				for (UStaticMesh* FloorMesh : FloorMeshes)
				{
					if (FloorMesh == StaticMesh && FloorMesh != MergedFloorMesh)
					{
						return true;
					}
				}

				for (UStaticMesh* WallMesh : WallMeshes)
				{
					if (WallMesh == StaticMesh && WallMesh != MergedWallMesh)
					{
						return true;
					}
				}

				for (UStaticMesh* OpeningMesh : OpeningMeshes)
				{
					if (OpeningMesh == StaticMesh && OpeningMesh != MergedOpeningMesh)
					{
						return true;
					}
				}
			}

			return false;
		});

		if (MergedFloorMesh && !InOutImportedObjects.Contains(MergedFloorMesh))
		{
			InOutImportedObjects.Add(MergedFloorMesh);
		}

		if (MergedWallMesh && !InOutImportedObjects.Contains(MergedWallMesh))
		{
			InOutImportedObjects.Add(MergedWallMesh);
		}

		if (MergedOpeningMesh && !InOutImportedObjects.Contains(MergedOpeningMesh))
		{
			InOutImportedObjects.Add(MergedOpeningMesh);
		}

		OutStatus = TEXT("Merged floor/wall/opening groups into top-level meshes and moved source pieces to /Children.");
	}

	void CreateLevelInstanceFromCombinedMeshes(const TArray<UObject*>& ImportedObjects, FString& OutStatus)
	{
		UStaticMesh* FloorMesh = nullptr;
		UStaticMesh* WallMesh = nullptr;
		UStaticMesh* OpeningMesh = nullptr;
		for (UObject* ImportedObject : ImportedObjects)
		{
			if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(ImportedObject))
			{
				const FString NameLower = StaticMesh->GetName().ToLower();
				if (!FloorMesh && NameLower.Contains(TEXT("floor")))
				{
					FloorMesh = StaticMesh;
				}
				else if (!WallMesh && NameLower.Contains(TEXT("wall")))
				{
					WallMesh = StaticMesh;
				}
				else if (!OpeningMesh && NameLower.Contains(TEXT("opening")))
				{
					OpeningMesh = StaticMesh;
				}
			}
		}

		if (!FloorMesh && !WallMesh && !OpeningMesh)
		{
			return;
		}

		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			OutStatus = TEXT("Could not create level instance: no editor world available.");
			return;
		}

		TArray<AActor*> ActorsToMove;
		auto SpawnMeshActorAtOrigin = [&](UStaticMesh* Mesh, const TCHAR* Label)
		{
			if (!Mesh)
			{
				return;
			}

			AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>();
			if (!MeshActor)
			{
				return;
			}

			MeshActor->SetActorLabel(Label);
			MeshActor->SetActorTransform(FTransform::Identity);
			if (UStaticMeshComponent* StaticMeshComponent = MeshActor->GetStaticMeshComponent())
			{
				StaticMeshComponent->SetStaticMesh(Mesh);
			}

			ActorsToMove.Add(MeshActor);
		};

		SpawnMeshActorAtOrigin(FloorMesh, TEXT("LG_Floor"));
		SpawnMeshActorAtOrigin(WallMesh, TEXT("LG_Wall"));
		SpawnMeshActorAtOrigin(OpeningMesh, TEXT("LG_Opening"));

		if (ActorsToMove.Num() == 0)
		{
			OutStatus = TEXT("Could not create level instance actors.");
			return;
		}

		ULevelInstanceSubsystem* LevelInstanceSubsystem = World->GetSubsystem<ULevelInstanceSubsystem>();
		if (!LevelInstanceSubsystem)
		{
			OutStatus = TEXT("LevelInstanceSubsystem is unavailable.");
			return;
		}

		FNewLevelInstanceParams CreationParams;
		CreationParams.bAlwaysShowDialog = true;
		CreationParams.bPromptForSave = true;
		CreationParams.PivotType = ELevelInstancePivotType::WorldOrigin;

		ILevelInstanceInterface* NewLevelInstance = LevelInstanceSubsystem->CreateLevelInstanceFrom(ActorsToMove, CreationParams);
		if (NewLevelInstance)
		{
			OutStatus = TEXT("Created level instance.");
		}
		else
		{
			OutStatus = TEXT("Failed to create level instance.");
		}
	}

	EActiveTimerReturnType OnAutoImportTick(double InCurrentTime, float InDeltaTime)
	{
		if (!bAutoImportDownloads || bIsImporting)
		{
			return EActiveTimerReturnType::Continue;
		}

		if (!DestinationPathTextBox.IsValid())
		{
			return EActiveTimerReturnType::Continue;
		}

		FString DestinationPath = DestinationPathTextBox->GetText().ToString().TrimStartAndEnd();
		if (!DestinationPath.StartsWith(TEXT("/Game")))
		{
			return EActiveTimerReturnType::Continue;
		}

		const FString DownloadsDir = GetDownloadsDirectory();
		if (DownloadsDir.IsEmpty() || !IFileManager::Get().DirectoryExists(*DownloadsDir))
		{
			return EActiveTimerReturnType::Continue;
		}

		TArray<FString> GlbFiles;
		IFileManager::Get().FindFiles(GlbFiles, *(DownloadsDir / TEXT("*.glb")), true, false);

		TSet<FString> ExistingFiles;
		FString LatestEligiblePath;
		FString LatestEligibleName;
		FDateTime LatestEligibleTimestamp = FDateTime::MinValue();
		for (const FString& FileName : GlbFiles)
		{
			const FString FullPath = FPaths::Combine(DownloadsDir, FileName);
			ExistingFiles.Add(FullPath);
			const FDateTime FileTimestamp = IFileManager::Get().GetTimeStamp(*FullPath);

			if (const FDateTime* KnownTimestamp = KnownDownloadTimestamps.Find(FullPath))
			{
				if (FileTimestamp <= *KnownTimestamp)
				{
					continue;
				}
			}
			else if (FileTimestamp <= AutoImportEnabledUtc)
			{
				KnownDownloadTimestamps.Add(FullPath, FileTimestamp);
				continue;
			}

			if (FileTimestamp > LatestEligibleTimestamp)
			{
				LatestEligibleTimestamp = FileTimestamp;
				LatestEligiblePath = FullPath;
				LatestEligibleName = FileName;
			}
		}

		if (!LatestEligiblePath.IsEmpty())
		{
			for (auto It = PendingDownloadSizes.CreateIterator(); It; ++It)
			{
				if (It.Key() != LatestEligiblePath)
				{
					It.RemoveCurrent();
				}
			}

			const int64 CurrentSize = IFileManager::Get().FileSize(*LatestEligiblePath);
			if (int64* LastSize = PendingDownloadSizes.Find(LatestEligiblePath))
			{
				if (CurrentSize > 0 && *LastSize == CurrentSize)
				{
					SnapshotCurrentDownloadsAsKnown();
					SetStatus(FString::Printf(TEXT("Auto importing downloaded GLB: %s"), *LatestEligibleName));
					ImportGlb(LatestEligiblePath, DestinationPath);
				}
				else
				{
					*LastSize = CurrentSize;
					if (!bIsDownloading)
					{
						SetDownloadingState(true, FString::Printf(TEXT("Downloading %s..."), *LatestEligibleName));
					}
				}
			}
			else
			{
				PendingDownloadSizes.Add(LatestEligiblePath, CurrentSize);
				if (!bIsDownloading)
				{
					SetDownloadingState(true, FString::Printf(TEXT("Downloading %s..."), *LatestEligibleName));
				}
			}
		}
		else
		{
			PendingDownloadSizes.Reset();
		}

		for (auto It = PendingDownloadSizes.CreateIterator(); It; ++It)
		{
			if (!ExistingFiles.Contains(It.Key()))
			{
				It.RemoveCurrent();
			}
		}

		for (auto It = KnownDownloadTimestamps.CreateIterator(); It; ++It)
		{
			if (!ExistingFiles.Contains(It.Key()))
			{
				It.RemoveCurrent();
			}
		}

		if (!bIsImporting && PendingDownloadSizes.Num() == 0)
		{
			SetDownloadingState(false);
		}

		return EActiveTimerReturnType::Continue;
	}

	void InitializeAutoImportBaseline()
	{
		AutoImportEnabledUtc = FDateTime::UtcNow();
		SnapshotCurrentDownloadsAsKnown();
	}

	void SnapshotCurrentDownloadsAsKnown()
	{
		PendingDownloadSizes.Reset();
		KnownDownloadTimestamps.Reset();

		const FString DownloadsDir = GetDownloadsDirectory();
		if (DownloadsDir.IsEmpty() || !IFileManager::Get().DirectoryExists(*DownloadsDir))
		{
			return;
		}

		TArray<FString> GlbFiles;
		IFileManager::Get().FindFiles(GlbFiles, *(DownloadsDir / TEXT("*.glb")), true, false);
		for (const FString& FileName : GlbFiles)
		{
			const FString FullPath = FPaths::Combine(DownloadsDir, FileName);
			KnownDownloadTimestamps.Add(FullPath, IFileManager::Get().GetTimeStamp(*FullPath));
		}
	}

	FString GetDownloadsDirectory() const
	{
		FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
		if (!UserProfile.IsEmpty())
		{
			return FPaths::Combine(UserProfile, TEXT("Downloads"));
		}

		return FPaths::Combine(FPlatformProcess::UserDir(), TEXT("Downloads"));
	}

	void SetImportingState(bool bInImporting, const FString& InLoadingMessage = FString())
	{
		bIsImporting = bInImporting;

		if (bIsImporting)
		{
			bIsDownloading = false;
		}

		const bool bShowBusy = (bIsImporting || bIsDownloading);

		if (RootSwitcher.IsValid())
		{
			const int32 WidgetIndex = bShowBusy ? 1 : (bShowSuccessIndicator ? 2 : 0);
			RootSwitcher->SetActiveWidgetIndex(WidgetIndex);
		}

		if (bShowBusy && LoadingText.IsValid())
		{
			const FString Message = InLoadingMessage.IsEmpty()
				? (bIsImporting ? TEXT("Importing GLB...") : TEXT("Downloading GLB..."))
				: InLoadingMessage;
			LoadingText->SetText(FText::FromString(Message));
		}
	}

	void SetDownloadingState(bool bInDownloading, const FString& InLoadingMessage = FString())
	{
		if (bIsImporting && bInDownloading)
		{
			return;
		}

		bIsDownloading = bInDownloading;
		SetImportingState(bIsImporting, InLoadingMessage);
	}

	void ApplySingleMaterialAsset(const TArray<UObject*>& ImportedObjects, const FString& DestinationPath, FString& OutStatus)
	{
		TArray<UStaticMesh*> ImportedStaticMeshes;
		UMaterialInterface* ParentMaterial = nullptr;
		UMaterialInterface* FloorOverrideMaterial = nullptr;
		UMaterialInterface* WallOverrideMaterial = nullptr;
		UMaterialInterface* OpeningOverrideMaterial = nullptr;

		if (!FloorMaterialObjectPath.IsEmpty())
		{
			FloorOverrideMaterial = LoadObject<UMaterialInterface>(nullptr, *FloorMaterialObjectPath);
		}

		if (!WallMaterialObjectPath.IsEmpty())
		{
			WallOverrideMaterial = LoadObject<UMaterialInterface>(nullptr, *WallMaterialObjectPath);
		}

		if (!OpeningMaterialObjectPath.IsEmpty())
		{
			OpeningOverrideMaterial = LoadObject<UMaterialInterface>(nullptr, *OpeningMaterialObjectPath);
		}

		for (UObject* ImportedObject : ImportedObjects)
		{
			if (!ParentMaterial)
			{
				ParentMaterial = Cast<UMaterialInterface>(ImportedObject);
			}

			if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(ImportedObject))
			{
				ImportedStaticMeshes.Add(StaticMesh);
			}
		}

		if (ImportedStaticMeshes.Num() == 0)
		{
			OutStatus = TEXT("No static meshes found for single-material assignment.");
			return;
		}

		if (!ParentMaterial)
		{
			for (UStaticMesh* StaticMesh : ImportedStaticMeshes)
			{
				for (const FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials())
				{
					if (StaticMaterial.MaterialInterface)
					{
						ParentMaterial = StaticMaterial.MaterialInterface;
						break;
					}
				}

				if (ParentMaterial)
				{
					break;
				}
			}
		}

		if (!ParentMaterial)
		{
			ParentMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		if (!ParentMaterial)
		{
			OutStatus = TEXT("Could not resolve a parent material.");
			return;
		}

		const FString SharedMaterialAssetName = TEXT("MI_LevelGenerator_Combined");
		const FString SharedMaterialPackagePath = DestinationPath / SharedMaterialAssetName;
		const FString SharedMaterialObjectPath = SharedMaterialPackagePath + TEXT(".") + SharedMaterialAssetName;

		UMaterialInstanceConstant* SharedMaterial = LoadObject<UMaterialInstanceConstant>(nullptr, *SharedMaterialObjectPath);
		if (!SharedMaterial)
		{
			UPackage* MaterialPackage = CreatePackage(*SharedMaterialPackagePath);
			if (!MaterialPackage)
			{
				OutStatus = TEXT("Failed to create package for shared material.");
				return;
			}

			SharedMaterial = NewObject<UMaterialInstanceConstant>(
				MaterialPackage,
				*SharedMaterialAssetName,
				RF_Public | RF_Standalone | RF_Transactional);

			if (!SharedMaterial)
			{
				OutStatus = TEXT("Failed to create shared material asset.");
				return;
			}

			FAssetRegistryModule::AssetCreated(SharedMaterial);
		}

		SharedMaterial->SetParentEditorOnly(ParentMaterial);
		SharedMaterial->PostEditChange();
		SharedMaterial->MarkPackageDirty();

		for (UStaticMesh* StaticMesh : ImportedStaticMeshes)
		{
			if (!StaticMesh)
			{
				continue;
			}

			UMaterialInterface* MaterialForMesh = SharedMaterial;
			const FString MeshNameLower = StaticMesh->GetName().ToLower();
			if (MeshNameLower.Contains(TEXT("floor")) && FloorOverrideMaterial)
			{
				MaterialForMesh = FloorOverrideMaterial;
			}
			else if (MeshNameLower.Contains(TEXT("wall")) && WallOverrideMaterial)
			{
				MaterialForMesh = WallOverrideMaterial;
			}
			else if (MeshNameLower.Contains(TEXT("opening")) && OpeningOverrideMaterial)
			{
				MaterialForMesh = OpeningOverrideMaterial;
			}

			const int32 MaterialSlotCount = StaticMesh->GetStaticMaterials().Num();
			for (int32 SlotIndex = 0; SlotIndex < MaterialSlotCount; ++SlotIndex)
			{
				StaticMesh->SetMaterial(SlotIndex, MaterialForMesh);
			}

			StaticMesh->PostEditChange();
			StaticMesh->MarkPackageDirty();
		}

		OutStatus = FString::Printf(TEXT("Assigned shared material %s to %d static mesh(es)."), *SharedMaterial->GetPathName(), ImportedStaticMeshes.Num());
	}

	void SetStatus(const FString& InStatus)
	{
		if (StatusText.IsValid())
		{
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("Status: %s"), *InStatus)));
		}
	}

	TSharedPtr<SEditableTextBox> DestinationPathTextBox;
	TSharedPtr<SCheckBox> AutoImportDownloadsCheckBox;
	TSharedPtr<SCheckBox> CombineMeshesCheckBox;
	TSharedPtr<SCheckBox> CombineMaterialsCheckBox;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> LoadingText;
	TSharedPtr<SWebBrowser> WebBrowserWidget;
	TSharedPtr<SWidgetSwitcher> RootSwitcher;
	bool bCombineStaticMeshes = false;
	bool bUseSingleMaterialAsset = false;
	bool bAutoImportDownloads = false;
	bool bIsDownloading = false;
	bool bIsImporting = false;
	bool bShowSuccessIndicator = false;
	FString FloorMaterialObjectPath;
	FString WallMaterialObjectPath;
	FString OpeningMaterialObjectPath;
	FDateTime AutoImportEnabledUtc = FDateTime::MinValue();
	TMap<FString, int64> PendingDownloadSizes;
	TMap<FString, FDateTime> KnownDownloadTimestamps;
	TArray<UE::Interchange::FAssetImportResultPtr> ActiveImports;
	TArray<UObject*> ActivePipelineOverrides;
};

void FLevelGeneratorDownloaderEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PluginTabName, FOnSpawnTab::CreateRaw(this, &FLevelGeneratorDownloaderEditorModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("PluginTabTitle", "Level Generator Downloader"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FLevelGeneratorDownloaderEditorModule::RegisterMenus));
}

void FLevelGeneratorDownloaderEditorModule::ShutdownModule()
{
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PluginTabName);
}

void FLevelGeneratorDownloaderEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("WindowLayout"));
	Section.AddMenuEntry(
		TEXT("OpenLevelGeneratorDownloader"),
		LOCTEXT("OpenLevelGeneratorDownloaderLabel", "Level Generator Downloader"),
		LOCTEXT("OpenLevelGeneratorDownloaderTooltip", "Open the Level Generator GLB downloader window."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FLevelGeneratorDownloaderEditorModule::OpenPluginWindow))
	);
}

void FLevelGeneratorDownloaderEditorModule::OpenPluginWindow()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PluginTabName);
}

TSharedRef<SDockTab> FLevelGeneratorDownloaderEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBox)
			.MinDesiredWidth(500.0f)
			[
				SNew(SLevelGeneratorDownloaderPanel)
			]
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLevelGeneratorDownloaderEditorModule, LevelGeneratorDownloaderEditor)

// Copyright Epic Games, Inc. All Rights Reserved.

#include "PuertsTools.h"
#include "PuertsToolsStyle.h"
#include "PuertsToolsCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "HAL/IConsoleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "BlueprintEditor.h"
#include "Engine/Blueprint.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include <Toolkits/AssetEditorToolkitMenuContext.h>
#include "Misc/PackageName.h"
#include "GameFramework/Actor.h"
#include "ui/CWidgetBase.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"   // FMenuBuilder
#include "Widgets/Input/SComboButton.h"           // SComboButton

static const FName PuertsToolsTabName("PuertsTools");
static void ExtractRegisterImports(const FString& Text, TArray<FString>& OutImports);
static FString BuildRegisterContent(const TArray<FString>& Imports);

DEFINE_LOG_CATEGORY_STATIC(LogPuertsTools, Log, All);

#define LOCTEXT_NAMESPACE "FPuertsToolsModule"


void FPuertsToolsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FPuertsToolsStyle::Initialize();
	FPuertsToolsStyle::ReloadTextures();

	FPuertsToolsCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FPuertsToolsCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FPuertsToolsModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPuertsToolsModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PuertsToolsTabName, FOnSpawnTab::CreateRaw(this, &FPuertsToolsModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FPuertsToolsTabTitle", "PuertsTools"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FPuertsToolsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FPuertsToolsStyle::Shutdown();

	FPuertsToolsCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PuertsToolsTabName);
}

TSharedRef<SDockTab> FPuertsToolsModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SButton)
						.Text(FText::FromString(TEXT("打开 Workspace")))
						.OnClicked(FOnClicked::CreateRaw(this, &FPuertsToolsModule::OnOpenWorkspaceClicked))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SButton)
						.Text(FText::FromString(TEXT("执行 tsc")))
						.OnClicked(FOnClicked::CreateRaw(this, &FPuertsToolsModule::OnRunTscClicked))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SButton)
						.Text(FText::FromString(TEXT("执行 Puerts.Gen FULL")))
						.OnClicked(FOnClicked::CreateRaw(this, &FPuertsToolsModule::OnRunPuertsGenClicked))
				]
		];
}

void FPuertsToolsModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PuertsToolsTabName);
}

void FPuertsToolsModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FPuertsToolsCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PuertsToolsPluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FPuertsToolsCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("AssetEditor.BlueprintEditor.ToolBar");
	if (Menu)
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("PluginTools");
		Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda(
			[this](FToolMenuSection& InSection)
			{
				UAssetEditorToolkitMenuContext* Context = InSection.FindContext<UAssetEditorToolkitMenuContext>();
				if (!Context) return;

				// 找到当前蓝图
				UBlueprint* Blueprint = nullptr;
				for (UObject* Obj : Context->GetEditingObjects())
				{
					if (UBlueprint* BP = Cast<UBlueprint>(Obj))
					{
						Blueprint = BP;
						break;
					}
				}
				if (!Blueprint) return;

				const FString TsFilePath = GetTsFilePathForBlueprint(Blueprint);
				const bool bHasFile = !TsFilePath.IsEmpty() && FPaths::FileExists(TsFilePath);

				// 用 ComboButton 做下拉菜单
				InSection.AddEntry(FToolMenuEntry::InitWidget(
					"PuertsGenerateTS",
					SNew(SComboButton)
					.HasDownArrow(true)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnGetMenuContent_Lambda([this, Blueprint]()
						{
							return BuildBlueprintTsMenu(Blueprint);
						})
					.ButtonContent()
					[
						SNew(STextBlock)
							.Text(FText::FromString(TEXT("Puerts")))
							.ColorAndOpacity(bHasFile ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor::White))
					],
					FText::FromString(TEXT("Puerts 相关操作")),
					true
				));
			}
		));
	}

	UToolMenu* WidgetMenu = UToolMenus::Get()->ExtendMenu("AssetEditor.WidgetBlueprintEditor.ToolBar");
	if (WidgetMenu)
	{
		FToolMenuSection& Section = WidgetMenu->FindOrAddSection("PluginTools");
		Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda(
			[this](FToolMenuSection& InSection)
			{
				UAssetEditorToolkitMenuContext* Context = InSection.FindContext<UAssetEditorToolkitMenuContext>();
				if (!Context) return;

				UBlueprint* Blueprint = nullptr;
				for (UObject* Obj : Context->GetEditingObjects())
				{
					if (UBlueprint* BP = Cast<UBlueprint>(Obj))
					{
						Blueprint = BP;
						break;
					}
				}
				if (!Blueprint) return;

				const FString TsFilePath = GetTsFilePathForBlueprint(Blueprint);
				const bool bHasFile = !TsFilePath.IsEmpty() && FPaths::FileExists(TsFilePath);

				InSection.AddEntry(FToolMenuEntry::InitWidget(
					"PuertsGenerateTS",
					SNew(SComboButton)
					.HasDownArrow(true)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnGetMenuContent_Lambda([this, Blueprint]()
						{
							return BuildBlueprintTsMenu(Blueprint);
						})
					.ButtonContent()
					[
						SNew(STextBlock)
							.Text(FText::FromString(TEXT("Puerts")))
							.ColorAndOpacity(bHasFile ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor::White))
					],
					FText::FromString(TEXT("Puerts 相关操作")),
					true
				));
			}
		));
	}
}

FString FPuertsToolsModule::GetTsFilePathForBlueprint(UBlueprint* TargetBlueprint) const
{
	if (!TargetBlueprint) return FString();

	const FString PackageName = TargetBlueprint->GetOutermost()->GetName(); // /Game/game/bag/BP_Bag
	const FString BlueprintName = TargetBlueprint->GetName();                 // BP_Bag

	// 镜像到 TypeScript/<Content 下相对目录>/
	FString RelativeDir;
	{
		const FString ContentRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
		const FString AssetFilePath = FPackageName::LongPackageNameToFilename(
			PackageName, FPackageName::GetAssetPackageExtension());
		FString AssetDir = FPaths::GetPath(AssetFilePath);
		FPaths::MakePathRelativeTo(AssetDir, *ContentRoot);
		AssetDir.RemoveFromStart(TEXT("/"));
		AssetDir.RemoveFromEnd(TEXT("/"));
		RelativeDir = AssetDir;
	}

	const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const FString TsRootDir = FPaths::Combine(ProjectRoot, TEXT("TypeScript"));
	const FString TsDir = RelativeDir.IsEmpty()
		? TsRootDir
		: FPaths::Combine(TsRootDir, RelativeDir);

	return FPaths::Combine(TsDir, BlueprintName + TEXT(".ts"));
}

void FPuertsToolsModule::BuildTsPaths(
	UBlueprint* TargetBlueprint,
	FString& OutAssetPathForLoad,
	FString& OutTypePath) const
{
	const FString PackageName = TargetBlueprint->GetOutermost()->GetName();
	const FString BlueprintName = TargetBlueprint->GetName();

	// "/Game/game/bag/BP_Bag" -> "/Game/game/bag/BP_Bag.BP_Bag_C"
	OutAssetPathForLoad = FString::Printf(TEXT("%s.%s_C"), *PackageName, *BlueprintName);

	// "/Game/game/bag/BP_Bag" -> "Game.game.bag.BP_Bag.BP_Bag_C"
	OutTypePath = PackageName;
	OutTypePath.RemoveFromStart(TEXT("/"));
	OutTypePath.ReplaceInline(TEXT("/"), TEXT("."));
	OutTypePath += FString::Printf(TEXT(".%s_C"), *BlueprintName);
}

FString FPuertsToolsModule::BuildTsContentForBlueprint(UBlueprint* TargetBlueprint) const
{
	if (!TargetBlueprint) return FString();

	const FString BlueprintName = TargetBlueprint->GetName();

	FString AssetPathForLoad, TypePath;
	BuildTsPaths(TargetBlueprint, AssetPathForLoad, TypePath);

	const UClass* ParentClass = TargetBlueprint->ParentClass;
	const bool bIsWidget = ParentClass && ParentClass->IsChildOf(UCWidgetBase::StaticClass());
	const bool bIsActor = ParentClass && ParentClass->IsChildOf(AActor::StaticClass());

	FString ClassBodyMethods;
	if (bIsWidget)
	{
		ClassBodyMethods +=
			"    //Construct(): void\n"
			"    //{\n"
			"    //}\n\n"
			"    //Destruct(): void\n"
			"    //{\n"
			"    //}\n\n"
			"    //Tick(MyGeometry: UE.Geometry, InDeltaTime: number) : void\n"
			"    //{\n"
			"    //}\n\n"
			"    //onCreate(winArgs: UE.TArray<UE.CGameValue>): void\n"
			"    //{\n"
			"    //}\n\n"
			"    //isCreate(winArgs: UE.TArray<UE.CGameValue>): boolean\n"
			"    //{\n"
			"    //    return true;\n"
			"    //}\n\n"
			"    //onBeforeDestroy(): void\n"
			"    //{\n"
			"    //}\n";
	}
	else if (bIsActor)
	{
		ClassBodyMethods +=
			"    //BeginPlay(): void\n"
			"    //{\n"
			"    //}\n\n"
			"    //EndPlay(EndPlayReason: UE.EEndPlayReason): void\n"
			"    //{\n"
			"    //}\n\n"
			"    //Tick(DeltaSeconds: number): void\n"
			"    //{\n"
			"    //}\n";
	}

	return FString::Printf(TEXT(
		"import {blueprint, toDelegate} from 'puerts'\n"
		"import * as UE from \"ue\"\n\n"
		"// 蓝图资产路径: %s\n"
		"let BluePrintClass = UE.Class.Load(\"%s\");\n"
		"let MixerClass = blueprint.tojs<typeof UE.%s>(BluePrintClass);\n\n"
		"interface %s extends UE.%s {};\n"
		"class %s\n"
		"{\n"
		"%s"
		"}\n\n"
		"blueprint.mixin(MixerClass, %s);\n"
		"export default %s;\n"
	),
		*AssetPathForLoad,
		*AssetPathForLoad,
		*TypePath,
		*BlueprintName,
		*TypePath,
		*BlueprintName,
		*ClassBodyMethods,
		*BlueprintName,
		*BlueprintName
	);
}

void FPuertsToolsModule::WriteTsFileForBlueprint(UBlueprint* TargetBlueprint)
{
	if (!TargetBlueprint) return;

	const FString TsFilePath = GetTsFilePathForBlueprint(TargetBlueprint);
	if (TsFilePath.IsEmpty()) return;

	// 确保目录存在
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TsDir = FPaths::GetPath(TsFilePath);
	if (!PlatformFile.DirectoryExists(*TsDir))
	{
		PlatformFile.CreateDirectoryTree(*TsDir);
	}

	const FString TsContent = BuildTsContentForBlueprint(TargetBlueprint);

	if (FFileHelper::SaveStringToFile(TsContent, *TsFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		// 新增：注册到 register.ts（无分类）
		const FString ImportPath = GetTsModuleImportPath(TargetBlueprint);
		if (!AddEntryToRegisterTs(ImportPath))
		{
			UE_LOG(LogPuertsTools, Warning, TEXT("Failed to add %s to register.ts"), *ImportPath);
		}
		this->OnOpenWorkspaceClicked();
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*TsFilePath);
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(TEXT("写入 TS 文件失败。")));
	}

	this->OnRunTscClicked();
}

void FPuertsToolsModule::OpenTsFileForBlueprint(UBlueprint* TargetBlueprint)
{
	if (!TargetBlueprint) return;

	const FString TsFilePath = GetTsFilePathForBlueprint(TargetBlueprint);
	if (TsFilePath.IsEmpty()) return;

	if (!FPaths::FileExists(TsFilePath))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(FString::Printf(TEXT("TS 文件不存在：%s"), *TsFilePath)));
		return;
	}

	FPlatformProcess::LaunchFileInDefaultExternalApplication(*TsFilePath);
}

void FPuertsToolsModule::DeleteTsFileForBlueprint(UBlueprint* TargetBlueprint)
{
	if (!TargetBlueprint) return;

	const FString TsFilePath = GetTsFilePathForBlueprint(TargetBlueprint);
	if (TsFilePath.IsEmpty()) return;

	if (!FPaths::FileExists(TsFilePath))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(FString::Printf(TEXT("TS 文件不存在：%s"), *TsFilePath)));
		return;
	}

	const EAppReturnType::Type Ret = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::FromString(FString::Printf(TEXT("确定删除 TS 文件？\n%s"), *TsFilePath)));

	if (Ret == EAppReturnType::Yes)
	{
		const FString ImportPath = GetTsModuleImportPath(TargetBlueprint);
		const FString TsDir = FPaths::GetPath(TsFilePath);           // 文件所在目录
		const FString TsRootDir = FPaths::Combine(
			FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()),
			TEXT("TypeScript"));                                     // 清理截止目录

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.DeleteFile(*TsFilePath))
		{
			RemoveEntryFromRegisterTs(ImportPath);
			CleanupEmptyDirsUpTo(TsDir, TsRootDir);                  // 新增：向上删空目录
			UE_LOG(LogPuertsTools, Log, TEXT("Deleted TS file: %s"), *TsFilePath);
		}
	}
}

void FPuertsToolsModule::GenerateTsForBlueprint(UBlueprint* TargetBlueprint)
{
	// 兼容旧调用点：内部走写入逻辑
	WriteTsFileForBlueprint(TargetBlueprint);
}

FReply FPuertsToolsModule::OnOpenWorkspaceClicked()
{
	// 项目根目录
	FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FString WorkspacePath = FPaths::Combine(ProjectRoot, TEXT("client.code-workspace"));

	if (FPaths::FileExists(WorkspacePath))
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*WorkspacePath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("client.code-workspace not found at: %s"), *WorkspacePath);
	}
	return FReply::Handled();
}

// 执行 tsc 命令
FReply FPuertsToolsModule::OnRunTscClicked()
{
	FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	int32 ReturnCode = 0;
	FString StdOut, StdErr;
	FPlatformProcess::ExecProcess(TEXT("tsc.cmd"), TEXT(""), &ReturnCode, &StdOut, &StdErr, *ProjectRoot);
	UE_LOG(LogPuertsTools, Log, TEXT("tsc exit: %d, out: %s, err: %s"), ReturnCode, *StdOut, *StdErr);
	return FReply::Handled();
}

void FPuertsToolsModule::RunCommandHidden(const FString& InExe, const FString& InArgs, const FString& InWorkingDir, TFunction<void()> OnFinished)
{
	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	uint32 ProcessId = 0;
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		*InExe,
		InArgs.IsEmpty() ? nullptr : *InArgs,
		true, true, true,
		&ProcessId,
		0,
		InWorkingDir.IsEmpty() ? nullptr : *InWorkingDir,
		WritePipe,
		nullptr,
		nullptr
	);

	if (!ProcHandle.IsValid())
	{
		UE_LOG(LogPuertsTools, Warning, TEXT("[RunCommandHidden] CreateProc FAILED. Exe=%s Args=%s Dir=%s"), *InExe, *InArgs, *InWorkingDir);
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		if (OnFinished) OnFinished();
		return;
	}
	UE_LOG(LogPuertsTools, Warning, TEXT("[RunCommandHidden] Proc started. PID=%u Exe=%s"), ProcessId, *InExe);

	struct FProcWatcher
	{
		FProcHandle Handle;
		void* ReadPipe;
		void* WritePipe;
		FString OutputAccum;
		TFunction<void()> Callback;
	};
	TSharedPtr<FProcWatcher> Watcher = MakeShared<FProcWatcher>();
	Watcher->Handle = ProcHandle;
	Watcher->ReadPipe = ReadPipe;
	Watcher->WritePipe = WritePipe;
	Watcher->Callback = MoveTemp(OnFinished);

	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([Watcher](float) -> bool
			{
				if (!Watcher.IsValid())
				{
					return true;
				}

				TArray<uint8> Buffer;
				while (FPlatformProcess::ReadPipeToArray(Watcher->ReadPipe, Buffer))
				{
					if (Buffer.Num() == 0) break;
					FString Str = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData())));
					UE_LOG(LogPuertsTools, Log, TEXT("%s"), *Str);
					Buffer.Reset();
				}

				if (!FPlatformProcess::IsProcRunning(Watcher->Handle))
				{
					FString Tail = FPlatformProcess::ReadPipe(Watcher->ReadPipe);
					if (!Tail.IsEmpty())
					{
						TArray<FString> Lines;
						Tail.ParseIntoArrayLines(Lines);
						for (const FString& Line : Lines)
						{
							if (!Line.IsEmpty())
								UE_LOG(LogPuertsTools, Warning, TEXT("%s"), *Line);
						}
					}

					UE_LOG(LogPuertsTools, Warning, TEXT("[RunCommandHidden] 进程已结束。PID=%u"), Watcher->Handle.Get());

					FPlatformProcess::CloseProc(Watcher->Handle);
					FPlatformProcess::ClosePipe(Watcher->ReadPipe, Watcher->WritePipe);
					TFunction<void()> Cb = MoveTemp(Watcher->Callback);
					if (Cb) Cb();
					return false;
				}
				return true;
			}),
		0.2f
	);
}

// 在 UE 命令行执行 Puerts.Gen FULL
FReply FPuertsToolsModule::OnRunPuertsGenClicked()
{
	// 通过控制台命令执行
	if (IConsoleManager::Get().FindConsoleObject(TEXT("Puerts.Gen")))
	{
		GEngine->Exec(nullptr, TEXT("Puerts.Gen FULL"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Puerts.Gen command not found."));
	}
	return FReply::Handled();
}

void FPuertsToolsModule::OnGenerateTsFromBlueprint()
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem) return;
}

TSharedRef<SWidget> FPuertsToolsModule::BuildBlueprintTsMenu(UBlueprint* Blueprint)
{
	const FString TsFilePath = GetTsFilePathForBlueprint(Blueprint);
	const bool bHasFile = !TsFilePath.IsEmpty() && FPaths::FileExists(TsFilePath);

	FMenuBuilder MenuBuilder(true, nullptr);

	if (!bHasFile)
	{
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("生成 TS")),
			FText::FromString(TEXT("为当前蓝图生成同名的 TypeScript 文件")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
			FUIAction(FExecuteAction::CreateLambda([this, Blueprint]()
				{
					WriteTsFileForBlueprint(Blueprint);
				}))
		);
	}
	else
	{
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("打开 TS")),
			FText::FromString(TEXT("在默认编辑器中打开 TS 文件")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.OpenInExternalEditor"),
			FUIAction(FExecuteAction::CreateLambda([this, Blueprint]()
				{
					OpenTsFileForBlueprint(Blueprint);
				}))
		);

		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("覆盖 TS")),
			FText::FromString(TEXT("重新生成并覆盖 TS 文件")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
			FUIAction(FExecuteAction::CreateLambda([this, Blueprint]()
				{
					// 覆盖前确认
					const EAppReturnType::Type Ret = FMessageDialog::Open(
						EAppMsgType::YesNo,
						FText::FromString(FString::Printf(TEXT("确定覆盖 TS 文件？\n%s"), *GetTsFilePathForBlueprint(Blueprint))));
					if (Ret == EAppReturnType::Yes)
					{
						WriteTsFileForBlueprint(Blueprint);
					}
				}))
		);

		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("删除 TS")),
			FText::FromString(TEXT("删除已存在的 TS 文件")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
			FUIAction(FExecuteAction::CreateLambda([this, Blueprint]()
				{
					DeleteTsFileForBlueprint(Blueprint);
				}))
		);
	}

	return MenuBuilder.MakeWidget();
}

FString FPuertsToolsModule::GetRegisterTsFilePath() const
{
	const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	return FPaths::Combine(ProjectRoot, TEXT("TypeScript"), TEXT("register.ts"));
}

// 把蓝图对应的 TS 文件路径转成 register.ts 里的相对 import 路径（带 "./" 前缀）
// 例：/Game/game/bag/BP_Bag -> ./game/bag/BP_Bag
FString FPuertsToolsModule::GetTsModuleImportPath(UBlueprint* TargetBlueprint) const
{
	if (!TargetBlueprint) return FString();

	const FString TsFilePath = GetTsFilePathForBlueprint(TargetBlueprint);
	const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const FString TsRootDir = FPaths::Combine(ProjectRoot, TEXT("TypeScript"));

	FString RelPath = TsFilePath;
	FPaths::MakePathRelativeTo(RelPath, *TsRootDir);   // game/bag/BP_Bag.ts
	RelPath.RemoveFromEnd(TEXT(".ts"));                // game/bag/BP_Bag
	RelPath.ReplaceInline(TEXT("\\"), TEXT("/"));      // 统一斜杠
	RelPath.RemoveFromStart(TEXT("TypeScript/"));
	return TEXT("./") + RelPath;
}

FString FPuertsToolsModule::GetRegisterCategoryForBlueprint(UBlueprint* TargetBlueprint) const
{
	if (!TargetBlueprint) return TEXT("Other");

	const UClass* ParentClass = TargetBlueprint->ParentClass;
	if (ParentClass && ParentClass->IsChildOf(UCWidgetBase::StaticClass()))
	{
		return TEXT("Widget");
	}
	if (ParentClass && ParentClass->IsChildOf(AActor::StaticClass()))
	{
		return TEXT("Actor");
	}
	return TEXT("Other");
}

bool FPuertsToolsModule::SaveRegisterTsContent(const FString& Content, const FString& RegisterPath) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString Dir = FPaths::GetPath(RegisterPath);
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		PlatformFile.CreateDirectoryTree(*Dir);
	}
	return FFileHelper::SaveStringToFile(
		Content, *RegisterPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool FPuertsToolsModule::AddEntryToRegisterTs(const FString& ImportPath)
{
	const FString RegisterPath = GetRegisterTsFilePath();

	TArray<FString> Imports;

	// 读取旧内容并抓取已有路径（文件不存在就当作空）
	FString Old;
	if (FPaths::FileExists(RegisterPath))
	{
		FFileHelper::LoadFileToString(Old, *RegisterPath);
		ExtractRegisterImports(Old, Imports);
	}

	// 合并新路径（AddUnique 自带去重）
	Imports.AddUnique(ImportPath);

	const FString NewContent = BuildRegisterContent(Imports);
	return SaveRegisterTsContent(NewContent, RegisterPath);
}

bool FPuertsToolsModule::RemoveEntryFromRegisterTs(const FString& ImportPath)
{
	const FString RegisterPath = GetRegisterTsFilePath();
	if (!FPaths::FileExists(RegisterPath)) return true;

	FString Old;
	if (!FFileHelper::LoadFileToString(Old, *RegisterPath)) return false;

	TArray<FString> Imports;
	ExtractRegisterImports(Old, Imports);

	const bool bChanged = Imports.Remove(ImportPath) > 0;
	// 即使未删除，也重写一次，保证格式统一（幂等）
	const FString NewContent = BuildRegisterContent(Imports);
	return SaveRegisterTsContent(NewContent, RegisterPath);
}

// 从任意文本中抓取所有形如 "./xxx" 的带引号字符串
static void ExtractRegisterImports(const FString& Text, TArray<FString>& OutImports)
{
	const int32 Len = Text.Len();
	for (int32 i = 0; i < Len; ++i)
	{
		if (Text[i] != TEXT('"')) continue;

		const int32 Start = i + 1;
		int32 End = Start;
		while (End < Len && Text[End] != TEXT('"')) ++End;
		if (End >= Len) break;

		const FString Inside = Text.Mid(Start, End - Start);
		if (Inside.StartsWith(TEXT("./")))
		{
			OutImports.AddUnique(Inside);
		}
		i = End; // 跳过闭合引号
	}
}

// 用给定路径集合重写 register.ts 内容
static FString BuildRegisterContent(const TArray<FString>& Imports)
{
	FString Out;
	Out += TEXT("// Auto-generated by PuertsTools. Do not edit manually.\n");
	Out += TEXT("export const mods: string[] = [\n");
	for (const FString& P : Imports)
	{
		Out += FString::Printf(TEXT("    \"%s\",\n"), *P);
	}
	Out += TEXT("];\n");
	return Out;
}

void FPuertsToolsModule::CleanupEmptyDirsUpTo(const FString& StartDir, const FString& StopDir) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FString StopAbs = FPaths::ConvertRelativePathToFull(StopDir);
	FPaths::NormalizeDirectoryName(StopAbs);

	FString CurAbs = FPaths::ConvertRelativePathToFull(StartDir);
	FPaths::NormalizeDirectoryName(CurAbs);

	if (CurAbs.Len() <= StopAbs.Len() || !CurAbs.StartsWith(StopAbs))
	{
		UE_LOG(LogPuertsTools, Warning,
			TEXT("[CleanupEmptyDirsUpTo] StartDir(%s) 不在 StopDir(%s) 内，放弃清理。"),
			*CurAbs, *StopAbs);
		return;
	}

	while (CurAbs.Len() > StopAbs.Len() && CurAbs.StartsWith(StopAbs))
	{
		if (PlatformFile.DirectoryExists(*CurAbs))
		{
			// DeleteDirectory 只在目录为空时成功；非空返回 false，视为"到此为止"
			if (!PlatformFile.DeleteDirectory(*CurAbs))
			{
				break;
			}
			UE_LOG(LogPuertsTools, Log, TEXT("[CleanupEmptyDirsUpTo] 已删除空目录: %s"), *CurAbs);
		}

		CurAbs = FPaths::GetPath(CurAbs);
		FPaths::NormalizeDirectoryName(CurAbs);

		if (CurAbs.Len() <= StopAbs.Len())
		{
			break;
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPuertsToolsModule, PuertsTools)
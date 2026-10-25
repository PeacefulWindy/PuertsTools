// PuertsTools.h
#pragma once

#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FPuertsToolsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    void PluginButtonClicked();

private:
    void RegisterMenus();

    void GenerateTsForBlueprint(UBlueprint* TargetBlueprint);

    /** 生成/覆盖：直接写文件 */
    void WriteTsFileForBlueprint(UBlueprint* TargetBlueprint);

    /** 打开已存在的 TS 文件 */
    void OpenTsFileForBlueprint(UBlueprint* TargetBlueprint);

    /** 删除已存在的 TS 文件 */
    void DeleteTsFileForBlueprint(UBlueprint* TargetBlueprint);

    /** 计算蓝图对应 TS 文件路径 */
    FString GetTsFilePathForBlueprint(UBlueprint* TargetBlueprint) const;

    /** 计算生成内容 */
    FString BuildTsContentForBlueprint(UBlueprint* TargetBlueprint) const;

    /** 计算 TS 类型路径 / 资产路径 */
    void BuildTsPaths(UBlueprint* TargetBlueprint, FString& OutAssetPathForLoad, FString& OutTypePath) const;

    TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

    // 新增：打开 client.vscode-workspace
    FReply OnOpenWorkspaceClicked();

    // 新增：执行 tsc 生成命令
    FReply OnRunTscClicked();

    // 新增：在 UE 命令行执行 Puerts.Gen FULL
    FReply OnRunPuertsGenClicked();

    // 新增：在蓝图编辑器生成 TS 文件（核心逻辑）
    void OnGenerateTsFromBlueprint();

    void RunCommandHidden(const FString& InExe, const FString& InArgs, const FString& InWorkingDir, TFunction<void()> OnFinished);

    FString GetRegisterTsFilePath() const;
    FString GetTsModuleImportPath(UBlueprint* TargetBlueprint) const;
    FString GetRegisterCategoryForBlueprint(UBlueprint* TargetBlueprint) const;

    bool AddEntryToRegisterTs(const FString& ImportPath);
    bool RemoveEntryFromRegisterTs(const FString& ImportPath);

    // 内部工具
    bool SaveRegisterTsContent(const FString& Content, const FString& RegisterPath) const;

    TSharedRef<SWidget> BuildBlueprintTsMenu(UBlueprint* Blueprint);

    // 从 StartDir 开始向上删除空目录，遇到 StopDir 停止（不删 StopDir 本身）
    void CleanupEmptyDirsUpTo(const FString& StartDir, const FString& StopDir) const;

private:
    TSharedPtr<class FUICommandList> PluginCommands;
};
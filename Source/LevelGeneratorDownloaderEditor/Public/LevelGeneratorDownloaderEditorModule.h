#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FLevelGeneratorDownloaderEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OpenPluginWindow();
	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	static const FName PluginTabName;
};

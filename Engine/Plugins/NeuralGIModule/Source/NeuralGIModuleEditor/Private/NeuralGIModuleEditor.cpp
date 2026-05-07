// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralGIModuleEditor.h"

#include "FVLMData.h"
#include "PrecomputedVolumetricLightmap.h"
#include "NeuralGIModuleEditorStyle.h"
#include "NeuralGIModuleEditorCommands.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "JsonObjectConverter.h"
#include "DesktopPlatformModule.h"
#include "Misc/FileHelper.h"

static const FName NeuralGIModuleEditorTabName("NeuralGIModuleEditor");

#define LOCTEXT_NAMESPACE "FNeuralGIModuleEditorModule"

void FNeuralGIModuleEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FNeuralGIModuleEditorStyle::Initialize();
	FNeuralGIModuleEditorStyle::ReloadTextures();

	FNeuralGIModuleEditorCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FNeuralGIModuleEditorCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FNeuralGIModuleEditorModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FNeuralGIModuleEditorModule::RegisterMenus));
}

void FNeuralGIModuleEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FNeuralGIModuleEditorStyle::Shutdown();

	FNeuralGIModuleEditorCommands::Unregister();
}

void FNeuralGIModuleEditorModule::PluginButtonClicked()
{
	if (!GEditor) return;
	const UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return;
	const ULevel* Level = World->GetCurrentLevel();
	if (!Level) return;

	const FPrecomputedVolumetricLightmap* VolumetricLightmap = Level->PrecomputedVolumetricLightmap;
	if (!VolumetricLightmap)
	{
		UE_LOG(LogTemp, Warning, TEXT("The current level does not have VLM Data!"));
		return;
	}
	const FPrecomputedVolumetricLightmapData* VLMData = VolumetricLightmap->Data;
	if (!VLMData)
	{
		UE_LOG(LogTemp, Warning, TEXT("The current level does not have VLM Data!"));
		return;
	}
	
	FVLMData ExportData;
	ExportData.LevelName = FPackageName::GetShortName(World->GetOutermost()->GetName());
	ExportData.BrickSize = VLMData->BrickSize;
	ExportData.IndirectionTextureDimensions = VLMData->IndirectionTextureDimensions;
	ExportData.IndirectionTextureData = VLMData->IndirectionTexture.Data;
	ExportData.IndirectionTextureDataSize = VLMData->IndirectionTexture.DataSize;
	ExportData.BrickDataDimensions = VLMData->BrickDataDimensions;
	ExportData.AmbientVectorData = VLMData->BrickData.AmbientVector.Data;
	ExportData.AmbientVectorDataSize = VLMData->BrickData.AmbientVector.DataSize;
	ExportData.SHCoefficient0Data = VLMData->BrickData.SHCoefficients[0].Data;
	ExportData.SHCoefficient0DataSize = VLMData->BrickData.SHCoefficients[0].DataSize;
	ExportData.SHCoefficient1Data = VLMData->BrickData.SHCoefficients[1].Data;
	ExportData.SHCoefficient1DataSize = VLMData->BrickData.SHCoefficients[1].DataSize;
	ExportData.SHCoefficient2Data = VLMData->BrickData.SHCoefficients[2].Data;
	ExportData.SHCoefficient2DataSize = VLMData->BrickData.SHCoefficients[2].DataSize;
	ExportData.SHCoefficient3Data = VLMData->BrickData.SHCoefficients[3].Data;
	ExportData.SHCoefficient3DataSize = VLMData->BrickData.SHCoefficients[3].DataSize;
	ExportData.SHCoefficient4Data = VLMData->BrickData.SHCoefficients[4].Data;
	ExportData.SHCoefficient4DataSize = VLMData->BrickData.SHCoefficients[4].DataSize;
	ExportData.SHCoefficient5Data = VLMData->BrickData.SHCoefficients[5].Data;
	ExportData.SHCoefficient5DataSize = VLMData->BrickData.SHCoefficients[5].DataSize;
	
	FString JsonString;
	if (!FJsonObjectConverter::UStructToJsonObjectString(ExportData, JsonString))
	{
		UE_LOG(LogTemp, Error, TEXT("JSON serialization failed!"));
		return;
	}
	
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return;
	
	const FString DefaultFileName = FString::Printf(TEXT("VLM_%s.json"), *ExportData.LevelName);
	TArray<FString> OutFiles;
	const bool bSaved = DesktopPlatform->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		TEXT("Export VLM Data"),
		FPaths::ProjectSavedDir(),
		DefaultFileName,
		TEXT("JSON Files (*.json)|*.json"),
		0,
		OutFiles);
	
	if (!bSaved || OutFiles.Num() == 0) return;
	
	// 写入文件
	const FString& SavePath = OutFiles[0];
	if (FFileHelper::SaveStringToFile(JsonString, *SavePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Log, TEXT("VLM exported to: %s"), *SavePath);
		FString Text = FString::Printf(TEXT("Export successful! File path: %s\nIndirection Texture Dimensions: %d, %d, %d"),
				*SavePath, ExportData.IndirectionTextureDimensions.X, ExportData.IndirectionTextureDimensions.Y, ExportData.IndirectionTextureDimensions.Z);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write file: %s"), *SavePath);
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ExportFailed", "Export failed, unable to write file!"));
	}
	
}

void FNeuralGIModuleEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FNeuralGIModuleEditorCommands::Get().PluginAction, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FNeuralGIModuleEditorCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNeuralGIModuleEditorModule, NeuralGIModuleEditor)
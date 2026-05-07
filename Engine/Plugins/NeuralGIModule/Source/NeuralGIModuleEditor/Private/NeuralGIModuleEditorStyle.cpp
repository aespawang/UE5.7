// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralGIModuleEditorStyle.h"
#include "NeuralGIModuleEditor.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FNeuralGIModuleEditorStyle::StyleInstance = nullptr;

void FNeuralGIModuleEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FNeuralGIModuleEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FNeuralGIModuleEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("NeuralGIModuleEditorStyle"));
	return StyleSetName;
}


const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FNeuralGIModuleEditorStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("NeuralGIModuleEditorStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("NeuralGIModule")->GetBaseDir() / TEXT("Resources"));

	Style->Set("NeuralGIModuleEditor.PluginAction", new IMAGE_BRUSH(TEXT("Icon128"), Icon20x20));
	return Style;
}

void FNeuralGIModuleEditorStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FNeuralGIModuleEditorStyle::Get()
{
	return *StyleInstance;
}

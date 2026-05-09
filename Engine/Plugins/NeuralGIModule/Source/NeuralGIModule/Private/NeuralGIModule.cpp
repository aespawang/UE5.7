// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralGIModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FNeuralGIModuleModule"

void FNeuralGIModuleModule::StartupModule()
{
	const FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("NeuralGIModule"))->GetBaseDir(),TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/NeuralGIModule"), PluginShaderDir);
}

void FNeuralGIModuleModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNeuralGIModuleModule, NeuralGIModule)
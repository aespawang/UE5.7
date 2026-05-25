// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralGIModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "FNeuralGIModuleModule"

namespace NeuralGIModuleConsoleVariables
{
	// CustomMeshPass Shader 变体切换：0 = 采样原始 VLM Ambient，1 = 采样 NeuralGI MLP 推理结果（占位）。
	// 仅控制 PS Permutation，启用与否复用 ShowFlags.LightMapDensity（详见 CustomMeshPass_Design.md §5.6.3）。
	// 生效时机：CustomMeshPass 注册时仅设 EMeshPassFlags::MainView，未启用 CachedMeshCommands，
	//          因此 PS Permutation 在每帧 DrawCommand 收集时读取 CVar，静态网格与动态网格切换均立即生效。
	static const TCHAR* CustomMeshPassSourceName = TEXT("r.NeuralGI.CustomMeshPass.Source");
}

void FNeuralGIModuleModule::StartupModule()
{
	const FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("NeuralGIModule"))->GetBaseDir(),TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/NeuralGIModule"), PluginShaderDir);

	// 注册 CustomMeshPass Source CVar。Renderer 模块通过名字反查（FindConsoleVariable）消费，
	// 因此本插件与引擎 Renderer 模块之间无任何符号依赖。
	IConsoleManager::Get().RegisterConsoleVariable(
		NeuralGIModuleConsoleVariables::CustomMeshPassSourceName,
		0,
		TEXT("NeuralGI CustomMeshPass Shader 变体切换：\n")
		TEXT("  0 - 采样原始 VLM Ambient（默认）\n")
		TEXT("  1 - 采样 NeuralGI MLP 推理结果\n")
		TEXT("仅控制 PS Permutation，启用与否复用 ShowFlags.LightMapDensity。\n")
		TEXT("CustomMeshPass 未启用 CachedMeshCommands，静态/动态网格均立即生效。"),
		ECVF_RenderThreadSafe);
}

void FNeuralGIModuleModule::ShutdownModule()
{
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(NeuralGIModuleConsoleVariables::CustomMeshPassSourceName))
	{
		IConsoleManager::Get().UnregisterConsoleObject(Var);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNeuralGIModuleModule, NeuralGIModule)
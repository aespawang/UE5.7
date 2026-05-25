// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NeuralGIBridge.h
	NeuralGI 项目用：Renderer ←→ NeuralGIModule 反向桥。

	背景：Renderer 模块不能依赖某个具体的游戏插件，但 CustomMeshPassRendering.cpp
	      需要拿到 NeuralGIModule 持有的 VolumetricLightmapMlpTexture 用于 PS 采样。
	做法：Renderer 自己声明并定义一个全局函数指针（默认 nullptr），由
	      NeuralGIModule 在合适时机（NeuralGIWorldSubsystem::Initialize / Deinitialize）
	      赋值与清空。整个桥仅依赖公共 RHI 类型 FRHITexture*，零类型耦合、零编译依赖。

	详见 CustomMeshPass_Design.md §5.6.4。
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

namespace NeuralGIBridge
{
	/**
	 * 反向桥函数签名：返回 NeuralGI 的 MLP 3D 纹理 RHI 句柄。
	 * - 返回 nullptr 表示插件未加载 / 未初始化 / 资源未就绪；
	 * - 调用方需做空指针兜底（如使用 GBlackVolumeTexture）。
	 */
	using FGetMlpTextureFn = FRHITexture* (*)();

	/** 反向桥全局指针。定义在 CustomMeshPassRendering.cpp，由插件赋值。 */
	extern RENDERER_API FGetMlpTextureFn GGetMlpTexture;
}

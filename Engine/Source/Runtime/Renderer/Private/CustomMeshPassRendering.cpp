// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CustomMeshPassRendering.cpp:
	NeuralGI 项目用：渲染 VLM Ambient 可视化的 CustomMeshPass。
	Step1（5.1）：注册 EMeshPass::CustomMeshPass 到引擎 MeshPass 体系（已完成）。
	Step2（5.2）：FCustomMeshPassProcessor 骨架 + 过滤规则 + 注册到工厂表（已完成）。
	Step3（5.3）：FCustomMeshPassVS / FCustomMeshPassPS Shader 类、Permutation、
	             ShouldCompilePermutation 限制、IMPLEMENT_SHADER_TYPE，并接通
	             BuildMeshDrawCommands。
	Step5（5.5）：RenderCustomMeshPass —— 仿 RenderLightMapDensities 的
	             "BasePass 互斥替身"渲染入口；直接复用 BasePassRenderTargets
	             （SceneColor + Depth），不创建独立 RT、不做 Copy、不依赖任何
	             前置 Pass。由 BasePassRendering.cpp 在
	             ShowFlags.NeuralGICustomMeshPass 启用时优先调用，完全替代 BasePass。
=============================================================================*/

#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "MeshMaterialShader.h"
#include "ScenePrivate.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderGraphUtils.h"
#include "InstanceCulling/InstanceCullingContext.h"

//==============================================================================
// 5.3 Shader 声明
//------------------------------------------------------------------------------
// Permutation：
//   FSourceDim = 0  -> 采样原始 VLM Ambient
//   FSourceDim = 1  -> 采样 NeuralGI MLP 推理结果（占位实现）
//==============================================================================

namespace CustomMeshPassUtils
{
	// 仅支持 SM5 及以上
	static bool ShouldCompileMeshPermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
		{
			return false;
		}

		const FMaterialShaderParameters& MaterialParameters = Parameters.MaterialParameters;

		// 仅 Surface 域；UI / PostProcess / Decal 等不参与
		if (MaterialParameters.MaterialDomain != MD_Surface)
		{
			return false;
		}

		// 仅 Opaque / Masked
		if (IsTranslucentBlendMode(MaterialParameters))
		{
			return false;
		}

		// 仅 Lit
		if (!MaterialParameters.ShadingModels.IsLit())
		{
			return false;
		}

		return true;
	}
}

/**
 * Vertex Shader：仅完成顶点变换 + 透传到 PS。
 * VLM 采样在 PS 中按 World Position 进行（精度更稳）。
 */
class FCustomMeshPassVS : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FCustomMeshPassVS, MeshMaterial);

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return CustomMeshPassUtils::ShouldCompileMeshPermutation(Parameters);
	}
	
	FCustomMeshPassVS() = default;

	explicit FCustomMeshPassVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}
};

/**
 * Pixel Shader：采样 VLM Ambient（或 MLP 推理结果），输出 RGB。
 * 提供 FSourceDim Permutation，便于一键切换到 NeuralGI MLP 数据源。
 */
class FCustomMeshPassPS : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FCustomMeshPassPS, MeshMaterial);

	// 与 USF 中的 CUSTOM_MESH_PASS_SOURCE_* 对应
	class FSourceDim : SHADER_PERMUTATION_INT("CUSTOM_MESH_PASS_SOURCE", 2);
	using FPermutationDomain = TShaderPermutationDomain<FSourceDim>;

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return FCustomMeshPassVS::ShouldCompilePermutation(Parameters);
	}
	
	FCustomMeshPassPS() = default;

	explicit FCustomMeshPassPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}
};

IMPLEMENT_SHADER_TYPE(, FCustomMeshPassVS, TEXT("/Engine/Private/CustomMeshPass.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FCustomMeshPassPS, TEXT("/Engine/Private/CustomMeshPass.usf"), TEXT("MainPS"), SF_Pixel);


//==============================================================================
// 5.2 / 5.3 Mesh Processor
//==============================================================================

/**
 * FCustomMeshPassProcessor
 * --------------------------------------------------
 * 用于 NeuralGI 的 VLM Ambient 可视化。
 *  - 仅收集 Opaque / Masked、Lit 的 Surface 材质 Mesh
 *  - 与 BasePass 一致：依赖前序 DepthPass 的 Depth（CF_Equal、关闭 DepthWrite）
 */
class FCustomMeshPassProcessor : public FMeshPassProcessor
{
public:
	FCustomMeshPassProcessor(
		const FScene* InScene,
		ERHIFeatureLevel::Type InFeatureLevel,
		const FSceneView* InViewIfDynamicMeshCommand,
		FMeshPassDrawListContext* InDrawListContext)
		: FMeshPassProcessor(EMeshPass::CustomMeshPass, InScene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	{
		// Opaque blend，写颜色，不写深度；与 BasePass 一致，深度测试走 CF_Equal、复用前序 DepthPass 结果。
		PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
		PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Equal>::GetRHI());
	}

	virtual void AddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId = -1) override final;

private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material);

	bool Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	FMeshPassProcessorRenderState PassDrawRenderState;
};

void FCustomMeshPassProcessor::AddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId /*= -1*/)
{
	if (FeatureLevel < ERHIFeatureLevel::SM5 || !MeshBatch.bUseForMaterial)
	{
		return;
	}

	const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
	while (MaterialRenderProxy)
	{
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
		if (Material)
		{
			if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
			{
				break;
			}
		}
		MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
	}
}

bool FCustomMeshPassProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material)
{
	// ---- 过滤规则（与 ShouldCompileMeshPermutation 保持一致）----
	if (IsTranslucentBlendMode(Material))
	{
		return false;
	}
	if (!Material.GetShadingModels().IsLit())
	{
		return false;
	}
	if (!ShouldIncludeMaterialInDefaultOpaquePass(Material))
	{
		return false;
	}

	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

	return Process(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		StaticMeshId,
		MaterialRenderProxy,
		Material,
		MeshFillMode,
		MeshCullMode);
}

bool FCustomMeshPassProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;
	FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

	// ---- 拉取 Shader ----
	// 一期固定使用 SourceDim = 0（Raw VLM Ambient）。
	// 后续切换 MLP 推理路径时，可在 Pass 渲染入口处根据 CVar 决定 PermutationVector。
	FCustomMeshPassPS::FPermutationDomain PSPermutationVector;
	PSPermutationVector.Set<FCustomMeshPassPS::FSourceDim>(0);

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FCustomMeshPassVS>();
	ShaderTypes.AddShaderType<FCustomMeshPassPS>(PSPermutationVector.ToDimensionValueId());

	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	TMeshProcessorShaders<FCustomMeshPassVS, FCustomMeshPassPS> CustomMeshPassShaders;
	Shaders.TryGetVertexShader(CustomMeshPassShaders.VertexShader);
	Shaders.TryGetPixelShader(CustomMeshPassShaders.PixelShader);

	if (!CustomMeshPassShaders.VertexShader.IsValid() || !CustomMeshPassShaders.PixelShader.IsValid())
	{
		return false;
	}

	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(
		CustomMeshPassShaders.VertexShader, CustomMeshPassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		CustomMeshPassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);

	return true;
}

// ---- 工厂 + 注册 ----
FMeshPassProcessor* CreateCustomMeshPassProcessor(
	ERHIFeatureLevel::Type FeatureLevel,
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext)
{
	return new FCustomMeshPassProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(
	CustomMeshPass,
	CreateCustomMeshPassProcessor,
	EShadingPath::Deferred,
	EMeshPass::CustomMeshPass,
	EMeshPassFlags::MainView);


//==============================================================================
// 5.5 渲染调度（方案 B —— BasePass 互斥替身）
//------------------------------------------------------------------------------
// 设计要点（详见 CustomMeshPass_Design.md §5.5）：
//   1) 函数签名与多 View 循环结构与 RenderLightMapDensities 完全对齐；
//   2) 不创建独立 RT —— 直接绑外部传入的 BasePassRenderTargets，写入 SceneColor；
//   3) 不做 Copy —— 已直接写入 SceneColor；
//   4) 不依赖任何前置 Pass —— VLM 与 Depth 在进入本函数时均已就绪：
//        · VLM 3D Texture 在 FViewInfo::SetupUniformBufferParameters 阶段
//          经 OrBlack3DIfNull 绑入 ViewUniformShaderParameters；
//        · Depth 由前序 DepthPass 写入，DepthStencilState = <false, CF_Equal>。
//==============================================================================

BEGIN_SHADER_PARAMETER_STRUCT(FCustomMeshPassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void RenderCustomMeshPass(
	FRDGBuilder& GraphBuilder,
	TArrayView<const FViewInfo> Views,
	const FRenderTargetBindingSlots& RenderTargets)
{
	RDG_EVENT_SCOPE(GraphBuilder, "NeuralGI.CustomMeshPass");

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = const_cast<FViewInfo&>(Views[ViewIndex]);

		FParallelMeshDrawCommandPass* Pass = View.ParallelMeshDrawCommandPasses[EMeshPass::CustomMeshPass];
		if (!Pass)
		{
			continue;
		}

		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);
		View.BeginRenderView();

		auto* PassParameters = GraphBuilder.AllocParameters<FCustomMeshPassParameters>();
		PassParameters->View          = View.GetShaderParameters();
		PassParameters->RenderTargets = RenderTargets;   // 直接复用 BasePass 的 RT 绑定（SceneColor + Depth）

		FScene* Scene = View.Family->Scene->GetRenderScene();
		check(Scene != nullptr);
		Pass->BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

		GraphBuilder.AddPass(
			{},
			PassParameters,
			ERDGPassFlags::Raster,
			[&View, Pass, PassParameters](FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0,
				                       View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);
				Pass->Draw(RHICmdList, &PassParameters->InstanceCullingDrawParams);
			});
	}
}

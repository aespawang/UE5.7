#include "NeuralGIModuleViewExtension.h"
#include "Engine/World.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogNeuralGI, Log, All);

class FMlpInferCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMlpInferCS);
	SHADER_USE_PARAMETER_STRUCT(FMlpInferCS, FGlobalShader);

	// 与 USF 中的 uniform 名字一一对应（大小写完全一致）
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<float>, InVolumetricLightmapMLPBuffer)
		SHADER_PARAMETER_UAV(RWTexture3D<float4>,    OutVolumetricLightmapMLPTexture)
		SHADER_PARAMETER(FIntVector,                  Dimensions)
	END_SHADER_PARAMETER_STRUCT()
	
};

IMPLEMENT_GLOBAL_SHADER(FMlpInferCS, "/Plugin/NeuralGIModule/MlpInfer.usf", "MainCS", SF_Compute);

FNeuralGIModuleViewExtension::FNeuralGIModuleViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
	, Dimensions(32, 32, 32)
{
	UE_LOG(LogNeuralGI, Log, TEXT("FNeuralGIModuleViewExtension created"));
}

FNeuralGIModuleViewExtension::~FNeuralGIModuleViewExtension()
{
	ReleaseResources();
	UE_LOG(LogNeuralGI, Log, TEXT("FNeuralGIModuleViewExtension destroyed"));
}

void FNeuralGIModuleViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	
}

void FNeuralGIModuleViewExtension::PreRenderBasePass_RenderThread(FRDGBuilder& GraphBuilder, bool bDepthBufferIsPopulated)
{
	DispatchMlPInferCS_RenderThread(GraphBuilder, bDepthBufferIsPopulated);
}

void FNeuralGIModuleViewExtension::InitResources(TResourceArray<float>& DataBufferCPU)
{
	if (DataBufferCPU.Num()== 0) return;
	ENQUEUE_RENDER_COMMAND(InitNeuralGIModuleResources)
	(
		[this, DataBufferCPU](FRHICommandListImmediate& RHICmdList) mutable
		{
				{
					const FRHIBufferCreateDesc MlpBufferDesc =
					   FRHIBufferCreateDesc::CreateStructured(TEXT("VolumetricLightmapMLPBuffer"), DataBufferCPU.GetResourceDataSize(), sizeof(float))
					   .AddUsage(EBufferUsageFlags::ShaderResource | EBufferUsageFlags::Static)
					   .SetInitialState(ERHIAccess::SRVMask)
					   .SetInitActionResourceArray(&DataBufferCPU);

					   VolumetricLightmapMlpBuffer.SafeRelease();
					   VolumetricLightmapMlpSRV.SafeRelease();
					   VolumetricLightmapMlpBuffer = RHICmdList.CreateBuffer(MlpBufferDesc);
					   VolumetricLightmapMlpSRV = RHICmdList.CreateShaderResourceView(VolumetricLightmapMlpBuffer, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(VolumetricLightmapMlpBuffer));
				}
				{
					const FRHITextureCreateDesc TexDesc =
						FRHITextureCreateDesc::Create3D(TEXT("VolumetricLightmapMLPTexture"),Dimensions.X, Dimensions.Y, Dimensions.Z,PF_A32B32G32R32F)
						.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV)
						.SetInitialState(ERHIAccess::UAVCompute);
					    VolumetricLightmapMlpTexture.SafeRelease();
					    VolumetricLightmapMlpTextureUAV.SafeRelease();
						VolumetricLightmapMlpTexture = RHICmdList.CreateTexture(TexDesc);
						VolumetricLightmapMlpTextureUAV = RHICmdList.CreateUnorderedAccessView(VolumetricLightmapMlpTexture,FRHIViewDesc::CreateTextureUAV().SetDimensionFromTexture(VolumetricLightmapMlpTexture));
				}
		}
	);
}

void FNeuralGIModuleViewExtension::DispatchMlPInferCS_RenderThread(FRDGBuilder& GraphBuilder, bool bDepthBufferIsPopulated) const
{
	if (!VolumetricLightmapMlpSRV.IsValid() || !VolumetricLightmapMlpTextureUAV.IsValid()) return;
	const TShaderMapRef<FMlpInferCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FMlpInferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMlpInferCS::FParameters>();
	PassParameters->InVolumetricLightmapMLPBuffer = VolumetricLightmapMlpSRV;
	PassParameters->OutVolumetricLightmapMLPTexture = VolumetricLightmapMlpTextureUAV;
	PassParameters->Dimensions = Dimensions;

	FRHITexture* TexRHI = VolumetricLightmapMlpTexture.GetReference();

	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(Dimensions, FIntVector(8,8,8));
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("MlpInferCS"),
		PassParameters,
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		[PassParameters, ComputeShader, GroupCount, TexRHI](FRHICommandList& RHICmdList)
		{
			RHICmdList.Transition(FRHITransitionInfo(TexRHI, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

			SetComputePipelineState(RHICmdList, ComputeShader.GetComputeShader());
			SetShaderParameters(RHICmdList, ComputeShader, ComputeShader.GetComputeShader(), *PassParameters);
			RHICmdList.DispatchComputeShader(GroupCount.X, GroupCount.Y, GroupCount.Z);
			UnsetShaderUAVs(RHICmdList, ComputeShader, ComputeShader.GetComputeShader());

			RHICmdList.Transition(FRHITransitionInfo(TexRHI, ERHIAccess::UAVCompute, ERHIAccess::SRVMask));
		}
	);
}


void FNeuralGIModuleViewExtension::ReleaseResources()
{
	VolumetricLightmapMlpBuffer.SafeRelease();
	VolumetricLightmapMlpSRV.SafeRelease();
	VolumetricLightmapMlpTexture.SafeRelease();
	VolumetricLightmapMlpTextureUAV.SafeRelease();
}

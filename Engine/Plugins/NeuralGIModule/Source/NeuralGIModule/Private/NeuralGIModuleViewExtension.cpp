#include "NeuralGIModuleViewExtension.h"

#include "Engine/World.h"
#include "SceneViewExtensionContext.h"
#include "RenderGraphBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogNeuralGI, Log, All);

FNeuralGIModuleViewExtension::FNeuralGIModuleViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld)
	: FWorldSceneViewExtension(AutoRegister, InWorld)
	, Dimensions(32, 32, 32)
{
	UE_LOG(LogNeuralGI, Log, TEXT("FNeuralGIModuleViewExtension created for World=%s"),
		*GetNameSafe(InWorld));
}

FNeuralGIModuleViewExtension::~FNeuralGIModuleViewExtension()
{
	UE_LOG(LogNeuralGI, Log, TEXT("FNeuralGIModuleViewExtension destroyed"));
}


void FNeuralGIModuleViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	
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

void FNeuralGIModuleViewExtension::ReleaseResources()
{
	
}

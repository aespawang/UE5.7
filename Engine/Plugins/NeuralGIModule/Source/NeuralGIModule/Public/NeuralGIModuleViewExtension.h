#pragma once
#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class FNeuralGIModuleViewExtension final : public FSceneViewExtensionBase
{
public:
	explicit FNeuralGIModuleViewExtension(const FAutoRegister& AutoRegister);
	virtual ~FNeuralGIModuleViewExtension() override;

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void PreRenderBasePass_RenderThread(FRDGBuilder& GraphBuilder, bool bDepthBufferIsPopulated) override;
	
	void InitResources(TResourceArray<float>& DataBufferCPU);

	/**
	 * 5.6.4 反向桥用：返回 MLP 3D 纹理的 RHI 句柄供 Renderer 端 CustomMeshPass 采样。
	 * 资源未初始化时返回 nullptr，调用方需做空指针兜底。
	 */
	FRHITexture* GetMlpTextureRHI() const { return VolumetricLightmapMlpTexture.GetReference(); }

private:
	void DispatchMlPInferCS_RenderThread(FRDGBuilder& GraphBuilder, bool bDepthBufferIsPopulated) const;
	FBufferRHIRef VolumetricLightmapMlpBuffer;
	FShaderResourceViewRHIRef VolumetricLightmapMlpSRV;
	FTextureRHIRef VolumetricLightmapMlpTexture;
	FUnorderedAccessViewRHIRef VolumetricLightmapMlpTextureUAV;
	FIntVector Dimensions;

	void ReleaseResources();
	
};
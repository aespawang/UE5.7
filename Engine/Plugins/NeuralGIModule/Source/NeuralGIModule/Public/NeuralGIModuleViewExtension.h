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

private:
	void DispatchMlPInferCS_RenderThread(FRDGBuilder& GraphBuilder, bool bDepthBufferIsPopulated) const;
	FBufferRHIRef VolumetricLightmapMlpBuffer;
	FShaderResourceViewRHIRef VolumetricLightmapMlpSRV;
	FTextureRHIRef VolumetricLightmapMlpTexture;
	FUnorderedAccessViewRHIRef VolumetricLightmapMlpTextureUAV;
	FIntVector Dimensions;

	void ReleaseResources();
	
};
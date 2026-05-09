#pragma once
#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class FNeuralGIModuleViewExtension final : public FSceneViewExtensionBase
{
public:
	explicit FNeuralGIModuleViewExtension(const FAutoRegister& AutoRegister);
	virtual ~FNeuralGIModuleViewExtension() override;

	//~ Begin ISceneViewExtension Interface
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	//~ End ISceneViewExtension Interface
	
	void InitResources(TResourceArray<float>& DataBufferCPU);

private:
	
	FBufferRHIRef VolumetricLightmapMlpBuffer;
	FShaderResourceViewRHIRef VolumetricLightmapMlpSRV;
	FTextureRHIRef VolumetricLightmapMlpTexture;
	FUnorderedAccessViewRHIRef VolumetricLightmapMlpTextureUAV;
	FIntVector Dimensions;

	void ReleaseResources();
	
	
};
#pragma once
#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class FNeuralGIModuleViewExtension final : public FWorldSceneViewExtension
{
public:
	explicit FNeuralGIModuleViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld);
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
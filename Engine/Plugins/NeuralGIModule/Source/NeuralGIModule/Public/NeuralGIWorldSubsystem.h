#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NeuralGIWorldSubsystem.generated.h"

class FNeuralGIModuleViewExtension;

UCLASS()
class UNeuralGIWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem Interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem Interface

private:
	/** 持有 VE 的强引用；引擎 FSceneViewExtensions 仅 weak 持有，这里归零后自动摘除。 */
	TSharedPtr<FNeuralGIModuleViewExtension, ESPMode::ThreadSafe> ViewExtension;
};

#include "NeuralGIWorldSubsystem.h"

#include "NeuralGIModuleViewExtension.h"

#include "Engine/World.h"
#include "SceneViewExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogNeuralGISubsystem, Log, All);

void LoadBinData(const FString& FilePath, TResourceArray<float>& DataBufferCPU)
{
	const TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*FilePath));
	if (FileReader)
	{
		const int64 FileSize = FileReader->TotalSize();
		const int32 NumFloats = FileSize / sizeof(float);

		if (NumFloats > 0)
		{
			DataBufferCPU.Empty(NumFloats);
			DataBufferCPU.AddUninitialized(NumFloats);
			FileReader->Serialize(DataBufferCPU.GetData(), FileSize);
		}
		FileReader->Close();
		UE_LOG(LogNeuralGISubsystem, Log, TEXT("NeuralGI: Loaded %d floats from %s"), NumFloats, *FilePath);
	}
	else
	{
		UE_LOG(LogNeuralGISubsystem, Error, TEXT("NeuralGI: Failed to load %s"), *FilePath);
	}
}

bool UNeuralGIWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	
	return true;
}

bool UNeuralGIWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	switch (WorldType)
	{
	case EWorldType::Game:
	case EWorldType::PIE:
	case EWorldType::Editor:
		return true;
	default:
		return false;
	}
}

void UNeuralGIWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ViewExtension = FSceneViewExtensions::NewExtension<FNeuralGIModuleViewExtension>();
	TResourceArray<float> DataBufferCPU;
	{
		const UWorld* World = GetWorld();
		if (!World) return;

		const FString CurrentPackageName = World->GetOutermost()->GetName();
		const FString CleanPackageName = UWorld::RemovePIEPrefix(CurrentPackageName);
		const FString MapAbsPath = FPackageName::LongPackageNameToFilename(CleanPackageName, FPackageName::GetMapPackageExtension());
		FString MapDir = FPaths::GetPath(MapAbsPath);
		const FString LevelName = FPackageName::GetShortName(CleanPackageName);
		const FString BinFilePath = FPaths::Combine(MapDir, LevelName + TEXT("_VLM-MLP.bin"));
		LoadBinData(BinFilePath, DataBufferCPU);
	}
	ViewExtension->InitResources(DataBufferCPU);
	UE_LOG(LogNeuralGISubsystem, Log, TEXT("NeuralGI: Subsystem initialized!"));
}

void UNeuralGIWorldSubsystem::Deinitialize()
{
	Super::Deinitialize();
	UE_LOG(LogNeuralGISubsystem, Log, TEXT("NeuralGI: Subsystem deinitialize!"));
}

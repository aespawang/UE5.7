#include "NeuralGIWorldSubsystem.h"

#include "NeuralGIModuleViewExtension.h"

#include "Engine/World.h"
#include "SceneViewExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogNeuralGISubsystem, Log, All);

//==============================================================================
// 5.6.4 反向桥全局弱引用
//------------------------------------------------------------------------------
// lambda 不能捕获 Subsystem 的 this（Subsystem 被 GC、跟随世界销毁，生命周期与
// Renderer 调用期不一致）。改用进程级 weak 指针持有 ViewExtension，
// 仅在 Initialize/Deinitialize 中赋值与重置。多世界（同帧多 Subsystem 实例）场景
// 下，仅“最后一个初始化”的 Subsystem 会被 Renderer 看到——与当前项目需求一致。
//==============================================================================
static TWeakPtr<FNeuralGIModuleViewExtension, ESPMode::ThreadSafe> GViewExtensionWeak;

//==============================================================================
// 5.6.4 反向桥：声明 Renderer 模块定义的全局函数指针
//------------------------------------------------------------------------------
// 与 Engine/Source/Runtime/Renderer/Private/NeuralGIBridge.h 中 extern 声明、
// CustomMeshPassRendering.cpp 中定义的全局变量同名同签名。
// 通过 RENDERER_API 链接到 Renderer.dll 的导出符号；插件不持有该变量的所有权，
// 仅在 Subsystem 生命周期内对其赋值/清空。
// 使用 FRHITexture* 这一公共 RHI 类型作为接口，避免任何头文件耦合。
//==============================================================================
namespace NeuralGIBridge
{
	using FGetMlpTextureFn = FRHITexture* (*)();
	extern RENDERER_API FGetMlpTextureFn GGetMlpTexture;
}

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
		UE_LOG(LogNeuralGISubsystem, Log, TEXT("NeuralGI: BinFilePath = %s"), *BinFilePath);
		LoadBinData(BinFilePath, DataBufferCPU);
	}
	ViewExtension->InitResources(DataBufferCPU);

	// 5.6.4：注册反向桥，让 Renderer 模块的 CustomMeshPass 能拿到本 Subsystem 持有的 MLP 3D 纹理。
	// 使用弱引用避免延长 ViewExtension 生命周期；Deinitialize 中清空函数指针。
	NeuralGIBridge::GGetMlpTexture = []() -> FRHITexture*
	{
		if (TSharedPtr<FNeuralGIModuleViewExtension, ESPMode::ThreadSafe> Pinned = GViewExtensionWeak.Pin())
		{
			return Pinned->GetMlpTextureRHI();
		}
		return nullptr;
	};
	GViewExtensionWeak = ViewExtension;

	UE_LOG(LogNeuralGISubsystem, Log, TEXT("NeuralGI: Subsystem initialized!"));
}

void UNeuralGIWorldSubsystem::Deinitialize()
{
	// 5.6.4：先清空函数指针，避免 Renderer 后续调到 dangling lambda（lambda 本身不持有资源，
	//        但 GViewExtensionWeak 决定了它能返回什么。这里取消注册是额外一道保险）。
	NeuralGIBridge::GGetMlpTexture = nullptr;
	GViewExtensionWeak.Reset();

	Super::Deinitialize();
	UE_LOG(LogNeuralGISubsystem, Log, TEXT("NeuralGI: Subsystem deinitialize!"));
}

# Volumetric Lightmap 构建与调试 —— 全流程图解

> 本文档以**流程图**和**类图**为核心，直观展示从点击 Build Light Only 到最终渲染的完整链路。

---

## 一、全局总览流程图

```mermaid
flowchart LR
    subgraph Phase1["🖱️ 阶段一：UI触发"]
        A1["点击 Build Light Only"] --> A2["BuildLightingOnly_Execute()"]
        A2 --> A3["EditorBuild()"]
        A3 --> A4["BuildLighting()"]
    end

    subgraph Phase2["⚙️ 阶段二：系统初始化"]
        B1["CreateStaticLightingSystem()"] --> B2["BeginLightmassProcess()"]
        B2 --> B3["CreateLightmassProcessor()"]
        B3 --> B4["GatherScene()"]
        B4 --> B5["InitiateLightmassProcessor()"]
    end

    subgraph Phase3["📦 阶段三：数据导出"]
        C1["WriteToChannel()"] --> C2["WriteVolumetricLightmapData()"]
        C1 --> C3["WriteLights()"]
        C1 --> C4["WriteModels/Meshes/Landscapes()"]
    end

    subgraph Phase4["🚀 阶段四：Swarm调度"]
        D1["KickoffSwarm()"] --> D2["BeginRun()"]
        D2 --> D3["BeginJobSpecification()"]
        D3 --> D4["AddTask × N"]
        D4 --> D5["EndJobSpecification()"]
        D5 --> D6["启动 UnrealLightmass.exe"]
    end

    subgraph Phase5["🔥 阶段五：Lightmass计算"]
        E1["BuildStaticLighting()"] --> E2["ImportScene()"]
        E2 --> E3["CalculateAdaptiveVolumetricLightmap()"]
        E3 --> E4["RecursivelyBuildBrickTree()"]
        E4 --> E5["多线程 ProcessBrickTask()"]
        E5 --> E6["ExportResults()"]
    end

    subgraph Phase6["📥 阶段六：结果导入"]
        F1["Update() 轮询"] --> F2["ImportIrradianceTasks()"]
        F2 --> F3["FinishLightmassProcess()"]
        F3 --> F4["ImportVolumetricLightmap()"]
        F4 --> F5["ApplyNewLightingData()"]
        F5 --> F6["AddToScene()"]
    end

    subgraph Phase7["🎨 阶段七：渲染可视化"]
        G1["FPrecomputedVolumetricLightmap"] --> G2["SetData() 渲染线程"]
        G2 --> G3["AddToSceneData() GPU"]
        G3 --> G4["VisualizeVolumetricLightmap()"]
    end

    Phase1 --> Phase2 --> Phase3 --> Phase4 --> Phase5 --> Phase6 --> Phase7
```

---

## 二、阶段一 & 二：从按钮到系统初始化

```mermaid
flowchart TD
    click["🖱️ 用户点击<br/>Build → Build Lighting Only"]
    click --> exec["FLevelEditorActionCallbacks<br/>::BuildLightingOnly_Execute()"]
    exec --> build["FEditorBuildUtils::EditorBuild()"]
    build --> lighting["UEditorEngine::BuildLighting()"]
    lighting --> manager["FStaticLightingManager<br/>::CreateStaticLightingSystem()"]

    manager --> sls["new FStaticLightingSystem()"]
    sls --> begin["BeginLightmassProcess()"]

    begin --> step1["① CreateLightmassProcessor()"]
    begin --> step2["② GatherScene()"]
    begin --> step3["③ InitiateLightmassProcessor()"]
    begin --> step4["④ KickoffSwarm()"]

    step1 --> init_swarm["FSwarmInterface::Initialize()"]
    step1 --> new_lmp["new FLightmassProcessor()"]
    new_lmp --> open_conn["Swarm.OpenConnection()"]
    new_lmp --> new_exp["new FLightmassExporter()"]

    step2 --> gather_lights["收集所有光源<br/>方向光/点光/聚光灯/矩形光"]
    step2 --> gather_geo["收集几何体<br/>StaticMesh/BSP/Landscape"]
    step2 --> gather_vlm["收集 VolumetricLightmap<br/>DensityVolume"]
    step2 --> gather_imp["收集 Importance Volume"]

    step3 --> write["FLightmassExporter<br/>::WriteToChannel()"]

    style click fill:#4CAF50,color:#fff
    style begin fill:#2196F3,color:#fff
    style step1 fill:#FF9800,color:#fff
    style step2 fill:#FF9800,color:#fff
    style step3 fill:#FF9800,color:#fff
    style step4 fill:#FF9800,color:#fff
```

**关键文件位置：**
| 类 | 文件 |
|---|---|
| `FLevelEditorActionCallbacks` | `Editor/LevelEditor/Private/LevelEditorActions.cpp` |
| `FEditorBuildUtils` | `Editor/UnrealEd/Private/EditorBuildUtils.cpp` |
| `UEditorEngine::BuildLighting` | `Editor/UnrealEd/Private/StaticLightingSystem/StaticLightingSystem.cpp` |
| `FStaticLightingSystem` | 同上 |
| `FLightmassProcessor` | `Editor/UnrealEd/Private/Lightmass/Lightmass.cpp` |
| `FLightmassExporter` | `Editor/UnrealEd/Private/Lightmass/LightmassExporter.cpp` |

---

## 三、阶段三：数据导出详细流程

```mermaid
flowchart TD
    write["FLightmassExporter::WriteToChannel()"]
    write --> w1["WriteSceneSettings()"]
    write --> w2["WriteLights()"]
    write --> w3["WriteModels()"]
    write --> w4["WriteStaticMeshes()"]
    write --> w5["WriteLandscapes()"]
    write --> w6["WriteVolumetricLightmapData() ⭐"]

    w6 --> vlm1["写入世界空间范围 Bounds"]
    w6 --> vlm2["写入砖块大小 BrickSize"]
    w6 --> vlm3["写入细分层级信息"]
    w6 --> vlm4["写入 DensityVolume 位置/范围"]
    w6 --> vlm5["写入任务GUID列表<br/>VolumetricLightmapTaskGuids"]

    vlm1 --> swarm_ch["Swarm.WriteChannel()"]
    vlm2 --> swarm_ch
    vlm3 --> swarm_ch
    vlm4 --> swarm_ch
    vlm5 --> swarm_ch

    swarm_ch --> cache["Swarm缓存目录<br/>（供Lightmass读取）"]

    style w6 fill:#E91E63,color:#fff
    style swarm_ch fill:#9C27B0,color:#fff
```

---

## 四、阶段四：Swarm任务调度流程

```mermaid
flowchart TD
    kick["FStaticLightingSystem::KickoffSwarm()"]
    kick --> beginrun["FLightmassProcessor::BeginRun()"]

    beginrun --> config["配置 UnrealLightmass.exe<br/>路径和依赖文件"]
    config --> jobspec["Swarm.BeginJobSpecification()<br/>JobSpec32 / JobSpec64"]

    jobspec --> tasks["遍历 VolumetricLightmapTaskGuids"]
    tasks --> addtask["Swarm.AddTask(<br/>FTaskSpecification{<br/>  TaskGuid,<br/>  'VolumetricLightmap',<br/>  ...})"]
    addtask --> |"× N 个任务"| addtask

    jobspec --> other_tasks["添加其他任务<br/>BSPMapping / SMTextureMapping<br/>/ LandscapeMapping"]

    addtask --> endspec["Swarm.EndJobSpecification()"]
    other_tasks --> endspec

    endspec --> |"Swarm自动启动"| lightmass["🔥 UnrealLightmass.exe"]

    endspec --> async["CurrentBuildStage =<br/>AsynchronousBuilding"]
    async --> loop["编辑器主循环轮询"]
    loop --> update["FLightmassProcessor::Update()"]
    update --> |"检查回调消息"| import_inc["ImportIrradianceTasks()<br/>增量导入砖块数据"]
    import_inc --> loop

    style kick fill:#4CAF50,color:#fff
    style lightmass fill:#F44336,color:#fff
    style loop fill:#607D8B,color:#fff
```

> **Debug Mode 分支**：如果 `bDebugMode=True`，则 `BeginRun()` 设置 `JOB_FLAG_MANUAL_START`，Swarm **不会**自动启动 Lightmass，需要手动从 VS 启动。

---

## 五、阶段五：Lightmass 计算核心流程

```mermaid
flowchart TD
    entry["BuildStaticLighting()<br/>📍 CPUSolver.cpp"]
    entry --> init["初始化 Swarm 连接<br/>new FLightmassSwarm()"]
    init --> import["FLightmassImporter::ImportScene()<br/>从 Swarm 通道读取场景数据"]
    import --> exporter["new FLightmassSolverExporter()"]
    exporter --> sls["new FStaticLightingSystem()<br/>（构造函数中启动计算）"]

    sls --> calc["CalculateAdaptiveVolumetricLightmap()"]

    calc --> build_tree["RecursivelyBuildBrickTree()"]

    build_tree --> tree_init["根据 Importance Volume<br/>确定整体范围"]
    tree_init --> tree_check{"每个体素是否<br/>需要更高精度？"}
    tree_check --> |"是"| tree_subdivide["递归创建子砖块"]
    tree_subdivide --> tree_check
    tree_check --> |"否（叶子节点）"| tree_task["生成 FIrradianceBrickBuildData"]

    tree_task --> queue["放入任务队列<br/>VolumetricLightmapBrickTasks"]

    queue --> threads["ThreadLoop() × NumThreads"]
    threads --> pick["ProcessVolumetricLightmapTaskIfAvailable()"]
    pick --> process["ProcessVolumetricLightmapBrickTask()"]

    process --> p1["计算采样点世界坐标"]
    p1 --> p2["对每个光源做光线追踪<br/>（可见性检测）"]
    p2 --> p3["计算直接光照 + 间接光照<br/>（光子映射 / Final Gather）"]
    p3 --> p4["编码为三阶球谐 SH3"]
    p4 --> p5["计算天空遮蔽 Sky Occlusion"]
    p5 --> p6["存储到 FVolumetricLightmapTaskData"]

    p6 --> export["FLightmassSolverExporter<br/>::ExportResults()"]
    export --> complete["Swarm.TaskCompleted(TaskGuid)<br/>通知编辑器"]

    style entry fill:#F44336,color:#fff
    style calc fill:#E91E63,color:#fff
    style build_tree fill:#9C27B0,color:#fff
    style process fill:#FF5722,color:#fff
    style complete fill:#4CAF50,color:#fff
```

**关键文件位置：**
| 类/函数 | 文件 |
|---|---|
| `BuildStaticLighting()` | `Programs/UnrealLightmass/Private/CPUSolver/CPUSolver.cpp` |
| `CalculateAdaptiveVolumetricLightmap()` | `Programs/UnrealLightmass/Private/Lighting/AdaptiveVolumetricLightmap.cpp` |
| `RecursivelyBuildBrickTree()` | 同上 |
| `ProcessVolumetricLightmapBrickTask()` | 同上 |
| `bDebugVolumetricLightmapCell` | 同上（调试开关变量） |

---

## 六、阶段六：结果导入与应用流程

```mermaid
flowchart TD
    finish["FStaticLightingSystem<br/>::FinishLightmassProcess()"]

    finish --> invalidate["InvalidateStaticLighting()<br/>使旧光照数据失效"]
    invalidate --> complete["LightmassProcessor->CompleteRun()"]
    complete --> import_vlm["ImportVolumetricLightmap() ⭐"]

    import_vlm --> read["从 Swarm 通道<br/>读取所有砖块数据"]
    read --> assemble["组装 FPrecomputedVolumetricLightmapData"]

    assemble --> set_bounds["设置 Bounds / BrickSize<br/>/ IndirectionTextureDimensions"]
    assemble --> fill_ind["填充 IndirectionTexture<br/>（间接寻址纹理）"]
    assemble --> fill_brick["填充 BrickData<br/>（SH系数 / 天空遮蔽）"]

    set_bounds --> registry["存储到 UMapBuildDataRegistry"]
    fill_ind --> registry
    fill_brick --> registry

    registry --> det["CompleteDeterministicMappings()"]
    det --> encode["EncodeTextures()"]
    encode --> close["LightmassProcessor->CloseJob()"]
    close --> apply["ApplyNewLightingData() ⭐"]

    apply --> loop_levels["遍历所有 Level"]
    loop_levels --> on_apply["Level->OnApplyNewLightingData()"]
    on_apply --> set_grid["Registry->SetVolumetricLightMapGridDesc()"]
    set_grid --> setup_cluster["Registry->SetupLightmapResourceClusters()"]
    setup_cluster --> init_render["Level->InitializeRenderingResources()"]
    init_render --> add_scene["FPrecomputedVolumetricLightmap<br/>::AddToScene() ⭐"]

    add_scene --> get_data["Registry->GetLevelPrecomputed<br/>VolumetricLightmapBuildData()"]
    get_data --> set_data["ENQUEUE_RENDER_COMMAND<br/>→ SetData(NewData, Scene)"]
    set_data --> add_pvl["Scene->AddPrecomputedVolumetricLightmap()"]
    add_pvl --> add_level["FVolumetricLightmapSceneData<br/>::AddLevelVolume()"]
    add_level --> add_to_scene["Data->AddToSceneData()<br/>创建/更新 GPU IndirectionTexture"]

    apply --> recreate["FGlobalComponentRecreateRenderStateContext()<br/>重建所有组件渲染状态"]
    recreate --> gc["CollectGarbage()<br/>清理旧数据"]

    style finish fill:#2196F3,color:#fff
    style import_vlm fill:#E91E63,color:#fff
    style apply fill:#E91E63,color:#fff
    style add_scene fill:#E91E63,color:#fff
    style add_to_scene fill:#9C27B0,color:#fff
```

**关键文件位置：**
| 类/函数 | 文件 |
|---|---|
| `ImportVolumetricLightmap()` | `Editor/UnrealEd/Private/Lightmass/ImportVolumetricLightmap.cpp` |
| `FPrecomputedVolumetricLightmap` | `Runtime/Engine/Private/PrecomputedVolumetricLightmap.cpp` |
| `FPrecomputedVolumetricLightmapData` | 同上 |
| `FVolumetricLightmapSceneData` | `Runtime/Renderer/Private/VolumetricLightmapSceneData.cpp` |

---

## 七、编辑器端类关系图

```mermaid
classDiagram
    direction TB

    class FStaticLightingManager {
        <<Singleton 单例>>
        +CreateStaticLightingSystem()
        +FailLightingBuild()
        -ActiveStaticLightingSystem
    }

    class FStaticLightingSystem {
        -LightmassProcessor* : FLightmassProcessor
        -LightingContext
        -CurrentBuildStage : LightingStage
        +BeginLightmassProcess()
        +CreateLightmassProcessor()
        +GatherScene()
        +InitiateLightmassProcessor()
        +KickoffSwarm()
        +UpdateLightingBuild()
        +FinishLightmassProcess()
        +ApplyNewLightingData()
    }

    class FLightmassProcessor {
        -Swarm& : FSwarmInterface
        -Exporter* : FLightmassExporter
        -Importer* : FLightmassImporter
        +BeginRun() : bool
        +Update() : bool
        +CompleteRun()
        +ImportVolumetricLightmap()
        +ImportIrradianceTasks()
        +CloseJob()
    }

    class FLightmassExporter {
        +VolumetricLightmapTaskGuids : TArray~FGuid~
        +QualityLevel
        +WriteToChannel()
        +WriteVolumetricLightmapData()
        +WriteLights()
        +WriteModels()
        +WriteStaticMeshes()
    }

    class FSwarmInterface {
        <<Abstract 抽象接口>>
        +Initialize()$
        +OpenConnection()
        +OpenJob() / CloseJob()
        +BeginJobSpecification()
        +AddTask()
        +EndJobSpecification()
        +OpenChannel() / WriteChannel() / ReadChannel()
    }

    class FSwarmInterfaceLocalImpl {
        -LightmassProcHandle
        +PrepareJobFiles()
        +PrepareTasksList()
    }

    class FSwarmInterfaceImpl {
        +InitSwarmInterfaceManaged()
    }

    class UMapBuildDataRegistry {
        +GetLevelPrecomputedVolumetricLightmapBuildData()
        +SetVolumetricLightMapGridDesc()
        +SetupLightmapResourceClusters()
    }

    FStaticLightingManager "1" --> "0..1" FStaticLightingSystem : 创建和管理
    FStaticLightingSystem "1" --> "1" FLightmassProcessor : 拥有
    FLightmassProcessor "1" --> "1" FLightmassExporter : 拥有
    FLightmassProcessor "1" --> "1" FSwarmInterface : 引用
    FLightmassExporter ..> FSwarmInterface : 通过通道写数据
    FSwarmInterface <|-- FSwarmInterfaceLocalImpl : 本地实现
    FSwarmInterface <|-- FSwarmInterfaceImpl : 分布式实现
    FLightmassProcessor ..> UMapBuildDataRegistry : 导入结果存入
```

---

## 八、Lightmass 端类关系图

```mermaid
classDiagram
    direction TB

    class FLightmassSwarm {
        -API& : FSwarmInterface
        -TaskQueue
        +RequestTask() : FGuid
        +AcceptTask()
        +TaskCompleted()
        +Read() / Write()
        +OpenChannel() / CloseChannel()
    }

    class FLightmassImporter {
        -Swarm* : FLightmassSwarm
        +ImportScene(Scene, SceneGuid)
        +ImportData()
        +ImportArray()
    }

    class FScene {
        +VolumetricLightmapSettings
        +Lights : TArray
        +StaticMeshInstances : TArray
        +ImportanceVolumeBounds
    }

    class FStaticLightingSystem {
        <<Lightmass端>>
        -Scene& : FScene
        -Exporter& : FLightmassSolverExporter
        -NumThreads : int32
        +CalculateAdaptiveVolumetricLightmap()
        +RecursivelyBuildBrickTree()
        +ProcessVolumetricLightmapBrickTask()
        +ProcessVolumetricLightmapTaskIfAvailable()
        +ThreadLoop()
    }

    class FLightmassSolverExporter {
        -Swarm* : FLightmassSwarm
        +ExportResults()
        +ExportVolumetricLightmapBrickData()
    }

    class FVolumetricLightmapTaskData {
        +Bricks : TArray~FIrradianceBrickBuildData~
    }

    class FIrradianceBrickBuildData {
        +BrickPosition : FIntVector
        +BrickSize : int32
        +SHCoefficients : TArray
        +SkyOcclusion : TArray
    }

    FLightmassSwarm --> FLightmassImporter : 提供数据通道
    FLightmassImporter --> FScene : 填充场景
    FScene --> FStaticLightingSystem : 输入
    FStaticLightingSystem --> FVolumetricLightmapTaskData : 生成
    FVolumetricLightmapTaskData "1" --> "*" FIrradianceBrickBuildData : 包含
    FStaticLightingSystem --> FLightmassSolverExporter : 输出结果
    FLightmassSolverExporter --> FLightmassSwarm : 写回
```

---

## 九、运行时渲染类关系图

```mermaid
classDiagram
    direction TB

    class FPrecomputedVolumetricLightmap {
        +Data* : FPrecomputedVolumetricLightmapData
        -bAddedToScene : bool
        -SourceRegistry : UMapBuildDataRegistry
        +AddToScene(Scene, Registry, Id, bPersistent)
        +RemoveFromScene(Scene)
        +SetData(NewData, Scene)
    }

    class FPrecomputedVolumetricLightmapData {
        +Bounds : FBox
        +BrickSize : int32
        +IndirectionTexture : FVolumetricLightmapDataLayer
        +IndirectionTextureDimensions : FIntVector
        +BrickData : FVolumetricLightmapBrickData
        +BrickDataDimensions : FIntVector
        +AddToSceneData(GlobalData)
        +FinalizeImport()
        +InitResource()
    }

    class FVolumetricLightmapBrickData {
        +AmbientVector : FVolumetricLightmapDataLayer
        +SHCoefficients[6] : FVolumetricLightmapDataLayer
        +SkyBentNormal : FVolumetricLightmapDataLayer
        +DirectionalLightShadowing : FVolumetricLightmapDataLayer
    }

    class FVolumetricLightmapSceneData {
        -Scene* : FScene
        -GlobalVolumetricLightmap : FPrecomputedVolumetricLightmap
        +GlobalVolumetricLightmapData : FPrecomputedVolumetricLightmapData
        -LevelVolumetricLightmaps : TArray
        -PersistentLevelVolumetricLightmap
        +CPUInterpolationCache : TMap
        +AddLevelVolume()
        +RemoveLevelVolume()
        +HasData() : bool
    }

    class FDeferredShadingSceneRenderer {
        +VisualizeVolumetricLightmap(GraphBuilder, SceneTextures)
    }

    class FVisualizeVolumetricLightmapPS {
        <<Pixel Shader>>
        采样 SH 系数并渲染球体
    }

    FPrecomputedVolumetricLightmap "1" --> "1" FPrecomputedVolumetricLightmapData : Data*
    FPrecomputedVolumetricLightmapData "1" --> "1" FVolumetricLightmapBrickData : BrickData
    FVolumetricLightmapSceneData "1" --> "*" FPrecomputedVolumetricLightmap : 管理多Level
    FVolumetricLightmapSceneData "1" --> "1" FPrecomputedVolumetricLightmapData : GlobalData
    FDeferredShadingSceneRenderer ..> FVolumetricLightmapSceneData : 读取数据
    FDeferredShadingSceneRenderer ..> FVisualizeVolumetricLightmapPS : 使用着色器
```

---

## 十、完整端到端时序图

```mermaid
sequenceDiagram
    actor User as 👤 用户
    box rgb(232,245,233) 编辑器进程
        participant UI as LevelEditorActions
        participant Engine as UEditorEngine
        participant SLM as FStaticLightingManager
        participant SLS as FStaticLightingSystem
        participant LMP as FLightmassProcessor
        participant EXP as FLightmassExporter
    end
    participant Swarm as 🔗 FSwarmInterface
    box rgb(255,235,238) Lightmass 独立进程
        participant LM as UnrealLightmass.exe
        participant LMI as FLightmassImporter
        participant LMSLS as FStaticLightingSystem
        participant LMSE as FLightmassSolverExporter
    end
    box rgb(227,242,253) 运行时渲染
        participant PVL as FPrecomputedVolumetricLightmap
        participant VLSD as FVolumetricLightmapSceneData
    end

    Note over User,VLSD: ═══════════ 阶段一 & 二：触发与初始化 ═══════════

    User->>UI: 点击 Build Light Only
    UI->>Engine: BuildLighting()
    Engine->>SLM: CreateStaticLightingSystem()
    SLM->>SLS: new FStaticLightingSystem()
    SLS->>SLS: BeginLightmassProcess()
    SLS->>LMP: new FLightmassProcessor()
    LMP->>Swarm: OpenConnection()
    LMP->>EXP: new FLightmassExporter()
    SLS->>SLS: GatherScene()

    Note over User,VLSD: ═══════════ 阶段三：数据导出 ═══════════

    SLS->>EXP: WriteToChannel()
    EXP->>EXP: WriteVolumetricLightmapData()
    EXP->>Swarm: WriteChannel(场景+VLM数据)

    Note over User,VLSD: ═══════════ 阶段四：Swarm 调度 ═══════════

    SLS->>LMP: KickoffSwarm() → BeginRun()
    LMP->>Swarm: BeginJobSpecification()
    loop 每个 VolumetricLightmap 任务
        LMP->>Swarm: AddTask(TaskGuid, "VolumetricLightmap")
    end
    LMP->>Swarm: EndJobSpecification()
    Swarm->>LM: 🚀 启动进程

    Note over User,VLSD: ═══════════ 阶段五：Lightmass 计算 ═══════════

    LM->>LM: BuildStaticLighting()
    LM->>LMI: ImportScene()
    LMI->>Swarm: 读取场景数据
    LM->>LMSLS: new FStaticLightingSystem()
    LMSLS->>LMSLS: CalculateAdaptiveVolumetricLightmap()
    LMSLS->>LMSLS: RecursivelyBuildBrickTree()

    loop 多线程处理每个砖块
        LMSLS->>LMSLS: ProcessVolumetricLightmapBrickTask()
        Note right of LMSLS: 光线追踪 → SH编码 → 天空遮蔽
        LMSLS->>LMSE: ExportResults(砖块数据)
        LMSE->>Swarm: WriteChannel(结果)
        LMSE->>Swarm: TaskCompleted(TaskGuid)
    end

    Note over User,VLSD: ═══════════ 阶段六：结果导入（编辑器端） ═══════════

    loop 异步轮询（编辑器主循环）
        SLS->>LMP: Update()
        LMP->>LMP: ImportIrradianceTasks()
        LMP->>Swarm: ReadChannel(砖块数据)
    end

    SLS->>SLS: FinishLightmassProcess()
    SLS->>LMP: CompleteRun()
    LMP->>LMP: ImportVolumetricLightmap()
    Note right of LMP: 组装 FPrecomputedVolumetricLightmapData<br/>→ 存入 UMapBuildDataRegistry

    Note over User,VLSD: ═══════════ 阶段七：应用到场景 ═══════════

    SLS->>SLS: ApplyNewLightingData()
    SLS->>PVL: AddToScene()
    PVL->>PVL: SetData(NewData) [渲染线程]
    PVL->>VLSD: AddLevelVolume()
    VLSD->>VLSD: Data->AddToSceneData() [GPU Compute]
    Note right of VLSD: 创建 IndirectionTexture<br/>上传 BrickData 到 GPU
```

---

## 十一、自适应砖块树结构图

```mermaid
flowchart TD
    subgraph BrickTree["🌳 自适应砖块树 (Adaptive Brick Tree)"]
        root["Level 0 (最粗)<br/>整个 Importance Volume<br/>BrickSize = 最大值"]
        root --> c1["Level 1 子砖块"]
        root --> c2["Level 1 子砖块"]
        root --> c3["Level 1 子砖块<br/>(无几何体，不再细分)"]

        c1 --> c1a["Level 2 子砖块"]
        c1 --> c1b["Level 2 子砖块<br/>(叶子 → 生成任务)"]

        c2 --> c2a["Level 2 子砖块"]
        c2 --> c2b["Level 2 子砖块"]

        c1a --> c1a1["Level 3 (最细)<br/>靠近几何体表面<br/>(叶子 → 生成任务)"]
        c1a --> c1a2["Level 3<br/>(叶子 → 生成任务)"]

        c2a --> c2a1["Level 3<br/>(叶子 → 生成任务)"]
        c2b --> c2b1["Level 3<br/>(叶子 → 生成任务)"]
    end

    subgraph Criteria["📏 细分判据"]
        cr1["几何体密度<br/>（靠近表面 → 细分）"]
        cr2["AVolumetricLightmapDensityVolume<br/>（手动指定高密度区域）"]
        cr3["最大细分层级限制"]
    end

    Criteria -.-> BrickTree

    style c3 fill:#90A4AE,color:#fff
    style c1b fill:#4CAF50,color:#fff
    style c1a1 fill:#4CAF50,color:#fff
    style c1a2 fill:#4CAF50,color:#fff
    style c2a1 fill:#4CAF50,color:#fff
    style c2b1 fill:#4CAF50,color:#fff
```

> 🟢 绿色 = 叶子节点，每个叶子生成一个 `FIrradianceBrickBuildData` 任务  
> ⬜ 灰色 = 不需要细分的区域（远离几何体）

---

## 十二、GPU 数据布局图

```mermaid
flowchart LR
    subgraph CPU["CPU 端数据"]
        pvld["FPrecomputedVolumetricLightmapData"]
        pvld --> ind_cpu["IndirectionTexture<br/>(3D 纹理, uint16×4)<br/>存储砖块在 BrickData 中的偏移"]
        pvld --> brick_cpu["BrickData<br/>AmbientVector (SH L0)<br/>SHCoefficients[0..5] (SH L1/L2)<br/>SkyBentNormal<br/>DirectionalLightShadowing"]
    end

    subgraph GPU["GPU 端纹理"]
        ind_gpu["🎨 IndirectionTexture3D<br/>Dimensions: IndirectionTextureDimensions<br/>每个 texel → 砖块偏移"]
        brick_gpu["🎨 BrickTexture3D<br/>Dimensions: BrickDataDimensions<br/>每个 texel → SH 系数"]
    end

    ind_cpu -->|"InitResource() /<br/>AddToSceneData()"| ind_gpu
    brick_cpu -->|"InitResource() /<br/>AddToSceneData()"| brick_gpu

    subgraph Sampling["🔍 运行时采样流程"]
        pos["世界坐标 P"]
        pos --> lookup["P → IndirectionTexture UV"]
        lookup --> offset["读取砖块偏移"]
        offset --> brick_uv["计算 BrickTexture UV"]
        brick_uv --> sample["采样 SH 系数"]
        sample --> eval["SH 求值 → 最终光照颜色"]
    end

    ind_gpu --> lookup
    brick_gpu --> sample

    style ind_gpu fill:#42A5F5,color:#fff
    style brick_gpu fill:#AB47BC,color:#fff
```

---

## 十三、调试断点速查图

```mermaid
flowchart TD
    subgraph EditorBreakpoints["🔴 编辑器端断点"]
        eb1["BeginLightmassProcess()<br/>📍 追踪构建触发"]
        eb2["WriteVolumetricLightmapData()<br/>📍 查看VLM数据导出"]
        eb3["BeginRun() 中 AddTask 处<br/>📍 查看Swarm任务提交"]
        eb4["ImportIrradianceTasks()<br/>📍 追踪增量导入"]
        eb5["ImportVolumetricLightmap()<br/>📍 追踪最终导入"]
        eb6["AddToScene()<br/>📍 数据应用到场景"]
        eb7["SetData()<br/>📍 渲染线程数据设置"]
        eb8["AddToSceneData()<br/>📍 GPU数据合并"]

        eb1 --> eb2 --> eb3 --> eb4 --> eb5 --> eb6 --> eb7 --> eb8
    end

    subgraph LightmassBreakpoints["🔵 Lightmass端断点"]
        lb1["BuildStaticLighting()<br/>📍 程序入口"]
        lb2["ImportScene()<br/>📍 场景导入"]
        lb3["CalculateAdaptiveVolumetricLightmap()<br/>📍 VLM计算入口"]
        lb4["RecursivelyBuildBrickTree()<br/>📍 砖块树构建"]
        lb5["ProcessVolumetricLightmapBrickTask()<br/>📍 单砖块计算"]
        lb6["bDebugVolumetricLightmapCell = true<br/>📍 调试特定单元格"]
        lb7["ExportResults()<br/>📍 结果导出"]

        lb1 --> lb2 --> lb3 --> lb4 --> lb5 --> lb6 --> lb7
    end

    subgraph DebugMethods["🛠️ 调试方法"]
        dm1["方法一：Debug Mode<br/>BaseLightmass.ini 设置<br/>bDebugMode=True<br/>手动从VS启动Lightmass"]
        dm2["方法二：Attach to Process<br/>正常Build后附加到<br/>UnrealLightmass.exe"]
        dm3["方法三：可视化<br/>Show → Visualize →<br/>Volumetric Lightmap"]
    end

    style eb1 fill:#F44336,color:#fff
    style eb5 fill:#F44336,color:#fff
    style eb6 fill:#F44336,color:#fff
    style lb3 fill:#2196F3,color:#fff
    style lb5 fill:#2196F3,color:#fff
    style dm1 fill:#FF9800,color:#fff
    style dm3 fill:#4CAF50,color:#fff
```

---

## 十四、可视化渲染管线

```mermaid
flowchart TD
    trigger["Show → Visualize → Volumetric Lightmap<br/>设置 ShowFlag"]
    trigger --> check{"FVolumetricLightmapSceneData<br/>::HasData() ?"}
    check -->|"否"| skip["跳过"]
    check -->|"是"| render["FDeferredShadingSceneRenderer<br/>::VisualizeVolumetricLightmap()"]

    render --> vs["顶点着色器 (VS)"]
    vs --> vs1["为每个 VLM 采样点<br/>生成 Billboard 四边形"]
    vs --> vs2["通过 IndirectionTexture<br/>查找砖块偏移"]
    vs --> vs3["计算 BrickUVs"]

    render --> ps["像素着色器 (PS)<br/>FVisualizeVolumetricLightmapPS"]
    ps --> ps1["从 BrickUVs 采样<br/>SH 系数"]
    ps1 --> ps2["GetVolumetricLightmapSH3()"]
    ps2 --> ps3["球面法线 × 漫反射传输 SH"]
    ps3 --> ps4["点乘 → 最终光照颜色"]
    ps4 --> ps5["远处渐变为环境光<br/>减少走样"]

    ps5 --> output["🖥️ 输出到 SceneTextures"]

    subgraph CVars["⚙️ 控制台变量"]
        cv1["r.VolumetricLightmap<br/>.VisualizationRadiusScale<br/>球体半径缩放"]
        cv2["r.VolumetricLightmap<br/>.VisualizationMinScreenFraction<br/>最小屏幕占比"]
    end

    CVars -.-> render

    style trigger fill:#4CAF50,color:#fff
    style render fill:#2196F3,color:#fff
    style output fill:#9C27B0,color:#fff
```

**着色器文件：** `Shaders/Private/VisualizeVolumetricLightmap.usf`

---

## 十五、关键日志与命令速查

| 类别 | 内容 |
|------|------|
| **编辑器日志** | `LogStaticLightingSystem` — 静态光照系统 |
| **编辑器日志** | `LogLightmassSolver` — Lightmass 处理器 |
| **Lightmass日志** | `LogLightmass` — Lightmass 进程 |
| **CVar** | `r.VolumetricLightmap.VisualizationRadiusScale` — 可视化球体半径 |
| **CVar** | `r.VolumetricLightmap.VisualizationMinScreenFraction` — 最小屏幕占比 |
| **统计** | `stat LightRendering` — 光照渲染统计 |
| **调试变量** | `bDebugVolumetricLightmapCell` — Lightmass 中的单元格调试开关 |
| **INI配置** | `BaseLightmass.ini` → `[DevOptions.StaticLighting]` → `bDebugMode=True` |

---

## 十六、一键导出插件方案（方案三：运行时读取）

> 本节对应之前讨论的**方案三**。该方案的核心思路是：**不修改引擎源码**，在编辑器运行时直接从已加载的 `FPrecomputedVolumetricLightmapData` 中读取 CPU 端数据并导出。

### 为什么选择方案三

| 维度 | 方案一（Build时介入） | 方案三（运行时读取）⭐ |
|------|----------------------|------------------------|
| **触发时机** | 必须在 Build Light 过程中执行 | **随时可以执行**，只要场景有已烘焙的 VLM 数据 |
| **侵入性** | 需要修改引擎 `ImportVolumetricLightmap()` 源码 | **可以做成纯插件**，不改引擎代码 |
| **用户体验** | 用户必须重新 Build Light 才能导出 | **一键导出**，打开任何已烘焙的场景直接导出 |
| **数据可用性** | 仅 Build 时一次性可用 | 编辑器模式下 CPU 数据始终保留 |
| **部署方式** | 改引擎源码，升级引擎时需要维护 | **独立插件**，引擎升级无影响 |

### 关键保障：编辑器模式下 CPU 数据始终可用

在 `PrecomputedVolumetricLightmap.cpp` 的 `SetData()` 中：

```cpp
Data->IndirectionTexture.bNeedsCPUAccess = GIsEditor;
Data->BrickData.SetNeedsCPUAccess(GIsEditor);
```

当 `bNeedsCPUAccess = true` 时，`FVolumetricLightmapDataLayer::Discard()` 不会释放 CPU 端的 `TArray<uint8> Data`，因此编辑器模式下 CPU 数据在 GPU 上传后依然保留。

### 插件实现流程图

```mermaid
flowchart TD
    A["🖱️ 用户点击插件按钮<br/>'Export VLM Probes'"] --> B["获取当前 World"]
    B --> C["获取 PersistentLevel 的<br/>FPrecomputedVolumetricLightmap"]
    C --> D["拿到 Data*<br/>(FPrecomputedVolumetricLightmapData)"]

    D --> E["读取 IndirectionTexture<br/>+ IndirectionTextureDimensions"]
    D --> F["读取 BrickData.AmbientVector<br/>+ BrickData.SHCoefficients[0..5]"]
    D --> G["读取 Bounds / BrickSize"]

    E --> H["遍历所有有效体素"]
    F --> H
    G --> H

    H --> I["对每个体素：<br/>1. IndirectionTexture → 砖块偏移<br/>2. 计算 BrickTexture 坐标<br/>3. 读取 AmbientVector (FFloat3Packed)<br/>4. 读取 SHCoefficients[0..5] (FColor)"]

    I --> J["计算体素的世界坐标<br/>= Bounds.Min + VoxelIndex × VoxelSize"]

    J --> K["导出为 CSV / JSON / 自定义格式<br/>每行: WorldPos, AmbientRGB, SH[0..5]"]

    style A fill:#4CAF50,color:#fff
    style K fill:#2196F3,color:#fff
```

### 数据访问路径

```cpp
// 在插件中获取数据的关键路径：
UWorld* World = GEditor->GetEditorWorldContext().World();
ULevel* Level = World->PersistentLevel;
FPrecomputedVolumetricLightmap* VLM = Level->PrecomputedVolumetricLightmap;
FPrecomputedVolumetricLightmapData* Data = VLM->Data;

// 然后就可以访问：
// Data->BrickData.AmbientVector.Data        → TArray<uint8>，内部是 FFloat3Packed
// Data->BrickData.SHCoefficients[i].Data    → TArray<uint8>，内部是 FColor (RGBA8)
// Data->IndirectionTexture.Data             → TArray<uint8>，间接寻址表
// Data->Bounds                              → 世界空间范围
// Data->BrickSize                           → 砖块大小（每个砖块的体素数，如 4）
// Data->IndirectionTextureDimensions        → 间接纹理维度
// Data->BrickDataDimensions                 → 砖块数据纹理维度
```

---

## 十七、烘焙数据从 UAsset 加载到 GPU 的完整流程

> 本节详细说明烘焙好的 Volumetric Lightmap 数据**存储在哪里**、**如何从磁盘加载到内存**、**何时上传到 GPU 纹理**。

### 17.1 数据存储位置

烘焙完成后，VLM 数据存储在 `UMapBuildDataRegistry` 这个 UObject 中，它是一个**独立的 uasset 文件**，通常位于：

```
Content/<MapName>_BuiltData.uasset
```

`UMapBuildDataRegistry` 内部用一个 `TMap` 来存储每个 Level 的 VLM 数据：

```cpp
// MapBuildDataRegistry.h
class UMapBuildDataRegistry : public UObject
{
    // ...
    TMap<FGuid, FPrecomputedVolumetricLightmapData*> LevelPrecomputedVolumetricLightmapBuildData;
    FVolumetricLightMapGridDesc* VolumetricLightMapGridDesc; // World Partition 用
};
```

每个 `FPrecomputedVolumetricLightmapData` 包含完整的 VLM 数据（IndirectionTexture + BrickData）。

### 17.2 完整加载流程图

```mermaid
flowchart TD
    subgraph Disk["💾 磁盘 (UAsset)"]
        uasset["MapName_BuiltData.uasset<br/>包含 UMapBuildDataRegistry"]
    end

    subgraph Deserialize["📦 反序列化阶段"]
        load["UObject 序列化系统<br/>调用 UMapBuildDataRegistry::Serialize()"]
        load --> ar_vlm["Ar << LevelPrecomputedVolumetricLightmapBuildData"]
        ar_vlm --> ar_data["对每个 FPrecomputedVolumetricLightmapData：<br/>Ar << Volume"]
        ar_data --> ar_fields["依次反序列化：<br/>① Bounds (FBox)<br/>② IndirectionTextureDimensions (FIntVector)<br/>③ IndirectionTexture (FVolumetricLightmapDataLayer)<br/>④ BrickSize (int32)<br/>⑤ BrickDataDimensions (FIntVector)<br/>⑥ BrickData.AmbientVector<br/>⑦ BrickData.SHCoefficients[0..5]<br/>⑧ BrickData.SkyBentNormal<br/>⑨ BrickData.DirectionalLightShadowing<br/>⑩ SubLevelBrickPositions<br/>⑪ IndirectionTextureOriginalValues"]
        ar_fields --> convert["ConvertBGRA8ToRGBA8ForLayer()<br/>将 PF_B8G8R8A8 → PF_R8G8B8A8"]
    end

    subgraph CPUMemory["🧠 CPU 内存"]
        registry["UMapBuildDataRegistry<br/>.LevelPrecomputedVolumetricLightmapBuildData<br/>TMap&lt;FGuid, FPrecomputedVolumetricLightmapData*&gt;"]
        registry --> pvlmd["FPrecomputedVolumetricLightmapData<br/>├─ IndirectionTexture.Data (TArray&lt;uint8&gt;)<br/>├─ BrickData.AmbientVector.Data<br/>├─ BrickData.SHCoefficients[0..5].Data<br/>├─ BrickData.SkyBentNormal.Data<br/>└─ BrickData.DirectionalLightShadowing.Data"]
    end

    subgraph SceneInit["🎬 场景初始化阶段"]
        level_init["ULevel::InitializeRenderingResources()"]
        level_init --> check_added{"PrecomputedVolumetricLightmap<br/>->IsAddedToScene() ?"}
        check_added -->|"否"| add_scene["PrecomputedVolumetricLightmap<br/>->AddToScene(Scene, Registry,<br/>LevelBuildDataId, bIsPersistent)"]
        add_scene --> get_data["Registry->GetLevelPrecomputed<br/>VolumetricLightmapBuildData(LevelBuildDataId)<br/>从 TMap 中查找对应 GUID 的数据"]
        get_data --> enqueue["ENQUEUE_RENDER_COMMAND<br/>→ SetData(NewData, Scene)"]
    end

    subgraph RenderThread["🔧 渲染线程"]
        set_data["SetData()"]
        set_data --> set_cpu["设置 bNeedsCPUAccess = GIsEditor<br/>（编辑器模式保留 CPU 数据）"]
        set_cpu --> init_rhi["Data->InitResource(RHICmdList)<br/>→ 调用 InitRHI()"]
    end

    subgraph GPU["🎮 GPU 上传"]
        init_rhi_detail["InitRHI()"]
        init_rhi_detail --> create_ind["IndirectionTexture.CreateTexture()<br/>创建 3D 纹理 + 上传数据<br/>格式: PF_R8G8B8A8<br/>维度: IndirectionTextureDimensions"]
        init_rhi_detail --> create_ambient["BrickData.AmbientVector.CreateTexture()<br/>格式: PF_FloatR11G11B10<br/>维度: BrickDataDimensions"]
        init_rhi_detail --> create_sh["BrickData.SHCoefficients[0..5].CreateTexture()<br/>格式: PF_R8G8B8A8<br/>维度: BrickDataDimensions"]
        init_rhi_detail --> create_sky["BrickData.SkyBentNormal.CreateTexture()<br/>（如果有数据）"]
        init_rhi_detail --> create_shadow["BrickData.DirectionalLightShadowing.CreateTexture()"]
        init_rhi_detail --> atlas_insert["GVolumetricLightmapBrickAtlas.Insert(this)<br/>将砖块数据拷贝到全局 Atlas 纹理"]
        atlas_insert --> release_rhi["BrickData.ReleaseRHI()<br/>释放单独的 GPU 纹理<br/>（数据已在 Atlas 中）"]
        release_rhi --> discard["Discard() CPU 数据<br/>（如果 bNeedsCPUAccess=false）"]
    end

    subgraph SceneData["🌍 场景数据合并"]
        add_pvl["Scene->AddPrecomputedVolumetricLightmap()"]
        add_pvl --> add_level["FVolumetricLightmapSceneData<br/>::AddLevelVolume()"]
        add_level --> add_to_scene["Data->AddToSceneData(&GlobalVolumetricLightmapData)<br/>将本 Level 的 IndirectionTexture<br/>合并到全局 SceneData"]
    end

    Disk --> Deserialize --> CPUMemory --> SceneInit --> RenderThread --> GPU
    RenderThread --> SceneData

    style uasset fill:#795548,color:#fff
    style init_rhi_detail fill:#E91E63,color:#fff
    style atlas_insert fill:#9C27B0,color:#fff
    style add_to_scene fill:#2196F3,color:#fff
```

### 17.3 关键函数调用链时序图

```mermaid
sequenceDiagram
    participant Disk as 💾 UAsset
    participant UObj as UObject 序列化
    participant Registry as UMapBuildDataRegistry
    participant Level as ULevel
    participant PVL as FPrecomputedVolumetricLightmap
    participant Data as FPrecomputedVolumetricLightmapData
    participant RT as 渲染线程
    participant Atlas as GVolumetricLightmapBrickAtlas
    participant VLSD as FVolumetricLightmapSceneData

    Note over Disk,VLSD: ═══════ 第一步：从磁盘反序列化到 CPU 内存 ═══════

    Disk->>UObj: 加载 MapName_BuiltData.uasset
    UObj->>Registry: UMapBuildDataRegistry::Serialize(Ar)
    Registry->>Registry: Ar << LevelPrecomputedVolumetricLightmapBuildData
    Registry->>Data: new FPrecomputedVolumetricLightmapData()
    Registry->>Data: Ar << (*Volume) 反序列化所有字段
    Note right of Data: IndirectionTexture.Data → TArray<uint8><br/>BrickData.AmbientVector.Data → TArray<uint8><br/>BrickData.SHCoefficients[0..5].Data → TArray<uint8><br/>加载时自动 BGRA→RGBA 转换

    Note over Disk,VLSD: ═══════ 第二步：Level 初始化渲染资源 ═══════

    Level->>Level: InitializeRenderingResources()
    Level->>PVL: AddToScene(Scene, Registry, LevelBuildDataId, bIsPersistent)
    PVL->>Registry: GetLevelPrecomputedVolumetricLightmapBuildData(Guid)
    Registry-->>PVL: 返回 FPrecomputedVolumetricLightmapData*

    Note over Disk,VLSD: ═══════ 第三步：渲染线程初始化 GPU 资源 ═══════

    PVL->>RT: ENQUEUE_RENDER_COMMAND → SetData(NewData, Scene)
    RT->>Data: SetData(): bNeedsCPUAccess = GIsEditor
    RT->>Data: InitResource(RHICmdList) → InitRHI()
    Data->>Data: IndirectionTexture.CreateTexture(Dims) → 创建 GPU 3D 纹理
    Data->>Data: BrickData.AmbientVector.CreateTexture(Dims)
    Data->>Data: BrickData.SHCoefficients[0..5].CreateTexture(Dims)
    Data->>Atlas: GVolumetricLightmapBrickAtlas.Insert(this)
    Atlas->>Atlas: CopyDataIntoAtlas() → 拷贝到全局 Atlas 纹理
    Data->>Data: BrickData.ReleaseRHI() → 释放单独 GPU 纹理
    Note right of Data: CPU 端 TArray<uint8> 数据：<br/>编辑器模式保留（bNeedsCPUAccess=true）<br/>打包模式释放（Discard()）

    Note over Disk,VLSD: ═══════ 第四步：合并到全局场景数据 ═══════

    PVL->>VLSD: Scene->AddPrecomputedVolumetricLightmap()
    VLSD->>VLSD: AddLevelVolume(Volume, ShadingPath, bIsPersistent)
    VLSD->>Data: Data->AddToSceneData(&GlobalVolumetricLightmapData)
    Note right of Data: 持久关卡：用 Compute Shader 拷贝整个 IndirectionTexture<br/>子关卡：用 PatchIndirectionTextureCS 修补 IndirectionTexture
```

### 17.4 数据存储位置速查表

| 阶段 | 数据位置 | 格式 | 生命周期 |
|------|---------|------|---------|
| **磁盘** | `MapName_BuiltData.uasset` 中的 `UMapBuildDataRegistry` | 二进制序列化 | 永久 |
| **CPU 内存** | `UMapBuildDataRegistry.LevelPrecomputedVolumetricLightmapBuildData[Guid]` → `FPrecomputedVolumetricLightmapData` | `TArray<uint8>` | 编辑器模式：始终保留<br/>打包模式：GPU 上传后释放 |
| **GPU 单独纹理** | `FPrecomputedVolumetricLightmapData.BrickData.*.Texture` | RHI 3D Texture | 临时，拷贝到 Atlas 后释放 |
| **GPU Atlas** | `GVolumetricLightmapBrickAtlas.TextureSet` | RHI 3D Texture (全局) | 场景存在期间 |
| **GPU IndirectionTexture** | `GlobalVolumetricLightmapData.IndirectionTexture.Texture` | RHI 3D Texture (全局) | 场景存在期间 |
| **场景引用** | `FScene.VolumetricLightmapSceneData.GlobalVolumetricLightmapData` | 指针 | 场景存在期间 |

---

## 十八、IndirectionTexture 与 BrickTexture 的映射计算方式

> 本节详细说明 Volumetric Lightmap 的**两级寻址机制**：从世界坐标到最终 SH 系数的完整计算过程。

### 18.1 核心概念

Volumetric Lightmap 使用**两级间接寻址**来高效存储自适应精度的光照数据：

```
世界坐标 P → IndirectionTexture（粗粒度，均匀网格）→ BrickTexture（细粒度，自适应）→ SH 系数
```

- **IndirectionTexture**：一个均匀的 3D 纹理，覆盖整个 Importance Volume。每个 texel 存储 4 字节 (R8G8B8A8)，指向 BrickTexture 中的砖块位置。
- **BrickTexture**：一个紧凑的 3D 纹理 Atlas，存储所有砖块的实际光照数据（AmbientVector、SHCoefficients 等）。

### 18.2 IndirectionTexture 的 Texel 格式

每个 IndirectionTexture texel 是 4 字节 `uint8[4]`：

| 字节 | 含义 |
|------|------|
| `[0]` (R) | `IndirectionBrickOffset.X` — 砖块在 BrickTexture 布局中的 X 位置 |
| `[1]` (G) | `IndirectionBrickOffset.Y` — 砖块在 BrickTexture 布局中的 Y 位置 |
| `[2]` (B) | `IndirectionBrickOffset.Z` — 砖块在 BrickTexture 布局中的 Z 位置 |
| `[3]` (A) | `IndirectionBrickSize` — 该砖块在 IndirectionTexture 中覆盖的格子数（0 = 无数据） |

### 18.3 完整映射计算流程图

```mermaid
flowchart TD
    subgraph Step1["步骤一：世界坐标 → IndirectionTexture 坐标"]
        P["世界坐标 P (FVector)"]
        P --> normalize["归一化到 [0,1]<br/>UV = (P - Bounds.Min) / Bounds.GetSize()"]
        normalize --> scale["缩放到纹理空间<br/>IndirectionCoord = UV × IndirectionTextureDimensions"]
        scale --> clamp["Clamp 到有效范围<br/>[0, Dims - 0.01]"]
    end

    subgraph Step2["步骤二：采样 IndirectionTexture"]
        clamp --> int_coord["取整 → IndirectionDataCoordinateInt"]
        int_coord --> linear_idx["线性索引 =<br/>(Z × DimsY + Y) × DimsX + X"]
        linear_idx --> read_texel["读取 4 字节<br/>IndirectionTextureData[index × 4]"]
        read_texel --> parse["解析：<br/>BrickOffset = (R, G, B)<br/>BrickSize = A"]
    end

    subgraph Step3["步骤三：计算 BrickTexture 坐标"]
        parse --> check{BrickSize > 0 ?}
        check -->|"否"| no_data["无有效数据<br/>返回黑色"]
        check -->|"是"| calc_frac["计算砖块内分数坐标<br/>InBricks = IndirectionCoord / BrickSize<br/>Frac = frac(InBricks)"]
        calc_frac --> calc_brick["BrickTextureCoord =<br/>BrickOffset × PaddedBrickSize<br/>+ Frac × BrickSize"]
    end

    subgraph Step4["步骤四：从 BrickTexture 采样"]
        calc_brick --> sample_ambient["采样 AmbientVector<br/>BrickData.AmbientVector[coord]<br/>格式: FFloat3Packed (R11G11B10)"]
        calc_brick --> sample_sh["采样 SHCoefficients[0..5]<br/>BrickData.SHCoefficients[i][coord]<br/>格式: FColor (R8G8B8A8)"]
        calc_brick --> sample_sky["采样 SkyBentNormal<br/>（如果有）"]
        calc_brick --> sample_shadow["采样 DirectionalLightShadowing"]
    end

    Step1 --> Step2 --> Step3 --> Step4

    style P fill:#4CAF50,color:#fff
    style parse fill:#FF9800,color:#fff
    style calc_brick fill:#E91E63,color:#fff
    style sample_ambient fill:#2196F3,color:#fff
    style sample_sh fill:#9C27B0,color:#fff
```

### 18.4 三个关键函数的代码解析

#### ① `ComputeIndirectionCoordinate()` — 世界坐标 → IndirectionTexture 坐标

```cpp
// 📍 PrecomputedVolumetricLightmap.cpp
FVector ComputeIndirectionCoordinate(
    FVector LookupPosition,
    const FBox& VolumeBounds,
    FIntVector IndirectionTextureDimensions)
{
    // 计算世界空间到 UV 空间的变换
    const FVector InvVolumeSize = FVector(1.0f) / VolumeBounds.GetSize();
    const FVector VolumeWorldToUVScale = InvVolumeSize;
    const FVector VolumeWorldToUVAdd = -VolumeBounds.Min * InvVolumeSize;

    // 变换到 IndirectionTexture 的纹素坐标
    FVector IndirectionCoord = (LookupPosition * VolumeWorldToUVScale + VolumeWorldToUVAdd)
                             * FVector(IndirectionTextureDimensions);

    // Clamp 到有效范围
    IndirectionCoord.X = FMath::Clamp(IndirectionCoord.X, 0.0f, IndirectionTextureDimensions.X - .01f);
    IndirectionCoord.Y = FMath::Clamp(IndirectionCoord.Y, 0.0f, IndirectionTextureDimensions.Y - .01f);
    IndirectionCoord.Z = FMath::Clamp(IndirectionCoord.Z, 0.0f, IndirectionTextureDimensions.Z - .01f);

    return IndirectionCoord;
}
```

**数学公式**：`IndirectionCoord = ((P - Bounds.Min) / Bounds.Size) × IndirectionTextureDimensions`

#### ② `SampleIndirectionTexture()` — 从 IndirectionTexture 读取砖块偏移

```cpp
// 📍 PrecomputedVolumetricLightmap.cpp
void SampleIndirectionTexture(
    FVector IndirectionDataSourceCoordinate,
    FIntVector IndirectionTextureDimensions,
    const uint8* IndirectionTextureData,
    FIntVector& OutIndirectionBrickOffset,
    int32& OutIndirectionBrickSize)
{
    // 取整得到整数坐标
    FIntVector IndirectionDataCoordinateInt(IndirectionDataSourceCoordinate);
    // Clamp
    IndirectionDataCoordinateInt.X = FMath::Clamp(IndirectionDataCoordinateInt.X, 0, IndirectionTextureDimensions.X - 1);
    IndirectionDataCoordinateInt.Y = FMath::Clamp(IndirectionDataCoordinateInt.Y, 0, IndirectionTextureDimensions.Y - 1);
    IndirectionDataCoordinateInt.Z = FMath::Clamp(IndirectionDataCoordinateInt.Z, 0, IndirectionTextureDimensions.Z - 1);

    // 计算线性索引（Z-major 排列）
    const int32 IndirectionDataIndex =
        ((IndirectionDataCoordinateInt.Z * IndirectionTextureDimensions.Y)
        + IndirectionDataCoordinateInt.Y) * IndirectionTextureDimensions.X
        + IndirectionDataCoordinateInt.X;

    // 读取 4 字节：[R=OffsetX, G=OffsetY, B=OffsetZ, A=BrickSize]
    const uint8* IndirectionVoxelPtr = &IndirectionTextureData[IndirectionDataIndex * 4];
    OutIndirectionBrickOffset = FIntVector(
        *(IndirectionVoxelPtr + 0),   // R → BrickOffset.X
        *(IndirectionVoxelPtr + 1),   // G → BrickOffset.Y
        *(IndirectionVoxelPtr + 2));  // B → BrickOffset.Z
    OutIndirectionBrickSize = *(IndirectionVoxelPtr + 3);  // A → BrickSize
}
```

#### ③ `ComputeBrickTextureCoordinate()` — IndirectionTexture 坐标 → BrickTexture 坐标

```cpp
// 📍 PrecomputedVolumetricLightmap.cpp
FVector ComputeBrickTextureCoordinate(
    FVector IndirectionDataSourceCoordinate,
    FIntVector IndirectionBrickOffset,
    int32 IndirectionBrickSize,
    int32 BrickSize)
{
    // 将 IndirectionTexture 坐标转换为"以砖块为单位"的坐标
    FVector IndirectionCoordInBricks = IndirectionDataSourceCoordinate / IndirectionBrickSize;

    // 取小数部分 → 砖块内的归一化位置 [0, 1)
    FVector FractionalCoord(
        FMath::Frac(IndirectionCoordInBricks.X),
        FMath::Frac(IndirectionCoordInBricks.Y),
        FMath::Frac(IndirectionCoordInBricks.Z));

    // PaddedBrickSize = BrickSize + 1（每个砖块有 1 个 texel 的 padding 用于三线性插值）
    int32 PaddedBrickSize = BrickSize + 1;

    // 最终 BrickTexture 坐标 = 砖块起始位置 + 砖块内偏移
    FVector BrickTextureCoordinate =
        FVector(IndirectionBrickOffset * PaddedBrickSize)  // 砖块在 Atlas 中的起始 texel
        + FractionalCoord * BrickSize;                      // 砖块内的偏移

    return BrickTextureCoordinate;
}
```

### 18.5 映射计算数学公式总结

```
给定：
  P           = 世界坐标
  Bounds      = VLM 覆盖的世界空间包围盒
  IndDims     = IndirectionTextureDimensions (如 64×64×32)
  BrickSize   = 每个砖块的体素数 (如 4)
  PaddedSize  = BrickSize + 1 = 5

步骤一：世界坐标 → IndirectionTexture 坐标
  IndCoord = ((P - Bounds.Min) / Bounds.Size) × IndDims

步骤二：采样 IndirectionTexture
  IntCoord = floor(IndCoord)
  LinearIndex = (IntCoord.Z × IndDims.Y + IntCoord.Y) × IndDims.X + IntCoord.X
  [R, G, B, A] = IndirectionTextureData[LinearIndex × 4 .. +3]
  BrickOffset = (R, G, B)    // 砖块在 BrickTexture 布局中的位置
  IndBrickSize = A            // 该砖块覆盖的 IndirectionTexture 格子数

步骤三：计算 BrickTexture 坐标
  CoordInBricks = IndCoord / IndBrickSize
  FracCoord = frac(CoordInBricks)
  BrickTexCoord = BrickOffset × PaddedSize + FracCoord × BrickSize

步骤四：从 BrickTexture 线性索引读取数据
  LinearBrickIndex = (BrickTexCoord.Z × BrickDataDims.Y + BrickTexCoord.Y)
                   × BrickDataDims.X + BrickTexCoord.X
  AmbientValue = AmbientVector.Data[LinearBrickIndex]        // FFloat3Packed
  SHValue[i]   = SHCoefficients[i].Data[LinearBrickIndex]    // FColor (RGBA8)
```

### 18.6 可视化示意图：两级寻址

```mermaid
flowchart LR
    subgraph World["🌍 世界空间"]
        wp["世界坐标 P = (1200, 500, 300)"]
    end

    subgraph IndTex["📐 IndirectionTexture (均匀网格)"]
        direction TB
        grid["64 × 64 × 32 的均匀网格<br/>每个格子覆盖一块世界空间<br/>格子大小 = Bounds.Size / IndDims"]
        cell["格子 [18, 7, 4]<br/>存储值: R=3, G=1, B=0, A=2<br/>→ BrickOffset=(3,1,0)<br/>→ BrickSize=2（覆盖2×2×2个格子）"]
    end

    subgraph BrickTex["🧱 BrickTexture (紧凑 Atlas)"]
        direction TB
        atlas["所有砖块紧凑排列<br/>维度: BrickDataDimensions"]
        brick["砖块起始位置:<br/>(3,1,0) × 5 = (15, 5, 0)<br/>砖块内偏移:<br/>frac(coord/2) × 4 = (2.4, 1.6, 0.8)<br/>最终坐标: (17.4, 6.6, 0.8)"]
        data["读取该坐标处的：<br/>AmbientVector (R11G11B10)<br/>SHCoefficients[0..5] (RGBA8)<br/>SkyBentNormal<br/>DirectionalLightShadowing"]
    end

    World -->|"ComputeIndirectionCoordinate()"| IndTex
    IndTex -->|"SampleIndirectionTexture()"| BrickTex
    BrickTex -->|"ComputeBrickTextureCoordinate()"| data

    style wp fill:#4CAF50,color:#fff
    style cell fill:#FF9800,color:#fff
    style data fill:#9C27B0,color:#fff
```

### 18.7 PaddedBrickSize 的作用

每个砖块在 BrickTexture 中占据 `(BrickSize + 1)³` 个 texel，而不是 `BrickSize³`。多出的 1 个 texel 是**边界 padding**，用于 GPU 三线性插值时避免采样到相邻砖块的数据：

```
BrickSize = 4 的砖块在 Atlas 中的布局：

  ┌─────────────┐
  │ 0 1 2 3 │ 4 │  ← 第 4 个 texel 是 padding（复制自相邻砖块或边界值）
  │ 有效数据  │pad│
  └─────────────┘
  实际占用 5 个 texel (PaddedBrickSize = 5)
```

### 18.8 关键文件位置

| 函数/类 | 文件 |
|---------|------|
| `ComputeIndirectionCoordinate()` | `Runtime/Engine/Private/PrecomputedVolumetricLightmap.cpp` |
| `SampleIndirectionTexture()` | 同上 |
| `ComputeBrickTextureCoordinate()` | 同上 |
| `FPrecomputedVolumetricLightmapData::InitRHI()` | 同上（GPU 纹理创建） |
| `FPrecomputedVolumetricLightmapData::AddToSceneData()` | 同上（场景数据合并） |
| `GVolumetricLightmapBrickAtlas` | 同上（全局 Atlas 管理） |
| `FVolumetricLightmapSceneData::AddLevelVolume()` | `Runtime/Renderer/Private/RendererScene.cpp` |
| `UMapBuildDataRegistry::Serialize()` | `Runtime/Engine/Private/MapBuildData.cpp` |
| `operator<<(FArchive&, FPrecomputedVolumetricLightmapData&)` | `Runtime/Engine/Private/PrecomputedVolumetricLightmap.cpp` |

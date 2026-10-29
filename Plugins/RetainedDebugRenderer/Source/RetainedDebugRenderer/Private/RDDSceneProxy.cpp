#include "RDDSceneProxy.h"

#include "RDDComponent.h"
#include "MeshElementCollector.h"
#include "PrimitiveDrawInterface.h"
#include "PrimitiveDrawingUtils.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "SceneView.h"
#include "Materials/Material.h" 
#include "Engine/Engine.h"
#include "UObject/GarbageCollectionSchema.h"

static TAutoConsoleVariable<int32> CVarRDDDrawPath(
	TEXT("RDD.DrawPath"), 2,
	TEXT("0 = draw nothing (to isolate buffer memory), 1 = PDI path (stage A), 2 = persistent buffer."),
	ECVF_RenderThreadSafe);

std::atomic<int32> FRDDSceneProxy::UploadCount{0};
std::atomic<int64> FRDDSceneProxy::UploadedVertices{0};
std::atomic<int64> FRDDSceneProxy::PartialWrites{0};
std::atomic<int64> FRDDSceneProxy::PartialBytesWritten{0};

namespace
{
	/** FColor is sRGB; the vertex colour stream is read as linear. One place, so build and update agree. */
	FColor ToVertexColor(const FColor& Color)
	{
		return FLinearColor(Color).ToFColor(/*bSRGB=*/false);
	}

	/** Writes Size bytes at Offset into a GPU buffer, touching nothing else in it. */
	void WriteRange(FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer,
					uint32 Offset, const void* Data, uint32 Size)
	{
		void* Dst = RHICmdList.LockBuffer(Buffer, Offset, Size, RLM_WriteOnly);
		FMemory::Memcpy(Dst, Data, Size);
		RHICmdList.UnlockBuffer(Buffer);
	}
}

FRDDSceneProxy::FRDDSceneProxy(const URDDComponent* InComponent)
	: FPrimitiveSceneProxy{InComponent}
, VertexFactory{GetScene().GetFeatureLevel(), "FRDDSceneProxy"}
{
	InComponent->GetRenderLines(Lines);
	bWillEverBeLit = false;
	
	UMaterialInterface* Material = InComponent->GetLineMaterial();
	MaterialProxy     = Material->GetRenderProxy();
	MaterialRelevance = Material->GetRelevance_Concurrent(GetScene().GetFeatureLevel());

	NumVertices = Lines.Num() * 2;
	if (NumVertices > 0)
	{
		BuildVertexBuffers();
		InitVertexFactory();
	}

	UE_LOG(LogTemp, Warning, TEXT("RDD proxy created: %d lines, %u vertices"),
		   Lines.Num(), NumVertices);
}

FRDDSceneProxy::~FRDDSceneProxy()
{
	// Proxies are deleted on the render thread, after the renderer has
	// finished with them, so releasing GPU resources here is safe.
	check(IsInParallelRenderingThread());

	VertexFactory.ReleaseResource();
	IndexBuffer.ReleaseResource();
	
	VertexBuffers.PositionVertexBuffer.ReleaseResource();
	VertexBuffers.ColorVertexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
}

SIZE_T FRDDSceneProxy::GetTypeHash() const
{
	// address of functioan local static is unique for each class
	// its like type id, simillar to what unreal uses
	
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

/*void FRDDSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RDDSceneProxy_GetDynamicMeshElements);
	check(IsInParallelRenderingThread());
	
	if (CVarRDDDrawPDI.GetValueOnAnyThread() == 0)
	{
		return;
	}
	
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (!(VisibilityMap & (1 << ViewIndex)))
		{
			continue;
		}
		
		// here doing it same as UE's ULineBatchComponent, 
		// so it draws in same pass and looks same
#if UE_ENABLE_DEBUG_DRAWING
		FPrimitiveDrawInterface* PDI = Collector.GetDebugPDI(ViewIndex);
#else 
		FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
#endif
		
		for (const FRDDLine& Line : Lines)
		{
			PDI->DrawTranslucentLine(Line.Start, Line.End, FLinearColor(Line.Color), 
				SDPG_World, /*thickness#1#0.f);
		}
	}
}*/

void FRDDSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RDDSceneProxy_GetDynamicMeshElements);
	check(IsInParallelRenderingThread());

	const int32 DrawPath = CVarRDDDrawPath.GetValueOnAnyThread();
	if (DrawPath == 0 || NumVertices == 0)
	{
		return;
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		if (!(VisibilityMap & (1 << ViewIndex)))
		{
			continue;
		}

		if (DrawPath == 1)
		{
			// Stage A: one PDI call per line, every frame.
#if UE_ENABLE_DEBUG_DRAWING
			FPrimitiveDrawInterface* PDI = Collector.GetDebugPDI(ViewIndex);
#else
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
#endif
			/*for (const FRDDLine& Line : Lines)
			{
				PDI->DrawTranslucentLine(Line.Start, Line.End, FLinearColor(Line.Color),
										 SDPG_World, 0.f);
			}*/
			for (const FRDDLine& Line : Lines)
			{
				if (Line.Start == Line.End) { continue; }   // released slot
				PDI->DrawTranslucentLine(Line.Start, Line.End, FLinearColor(Line.Color),
										 SDPG_World, 0.f);
			}
			continue;
		}

		// Persistent buffer: one description for every line.
		// No vertex data here — only pointers and a count.
		FMeshBatch& Mesh = Collector.AllocateMesh();
		Mesh.VertexFactory               = &VertexFactory;
		Mesh.MaterialRenderProxy         = MaterialProxy;
		Mesh.Type                        = PT_LineList;
		Mesh.DepthPriorityGroup          = SDPG_World;
		Mesh.CastShadow                  = false;
		Mesh.bCanApplyViewModeOverrides  = false;

		FMeshBatchElement& Element    = Mesh.Elements[0];
		Element.IndexBuffer            = &IndexBuffer;
		Element.FirstIndex             = 0;
		Element.NumPrimitives          = NumVertices / 2;
		Element.MinVertexIndex         = 0;
		Element.MaxVertexIndex         = NumVertices - 1;
		Element.PrimitiveUniformBuffer = GetUniformBuffer();

		Collector.AddMesh(ViewIndex, Mesh);
	}
}

FPrimitiveViewRelevance FRDDSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Relevance;
	Relevance.bDrawRelevance    = IsShown(View);
	Relevance.bDynamicRelevance = true;
	Relevance.bRenderInMainPass = ShouldRenderInMainPass();
	Relevance.bShadowRelevance  = false;

	MaterialRelevance.SetPrimitiveViewRelevance(Relevance);

	// Conservative, for the PDI path's translucent lines.
	Relevance.bSeparateTranslucency = Relevance.bNormalTranslucency = true;
	return Relevance;
}

uint32 FRDDSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

void FRDDSceneProxy::UpdateSlot_RenderThread(FRHICommandListImmediate& RHICmdList,
											 uint32 SlotIndex, const FRDDLine& Line)
{
	check(IsInRenderingThread());

	const uint32 V0 = SlotIndex * 2;
	if (V0 + 1 >= NumVertices)
	{
		// This proxy was built before the slot existed. A rebuild is
		// already queued, and it'll read the new data from the slot.
		return;
	}

	// Keep the render thread's copy in step, so the PDI path agrees too.
	Lines[SlotIndex] = Line;

	const FVector3f Positions[2] = { FVector3f(Line.Start), FVector3f(Line.End) };
	const FColor    Color        = ToVertexColor(Line.Color);
	const FColor    Colors[2]    = { Color, Color };

	WriteRange(RHICmdList, VertexBuffers.PositionVertexBuffer.VertexBufferRHI,
			   V0 * sizeof(FVector3f), Positions, sizeof(Positions));
	WriteRange(RHICmdList, VertexBuffers.ColorVertexBuffer.VertexBufferRHI,
			   V0 * sizeof(FColor), Colors, sizeof(Colors));

	// Tangents and UVs are fixed placeholders, so they never need rewriting.
	++PartialWrites;
	PartialBytesWritten += sizeof(Positions) + sizeof(Colors);
}

uint32 FRDDSceneProxy::GetAllocatedSize() const
{
	return FPrimitiveSceneProxy::GetAllocatedSize() + Lines.GetAllocatedSize();
}

void FRDDSceneProxy::BuildVertexBuffers()
{
	// bNeedsCPUAccess = false; the CPU-side copy is freed once it's been uploaded
	// so the data lives only on the GPU.
	
	TRACE_CPUPROFILER_EVENT_SCOPE(RDD_BuildVertexBuffers);
	
	VertexBuffers.PositionVertexBuffer.Init(NumVertices, /*bNeedsCPUAccess=*/false);
	VertexBuffers.ColorVertexBuffer.Init(NumVertices, /*bNeedsCPUAccess=*/false);
	VertexBuffers.StaticMeshVertexBuffer.Init(NumVertices, /*NumTexCoords=*/1, /*bNeedsCPUAccess=*/false);

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const FRDDLine& Line = Lines[i];
		const uint32 V0 = i * 2;
		const uint32 V1 = V0 + 1;

		// World-space positions, narrowed from double to float for the GPU.
		VertexBuffers.PositionVertexBuffer.VertexPosition(V0) = FVector3f(Line.Start);
		VertexBuffers.PositionVertexBuffer.VertexPosition(V1) = FVector3f(Line.End);

		const FColor VertexColor = ToVertexColor(Line.Color);
		VertexBuffers.ColorVertexBuffer.VertexColor(V0) = VertexColor;
		VertexBuffers.ColorVertexBuffer.VertexColor(V1) = VertexColor;
		
		// The standard vertex factory expects a tangent basis and a UV.
		// Lines use neither, so these are fixed placeholders.
		for (const uint32 V : { V0, V1 })
		{
			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(
				V, FVector3f::XAxisVector, FVector3f::YAxisVector, FVector3f::ZAxisVector);
			VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(V, 0, FVector2f::ZeroVector);
		}
	}
	
	// Queues the upload to the render thread. The constructor isn't on the
	// render thread, so GPU resources can't be created here directly.
	BeginInitResource(&VertexBuffers.PositionVertexBuffer);
	BeginInitResource(&VertexBuffers.ColorVertexBuffer);
	BeginInitResource(&VertexBuffers.StaticMeshVertexBuffer);
	
	// Lines are independent pairs, so the indices are just 0, 1, 2, 3...
	// 32-bit because 100k+ vertices won't fit in 16.
	IndexBuffer.Indices.SetNumUninitialized(NumVertices);
	for (uint32 i = 0; i < NumVertices; ++i)
	{
		IndexBuffer.Indices[i] = i;
	}
	BeginInitResource(&IndexBuffer);
	
	++UploadCount;
	UploadedVertices += NumVertices;
}

void FRDDSceneProxy::InitVertexFactory()
{
	// Binding reads the buffers' GPU views, which only exist once the
	// buffers are initialized so this must run on the render thread,
	// queued after the BeginInitResource calls above.
	//
	// Capturing 'this' is safe coz the proxy's deletion is queued later,
	// and render commands run in order.
	ENQUEUE_RENDER_COMMAND(RDDInitVertexFactory)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			FLocalVertexFactory::FDataType Data;
			VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(&VertexFactory, Data);
			VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(&VertexFactory, Data);
			VertexBuffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&VertexFactory, Data);
			VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(&VertexFactory, Data);

			VertexFactory.SetData(RHICmdList, Data);
			VertexFactory.InitResource(RHICmdList);
		});
}


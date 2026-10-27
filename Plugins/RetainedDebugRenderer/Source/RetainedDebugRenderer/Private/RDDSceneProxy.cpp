#include "RDDSceneProxy.h"

#include "RDDComponent.h"
#include "MeshElementCollector.h"
#include "PrimitiveDrawInterface.h"
#include "PrimitiveDrawingUtils.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "SceneView.h"
#include "UObject/GarbageCollectionSchema.h"

static TAutoConsoleVariable<int32> CVarRDDDrawPDI(
	TEXT("RDD.DrawPDI"), 1,
	TEXT("1 = draw via the PDI path. 0 = skip drawing, to isolate buffer memory."),
	ECVF_RenderThreadSafe);

FRDDSceneProxy::FRDDSceneProxy(const URDDComponent* InComponent)
	: FPrimitiveSceneProxy{InComponent}
	, Lines{InComponent->GetLines()}	// this is a full copy, not reference
{
	bWillEverBeLit = false;
	
	NumVertices = Lines.Num() * 2;
	if (NumVertices > 0)
	{
		BuildVertexBuffers();
	}

	UE_LOG(LogTemp, Warning, TEXT("RDD proxy created: %d lines, %u vertices"),
		   Lines.Num(), NumVertices);
}

FRDDSceneProxy::~FRDDSceneProxy()
{
	// Proxies are deleted on the render thread, after the renderer has
	// finished with them, so releasing GPU resources here is safe.
	check(IsInParallelRenderingThread());

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

void FRDDSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
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
				SDPG_World, /*thickness*/0.f);
		}
	}
}

FPrimitiveViewRelevance FRDDSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Relevance;
	Relevance.bDrawRelevance    = IsShown(View);
	Relevance.bDynamicRelevance = true;   // without this, GetDynamicMeshElements is never called
	Relevance.bSeparateTranslucency = Relevance.bNormalTranslucency = true;
	return Relevance;
}

uint32 FRDDSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

uint32 FRDDSceneProxy::GetAllocatedSize() const
{
	return FPrimitiveSceneProxy::GetAllocatedSize() + Lines.GetAllocatedSize();
}

void FRDDSceneProxy::BuildVertexBuffers()
{
	// bNeedsCPUAccess = false; the CPU-side copy is freed once it's been uploaded
	// so the data lives only on the GPU.
	
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

		VertexBuffers.ColorVertexBuffer.VertexColor(V0) = Line.Color;
		VertexBuffers.ColorVertexBuffer.VertexColor(V1) = Line.Color;

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
}


#include "RDDSceneProxy.h"

#include "RDDComponent.h"
#include "MeshElementCollector.h"
#include "PrimitiveDrawInterface.h"
#include "PrimitiveDrawingUtils.h"
#include "PrimitiveViewRelevance.h"
#include "SceneView.h"
#include "UObject/GarbageCollectionSchema.h"

FRDDSceneProxy::FRDDSceneProxy(const URDDComponent* InComponent)
	: FPrimitiveSceneProxy{InComponent}
	, Lines{InComponent->GetLines()}	// this is a full copy, not reference
{
	bWillEverBeLit = false;
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


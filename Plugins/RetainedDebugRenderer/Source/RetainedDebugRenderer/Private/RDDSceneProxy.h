#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "RDDTypes.h"
#include "StaticMeshResources.h"

class URDDComponent;

/**
 * This will have the Render-Thread copy of a URDDComponent's Lines.
 * Firstly, I draw using the PDI, so for now it rebuilds vertices every frame.
 */
 
class FRDDSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FRDDSceneProxy(const URDDComponent* InComponent);
	
	virtual ~FRDDSceneProxy() override;
	
	virtual SIZE_T GetTypeHash() const override;
	
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, 
										const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, 
										class FMeshElementCollector& Collector) const override;
	
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override;
	
private:
	uint32 GetAllocatedSize() const;
	
	void BuildVertexBuffers();

	/** SoA vertex data on the GPU: positions, colours, and placeholder tangents/UVs. */
	FStaticMeshVertexBuffers VertexBuffers;
	uint32 NumVertices = 0;
	
	TArray<FRDDLine> Lines;
};
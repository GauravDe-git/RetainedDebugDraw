#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "RDDTypes.h"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"        // FDynamicMeshIndexBuffer32
#include "LocalVertexFactory.h"
#include "MaterialShared.h"            // FMaterialRelevance
#include "StaticMeshResources.h"

#include <atomic>

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
	
	/** Total GPU buffer builds since startup, for the bench overlay. */
	static int32 GetUploadCount()     { return UploadCount.load(); }
	static int64 GetUploadedVertices() { return UploadedVertices.load(); }
	
private:
	uint32 GetAllocatedSize() const;
	
	void BuildVertexBuffers();
	
	void InitVertexFactory();

	FLocalVertexFactory          VertexFactory;
	FDynamicMeshIndexBuffer32    IndexBuffer;
	FMaterialRenderProxy*        MaterialProxy = nullptr;
	FMaterialRelevance           MaterialRelevance;

	/** SoA vertex data on the GPU: positions, colours, and placeholder tangents/UVs. */
	FStaticMeshVertexBuffers VertexBuffers;
	uint32 NumVertices = 0;
	
	TArray<FRDDLine> Lines;
	
	// Atomic because proxies can be constructed off the game thread.
	static std::atomic<int32> UploadCount;
	static std::atomic<int64> UploadedVertices;
};
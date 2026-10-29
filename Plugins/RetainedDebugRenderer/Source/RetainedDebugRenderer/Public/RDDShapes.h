#pragma once

#include "CoreMinimal.h"
#include "RDDTypes.h"

/**
 * Turns shapes into lines using exactly the same geometry as Unreal's
 * ULineBatchComponent, so a retained shape and a stock one match line for line.
 */
namespace RDDShapes
{
	/** Axis-aligned box: 12 lines. Extent is the half-size, as in DrawDebugBox. */
	RETAINEDDEBUGRENDERER_API void AppendBox(TArray<FRDDLine>& Out, const FVector& Center,
											 const FVector& Extent, const FColor& Color);

	/** Wire sphere: 2 × Segments × Segments lines, so 288 at 12 segments. Mirrors DrawDebugSphere. */
	RETAINEDDEBUGRENDERER_API void AppendSphere(TArray<FRDDLine>& Out, const FVector& Center,
												float Radius, int32 Segments, const FColor& Color);
}
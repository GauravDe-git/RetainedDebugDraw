#pragma once

#include "CoreMinimal.h"

/**
 * One debug line in world-space, as stored on the game thread.
 * Positions can be double precision here, narrowing to float can happen
 *  when vertices are built for the GPU
 */

struct FRDDLine
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FColor Color = FColor::White;
	
	FRDDLine() = default;
	
	FRDDLine(const FVector& InStart, const FVector& InEnd, const FColor& InColor)
		: Start{InStart}, End{InEnd}, Color{InColor}
	{
		
	}
};
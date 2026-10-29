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

/**
 * Names one submitted line. Index finds its slot; Generation detects
 * that the slot has been reused since this handle was issued.
 * Only the component can say whether a handle is still valid.
 */
struct FRDDHandle
{
	static constexpr uint32 InvalidIndex = MAX_uint32;

	uint32 Index      = InvalidIndex;
	uint32 Generation = 0;

	/** True if this handle was ever issued. Says nothing about whether it's still live. */
	bool IsSet() const { return Index != InvalidIndex; }
};
// Fill out your copyright notice in the Description page of Project Settings.

#include "RDDComponent.h"

#include "Engine/CollisionProfile.h"

// Sets default values for this component's properties
URDDComponent::URDDComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);

	// Same as ULineBatchComponent: we don't implement GetUsedMaterials().
	SetIgnoreStreamingManagerUpdate(true);
}

void URDDComponent::AddLines(TConstArrayView<FRDDLine> NewLines)
{
	check(IsInGameThread());

	Lines.Append(NewLines.GetData(), NewLines.Num());

	// Stage A: rebuild the whole proxy on any change.
	// This is the coarse path stage B and M3 exist to replace.
	MarkRenderStateDirty();
}

void URDDComponent::ClearLines()
{
	check(IsInGameThread());

	Lines.Reset();
	MarkRenderStateDirty();
}

int32 URDDComponent::GetNumLines() const
{
	check(IsInGameThread());
	return Lines.Num();
}

FPrimitiveSceneProxy* URDDComponent::CreateSceneProxy()
{
	// #14 replaces this with: return new FRDDSceneProxy(this);
	// Returning nullptr is valid — the component simply has nothing to render yet.
	return nullptr;
}

FBoxSphereBounds URDDComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Lines are stored in world space, so LocalToWorld is ignored.
	// Deliberately huge and constant: culling can't hide geometry while debugging,
	// and adding lines never needs a bounds update. Mirrors ULineBatchComponent
	// when bCalculateAccurateBounds is false.
	const FVector BoxExtent(HALF_WORLD_MAX);
	return FBoxSphereBounds(FVector::ZeroVector, BoxExtent, BoxExtent.Size());
}



#include "RDDComponent.h"

#include "RDDSceneProxy.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"

URDDComponent::URDDComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);

	// No textures to stream, so stay out of the texture streaming manager.
	SetIgnoreStreamingManagerUpdate(true);
}

// ---------------------------------------------------------------------------
// Slot bookkeeping
// ---------------------------------------------------------------------------

FRDDHandle URDDComponent::AddSlot(const FRDDLine& Line)
{
	uint32 Index;
	if (FreeSlots.Num() > 0)
	{
		Index = FreeSlots.Pop();
	}
	else
	{
		Index = static_cast<uint32>(Slots.AddDefaulted());
	}

	FSlot& Slot = Slots[Index];
	Slot.Line   = Line;
	Slot.bAlive = true;
	++NumAlive;

	FRDDHandle Handle;
	Handle.Index      = Index;
	Handle.Generation = Slot.Generation;
	return Handle;
}

FRDDLine URDDComponent::MakeRenderLine(const FSlot& Slot)
{
	return Slot.bAlive
		? Slot.Line
		// Released slot: a zero-length line, which should rasterise nothing.
		: FRDDLine(Slot.Line.Start, Slot.Line.Start, FColor::Transparent);
}

void URDDComponent::SyncSlotToRenderer(uint32 Index)
{
	// The current proxy doesn't have this slot — it's new, or the proxy was
	// built empty. Only a rebuild can add it.
	if (static_cast<int32>(Index) >= ProxySlotCapacity)
	{
		MarkRenderStateDirty();
		return;
	}

	// Otherwise send just this slot. Capturing the proxy pointer is safe:
	// its deletion is queued after this command, and render commands run in order.
	if (FRDDSceneProxy* Proxy = static_cast<FRDDSceneProxy*>(SceneProxy))
	{
		const FRDDLine RenderLine = MakeRenderLine(Slots[Index]);
		ENQUEUE_RENDER_COMMAND(RDDSyncSlot)(
			[Proxy, Index, RenderLine](FRHICommandListImmediate& RHICmdList)
			{
				Proxy->UpdateSlot_RenderThread(RHICmdList, Index, RenderLine);
			});
	}
}

// ---------------------------------------------------------------------------
// Single-line API: each change is a 32-byte write where possible
// ---------------------------------------------------------------------------

FRDDHandle URDDComponent::SubmitLine(const FRDDLine& Line)
{
	check(IsInGameThread());

	// Reusing a slot the proxy already has: a write. A brand-new slot: a rebuild to grow.
	const FRDDHandle Handle = AddSlot(Line);
	SyncSlotToRenderer(Handle.Index);
	return Handle;
}

bool URDDComponent::IsValidHandle(FRDDHandle Handle) const
{
	check(IsInGameThread());

	return Handle.IsSet()
		&& Slots.IsValidIndex(static_cast<int32>(Handle.Index))
		&& Slots[Handle.Index].bAlive
		&& Slots[Handle.Index].Generation == Handle.Generation;
}

bool URDDComponent::GetLine(FRDDHandle Handle, FRDDLine& OutLine) const
{
	if (!IsValidHandle(Handle)) { return false; }

	OutLine = Slots[Handle.Index].Line;
	return true;
}

bool URDDComponent::ReleaseLine(FRDDHandle Handle)
{
	check(IsInGameThread());
	if (!IsValidHandle(Handle)) { return false; }

	FSlot& Slot = Slots[Handle.Index];
	Slot.bAlive = false;
	++Slot.Generation;              // every handle to the old occupant is now stale
	FreeSlots.Push(Handle.Index);
	--NumAlive;

	if (NumAlive == 0)
	{
		// Last line gone: rebuild empty, rather than keep drawing invisible lines.
		MarkRenderStateDirty();
	}
	else
	{
		SyncSlotToRenderer(Handle.Index);
	}
	return true;
}

bool URDDComponent::UpdateLine(FRDDHandle Handle, const FRDDLine& NewLine)
{
	check(IsInGameThread());
	if (!IsValidHandle(Handle)) { return false; }

	// The slot stays the source of truth, so any later rebuild picks this up.
	Slots[Handle.Index].Line = NewLine;
	SyncSlotToRenderer(Handle.Index);
	return true;
}

// ---------------------------------------------------------------------------
// Bulk API: always a rebuild
// ---------------------------------------------------------------------------

void URDDComponent::AddLines(TConstArrayView<FRDDLine> NewLines, TArray<FRDDHandle>* OutHandles)
{
	check(IsInGameThread());
	if (NewLines.IsEmpty()) { return; }

	if (OutHandles)
	{
		OutHandles->Reserve(OutHandles->Num() + NewLines.Num());
	}

	for (const FRDDLine& Line : NewLines)
	{
		const FRDDHandle Handle = AddSlot(Line);
		if (OutHandles) { OutHandles->Add(Handle); }
	}

	// One rebuild, not one write per line: thousands of separate GPU writes would cost far more.
	MarkRenderStateDirty();
}

void URDDComponent::ClearLines()
{
	check(IsInGameThread());
	if (NumAlive == 0) { return; }

	// Release rather than empty the arrays: emptying would reset every
	// generation, so old handles would match new lines in reused slots.
	// Pushed in reverse so the next refill takes slots 0, 1, 2... in order.
	for (int32 i = Slots.Num() - 1; i >= 0; --i)
	{
		FSlot& Slot = Slots[i];
		if (Slot.bAlive)
		{
			Slot.bAlive = false;
			++Slot.Generation;
			FreeSlots.Push(static_cast<uint32>(i));
		}
	}
	NumAlive = 0;

	// One rebuild to an empty proxy, not one write per released line.
	MarkRenderStateDirty();
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

int32 URDDComponent::GetNumLines() const
{
	check(IsInGameThread());
	return NumAlive;
}

int32 URDDComponent::GetNumSlots() const
{
	check(IsInGameThread());
	return Slots.Num();
}

int32 URDDComponent::GetNumFreeSlots() const
{
	check(IsInGameThread());
	return FreeSlots.Num();
}

void URDDComponent::GetRenderLines(TArray<FRDDLine>& OutLines) const
{
	OutLines.Reset();

	// Nothing alive: draw nothing, rather than a buffer full of invisible
	// lines that would still cost GPU time in stock-mode measurements.
	if (NumAlive == 0) { return; }

	OutLines.SetNum(Slots.Num());
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		OutLines[i] = MakeRenderLine(Slots[i]);
	}
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

UMaterialInterface* URDDComponent::GetLineMaterial() const
{
	return GEngine ? GEngine->VertexColorMaterial : nullptr;
}

void URDDComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials,
                                     bool bGetDebugMaterials) const
{
	// The engine rebuilds our proxy when this material's shaders recompile,
	// so it has to know we use it.
	if (UMaterialInterface* Material = GetLineMaterial())
	{
		OutMaterials.Add(Material);
	}
}

FPrimitiveSceneProxy* URDDComponent::CreateSceneProxy()
{
	// Must match GetRenderLines: an empty component builds an empty proxy.
	ProxySlotCapacity = (NumAlive > 0) ? Slots.Num() : 0;
	return new FRDDSceneProxy(this);
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
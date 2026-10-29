#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "RDDTypes.h"
#include "RDDComponent.generated.h"

/**
 * Game-thread owner of retained debug lines.
 * Each line lives in a slot and is named by a generational handle.
 * Hands the renderer a scene proxy holding its own copy of the data.
 */
UCLASS(ClassGroup = (Rendering))
class RETAINEDDEBUGRENDERER_API URDDComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	URDDComponent();

	FRDDHandle SubmitLine(const FRDDLine& Line);
	/** Moves or recolours one line without rebuilding the proxy. */
	bool UpdateLine(FRDDHandle Handle, const FRDDLine& NewLine);
	bool       ReleaseLine(FRDDHandle Handle);
	bool       IsValidHandle(FRDDHandle Handle) const;
	bool       GetLine(FRDDHandle Handle, FRDDLine& OutLine) const;

	/** Bulk submit. Marks render state dirty once rather than per line. */
	void  AddLines(TConstArrayView<FRDDLine> NewLines, TArray<FRDDHandle>* OutHandles = nullptr);
	void  ClearLines();
	int32 GetNumLines() const;

	/**
	 * One entry per slot, in slot order, for the proxy. Released slots are
	 * zero-length lines so slot i keeps vertices 2i and 2i+1.
	 * No game-thread check: the proxy can be built off the game thread.
	 */
	void GetRenderLines(TArray<FRDDLine>& OutLines) const;

	/** The material lines are rendered with. Single source of truth for the proxy and GetUsedMaterials. */
	UMaterialInterface* GetLineMaterial() const;

	//~ UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials,
								  bool bGetDebugMaterials = false) const override;
	
	int32 GetNumSlots() const;
	int32 GetNumFreeSlots() const;

private:
	struct FSlot
	{
		FRDDLine Line;
		uint32   Generation = 0;
		bool     bAlive     = false;
	};

	/** Puts a line in a free slot, or a new one. Doesn't mark render state dirty. */
	FRDDHandle AddSlot(const FRDDLine& Line);
	
	/** The line the renderer should draw for a slot: itself, or zero-length if released. */
	static FRDDLine MakeRenderLine(const FSlot& Slot);

	/** Gets one slot to the renderer: a 32-byte write if the proxy has it, a rebuild if not. */
	void SyncSlotToRenderer(uint32 Index);

	/** How many slots the current proxy was built with. Set in CreateSceneProxy. */
	int32 ProxySlotCapacity = 0;

	TArray<FSlot>  Slots;
	TArray<uint32> FreeSlots;
	int32          NumAlive = 0;
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "RDDTypes.h"
#include "RDDComponent.generated.h"

/**
 * Game-thread owner of retained debug lines.
 * Hands the renderer a scene proxy holding its own copy of the data.
 */
UCLASS(ClassGroup = (Rendering))
class RETAINEDDEBUGRENDERER_API URDDComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URDDComponent();
	
	void  AddLines(TConstArrayView<FRDDLine> NewLines);
	void  ClearLines();
	int32 GetNumLines() const;
	
	/** Read by the scene proxy's constructor when it copies the data. */
	const TArray<FRDDLine>& GetLines() const { return Lines; }

	//~ UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private:
	TArray<FRDDLine> Lines;

};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RDDTypes.h"
#include "RDDSubsystem.generated.h"

class URDDComponent;

/**
 *  This will own the world's retained debug line component, so any code can submit lines
 *	without placing any actor in the level.
 */
UCLASS()
class RETAINEDDEBUGRENDERER_API URDDSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	
	void AddLines(TConstArrayView<FRDDLine> Lines);
	void ClearLines();
	int32 GetNumLines() const;
	
private:
	/** making it UPROP so garbage collection knows we holding it **/
	UPROPERTY(Transient)
	TObjectPtr<URDDComponent> Component;
};

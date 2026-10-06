// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RDDComponent.h"
#include "GameFramework/Actor.h"
#include "RDDTestActor.generated.h"

/** Temp Actor to test my URDDComponent before adding the subsystem **/
UCLASS()
class RETAINEDDEBUGRENDERER_API ARDDTestActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ARDDTestActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	
private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URDDComponent> LineComp;
	
	float Timer = 0.f;
	int32 SpokesAdded = 0;
	
};

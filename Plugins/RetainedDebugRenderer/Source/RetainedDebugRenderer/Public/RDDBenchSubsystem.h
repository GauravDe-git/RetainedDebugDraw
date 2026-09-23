// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RDDBenchSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class RETAINEDDEBUGRENDERER_API URDDBenchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	
	void SetStockLineCount(int32 Count);
private:
	void EmitStockLines();
	
	int32 StockLineCount = 0;
	
	static constexpr int32 RDDSeed = 12345; 
};

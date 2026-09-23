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
	void StartCapture(int32 NumFrames);
	void SnapToBenchCamera();
private:
	void EmitStockLines();
	void SampleFrame(float DeltaTime);
	void FinishCapture();
	
	int32 StockLineCount = 0;
	
	bool bCapturing = false;
	int32 FramesRemaining = 0;
	
	TArray<float> FrameSamples;
	TArray<float> GameSamples;
	TArray<float> RenderSamples; 
	TArray<float> GPUSamples;
	
	static constexpr int32 RDDSeed = 12345; 
};

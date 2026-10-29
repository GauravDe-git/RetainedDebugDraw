// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RDDTypes.h"
#include "RDDBenchSubsystem.generated.h"

/**
 * 
 */

enum class ERDDBenchMode : uint8
{
	Stock,     // DrawDebugLine every frame
	Retained,  // submitted once to my renderer
};

UCLASS()
class RETAINEDDEBUGRENDERER_API URDDBenchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	
	void SetStockLineCount(int32 Count);
	void SetStockBoxCount(int32 Count);
	void SetStockSphereCount(int32 Count);
	void ClearAll();
	
	void StartCapture(int32 NumFrames);
	void SnapToBenchCamera();
	
	void SetMode(ERDDBenchMode NewMode);
	
	void SetMoveOne(bool bEnable);
	
	void ReleaseFromMiddle(int32 Count);
	void SubmitExtra(int32 Count);
	
	void SetDemoSpheres(bool bEnable);
private:
	void EmitStockLines();
	void EmitStockBoxes();
	void EmitStockSpheres();
	
	void SampleFrame(float DeltaTime);
	void FinishCapture();
	
	int32 StockLineCount = 0;
	int32 StockBoxCount = 0;
	int32 StockSphereCount = 0;
	
	bool bCapturing = false;
	int32 FramesRemaining = 0;
	
	TArray<float> FrameSamples;
	TArray<float> GameSamples;
	TArray<float> RenderSamples; 
	TArray<float> GPUSamples;
	
	static constexpr int32 RDDSeed = 12345; 
	
	void RefreshRetainedLines();

	ERDDBenchMode Mode = ERDDBenchMode::Stock;
	
	TArray<FRDDHandle> RetainedHandles;
	bool  bMoveOne     = false;
	float MoveOneTime  = 0.f;
	
	int32 ExtraSubmitBatch = 0;
	
	void GatherDemoTargets();
	void EmitStockDemoSpheres();

	/** Centre and radius of every mesh that gets a demo sphere. */
	TArray<FSphere> DemoTargets;
};

// Fill out your copyright notice in the Description page of Project Settings.

#include "RDDBenchSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	void MeanAndStdDev(const TArray<float>& Samples, float& OutMean, float& OutStdDev)
	{
		OutMean = 0.f;
		OutStdDev = 0.f;
		if (Samples.Num() == 0) { return; }

		for (float S : Samples) { OutMean += S; }
		OutMean /= Samples.Num();

		if (Samples.Num() < 2) { return; }

		float SumSq = 0.f;
		for (float S : Samples)
		{
			const float D = S - OutMean;
			SumSq += D * D;
		}
		OutStdDev = FMath::Sqrt(SumSq / (Samples.Num() - 1));
	}
}

void URDDBenchSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	check(IsInGameThread());
	
	if (StockLineCount > 0)
	{
		EmitStockLines();
	}
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			1, 0.f, FColor::Yellow, 
			FString::Printf(TEXT("Retained Draw | mode : STOCK | lines: %d"), StockLineCount));
	}
	
	if (bCapturing)
	{
		SampleFrame(DeltaTime);
		if (--FramesRemaining <= 0)
		{
			FinishCapture();
		}
	}
}

TStatId URDDBenchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URDDBenchSubsystem, STATGROUP_Tickables); 
}

void URDDBenchSubsystem::SetStockLineCount(int32 Count)
{
	check(IsInGameThread()); 
	StockLineCount = FMath::Max(0, Count);
}

void URDDBenchSubsystem::StartCapture(int32 NumFrames)
{
	check(IsInGameThread());
	
	FrameSamples.Reset();
	GameSamples.Reset();
	RenderSamples.Reset();
	GPUSamples.Reset();
	
	FrameSamples.Reserve(NumFrames);
	GameSamples.Reserve(NumFrames);
	RenderSamples.Reserve(NumFrames);
	GPUSamples.Reserve(NumFrames);
	
	FramesRemaining = FMath::Max(1, NumFrames);
	bCapturing = true; 
	
	UE_LOG(LogTemp, Warning, TEXT("RDD capture started: %d frames"), FramesRemaining);
}

void URDDBenchSubsystem::SampleFrame(float DeltaTime)
{
	const float GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	const float RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	
	// Query GPU 0 cycle time and convert to milliseconds:
	const float GPUMs    = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
	
	// console command: state unit's Frame is max of the 3
	const float FrameMs = FMath::Max3(GameMs, RenderMs, GPUMs);
	
	FrameSamples.Add(DeltaTime * 1000.f);
	GameSamples.Add(GameMs);
	RenderSamples.Add(RenderMs);
	GPUSamples.Add(GPUMs);
}

void URDDBenchSubsystem::FinishCapture()
{
	bCapturing = false;

	float FrameMean, FrameSD, GameMean, GameSD, RenderMean, RenderSD, GPUMean, GPUSD;
	MeanAndStdDev(FrameSamples,  FrameMean,  FrameSD);
	MeanAndStdDev(GameSamples,   GameMean,   GameSD);
	MeanAndStdDev(RenderSamples, RenderMean, RenderSD);
	MeanAndStdDev(GPUSamples,    GPUMean,    GPUSD);

	const FString Dir = FPaths::ProjectDir() / TEXT("Docs/measurements");
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Path = Dir / TEXT("results.csv");

	if (!IFileManager::Get().FileExists(*Path))
	{
		FFileHelper::SaveStringToFile(
			TEXT("Timestamp,Mode,Count,Frames,")
			TEXT("FrameMean,FrameSD,GameMean,GameSD,")
			TEXT("RenderMean,RenderSD,GPUMean,GPUSD\n"),
			*Path);
	}

	const FString Row = FString::Printf(
		TEXT("%s,STOCK,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),
		*FDateTime::Now().ToString(),
		StockLineCount,
		FrameSamples.Num(),
		FrameMean, FrameSD, GameMean, GameSD,
		RenderMean, RenderSD, GPUMean, GPUSD);

	FFileHelper::SaveStringToFile(Row, *Path,
		FFileHelper::EEncodingOptions::AutoDetect,
		&IFileManager::Get(),
		EFileWrite::FILEWRITE_Append);

	UE_LOG(LogTemp, Warning,
		TEXT("RDD capture done: Frame %.3f +/- %.3f ms over %d frames -> %s"),
		FrameMean, FrameSD, FrameSamples.Num(), *Path);
}

void URDDBenchSubsystem::EmitStockLines()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RDD_EmitStockLines);
	
	UWorld* World = GetWorld();
	if (!World)
		return;
	
	FRandomStream Rand(RDDSeed);

	for (int i = 0; i < StockLineCount; ++i)
	{
		const FVector A = Rand.GetUnitVector() * Rand.FRandRange(200.f, 3000.f);
		const FVector B = A + Rand.GetUnitVector() * 150.f;
		
		DrawDebugLine(World, A, B, FColor(160,0,0),
					 /*bPersistentLines=*/false,
					 /*LifeTime=*/-1.f,
					 /*DepthPriority=*/0,
					 /*Thickness=*/0.f);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GRDDStressLines(
	TEXT("RDD.Stress.Lines"),
	TEXT("Emit N stock DrawDebugLine calls per frame. Usage: RDD.Stress.Lines <N>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) return;
			
			if (URDDBenchSubsystem* Sub = World->GetSubsystem<URDDBenchSubsystem>())
			{
				const int32 N = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
				Sub->SetStockLineCount(N);
				UE_LOG(LogTemp, Warning, TEXT("RDD.Stress.Lines -> %d"), N);
			}
		}
		)
);

static FAutoConsoleCommandWithWorldAndArgs GRDDBenchReport(
	TEXT("RDD.Bench.Report"),
	TEXT("Capture N frames and append mean/stddev to Docs/measurements/results.csv"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) { return; }
			if (URDDBenchSubsystem* Sub = World->GetSubsystem<URDDBenchSubsystem>())
			{
				const int32 N = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 300;
				Sub->StartCapture(N);
			}
		}));

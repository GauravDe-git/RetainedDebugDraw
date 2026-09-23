// Fill out your copyright notice in the Description page of Project Settings.

#include "RDDBenchSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

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

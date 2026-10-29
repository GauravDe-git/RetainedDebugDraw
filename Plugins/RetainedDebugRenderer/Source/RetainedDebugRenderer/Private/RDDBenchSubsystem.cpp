// Fill out your copyright notice in the Description page of Project Settings.

#include "RDDBenchSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "RDDSubsystem.h"
#include "RDDTypes.h"
#include "RDDSceneProxy.h"
#include "RDDShapes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

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
	
	/** One seeded bench line. Both modes call this, so both draw identical lines. */
	FRDDLine MakeBenchLine(FRandomStream& Rand)
	{
		// Separate statements on purpose: C++ doesn't define which operand of *
		// is evaluated first, so a one-line version could consume the random
		// stream in a different order on another compiler.
		const FVector Dir  = Rand.GetUnitVector();
		const float   Dist = Rand.FRandRange(200.f, 3000.f);
		const FVector A    = Dir * Dist;
		const FVector B    = A + Rand.GetUnitVector() * 150.f;

		return FRDDLine(A, B, FColor(160, 0, 0));
	}
	
	constexpr int32 SphereSegments = 12;
	const FColor    ShapeColor(170, 0, 0);
	const FColor DemoColor(0, 200, 0);

	struct FBenchShape
	{
		FVector Center;
		float   Size;   // box half-extent, or sphere radius
	};

	/** Shared by both modes and both shapes. Separate statements fix the random-stream order. */
	FBenchShape MakeBenchShape(FRandomStream& Rand)
	{
		const FVector Dir  = Rand.GetUnitVector();
		const float   Dist = Rand.FRandRange(200.f, 3000.f);
		const float   Size = Rand.FRandRange(20.f, 60.f);
		return { Dir * Dist, Size };
	}

	const TCHAR* ModeName(ERDDBenchMode Mode)
	{
		return Mode == ERDDBenchMode::Retained ? TEXT("RETAINED") : TEXT("STOCK");
	}
	
	/** Reads the proxy's RDD.DrawPath cvar, so captures record which path was active. */
	int32 GetDrawPath()
	{
		static IConsoleVariable* CVar =
			IConsoleManager::Get().FindConsoleVariable(TEXT("RDD.DrawPath"));
		return CVar ? CVar->GetInt() : -1;
	}
}
void URDDBenchSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	check(IsInGameThread());

	if (Mode == ERDDBenchMode::Stock)
	{
		if (StockLineCount > 0)   { EmitStockLines(); }
		if (StockBoxCount > 0)    { EmitStockBoxes(); }
		if (StockSphereCount > 0) { EmitStockSpheres(); }
		if (DemoTargets.Num() > 0) { EmitStockDemoSpheres(); }
	}

	// Spin line 0 through its handle: one line changes, the rest stay put.
	if (bMoveOne && Mode == ERDDBenchMode::Retained && RetainedHandles.Num() > 0)
	{
		MoveOneTime += DeltaTime;
		const FVector Dir(FMath::Cos(MoveOneTime), FMath::Sin(MoveOneTime), 0.f);
		const FRDDLine Spinner(FVector::ZeroVector, Dir * 1500.f, FColor::Yellow);

		if (URDDSubsystem* RDD = GetWorld() ? GetWorld()->GetSubsystem<URDDSubsystem>() : nullptr)
		{
			RDD->UpdateLine(RetainedHandles[0], Spinner);
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			1, 0.f, FColor::Yellow,
			FString::Printf(TEXT("Retained Draw | %s | Path:%d | L:%d B:%d S:%d"),
				ModeName(Mode), GetDrawPath(), StockLineCount, StockBoxCount, StockSphereCount));

		GEngine->AddOnScreenDebugMessage(
			2, 0.f, FColor::Cyan,
			FString::Printf(TEXT("RDD buffer uploads: %d (%lld vertices total)"),
				FRDDSceneProxy::GetUploadCount(), FRDDSceneProxy::GetUploadedVertices()));

		GEngine->AddOnScreenDebugMessage(
			3, 0.f, FColor::Cyan,
			FString::Printf(TEXT("RDD partial writes: %lld (%lld bytes total)"),
				FRDDSceneProxy::GetPartialWrites(), FRDDSceneProxy::GetPartialBytesWritten()));
		
		if (URDDSubsystem* RDD = GetWorld() ? GetWorld()->GetSubsystem<URDDSubsystem>() : nullptr)
		{
			const int32 NumSlots = RDD->GetNumSlots();
			const int32 NumFree  = RDD->GetNumFreeSlots();
			const float FragPct  = NumSlots > 0 ? 100.f * NumFree / NumSlots : 0.f;

			GEngine->AddOnScreenDebugMessage(
				4, 0.f, FColor::Cyan,
				FString::Printf(TEXT("RDD slots: %d total, %d live, %d free (%.1f%% free)"),
					NumSlots, RDD->GetNumLines(), NumFree, FragPct));
		}
	}

	if (bCapturing)
	{
		SampleFrame(DeltaTime);
		if (FramesRemaining % 50 == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("RDD capture: %d frames left"), FramesRemaining);
		}
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
	RefreshRetainedLines();
}

void URDDBenchSubsystem::SetStockBoxCount(int32 Count)
{
	check(IsInGameThread());
	StockBoxCount = FMath::Max(0, Count);
	RefreshRetainedLines();
}

void URDDBenchSubsystem::SetStockSphereCount(int32 Count)
{
	check(IsInGameThread());
	StockSphereCount = FMath::Max(0, Count);
	RefreshRetainedLines();
}

void URDDBenchSubsystem::ClearAll()
{
	check(IsInGameThread());
	StockLineCount = 0;
	StockBoxCount = 0;
	StockSphereCount = 0;

	if (UWorld* World = GetWorld())
	{
		FlushPersistentDebugLines(World);
	}
	
	RefreshRetainedLines();
}

void URDDBenchSubsystem::EmitStockBoxes()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RDD_EmitStockBoxes);

	UWorld* World = GetWorld();
	if (!World) { return; }

	FRandomStream Rand(RDDSeed);
	for (int32 i = 0; i < StockBoxCount; ++i)
	{
		const FBenchShape Shape = MakeBenchShape(Rand);
		DrawDebugBox(World, Shape.Center, FVector(Shape.Size), ShapeColor,
					 /*bPersistentLines=*/false, /*LifeTime=*/-1.f,
					 /*DepthPriority=*/0, /*Thickness=*/0.f);
	}
}

void URDDBenchSubsystem::EmitStockSpheres()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RDD_EmitStockSpheres);

	UWorld* World = GetWorld();
	if (!World) { return; }

	FRandomStream Rand(RDDSeed);
	for (int32 i = 0; i < StockSphereCount; ++i)
	{
		const FBenchShape Shape = MakeBenchShape(Rand);
		DrawDebugSphere(World, Shape.Center, Shape.Size, SphereSegments, ShapeColor,
						/*bPersistentLines=*/false, /*LifeTime=*/-1.f,
						/*DepthPriority=*/0, /*Thickness=*/0.f);
	}
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

void URDDBenchSubsystem::SnapToBenchCamera()
{
	check(IsInGameThread());
	
	UWorld* World = GetWorld();
	if (!World) return;
	
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("RDD: no player controller yet"));
		return;
	}
	
	ACameraActor* BenchCam = nullptr;
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("RDDBenchCam")))
		{
			BenchCam = *It;
			break;
		}
	}
	if (!BenchCam)
	{
		UE_LOG(LogTemp, Error, TEXT("RDD: no CameraActor tagged 'RDDBenchCam' found"));
		return;
	}
	
	PC->SetViewTargetWithBlend(BenchCam, 0.f);
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	
	const FVector Loc = BenchCam->GetActorLocation();
	const FRotator Rot = BenchCam->GetActorRotation();
	UE_LOG(LogTemp, Warning, TEXT("RDD: camera locked at %s / %s"),
		   *Loc.ToString(), *Rot.ToString());
}

void URDDBenchSubsystem::SampleFrame(float DeltaTime)
{
	const float GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	const float RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	
	// Query GPU 0 cycle time and convert to milliseconds:
	const float GPUMs    = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
	
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
			TEXT("Timestamp,Mode,DrawPath,Lines,Boxes,Spheres,Frames,")
			TEXT("FrameMean,FrameSD,GameMean,GameSD,")
			TEXT("RenderMean,RenderSD,GPUMean,GPUSD\n"),
			*Path);
	}

	const FString Row = FString::Printf(
		TEXT("%s,%s,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),
		*FDateTime::Now().ToString(),
		ModeName(Mode),
		GetDrawPath(),
		StockLineCount,
		StockBoxCount,
		StockSphereCount,
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

	/*for (int i = 0; i < StockLineCount; ++i)
	{
		const FVector A = Rand.GetUnitVector() * Rand.FRandRange(200.f, 3000.f);
		const FVector B = A + Rand.GetUnitVector() * 150.f;
		
		DrawDebugLine(World, A, B, FColor(160,0,0),
					 /*bPersistentLines=#1#false,
					 /*LifeTime=#1#-1.f,
					 /*DepthPriority=#1#0,
					 /*Thickness=#1#0.f);
	}*/
	for (int32 i = 0; i < StockLineCount; ++i)
	{
		const FRDDLine Line = MakeBenchLine(Rand);
		DrawDebugLine(World, Line.Start, Line.End, Line.Color,
					  /*bPersistentLines=*/false, /*LifeTime=*/-1.f,
					  /*DepthPriority=*/0, /*Thickness=*/0.f);
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

void URDDBenchSubsystem::SetMode(ERDDBenchMode NewMode)
{
	check(IsInGameThread());
	Mode = NewMode;
	RefreshRetainedLines();
}

void URDDBenchSubsystem::SetMoveOne(bool bEnable)
{
	check(IsInGameThread());
	bMoveOne    = bEnable;
	MoveOneTime = 0.f;
}

void URDDBenchSubsystem::ReleaseFromMiddle(int32 Count)
{
	check(IsInGameThread());

	URDDSubsystem* RDD = GetWorld() ? GetWorld()->GetSubsystem<URDDSubsystem>() : nullptr;
	if (!RDD || Mode != ERDDBenchMode::Retained) { return; }

	Count = FMath::Clamp(Count, 0, RetainedHandles.Num());
	const int32 Start = (RetainedHandles.Num() - Count) / 2;

	for (int32 i = Start; i < Start + Count; ++i)
	{
		RDD->ReleaseLine(RetainedHandles[i]);
	}
	RetainedHandles.RemoveAt(Start, Count);
}

void URDDBenchSubsystem::SubmitExtra(int32 Count)
{
	check(IsInGameThread());

	URDDSubsystem* RDD = GetWorld() ? GetWorld()->GetSubsystem<URDDSubsystem>() : nullptr;
	if (!RDD || Mode != ERDDBenchMode::Retained) { return; }

	// A different seed per batch so these are new lines, in yellow so they stand out.
	FRandomStream Rand(RDDSeed + 1 + ExtraSubmitBatch++);
	for (int32 i = 0; i < Count; ++i)
	{
		FRDDLine Line = MakeBenchLine(Rand);
		Line.Color = FColor::Yellow;
		RetainedHandles.Add(RDD->SubmitLine(Line));
	}
}

void URDDBenchSubsystem::RefreshRetainedLines()
{
	check(IsInGameThread());

	UWorld* World = GetWorld();
	URDDSubsystem* RDD = World ? World->GetSubsystem<URDDSubsystem>() : nullptr;
	if (!RDD) { return; }

	// Always clear first: AddLines appends, so re-submitting without
	// clearing would draw everything twice.
	RDD->ClearLines();

	// Clearing released every line, so the old handles are all stale now.
	RetainedHandles.Reset();

	if (Mode != ERDDBenchMode::Retained) { return; }

	const int32 TotalLines = StockLineCount
					   + StockBoxCount    * 12
					   + StockSphereCount * 2 * SphereSegments * SphereSegments
					   + DemoTargets.Num() * 2 * SphereSegments * SphereSegments;
	if (TotalLines == 0) { return; }

	TArray<FRDDLine> Lines;
	Lines.Reserve(TotalLines);

	// Each shape type uses its own stream from the same seed — exactly as the
	// stock emit functions do — so both modes generate identical shapes.
	{
		FRandomStream Rand(RDDSeed);
		for (int32 i = 0; i < StockLineCount; ++i)
		{
			Lines.Add(MakeBenchLine(Rand));
		}
	}
	{
		FRandomStream Rand(RDDSeed);
		for (int32 i = 0; i < StockBoxCount; ++i)
		{
			const FBenchShape Shape = MakeBenchShape(Rand);
			RDDShapes::AppendBox(Lines, Shape.Center, FVector(Shape.Size), ShapeColor);
		}
	}
	{
		FRandomStream Rand(RDDSeed);
		for (int32 i = 0; i < StockSphereCount; ++i)
		{
			const FBenchShape Shape = MakeBenchShape(Rand);
			RDDShapes::AppendSphere(Lines, Shape.Center, Shape.Size, SphereSegments, ShapeColor);
		}
	}
	
	for (const FSphere& Target : DemoTargets)
	{
		RDDShapes::AppendSphere(Lines, Target.Center, static_cast<float>(Target.W),
								SphereSegments, DemoColor);
	}

	// Keep a handle for every line, so individual lines can be moved later.
	RDD->AddLines(Lines, &RetainedHandles);
}

void URDDBenchSubsystem::GatherDemoTargets()
{
	DemoTargets.Reset();

	UWorld* World = GetWorld();
	if (!World) { return; }

	// Anything bigger than this is scenery — floor, sky — not something to mark.
	constexpr double MaxRadius = 500.0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UStaticMeshComponent*> Components(*It);
		for (UStaticMeshComponent* Comp : Components)
		{
			if (!Comp || !Comp->GetStaticMesh()) { continue; }

			if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Comp))
			{
				// Foliage and other instanced meshes: one sphere per instance.
				const FBoxSphereBounds LocalBounds = ISM->GetStaticMesh()->GetBounds();
				for (int32 i = 0; i < ISM->GetInstanceCount(); ++i)
				{
					FTransform Instance;
					if (!ISM->GetInstanceTransform(i, Instance, /*bWorldSpace=*/true)) { continue; }

					const double Radius = LocalBounds.SphereRadius * Instance.GetMaximumAxisScale();
					if (Radius <= MaxRadius)
					{
						DemoTargets.Emplace(Instance.TransformPosition(LocalBounds.Origin), Radius);
					}
				}
			}
			else if (Comp->Bounds.SphereRadius <= MaxRadius)
			{
				// A normal placed mesh: its world bounds are already computed.
				DemoTargets.Emplace(Comp->Bounds.Origin, Comp->Bounds.SphereRadius);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("RDD demo: %d targets"), DemoTargets.Num());
}

void URDDBenchSubsystem::EmitStockDemoSpheres()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	for (const FSphere& Target : DemoTargets)
	{
		DrawDebugSphere(World, Target.Center, static_cast<float>(Target.W), SphereSegments, DemoColor,
						/*bPersistentLines=*/false, /*LifeTime=*/-1.f,
						/*DepthPriority=*/0, /*Thickness=*/0.f);
	}
}

void URDDBenchSubsystem::SetDemoSpheres(bool bEnable)
{
	check(IsInGameThread());

	if (bEnable) { GatherDemoTargets(); }
	else         { DemoTargets.Reset(); }

	RefreshRetainedLines();
}

static FAutoConsoleCommandWithWorldAndArgs GRDDStressBoxes(
	TEXT("RDD.Stress.Boxes"),
	TEXT("Emit N stock DrawDebugBox calls per frame. Usage: RDD.Stress.Boxes <N>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) { return; }
			if (URDDBenchSubsystem* Sub = World->GetSubsystem<URDDBenchSubsystem>())
			{
				const int32 N = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
				Sub->SetStockBoxCount(N);
				UE_LOG(LogTemp, Warning, TEXT("RDD.Stress.Boxes -> %d"), N);
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GRDDStressSpheres(
	TEXT("RDD.Stress.Spheres"),
	TEXT("Emit N stock DrawDebugSphere calls per frame. Usage: RDD.Stress.Spheres <N>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) { return; }
			if (URDDBenchSubsystem* Sub = World->GetSubsystem<URDDBenchSubsystem>())
			{
				const int32 N = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
				Sub->SetStockSphereCount(N);
				UE_LOG(LogTemp, Warning, TEXT("RDD.Stress.Spheres -> %d"), N);
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GRDDStressClear(
	TEXT("RDD.Stress.Clear"),
	TEXT("Clear all stock shape counts and flush persistent debug lines."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) { return; }
			if (URDDBenchSubsystem* Sub = World->GetSubsystem<URDDBenchSubsystem>())
			{
				Sub->ClearAll();
				UE_LOG(LogTemp, Warning, TEXT("RDD.Stress.Clear"));
			}
		}));

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

static FAutoConsoleCommandWithWorldAndArgs GRDDBenchSetCamera(
	TEXT("RDD.Bench.SetCamera"),
	TEXT("Snap to the CameraActor tagged RDDBenchCam and lock input."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) { return; }
			if (URDDBenchSubsystem* Sub = World->GetSubsystem<URDDBenchSubsystem>())
			{
				Sub->SnapToBenchCamera();
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GRDDStressMode(
	TEXT("RDD.Stress.Mode"),
	TEXT("0 = stock DrawDebugLine every frame, 1 = retained renderer. Lines only."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) { return; }
			if (URDDBenchSubsystem* Sub = World->GetSubsystem<URDDBenchSubsystem>())
			{
				const int32 M = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
				Sub->SetMode(M == 1 ? ERDDBenchMode::Retained : ERDDBenchMode::Stock);
				UE_LOG(LogTemp, Warning, TEXT("RDD.Stress.Mode -> %s"),
					   M == 1 ? TEXT("RETAINED") : TEXT("STOCK"));
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GRDDStressMoveOne(
	TEXT("RDD.Stress.MoveOne"),
	TEXT("1 = spin one retained line every frame through its handle, 0 = stop."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) { return; }
			if (URDDBenchSubsystem* Sub = World->GetSubsystem<URDDBenchSubsystem>())
			{
				const bool bEnable = Args.Num() > 0 && FCString::Atoi(*Args[0]) != 0;
				Sub->SetMoveOne(bEnable);
				UE_LOG(LogTemp, Warning, TEXT("RDD.Stress.MoveOne -> %d"), bEnable ? 1 : 0);
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GRDDStressRelease(
	TEXT("RDD.Stress.Release"),
	TEXT("Release N retained lines from the middle of the list. Usage: RDD.Stress.Release <N>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) { return; }
			if (URDDBenchSubsystem* Sub = World->GetSubsystem<URDDBenchSubsystem>())
			{
				const int32 N = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
				Sub->ReleaseFromMiddle(N);
				UE_LOG(LogTemp, Warning, TEXT("RDD.Stress.Release -> %d"), N);
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GRDDStressSubmit(
	TEXT("RDD.Stress.Submit"),
	TEXT("Submit N new yellow lines through handles. Usage: RDD.Stress.Submit <N>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) { return; }
			if (URDDBenchSubsystem* Sub = World->GetSubsystem<URDDBenchSubsystem>())
			{
				const int32 N = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
				Sub->SubmitExtra(N);
				UE_LOG(LogTemp, Warning, TEXT("RDD.Stress.Submit -> %d"), N);
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GRDDDemoSpheres(
	TEXT("RDD.Demo.Spheres"),
	TEXT("1 = a debug sphere on every small mesh and instance in the level, 0 = remove."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) { return; }
			if (URDDBenchSubsystem* Sub = World->GetSubsystem<URDDBenchSubsystem>())
			{
				const bool bEnable = Args.Num() > 0 && FCString::Atoi(*Args[0]) != 0;
				Sub->SetDemoSpheres(bEnable);
			}
		}));
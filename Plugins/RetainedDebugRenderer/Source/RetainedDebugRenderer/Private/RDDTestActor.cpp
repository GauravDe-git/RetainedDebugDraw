// Fill out your copyright notice in the Description page of Project Settings.


#include "RDDTestActor.h"


// Sets default values
ARDDTestActor::ARDDTestActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	LineComp = CreateDefaultSubobject<URDDComponent>(TEXT("LineComp"));
	RootComponent = LineComp;
}

// Called when the game starts or when spawned
void ARDDTestActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Gonna added 3 axis lines, to easily check if pos and color right
	const FVector O = GetActorLocation();
	const FRDDLine Axes[] =
	{
		FRDDLine(O, O + FVector(200, 0, 0), FColor::Red),
		FRDDLine(O, O + FVector(0, 200, 0), FColor::Green),
		FRDDLine(O, O + FVector(0, 0, 200), FColor::Blue),
	};
	LineComp->AddLines(MakeArrayView(Axes));
}

// Called every frame
void ARDDTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Add one yellow spoke per second, 12 in total.
	// Tests that adding lines later rebuilds the proxy and shows up.
	Timer += DeltaTime;
	if (Timer >= 1.f && SpokesAdded < 12)
	{
		Timer = 0.f;

		const float Angle = SpokesAdded * (2.f * PI / 12.f);
		const FVector O = GetActorLocation();
		const FVector Tip = O + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 300.f;

		const FRDDLine Spoke(O, Tip, FColor::Yellow);
		LineComp->AddLines(MakeArrayView(&Spoke, 1));

		++SpokesAdded;
	}
}


// Fill out your copyright notice in the Description page of Project Settings.

#include "RDDSubsystem.h"

#include "RDDComponent.h"
#include "Engine/World.h"
bool URDDSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Only real game worlds, not the editor world or asset preview scenes.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URDDSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	check(IsInGameThread());
	
	// No owning actor, component registered straight into world
	// using same pattern as Uworld's line batchers
	
	Component = NewObject<URDDComponent>(this, TEXT("RDDComponent"));
	Component->RegisterComponentWithWorld(&InWorld);
}

void URDDSubsystem::Deinitialize()
{
	if (Component && Component->IsRegistered())
	{
		Component->UnregisterComponent();
	}
	Component = nullptr;

	Super::Deinitialize();
}

void URDDSubsystem::AddLines(TConstArrayView<FRDDLine> Lines)
{
	check(IsInGameThread());

	if (!Component)
	{
		UE_LOG(LogTemp, Warning, TEXT("RDD: AddLines called before world begin play: ignored"));
		return;
	}
	Component->AddLines(Lines);
}

void URDDSubsystem::ClearLines()
{
	check(IsInGameThread());

	if (Component)
	{
		Component->ClearLines();
	}
}

int32 URDDSubsystem::GetNumLines() const
{
	check(IsInGameThread());
	return Component ? Component->GetNumLines() : 0;
}

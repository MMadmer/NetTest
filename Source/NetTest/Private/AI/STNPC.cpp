// NetTest project. All rights reserved.


#include "AI/STNPC.h"

#include "Components/NTMovementComponent.h"

ANTNPC::ANTNPC(const FObjectInitializer& ObjInit) : Super(
	ObjInit.SetDefaultSubobjectClass<UNTMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ANTNPC::BeginPlay()
{
	Super::BeginPlay();
}

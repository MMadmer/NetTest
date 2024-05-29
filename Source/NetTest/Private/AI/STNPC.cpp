// NetTest project. All rights reserved.


#include "AI/STNPC.h"

#include "AIController.h"
#include "Components/NTMovementComponent.h"

ANTNPC::ANTNPC(const FObjectInitializer& ObjInit) : Super(
	ObjInit.SetDefaultSubobjectClass<UNTMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

FORCEINLINE bool ANTNPC::GetIsFocused() const
{
	const auto AIController = Cast<AAIController>(Controller);
	return AIController && IsValid(AIController->GetFocusActor());
}

void ANTNPC::BeginPlay()
{
	Super::BeginPlay();
}

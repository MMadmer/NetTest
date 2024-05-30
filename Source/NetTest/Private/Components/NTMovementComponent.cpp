// NetTest project. All rights reserved.


#include "Components/NTMovementComponent.h"

#include "Net/UnrealNetwork.h"

UNTMovementComponent::UNTMovementComponent()
{
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
	bWantsToRun = false;
}

void UNTMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNTMovementComponent, bWantsToRun);
	DOREPLIFETIME(UNTMovementComponent, RunModifier);
}

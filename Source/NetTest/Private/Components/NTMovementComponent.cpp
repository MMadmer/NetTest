// NetTest project. All rights reserved.


#include "Components/NTMovementComponent.h"

#include "Net/UnrealNetwork.h"

UNTMovementComponent::UNTMovementComponent()
{
	SetIsReplicated(true);
	bWantsToRun = false;
}

void UNTMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNTMovementComponent, bWantsToRun);
	DOREPLIFETIME(UNTMovementComponent, RunModifier);
}

FORCEINLINE float UNTMovementComponent::GetMaxSpeed() const
{
	return IsRunning() ? Super::GetMaxSpeed() * RunModifier : Super::GetMaxSpeed();
}

FORCEINLINE bool UNTMovementComponent::IsRunning() const
{
	return bWantsToRun && !IsFalling() && Velocity.Size() > 0.0f;
}

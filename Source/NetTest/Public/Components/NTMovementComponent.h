// NetTest project. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NTMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class NETTEST_API UNTMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UNTMovementComponent();

	FORCEINLINE virtual float GetMaxSpeed() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadWrite, Replicated)
	uint8 bWantsToRun : 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Replicated)
	float RunModifier = 2.0f;

	UFUNCTION(BlueprintCallable)
	FORCEINLINE bool IsRunning() const;
};


FORCEINLINE float UNTMovementComponent::GetMaxSpeed() const
{
	return IsRunning() ? Super::GetMaxSpeed() * RunModifier : Super::GetMaxSpeed();
}

FORCEINLINE bool UNTMovementComponent::IsRunning() const
{
	return bWantsToRun && !IsFalling() && Velocity.Size() > 0.0f;
}

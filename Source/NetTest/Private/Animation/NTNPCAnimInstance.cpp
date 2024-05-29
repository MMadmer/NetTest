// NetTest project. All rights reserved.


#include "Animation/NTNPCAnimInstance.h"

#include "AI/STNPC.h"
#include "Components/NTMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

UNTNPCAnimInstance::UNTNPCAnimInstance(): NPC(nullptr)
{
	bIsFocused = false;
	bIsInAir = false;
	bIsRunning = false;
}

void UNTNPCAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	NPC = Cast<ANTNPC>(TryGetPawnOwner());
}

void UNTNPCAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!NPC) NPC = Cast<ANTNPC>(TryGetPawnOwner());
	if (!NPC) return;

	bIsFocused = NPC->GetIsFocused();

	if (const auto MovementComp = Cast<UNTMovementComponent>(NPC->GetCharacterMovement()))
	{
		bIsInAir = MovementComp->IsFalling();
		bIsRunning = MovementComp->IsRunning();
	}

	Speed = NPC->GetVelocity().Size();

	// Yaw offset
	const FRotator AimRotation = NPC->GetActorForwardVector().Rotation();
	const FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(NPC->GetVelocity());

	const FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaSeconds, 5.0f);
	YawOffset = DeltaRotation.Yaw;
}

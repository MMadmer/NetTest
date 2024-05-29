// NetTest project. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "NTNPCAnimInstance.generated.h"

class ANTNPC;
/**
 * 
 */
UCLASS()
class NETTEST_API UNTNPCAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UNTNPCAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly)
	uint8 bIsFocused : 1;

	UPROPERTY(BlueprintReadOnly)
	uint8 bIsInAir : 1;

	UPROPERTY(BlueprintReadOnly)
	uint8 bIsRunning : 1;

	UPROPERTY(BlueprintReadOnly)
	float YawOffset = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	ANTNPC* NPC;

private:
	FRotator DeltaRotation;
};

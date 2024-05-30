// NetTest project. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "STNPC.generated.h"

UCLASS()
class NETTEST_API ANTNPC : public ACharacter
{
	GENERATED_BODY()

public:
	ANTNPC(const FObjectInitializer& ObjInit);

	FORCEINLINE bool GetIsFocused() const;

protected:
	virtual void BeginPlay() override;
};

FORCEINLINE bool ANTNPC::GetIsFocused() const
{
	const auto AIController = Cast<AAIController>(Controller);
	return AIController && IsValid(AIController->GetFocusActor());
}

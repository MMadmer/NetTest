// NetTest project. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "STNPC.generated.h"

UCLASS()
class NETTEST_API ANTNPC : public ACharacter
{
	GENERATED_BODY()

public:
	ANTNPC(const FObjectInitializer& ObjInit);

	bool GetIsFocused() const;

protected:
	virtual void BeginPlay() override;
};

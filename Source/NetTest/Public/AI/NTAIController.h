// NetTest project. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "NTAIController.generated.h"

/**
 * 
 */
UCLASS()
class NETTEST_API ANTAIController : public AAIController
{
	GENERATED_BODY()

public:
	ANTAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	TArray<APawn*> UsedPlayers;

	UPROPERTY(Replicated)
	APawn* LastTarget;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI")
	UBehaviorTree* BehaviorTree;
};

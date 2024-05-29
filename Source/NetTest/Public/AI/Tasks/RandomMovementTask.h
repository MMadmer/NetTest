// NetTest project. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "RandomMovementTask.generated.h"

/**
 * 
 */
UCLASS()
class NETTEST_API URandomMovementTask : public UBTTaskNode
{
	GENERATED_BODY()

public:
	URandomMovementTask();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
	FTimerHandle MovementTimerHandle;
	const float MaxMovementDuration = 8.0f;
	uint8 bIsTimedOut : 1;

	void SafeFinish(UBehaviorTreeComponent& OwnerComp, const EBTNodeResult::Type& Type);
};

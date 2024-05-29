// NetTest project. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "FindTargetTask.generated.h"

/**
 * 
 */
UCLASS()
class NETTEST_API UFindTargetTask : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UFindTargetTask();
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	TArray<APawn*> GetCurrentPlayers() const;
};

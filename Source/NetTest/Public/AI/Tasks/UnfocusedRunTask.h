// NetTest project. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "UnfocusedRunTask.generated.h"

/**
 * 
 */
UCLASS()
class NETTEST_API UUnfocusedRunTask : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UUnfocusedRunTask();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

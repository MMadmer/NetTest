// NetTest project. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "FocusedStrafeTask.generated.h"

/**
 * 
 */
UCLASS()
class NETTEST_API UFocusedStrafeTask : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UFocusedStrafeTask();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

// NetTest project. All rights reserved.


#include "AI/Tasks/FocusedStrafeTask.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/NTMovementComponent.h"

UFocusedStrafeTask::UFocusedStrafeTask()
{
	NodeName = TEXT("Focused Strafe");
}

EBTNodeResult::Type UFocusedStrafeTask::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const auto Controller = OwnerComp.GetAIOwner();
	if (!Controller) return EBTNodeResult::Failed;

	const auto Blackboard = Controller->GetBlackboardComponent();
	if (!Blackboard) return EBTNodeResult::Failed;

	const auto Pawn = Controller->GetPawn();
	if (!Pawn) return EBTNodeResult::Failed;

	const auto MovementComp = Cast<
		UNTMovementComponent>(Pawn->GetComponentByClass(UNTMovementComponent::StaticClass()));
	if (!MovementComp) return EBTNodeResult::Failed;

	Controller->SetFocus(Cast<AActor>(Blackboard->GetValueAsObject(TEXT("Target"))));
	MovementComp->bWantsToRun = false;

	return EBTNodeResult::Succeeded;
}

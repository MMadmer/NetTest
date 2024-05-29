// NetTest project. All rights reserved.


#include "AI/Tasks/UnfocusedRunTask.h"

#include "AIController.h"
#include "Components/NTMovementComponent.h"

UUnfocusedRunTask::UUnfocusedRunTask()
{
	NodeName = TEXT("Unfocused Run");
}

EBTNodeResult::Type UUnfocusedRunTask::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const auto Controller = OwnerComp.GetAIOwner();
	if (!Controller) return EBTNodeResult::Failed;

	const auto Pawn = Controller->GetPawn();
	if (!Pawn) return EBTNodeResult::Failed;

	const auto MovementComp = Cast<
		UNTMovementComponent>(Pawn->GetComponentByClass(UNTMovementComponent::StaticClass()));
	if (!MovementComp) return EBTNodeResult::Failed;

	Controller->ClearFocus(EAIFocusPriority::Gameplay);
	MovementComp->bWantsToRun = true;

	return EBTNodeResult::Succeeded;
}

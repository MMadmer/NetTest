// NetTest project. All rights reserved.


#include "AI/Tasks/RandomMovementTask.h"

#include "AIController.h"
#include "NavigationSystem.h"

URandomMovementTask::URandomMovementTask()
{
	bNotifyTick = true;
	NodeName = TEXT("Random Movement");

	bIsTimedOut = false;
}

EBTNodeResult::Type URandomMovementTask::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const auto Controller = OwnerComp.GetAIOwner();
	if (!Controller) return EBTNodeResult::Failed;

	const auto Pawn = Controller->GetPawn();
	if (!Pawn) return EBTNodeResult::Failed;

	const auto NavSys = UNavigationSystemV1::GetCurrent(Pawn);
	if (!NavSys) return EBTNodeResult::Failed;

	FNavLocation NavLocation{};
	if (!NavSys->GetRandomReachablePointInRadius(Pawn->GetActorLocation(), FLT_MAX, NavLocation))
		return EBTNodeResult::Failed;

	bIsTimedOut = false;
	const auto World = GetWorld();
	if (!World) return EBTNodeResult::Failed;

	Controller->MoveToLocation(NavLocation);

	World->GetTimerManager().SetTimer(MovementTimerHandle, [&]
	{
		bIsTimedOut = true;
	}, MaxMovementDuration, false);

	return EBTNodeResult::InProgress;
}

void URandomMovementTask::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	const auto AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		SafeFinish(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (bIsTimedOut)
	{
		AIController->StopMovement();
		SafeFinish(OwnerComp, EBTNodeResult::Failed);
	}
	else if (AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		SafeFinish(OwnerComp, EBTNodeResult::Succeeded);
	}
}

void URandomMovementTask::SafeFinish(UBehaviorTreeComponent& OwnerComp, const EBTNodeResult::Type& Type)
{
	const auto World = GetWorld();
	if (World) World->GetTimerManager().ClearTimer(MovementTimerHandle);

	FinishLatentTask(OwnerComp, Type);
}

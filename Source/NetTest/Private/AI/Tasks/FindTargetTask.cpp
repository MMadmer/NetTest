// NetTest project. All rights reserved.


#include "AI/Tasks/FindTargetTask.h"

#include "AI/NTAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

UFindTargetTask::UFindTargetTask()
{
	NodeName = TEXT("Find Target");
}

EBTNodeResult::Type UFindTargetTask::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const auto Controller = Cast<ANTAIController>(OwnerComp.GetAIOwner());
	if (!Controller) return EBTNodeResult::Failed;

	const auto World = GetWorld();
	if (!World) return EBTNodeResult::Failed;

	const auto GameState = World->GetGameState();
	if (!GameState) return EBTNodeResult::Failed;

	const TArray<APawn*> CurrentPlayers = GetCurrentPlayers();
	if (CurrentPlayers.Num() == 0) return EBTNodeResult::Failed;

	// Update Controller's used players
	int32 Equals = 0;
	for (int32 i = Controller->UsedPlayers.Num() - 1; i >= 0; i--)
	{
		if (!Controller->UsedPlayers.IsValidIndex(i)) continue;
		if (!CurrentPlayers.Contains(Controller->UsedPlayers[i])) Controller->UsedPlayers.RemoveAt(i);
		else Equals++;
	}
	if (Equals == CurrentPlayers.Num()) Controller->UsedPlayers.Empty();

	// Get new random player without repeating
	int32 TargetIndex;
	do
	{
		TargetIndex = FMath::RandRange(0, CurrentPlayers.Num() - 1);
	}
	while (Controller->UsedPlayers.Contains(CurrentPlayers[TargetIndex]) || CurrentPlayers.Num() > 1 && CurrentPlayers[
		TargetIndex] == Controller->LastTarget);

	const auto BlackBoard = Controller->GetBlackboardComponent();
	if (!BlackBoard) return EBTNodeResult::Failed;

	BlackBoard->SetValueAsObject(FName(TEXT("Target")), CurrentPlayers[TargetIndex]);
	Controller->UsedPlayers.Add(CurrentPlayers[TargetIndex]);
	Controller->LastTarget = CurrentPlayers[TargetIndex];

	return EBTNodeResult::Succeeded;
}

TArray<APawn*> UFindTargetTask::GetCurrentPlayers() const
{
	TArray<APawn*> CurrentPlayers{};

	const auto World = GetWorld();
	if (!World) return CurrentPlayers;

	const auto GameState = World->GetGameState();
	if (!GameState) return CurrentPlayers;

	for (const auto PlayerState : GameState->PlayerArray)
	{
		APawn* PlayerPawn = PlayerState->GetPawn();
		if (PlayerPawn) CurrentPlayers.Add(PlayerPawn);
	}

	return CurrentPlayers;
}

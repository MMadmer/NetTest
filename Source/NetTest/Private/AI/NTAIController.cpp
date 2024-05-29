// NetTest project. All rights reserved.


#include "AI/NTAIController.h"

#include "Net/UnrealNetwork.h"

ANTAIController::ANTAIController(): LastTarget(nullptr), BehaviorTree(nullptr)
{
	bReplicates = true;
}

void ANTAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!BehaviorTree) return;

	RunBehaviorTree(BehaviorTree);
}

void ANTAIController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANTAIController, UsedPlayers);
	DOREPLIFETIME(ANTAIController, LastTarget);
}

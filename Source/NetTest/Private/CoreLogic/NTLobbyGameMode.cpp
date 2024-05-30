// NetTest project. All rights reserved.


#include "CoreLogic/NTLobbyGameMode.h"

#include "GameFramework/GameStateBase.h"

ANTLobbyGameMode::ANTLobbyGameMode()
{
	bUseSeamlessTravel = true;
}

void ANTLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!GameState) return;

	if (GameState->PlayerArray.Num() == PlayersToStart)
	{
		UWorld* World = GetWorld();
		if (!World) return;

		World->ServerTravel(FString::Printf(TEXT("%s?listen"), *LevelPath));
	}
}

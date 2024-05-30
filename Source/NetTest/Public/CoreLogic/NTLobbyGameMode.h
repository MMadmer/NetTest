// NetTest project. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "NTLobbyGameMode.generated.h"

/**
 * 
 */
UCLASS()
class NETTEST_API ANTLobbyGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ANTLobbyGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	uint8 PlayersToStart = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FString LevelPath;
};

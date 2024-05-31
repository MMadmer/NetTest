// Copyright (C) 2021 Space Raccoon Game Studio. All rights reserved. Contacts: <business@space-raccoon.com>
// Created by AfroStalin

#include "OptimizationManager.h"
#include "OptimizationComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Classes/Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Async/Async.h"
#include "DrawDebugHelpers.h"
#include "AIController.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NPC_Optimizator.h"

DECLARE_CYCLE_STAT(TEXT("Manager - Thread tick"), STAT_OptimizationThreadTick, STATGROUP_NPC_Optimizator);
DECLARE_CYCLE_STAT(TEXT("Manager - Optimization tick"), STAT_OptimizationComponentsTick, STATGROUP_NPC_Optimizator);

#if SIMULATION_IN_BG_ENABLED
DECLARE_CYCLE_STAT(TEXT("Manager - Simulation tick"), STAT_SimulationTick, STATGROUP_NPC_Optimizator);
DECLARE_DWORD_COUNTER_STAT(TEXT("NPC count - Simulated in BG"), Stat_SimulatedInBG, STATGROUP_NPC_Optimizator);
DECLARE_DWORD_COUNTER_STAT(TEXT("NPC count - Respawn queue"), Stat_NeedToBeRespawned, STATGROUP_NPC_Optimizator);
#endif

DECLARE_DWORD_COUNTER_STAT(TEXT("NPC count (all with optimization)"), STAT_NPC_Count, STATGROUP_NPC_Optimizator);
DECLARE_DWORD_COUNTER_STAT(TEXT("NPC count - Invisible"), STAT_InvisibleCount, STATGROUP_NPC_Optimizator);
DECLARE_DWORD_COUNTER_STAT(TEXT("NPC count - Without optimization"), STAT_NoOptimizedCount, STATGROUP_NPC_Optimizator);
DECLARE_DWORD_COUNTER_STAT(TEXT("NPC count - First wave"), Stat_FirstWaveCount, STATGROUP_NPC_Optimizator);
DECLARE_DWORD_COUNTER_STAT(TEXT("NPC count - Second wave"), Stat_SecondWaveCount, STATGROUP_NPC_Optimizator);
DECLARE_DWORD_COUNTER_STAT(TEXT("NPC count - Third wave"), Stat_ThirdWaveCount, STATGROUP_NPC_Optimizator);


static TAutoConsoleVariable<float> CVarMaxFOV(
	TEXT("npc.optimizator.fov"),
	90.f,
	TEXT("Camera FOV for checking NPCs in frame\n"),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarMaxOptimizationOnOneFrame(
	TEXT("npc.optimizator.MaxOptimizedOnOneFrame"),
	15,
	TEXT("How many NPC can be optimizated in one frame\n"),
	ECVF_Scalability);

#if SIMULATION_IN_BG_ENABLED
static TAutoConsoleVariable<int32> CVarMaxRespawnOnOneFrame(
	TEXT("npc.optimizator.MaxRespawnOnOneFrame"),
	5,
	TEXT("How many NPC can be respawned from pseudo-simulation in one frame\n"),
	ECVF_Scalability);

AProxyNPC::AProxyNPC() 
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;

	StaticMeshInstanceComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StaticMeshInstanceComponent"));
	StaticMeshInstanceComponent->SetupAttachment(RootComponent);
	StaticMeshInstanceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshInstanceComponent->SetGenerateOverlapEvents(false);
	StaticMeshInstanceComponent->SetCastShadow(false);
}

void AProxyNPC::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShouldDrawDebug)
	{
		UKismetSystemLibrary::DrawDebugBox(GetWorld(), GetActorLocation(), FVector(100.f, 100.f, 100.f), FLinearColor::Red, FRotator::ZeroRotator, PrimaryActorTick.TickInterval);
	}
}

void AProxyNPC::BeginPlay()
{
	Super::BeginPlay();

	if (!bShouldDrawDebug)
	{
		SetActorTickEnabled(false);
	}

	NavigationSystem = UNavigationSystemV1::GetNavigationSystem(this);
}

EOptimizationBaseType AProxyNPC::GetOptimizationBasedType() const
{
	return OptimizationBasedOn;
}

AActor* AProxyNPC::GetOwnerActor()
{
	return this;
}

float AProxyNPC::GetMaxVisibleDistance() const
{
	return MaxVisibleDistance;
}

float AProxyNPC::GetDistToWave(EOptimizationWave Wave) const
{
	switch (Wave)
	{
	case Invisible:
		break;
	case NoOptimization:
		break;
	case FirstWave:
		return DistanceToFirstOptimization;
		break;
	case SecondWave:
		return DistanceToSecondOptimization;
		break;
	case ThirdWave:
		return DistanceToThirdOptimization;
		break;
	default:
		break;
	}

	return 0.0f;
}
#endif

FOptimizationThread::FOptimizationThread(UWorld* World, FOptimizationManager* InManager) : bIsRunning(true), GameWorld(World), Manager(InManager)
{
	if (GameWorld != nullptr)
	{
		Thread.Reset(FRunnableThread::Create(this, *FString::Printf(TEXT("NPC_Optimizator_Thread_%s"), *GameWorld->GetName())));
	}
}

uint32 FOptimizationThread::Run()
{
	while (bIsRunning)
	{
		SCOPE_CYCLE_COUNTER(STAT_OptimizationThreadTick);

		APlayerCameraManager* PlayerCamera = UGameplayStatics::GetPlayerCameraManager(GameWorld, 0);
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GameWorld, 0);
		APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;

		SET_DWORD_STAT(STAT_NPC_Count, 0);
		SET_DWORD_STAT(STAT_InvisibleCount, 0);
		SET_DWORD_STAT(STAT_NoOptimizedCount, 0);
		SET_DWORD_STAT(Stat_FirstWaveCount, 0);
		SET_DWORD_STAT(Stat_SecondWaveCount, 0);
		SET_DWORD_STAT(Stat_ThirdWaveCount, 0);

		const float FOV = CVarMaxFOV.GetValueOnAnyThread();

		// Calculate visible & optimization waves for proxy components
		Mutex.Lock();
		{
			SCOPE_CYCLE_COUNTER(STAT_OptimizationComponentsTick);

			const uint32 MaxOptimizatedInOneFrame = CVarMaxOptimizationOnOneFrame.GetValueOnAnyThread();
			uint32 OptimizatedCount = 0;

			for (auto& it : Components)
			{
				if (it == nullptr || (it && it->IsPendingKill()))
					continue;

				const EOptimizationWave CurrentWave = it->GetOptimizationWave();

				INC_DWORD_STAT(STAT_NPC_Count);

				// Stats counters
				{
					switch (CurrentWave)
					{
					case EOptimizationWave::Invisible:
						INC_DWORD_STAT(STAT_InvisibleCount);
						break;
					case EOptimizationWave::NoOptimization:
						INC_DWORD_STAT(STAT_NoOptimizedCount);
						break;
					case EOptimizationWave::FirstWave:
						INC_DWORD_STAT(Stat_FirstWaveCount);
						break;
					case EOptimizationWave::SecondWave:
						INC_DWORD_STAT(Stat_SecondWaveCount);
						break;
					case EOptimizationWave::ThirdWave:
						INC_DWORD_STAT(Stat_ThirdWaveCount);
						break;
					}
				}

				if (it->IsNeedToBeOptimized())
				{
					continue;
				}

				if (OptimizatedCount >= MaxOptimizatedInOneFrame)
					break;

				const bool IsVisible = it->IgnoreCameraSight ? true : IsInCameraSight(PlayerCamera, PlayerPawn, it, FOV);
				const EOptimizationWave NewWave = GetOptimizationWave(PlayerPawn, PlayerCamera, it, IsVisible);

				if (CurrentWave != NewWave)
				{
					it->SetOptimizationWave(NewWave);
					it->SetIsNeedToBeOptimized(true);

					OptimizatedCount++;
				}
			}
		}
		Mutex.Unlock();

#if SIMULATION_IN_BG_ENABLED
		TArray<AProxyNPC*> NeedToRespawn;

		// Pseudo-simulation for NPC
		ProxyMutex.Lock();
		{
			SCOPE_CYCLE_COUNTER(STAT_SimulationTick);

			const uint32 MaxRespawnCount = CVarMaxRespawnOnOneFrame.GetValueOnAnyThread();
			uint32 RespawnCount = 0;

			for (auto& it : ProxyActors)
			{
				INC_DWORD_STAT(Stat_SimulatedInBG);

				const bool IsVisible = IsInCameraSight(PlayerCamera, PlayerPawn, it, FOV);
				const EOptimizationWave NewWave = GetOptimizationWave(PlayerPawn, PlayerCamera, it, IsVisible);

				if (NewWave != Invisible)
				{
					if (RespawnCount >= MaxRespawnCount)
					{
						continue;
					}

					it->WaveOnRespawn = NewWave;

					INC_DWORD_STAT(Stat_NeedToBeRespawned);

					NeedToRespawn.Add(it);
					RespawnCount++;
				}
				else
				{
					// Simulate NPC moving
				}
			}

			for (const auto& it : NeedToRespawn)
			{
				ProxyActors.RemoveSingle(it);
			}
		}
		ProxyMutex.Unlock();

		if (NeedToRespawn.Num() > 0)
		{
			AsyncTask(ENamedThreads::GameThread, [this, NeedToRespawn]
				{
					if (Manager != nullptr)
					{
						Manager->RespawnNPC(NeedToRespawn);
					}
				});
		}
#endif

		FPlatformProcess::Sleep(0.01f);
	}

	return 0;
}

void FOptimizationThread::Exit()
{
	Manager = nullptr;
	bIsRunning = false;
}

void FOptimizationThread::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(GameWorld);

	for (auto& it : Components)
	{
		Collector.AddReferencedObject(it);
	}

#if SIMULATION_IN_BG_ENABLED
	for (auto& it : ProxyActors)
	{
		Collector.AddReferencedObject(it);
	}
#endif
}

void FOptimizationThread::RegisterComponent(UOptimizationProxyComponent* Component)
{
	Mutex.Lock();
	{
		check(Component);

		if (Component->GetOptimizationHandle() == INDEX_NONE)
		{
			Component->SetOptimizationHandle(Components.Num());

			Components.Add(Component);
		}
	} 
	Mutex.Unlock();
}

void FOptimizationThread::UnregisterComponent(UOptimizationProxyComponent* Component)
{
	Mutex.Lock();
	{
		check(Component);

		const int32 Handle = Component->GetOptimizationHandle();
		if (Handle != INDEX_NONE)
		{
			RemoveComponentByHandle(Handle);
		}
	}
	Mutex.Unlock();
}

#if SIMULATION_IN_BG_ENABLED
void FOptimizationThread::SimulateInBG(AProxyNPC* ProxyNPC)
{
	ProxyMutex.Lock();
	{
		ProxyActors.Add(ProxyNPC);
	}
	ProxyMutex.Unlock();
}
#endif

void FOptimizationThread::HandlePostGarbageCollect()
{
	Mutex.Lock();
	{
		bool bRemoved = false;
		do
		{
			bRemoved = false;
			for (int32 DataIndex = 0; DataIndex < Components.Num(); ++DataIndex)
			{
				if (Components[DataIndex] == nullptr)
				{
					RemoveComponentByHandle(DataIndex);
					bRemoved = true;
				}
			}
		} while (bRemoved);
	}
	Mutex.Unlock();
}

bool FOptimizationThread::IsInCameraSight(APlayerCameraManager* Camera, APawn* LocalPlayer, IOptimizationProxy* Proxy, float InFOV)
{
	if (Camera != nullptr && Proxy != nullptr)
	{
		float Distance = 10000.f;

		if (Proxy->GetOptimizationBasedType() == EOptimizationBaseType::PlayerCamera)
		{
			const FVector CameraLocation = Camera->GetCameraLocation();

			Distance = FVector::Distance(CameraLocation, Proxy->GetOwnerActor()->GetActorLocation());
		}
		else if (Proxy->GetOptimizationBasedType() == EOptimizationBaseType::PlayerPawn && LocalPlayer)
		{
			Distance = FVector::Distance(LocalPlayer->GetActorLocation(), Proxy->GetOwnerActor()->GetActorLocation());
		}

		const float CameraDotProd = Camera->GetDotProductTo(Proxy->GetOwnerActor());
		const float CamAngle = 1.f - (InFOV * 0.01f);

		if (CameraDotProd < CamAngle || Distance > Proxy->GetMaxVisibleDistance())
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	return true;
}

EOptimizationWave FOptimizationThread::GetOptimizationWave(APawn* LocalPlayer, APlayerCameraManager* Camera, IOptimizationProxy* Proxy, bool IsVisible)
{
	if (!IsVisible && !Proxy->IsIgnoreCameraSightOnSmallDistance())
	{
		return EOptimizationWave::Invisible;
	}

	float DistToPlayer = 10000.f;

	if (LocalPlayer)
	{
		DistToPlayer = FVector::Distance(LocalPlayer->GetActorLocation(), Proxy->GetOwnerActor()->GetActorLocation());
	}

	if (Camera && Proxy->GetOptimizationBasedType() == EOptimizationBaseType::PlayerCamera)
	{
		const FVector CameraLocation = Camera->GetCameraLocation();
		DistToPlayer = FVector::Distance(CameraLocation, Proxy->GetOwnerActor()->GetActorLocation());
	}

	if (!IsVisible && Proxy->IsIgnoreCameraSightOnSmallDistance() )
	{
		if (DistToPlayer < Proxy->GetDistToWave(EOptimizationWave::FirstWave))
		{
			return EOptimizationWave::NoOptimization;
		}
		else
		{
			return EOptimizationWave::Invisible;
		}
	}

	if (DistToPlayer < Proxy->GetDistToWave(EOptimizationWave::FirstWave))
	{
		return EOptimizationWave::NoOptimization;
	}
	else if (DistToPlayer >= Proxy->GetDistToWave(EOptimizationWave::FirstWave) && DistToPlayer < Proxy->GetDistToWave(EOptimizationWave::SecondWave))
	{
		return EOptimizationWave::FirstWave;
	}
	else if (DistToPlayer >= Proxy->GetDistToWave(EOptimizationWave::SecondWave) && DistToPlayer < Proxy->GetDistToWave(EOptimizationWave::ThirdWave))
	{
		return EOptimizationWave::SecondWave;
	}
	else if (DistToPlayer >= Proxy->GetDistToWave(EOptimizationWave::ThirdWave) && DistToPlayer < Proxy->GetMaxVisibleDistance())
	{
		return EOptimizationWave::ThirdWave;
	}

	return EOptimizationWave::Invisible;
}

void FOptimizationThread::RemoveComponentByHandle(int32 Handle)
{
	if (Components.IsValidIndex(Handle))
	{
		if (Components[Handle] != nullptr && !Components[Handle]->IsPendingKill())
		{
			Components[Handle]->SetOptimizationHandle(INDEX_NONE);
		}

		Components.RemoveAtSwap(Handle, 1, false);

		const int32 NumRemaining = Components.Num();
		if (NumRemaining > 0 && Handle != NumRemaining)
		{
			if (Components[Handle] != nullptr && !Components[Handle]->IsPendingKill())
			{
				Components[Handle]->SetOptimizationHandle(Handle);
			}
		}
	}
}

FOptimizationManager::FOptimizationManager(UWorld* InWorld) : GameWorld(InWorld), OptimizationThread(MakeUnique<FOptimizationThread>(InWorld, this))
{
	PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(this, &FOptimizationManager::HandlePostGarbageCollect);
	//OnWorldPreActorTickHandle = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FOptimizationManager::OnWorldPreActorTick);
}

FOptimizationManager::~FOptimizationManager()
{
	if (OptimizationThread.IsValid())
	{
		OptimizationThread->Exit();
	}

	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectHandle);
	//FWorldDelegates::OnWorldPreActorTick.Remove(OnWorldPreActorTickHandle);
}

void FOptimizationManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(GameWorld);
}

void FOptimizationManager::OnWorldPreActorTick(UWorld* InWorld, ELevelTick InLevelTick, float InDeltaSeconds)
{

}

void FOptimizationManager::RegisterComponent(UOptimizationProxyComponent* Component)
{
	if (OptimizationThread.IsValid())
	{
		OptimizationThread->RegisterComponent(Component);
	}
}

void FOptimizationManager::UnregisterComponent(UOptimizationProxyComponent* Component)
{
	if (OptimizationThread.IsValid())
	{
		OptimizationThread->UnregisterComponent(Component);
	}
}

#if SIMULATION_IN_BG_ENABLED
void FOptimizationManager::SimulateInBG(UOptimizationProxyComponent* Component)
{
	if (OptimizationThread.IsValid())
	{
		AActor* NPCActor = Component->GetOwner();
		UClass* NPC_Class = NPCActor->GetClass();
		const FVector NPC_Location = NPCActor->GetActorLocation();
		const FRotator NPC_Rotation = NPCActor->GetActorRotation();
		const FVector NPC_Scale = NPCActor->GetActorScale3D();

		FTransform ProxyTM = FTransform(NPC_Rotation.Quaternion(), NPC_Location, NPC_Scale);

		AProxyNPC* ProxyNPC = GameWorld->SpawnActorDeferred<AProxyNPC>(AProxyNPC::StaticClass(), ProxyTM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if(ProxyNPC != nullptr)
		{ 
			ProxyNPC->NPC_Class = NPC_Class;
			ProxyNPC->bShouldDrawDebug = Component->Invisible_DrawDebug;
			ProxyNPC->MaxVisibleDistance = Component->MaxVisibleDistance;
			ProxyNPC->DistanceToFirstOptimization = Component->DistanceToFirstOptimization;
			ProxyNPC->DistanceToSecondOptimization = Component->DistanceToSecondOptimization;
			ProxyNPC->DistanceToThirdOptimization = Component->DistanceToThirdOptimization;
			ProxyNPC->OptimizationBasedOn = Component->OptimizationBasedOn;

			UGameplayStatics::FinishSpawningActor(ProxyNPC, ProxyTM);


			ProxyNPC->SetActorLocation(NPC_Location);
		}

		OptimizationThread->SimulateInBG(ProxyNPC);

		NPCActor->Destroy();
	}
}

void FOptimizationManager::RespawnNPC(TArray<AProxyNPC*> InNPC)
{
	check(IsInGameThread());

	for (const auto& it : InNPC)
	{
		const FVector NPC_Location = it->GetActorLocation();
		const FRotator NPC_Rotation = it->GetActorRotation();
		const FVector NPC_Scale = it->GetActorScale3D();
		UClass* NPC_Class = it->NPC_Class;
		const EOptimizationWave WaveOnRespawn = it->WaveOnRespawn;

		FTransform ActorTM = FTransform(NPC_Rotation, NPC_Location, NPC_Scale);

		APawn* RespawnedNPC = GameWorld->SpawnActorDeferred<APawn>(NPC_Class, ActorTM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (RespawnedNPC != nullptr)
		{
			RespawnedNPC->SpawnDefaultController();

			UOptimizationProxyComponent* Proxy = Cast<UOptimizationProxyComponent>(RespawnedNPC->GetComponentByClass(UOptimizationProxyComponent::StaticClass()));

			if (Proxy != nullptr)
			{
				Proxy->SetOptimizationWave(WaveOnRespawn);
				Proxy->OptimizeComponent();
			}

			UGameplayStatics::FinishSpawningActor(RespawnedNPC, ActorTM);
		}


		it->Destroy();
	}
}
#endif

void FOptimizationManager::HandlePostGarbageCollect()
{
	if (OptimizationThread.IsValid())
	{
		OptimizationThread->HandlePostGarbageCollect();
	}
}
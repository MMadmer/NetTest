// Copyright (C) 2021 Space Raccoon Game Studio. All rights reserved. Contacts: <business@space-raccoon.com>
// Created by AfroStalin

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/World.h"
#include "Stats/Stats.h"
#include "OptimizationComponent.h"

#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"

//#include "OptimizationManager.generated.h"

DECLARE_STATS_GROUP(TEXT("NPC_Optimizator"), STATGROUP_NPC_Optimizator, STATCAT_Advanced);

class  FRunnableThread;

//UCLASS()
//class AProxyNPC : public AActor, public IOptimizationProxy
//{
//	GENERATED_BODY()
//public:
//	AProxyNPC();
//public:
//	// AActor
//	virtual void Tick(float DeltaTime) override;
//	virtual void BeginPlay() override;
//	//~AActor
//public:
//	// IOptimizationProxy
//	virtual EOptimizationBaseType GetOptimizationBasedType() const override;
//	virtual AActor*               GetOwnerActor() override;
//	virtual float                 GetMaxVisibleDistance() const override;
//	virtual float                 GetDistToWave(EOptimizationWave Wave) const override;
//	//~IOptimizationProxy
//public:
//	UPROPERTY()
//	UClass* NPC_Class = nullptr;
//
//	UPROPERTY()
//	bool bShouldDrawDebug = false;
//
//	UPROPERTY()
//	float MaxVisibleDistance = 8000.f;
//
//	UPROPERTY()
//	float DistanceToFirstOptimization = 1500.f;
//
//	UPROPERTY()
//	float DistanceToSecondOptimization = 2500.f;
//
//	UPROPERTY()
//	float DistanceToThirdOptimization = 3500.f;
//
//	UPROPERTY()
//	EOptimizationBaseType OptimizationBasedOn = EOptimizationBaseType::PlayerPawn;
//public:
//	EOptimizationWave WaveOnRespawn = EOptimizationWave::NoOptimization;
//protected:
//	class UInstancedStaticMeshComponent* StaticMeshInstanceComponent = nullptr;
//private:
//	class UNavigationSystemV1* NavigationSystem = nullptr;
//};

class FOptimizationThread : public FRunnable, public FGCObject
{
public:
	FOptimizationThread(UWorld* World, class FOptimizationManager* InManager);
public:
	// FRunnable
	virtual uint32 Run() override;
	virtual void   Exit() override;
	//~FRunnable
public:
	// FGCObject
	virtual void      AddReferencedObjects(FReferenceCollector& Collector) override;
	//~FGCObject
public:
	void              RegisterComponent(UOptimizationProxyComponent* Component);
	void              UnregisterComponent(UOptimizationProxyComponent* Component);
#if SIMULATION_IN_BG_ENABLED
	void              SimulateInBG(AProxyNPC* ProxyNPC);
#endif
public:
	void              HandlePostGarbageCollect();
private:
	void              RemoveComponentByHandle(int32 Handle);
	EOptimizationWave GetOptimizationWave(APawn* LocalPlayer, class APlayerCameraManager* Camera, IOptimizationProxy* Proxy, bool IsVisible);
	bool              IsInCameraSight(class APlayerCameraManager* Camera, APawn* LocalPlayer, IOptimizationProxy* Proxy, float InFOV);
protected:
	UPROPERTY()
	TArray<UOptimizationProxyComponent*> Components;

	//UPROPERTY()
	//TArray<AProxyNPC*>                   ProxyActors;

	mutable FCriticalSection             Mutex;
	mutable FCriticalSection             ProxyMutex;
	TUniquePtr<FRunnableThread>          Thread;
	FThreadSafeBool                      bIsRunning = true;
							             
	UPROPERTY()				             
	UWorld*                              GameWorld = nullptr;
							             
	class FOptimizationManager*          Manager = nullptr;
};

class FOptimizationManager : public FGCObject
{
public:
	FOptimizationManager(UWorld* InWorld);
	~FOptimizationManager();
public:
	// FGCObject
	virtual void      AddReferencedObjects(FReferenceCollector& Collector) override;
	//~FGCObject
public:
	void              OnWorldPreActorTick(UWorld* InWorld, ELevelTick InLevelTick, float InDeltaSeconds);
public:
	void              RegisterComponent(UOptimizationProxyComponent* Component);
	void              UnregisterComponent(UOptimizationProxyComponent* Component);
public:
#if SIMULATION_IN_BG_ENABLED
	void              SimulateInBG(UOptimizationProxyComponent* Component);
	void              RespawnNPC(TArray<AProxyNPC*> InNPC);
#endif
protected:
	void              HandlePostGarbageCollect();
protected:
	FDelegateHandle   OnWorldPreActorTickHandle;
	FDelegateHandle   PostGarbageCollectHandle;
private:
	//UPROPERTY()
	//TArray<AProxyNPC*> NeedToBeRespawned;

	UPROPERTY()
	UWorld*            GameWorld = nullptr;
private:
	TUniquePtr<FOptimizationThread> OptimizationThread;
	FCriticalSection                Mutex;
};
// Copyright (C) 2021 Space Raccoon Game Studio. All rights reserved. Contacts: <business@space-raccoon.com>
// Created by AfroStalin

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/ThreadSafeBool.h"

#include "OptimizationComponent.generated.h"

#define SIMULATION_IN_BG_ENABLED 0

UENUM(BlueprintType)
enum class EOptimizationWave : uint8
{
	Invisible UMETA(DisplayName = "Invisible"),
	NoOptimization UMETA(DisplayName = "No optimization"),
	FirstWave UMETA(DisplayName = "First wave"),
	SecondWave UMETA(DisplayName = "Second wave"),
	ThirdWave UMETA(DisplayName = "Third wave")
};

UENUM()
enum class EOptimizationBaseType
{
	PlayerPawn UMETA(DisplayName = "Based on player pawn position"),
	PlayerCamera UMETA(DisplayName = "Based on player camera position")
};

struct FSDefaultSkeletalMeshSettings
{
	bool IsVisible = true;
	bool IsCastShadows = true;
	ECollisionEnabled::Type Collision = ECollisionEnabled::QueryAndPhysics;
	FCollisionResponseContainer CollisionResponseToChannels;
	bool EnableUpdateRateOptimizations = true;
	bool PerBoneMotionBlur = true;
	bool AllowRigidBodyAnimNode = true;
	bool SkipKinematicUpdateWhenInterpolating = false;
	bool SkipBoundsUpdateWhenInterpolating = false;
	bool GenerateOverlapEvents = true;
	EVisibilityBasedAnimTickOption VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	float MaxDrawDistance = 12000.f;
};

struct FSDefaultStaticMeshSettings
{
	bool IsVisible = true;
};

struct FSDefaultMovementSettings
{
	bool AlwaysCheckFloor = true;
	bool EnablePhysicsInteraction = true;
	float DefaultMovementComponentTick = 0.f;
	float MaxSimulationTimeStep = 0.05f;
	int32 MaxSimulationIterations = 8;
	bool RunPhysicsWithNoController = true;
	bool SweepWhileNavWalking = true;
};

struct FSOptimizedStaticMesh
{
	FSOptimizedStaticMesh() {}
	FSOptimizedStaticMesh(UStaticMeshComponent* InComp, FSDefaultStaticMeshSettings InMesh)
		: Component(InComp)
		, MeshSettings(InMesh)
	{}
	
	TWeakObjectPtr<UStaticMeshComponent> Component = nullptr;

	FSDefaultStaticMeshSettings MeshSettings;
};

struct FSOptimizedComponent
{
	FSOptimizedComponent() {}
	FSOptimizedComponent(UActorComponent* InComp, FSDefaultSkeletalMeshSettings InMesh, TArray<FSOptimizedStaticMesh> ChildMeshes)
		: Component(InComp)
		, MeshSettings(InMesh)
		, ChildStaticMeshes(ChildMeshes)
	{}


	TWeakObjectPtr<UActorComponent>  Component = nullptr;
	FSDefaultSkeletalMeshSettings    MeshSettings;
	TArray<FSOptimizedStaticMesh>    ChildStaticMeshes;
};

struct IOptimizationProxy
{
	virtual ~IOptimizationProxy() = default;
	
	virtual EOptimizationBaseType GetOptimizationBasedType() const = 0;
	virtual AActor*               GetOwnerActor() = 0;
	virtual float                 GetMaxVisibleDistance() const = 0;
	virtual float                 GetDistToWave(EOptimizationWave Wave) const = 0;
	virtual bool                  IsIgnoreCameraSightOnSmallDistance() const = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOptimizationChangeWave, EOptimizationWave, NewWave);

UCLASS(ClassGroup = (Custom), meta=(BlueprintSpawnableComponent))
class NPC_OPTIMIZATOR_API UOptimizationProxyComponent : public UActorComponent, public IOptimizationProxy
{
	GENERATED_BODY()
public:
	UOptimizationProxyComponent();
public:
	// UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~UActorComponent
public:
	// IOptimizationProxy
	virtual EOptimizationBaseType GetOptimizationBasedType() const override;
	virtual AActor*               GetOwnerActor() override;
	virtual float                 GetMaxVisibleDistance() const override;
	virtual float                 GetDistToWave(EOptimizationWave Wave) const override;
	virtual bool                  IsIgnoreCameraSightOnSmallDistance() const override;
	//~IOptimizationProxy
public:
	int32                    GetOptimizationHandle() const { return OptimizationHandle; }
	EOptimizationWave        GetOptimizationWave() const;

	void                     SetOptimizationHandle(int32 Handle) { OptimizationHandle = Handle; }
	void                     SetOptimizationWave(EOptimizationWave Wave);

	void                     SetIsNeedToBeOptimized(bool NeedToOptimize);
	bool                     IsNeedToBeOptimized() const;

	void                     OptimizeComponent();
public:
	// Execute this function for disable all optimization for current owner
	UFUNCTION(BlueprintCallable, Category = "NPC Optimizator")
	void                     DisableAllOptimizations();

	// Execute this function for enable optimization after disabling it
	UFUNCTION(BlueprintCallable, Category = "NPC Optimizator")
	void                     EnableOptimizations();
public:
	// The distance from the player to the bots at which the first optimization level is enabled. 
	// Bots closer than this distance are not optimized
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization")
	float DistanceToFirstOptimization = 1500.f;

	// The distance from the player to the bots at which the second level of optimizations is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization")
	float DistanceToSecondOptimization = 2500.f;

	// The distance from the player to the bots at which the third level of optimizations is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization")
	float DistanceToThirdOptimization = 3500.f;

	// Bots outside this distance will be hidden and simplified as much as possible
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization")
	float MaxVisibleDistance = 8000.f;

	// Optimization is based on the distance from the player's pawn or camera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization")
	EOptimizationBaseType OptimizationBasedOn = EOptimizationBaseType::PlayerPawn;

	// If you want optimize only component with special tag - enable it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization")
	bool UseOptimizationByTag = false;

	// Special tag for component needed for optimization it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization")
	FName OptimizationTag = "NeedOptimization";

	// Special tag to ignore any component optimizations
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization")
	FName IgnoreOptimizationTag = "IgnoreOptimization";

	// If you want stop NPC movement when it not visible
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", DisplayName = "Disable movement when invisible")
	bool DisableMovementWhenNotVisible = false;

	// Enable this setting in order to ignore the calculation of the visibility of NPCs by the camera and calculate only by distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", DisplayName = "Ignore camera sight")
	bool IgnoreCameraSight = false;

	// Ignore camera visibility when bots are close to the player
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", DisplayName = "Ignore camera sight in no optimization distance")
	bool IgnoreCameraSightOnSmallDistance = false;

	// Enabling this setting gives you more frames per second, but when mesh switch from invisibility back to visibility, the number of frames may drop slightly
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", DisplayName = "Disable skeletal mesh tick when invisible")
	bool DisableSkeletalMeshTickWhenNotVisible = false;

	// If you want hide all static meshes attached to skeletal meshes
	// If you need hide only special static meshes use Optimization Tag for it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", DisplayName = "Hide all attached static meshes")
	bool HideAllStaticMeshes = true;

	/*
	* When the bot is not in the frame, it is replaced by a dummy that moves randomly along the navigation grid, simulating some activity.
	All bot logic doesn't work. The bot will respawn again when it is in sight, while its position and rotation will correspond to the simulation. 
	This method is suitable for simulating a huge number of bots on a map.
	*/
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", DisplayName = "Despawn when invisible and simulate walking")
	//bool SimulateNPCWalking = false;
public:
	// Movement component need always check floor on this wave?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Movement", DisplayName = "Always check floor")
	bool FirstWave_AlwaysCheckFloor = true;

	// Enable physics interaction on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Movement", DisplayName = "Enable physics interaction")
	bool FirstWave_EnablePhysicsInteraction = true;

	// Max simulation time step
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Movement", DisplayName = "Max simulation time step")
	float FirstWave_MaxSimulationTimeStep = 0.025f;

	// Max simulation iterations
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Movement", DisplayName = "Max simulation iterations")
	int32 FirstWave_MaxSimulationIterations = 4;

	// Run physics with no controller
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Movement", DisplayName = "Run physics with no controller")
	bool FirstWave_RunPhysicsWithNoController = true;

	// Movement mode on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Movement", DisplayName = "Movement mode")
	TEnumAsByte<EMovementMode> FirstWave_MovementMode = EMovementMode::MOVE_NavWalking;

	// Whether or not the character should sweep for collision geometry while walking
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Movement", DisplayName = "Sweep while nav walking")
	bool FirstWave_SweepWhileNavWalking = true;
public:
	// Movement component tick rate when random tick not used
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Movement", DisplayName = "Fixed movement tick")
	float FirstWave_OptimizatedMovementTick = 0.05f;

	// Movement component min tick rate when random tick enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Movement", DisplayName = "Min movement random tick")
	float FirstWave_OptimizatedMovementTickMin = 0.01f;

	// Movement component max tick rate when random tick enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Movement", DisplayName = "Max movement random tick")
	float FirstWave_OptimizatedMovementTickMax = 0.05f;

	// Use random tick for movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Movement", DisplayName = "Use random movement tick")
	bool FirstWave_UseRandomOptimizationTickForMovement = true;
public:
	// Hide shadows from skeletal mesh on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Skeletal meshes", DisplayName = "Hide shadows")
	bool FirstWave_HideShadows = false;

	// Disable skeletal meshes collision on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Skeletal meshes", DisplayName = "Disable mesh collision")
	bool FirstWave_DisableMeshCollision = false;

	// Whether or not the character should hide attached static meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Skeletal meshes", DisplayName = "Need hide static meshes")
	bool FirstWave_NeedHideStaticMeshes = false;

	// Use update rate optimizations for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Skeletal meshes", DisplayName = "Use URO")
	bool FirstWave_UseUpdateRateOptimizations = true;

	// Use per bone motion blur for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Skeletal meshes", DisplayName = "Use per bone motion blur")
	bool FirstWave_UsePerBoneMotionBlur = true;

	// Disable cloth simulation for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Skeletal meshes", DisplayName = "Disable cloth simulation")
	bool FirstWave_DisableClothSimulation = true;

	// Disable morph target simulation for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Skeletal meshes", DisplayName = "Disable morph target")
	bool FirstWave_DisableMorphTarget = true;

	// Skip kinematic update when interpolating for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Skeletal meshes", DisplayName = "Skip kinematic update when interpolating")
	bool FirstWave_SkipKinematicUpdateWhenInterpolating = true;

	// Skip bounds update when interpolating for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Skeletal meshes", DisplayName = "Skip bounds update when interpolating")
	bool FirstWave_SkipBoundsUpdateWhenInterpolating = true;

	// Allow rigid body anim node for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Skeletal meshes", DisplayName = "Allow rigid body anim node")
	bool FirstWave_AllowRigidBodyAnimNode = true;

	// Generate overlap events?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|First wave|Skeletal meshes", DisplayName = "Generate overlap events")
	bool FirstWave_GenerateOverlapEvents = true;
public:
	// Movement component need always check floor on this wave?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Movement", DisplayName = "Always check floor")
	bool SecondWave_AlwaysCheckFloor = false;

	// Enable physics interaction on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Movement", DisplayName = "Enable physics interaction")
	bool SecondWave_EnablePhysicsInteraction = false;

	// Max simulation time step
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Movement", DisplayName = "Max simulation time step")
	float SecondWave_MaxSimulationTimeStep = 0.035f;

	// Max simulation iterations
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Movement", DisplayName = "Max simulation iterations")
	int32 SecondWave_MaxSimulationIterations = 2;

	// Run physics with no controller
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Movement", DisplayName = "Run physics with no controller")
	bool SecondWave_RunPhysicsWithNoController = false;

	// Movement mode on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Movement", DisplayName = "Movement mode")
	TEnumAsByte<EMovementMode> SecondWave_MovementMode = EMovementMode::MOVE_NavWalking;

	// Whether or not the character should sweep for collision geometry while walking
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Movement", DisplayName = "Sweep while nav walking")
	bool SecondWave_SweepWhileNavWalking = false;
public:
	// Movement component tick rate when random tick not used
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Movement", DisplayName = "Fixed movement tick")
	float SecondWave_OptimizatedMovementTick = 0.1f;

	// Movement component min tick rate when random tick enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Movement", DisplayName = "Min movement random tick")
	float SecondWave_OptimizatedMovementTickMin = 0.05f;

	// Movement component max tick rate when random tick enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Movement", DisplayName = "Max movement random tick")
	float SecondWave_OptimizatedMovementTickMax = 0.1f;

	// Use random tick for movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Movement", DisplayName = "Use random movement tick")
	bool SecondWave_UseRandomOptimizationTickForMovement = true;
public:
	// Hide shadows from skeletal mesh on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Skeletal meshes", DisplayName = "Hide shadows")
	bool SecondWave_HideShadows = true;

	// Disable skeletal meshes collision on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Skeletal meshes", DisplayName = "Disable mesh collision")
	bool SecondWave_DisableMeshCollision = true;

	// Whether or not the character should hide attached static meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Skeletal meshes", DisplayName = "Need hide static meshes")
	bool SecondWave_NeedHideStaticMeshes = true;

	// Use update rate optimizations for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Skeletal meshes", DisplayName = "Use URO")
	bool SecondWave_UseUpdateRateOptimizations = true;

	// Use per bone motion blur for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Skeletal meshes", DisplayName = "Use per bone motion blur")
	bool SecondWave_UsePerBoneMotionBlur = false;

	// Disable cloth simulation for skeletal mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Skeletal meshes", DisplayName = "Disable cloth simulation")
	bool SecondWave_DisableClothSimulation = true;

	// Disable morph target simulation for skeletal mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Skeletal meshes", DisplayName = "Disable morph target")
	bool SecondWave_DisableMorphTarget = true;

	// Skip kinematic update when interpolating for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Skeletal meshes", DisplayName = "Skip kinematic update when interpolating")
	bool SecondWave_SkipKinematicUpdateWhenInterpolating = true;

	// Skip bounds update when interpolating for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Skeletal meshes", DisplayName = "Skip bounds update when interpolating")
	bool SecondWave_SkipBoundsUpdateWhenInterpolating = true;

	// Allow rigid body anim node for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Skeletal meshes", DisplayName = "Allow rigid body anim node")
	bool SecondWave_AllowRigidBodyAnimNode = false;

	// Generate overlap events?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Second wave|Skeletal meshes", DisplayName = "Generate overlap events")
	bool SecondWave_GenerateOverlapEvents = false;
public:
	// Movement component need always check floor on this wave?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Movement", DisplayName = "Always check floor")
	bool ThirdWave_AlwaysCheckFloor = false;

	// Enable physics interaction on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Movement", DisplayName = "Enable physics interaction")
	bool ThirdWave_EnablePhysicsInteraction = false;

	// Max simulation time step
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Movement", DisplayName = "Max simulation time step")
	float ThirdWave_MaxSimulationTimeStep = 0.05f;

	// Max simulation iterations
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Movement", DisplayName = "Max simulation iterations")
	int32 ThirdWave_MaxSimulationIterations = 1;

	// Run physics with no controller
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Movement", DisplayName = "Run physics with no controller")
	bool ThirdWave_RunPhysicsWithNoController = false;

	// Movement mode on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Movement", DisplayName = "Movement mode")
	TEnumAsByte<EMovementMode> ThirdWave_MovementMode = EMovementMode::MOVE_NavWalking;

	// Whether or not the character should sweep for collision geometry while walking
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Movement", DisplayName = "Sweep while nav walking")
	bool ThirdWave_SweepWhileNavWalking = false;
public:
	// Movement component tick rate when random tick not used
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Movement", DisplayName = "Fixed movement tick")
	float ThirdWave_OptimizatedMovementTick = 0.2f;

	// Movement component min tick rate when random tick enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Movement", DisplayName = "Min movement random tick")
	float ThirdWave_OptimizatedMovementTickMin = 0.1f;

	// Movement component max tick rate when random tick enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Movement", DisplayName = "Max movement random tick")
	float ThirdWave_OptimizatedMovementTickMax = 0.2f;

	// Use random tick for movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Movement", DisplayName = "Use random movement tick")
	bool ThirdWave_UseRandomOptimizationTickForMovement = true;
public:
	// Hide shadows from skeletal mesh on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Skeletal meshes", DisplayName = "Hide shadows")
	bool ThirdWave_HideShadows = true;

	// Disable skeletal meshes collision on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Skeletal meshes", DisplayName = "Disable mesh collision")
	bool ThirdWave_DisableMeshCollision = true;

	// Whether or not the character should hide attached static meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Skeletal meshes", DisplayName = "Need hide static meshes")
	bool ThirdWave_NeedHideStaticMeshes = true;

	// Use update rate optimizations for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Skeletal meshes", DisplayName = "Use URO")
	bool ThirdWave_UseUpdateRateOptimizations = true;

	// Use per bone motion blur for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Skeletal meshes", DisplayName = "Use per bone motion blur")
	bool ThirdWave_UsePerBoneMotionBlur = false;

	// Disable cloth simulation for skeletal mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Skeletal meshes", DisplayName = "Disable cloth simulation")
	bool ThirdWave_DisableClothSimulation = true;

	// Disable morph target simulation for skeletal mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Skeletal meshes", DisplayName = "Disable morph target")
	bool ThirdWave_DisableMorphTarget = true;

	// Skip kinematic update when interpolating for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Skeletal meshes", DisplayName = "Skip kinematic update when interpolating")
	bool ThirdWave_SkipKinematicUpdateWhenInterpolating = true;

	// Skip bounds update when interpolating for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Skeletal meshes", DisplayName = "Skip bounds update when interpolating")
	bool ThirdWave_SkipBoundsUpdateWhenInterpolating = true;

	// Allow rigid body anim node for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Skeletal meshes", DisplayName = "Allow rigid body anim node")
	bool ThirdWave_AllowRigidBodyAnimNode = false;

	// Generate overlap events?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Third wave|Skeletal meshes", DisplayName = "Generate overlap events")
	bool ThirdWave_GenerateOverlapEvents = false;
public:
	// Movement component need always check floor on this wave?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Movement", DisplayName = "Always check floor")
	bool Invisible_AlwaysCheckFloor = false;

	// Enable physics interaction on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Movement", DisplayName = "Enable physics interaction")
	bool Invisible_EnablePhysicsInteraction = false;

	// Max simulation time step
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Movement", DisplayName = "Max simulation time step")
	float Invisible_MaxSimulationTimeStep = 0.5f;

	// Max simulation iterations
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Movement", DisplayName = "Max simulation iterations")
	int32 Invisible_MaxSimulationIterations = 1;

	// Run physics with no controller
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Movement", DisplayName = "Run physics with no controller")
	bool Invisible_RunPhysicsWithNoController = false;

	// Movement mode on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Movement", DisplayName = "Movement mode")
	TEnumAsByte<EMovementMode> Invisible_MovementMode = EMovementMode::MOVE_NavWalking;

	// Whether or not the character should sweep for collision geometry while walking
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Movement", DisplayName = "Sweep while nav walking")
	bool Invisible_SweepWhileNavWalking = false;
public:
	// Movement component tick rate when random tick not used
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Movement", DisplayName = "Fixed movement tick")
	float Invisible_OptimizatedMovementTick = 1.f;

	// Movement component min tick rate when random tick enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Movement", DisplayName = "Min movement random tick")
	float Invisible_OptimizatedMovementTickMin = 1.f;

	// Movement component max tick rate when random tick enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Movement", DisplayName = "Max movement random tick")
	float Invisible_OptimizatedMovementTickMax = 1.5f;

	// Use random tick for movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Movement", DisplayName = "Use random movement tick")
	bool Invisible_UseRandomOptimizationTickForMovement = true;
public:
	// Hide skeletal mesh when it not visible for player
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Hide skeletal mesh")
	bool Invisible_HideSkeletalMesh = true;

	// Hide shadows from skeletal mesh on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Hide shadows")
	bool Invisible_HideShadows = true;

	// Disable skeletal meshes collision on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Disable mesh collision")
	bool Invisible_DisableMeshCollision = true;

	// Whether or not the character should hide attached static meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Need hide static meshes")
	bool Invisible_NeedHideStaticMeshes = true;

	// Use update rate optimizations for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Use URO")
	bool Invisible_UseUpdateRateOptimizations = true;

	// Use per bone motion blur for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Use per bone motion blur")
	bool Invisible_UsePerBoneMotionBlur = false;

	// Disable cloth simulation for skeletal mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Disable cloth simulation")
	bool Invisible_DisableClothSimulation = true;

	// Disable morph target simulation for skeletal mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Disable morph target")
	bool Invisible_DisableMorphTarget = true;

	// Skip kinematic update when interpolating for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Skip kinematic update when interpolating")
	bool Invisible_SkipKinematicUpdateWhenInterpolating = true;

	// Skip bounds update when interpolating for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Skip bounds update when interpolating")
	bool Invisible_SkipBoundsUpdateWhenInterpolating = true;

	// Allow rigid body anim node for skeletal meshes on this wave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Allow rigid body anim node")
	bool Invisible_AllowRigidBodyAnimNode = false;

	// Generate overlap events?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Invisible|Skeletal meshes", DisplayName = "Generate overlap events")
	bool Invisible_GenerateOverlapEvents = false;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Debug draw")
	bool NoOptimization_DrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Debug draw")
	bool FirstWave_DrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Debug draw")
	bool SecondWave_DrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Debug draw")
	bool ThirdWave_DrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization|Debug draw")
	bool Invisible_DrawDebug = false;
public:
	/** Called when component going to new optimization wave */
	UPROPERTY(BlueprintAssignable, Category = "Optimization")
	FOptimizationChangeWave  OnChangeWave;
protected:
	void OptimizeSkeletalMeshes(EOptimizationWave Wave);
	void OptimizeMovement(EOptimizationWave Wave) const;
public:
	FSDefaultMovementSettings     MovementSettings;
private:
	mutable FCriticalSection      LockCS;
	int32                         OptimizationHandle = -1;
	EOptimizationWave             OptimizationWave = EOptimizationWave::NoOptimization;
	EOptimizationWave             PreviousWave = EOptimizationWave::NoOptimization;
	FThreadSafeBool               bIsNeedToBeOptimized = false;
	TArray<FSOptimizedComponent>  ComponentsForOptimization;
};
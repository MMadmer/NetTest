// Copyright 2018 Elliot Gray. All Rights Reserved.

#include "UIWSRiver.h"

#include "UIWSManager.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Components/PostProcessComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"


AUIWSRiver::AUIWSRiver()
{
	SplineComp = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComp"));
	SplineComp->SetMobility(EComponentMobility::Movable);
	SplineComp->SetupAttachment(RootComponent);
	PostProcessComp->bEnabled = false;
	WaterVolume->SetVisibility(false);
	WaterVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	bDisableAutomaticInteraction = true;
	//get water static mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WaterSM(
		TEXT("/UIWS/Materials/Simulation/Meshes/UIWSRiverMesh.UIWSRiverMesh"));
	if (WaterSM.Succeeded())
	{
		WaterMeshSM = WaterSM.Object;
		//WaterMeshComp->SetStaticMesh(WaterSM.Object);
		//WaterMeshComp->SetMobility(EComponentMobility::Static);
	}

	//get water material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterMaterial(
		TEXT("/UIWS/Materials/FlowingRiverMat_Inst.FlowingRiverMat_Inst")); //transparent water mat
	if (WaterMaterial.Succeeded())
	{
		WaterMat = WaterMaterial.Object;
	}

	//get water material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterMaterialLod1(
		TEXT("/UIWS/Materials/FlowingRiverMat_Inst_Opaque.FlowingRiverMat_Inst_Opaque")); //transparent water mat
	if (WaterMaterialLod1.Succeeded())
	{
		WaterMatLOD1 = WaterMaterialLod1.Object;
	}

	bSupportsEdgeReflection = false;
}

void AUIWSRiver::CreateMeshSurface()
{
	/** Get rid of the old chunks*/
	for (auto& Comp : SplineMeshComps)
	{
		Comp->DestroyComponent();
	}
	SplineMeshComps.Empty();


	InitializeWaterMaterial(false);

	for (int i = 0; i < SplineComp->GetNumberOfSplinePoints() - 1; i++)
	{
		USplineMeshComponent* MeshComp = CreateSplineMeshComp();
		FVector StartPos;
		FVector StartTan;
		FVector EndPos;
		FVector EndTan;
		FVector ScaleStart;
		FVector ScaleEnd;
		SplineComp->GetLocationAndTangentAtSplinePoint(i, StartPos, StartTan, ESplineCoordinateSpace::Local);
		SplineComp->GetLocationAndTangentAtSplinePoint(i + 1, EndPos, EndTan, ESplineCoordinateSpace::Local);
		ScaleStart = SplineComp->GetScaleAtSplinePoint(i);
		ScaleEnd = SplineComp->GetScaleAtSplinePoint(i + 1);
		MeshComp->SetStartAndEnd(StartPos, StartTan, EndPos, EndTan);
		MeshComp->SetStartScale(FVector2D(ScaleStart.Y, ScaleStart.Z));
		MeshComp->SetEndScale(FVector2D(ScaleEnd.Y, ScaleEnd.Z));
		SplineMeshComps.Add(MeshComp);
	}

	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Orange, "CreateMeshSurface() (Spline)");
}

USplineMeshComponent* AUIWSRiver::CreateSplineMeshComp()
{
	USplineMeshComponent* Comp = NewObject<USplineMeshComponent>(this);
	Comp->RegisterComponent();
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(WaterMeshSM);
	Comp->SetMaterial(0, WaterMID);
	Comp->SetMaterial(1, WaterMIDLOD1);
	Comp->AttachToComponent(SplineComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Comp->SetRelativeLocation(FVector(0, 0, 0));
	Comp->SetRelativeScale3D(FVector(1, 1, 1));

	if (bUseCustomCollisionProfile)
	{
		Comp->SetCollisionProfileName(CustomCollisionProfile.Name);
	}
	else if (WaterVolume && MyManager.IsValid() && MyManager->bUseCustomCollisionProfile)
	{
		Comp->SetCollisionProfileName(MyManager->CustomCollisionProfile.Name);
	}
	else
	{
		Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
		Comp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (AllowCameraUnder == false)
	{
		Comp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Block);
	}

	Comp->bCastDynamicShadow = false;
	Comp->bCastVolumetricTranslucentShadow = false;
	Comp->bCastFarShadow = false;
	Comp->bCastStaticShadow = false;
	//comp->SetNotifyRigidBodyCollision(true);
	return Comp;
}


#if WITH_EDITOR
void AUIWSRiver::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Orange, "PostEditChangeProp");
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AUIWSRiver::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	CreateMeshSurface();
}
#endif

void AUIWSRiver::AddToMPC()
{
	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Orange, "AddtoMPC()");
	if (WaterBodyNum != 0 && WaterBodyNum <= UKismetMaterialLibrary::GetScalarParameterValue(
		this, MPC_UIWSWaterBodies, TEXT("SupportedBodyCount")))
	{
		const FString ThisBodyPosName = "WaterBody" + FString::FromInt(WaterBodyNum) + "_Position";
		//Set my position in param collection
		UKismetMaterialLibrary::SetVectorParameterValue(this, MPC_UIWSWaterBodies, FName(*ThisBodyPosName),
		                                                FLinearColor(GetActorLocation()));

		const FString ThisBodyScaleName = "WaterBody" + FString::FromInt(WaterBodyNum) + "_Scale";
		//Set my sale in param collection
		UKismetMaterialLibrary::SetVectorParameterValue(this, MPC_UIWSWaterBodies, FName(*ThisBodyScaleName),
		                                                FLinearColor(GetActorScale3D() * 0));

		const FString ThisBodyRotName = "WaterBody" + FString::FromInt(WaterBodyNum) + "_Rot";
		//Set my sale in param collection
		UKismetMaterialLibrary::SetVectorParameterValue(this, MPC_UIWSWaterBodies, FName(*ThisBodyRotName),
		                                                FLinearColor(GetActorRotation().Euler()));
	}
}

void AUIWSRiver::BeginPlay()
{
	Super::BeginPlay();
	bSupportsEdgeReflection = false;
}

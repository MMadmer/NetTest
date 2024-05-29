// Copyright 2018 Elliot Gray. All Rights Reserved.

#include "UIWSWaterBody.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UIWSCapture.h"
#include "UIWSManager.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/ConstructorHelpers.h"

//DECLARE_CYCLE_STAT(TEXT("UIWS/Automatic Interaction"), STAT_AutoInteraction, STATGROUP_UIWS);
//DECLARE_CYCLE_STAT(TEXT("UIWS/Manual Interaction"), STAT_ManualInteraction, STATGROUP_UIWS);
//DECLARE_CYCLE_STAT(TEXT("UIWS/WaterBody"), STAT_WaterBody, STATGROUP_UIWS);

// Sets default values
AUIWSWaterBody::AUIWSWaterBody()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = false;

	// Box comp that might come in handy later for more aggressive culling/lodding
	BoxComp = CreateDefaultSubobject<UBoxComponent>(TEXT("ActivationCollision"));
	SetRootComponent(BoxComp);
	BoxComp->SetVisibility(false, false);
	BoxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	/** Setup Post Process Volume*/
	WaterVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("Post Process Volume"));
	WaterVolume->SetupAttachment(RootComponent);
	WaterVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	WaterVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	WaterVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	/** Setup Post Process comp*/
	PostProcessComp = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Post Process Comp"));
	PostProcessComp->SetupAttachment(WaterVolume);

	/** Setup the post process settings for underwater visuals*/
	PostProcessComp->Activate();
	PostProcessComp->bUnbound = false;
	PostProcessComp->BlendRadius = 20;
	PostProcessComp->Settings.bOverride_DepthOfFieldFstop = 1;
	PostProcessComp->Settings.bOverride_DepthOfFieldFocalDistance = 1;
	PostProcessComp->Settings.DepthOfFieldFstop = 1.0;
	PostProcessComp->Settings.DepthOfFieldFocalDistance = 50;

	/** Setup bool*/
	bUseCustomCollisionProfile = false;
	bNoLocationSet = false;
	bLowFps = false;
	OverrideWaterMaterials = false;
	bGeneratesInteractiveCaustics = false;
	F = false;
	bIsInteractive = true;
	bDisableAutomaticInteraction = false;
	bDisableAutomaticInteractionNintendoSwitch = true;
	bDisableAutomaticInteractionAndroid = true;
	bDisableAutomaticInteractionIOS = true;
	bTieSimToFPS = false;
	InteractOnDamage = true;
	bLimitTickRate = false;
	bSupportsEdgeReflection = true;
	bEnableParticleOnDamage = true;
	bEnableParticleOnCollision = true;
	bOverrideParticleSettings = false;

#if ENGINE_MINOR_VERSION < 23 && ENGINE_MAJOR_VERSION < 5
	PostProcessComp->Settings.bOverride_DepthOfFieldMethod = 1;
	PostProcessComp->Settings.DepthOfFieldMethod = EDepthOfFieldMethod::DOFM_CircleDOF;
#endif
#if ENGINE_MINOR_VERSION > 22 || ENGINE_MAJOR_VERSION > 4
	PostProcessComp->Settings.bOverride_DepthOfFieldFocalDistance = true;
	PostProcessComp->Settings.DepthOfFieldFocalDistance = 50;
#endif

	//get all the content refs we need
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPC_BodiesAsset(
		TEXT("/UIWS/MPC_UIWSWaterBodies.MPC_UIWSWaterBodies"));
	if (MPC_BodiesAsset.Succeeded())
	{
		MPC_UIWSWaterBodies = MPC_BodiesAsset.Object;
	}
	//get water static mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WaterSM(
		TEXT("/UIWS/Materials/Simulation/Meshes/UIWSMesh16x16fbx.UIWSMesh16x16fbx"));
	if (WaterSM.Succeeded())
	{
		WaterMeshSM = WaterSM.Object;
	}

	//get water material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterMaterialLod1(
		TEXT("/UIWS/Materials/Simulation/UIWSWaterMaterialInst_LOD1.UIWSWaterMaterialInst_LOD1"));
	//transparent water mat
	if (WaterMaterialLod1.Succeeded())
	{
		WaterMatLOD1 = WaterMaterialLod1.Object;
	}

	//get water material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterMaterial(
		TEXT("/UIWS/Materials/Simulation/UIWSWaterMaterial_Inst.UIWSWaterMaterial_Inst")); //transparent water mat
	if (WaterMaterial.Succeeded())
	{
		WaterMat = WaterMaterial.Object;
	}

	//get force splat material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SplatMat(
		TEXT("/UIWS/Materials/Simulation/UIWSForceSplat.UIWSForceSplat"));
	if (SplatMat.Succeeded())
	{
		ForceSplatMat = SplatMat.Object;
	}

	//get manual force splat material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ManSplatMat(
		TEXT("/UIWS/Materials/Simulation/UIWSManualSplat.UIWSManualSplat"));
	if (ManSplatMat.Succeeded())
	{
		ManForceSplatMat = ManSplatMat.Object;
	}

	//get height sim material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HSMat(
		TEXT("/UIWS/Materials/Simulation/UIWSHeightSim.UIWSHeightSim"));
	if (HSMat.Succeeded())
	{
		HeightSimMat = HSMat.Object;
	}

	//get compute normal material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CNMat(
		TEXT("/UIWS/Materials/Simulation/UIWSComputeNormal.UIWSComputeNormal"));
	if (CNMat.Succeeded())
	{
		ComputeNormalMat = CNMat.Object;
	}

	//get global rts
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> gh0(
		TEXT("/UIWS/Materials/Simulation/UIWSHeight0.UIWSHeight0"));
	if (gh0.Succeeded())
	{
		GlobalHeight0 = gh0.Object;
		//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Found gh0");
	}

	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> gh1(
		TEXT("/UIWS/Materials/Simulation/UIWSHeight1.UIWSHeight1"));
	if (gh1.Succeeded())
	{
		GlobalHeight1 = gh1.Object;
		//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Found gh1");
	}

	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> gh2(
		TEXT("/UIWS/Materials/Simulation/UIWSHeight2.UIWSHeight2"));
	if (gh2.Succeeded())
	{
		GlobalHeight2 = gh2.Object;
		//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Found gh2");
	}
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> gn(
		TEXT("/UIWS/Materials/Simulation/UIWSHeightfieldNormal.UIWSHeightfieldNormal"));
	if (gn.Succeeded())
	{
		GlobalNormal = gn.Object;
		//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Found gn");
	}

	//get the underwater post process material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnderWaterPPMat(
		TEXT("/UIWS/Materials/Simulation/PP_UnderWater.PP_UnderWater"));
	if (UnderWaterPPMat.Succeeded())
	{
		PPUnderWaterMat = UnderWaterPPMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UParticleSystem> ds(
		TEXT("/UIWS/ParticleEffects/DefaultSplash.DefaultSplash"));
	if (ds.Succeeded())
	{
		DefaultSplashEffect = ds.Object;
		//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Found gn");
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> nds(
		TEXT("/UIWS/ParticleEffects/Niagara/NS_MeshSplash.NS_MeshSplash"));
	if (nds.Succeeded())
	{
		DefaultNiagaraSplashEffect = nds.Object;
		//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Found gn");
	}

	static ConstructorHelpers::FObjectFinder<UParticleSystem> ief(
		TEXT("/UIWS/ParticleEffects/P_MeshSplash.P_MeshSplash"));
	if (ief.Succeeded())
	{
		InteractionEffect = ief.Object;
		//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Found gn");
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> nief(
		TEXT("/UIWS/ParticleEffects/Niagara/NS_MeshSplash.NS_MeshSplash"));
	if (nief.Succeeded())
	{
		NiagaraInteractionEffect = nief.Object;
		//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Found gn");
	}

	static ConstructorHelpers::FObjectFinder<UParticleSystem> def(
		TEXT("/UIWS/ParticleEffects/P_MeshSplashHit.P_MeshSplashHit"));
	if (def.Succeeded())
	{
		DamageEffect = def.Object;
		//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Found gn");
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ndef(
		TEXT("/UIWS/ParticleEffects/Niagara/NS_MeshSplashHit.NS_MeshSplashHit"));
	if (ndef.Succeeded())
	{
		NiagaraDamageEffect = ndef.Object;
		//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Found gn");
	}
}

void AUIWSWaterBody::AddToMPC()
{
	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, "AddtoMPC()");
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
		                                                FLinearColor(GetActorScale3D() * 1000));

		const FString ThisBodyRotName = "WaterBody" + FString::FromInt(WaterBodyNum) + "_Rot";
		//Set my sale in param collection
		UKismetMaterialLibrary::SetVectorParameterValue(this, MPC_UIWSWaterBodies, FName(*ThisBodyRotName),
		                                                FLinearColor(GetActorRotation().Euler()));
	}
}

// Called when the game starts or when spawned
void AUIWSWaterBody::BeginPlay()
{
	Super::BeginPlay();
	/** Binding to check when player character hits the water surface.
	 *  Used to request body priority switch and make the currently stood in body the one affecting interactive caustics*/
#if !WITH_EDITOR
	CreateMeshSurface();
	TArray<UActorComponent*> StaticComps;
	GetComponents(UStaticMeshComponent::StaticClass(), StaticComps);
	for (auto &comp : StaticComps)
	{
		UStaticMeshComponent* sm = Cast<UStaticMeshComponent>(comp);
		if (sm)
		{
			sm->SetMaterial(0, WaterMID);
			sm->SetMaterial(1, WaterMIDLOD1);
		}
	}
#endif
	//Find Manager
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUIWSManager::StaticClass(), FoundActors);

	if (FoundActors.Num() < 1)
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
			                                 "No UIWS Manager found, have you added one to the persistent level?");
	}
	else
	{
		AUIWSManager* Manager = Cast<AUIWSManager>(FoundActors[0]);
		if (Manager)
		{
			MyManager = Manager;
		}
		else
		{
			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
				                                 "Something went wrong.  Couldn't find a UIWS water manager");
		}
	}

	if (bUseCustomCollisionProfile)
	{
		WaterVolume->SetCollisionProfileName(CustomCollisionProfile.Name);
	}
	else if (WaterVolume && MyManager.IsValid() && MyManager->bUseCustomCollisionProfile)
	{
		WaterVolume->SetCollisionProfileName(MyManager->CustomCollisionProfile.Name);
	}

	RegisterWithManager();
	if (GetWorld()->GetNetMode() == NM_DedicatedServer || GetGameInstance()->GetWorldContext()->RunAsDedicated)
	{
		bIsInteractive = false;
	}
	if (bIsInteractive)
	{
		WaterVolume->OnComponentBeginOverlap.AddDynamic(this, &AUIWSWaterBody::OnWaterOverlap);
		PPUnderWaterMID = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, PPUnderWaterMat);
		PostProcessComp->AddOrUpdateBlendable(PPUnderWaterMID);

		if (bLimitTickRate)
		{
			ChangeBodyTickRate(TickRate);
		}
		SetWaterVisualParams();
		//ApplyInteractivityForces();
		//PropagateRipples(0.016);

		if (UGameplayStatics::GetPlayerPawn(this, 0))
		{
			if (WaterVolume->IsOverlappingActor(UGameplayStatics::GetPlayerPawn(this, 0)))
			{
				RequestPriorityManual();
			}
		}
	}

	ForceSplatInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, ForceSplatMat);
	HeightSimInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, HeightSimMat);
	ManualSplatInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, ManForceSplatMat);
	ComputeNormalInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, ComputeNormalMat);
}

void AUIWSWaterBody::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (MyManager.IsValid())
	{
		MyManager->UpdateRegistration(false, this);
	}
}

void AUIWSWaterBody::ApplyInteractivityForces()
{
	ForceSplatInst->SetTextureParameterValue(TEXT("RTPersistentIn"), MyCaptureRT);

	//Get player position in interactive UV space
	const FLinearColor UVLC = UKismetMaterialLibrary::GetVectorParameterValue(
		this, MPC_UIWSWaterBodies, TEXT("playerpos"));;
	const FVector WPVec = FVector(UVLC.R, UVLC.G, 0);
	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Blue, WorldPosToRelativeUV(WPVec).ToString());
	ForceSplatInst->SetVectorParameterValue(TEXT("ForcePosition"), FLinearColor(WorldPosToRelativeUV(WPVec)));
	ForceSplatInst->SetScalarParameterValue(TEXT("ForceStrength"), 1);
	if (GetHeightRT(HeightState) != nullptr)
	{
		//badUKismetRenderingLibrary::DrawMaterialToRenderTarget(this, GetHeightRT(iHeightState), ForceSplatInst);
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, GetHeightRT(HeightState), Canvas, Size, Context);
		Canvas->K2_DrawMaterial(ForceSplatInst, FVector2D(0, 0), Size, FVector2D(0, 0), FVector2D(1, 1), 0.0f,
		                        FVector2D(0, 0));
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	}
}

void AUIWSWaterBody::PropagateRipples(const float InDeltaTime)
{
	if (bTieSimToFPS)
	{
		HeightState = (HeightState + 1) % 3;
		if (GetLastHeightRT(HeightState, 1) != nullptr)
		{
			HeightSimInst->SetTextureParameterValue(TEXT("PreviousHeight1"), GetLastHeightRT(HeightState, 1));
		}
		if (GetLastHeightRT(HeightState, 2) != nullptr)
		{
			HeightSimInst->SetTextureParameterValue(TEXT("PreviousHeight2"), GetLastHeightRT(HeightState, 2));
		}

		if (GetHeightRT(HeightState) != nullptr)
		{
			UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, GetHeightRT(HeightState), Canvas, Size,
			                                                       Context);
			Canvas->K2_DrawMaterial(HeightSimInst, FVector2D(0, 0), Size, FVector2D(0, 0), FVector2D(1, 1), 0.0f,
			                        FVector2D(0, 0));
			UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
		}
	}
	else
	{
		//propagate ripples as per requested simulation rate
		TimeAccumulator = TimeAccumulator + InDeltaTime;
		const float ac1 = 1 / FMath::Min(UpdateRate, 120.0f);

		while (TimeAccumulator >= ac1)
		{
			TimeAccumulator = TimeAccumulator - ac1;
			HeightState = (HeightState + 1) % 3;
			//UMaterialInstanceDynamic* HeightSimInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, HeightSimMat);
			if (GetLastHeightRT(HeightState, 1) != nullptr)
			{
				HeightSimInst->SetTextureParameterValue(TEXT("PreviousHeight1"), GetLastHeightRT(HeightState, 1));
			}
			if (GetLastHeightRT(HeightState, 2) != nullptr)
			{
				HeightSimInst->SetTextureParameterValue(TEXT("PreviousHeight2"), GetLastHeightRT(HeightState, 2));
			}

			if (GetHeightRT(HeightState) != nullptr)
			{
				UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, GetHeightRT(HeightState), Canvas, Size,
				                                                       Context);
				Canvas->K2_DrawMaterial(HeightSimInst, FVector2D(0, 0), Size, FVector2D(0, 0), FVector2D(1, 1), 0.0f,
				                        FVector2D(0, 0));
				UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
			}
		}
	}

	//compute normals once per frame
	if (GetHeightRT(HeightState) != nullptr)
	{
		//UMaterialInstanceDynamic* ComputeNormalInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, ComputeNormalMat);
		ComputeNormalInst->SetTextureParameterValue(TEXT("Heightfield"), GetHeightRT(HeightState));
		//badUKismetRenderingLibrary::DrawMaterialToRenderTarget(this, activenormal, ComputeNormalInst);

		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, ActiveNormal, Canvas, Size, Context);
		Canvas->K2_DrawMaterial(ComputeNormalInst, FVector2D(0, 0), Size, FVector2D(0, 0), FVector2D(1, 1), 0.0f,
		                        FVector2D(0, 0));
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);


		WaterMID->SetTextureParameterValue(TEXT("Heightfield"), GetHeightRT(HeightState));
		WaterMIDLOD1->SetTextureParameterValue(TEXT("Heightfield"), GetHeightRT(HeightState));
	}

	WaterMID->SetTextureParameterValue(TEXT("HeightfieldNormal"), ActiveNormal);
	WaterMIDLOD1->SetTextureParameterValue(TEXT("HeightfieldNormal"), ActiveNormal);

	if (GetHeightRT(HeightState) == nullptr)
	{
		//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, "HEIGHT RT IS NULL");
	}
	if (ActiveNormal == nullptr)
	{
		//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, "ACTIVENORMAL RT IS NULL");
	}
	if (GetLastHeightRT(HeightState, 1) == nullptr)
	{
		//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, "HEIGHT RT LAST1 IS NULL");
	}
	if (GetLastHeightRT(HeightState, 2) == nullptr)
	{
		//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, "HEIGHT RT LAST2 IS NULL");
	}
}

float AUIWSWaterBody::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
                                 AActor* DamageDealer)
{
	/*UDamageType const* DamageTypeCDO = DamageEvent.DamageTypeClass
		                                         ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>()
		                                         : GetDefault<UDamageType>();*/

	//Handle Point damage
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* PDEvent = (FPointDamageEvent*)&DamageEvent;
		if (InteractOnDamage)
		{
			PointDamageSplashAtLocation(PDEvent->HitInfo.ImpactPoint, DamageAmount);
		}
	}
	//Handle radial Damage
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		const FRadialDamageEvent* PDEvent = (FRadialDamageEvent*)&DamageEvent;
		if (InteractOnDamage)
		{
			RadialDamageSplashAtLocation(PDEvent->Origin, DamageAmount);
		}
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageDealer);
}

void AUIWSWaterBody::SplashAtLocation(FVector SplashLoc, FVector SplashVelocity, float SplashStrengthPercent)
{
	//SCOPE_CYCLE_COUNTER(STAT_ManualInteraction);
	// Don't spawn vfx on a dedi
	if ((GetWorld()->GetNetMode() == NM_DedicatedServer))
	{
		SplashedAtLocation(SplashLoc, SplashVelocity, SplashStrengthPercent);
		return;
	}

	UNiagaraSystem* NiagaraTemplate;
	if (NiagaraInteractionEffect)
	{
		NiagaraTemplate = NiagaraInteractionEffect;
	}
	else
	{
		NiagaraTemplate = DefaultNiagaraSplashEffect;
	}

	if (NiagaraTemplate)
	{
		UNiagaraComponent* NiagaraSplash = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, NiagaraTemplate, (bOverrideParticleSettings ? (SplashLoc + InteractionEffectOffset) : SplashLoc),
			FRotator::ZeroRotator,
			FVector(UKismetMathLibrary::Lerp(InteractionEffectScaleMin, InteractionEffectScaleMax,
			                                 SplashStrengthPercent)), true);
		NiagaraSplash->SetRenderCustomDepth(true);
		NiagaraSplash->SetCustomDepthStencilValue(1);
		NiagaraSplash->SetVariableVec3(SplashVelocityNiagaraParamName, SplashVelocity);

		SplashedAtLocation(SplashLoc, SplashVelocity, SplashStrengthPercent);
		return;
	}

	UParticleSystem* SysToSpawn;
	if (InteractionEffect)
	{
		SysToSpawn = InteractionEffect;
	}
	else
	{
		SysToSpawn = DefaultSplashEffect;
	}


	if (NiagaraWarningCounter < 1000 && NiagaraWarningCounter % 30 == 0)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT(
			       "Cascade support will be removed in a future release.  Please upgrade your UIWS splash FX overrides to Niagara systems"
		       ));
	}
	++NiagaraWarningCounter;

	UParticleSystemComponent* Splash = UGameplayStatics::SpawnEmitterAtLocation(
		this, SysToSpawn, (bOverrideParticleSettings ? SplashLoc + InteractionEffectOffset : SplashLoc),
		FRotator::ZeroRotator, true);
	Splash->SetRenderCustomDepth(true);
	Splash->SetCustomDepthStencilValue(1);
	Splash->SetWorldScale3D(FVector(
		UKismetMathLibrary::Lerp(InteractionEffectScaleMin, InteractionEffectScaleMax, SplashStrengthPercent)));
	Splash->SetVectorParameter(TEXT("SplashVelocity"), SplashVelocity);

	SplashedAtLocation(SplashLoc, SplashVelocity, SplashStrengthPercent);
	//Splash->SetWorldScale3D(FVector(SplashStrengthPercent));
}

void AUIWSWaterBody::PointDamageSplashAtLocation(FVector SplashLoc, float DamageAmount /*= 1.0f*/)
{
	//SCOPE_CYCLE_COUNTER(STAT_ManualInteraction);
	const float RippleStrengthScaled = FMath::GetMappedRangeValueClamped(
		FVector2D(5, 10), FVector2D(0, DamageEffectScaleMax),
		DamageAmount);
	const float RippleSizeScaled = FMath::GetMappedRangeValueClamped(FVector2D(0, 1),
	                                                                 FVector2D(0, DamageEffectScaleMax),
	                                                                 DamageAmount);
	// Zero the splash spawn location in case of radial damage origin
	// SplashLoc.Z = GetActorLocation().Z;
	const float DamageAlpha = FMath::GetMappedRangeValueClamped(FVector2D(0, DamageScale), FVector2D(0, 1),
	                                                            DamageAmount);
	const float ParticleScaleMult = FMath::GetMappedRangeValueClamped(FVector2D(0, DamageScale),
	                                                                  FVector2D(DamageEffectScaleMin,
		                                                                  DamageEffectScaleMax),
	                                                                  DamageAmount);

	// No visual effects on server
	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		OnPointDamageEffect(SplashLoc, RippleStrengthScaled, RippleSizeScaled, DamageAmount);
		return;
	}

	// Render Ripples
	ApplyForceAtLocation(RippleStrengthScaled, RippleSizeScaled, SplashLoc);

	UNiagaraSystem* NiagaraTemplate;
	if (NiagaraDamageEffect)
	{
		NiagaraTemplate = NiagaraDamageEffect;
	}
	else
	{
		NiagaraTemplate = DefaultNiagaraSplashEffect;
	}

	if (NiagaraTemplate && bEnableParticleOnDamage)
	{
		UNiagaraComponent* NiagaraSplash = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, NiagaraTemplate, (bOverrideParticleSettings ? SplashLoc + InteractionEffectOffset : SplashLoc),
			FRotator::ZeroRotator, FVector(ParticleScaleMult), true);
		NiagaraSplash->SetRenderCustomDepth(true);
		NiagaraSplash->SetCustomDepthStencilValue(1);
		NiagaraSplash->SetVariableFloat(DamageAlphaNiagaraParamName, DamageAlpha);

		OnPointDamageEffect(SplashLoc, RippleStrengthScaled, RippleSizeScaled, DamageAmount);
		return;
	}

	UParticleSystem* SysToSpawn;
	if (DamageEffect)
	{
		SysToSpawn = DamageEffect;
	}
	else
	{
		SysToSpawn = DefaultSplashEffect;
	}

	//Spawn particle
	if (bEnableParticleOnDamage)
	{
		if (NiagaraWarningCounter < 1000 && NiagaraWarningCounter % 30 == 0)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "Cascade support will be removed in a future release.  Please upgrade your UIWS Damage FX overrides to Niagara systems"
			       ));
		}
		++NiagaraWarningCounter;

		UParticleSystemComponent* Splash = UGameplayStatics::SpawnEmitterAtLocation(
			this, SysToSpawn, (bOverrideParticleSettings ? SplashLoc + InteractionEffectOffset : SplashLoc),
			FRotator::ZeroRotator, true);
		Splash->SetRenderCustomDepth(true);
		Splash->SetCustomDepthStencilValue(1);
		Splash->SetWorldScale3D(FVector(ParticleScaleMult));
	}
	OnPointDamageEffect(SplashLoc, RippleStrengthScaled, RippleSizeScaled, DamageAmount);
}

void AUIWSWaterBody::RadialDamageSplashAtLocation(FVector SplashLoc, float DamageAmount /*= 100.0f*/)
{
	//SCOPE_CYCLE_COUNTER(STAT_ManualInteraction);

	// Render Ripples
	const float RippleStrengthScaled = FMath::GetMappedRangeValueClamped(
		FVector2D(5, 10), FVector2D(0, DamageEffectScaleMax),
		DamageAmount);
	const float RippleSizeScaled = FMath::GetMappedRangeValueClamped(FVector2D(0, 1),
	                                                                 FVector2D(0, DamageEffectScaleMax),
	                                                                 DamageAmount);
	// Zero the splash spawn location incase of radial damage origin
	SplashLoc.Z = GetActorLocation().Z;
	const float DamageAlpha = FMath::GetMappedRangeValueClamped(FVector2D(0, DamageScale), FVector2D(0, 1),
	                                                            DamageAmount);
	const float ParticleScaleMult = FMath::GetMappedRangeValueClamped(FVector2D(0, DamageScale),
	                                                                  FVector2D(DamageEffectScaleMin,
		                                                                  DamageEffectScaleMax),
	                                                                  DamageAmount);

	// No visual effects on server
	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		OnRadialDamageEffect(SplashLoc, RippleStrengthScaled, RippleSizeScaled, DamageAmount);
		return;
	}

	ApplyForceAtLocation(RippleStrengthScaled, RippleSizeScaled, SplashLoc);

	UNiagaraSystem* NiagaraTemplate;
	if (NiagaraDamageEffect)
	{
		NiagaraTemplate = NiagaraDamageEffect;
	}
	else
	{
		NiagaraTemplate = DefaultNiagaraSplashEffect;
	}

	if (NiagaraTemplate && bEnableParticleOnDamage)
	{
		UNiagaraComponent* NiagaraSplash = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, NiagaraTemplate, (bOverrideParticleSettings ? SplashLoc + DamageEffectOffset : SplashLoc),
			FRotator::ZeroRotator, FVector(ParticleScaleMult), true);
		NiagaraSplash->SetRenderCustomDepth(true);
		NiagaraSplash->SetCustomDepthStencilValue(1);
		NiagaraSplash->SetVariableFloat(DamageAlphaNiagaraParamName, DamageAlpha);

		OnRadialDamageEffect(SplashLoc, RippleStrengthScaled, RippleSizeScaled, DamageAmount);
		return;
	}

	UParticleSystem* SysToSpawn;
	if (DamageEffect)
	{
		SysToSpawn = DamageEffect;
	}
	else
	{
		SysToSpawn = DefaultSplashEffect;
	}

	//Spawn particle
	if (bEnableParticleOnDamage)
	{
		if (NiagaraWarningCounter < 1000 && NiagaraWarningCounter % 30 == 0)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "Cascade support will be removed in a future release.  Please upgrade your UIWS Damage FX overrides to Niagara systems"
			       ));
		}
		++NiagaraWarningCounter;
		UParticleSystemComponent* Splash = UGameplayStatics::SpawnEmitterAtLocation(
			this, SysToSpawn, (bOverrideParticleSettings ? SplashLoc + DamageEffectOffset : SplashLoc),
			FRotator::ZeroRotator, true);
		Splash->SetWorldScale3D(FVector(ParticleScaleMult));
	}
	OnRadialDamageEffect(SplashLoc, RippleStrengthScaled, RippleSizeScaled, DamageAmount);
}

FVector AUIWSWaterBody::WorldPosToRelativeUV(const FVector WorldPos)
{
	const float IntDistance = UKismetMaterialLibrary::GetScalarParameterValue(this, MPC_UIWSWaterBodies,
	                                                                          TEXT("InteractiveDistance"));

	const float x = (UKismetMathLibrary::GenericPercent_FloatFloat(WorldPos.X + (IntDistance / 2), IntDistance)) / -
		IntDistance;
	const float y = (UKismetMathLibrary::GenericPercent_FloatFloat(WorldPos.Y + (IntDistance / 2), IntDistance)) /
		IntDistance;
	return FVector(x, y, 0);
}

UTextureRenderTarget2D* AUIWSWaterBody::SetupCaptureCPP()
{
	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, "SetupCaptureCPP()");
	FActorSpawnParameters SpawnParams;
	FRotator SpawnRot;
	SpawnParams.Owner = this;
	SpawnRot.Pitch = GetActorRotation().Pitch;
	SpawnRot.Roll = GetActorRotation().Roll;
	SpawnRot.Yaw = 0.0f;
	MyCaptureActor = GetWorld()->SpawnActor<AUIWSCapture>(GetActorLocation() + FVector(0, 0, CaptureOffset), SpawnRot,
	                                                      SpawnParams);
	return MyCaptureActor->SetupCapture(EdgeDepth, CaptureRes);
}

void AUIWSWaterBody::InitializeWaterMaterial(const bool bUpdate)
{
	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, "InitializeWaterMaterial() Update = " + UKismetStringLibrary::Conv_BoolToString(bUpdate));
	if (!bUpdate)
	{
		UMaterialInterface* ThisWaterMat;
		UMaterialInterface* ThisDistantMat;
		if (OverrideWaterMaterials == true && CustomWaterMaterial != nullptr)
		{
			ThisWaterMat = CustomWaterMaterial;
			if (CustomWaterMaterialDistant != nullptr)
			{
				ThisDistantMat = CustomWaterMaterialDistant;
			}
			else
			{
				ThisDistantMat = CustomWaterMaterial;
			}
		}
		else
		{
			ThisWaterMat = WaterMat;
			ThisDistantMat = WaterMatLOD1;
		}
		WaterMID = UMaterialInstanceDynamic::Create(ThisWaterMat, this);
		WaterMIDLOD1 = UMaterialInstanceDynamic::Create(ThisDistantMat, this);
		/*
		for (auto &XMesh : XMeshes)
		{
			XMesh->SetMaterial(0, WaterMID);;
			XMesh->SetMaterial(1, WaterMIDLOD1);
		}

		for (auto &YMesh : YMeshes)
		{
			YMesh->SetMaterial(0, WaterMID);;
			YMesh->SetMaterial(1, WaterMIDLOD1);
		}
		*/
		//WaterMeshComp->SetMaterial(0,WaterMID);
		//WaterMeshComp->SetMaterial(1, WaterMIDLOD1);

		TArray<UActorComponent*> StaticComps;
		GetComponents(UStaticMeshComponent::StaticClass(), StaticComps);
		for (const auto& Comp : StaticComps)
		{
			UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(Comp);
			if (SM)
			{
				SM->SetMaterial(0, WaterMID);
				SM->SetMaterial(1, WaterMIDLOD1);
			}
		}

		SetWaterVisualParams();
	}
	//WaterMID->SetScalarParameterValue(TEXT("MyHeight(Needs Replacement)"), GetActorLocation().Z); //this is dumb change this functionality in the shader 
}

void AUIWSWaterBody::InitializeRenderTargets(const bool bUpdate)
{
	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, "InitializeRenderTargets() Update = " + UKismetStringLibrary::Conv_BoolToString(bUpdate));
	if (bGeneratesInteractiveCaustics == true)
	{
		//Clear global render targets ready for use
		UKismetRenderingLibrary::ClearRenderTarget2D(this, GlobalHeight0);
		UKismetRenderingLibrary::ClearRenderTarget2D(this, GlobalHeight1);
		UKismetRenderingLibrary::ClearRenderTarget2D(this, GlobalHeight2);
		UKismetRenderingLibrary::ClearRenderTarget2D(this, GlobalNormal);
		if (HeightSimInst)
			HeightSimInst->SetScalarParameterValue(TEXT("SupportsReflection"), bSupportsEdgeReflection);
		if (bUpdate)
		{
			//if this is an update (ie, not first init) we need to copy the local ripple rt's to the global so the existing ripples persist.  This is pretty heavy so only persisting for the new primary body and letting the others just reset

			UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, GlobalHeight0, Canvas, Size, Context);
			Canvas->K2_DrawTexture(LocalHeight0, FVector2D(0, 0), Size, FVector2D(0, 0), FVector2D(1, 1),
			                       FLinearColor::White, EBlendMode::BLEND_Opaque);
			UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);

			UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, GlobalHeight1, Canvas, Size, Context);
			Canvas->K2_DrawTexture(LocalHeight1, FVector2D(0, 0), Size, FVector2D(0, 0), FVector2D(1, 1),
			                       FLinearColor::White, EBlendMode::BLEND_Opaque);
			UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);

			UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, GlobalHeight2, Canvas, Size, Context);
			Canvas->K2_DrawTexture(LocalHeight2, FVector2D(0, 0), Size, FVector2D(0, 0), FVector2D(1, 1),
			                       FLinearColor::White, EBlendMode::BLEND_Opaque);
			UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);

			UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, GlobalNormal, Canvas, Size, Context);
			Canvas->K2_DrawTexture(LocalNormal, FVector2D(0, 0), Size, FVector2D(0, 0), FVector2D(1, 1),
			                       FLinearColor::White, EBlendMode::BLEND_Opaque);
			UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
		}
		ActiveHeight0 = GlobalHeight0;
		ActiveHeight1 = GlobalHeight1;
		ActiveHeight2 = GlobalHeight2;
		ActiveNormal = GlobalNormal;
	}
	else
	{
		//create local render targets if they don't already exist
		if (LocalHeight0 == nullptr)
		{
			LocalHeight0 = UKismetRenderingLibrary::CreateRenderTarget2D(this, SimResMin, SimResMin);
		}
		if (LocalHeight1 == nullptr)
		{
			LocalHeight1 = UKismetRenderingLibrary::CreateRenderTarget2D(this, SimResMin, SimResMin);
		}
		if (LocalHeight2 == nullptr)
		{
			LocalHeight2 = UKismetRenderingLibrary::CreateRenderTarget2D(this, SimResMin, SimResMin);
		}
		if (LocalNormal == nullptr)
		{
			LocalNormal = UKismetRenderingLibrary::CreateRenderTarget2D(this, SimResMin, SimResMin);
		}
		UKismetRenderingLibrary::ClearRenderTarget2D(this, LocalHeight0);
		UKismetRenderingLibrary::ClearRenderTarget2D(this, LocalHeight1);
		UKismetRenderingLibrary::ClearRenderTarget2D(this, LocalHeight2);
		UKismetRenderingLibrary::ClearRenderTarget2D(this, LocalNormal);
		if (HeightSimInst)
			HeightSimInst->SetScalarParameterValue(TEXT("SupportsReflection"), 0);
		ActiveHeight0 = LocalHeight0;
		ActiveHeight1 = LocalHeight1;
		ActiveHeight2 = LocalHeight2;
		ActiveNormal = LocalNormal;
	}

	WaterMID->SetTextureParameterValue(TEXT("HeightfieldNormal"), ActiveNormal);
	//Only spawn and configure a scene capture if we're initializing for the first time

	/** Platform specific interaction type overrides*/
#if PLATFORM_SWITCH
	if(!bDisableAutomaticInteraction)
	{
		bDisableAutomaticInteraction = bDisableAutomaticInteractionNintendoSwitch;
	}
#endif
	//#if PLATFORM_XBOXONE
	//	if (!bDisableAutomaticInteraction)
	//	{
	//		bDisableAutomaticInteraction = bDisableAutomaticInteractionXbox;
	//	}
	//#endif
	//#if PLATFORM_PS4
	//	if (!bDisableAutomaticInteraction)
	//	{
	//		bDisableAutomaticInteraction = bDisableAutomaticInteractionPS4;
	//	}
	//#endif
#if PLATFORM_ANDROID
	if (!bDisableAutomaticInteraction)
	{
		bDisableAutomaticInteraction = bDisableAutomaticInteractionAndroid;
	}
#endif
#if PLATFORM_IOS
	if (!bDisableAutomaticInteraction)
	{
		bDisableAutomaticInteraction = bDisableAutomaticInteractionIOS;
	}
#endif

	if (!bUpdate && !bDisableAutomaticInteraction)
	{
		MyCaptureRT = SetupCaptureCPP();
	}
}

void AUIWSWaterBody::ChangeBodyTickRate(const float NewTickRate)
{
	if (NewTickRate == 0)
	{
		SetActorTickInterval(0);
		if (MyCaptureActor)
		{
			MyCaptureActor->SetActorTickInterval(0);
		}
	}
	else
	{
		SetActorTickInterval(1 / NewTickRate);
		if (MyCaptureActor)
		{
			MyCaptureActor->SetActorTickInterval(1 / NewTickRate);
		}
	}
}

UTextureRenderTarget2D* AUIWSWaterBody::GetHeightRT(const int IndexIn) const
{
	if (IndexIn == 0)
	{
		return ActiveHeight0;
	}
	else if (IndexIn == 1)
	{
		return ActiveHeight1;
	}
	else
	{
		return ActiveHeight2;
	}
}

void AUIWSWaterBody::OnWaterOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                    const FHitResult& SweepResult)
{
	//SCOPE_CYCLE_COUNTER(STAT_ManualInteraction);

	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, "OnWaterOverlap()");
	if (OtherActor == UGameplayStatics::GetPlayerPawn(this, 0))
	{
		//CheckIfCulled();
		//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, "Pawn overlapped me oof");
		if (MyManager.IsValid())
		{
			MyManager->RequestPriority(this);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Manager pointer was invalid when RequestPriorityManual() was called"));
		}
	}
	if (bEnableParticleOnCollision)
	{
		FVector Loc = OtherActor->GetActorLocation();
		Loc.Z = GetActorLocation().Z;
		FVector Vel = OtherActor->GetVelocity();
		Vel.Z = -Vel.Z;
		SplashAtLocation(Loc, Vel, 1);
	}
}

void AUIWSWaterBody::RegisterWithManager()
{
	//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Water body Registering with Manager");
	if (MyManager.IsValid())
	{
		MyManager->UpdateRegistration(true, this);
		InitializeWaterMaterial(false);
		InitializeRenderTargets(false);
	}
	else
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
			                                 "Body failed to register with manager as MyManager variable was null");
	}
}

void AUIWSWaterBody::UnRegisterWithManager()
{
	//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Water body Un-Registering with Manager");
	if (MyManager.IsValid())
	{
		MyManager->UpdateRegistration(false, this);
	}
	else
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
			                                 "Body failed to Un-register with manager as MyManager variable was null");
	}
}

// Called every frame
void AUIWSWaterBody::Tick(float DeltaTime)
{
	//SCOPE_CYCLE_COUNTER(STAT_WaterBody);
	Super::Tick(DeltaTime);
	bLowFps = DeltaTime > 0.2f;

	if (bIsInteractive)
	{
		/*Unfortunately edge bounce introduces artefacting at low fps.  Unavoidable cost of entry*/
		if (bSupportsEdgeReflection && HeightSimInst)
		{
			if (bLowFps)
				HeightSimInst->SetScalarParameterValue(TEXT("SupportsReflection"), 0);
			else
				HeightSimInst->SetScalarParameterValue(TEXT("SupportsReflection"), 1);
		}
		ApplyInteractivityForces();
		PropagateRipples(DeltaTime);
	}
}


#if WITH_EDITOR
void AUIWSWaterBody::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, "PostEditChangeProperty()");
	Super::PostEditChangeProperty(PropertyChangedEvent);
	//bIsInteractive = true;
	SetWaterVisualParams();
	//if(Location != FVector(1333333.3, 13333.33, 1333333.335))
	//{
	//SetActorLocation(Location);
	//SetActorScale3D(Scale);
	//}
}

void AUIWSWaterBody::PostEditMove(bool bFinished)
{
	//CreateMeshSurface();
	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, "PostEditMove()");
	Super::PostEditMove(bFinished);
	//SetActorRotation(FRotator::ZeroRotator);

	//Location = GetActorLocation();
	//Scale = GetActorScale();

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
		                                                FLinearColor(GetActorScale3D() * 1000));


		const FString ThisBodyRotName = "WaterBody" + FString::FromInt(WaterBodyNum) + "_Rot";
		//Set my sale in param collection

		UKismetMaterialLibrary::SetVectorParameterValue(this, MPC_UIWSWaterBodies, FName(*ThisBodyRotName),
		                                                FLinearColor(FVector(
			                                                GetActorRotation().Roll, GetActorRotation().Pitch,
			                                                GetActorRotation().Yaw)));
	}
}

void AUIWSWaterBody::EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	Super::EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);
	FRotator ClearedRotation;
	//ClearedRotation.Pitch = FMath::Clamp(GetActorRotation().Pitch, -15.0f, 15.0f);
	//ClearedRotation.Roll = FMath::Clamp(GetActorRotation().Roll,-15.0f,15.0f);
	ClearedRotation.Pitch = 0;
	ClearedRotation.Roll = 0;
	ClearedRotation.Yaw = GetActorRotation().Yaw;
	SetActorRotation(ClearedRotation);

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
		                                                FLinearColor(GetActorScale3D() * 1000));

		const FString ThisBodyRotName = "WaterBody" + FString::FromInt(WaterBodyNum) + "_Rot";
		//Set my sale in param collection
		UKismetMaterialLibrary::SetVectorParameterValue(this, MPC_UIWSWaterBodies, FName(*ThisBodyRotName),
		                                                FLinearColor(GetActorRotation().Euler()));
	}
}
#endif

void AUIWSWaterBody::BeginDestroy()
{
	//Clean up my reference in the MPC
	if (WaterBodyNum != 0 && WaterBodyNum <= UKismetMaterialLibrary::GetScalarParameterValue(
		this, MPC_UIWSWaterBodies, TEXT("SupportedBodyCount")))
	{
		const FString ThisBodyPosName = "WaterBody" + FString::FromInt(WaterBodyNum) + "_Position";
		//Set my position in param collection
		UKismetMaterialLibrary::SetVectorParameterValue(this, MPC_UIWSWaterBodies, FName(*ThisBodyPosName),
		                                                FLinearColor(FVector(0)));

		const FString ThisBodyScaleName = "WaterBody" + FString::FromInt(WaterBodyNum) + "_Scale";
		//Set my sale in param collection
		UKismetMaterialLibrary::SetVectorParameterValue(this, MPC_UIWSWaterBodies, FName(*ThisBodyScaleName),
		                                                FLinearColor(FVector(0)));
	}
	Super::BeginDestroy();
}

void AUIWSWaterBody::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (bUseCustomCollisionProfile)
	{
		WaterVolume->SetCollisionProfileName(CustomCollisionProfile.Name);
	}
	else if (WaterVolume && MyManager.IsValid() && MyManager->bUseCustomCollisionProfile)
	{
		WaterVolume->SetCollisionProfileName(MyManager->CustomCollisionProfile.Name);
	}
}

void AUIWSWaterBody::SetWaterVisualParams()
{
	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, "SetWaterVisualParams()");
	if (WaterMID)
	{
		WaterMID->SetVectorParameterValue(TEXT("WaterColor"), WaterColor);
		WaterMID->SetVectorParameterValue(TEXT("DeepWater"), DeepOpacity);
		WaterMID->SetVectorParameterValue(TEXT("ShallowWater"), ShallowOpacity);
		WaterMID->SetScalarParameterValue(TEXT("Water Depth"), DepthTransitionDistance);
		WaterMID->SetScalarParameterValue(TEXT("Shallow EdgeFadeDistance"), ShallowEdgeFadeDistance);
		WaterMID->SetScalarParameterValue(TEXT("ShallowEdgeFadePower"), ShallowEdgeFadePower);

		WaterMIDLOD1->SetVectorParameterValue(TEXT("WaterColor"), WaterColor);
		WaterMIDLOD1->SetVectorParameterValue(TEXT("DeepWater"), DeepOpacity);
		WaterMIDLOD1->SetVectorParameterValue(TEXT("ShallowWater"), ShallowOpacity);
		WaterMIDLOD1->SetScalarParameterValue(TEXT("Water Depth"), DepthTransitionDistance);
		WaterMIDLOD1->SetScalarParameterValue(TEXT("Shallow EdgeFadeDistance"), ShallowEdgeFadeDistance);
		WaterMIDLOD1->SetScalarParameterValue(TEXT("ShallowEdgeFadePower"), ShallowEdgeFadePower);
	}


	if (PPUnderWaterMID)
	{
		PPUnderWaterMID->SetVectorParameterValue(TEXT("UnderWaterTint"), WaterColor);
		PPUnderWaterMID->SetVectorParameterValue(TEXT("MyBodyPos"), FLinearColor(GetActorLocation()));
		PPUnderWaterMID->SetVectorParameterValue(TEXT("MyBodyScale"), FLinearColor(GetActorScale3D() * 1000));
		PPUnderWaterMID->SetScalarParameterValue(TEXT("MaxUnderwaterBrightness"), UnderWaterBrightnessMult);
	}

	if (WaterVolume)
	{
		WaterVolume->SetRelativeScale3D(FVector(1, 1, 1));
		WaterVolume->SetBoxExtent(FVector(500, 500, 500));
		const FVector Rot = FVector(500 * GetActorScale3D().X, 500 * GetActorScale3D().Y, 0);
		//FVector NewPos = rot.RotateAngleAxis(GetActorRotation().Yaw,FVector(0,0,1));
		FVector NewPos = Rot.RotateAngleAxis(GetActorRotation().Roll, FVector(1, 0, 0));
		NewPos = NewPos.RotateAngleAxis(GetActorRotation().Pitch, FVector(0, 1, 0));
		NewPos = NewPos.RotateAngleAxis(GetActorRotation().Yaw, FVector(0, 0, 1));

		//WaterVolume->SetWorldLocation(GetActorLocation() + FVector(500 * GetActorScale3D().X, 500 * GetActorScale3D().Y, -(WaterVolume->GetScaledBoxExtent().Z)));
		WaterVolume->SetWorldLocation(
			GetActorLocation() + FVector(NewPos.X, NewPos.Y, -(WaterVolume->GetScaledBoxExtent().Z)));
		//WaterVolume->SetRelativeLocation(FVector::ZeroVector);
		//WaterVolume->AddRelativeLocation(FVector(500*GetActorScale3D().X, 500 * GetActorScale3D().Y, -1000 * GetActorScale3D().Y));
	}
}

void AUIWSWaterBody::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);


	InitializeWaterMaterial(false);
	CreateMeshSurface();
}

void AUIWSWaterBody::CheckIfCulled()
{
	//if (GEngine) //GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, "Checking if culled");
	// Do fps culling tasks
	// If the body is culled do the sim at a low framerate.
	// Will still do the caustic sim etc and be ready to go when it becomes visible but this should help performance
	// Stops the scene capture from rendering if the body isn't in view
	constexpr bool bIsVisible = true; // GetWorld()->GetTimeSeconds() - WaterMeshComp->LastRenderTimeOnScreen <= 0.5f;
	if (!bIsVisible)
	{
		//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Orange, "Culling a body");
		ChangeBodyTickRate(UKismetMathLibrary::RandomFloatInRange(1, 2));
		MyCaptureActor->SceneCaptureComp->SetComponentTickInterval(1 / UKismetMathLibrary::RandomFloatInRange(9, 11));
		MyCaptureActor->SceneCaptureComp->SetHiddenInGame(true);
	}
	else if (bLimitTickRate)
	{
		ChangeBodyTickRate(TickRate);
		MyCaptureActor->SceneCaptureComp->SetComponentTickInterval(1 / TickRate);
		//myCaptureActor->SetActorHiddenInGame(false);
		//myCaptureRT = SetupCaptureCPP();
		MyCaptureActor->SceneCaptureComp->SetHiddenInGame(false);
	}
	else
	{
		ChangeBodyTickRate(0.0f);
		MyCaptureActor->SceneCaptureComp->SetComponentTickInterval(1 / 10);
		//myCaptureActor->SetActorHiddenInGame(false);
		//myCaptureRT = SetupCaptureCPP();
		MyCaptureActor->SceneCaptureComp->SetHiddenInGame(false);
	}
}

void AUIWSWaterBody::CreateMeshSurface()
{
	/** Get rid of the old chunks*/
	for (const auto& XMesh : XMeshes)
	{
		if (XMesh)
			XMesh->DestroyComponent();
	}
	XMeshes.Empty();

	for (const auto& YMesh : YMeshes)
	{
		if (YMesh)
			YMesh->DestroyComponent();
	}
	YMeshes.Empty();
	CurrentYArray.Empty();

	InitializeWaterMaterial(false);

	UStaticMeshComponent* FirstMesh = CreateSurfaceComponent();
	XMeshes.Add(FirstMesh);
	/** Determine how many chunks we want to break the mesh up into, if any*/
	const int ScaleX = FMath::TruncToInt(GetActorScale3D().X);
	const int ChunksX = ScaleX / FMath::Clamp(MaxTileScale, 1, 1000);
	if (ChunksX > 0)
	{
		for (int i = 1; i < ChunksX; i++)
		{
			UStaticMeshComponent* Mesh = CreateSurfaceComponent();
			XMeshes.Add(Mesh);
			Mesh->SetWorldLocation(XMeshes[i - 1]->GetSocketLocation(TEXT("Extent_X")));
		}
	}

	/** Correctly place and scale each chunk*/
	for (auto& XMesh : XMeshes)
	{
		const float Divisor = XMeshes.Num();
		const float MeshScaleX = XMesh->GetRelativeTransform().GetScale3D().X / Divisor;
		//float meshscaley = XMesh->GetRelativeTransform().GetScale3D().Y;
		XMesh->SetRelativeScale3D(FVector(MeshScaleX, 1, 1));
		const int ArrayLoc = XMeshes.Find(XMesh);
		if (ArrayLoc != 0)
		{
			XMesh->SetWorldLocation(XMeshes[ArrayLoc - 1]->GetSocketLocation(TEXT("Extent_X")));
		}
	}


	/** Spawn all the Y axis chunks*/
	for (auto& XMesh : XMeshes)
	{
		CurrentYArray.Empty();
		CurrentYArray.Add(XMesh);
		const int ScaleY = FMath::TruncToInt(GetActorScale3D().Y);
		const int ChunksY = ScaleY / FMath::Clamp(MaxTileScale, 1, 1000);

		if (ChunksY > 0)
		{
			for (int i = 1; i < ChunksY; i++)
			{
				UStaticMeshComponent* Mesh = CreateSurfaceComponent();
				CurrentYArray.Add(Mesh);
				YMeshes.Add(Mesh);
				Mesh->SetWorldLocation(CurrentYArray[i - 1]->GetSocketLocation(TEXT("Extent_Y")));
			}


			/** Set Y Axis meshes scale*/
			for (auto& YMesh : CurrentYArray)
			{
				const float Divisor = CurrentYArray.Num();
				const float MeshScaleY = YMesh->GetRelativeTransform().GetScale3D().Y / Divisor;
				const float MeshScalex = XMesh->GetRelativeTransform().GetScale3D().X;
				YMesh->SetRelativeScale3D(FVector(MeshScalex, MeshScaleY, 1));
				const int ArrayLoc = CurrentYArray.Find(YMesh);
				if (ArrayLoc != 0)
				{
					YMesh->SetWorldLocation(CurrentYArray[ArrayLoc - 1]->GetSocketLocation(TEXT("Extent_Y")));
				}
			}
		}
	}
}

UStaticMeshComponent* AUIWSWaterBody::CreateSurfaceComponent()
{
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->RegisterComponent();
	Comp->SetStaticMesh(WaterMeshSM);
	Comp->SetMaterial(0, WaterMID);
	Comp->SetMaterial(1, WaterMIDLOD1);
	Comp->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Comp->SetRelativeLocation(FVector(0, 0, 0));
	Comp->SetRelativeScale3D(FVector(1, 1, 1));
	Comp->LightmapType = ELightmapType::ForceSurface;
	Comp->SetMobility(EComponentMobility::Movable);
	//comp->SetMaterial(0,MID)
	//comp->SetMaterial(0, WaterMID);
	//comp->SetMaterial(1, WaterMIDLOD1);
	//WaterMeshComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
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
	}

	/** For Joe so he stops whining */
	Comp->bCastDynamicShadow = false;
	Comp->bCastVolumetricTranslucentShadow = false;
	Comp->bCastFarShadow = false;
	Comp->bCastStaticShadow = false;
	return Comp;
}

void AUIWSWaterBody::ApplyForceAtLocation(float Strength, float SizePercent, FVector HitLocation, bool bWithEffect)
{
	//SCOPE_CYCLE_COUNTER(STAT_ManualInteraction);
	const FLinearColor UVLC = UKismetMaterialLibrary::GetVectorParameterValue(
		this, MPC_UIWSWaterBodies, TEXT("playerpos"));;
	const float IntDistance = UKismetMaterialLibrary::GetScalarParameterValue(this, MPC_UIWSWaterBodies,
	                                                                          TEXT("InteractiveDistance"));
	const FVector WPVec = FVector(UVLC.R, UVLC.G, 0);

	//if the ripple is within interactive bounds draw it
	if (HitLocation.X > WPVec.X - (IntDistance - 400) / 2 && HitLocation.X < WPVec.X + (IntDistance - 400) / 2 &&
		HitLocation.Y > WPVec.Y - (IntDistance - 400) / 2 && HitLocation.Y < WPVec.Y + (IntDistance - 400) / 2)
	{
		//UMaterialInstanceDynamic* ManualSplatInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, ManForceSplatMat);
		const FVector HitVec = HitLocation;
		const FVector UVVec = WorldPosToRelativeUV(HitVec);
		ManualSplatInst->SetVectorParameterValue(TEXT("ForcePosition"), FLinearColor(UVVec));
		ManualSplatInst->SetScalarParameterValue(TEXT("ForceSizePercent"), SizePercent);
		ManualSplatInst->SetScalarParameterValue(TEXT("ForceStrength"), Strength);

		//		UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, GetHeightRT(iHeightState), ManualSplatInst);
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, GetHeightRT(HeightState), CanvasMan, SizeMan,
		                                                       ContextMan);
		CanvasMan->K2_DrawMaterial(ManualSplatInst, FVector2D(0, 0), SizeMan, FVector2D(0, 0), FVector2D(1, 1), 0.0f,
		                           FVector2D(0, 0));
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, ContextMan);
	}

	if (bWithEffect)
	{
		SplashAtLocation(HitLocation, FVector(0, 0, 100), Strength);
	}
	ForceAppliedAtLocation(HitLocation, Strength);
}

void AUIWSWaterBody::RequestPriorityManual()
{
	if (MyManager.IsValid())
	{
		MyManager->RequestPriority(this);
		//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Purple, "RequesPriorityManual()");
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Manager pointer was invalid when RequestPriorityManual() was called"));
	}
}

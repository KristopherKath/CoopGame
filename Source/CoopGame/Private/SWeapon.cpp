// Fill out your copyright notice in the Description page of Project Settings.


#include "SWeapon.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "../CoopGame.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"


//Created a console variable. Global
static int32 DebugWeaponDrawing = 0;
FAutoConsoleVariableRef CVARDebugWeaponDrawing(
	TEXT("COOP.DebugWeapons"), 
	DebugWeaponDrawing, 
	TEXT("Draw Debug Lines for Weapons"), 
	ECVF_Cheat);

// Sets default values
ASWeapon::ASWeapon()
{
	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp; //set mesh as new root

	MuzzleSocketName = "MuzzleSocket";

	TracerTargetName = "Target";

	BaseDamage = 20.0f;

	BulletSpread = 1.0f;

	RateOfFire = 600;

	SetReplicates(true); //when spawned on server, will also spawn on clients

	NetUpdateFrequency = 66.0f;
	MinNetUpdateFrequency = 33.0f;
}


void ASWeapon::BeginPlay()
{
	Super::BeginPlay();

	TimeBetweenShots = 60 / RateOfFire;
}



void ASWeapon::Fire()
{
	//Clients only
	if (!HasAuthority())
		ServerFire();


	//Trace the world, from pawn eyes to crosshair location
	AActor* MyOwner = GetOwner();
	if (!MyOwner) return;


	FVector EyeLocation;
	FRotator EyeRotation;
	MyOwner->GetActorEyesViewPoint(EyeLocation, EyeRotation); //Fill passed variables for use

	FVector ShotDirection = EyeRotation.Vector();

	float HalfRad = FMath::DegreesToRadians(BulletSpread);
	ShotDirection = FMath::VRandCone(ShotDirection, HalfRad, HalfRad);

	FVector TraceEnd = EyeLocation + (ShotDirection * 10000); //Trace an end location

	//Collision Paramaters
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(MyOwner);
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = true; //Does very specific tracing (more expensive). So we can calculate headshots
	QueryParams.bReturnPhysicalMaterial = true; //get data on what type of material hit

	// Particle "Target" parameter 
	FVector TracerEndPoint = TraceEnd;

	EPhysicalSurface SurfaceType = SurfaceType_Default;
		
	FHitResult Hit;
	//if blocking collision calculated
	if (GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, TraceEnd, COLLISION_WEAPON, QueryParams))
	{
		AActor* HitActor = Hit.GetActor();

		// Select the proper impact effect and play it
		SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

		//Set Damage Amount
		float ActualDamage = BaseDamage;
		if (SurfaceType == SURFACE_FLESHVULNERABLE)
			ActualDamage *= 4.0f;
			
		//Apply Damage to hit Actor
		UGameplayStatics::ApplyPointDamage(HitActor, ActualDamage, ShotDirection, Hit, 
			MyOwner->GetInstigatorController(), MyOwner, DamageType);
			
		PlayImpactEffects(SurfaceType, Hit.ImpactPoint);

		TracerEndPoint = Hit.ImpactPoint;
	}
		

	PlayFireEffects(TracerEndPoint);


	//if run by server set hitscan end point
	if (HasAuthority())
	{
		HitScanTrace.TraceTo = TracerEndPoint;
		HitScanTrace.SurfaceType = SurfaceType;
	}

	LastFireTime = GetWorld()->TimeSeconds;

	//For Debuging
	if (DebugWeaponDrawing > 0)
		DrawDebugLine(GetWorld(), EyeLocation, TraceEnd, FColor::White, false, 1.0f, 0, 1.0f);
}

void ASWeapon::StartFire()
{
	float Delay = FMath::Max(LastFireTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f);

	GetWorldTimerManager().SetTimer(TimeHandle_TimeBetweenShots, this, &ASWeapon::Fire, TimeBetweenShots, true, Delay);
}

void ASWeapon::ServerFire_Implementation()
{
	Fire();
}

//Validate code. If false then disconnect client
bool ASWeapon::ServerFire_Validate()
{
	return true;
}

//replicates scan trace
void ASWeapon::OnRep_HitScanTrace()
{
	//Play cosmetic FX
	PlayFireEffects(HitScanTrace.TraceTo);

	PlayImpactEffects(HitScanTrace.SurfaceType, HitScanTrace.TraceTo);
}


void ASWeapon::StopFire()
{
	GetWorldTimerManager().ClearTimer(TimeHandle_TimeBetweenShots);
}



void ASWeapon::PlayFireEffects(FVector TracerEndPoint)
{
	if (MuzzleEffect)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, MeshComp, MuzzleSocketName);
	}


	if (TracerEffect)
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);

		UParticleSystemComponent* TracerComp = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TracerEffect, MuzzleLocation);

		if (TracerComp)
		{
			TracerComp->SetVectorParameter(TracerTargetName, TracerEndPoint);
		}
	}

	APawn* MyOwner = Cast<APawn>(GetOwner());
	if (MyOwner)
	{
		APlayerController* PC = Cast<APlayerController>(MyOwner->GetController());
		if (PC)
		{
			PC->ClientStartCameraShake(FireCamShake);
		}
	}
}

void ASWeapon::PlayImpactEffects(EPhysicalSurface SurfaceType, FVector ImpactPoint)
{
	UParticleSystem* SelectedEffect = nullptr;
	switch (SurfaceType)
	{
	case SURFACE_FLESHDEFAULT:
	case SURFACE_FLESHVULNERABLE:
		SelectedEffect = FleshImpactEffect;
		break;
	default:
		SelectedEffect = DefaultImpactEffect;
		break;
	}
	if (SelectedEffect)
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);
		FVector ShotDir = ImpactPoint - MuzzleLocation;
		ShotDir.Normalize();

		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SelectedEffect,
			ImpactPoint, ShotDir.Rotation());
	}
}



void ASWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);


	DOREPLIFETIME_CONDITION(ASWeapon, HitScanTrace, COND_SkipOwner); //Replicated variable to all machines
}

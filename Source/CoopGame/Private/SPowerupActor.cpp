// Fill out your copyright notice in the Description page of Project Settings.


#include "SPowerupActor.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ASPowerupActor::ASPowerupActor()
{
	PowerupInterval = 0.0f;
	TotalNumOfTicks = 0;

	bIsPowerupActive = false;

	SetReplicates(true);
}


void ASPowerupActor::OnTickPowerup()
{
	TicksProcessed++;

	OnPowerupTicked(); //call blueprint implemented code

	if (TotalNumOfTicks <= TicksProcessed)
	{
		OnExpired();

		bIsPowerupActive = false; //replicates to all clients. OnRep_PowerupActive will be called for each client
		OnRep_PowerupActive(); //call rep function for the server as well

		//Delete Timer
		GetWorldTimerManager().ClearTimer(TimerHandle_PowerupTick);
	}
}

//replicated function call
void ASPowerupActor::OnRep_PowerupActive()
{
	OnPowerupStateChanged(bIsPowerupActive); //calls blueprint code
}

//server only
void ASPowerupActor::ActivatePowerup()
{
	OnActivated(); //call blueprint implemented code

	bIsPowerupActive = true; //replicates to all clients. OnRep_PowerupActive will be called for each client
	OnRep_PowerupActive(); //call rep function for the server as well

	//activate powerup for time
	if (PowerupInterval > 0.0f)
		GetWorldTimerManager().SetTimer(TimerHandle_PowerupTick, this, &ASPowerupActor::OnTickPowerup, PowerupInterval, true);
	else
		OnTickPowerup();
}


void ASPowerupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);


	DOREPLIFETIME(ASPowerupActor, bIsPowerupActive); //Replicated variable to all machines
}


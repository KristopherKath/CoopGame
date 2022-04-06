// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SGameMode.generated.h"


/**
 * 
 */
UCLASS()
class COOPGAME_API ASGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
protected:

	FTimerHandle TimerHandle_BotSpawner;
	FTimerHandle TimerHandle_NextWaveStart;

	//number of bots to spawn in current wave
	int32 NumBotsToSpawn;

	int32 WaveCount;

	UPROPERTY(EditDefaultsOnly, Category = "GameMode")
	float TimeBetweenWaves;

protected:

	// Hook for BP to spawn a single bot
	UFUNCTION(BlueprintImplementableEvent, Category = "GameMode")
	void SpawnNewBot();

	void SpawnBotTimerElapsed();

	// Start Spawning bots
	void StartWave();

	//Stop Spawning bots
	void EndWave();

	// Set timer for the next start wave
	void PrepareForNextWave();

	void CheckWaveState();

public:

	ASGameMode();


	virtual void StartPlay() override;

	virtual void Tick(float DeltaSeconds) override;

};

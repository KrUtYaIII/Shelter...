// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "PickableItem.h"
#include "BP_PlayerController.h"

#include "GS.generated.h"

class ASpectator;

#define MAX_PLAYER_COUNT 2

#define MIN_COUNT_KeyShelter 3
#define MIN_COUNT_Boltorez 3
#define MIN_COUNT_Otvertka 3

#define MAX_COUNT_KeySHelter 3
#define MAX_COUNT_Boltorez 3
#define MAX_COUNT_Otvertka 3

#define STONE_DAMAGE 0.15f
#define DeltaRadiation 0.0f

#define SPAWN_TIME 10.f
/**
 * 
 */

class AAreaCollision;

UCLASS()
class MYPROJECT9_API AGS : public AGameStateBase
{
	GENERATED_BODY()

public:
	AGS();
	void GetLifetimeReplicatedProps(TArray < FLifetimeProperty >& OutLifetimeProps) const override;
	
	void GameBegin();

	void Tick(float DeltaSeconds) override;

	UFUNCTION()
		void SpawnPlayers();
	UFUNCTION(BlueprintCallable)
		void ResetGame();

public:
	//For indexing
	UPROPERTY(BlueprintReadWrite)
		int AmountOfPlayers;
	int AcceptPiedistalAmount;
	//Rematch
	UPROPERTY(BlueprintReadWrite)
		int AgreedPlayers;

	TArray<FText> Winners;

	UPROPERTY(Replicated)
		TArray<FText> NickNames;
	UPROPERTY(Replicated)
		TArray<FText> Kills;
	UPROPERTY(Replicated)
		TArray<FText> Deaths;
	UPROPERTY(Replicated)
		TArray<int32> EscapeTime;
	
	TArray<ASpectator*> Spectators;
	UPROPERTY(Replicated)
		TArray<bool> AreaAvaliables;
	UPROPERTY(Replicated)
		TArray<bool> AreaClosed;
	TArray<AAreaCollision*>Areas;

	UPROPERTY(EditAnywhere, Category = "ItemClasses")
		TSubclassOf<APickableItem> KeyShelter;
	UPROPERTY(EditAnywhere, Category = "ItemClasses")
		TSubclassOf<APickableItem> Boltorez;
	UPROPERTY(EditAnywhere, Category = "ItemClasses")
		TSubclassOf<APickableItem> Otvertka;

	int CurrentOtvertka;
	int CurrentKeyShelter;
	int CurrentBoltorez;

	int CurrentTime;

	int32 GeneralLayer;

	bool IsGameStarted;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Runtime/Engine/Classes/Particles/ParticleSystemComponent.h"
#include "DezinfectorNasos.h"
#include "DezinfectorNasosZatichka.generated.h"

UCLASS()
class MYPROJECT9_API ADezinfectorNasosZatichka : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADezinfectorNasosZatichka();

	void Interact();

	UFUNCTION(BlueprintImplementation)
		void SwitchAnimUp();
	UFUNCTION(BlueprintImplementation)
		void SwitchAnimDown();

public:	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
		UParticleSystemComponent* PS;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Player")
		UStaticMeshComponent* Lampochka;

	UPROPERTY(EditAnywhere)
		ADezinfectorNasos* Nasos;
	UPROPERTY(BlueprintReadWrite)
		bool DoesLock;
	UPROPERTY(BlueprintReadWrite)
		bool DoesPlayAnim;
};

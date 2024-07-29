// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/ALSCharacter.h"
#include "ProjectSO/Weapon/SOGunBase.h"
#include "SOCharacterBase.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTSO_API ASOCharacterBase : public AALSCharacter
{
	GENERATED_BODY()

public:
	ASOCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 나중에 장착 가능한 것들만 모아서 하기
	void EquipItem(ISOEquippableInterface* InEquipment);

	/** Input */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SO|Input")
	void AttackAction(bool bValue);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ALS|Input")
	void ChangeFireModeAction(bool bValue);

		
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "SO|Equip")
	TObjectPtr<ASOGunBase> CurrentWeapon;
};

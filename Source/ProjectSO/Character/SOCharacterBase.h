// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/ALSCharacter.h"
#include "ProjectSO/Weapon/SOGunBase.h"
#include "ProjectSO/Interface/SOEquippableInterface.h"
#include "SOCharacterBase.generated.h"

class USOInventoryComponent;
class USOHealthComponent;
/**
 * 
 */
UCLASS()
class PROJECTSO_API ASOCharacterBase : public AALSCharacter
{
	GENERATED_BODY()

public:
	ASOCharacterBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaTime) override;
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 나중에 장착 가능한 것들만 모아서 하기
	void EquipItem(ASOItemActor* InEquipment);

	/** Input */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SO|Input")
	void AttackAction(bool bValue);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SO|Input")
	void ChangeFireModeAction(bool bValue);

	UFUNCTION(BlueprintCallable, Category = "SO|Input")
	void AimAction_(bool bValue);

	UFUNCTION(BlueprintCallable, Category = "SO|Input")
	void ReloadAction(bool bValue);
	
	void UpdateCharacterMinigunMovement();

public:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPCEquipItem(ASOItemActor* Weapon);

public:
	FORCEINLINE USOHealthComponent* GetHealth() const { return HealthComponent; }
	FORCEINLINE USOInventoryComponent* GetInventory() const { return InventoryComponent; }
	
	// Health
protected:
	//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS|Component")
	//	TObjectPtr<USkeletalMeshComponent> SkeletalMesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SO|MeshComponent")
	TObjectPtr<USkeletalMeshComponent> CharacterMesh;
	
protected:
	UFUNCTION()
	void ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, class AController* InstigatorController, AActor* DamageCauser);

public:
	//Apply Recoil 
	void ApplyRecoil(const float InYawRecoil, const float InPitchRecil);
	FString GetHitParentBone(const FName& InBoneString);
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "SO|Equip")
	TObjectPtr<ASOGunBase> CurrentWeapon;

protected:
	UPROPERTY(EditAnywhere, Replicated, Category = "SO|Health")
	TObjectPtr<USOHealthComponent> HealthComponent;

	UPROPERTY(EditAnywhere, Replicated, Category = "SO|Inventory")
	TObjectPtr<USOInventoryComponent> InventoryComponent;

	
	
};

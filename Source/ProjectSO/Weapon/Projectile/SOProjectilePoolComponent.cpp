// Fill out your copyright notice in the Description page of Project Settings.


#include "SOProjectilePoolComponent.h"

#include "ProjectSO/Weapon/SOProjectileBase.h"

// Sets default values for this component's properties
USOProjectilePoolComponent::USOProjectilePoolComponent()
{
	//Todo 숫자
	InitialPoolSize = 30;
	ExpandSize = 1;
}


// Called when the game starts
void USOProjectilePoolComponent::BeginPlay()
{
	Super::BeginPlay();

}

void USOProjectilePoolComponent::PushProjectileInPool(ASOProjectileBase* Projectile)
{
	Pool.Push(Projectile);
}

void USOProjectilePoolComponent::SetAmmoClass(TSubclassOf<ASOProjectileBase> InAmmoClass)
{
	AmmoClass = InAmmoClass;
}

void USOProjectilePoolComponent::Expand()
{
	if (AmmoClass)
	{
		for (int i = 0; i < ExpandSize; i++)
		{
			ASOProjectileBase* SpawnedProjectile = GetWorld()->SpawnActor<ASOProjectileBase>(AmmoClass, FVector().ZeroVector, FRotator().ZeroRotator);
			SpawnedProjectile->SetProjectileActive(false);
			SpawnedProjectile->ProjectilePool = this;
			Pool.Push(SpawnedProjectile);
		}
	}
}

void USOProjectilePoolComponent::Initialize()
{

	if (AmmoClass)
	{
		for (int i = 0; i < InitialPoolSize; i++)
		{
			ASOProjectileBase* SpawnedProjectile = GetWorld()->SpawnActor<ASOProjectileBase>(AmmoClass, FVector().ZeroVector, FRotator().ZeroRotator);
			SpawnedProjectile->SetProjectileActive(false);
			SpawnedProjectile->ProjectilePool = this;
			Pool.Push(SpawnedProjectile);
		}
	}
}

ASOProjectileBase* USOProjectilePoolComponent::PullProjectile()
{
	if (Pool.Num() == 0)
	{
		Expand();
	}
	UE_LOG(LogTemp, Warning, TEXT("Pool Num :  %d"), Pool.Num());
	return Pool.Pop();
}



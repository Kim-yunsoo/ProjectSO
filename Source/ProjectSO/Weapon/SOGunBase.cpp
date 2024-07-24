// Fill out your copyright notice in the Description page of Project Settings.


#include "SOGunBase.h"

#include "Components/CapsuleComponent.h"
#include "KisMet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "ProjectSO/Character/SOCharacterBase.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectSO/Core/SOGameSubsystem.h"

// Sets default values
ASOGunBase::ASOGunBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Root"));
	CollisionComp->InitCapsuleSize(40.0f, 50.0f);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = CollisionComp;
	
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(FName("WeaponMesh"));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WeaponMesh->CastShadow = true;
	WeaponMesh->SetVisibility(true, false);	
	WeaponMesh->SetupAttachment(CollisionComp);
	
	CurrentFireMode = ESOFireMode::Single;
}

void ASOGunBase::SetGunData(const uint8 InID)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("World is not available."));
		return;
	}

	// 게임 인스턴스 가져오기.
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(World);
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameInstance is not available."));
		return;
	}

	// 게임 인스턴스에서 서브시스템을 가져오기.
	USOGameSubsystem* SOGameSubsystem = GameInstance->GetSubsystem<USOGameSubsystem>();
	if (!SOGameSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("SOGameSubsystem is not available."));
		return;
	}

	// 서브시스템에서 원하는 ID의 WeaponStatData를 가져오기.
	FSOWeaponStat* SelectedWeaponStat  = SOGameSubsystem->GetWeaponStatData(InID);
	if(SelectedWeaponStat)
	{
		WeaponStat = *SelectedWeaponStat;
	}

	FSOWeaponData* SelectedWeaponData  = SOGameSubsystem->GetWeaponData(InID);
	if(SelectedWeaponData)
	{
		WeaponData = *SelectedWeaponData;
	}
}


// Called when the game starts or when spawned
void ASOGunBase::BeginPlay()
{
	Super::BeginPlay();
	SetGunData(0);

	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ASOGunBase::OnSphereBeginOverlap);
}

// Called every frame
void ASOGunBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ASOGunBase::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(HasAuthority())
	{
		ASOCharacterBase* CharacterBase = Cast<ASOCharacterBase>(OtherActor);
		OwningCharacter = CharacterBase;
		if(OwningCharacter)
		{
			bIsEquipped = true;
			OwningCharacter->EquipGun(this);
			CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void ASOGunBase::PressLMB()
{
	UE_LOG(LogTemp, Warning, TEXT("%s"), *FString(__FUNCTION__))
	OnFire();
}

void ASOGunBase::OnFire()
{
	if(!bIsEquipped) return;
	if(bReloading || CurrentAmmoInClip <= 0) return;
	switch (CurrentFireMode)
	{
	case ESOFireMode::Auto:
		AutoFire();
		break;
	case ESOFireMode::Burst:
		BurstFire();
		break;
	case ESOFireMode::Single:
		SingleFire();
		break;
	default:
		break;
	}
}

void ASOGunBase::AutoFire()
{
	
}

void ASOGunBase::BurstFire()
{
	
}

void ASOGunBase::SingleFire()
{
}

void ASOGunBase::FireProjectile()
{
	AController* OwnerController = OwningCharacter->GetController();
	if (OwnerController == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("OwnerController"));
		return;
	}

	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation,
		CrosshairWorldPosition,
		CrosshairWorldDirection
	);
	
}

void ASOGunBase::CreateProjectile(FVector StartPosition, FRotator StartRotation)
{
}

void ASOGunBase::ShowEffect(FVector StartPosition, FRotator StartRotation)
{
}

void ASOGunBase::PlaySound()
{
}

void ASOGunBase::Recoil()
{
}

void ASOGunBase::Reload()
{
}

void ASOGunBase::Aim()
{
	
}

void ASOGunBase::Equip()
{
	if(!bIsEquipped) return;

	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	const USkeletalMeshSocket* HandSocket = OwningCharacter->GetMesh()->GetSocketByName(AttachPoint);
	if(HandSocket)
	{
		HandSocket->AttachActor(this,OwningCharacter->GetMesh());		
	}
	WeaponMesh->AttachToComponent(OwningCharacter->GetMesh(), AttachmentRules, AttachPoint);
}

void ASOGunBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASOGunBase, OwningCharacter);
	DOREPLIFETIME(ASOGunBase, ClipSize);
	DOREPLIFETIME(ASOGunBase, MaxAmmoCapacity);
	DOREPLIFETIME(ASOGunBase, bInfiniteAmmo);
	DOREPLIFETIME(ASOGunBase, CurrentFireMode);
	DOREPLIFETIME(ASOGunBase, bIsEquipped);
	DOREPLIFETIME(ASOGunBase, bReloading);
	DOREPLIFETIME(ASOGunBase, bTrigger);
	DOREPLIFETIME(ASOGunBase, CurrentAmmo);
	DOREPLIFETIME(ASOGunBase, CurrentAmmoInClip);
}

void ASOGunBase::MulticastRPCShowEffect_Implementation(FVector StartPosition, FRotator StartRotation)
{
	
}

void ASOGunBase::ServerRPCOnFire_Implementation(FVector StartPosition, FRotator StartRotation)
{
	
}


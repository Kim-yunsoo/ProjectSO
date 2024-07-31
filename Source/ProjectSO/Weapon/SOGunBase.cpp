// Fill out your copyright notice in the Description page of Project Settings.


#include "SOGunBase.h"

#include "NiagaraFunctionLibrary.h"
#include "KisMet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/CapsuleComponent.h"

#include "SOProjectileBase.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "ProjectSO/Character/SOCharacterBase.h"
#include "Projectile/SOProjectilePoolComponent.h"
#include "ProjectSO/ProjectSO.h"
#include "ProjectSO/Core/SOGameSubsystem.h"

// Sets default values
ASOGunBase::ASOGunBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(FName("WeaponMesh"));
	//WeaponMesh->SetCollisionEnabled();
	WeaponMesh->SetCollisionProfileName(TEXT("WeaponMesh"));
	WeaponMesh->CastShadow = true;
	WeaponMesh->SetVisibility(true, false);	
	WeaponMesh->SetMobility(EComponentMobility::Movable);
	WeaponMesh->SetSimulatePhysics(true);
	//WeaponMesh->SetupAttachment(CollisionComp);
	RootComponent = WeaponMesh;
	
	CollisionComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionComp"));
	CollisionComp->InitCapsuleSize(40.0f, 50.0f);
//	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
//	CollisionComp->SetMobility(EComponentMobility::Movable);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComp->SetupAttachment(RootComponent);

	//RootComponent = CollisionComp;

	ProjectilePoolComponent = CreateDefaultSubobject<USOProjectilePoolComponent>(TEXT("ProjectilePool"));
	
	// 초기 CurrentFireMode 설정
	CurrentFireMode = ESOFireMode::None;
	
	CurrentAmmoInClip = 30;
	MaxRepeatCount = 3;
	bPlayFireEffect = false;
}

// Called when the game starts or when spawned
void ASOGunBase::BeginPlay()
{
	Super::BeginPlay();

	//Gun Data Setting
	SetGunData(2);

	//Object Pool
	ProjectilePoolComponent->Initialize();
	
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ASOGunBase::OnSphereBeginOverlap);
	
	//여기서 타이머
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &ASOGunBase::DisablePhysics, 2.0f, false);

	AvailableFireModeCount = CalculateAvailableFireModeCount();
	//SO_LOG(LogSOTemp,Warning, TEXT("AvailableFireMode : %d"), AvailableFireMode)	
	InitCurrentFireMode();
}

// Called every frame
void ASOGunBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASOGunBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASOGunBase, OwningCharacter);
	DOREPLIFETIME(ASOGunBase, MaxAmmoCapacity);
	DOREPLIFETIME(ASOGunBase, bInfiniteAmmo);
	DOREPLIFETIME(ASOGunBase, CurrentFireMode);
	DOREPLIFETIME(ASOGunBase, bIsEquipped);
	DOREPLIFETIME(ASOGunBase, bReloading);
	DOREPLIFETIME(ASOGunBase, bTrigger);
	DOREPLIFETIME(ASOGunBase, CurrentAmmo);
	DOREPLIFETIME(ASOGunBase, CurrentAmmoInClip);
	//DOREPLIFETIME(ASOGunBase, FireEffect);
	DOREPLIFETIME(ASOGunBase, bPlayFireEffect);
}

void ASOGunBase::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                      const FHitResult& SweepResult)
{
	if (HasAuthority())
	{
		ASOCharacterBase* CharacterBase = Cast<ASOCharacterBase>(OtherActor);
		OwningCharacter = CharacterBase;
		if (OwningCharacter) // && !bIsEquipped)
		{
			bIsEquipped = true;
			OwningCharacter->EquipItem(this);
		}
	}
}

void ASOGunBase::PressLMB()
{
	SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	OnFire(CurrentFireMode);
}

void ASOGunBase::ReleaseLMB()
{
	SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	StopFire();
}

EALSOverlayState ASOGunBase::GetOverlayState() const
{
	return WeaponData.OverlayState;
}

void ASOGunBase::OnFire(ESOFireMode InFireMode)
{
	SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	if (!bIsEquipped) return;
	if (bReloading || CurrentAmmoInClip <= 0) return;
	switch (InFireMode)
	{
	case ESOFireMode::Auto:
		FireAuto();
		break;
	case ESOFireMode::Burst:
		FireBurst(MaxRepeatCount);
		break;
	case ESOFireMode::Single:
		FireSingle();
		break;
	default:
		break;
	}
}

void ASOGunBase::FireAuto()
{
	SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);

	FireProjectile();
	GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &ASOGunBase::FireProjectile, WeaponStat.FireInterval, true);
}

void ASOGunBase::FireBurst(uint32 InMaxRepeatCount)
{
	SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	const int InitialRepeatCount = 0;
	// 점사 횟수, 필요에 따라 변경

	FireContinuously(InitialRepeatCount, InMaxRepeatCount);
}

void ASOGunBase::FireContinuously(int32 InCurRepeatCount, int32 InMaxRepeatCount)
{
	if(InCurRepeatCount >= InMaxRepeatCount)
	{
		GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
		return;
	}
	FireProjectile();
	
	GetWorld()->GetTimerManager().SetTimer(
		BurstTimerHandle,
		[this, InCurRepeatCount, InMaxRepeatCount]()
		{
			FireContinuously(InCurRepeatCount + 1, InMaxRepeatCount);
		},
		WeaponStat.FireInterval, false);
}

void ASOGunBase::FireSingle()
{
	SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	FireProjectile();
}

void ASOGunBase::FireProjectile()
{
	SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	AController* OwnerController = OwningCharacter->GetController();
	if (OwnerController == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("OwnerController"));
		return;
	}

	// Viewport LineTrace
	// FHitResult ScreenLaserHit;
	// FCollisionQueryParams Params;
	// Params.AddIgnoredActor(this);
	// Params.AddIgnoredActor(GetOwner());

	// 화면 중앙 LineTrace
	FVector TraceStartLocation;
	FRotator TraceStartRotation;
	OwnerController->GetPlayerViewPoint(TraceStartLocation, TraceStartRotation);

	// 수정 필요 
	FVector TraceEnd = TraceStartLocation + TraceStartRotation.Vector() *  WeaponStat.MaxRange * 100; 
	// bool bScreenLaserSuccess = GetWorld()->LineTraceSingleByChannel(ScreenLaserHit, TraceStartLocation, TraceEnd, ECC_Projectile, Params);
	
	// 허공이면 TraceEnd, 아니면 Hit.Location
	// FVector HitLocation = bScreenLaserSuccess ? ScreenLaserHit.Location : TraceEnd;
	// UE_LOG(LogTemp, Log, TEXT("ScreenLaserHit : %s "), *HitLocation.ToString());

	FTransform MuzzleSocketTransform;
	FTransform AmmoEjectSocketTransform;
	
	// 소켓 위치
	const USkeletalMeshSocket* AmmoEjectSocket = WeaponMesh->GetSocketByName(AmmoEjectSocketName);
	const USkeletalMeshSocket* MuzzleSocket = WeaponMesh->GetSocketByName(WeaponData.MuzzleSocketName);
	
	// ensure(AmmoEjectSocket);
	if(IsValid(AmmoEjectSocket))
	{
		AmmoEjectSocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh);	
	}

	if(IsValid(MuzzleSocket))
	{
		MuzzleSocketTransform = MuzzleSocket->GetSocketTransform(WeaponMesh);	
	}

	DrawDebugLine(GetWorld(), MuzzleSocketTransform.GetLocation(), TraceEnd, FColor::Red,false, 5, 0, 2);
	//DrawDebugPoint(GetWorld(), ScreenLaserHit.Location, 3, FColor::Red, false, 5,0);

	FRotator MuzzleRotation = MuzzleSocketTransform.GetRotation().Rotator();
	FRotator EjectRotation = AmmoEjectSocketTransform.GetRotation().Rotator();
	PlayMuzzleEffect(MuzzleSocketTransform.GetLocation(), MuzzleRotation);
	PlayEjectAmmoEffect(AmmoEjectSocketTransform.GetLocation(), EjectRotation);
	// CreateFireEffectActor(MuzzleSocketTransform, AmmoEjectSocketTransform);
	
	PlaySound();
	bPlayFireEffect = true;
	// 총알 생성
	ServerRPCOnFire(MuzzleSocketTransform, AmmoEjectSocketTransform, TraceEnd);
}

void ASOGunBase::CreateProjectile(const FTransform& MuzzleTransform, const FVector& HitLocation)
{
	if (OwningCharacter == nullptr || OwningCharacter->GetController() == nullptr)
	{
		return;
	}

	/*APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("OwnerPawn"));
		return;
	}
	
	AController* OwnerController = OwnerPawn->GetController();
	if (OwnerController == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("OwnerController"));
	}*/

	APawn* InstigatorPawn = Cast<APawn>(GetOwner());

	// Try and fire a projectile
	if (ProjectileClass == nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("ProjectileClass is null"));
		return;
	}

	FVector SpawnLocation = MuzzleTransform.GetLocation();
	FVector ToTarget = HitLocation - SpawnLocation;
	FRotator SpawnRotation = ToTarget.Rotation();

	// Set Spawn Collision Handling Override
	 FActorSpawnParameters ActorSpawnParams;
	// ActorSpawnParams.Owner = GetOwner();
	// ActorSpawnParams.Instigator = InstigatorPawn;
	// ASOProjectileBase* Projectile = nullptr;

	// 서버에서 생성하면 자동 리플리케이션
	 //Projectile = GetWorld()->SpawnActor<ASOProjectileBase>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
	// if(Projectile) Projectile->SetOwner(OwningCharacter);		
	// UE_LOG(LogTemp, Warning, TEXT("%s"), *FString(__FUNCTION__))

	//검사 로직 추가
	ASOProjectileBase* Projectile = ProjectilePoolComponent->PullProjectile();
	Projectile->InitializeProjectile(SpawnLocation, SpawnRotation);
}

void ASOGunBase::StopFire()
{
	GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
}

void ASOGunBase::PlayMuzzleEffect(const FVector& MuzzleLocation, FRotator& MuzzleRotation)
{
	if (WeaponData.MuzzleFlashEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			WeaponData.MuzzleFlashEffect,
			MuzzleLocation,
			MuzzleRotation
		);
	}	
}

void ASOGunBase::PlayEjectAmmoEffect(const FVector& EjectLocation, FRotator& EjectRotation)
{
	if (WeaponData.EjectShellParticles)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), WeaponData.EjectShellParticles,
			EjectLocation, EjectRotation);		
	}
}

void ASOGunBase::PlaySound()
{		
	if (WeaponData.FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			WeaponData.FireSound,
			GetActorLocation()
		);
	}
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
	SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	
	if (!bIsEquipped) return;

	// 이거 해줘야 ServerRPC 가능
	SetOwner(OwningCharacter);	
	
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	const USkeletalMeshSocket* HandSocket = OwningCharacter->GetMesh()->GetSocketByName(WeaponData.EquipSocketName);
	if (HandSocket)
	{
		HandSocket->AttachActor(this, OwningCharacter->GetMesh());
	}
	this->AttachToComponent(OwningCharacter->GetMesh(), AttachmentRules, WeaponData.EquipSocketName);

	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	SO_LOG(LogSOTemp, Warning, TEXT("End"))
}

void ASOGunBase::SetGunData(const uint8 InID)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("World is not available."))
		return;
	}

	// 게임 인스턴스 가져오기.
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(World);
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameInstance is not available."))
		return;
	}

	// 게임 인스턴스에서 서브시스템을 가져오기.
	USOGameSubsystem* SOGameSubsystem = GameInstance->GetSubsystem<USOGameSubsystem>();
	if (!SOGameSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("SOGameSubsystem is not available."))
		return;
	}

	// 서브시스템에서 원하는 ID의 WeaponStatData를 가져오기.
	FSOWeaponStat* SelectedWeaponStat = SOGameSubsystem->GetWeaponStatData(InID);
	if (SelectedWeaponStat)
	{
		WeaponStat = *SelectedWeaponStat;
	}

	FSOWeaponData* SelectedWeaponData = SOGameSubsystem->GetWeaponData(InID);
	if (SelectedWeaponData)
	{
		WeaponData = *SelectedWeaponData;
		WeaponMesh->SetSkeletalMesh(WeaponData.SkeletalMesh);
		//AttachPoint = WeaponData.SocketName;
	}
	if(WeaponData.AmmoClass)
	{
		ProjectilePoolComponent->SetAmmoClass(WeaponData.AmmoClass);
	}
}

void ASOGunBase::DisablePhysics()
{
	SO_LOG(LogSOTemp, Log, TEXT("DisablePhysics"))
	WeaponMesh->SetSimulatePhysics(false);
}

void ASOGunBase::ServerRPCOnFire_Implementation(const FTransform& MuzzleTransform, const FTransform& EjectTransform, const FVector& HitLocation)
{
	SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	CreateProjectile(MuzzleTransform, HitLocation);

	bPlayFireEffect = !bPlayFireEffect;		// 여기서 OnRep
	
	SO_LOG(LogSOTemp, Warning, TEXT("End"))
}

int32 ASOGunBase::CalculateAvailableFireModeCount()
{
	uint8 Mode = WeaponStat.FireMode;
	int32 Count = 0;
	while (Mode)
	{
		Count += Mode & 1;
		Mode >>= 1;
	}
	return Count;
}

void ASOGunBase::InitCurrentFireMode()
{
	// 초기 사격 모드 초기화
	for(uint8 i = 1; i <= static_cast<uint8>(ESOFireMode::Max); i <<= 1)
	{
		// 작은 수부터 시작해서 낮은 비트 시 break;
		if(WeaponStat.FireMode & i)
		{
			CurrentFireMode = static_cast<ESOFireMode>(i);
			break;		
		}
	}
}

ESOFireMode ASOGunBase::GetNextValidFireMode()
{
	// 순환 방식으로 다음 발사 모드를 반환
	uint8 CurrentMode = static_cast<uint8>(CurrentFireMode);
	uint8 NextMode = CurrentMode << 1;

	StopFire();

	// 가능한 모드인가?
	// true = 불가능한 모드 = 다음 모드 탐색
	// false = 가능 = 탈출 후 반환
	while ((NextMode & WeaponStat.FireMode) == 0)
	{
		// 다음 모드
		NextMode <<= 1;
		// 만약 다음 모드가 끝에 도달 시 원복
		if (NextMode == static_cast<uint8>(ESOFireMode::Max))
		{
			// Auto로 복귀
			NextMode = 1;
		}
	}

	return static_cast<ESOFireMode>(NextMode);
}

void ASOGunBase::OnRep_PlayFireEffect()
{
	const USkeletalMeshSocket* AmmoEjectSocket = WeaponMesh->GetSocketByName(AmmoEjectSocketName);
	const USkeletalMeshSocket* MuzzleSocket = WeaponMesh->GetSocketByName(WeaponData.MuzzleSocketName);
	

	FTransform MuzzleSocketTransform;
	FTransform AmmoEjectSocketTransform;
	
	if(IsValid(AmmoEjectSocket))
	{
		AmmoEjectSocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh);
	}

	if(IsValid(MuzzleSocket))
	{
		MuzzleSocketTransform = MuzzleSocket->GetSocketTransform(WeaponMesh);	
	}
	
	FRotator MuzzleRotation = MuzzleSocketTransform.GetRotation().Rotator();
	FRotator EjectRotation = AmmoEjectSocketTransform.GetRotation().Rotator();
	PlayMuzzleEffect(MuzzleSocketTransform.GetLocation(), MuzzleRotation);
	PlayEjectAmmoEffect(AmmoEjectSocketTransform.GetLocation(), EjectRotation);		
	
}


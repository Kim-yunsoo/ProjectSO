// Fill out your copyright notice in the Description page of Project Settings.


#include "SOGunBase.h"

#include "NiagaraFunctionLibrary.h"
#include "KisMet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/CapsuleComponent.h"

#include "SOProjectileBase.h"
#include "Camera/CameraComponent.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "ProjectSO/Character/SOCharacterBase.h"
#include "Projectile/SOProjectilePoolComponent.h"
#include "ProjectSO/ProjectSO.h"
#include "ProjectSO/Component/SOInventoryComponent.h"
#include "ProjectSO/Core/SOGameSubsystem.h"
#include "ProjectSO/Library/SOWeaponMeshDataAsset.h"
#include "ProjectSO/Library/SOWeaponStructLibrary.h"
#include "ProjectSO/Player/SOPlayerController.h"

// Sets default values
ASOGunBase::ASOGunBase()
{
	UE_LOG(LogTemp, Log, TEXT("ASOGunBase"));

	PrimaryActorTick.bCanEverTick = false;
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

	ScopeCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ScopeCamera"));
	ScopeCamera->SetupAttachment(WeaponMesh);	
	ScopeCamera->bUsePawnControlRotation = true;

	ProjectilePoolComponent = CreateDefaultSubobject<USOProjectilePoolComponent>(TEXT("ProjectilePool"));

	// 초기 CurrentFireMode 설정
	CurrentFireMode = ESOFireMode::None;
	
	CurrentAmmo = 999;
	
	MaxRepeatCount = 3;
	bReloading = false;
	bInfiniteAmmo = false;
	
	HoldThreshold = 0.2f;
	bScopeAim = false;
	
}

// Called when the game starts or when spawned
void ASOGunBase::BeginPlay()
{
	Super::BeginPlay();
	//UE_LOG(LogTemp, Log, TEXT("ASOGunBase : BeginPlay"));
	//Gun Data Setting
	//삭제 필요 ID 
	SetGunData(ID);
	CurrentAmmoInClip = WeaponStat.ClipSize;
	//Object Pool
	if (HasAuthority())
	{
		ProjectilePoolComponent->Reserve();
	}
	
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ASOGunBase::OnSphereBeginOverlap);
	CollisionComp->OnComponentEndOverlap.AddDynamic(this, &ASOGunBase::OnSphereEndOverlap);
	//여기서 타이머
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &ASOGunBase::DisablePhysics, 2.0f, false);

	AvailableFireModeCount = CalculateAvailableFireModeCount();
	// SO_LOG(LogSOTemp,Warning, TEXT("AvailableFireMode : %d"), AvailableFireMode)
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
	DOREPLIFETIME(ASOGunBase, FireStartTime);
}

void ASOGunBase::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                      const FHitResult& SweepResult)
{
	//SO_LOG(LogSONetwork, Log, TEXT("OnSphereBeginOverlap Name : %s"), *OtherActor->GetName());

	if (HasAuthority())
	{
		ASOCharacterBase* CharacterBase = Cast<ASOCharacterBase>(OtherActor);
		if(CharacterBase)
		{
			CharacterBase->InteractionCheck(this);
			
			// USOInventoryComponent* Inven = CharacterBase->GetInventory();
			// if(Inven)
			// {
			// 	// 인벤토리 컴포넌트 가져오기
			// 	bIsEquipped = true;
			// 	Inven->AddToInventory(this);
			// 	// CharacterBase->EquipItem(this);
			// }
			
		}
	}
}

void ASOGunBase::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (HasAuthority())
	{
		ASOCharacterBase* CharacterBase = Cast<ASOCharacterBase>(OtherActor);
		if(CharacterBase)
		{
			CharacterBase->NoInteractableFound();
		}
	}
}

void ASOGunBase::PressLMB()
{
	//SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	OnFire(CurrentFireMode);
}

void ASOGunBase::ReleaseLMB()
{
	StopFire();
}

EALSOverlayState ASOGunBase::GetOverlayState() const
{
	return WeaponData.OverlayState;
}

void ASOGunBase::OnFire(ESOFireMode InFireMode)
{	
	if (!bIsEquipped) return;
	
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
	GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);

	FireProjectile();
	GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &ASOGunBase::FireProjectile, WeaponStat.FireInterval,
	                                       true);
}

void ASOGunBase::FireBurst(uint32 InMaxRepeatCount) // 점사 
{
	const int InitialRepeatCount = 0;
	// 점사 횟수, 필요에 따라 변경
	FireContinuously(InitialRepeatCount, InMaxRepeatCount);
}

void ASOGunBase::FireContinuously(int32 InCurRepeatCount, int32 InMaxRepeatCount)
{
	if (InCurRepeatCount >= InMaxRepeatCount)
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

void ASOGunBase::FireProjectile() //클라이언트 들어오는 함수 
{
	if (bReloading || CurrentAmmoInClip <= 0)
	{
		Reload();
		SO_LOG(LogSOGun, Log, TEXT("재장전"))
		return;
	}
	
	//SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	AController* OwnerController = OwningCharacter->GetController();
	if (OwnerController == nullptr)
	{
		//UE_LOG(LogTemp, Warning, TEXT("OwnerController"));
		return;
	}

	// Viewport LineTrace
	// FHitResult ScreenLaserHit;
	// FCollisionQueryParams Params;
	// Params.AddIgnoredActor(this);
	// Params.AddIgnoredActor(GetOwner());

	//반동 코드 넣기 
	//OwningCharacter->ApplyRecoil(WeaponStat.AimedRecoilYaw,WeaponStat.AimedRecoilPitch);


	// 화면 중앙 LineTrace
	FVector TraceStartLocation;
	FRotator TraceStartRotation;
	OwnerController->GetPlayerViewPoint(TraceStartLocation, TraceStartRotation);

	// 수정 필요 
	FVector TraceEnd = TraceStartLocation + TraceStartRotation.Vector() * WeaponStat.MaxRange * 100;
	// bool bScreenLaserSuccess = GetWorld()->LineTraceSingleByChannel(ScreenLaserHit, TraceStartLocation, TraceEnd, ECC_Projectile, Params);

	// 허공이면 TraceEnd, 아니면 Hit.Location
	// FVector HitLocation = bScreenLaserSuccess ? ScreenLaserHit.Location : TraceEnd;
	// UE_LOG(LogTemp, Log, TEXT("ScreenLaserHit : %s "), *HitLocation.ToString());

	FTransform MuzzleSocketTransform = GetSocketTransformByName(WeaponData.MuzzleSocketName, WeaponMesh);
	FTransform AmmoEjectSocketTransform = GetSocketTransformByName(AmmoEjectSocketName, WeaponMesh);
	
	
	const USkeletalMeshSocket* AmmoEjectSocket = WeaponMesh->GetSocketByName(AmmoEjectSocketName);
	const USkeletalMeshSocket* MuzzleSocket = WeaponMesh->GetSocketByName(WeaponData.MuzzleSocketName);

	// ensure(AmmoEjectSocket);
	if (IsValid(AmmoEjectSocket))
	{
		AmmoEjectSocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh);
	}

	if (IsValid(MuzzleSocket))
	{
		MuzzleSocketTransform = MuzzleSocket->GetSocketTransform(WeaponMesh);
	}

	DrawDebugLine(GetWorld(), MuzzleSocketTransform.GetLocation(), TraceEnd, FColor::Red, false, 5, 0, 2);
	//DrawDebugPoint(GetWorld(), ScreenLaserHit.Location, 3, FColor::Red, false, 5,0);

	FRotator MuzzleRotation = MuzzleSocketTransform.GetRotation().Rotator();
	FRotator EjectRotation = AmmoEjectSocketTransform.GetRotation().Rotator();
	PlayMuzzleEffect(MuzzleSocketTransform.GetLocation(), MuzzleRotation);
	PlayEjectAmmoEffect(AmmoEjectSocketTransform.GetLocation(), EjectRotation);
	// CreateFireEffectActor(MuzzleSocketTransform, AmmoEjectSocketTransform);

	PlaySound();

	if(!bInfiniteAmmo) { CurrentAmmoInClip--; }
	// bPlayFireEffect = true;
	// 총알 생성
	ServerRPCOnFire(MuzzleSocketTransform, TraceEnd);
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
	// if (ProjectileClass == nullptr)
	// {
	// 	UE_LOG(LogTemp, Log, TEXT("ProjectileClass is null"));
	// 	return;
	// }

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
	//서버에서 호출 
	ASOProjectileBase* Projectile = ProjectilePoolComponent->GetProjectile();
	if(Projectile)
	{
		FProjectileData ProjectileData;
		ProjectileData.Location = SpawnLocation;
		ProjectileData.Rotation = SpawnRotation;
		ProjectileData.FiringPawn = OwningCharacter;
		ProjectileData.Damage = WeaponStat.Damage;
		ProjectileData.WeaponType = WeaponData.WeaponType;
		ProjectileData.DistanceDamageFalloff = WeaponData.DistanceDamageFalloff;
		Projectile->InitializeProjectile(ProjectileData);
	}
}

void ASOGunBase::StopFire()
{
	GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
}

void ASOGunBase::PlayMuzzleEffect(const FVector& MuzzleLocation, FRotator& MuzzleRotation)
{
	if (WeaponData.MuzzleFlashEffect)
	{
		//FVector MuzzleFlashScale = FVector(0.3f, 0.3f, 0.3f);
		
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

float ASOGunBase::PlayAnimMontage(UAnimMontage* AnimMontage, USkeletalMeshComponent* Mesh, float InPlayRate, FName StartSectionName)
{
	UAnimInstance * AnimInstance = (Mesh)? Mesh->GetAnimInstance() : nullptr; 
	if( AnimMontage && AnimInstance )
	{
		float const Duration = AnimInstance->Montage_Play(AnimMontage, InPlayRate);

		if (Duration > 0.f)
		{
			// Start at a given Section.
			if( StartSectionName != NAME_None )
			{
				AnimInstance->Montage_JumpToSection(StartSectionName, AnimMontage);
			}

			return Duration;
		}
	}
	else
	{
		SO_LOG(LogSOTemp,Log,TEXT("WeaponAnimInstance is null"))
	}

	return 0.f;
}

void ASOGunBase::Recoil()
{
}

void ASOGunBase::Reload()
{
	SO_LOG(LogSOTemp, Log, TEXT("bReloading : %d bInfiniteAmmo : %d"), bReloading, bInfiniteAmmo)
	if(bReloading) return;
	
	// 소유한 총알 = 0
	if(CurrentAmmo <= 0)
	{
		SO_LOG(LogSOTemp,Log,TEXT("Find Ammo"));
		return;
	}

	// 탄창에 총알 가득
	if(CurrentAmmoInClip == WeaponStat.ClipSize)
	{
		SO_LOG(LogSOTemp,Log,TEXT("Full Ammo"));
		return;
	}

	if(WeaponData.ReloadWeaponMontage)
	{
		PlayAnimMontage(WeaponData.ReloadWeaponMontage, WeaponMesh);
	}

	if(WeaponData.ReloadMontage)
	{
		PlayAnimMontage(WeaponData.ReloadMontage, OwningCharacter->GetMesh());
	}
	ServerRPCOnReload();

	// 탄창 증가
	if(CurrentAmmo > 0)
	{
		int32 NeededAmmo;
		NeededAmmo = WeaponStat.ClipSize - CurrentAmmoInClip;
		if(CurrentAmmo > NeededAmmo)
		{
			CurrentAmmoInClip = WeaponStat.ClipSize;
			CurrentAmmo -= NeededAmmo;
		}
		else
		{
			CurrentAmmoInClip += CurrentAmmo;
			CurrentAmmo = 0;
		}		
	}		
	
	FTimerHandle ReloadTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, [this]()
	{
		bReloading = false;		
	}, 2.0f, false);
	//TODO
	// ReloadInterval 변수 추가하기
}

void ASOGunBase::Aim(bool bPressed)
{
	if(bPressed)
	{
		PressedTime = GetWorld()->GetTimeSeconds();
		// 꾹 누르고 있는 경우엔 그냥 조준 모션만 취하기
		// 확대 조준 상태에서 다시 누르면 해제		
	}
	else
	{
		if(bScopeAim)
		{
			bScopeAim = false;
			// Lens->SetVisibility(false);
			if(ASOPlayerController* PlayerController = CastChecked<ASOPlayerController>(OwningCharacter->GetController()))
			{
				PlayerController->SetViewTargetWithBlend(OwningCharacter,0.2);	
			}
			return;
		}
		ReleasedTime = GetWorld()->GetTimeSeconds();
		const float ClickDuration = ReleasedTime - PressedTime;
		if(ClickDuration < HoldThreshold)
		{
			bScopeAim = true;
			// Short click action
			// Lens->SetVisibility(true);
			if(ASOPlayerController* PlayerController = CastChecked<ASOPlayerController>(OwningCharacter->GetController()))
			{
				PlayerController->SetViewTargetWithBlend(this,0.2);	
			}
		}
	}
}

void ASOGunBase::Equip()
{
	SO_LOG(LogSOTemp, Warning, TEXT("Equip"))

	if (!bIsEquipped) return;
	if(!IsValid(OwningCharacter)) return;

	AActor* OwnerActor = GetOwner();
	if(OwnerActor)
	{
		SO_LOG(LogSONetwork, Log, TEXT("Owner : %s"), *OwnerActor->GetName())
	}
	else
	{
		SO_LOG(LogSONetwork, Log, TEXT("%s"), TEXT("No Owner"))
	}

	// OwningCharacter = OwnerActor;
	// 이거 해줘야 ServerRPC 가능
	// SetOwner(OwningCharacter);
	
	if(OwnerActor)
	{
		SO_LOG(LogSONetwork, Log, TEXT("Owner : %s"), *OwnerActor->GetName())
	}
	else
	{
		SO_LOG(LogSONetwork, Log, TEXT("%s"), TEXT("No Owner"))
	}

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

void ASOGunBase::Interact(ASOCharacterBase* PlayerCharacter)
{
	// Super::Interact(PlayerCharacter);
	USOInventoryComponent* Inven = PlayerCharacter->GetInventory();
	if(Inven)
	{
		// 인벤토리 컴포넌트 가져오기
		bIsEquipped = true;
		Inven->AddToInventory(this);
		// CharacterBase->EquipItem(this);
	}
}

void ASOGunBase::SetOwningCharacter(ASOCharacterBase* InOwningCharacter)
{
	SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	AActor* OwnerActor = GetOwner();
	if(OwnerActor)
	{
		SO_LOG(LogSONetwork, Log, TEXT("Owner : %s"), *OwnerActor->GetName())
	}
	else
	{
		SO_LOG(LogSONetwork, Log, TEXT("%s"), TEXT("No Owner"))
	}
	
	OwningCharacter = InOwningCharacter;
	OwnerActor = OwningCharacter;
	SetOwner(OwningCharacter);
	if (OwningCharacter == nullptr)	return;

	if(OwnerActor)
	{
		SO_LOG(LogSONetwork, Log, TEXT("Owner : %s"), *OwnerActor->GetName())
	}
	else
	{
		SO_LOG(LogSONetwork, Log, TEXT("%s"), TEXT("No Owner"))
	}
	SO_LOG(LogSOTemp, Warning, TEXT("End"))
}

FName ASOGunBase::GetPartsSocket(ESOGunPartsType InPartsType)
{
	return GetWeaponData()->PartsSocketMap[InPartsType];
}

void ASOGunBase::SetGunData(const uint8 InID)
{
	UE_LOG(LogTemp, Log, TEXT("ASOGunBase : SetGunData"));

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
		//0 인덱스 쓴거 수정 필요
		WeaponMesh->SetSkeletalMesh(WeaponData.WeaponMeshDataAsset->WeaponSkeletalMesh[0]);
		//AttachPoint = WeaponData.SocketName;
	}
	if (WeaponData.AmmoClass)
	{
		ProjectilePoolComponent->SetAmmoClass(WeaponData.AmmoClass);
	}
}

void ASOGunBase::DisablePhysics()
{
	SO_LOG(LogSOTemp, Log, TEXT("DisablePhysics"))
	WeaponMesh->SetSimulatePhysics(false);
}

void ASOGunBase::ServerRPCOnFire_Implementation(const FTransform& MuzzleTransform, const FVector& HitLocation)
{
	SO_LOG(LogSOTemp, Warning, TEXT("Begin"))
	CreateProjectile(MuzzleTransform, HitLocation);
	// 현재 시간. 쏜
	FireStartTime = GetWorld()->GetTimeSeconds();

	// bPlayFireEffect = !bPlayFireEffect;		// 여기서 OnRep
	
	SO_LOG(LogSOTemp, Warning, TEXT("End"))
}

void ASOGunBase::ServerRPCOnReload_Implementation()
{
	// Reload
	bReloading = true;

	FTimerHandle ReloadTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, [this]()
	{
		bReloading = false;		
	}, 2.0f, false);
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
	for (uint8 i = 1; i <= static_cast<uint8>(ESOFireMode::Max); i <<= 1)
	{
		// 작은 수부터 시작해서 낮은 비트 시 break;
		if (WeaponStat.FireMode & i)
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

FTransform ASOGunBase::GetSocketTransformByName(FName InSocketName, const USkeletalMeshComponent* SkelComp)
{
	const USkeletalMeshSocket* Socket = SkelComp->GetSocketByName(InSocketName);
	FTransform SocketTransform;
		
	if(IsValid(Socket))
	{
		SocketTransform = Socket->GetSocketTransform(SkelComp);
	}

	return SocketTransform;
}


//이펙트 동기화 
void ASOGunBase::OnRep_FireStartTime()
{
	// 클라 && 서버 X => 클라 본인 제외
	if(OwningCharacter->IsLocallyControlled() && !OwningCharacter->HasAuthority()) return;
	FTransform FireEffectSocketTransform = GetSocketTransformByName(WeaponData.FireEffectSocketName, WeaponMesh);
	FTransform AmmoEjectSocketTransform = GetSocketTransformByName(AmmoEjectSocketName, WeaponMesh);
	
	FRotator FireEffectRotation = FireEffectSocketTransform.GetRotation().Rotator();
	FRotator EjectRotation = AmmoEjectSocketTransform.GetRotation().Rotator();
	PlayMuzzleEffect(FireEffectSocketTransform.GetLocation(), FireEffectRotation);
	PlayEjectAmmoEffect(AmmoEjectSocketTransform.GetLocation(), EjectRotation);		
}

void ASOGunBase::OnRep_bReloading()
{
	if(OwningCharacter->IsLocallyControlled() && !OwningCharacter->HasAuthority()) return;
	if(bReloading)
	{
		if(WeaponData.ReloadWeaponMontage)
		{
			PlayAnimMontage(WeaponData.ReloadWeaponMontage, WeaponMesh);
		}

		if(WeaponData.ReloadMontage)
		{
			PlayAnimMontage(WeaponData.ReloadMontage, OwningCharacter->GetMesh());
		}
	}	
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "SOGameSubsystem.h"

#include "ProjectSO/Library/SOWeaponStructLibrary.h"

USOGameSubsystem::USOGameSubsystem()
{
	ConstructorHelpers::FObjectFinder<UDataTable> WeaponDataRef(
		TEXT("/Script/Engine.DataTable'/Game/StellarObsidian/GameData/DT_SOWeaponDataTable.DT_SOWeaponDataTable'"));
	if (WeaponDataRef.Succeeded())
	{
		WeaponDataTable = WeaponDataRef.Object;
	}

	ConstructorHelpers::FObjectFinder<UDataTable> WeaponStatRef(
		TEXT("/Script/Engine.DataTable'/Game/StellarObsidian/GameData/DT_SOWeaponStatTable.DT_SOWeaponStatTable'"));
	if (WeaponStatRef.Succeeded())
	{
		WeaponStatTable = WeaponStatRef.Object;
	}

	ConstructorHelpers::FObjectFinder<UDataTable> SpanwableDataRef(
		TEXT(
			"/Script/Engine.DataTable'/Game/StellarObsidian/GameData/DT_SOSpanwnableItemClasses.DT_SOSpanwnableItemClasses'"));
	if (SpanwableDataRef.Succeeded())
	{
		SpawnableItemDataTable = SpanwableDataRef.Object;
		TArray<FName> RowNames = SpawnableItemDataTable->GetRowNames();
		TotalSpawnableItem = RowNames.Num();
	}

	ConstructorHelpers::FObjectFinder<UDataTable> WeaponDamageDataRef(
		TEXT("/Script/Engine.DataTable'/Game/StellarObsidian/GameData/DT_SOWeaponDamageData.DT_SOWeaponDamageData'"));
	if (WeaponDamageDataRef.Succeeded())
	{
		WeaponDamageTable = WeaponDamageDataRef.Object;
	}

	ConstructorHelpers::FObjectFinder<UDataTable> WeaponDamageByBoneRef(
		TEXT("/Script/Engine.DataTable'/Game/StellarObsidian/GameData/DT_SOWeaponDamageByBone.DT_SOWeaponDamageByBone'"));
	if (WeaponDamageDataRef.Succeeded())
	{
		WeaponDamageByBoneTable = WeaponDamageByBoneRef.Object;
	}
}

void USOGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Warning, TEXT("Initialize "));

	ProcessWeaponDamageDataRows();
}

USOGameSubsystem* USOGameSubsystem::GetSOGameSubsystem()
{
	if (GetWorld())
	{
		if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
		{
			return GameInstance->GetSubsystem<USOGameSubsystem>();
		}
	}
	return nullptr;
}

FSOWeaponStat* USOGameSubsystem::GetWeaponStatData(const uint8 InID)
{
	if (!WeaponStatTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("WeaponStatTable is not assigned."));
		return nullptr;
	}

	FString RowName = FString::Printf(TEXT("%d"), InID);

	FSOWeaponStat* WeaponStatDataRow = WeaponStatTable->FindRow<FSOWeaponStat>(FName(*RowName), "");
	if (WeaponStatDataRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("Found WeaponStatData for ID: %d"), InID);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No WeaponStatData found for ID: %d"), InID);
	}
	return WeaponStatDataRow;
}

FSOWeaponData* USOGameSubsystem::GetWeaponData(const uint8 InID)
{
	if (!WeaponDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("WeaponDataTable is not assigned."));
		return nullptr;
	}

	FString RowName = FString::Printf(TEXT("%d"), InID);

	FSOWeaponData* WeaponDataRow = WeaponDataTable->FindRow<FSOWeaponData>(FName(*RowName), "");
	if (WeaponDataRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("Found WeaponDataTable for ID: %d"), InID);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No WeaponDataTable found for ID: %d"), InID);
	}
	return WeaponDataRow;
}

FSOWeaponDamageData* USOGameSubsystem::GetWeaponDamageData(const uint8 InID)
{
	if (!WeaponDamageTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("WeaponDamageTable is not assigned."));
		return nullptr;
	}

	FString RowName = FString::Printf(TEXT("%d"), InID);

	FSOWeaponDamageData* WeaponDamageDataRow = WeaponDamageTable->FindRow<FSOWeaponDamageData>(FName(*RowName), "");
	if (WeaponDamageDataRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("Found WeaponDamageTable for ID: %d"), InID);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No WeaponDamageTable found for ID: %d"), InID);
	}
	return WeaponDamageDataRow;
}

FSOSpawnableItemClasses* USOGameSubsystem::GetSpawnableItemData(const int32 InIndex)
{
	if (!SpawnableItemDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("WeaponDataTable is not assigned."));
		return nullptr;
	}

	FString RowName = FString::Printf(TEXT("%d"), InIndex);

	FSOSpawnableItemClasses* SpawnableDataRow = SpawnableItemDataTable->FindRow<FSOSpawnableItemClasses>(
		FName(*RowName), "");
	if (SpawnableDataRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("Found WeaponDataTable for ID: %d"), InIndex);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No WeaponDataTable found for ID: %d"), InIndex);
	}
	return SpawnableDataRow;
}

//Initialize 에서 ProcessWeaponDamageDataRows 호출
//이 함수는 Weapon Damage by bone 에 대한 정보를 가져온다. (파싱? )
void USOGameSubsystem::ProcessWeaponDamageDataRows() 
{
	if (!WeaponDamageTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("No WeaponDamageTable found "));

		return;
	}
	TArray<FName> RowNames = WeaponDamageTable->GetRowNames();
	int32 NumDamageDataRows = RowNames.Num();

	for (int32 ID = 0; ID < NumDamageDataRows; ++ID)
	{
		FSOWeaponDamageData* DamageDataTable = GetWeaponDamageData(ID); //행 가져오기 

		if (DamageDataTable)
		{
			for (TFieldIterator<FProperty> It(FSOWeaponDamageData::StaticStruct()); It; ++It)
			{
				FProperty* Property = *It;
				FString PropertyName = Property->GetName();
				FString PropertyValue;
				// Property Value 값 넣어주기 
				Property->ExportText_InContainer(0, PropertyValue, DamageDataTable, DamageDataTable, nullptr, PPF_None);
				InitializeWeaponDamageByBoneTable( ID, PropertyName ,PropertyValue);
			}
		}
	}
}

//WeaponDamageByBoneTable에 실질적인 데이터 Setting 진행 
void USOGameSubsystem::InitializeWeaponDamageByBoneTable(const int32 InID , const FString& InBoneName, const FString& InDamageValue)
{
	if(!WeaponDamageByBoneTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("No WeaponDamageByBoneTable found "));
		return ; 
	}
	
	FString RowName = FString::Printf(TEXT("%d"), InID);

	FSOWeaponDamageByBone* WeaponDamageByBoneDataRow = WeaponDamageByBoneTable->FindRow<FSOWeaponDamageByBone>(
		FName(*RowName), "");
	if (WeaponDamageByBoneDataRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("Found WeaponDamageByBoneTable for ID: %d"), InID);
		float InDamageFloatValue = FCString::Atof(*InDamageValue);
		WeaponDamageByBoneDataRow->DamageByHitLocationMap.Add(InBoneName,InDamageFloatValue);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No WeaponDamageByBoneTable found for ID: %d"), InID);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/LevelScriptActor.h"

#include "Infinite.generated.h"

class UCustomizableObjectInstanceUsage;
class ASkeletalMeshActor;
class APawn;
class UCustomizableObject;
class UCustomizableObjectInstance;
class UAnimInstance;
class UAnimationSharingSetup;
class USkeleton;

UCLASS(BlueprintType)
class AInstanceActor : public AActor
{
	GENERATED_BODY()

public:

	UPROPERTY()
	TArray<TObjectPtr<UCustomizableObjectInstanceUsage>> Usages;

	UPROPERTY(EditAnywhere, Category = Default)
	TObjectPtr<UCustomizableObject> CustomizableObject = nullptr;

	UPROPERTY(EditAnywhere, Category = Default)
	TSubclassOf<UAnimInstance> Animation = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Default)
	TObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance = nullptr;

};



USTRUCT()
struct FRegionScenery
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Scenery;
};


USTRUCT()
struct FRegionInstance
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<AInstanceActor>> Instances;
};


UCLASS()
class AInfinite : public ALevelScriptActor
{
	GENERATED_BODY()

protected:

	// AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	
	// Own interface
	void IterateRegions(const FIntVector2& StartRegionCoord, const TFunction<bool(FIntVector2, int32)>& VisitRegion);

	bool SpawnScenery(const FIntVector2 RegionCoord, const int32 RingIndex);

	bool SpawnInstances(const FIntVector2 RegionCoord, const int32 RingIndex);

public:

	UPROPERTY(BlueprintReadWrite, Category = Infinite)
	bool bBenchmarking = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LODManagement)
	float UpdateDistance = 0.0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Infinite)
	int32 RegionWidthScenery = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Infinite)
	int32 RegionWidthInstances = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Infinite)
	int32 OccludersNum = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Infinite)
	int32 InstancesNum = 8;

	UPROPERTY(BlueprintReadOnly, Category = Stats)
	int32 InstancesTotal = 0;

	UPROPERTY(BlueprintReadOnly, Category = Stats)
	int32 InstancesGenerated = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Infinite)
	int32 MaxRegionPerTick = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Infinite)
	int32 MaxInstancesPerTick = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Infinite)
	FVector2f RegionSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LODManagement)
	TObjectPtr<AActor> CentralActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Infinite)
	TObjectPtr<AActor> BenchmarkCamera = nullptr;
	
	UPROPERTY(EditAnywhere, Category = Infinite)
	TObjectPtr<APawn> PlayerPawn = nullptr;

	UPROPERTY(EditAnywhere, Category = Infinite)
	TSubclassOf<AActor> PlaneActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Infinite)
	TSubclassOf<AActor> OccluderActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Infinite)
	TArray<TSubclassOf<AInstanceActor>> InstanceActors;

private:

	UPROPERTY()
	TMap<FIntVector2, FRegionScenery> RegionScenery;

	UPROPERTY()
	TMap<FIntVector2, FRegionInstance> RegionInstances;

	FRandomStream RandomStream;
	
	FVector OriginalOccluderSize;
};
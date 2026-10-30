// Copyright Epic Games, Inc. All Rights Reserved.

#include "Infinite.h"

#include "Animation/AnimInstance.h"
#include "MuCO/CustomizableInstanceLODManagement.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstanceUsage.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"


void AInfinite::BeginPlay()
{
	Super::BeginPlay();

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
	System->SetReplaceDiscardedWithReferenceMeshEnabled(true);

	IConsoleVariable* OnlyUpdateCloseCustomizableObjectsCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("b.OnlyUpdateCloseCustomizableObjects"));
	if (ensure(OnlyUpdateCloseCustomizableObjectsCvar))
	{
		OnlyUpdateCloseCustomizableObjectsCvar->Set(true);
	}
	
	if (UCustomizableInstanceLODManagement* LODManagement = Cast<UCustomizableInstanceLODManagement>(System->GetInstanceLODManagement()))
	{
		LODManagement->SetCustomizableObjectsUpdateDistance(UpdateDistance);
	}
	
	FTransform Transform;
	if (AActor* Actor = UGameplayStatics::BeginDeferredActorSpawnFromClass(GetWorld(), OccluderActor, Transform))
	{
		UGameplayStatics::FinishSpawningActor(Actor, Transform);

		FBox Box = Actor->CalculateComponentsBoundingBoxInLocalSpace();
		OriginalOccluderSize = Box.GetSize();

		Actor->Destroy();
	}

	RandomStream.Initialize(0);
}


void AInfinite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	IConsoleVariable* OnlyUpdateCloseCustomizableObjectsCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("b.OnlyUpdateCloseCustomizableObjects"));
	if (ensure(OnlyUpdateCloseCustomizableObjectsCvar))
	{
		OnlyUpdateCloseCustomizableObjectsCvar->Set(false);
	}
}


void AInfinite::IterateRegions(const FIntVector2& StartRegionCoord, const TFunction<bool(FIntVector2, int32)>& VisitRegion)
{
	FIntVector2 Offsets[] = { FIntVector2(0, -1), FIntVector2(-1, 0), FIntVector2(0, 1), FIntVector2(1, 0) };

	if (VisitRegion(StartRegionCoord, 0))
	{
		return;
	}
	
	const int32 NumRing = FMath::Max(RegionWidthScenery, RegionWidthInstances);
	for (int32 RingIndex = 1; RingIndex < NumRing; ++RingIndex)
	{
		const int32 NumRegions = 8 * RingIndex;

		const FIntVector2 Start(RingIndex, RingIndex);
		FIntVector2 LocalRegionCoord = Start;
		
		for (int32 RegionIndex = 0; RegionIndex < NumRegions; ++RegionIndex)
		{
			const FIntVector2 RegionCoord = StartRegionCoord + LocalRegionCoord;

			if (VisitRegion(RegionCoord, RingIndex))
			{
				return;
			}

			LocalRegionCoord += Offsets[RegionIndex / (RingIndex * 2)];
		}
	}
}


bool AInfinite::SpawnScenery(const FIntVector2 RegionCoord, const int32 RingIndex)
{
	if (RegionWidthScenery > RingIndex)
	{
		FRegionScenery& Region = RegionScenery.FindOrAdd(RegionCoord);

		if (Region.Scenery.IsEmpty())
		{
			const FVector RegionLocation(RegionCoord.X * RegionSize.X, RegionCoord.Y * RegionSize.Y, 0.0);

			if (PlaneActor)
			{
				FTransform Transform;
				Transform.SetLocation(RegionLocation);
				Transform.SetScale3D(FVector(RegionSize.X, RegionSize.Y, 1.0));
					
				if (AActor* Actor = UGameplayStatics::BeginDeferredActorSpawnFromClass(GetWorld(), PlaneActor, Transform))
				{
					UGameplayStatics::FinishSpawningActor(Actor, Transform);

					Region.Scenery.Add(Actor);	
				}
			}

			if (OccluderActor)
			{
				for (int32 OccluderIndex = 0; OccluderIndex < OccludersNum; ++OccluderIndex)
				{
					const FVector RegionSize3D(RegionSize.X, RegionSize.Y, 1.0);
						
					FVector OccluderSize(RandomStream.FRandRange(200.0, 1000.0), RandomStream.FRandRange(200.0, 1000.0), RandomStream.RandRange(500.0, 3000.0));
						
					FVector Location(RandomStream.GetFraction(), RandomStream.GetFraction(), OccluderSize.Z * 0.5);
					Location = RegionLocation + (Location * RegionSize3D - RegionSize3D / 2.0);
						
					FTransform Transform;
					Transform.SetLocation(Location);
					Transform.SetScale3D(OccluderSize / OriginalOccluderSize);

					constexpr ESpawnActorCollisionHandlingMethod CollisionHandling = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
					if (AActor* Actor = UGameplayStatics::BeginDeferredActorSpawnFromClass(GetWorld(), OccluderActor, Transform, CollisionHandling))
					{
						UGameplayStatics::FinishSpawningActor(Actor, Transform);

						Region.Scenery.Add(Actor);
					}
				}
			}

			return true;
		}
	}
	
	return false;
}


bool AInfinite::SpawnInstances(const FIntVector2 RegionCoord, const int32 RingIndex)
{
	if (RegionWidthInstances > RingIndex)
	{
		FRegionInstance& Region = RegionInstances.FindOrAdd(RegionCoord);
		
		const FVector RegionLocation(RegionCoord.X * RegionSize.X, RegionCoord.Y * RegionSize.Y, 0.0);

		bool bInstancesSpawned = false;
		for (int32 InstanceIndex = 0; InstanceIndex < MaxInstancesPerTick && Region.Instances.Num() < InstancesNum; ++InstanceIndex)
		{
			const FVector RegionSize3D(RegionSize.X, RegionSize.Y, 0.0);

			FVector Location = RegionLocation + FVector(RandomStream.RandRange(-RegionSize3D.X/2, +RegionSize3D.X/2), RandomStream.RandRange(-RegionSize3D.Y/2, +RegionSize3D.Y/2), 0.0);
			FRotator Rotator(0.0, RandomStream.GetFraction() * 360.0, 0.0);
			
			FTransform Transform;
			Transform.SetLocation(Location);
			Transform.SetRotation(Rotator.Quaternion());
	
			const TSubclassOf<AInstanceActor>& InstanceActorClass =  InstanceActors[RandomStream.RandRange(0, InstanceActors.Num() - 1)];
			if (!InstanceActorClass)
			{
				continue;
			}

			AInstanceActor* InstanceActor = InstanceActorClass.GetDefaultObject();

			UCustomizableObject* Object = InstanceActor->CustomizableObject;
			if (!InstanceActor->CustomizableObject)
			{
				continue;
			}

			constexpr ESpawnActorCollisionHandlingMethod CollisionHandling = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			if (AInstanceActor* Actor = Cast<AInstanceActor>(UGameplayStatics::BeginDeferredActorSpawnFromClass(GetWorld(), InstanceActorClass, Transform, CollisionHandling)))
			{
				UGameplayStatics::FinishSpawningActor(Actor, Transform);
				
				UCustomizableObjectInstance* Instance = InstanceActor->CustomizableObject->CreateInstance();
				Instance->SetRandomValuesFromStream(RandomStream);

				Actor->CustomizableObjectInstance = Instance;

				TArray<USkeletalMeshComponent*> Components;
				
				for (int32 ComponentIndex = 0; ComponentIndex < Instance->GetNumComponents(); ++ComponentIndex)
				{
					FString SkeletalMeshComponentName = FString::Printf(TEXT("Skeletal Mesh Component %d"), ComponentIndex);

					USkeletalMeshComponent* Component;
					if (ComponentIndex == 0)
					{
						Component = Cast<USkeletalMeshComponent>(Actor->GetRootComponent());
						Component->Rename(*SkeletalMeshComponentName);
					}
					else
					{
						Component = NewObject<USkeletalMeshComponent>(Actor, USkeletalMeshComponent::StaticClass(), FName(*SkeletalMeshComponentName));
						Component->CreationMethod = EComponentCreationMethod::SimpleConstructionScript;
						Component->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
						Component->RegisterComponent();
					}
					
					if (!Component)
					{
						continue;
					}

					const FName& ComponentName = Object->GetComponentName(ComponentIndex);

					FString CustomizableComponentName = FString::Printf(TEXT("Customizable Object Instance Usage %d"), ComponentIndex);
					UCustomizableObjectInstanceUsage* Usage = NewObject<UCustomizableObjectInstanceUsage>(Actor, UCustomizableObjectInstanceUsage::StaticClass(), FName(*CustomizableComponentName));
					Usage->AttachTo(Component);
					Usage->SetCustomizableObjectInstance(Instance);
					Usage->SetComponentName(ComponentName);

					Components.Add(Component);
					
					Actor->Usages.Add(Usage);
				}

				if (!Components.IsEmpty())
				{
					// Set the animation Instance
					Components[0]->SetAnimInstanceClass(Actor->Animation);
				
					for (int32 ComponentIndex = 1; ComponentIndex < Components.Num(); ++ComponentIndex)
					{
						Components[ComponentIndex]->SetLeaderPoseComponent(Components[0]);
					}
				}

				Region.Instances.Add(Actor);
				bInstancesSpawned = true;
			}
		}
		
		return bInstancesSpawned;
	}

	return false;
}


void AInfinite::Tick(float DeltaSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AInfinite::Tick)
	
	Super::Tick(DeltaSeconds);

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();

	AActor* PlayerActor = bBenchmarking ? BenchmarkCamera : PlayerPawn;

	FVector2f PlayerLocation = FVector2f::Zero();
	if (PlayerActor)
	{
		FVector Location = PlayerActor->GetActorLocation();
		PlayerLocation = FVector2f(Location.X, Location.Y);
		
		if (UCustomizableInstanceLODManagement* LODManagement = Cast<UCustomizableInstanceLODManagement>(System->GetInstanceLODManagement()))
		{
			LODManagement->ClearViewCenters();
			LODManagement->AddViewCenter(PlayerActor);
		}
	}
	
	FVector2f CurrentRegionCoordF = (PlayerLocation / RegionSize).RoundToVector();
	FIntVector2 CurrentRegionCoord(CurrentRegionCoordF.X, CurrentRegionCoordF.Y);

	// Destroy Regions
	for (TMap<FIntVector2, FRegionScenery>::TIterator It = RegionScenery.CreateIterator(); It; ++It)
	{
		FRegionScenery& Region = It.Value();

		const int32 DiffX = FMath::Abs(It.Key().X - CurrentRegionCoord.X);
		const int32 DiffY = FMath::Abs(It.Key().Y - CurrentRegionCoord.Y);
			
		if (DiffX > RegionWidthScenery || DiffY > RegionWidthScenery)
		{
			for (AActor* Scenery : Region.Scenery)
			{
				Scenery->Destroy();
			}
			
			It.RemoveCurrent();
		}
	}

	for (TMap<FIntVector2, FRegionInstance>::TIterator It = RegionInstances.CreateIterator(); It; ++It)
	{
		FRegionInstance& Region = It.Value();
		
		const int32 DiffX = FMath::Abs(It.Key().X - CurrentRegionCoord.X);
		const int32 DiffY = FMath::Abs(It.Key().Y - CurrentRegionCoord.Y);
		
		if (DiffX > RegionWidthInstances || DiffY > RegionWidthInstances)
		{
			for (const TObjectPtr<AInstanceActor>& Instance : Region.Instances)
			{
				Instance->Destroy();
			}
			
			It.RemoveCurrent();
		}
	}

	// Spawn Scenary
	IterateRegions(CurrentRegionCoord, [this](const FIntVector2 RegionCoord, const int32 RegionIndex)
	{
		return SpawnScenery(RegionCoord, RegionIndex);
	});

	// Spawn Instances
	IterateRegions(CurrentRegionCoord, [this](const FIntVector2 RegionCoord, const int32 RegionIndex)
	{
		return SpawnInstances(RegionCoord, RegionIndex);
	});
	
	// Stats
	InstancesTotal = 0;
	InstancesGenerated = 0;
	
	for (const TTuple<FIntVector2, FRegionInstance>& Pair : RegionInstances)
	{
		InstancesTotal += Pair.Value.Instances.Num();

		for (const TObjectPtr<AInstanceActor>& Instance : Pair.Value.Instances)
		{
			InstancesGenerated += Instance->CustomizableObjectInstance->HasAnySkeletalMesh();
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomRebindSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GroomComponent.h"
#include "GroomAsset.h"
#include "TalkFaceAnimInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGroomRebind, Log, All);

namespace GroomRebind
{
	static constexpr float PollInterval = 0.25f;
	static const FName HeadBone(TEXT("head"));

	// Two Fab grooms were authored around their own head meshes, which do not sit
	// where the Mutable-generated head sits. The groom binding does not relocate
	// the strands (it only makes them follow the mesh), so the offset has to be
	// applied on the component.
	//
	// Offset = crown of the runtime head - crown of the head the groom was authored on,
	// measured from the mesh bounds:
	//   runtime head (SK_CO_Character_Head / SK_ReferenceMesh): crown z = 174.61, y = 3.42
	//   SK_FemaleHead_BA   (NomadWaveUndercut): crown z = 180.05, y = 1.94
	//   SKM_MH_Groom_Head  (Hair_M_UpdoMessyBun): crown z = 159.74, y = 2.18
	// Scale stays at 1: the author heads and the runtime head have the same depth
	// (~21 cm), so only a translation is needed.
	static bool GetCustomTransform(const UGroomComponent* Groom, FVector& OutLocation)
	{
		if (!Groom || !Groom->GroomAsset)
		{
			return false;
		}

		const FString AssetName = Groom->GroomAsset->GetName();
		if (AssetName.Contains(TEXT("Hair_M_UpdoMessyBun")))
		{
			OutLocation = FVector(0.0, 1.24, 14.87);
			return true;
		}

		if (AssetName.Contains(TEXT("NomadWaveUndercut")))
		{
			OutLocation = FVector(0.0, 1.48, -5.44);
			return true;
		}

		return false;
	}
}

bool UGroomRebindSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UGroomRebindSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Strip the bindings before the first frame renders: the projection pass asserts
	// as soon as it sees a binding against a Mutable mesh that is mid-update.
	PollGrooms();
	ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UGroomRebindSubsystem::OnActorSpawned));

	InWorld.GetTimerManager().SetTimer(
		PollTimer, this, &UGroomRebindSubsystem::PollGrooms, GroomRebind::PollInterval, true);
}

void UGroomRebindSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimer);
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}

	Super::Deinitialize();
}

void UGroomRebindSubsystem::PollGrooms()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UpdateActorGrooms(*It);
	}
}

void UGroomRebindSubsystem::OnActorSpawned(AActor* Actor)
{
	if (Actor)
	{
		UpdateActorGrooms(Actor);
	}
}

void UGroomRebindSubsystem::UpdateActorGrooms(AActor* Actor)
{
	TInlineComponentArray<UGroomComponent*> Grooms(Actor);

	for (UGroomComponent* Groom : Grooms)
	{
		// MetaHuman grooms are bound to the MetaHuman face, which is not a Mutable mesh.
		if (!Groom || Groom->ComponentHasTag(UTalkFaceAnimInstance::MetaHumanTag))
		{
			continue;
		}

		// The mesh the strands are attached to.
		const USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(Groom->GetAttachParent());
		USkeletalMesh* Mesh = Parent ? Parent->GetSkeletalMeshAsset() : nullptr;

		// The groom binding never becomes valid against a Mutable-generated mesh
		// ("Waiting for groom bindings to be ready 0/1"), so the strands do not
		// follow the head on their own. Parent them to the head bone instead and
		// bake the authored-space correction into the relative transform.
		FVector DesiredLocation = FVector::ZeroVector;
		GroomRebind::GetCustomTransform(Groom, DesiredLocation);

		bool bTransformChanged = false;
		if (Mesh && Mesh->GetRefSkeleton().FindBoneIndex(GroomRebind::HeadBone) != INDEX_NONE)
		{
			// Desired is expressed in the parent component's space, so undo the
			// head bone's reference transform to get the socket-relative one.
			const FTransform HeadRef(Mesh->GetComposedRefPoseMatrix(GroomRebind::HeadBone));
			const FTransform Desired(FQuat::Identity, DesiredLocation);
			const FTransform Rel = Desired * HeadRef.Inverse();

			if (Groom->GetAttachSocketName() != GroomRebind::HeadBone)
			{
				Groom->AttachToComponent(
					const_cast<USkeletalMeshComponent*>(Parent),
					FAttachmentTransformRules::KeepRelativeTransform,
					GroomRebind::HeadBone);
				bTransformChanged = true;
			}

			if (!Groom->GetRelativeTransform().Equals(Rel, KINDA_SMALL_NUMBER))
			{
				Groom->SetRelativeTransform(Rel);
				bTransformChanged = true;
			}
		}
		else if (!Groom->GetRelativeLocation().Equals(DesiredLocation, KINDA_SMALL_NUMBER))
		{
			Groom->SetRelativeLocation(DesiredLocation);
			bTransformChanged = true;
		}

		// The binding is never valid against a Mutable-generated mesh, and while the
		// mesh is being regenerated the projection pass reads a stale position buffer
		// and asserts (HairStrandsMeshProjection.cpp, MeshPositionBuffer0 == Section.PositionBuffer).
		// The strands already follow the head through the bone attachment above,
		// so drop the binding entirely and render the groom rigidly.
		if (Groom->BindingAsset)
		{
			Groom->SetBindingAsset(nullptr);
			UE_LOG(LogGroomRebind, Log, TEXT("Cleared groom binding on %s (%s)"),
				*Groom->GetName(), Mesh ? *Mesh->GetName() : TEXT("no mesh"));
		}
		else if (bTransformChanged)
		{
			UE_LOG(LogGroomRebind, Verbose, TEXT("Reattached %s to %s"),
				*Groom->GetName(), Mesh ? *Mesh->GetName() : TEXT("no mesh"));
		}
	}
}

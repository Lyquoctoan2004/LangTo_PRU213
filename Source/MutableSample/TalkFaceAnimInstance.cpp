#include "TalkFaceAnimInstance.h"

#include "CharacterTalkChat.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogTalkFace, Log, All);

const FName UTalkFaceAnimInstance::MetaHumanTag(TEXT("MetaHuman"));

void FTalkFaceAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UTalkFaceAnimInstance* Instance = CastChecked<UTalkFaceAnimInstance>(InAnimInstance);
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	Curves = Instance->GetCurves();
	NeckThickness = Settings.MetaHumanBodyNeckScale;
	LowerNeckThickness = Settings.MetaHumanBodyLowerNeckScale;
	SourcePose.Reset();

	USkeletalMeshComponent* Source = Instance->PoseSource.Get();
	if (Source && Source->LeaderPoseComponent.IsValid())
	{
		Source = Cast<USkeletalMeshComponent>(Source->LeaderPoseComponent.Get());
	}
	const USkeletalMesh* SourceMesh = Source ? Source->GetSkeletalMeshAsset() : nullptr;
	const USkeletalMesh* FaceMesh = Instance->GetSkelMeshComponent()->GetSkeletalMeshAsset();
	if (!SourceMesh || !FaceMesh)
	{
		return;
	}

	// Mutable rebuilds the source mesh when the character changes, so map the bones again whenever either mesh changes.
	if (MappedFaceMesh.Get() != FaceMesh || MappedSourceMesh.Get() != SourceMesh)
	{
		const FReferenceSkeleton& FaceSkeleton = FaceMesh->GetRefSkeleton();
		const FReferenceSkeleton& SourceSkeleton = SourceMesh->GetRefSkeleton();
		SourceBoneOfFaceBone.Init(INDEX_NONE, FaceSkeleton.GetNum());
		NeckBones.Reset();
		LowerNeckBones.Reset();
		for (int32 Bone = 0; Bone < FaceSkeleton.GetNum(); ++Bone)
		{
			// RigLogic drives the face's facial joints; the source's FACIAL_ bones belong to its own head shape.
			const FName Name = FaceSkeleton.GetBoneName(Bone);
			if (Instance->bBody || !Name.ToString().StartsWith(TEXT("FACIAL_")))
			{
				SourceBoneOfFaceBone[Bone] = SourceSkeleton.FindBoneIndex(Name);
			}
			// The body's upper neck (whose top, and the scalp, are skinned to its head bone) is thinned to stay inside the
			// face's neck; its lower neck only a little, as it fits the clothes. The face's own neck base is pulled in
			// after RigLogic (ShrinkNeckBase), which would undo a change made here.
			if (Instance->bBody && (Name == TEXT("neck_02") || Name == TEXT("head")))
			{
				NeckBones.Add(Bone);
			}
			else if (Instance->bBody && Name == TEXT("neck_01"))
			{
				LowerNeckBones.Add(Bone);
			}
		}
		MappedFaceMesh = FaceMesh;
		MappedSourceMesh = SourceMesh;
	}
	SourcePose = Source->GetComponentSpaceTransforms();
}

bool FTalkFaceAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	Output.ResetToRefPose();
	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& Bones = Pose.GetBoneContainer();

	if (SourcePose.Num() > 0)
	{
		// The face is attached to the source with no offset, so both share one component space. Copied bones take the
		// source's component-space transform; the rest keep their reference pose under their (possibly copied) parent.
		TArray<FTransform> ComponentSpace;
		ComponentSpace.SetNum(Pose.GetNumBones());
		for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
		{
			const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
			const FTransform ParentSpace = Parent.IsValid() ? ComponentSpace[Parent.GetInt()] : FTransform::Identity;
			const int32 MeshBone = Bones.MakeMeshPoseIndex(Bone).GetInt();
			const int32 Source = SourceBoneOfFaceBone.IsValidIndex(MeshBone) ? SourceBoneOfFaceBone[MeshBone] : INDEX_NONE;
			if (SourcePose.IsValidIndex(Source))
			{
				FTransform Copied = SourcePose[Source];
				// Neck bones run along their X axis; scaling Y and Z changes the neck's thickness, not its length.
				const float Thickness = NeckBones.Contains(MeshBone) ? NeckThickness : LowerNeckBones.Contains(MeshBone) ? LowerNeckThickness : 1.f;
				if (Thickness != 1.f)
				{
					Copied.SetScale3D(Copied.GetScale3D() * FVector(1.f, Thickness, Thickness));
				}
				ComponentSpace[Bone.GetInt()] = Copied;
				Pose[Bone] = Copied.GetRelativeTransform(ParentSpace);
				Pose[Bone].NormalizeRotation();
			}
			else
			{
				ComponentSpace[Bone.GetInt()] = Pose[Bone] * ParentSpace;
			}
		}
	}

	for (const TPair<FName, float>& Curve : Curves)
	{
		Output.Curve.Set(Curve.Key, Curve.Value);
	}
	return true;
}

void UTalkFaceAnimInstance::ShrinkNeckBase()
{
	USkeletalMeshComponent* Face = GetSkelMeshComponent();
	const USkeletalMesh* Mesh = Face ? Face->GetSkeletalMeshAsset() : nullptr;
	const float Scale = GetDefault<UCharacterTalkSettings>()->MetaHumanNeckBaseScale;
	if (!Mesh || Scale == 1.f)
	{
		return;
	}

	if (NeckBaseMesh.Get() != Mesh)
	{
		// neck_01 is thinned across; everything below the neck (spine, clavicles, chest helpers) shrinks every way towards
		// its joint inside the body. Parents come before their children, so InHead marks neck_02 and all under it.
		const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
		TArray<bool> InHead;
		InHead.Init(false, Skeleton.GetNum());
		NeckBaseBones.Reset();
		for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
		{
			const FName Name = Skeleton.GetBoneName(Bone);
			const int32 Parent = Skeleton.GetParentIndex(Bone);
			InHead[Bone] = Name == TEXT("neck_02") || (Parent != INDEX_NONE && InHead[Parent]);
			if (!InHead[Bone])
			{
				NeckBaseBones.Emplace(Bone, Name == TEXT("neck_01"));
			}
		}
		NeckBaseMesh = Mesh;
	}

	// The finished pose that renders this frame (RigLogic has run); it is rebuilt every frame, so nothing accumulates.
	TArray<FTransform>& Pose = const_cast<TArray<FTransform>&>(Face->GetComponentSpaceTransforms());
	for (const TPair<int32, bool>& Bone : NeckBaseBones)
	{
		if (Pose.IsValidIndex(Bone.Key))
		{
			Pose[Bone.Key].SetScale3D(Pose[Bone.Key].GetScale3D() * (Bone.Value ? FVector(1.f, Scale, Scale) : FVector(Scale)));
		}
	}
}

USkeletalMeshComponent* UTalkFaceAnimInstance::AttachTo(AActor* Speaker)
{
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	if (!Speaker || Settings.MetaHumanFace.IsNull())
	{
		return nullptr;
	}

	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Speaker);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->ComponentHasTag(MetaHumanTag))
		{
			return Mesh;
		}
	}

	// The speaker's own head: Mutable's "Head" component, or failing that any mesh with a head bone.
	USkeletalMeshComponent* Head = nullptr;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetBoneIndex(TEXT("head")) != INDEX_NONE && (!Head || Mesh->GetName().Contains(TEXT("Head"))))
		{
			Head = Mesh;
		}
	}
	// Mutable builds the head asynchronously; until then there is nothing to follow (callers may try again).
	if (!Head)
	{
		return nullptr;
	}
	USkeletalMesh* FaceMesh = Cast<USkeletalMesh>(Settings.MetaHumanFace.TryLoad());
	if (!FaceMesh)
	{
		UE_LOG(LogTalkFace, Warning, TEXT("No MetaHuman face on '%s': cannot load %s."), *Speaker->GetName(), *Settings.MetaHumanFace.ToString());
		return nullptr;
	}
	USkeletalMeshComponent* PoseSource = Head->LeaderPoseComponent.IsValid() ? Cast<USkeletalMeshComponent>(Head->LeaderPoseComponent.Get()) : Head;

	USkeletalMeshComponent* Face = NewObject<USkeletalMeshComponent>(Speaker, TEXT("MetaHumanFace"));
	Face->ComponentTags.Add(MetaHumanTag);
	Face->SetSkeletalMesh(FaceMesh);
	Face->SetAnimInstanceClass(UTalkFaceAnimInstance::StaticClass());
	Face->SetupAttachment(Head);
	Face->AddTickPrerequisiteComponent(PoseSource);
	Face->RegisterComponent();
	Speaker->AddInstanceComponent(Face);
	if (UTalkFaceAnimInstance* Anim = Cast<UTalkFaceAnimInstance>(Face->GetAnimInstance()))
	{
		Anim->PoseSource = PoseSource;
		// The face's neck base and upper chest would show through the clothes of slimmer bodies; RigLogic sets those joints
		// in the post-process, so they are pulled in once the pose is final.
		Face->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(Anim, &UTalkFaceAnimInstance::ShrinkNeckBase));
	}

	// The body's neck can be wider than the face's and would show around it. Meshes that followed the head's pose copy it
	// through this anim instance instead, which thins their neck (a follower cannot be posed apart from its leader).
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh != Head && Mesh->LeaderPoseComponent.Get() == PoseSource)
		{
			Mesh->SetLeaderPoseComponent(nullptr);
			Mesh->SetAnimInstanceClass(UTalkFaceAnimInstance::StaticClass());
			Mesh->AddTickPrerequisiteComponent(PoseSource);
			if (UTalkFaceAnimInstance* Anim = Cast<UTalkFaceAnimInstance>(Mesh->GetAnimInstance()))
			{
				Anim->PoseSource = PoseSource;
				Anim->bBody = true;
			}
		}
	}

	// The speaker's own head and hair are hidden; the head keeps animating because the face copies its pose.
	Head->SetHiddenInGame(true);
	Head->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	TInlineComponentArray<UGroomComponent*> OwnGrooms(Speaker);
	for (UGroomComponent* Groom : OwnGrooms)
	{
		if (Groom)
		{
			Groom->SetHiddenInGame(true);
		}
	}

	// Grooms built with the MetaHuman, each bound to the face through "<groom>_Binding" next to it.
	for (const FSoftObjectPath& Path : Settings.MetaHumanGrooms)
	{
		UGroomAsset* GroomAsset = Cast<UGroomAsset>(Path.TryLoad());
		const FString BindingName = Path.GetAssetName() + TEXT("_Binding");
		UGroomBindingAsset* Binding = Cast<UGroomBindingAsset>(
			FSoftObjectPath(FPaths::GetPath(Path.GetLongPackageName()) / BindingName + TEXT(".") + BindingName).TryLoad());
		if (!GroomAsset)
		{
			UE_LOG(LogTalkFace, Warning, TEXT("Cannot load MetaHuman groom %s."), *Path.ToString());
			continue;
		}
		UGroomComponent* Groom = NewObject<UGroomComponent>(Speaker);
		Groom->ComponentTags.Add(MetaHumanTag);
		Groom->SetupAttachment(Face);
		Groom->SetGroomAsset(GroomAsset, Binding);
		Groom->RegisterComponent();
		Speaker->AddInstanceComponent(Groom);
	}

	UE_LOG(LogTalkFace, Log, TEXT("MetaHuman face %s on '%s' (pose from %s, %d grooms)."), *FaceMesh->GetName(), *Speaker->GetName(),
		*PoseSource->GetName(), Settings.MetaHumanGrooms.Num());
	return Face;
}

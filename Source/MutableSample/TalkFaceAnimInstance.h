#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "CoreMinimal.h"
#include "TalkFaceAnimInstance.generated.h"

class USkeletalMeshComponent;

/**
 * Evaluates the MetaHuman face: the body bones are copied by name from the character's own skeleton (so the face follows
 * the head and neck), the FACIAL_ bones stay for RigLogic, and the expression curves are added for the face mesh's
 * RigLogic post-process blueprint to turn into joints, blend shapes and wrinkles.
 */
USTRUCT()
struct FTalkFaceAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FTalkFaceAnimInstanceProxy() = default;
	explicit FTalkFaceAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	// Copied on the game thread in PreUpdate, read while evaluating.
	TArray<FTransform> SourcePose;
	/** For each bone of the face mesh, the matching bone of the source mesh (INDEX_NONE: keep the face's own pose). */
	TArray<int32> SourceBoneOfFaceBone;
	TWeakObjectPtr<const USkeletalMesh> MappedFaceMesh;
	TWeakObjectPtr<const USkeletalMesh> MappedSourceMesh;
	/** The body's upper neck bones, thinned by NeckThickness, and its lower neck, by LowerNeckThickness. */
	TArray<int32> NeckBones;
	TArray<int32> LowerNeckBones;
	float NeckThickness = 1.f;
	float LowerNeckThickness = 1.f;
	TArray<TPair<FName, float>> Curves;
};

/** The anim instance of the MetaHuman face put on a talking character (see AttachTo). */
UCLASS(Transient, NotBlueprintable)
class MUTABLESAMPLE_API UTalkFaceAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Tag on the face and its grooms, so other code (GroomRebindSubsystem) leaves them alone. */
	static const FName MetaHumanTag;

	/**
	 * Puts the MetaHuman face from UCharacterTalkSettings on Speaker in place of its own head: the face follows the
	 * speaker's skeleton, its grooms are added, and the speaker's own head and grooms are hidden. Does nothing without a
	 * configured face; calling it again returns the face already there. Returns the face component, or null.
	 */
	static USkeletalMeshComponent* AttachTo(AActor* Speaker);

	/** The mesh whose pose the face copies (the speaker's own head). */
	TWeakObjectPtr<USkeletalMeshComponent> PoseSource;
	/**
	 * Set on the speaker's body meshes, which copy the head's pose through this instance too: every bone is copied and the
	 * neck is thinned (MetaHumanBodyNeckScale) so it stays inside the face's neck. Otherwise this is the face itself.
	 */
	bool bBody = false;

	/** Expression curve values (CTRL_expressions_* names, 0..1) for the next frames, replacing the previous ones. */
	void SetCurves(TArray<TPair<FName, float>>&& InCurves) { Curves = MoveTemp(InCurves); }
	const TArray<TPair<FName, float>>& GetCurves() const { return Curves; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FTalkFaceAnimInstanceProxy(this); }

private:
	/** Pulls the face's neck base and upper chest in (MetaHumanNeckBaseScale), on its final pose after RigLogic. */
	void ShrinkNeckBase();

	TArray<TPair<FName, float>> Curves;
	/** Bones below the face's neck_02 and whether each is only thinned across (neck_01) rather than shrunk every way. */
	TArray<TPair<int32, bool>> NeckBaseBones;
	TWeakObjectPtr<const USkeletalMesh> NeckBaseMesh;
};

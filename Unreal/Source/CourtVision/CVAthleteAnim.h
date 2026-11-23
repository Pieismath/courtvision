#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CVRacketGrip.h"
#include "CVAthleteAnim.generated.h"

class UAnimSequence;
struct FCVAthleteAnimProxy;

/** Native tennis graph: continuous locomotion, one-shot strokes and tracked wrist IK.
 * Configure once after SetAnimInstanceClass; SetTennisState/SetTrackedRacketTarget
 * are game-thread inputs. All pose work runs through Unreal's animation proxy.
 */
UCLASS(Transient, Blueprintable)
class COURTVISION_API UCVAthleteAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="CourtVision|Animation")
    void ConfigureClips(UAnimSequence* Ready, UAnimSequence* Run,
                        UAnimSequence* Forehand, UAnimSequence* Backhand, UAnimSequence* Serve);

    /** Call once per game tick. An increased remaining timer starts a new stroke;
     * a continuous positive timer never restarts the current animation. */
    UFUNCTION(BlueprintCallable, Category="CourtVision|Animation")
    void SetTennisState(float MovementSpeedCmS, float RemainingSwingSeconds,
                       bool bBackhand, float SwingPlayRate = 1.f, bool bServe = false);

    /** Explicit event alternative for callers that do not use a countdown timer. */
    UFUNCTION(BlueprintCallable, Category="CourtVision|Animation")
    void TriggerSwing(bool bBackhand, float SwingPlayRate = 1.f, bool bServe = false);

    /** Register confirmed contact without seeking or restarting the stroke timeline. */
    void NotifyRacketContact(bool bBackhand);
    void SetContactTarget(FVector ComponentSpaceHead);
    void PrepareContact(FVector ComponentSpaceHead);
    void ClearContactPlan(){bPlannedContact=false;}
    FVector AuthoredContact[2];
    bool bPlannedContact=false;
    void ConfigureGrip(const USkeletalMesh* Mesh);
    FVector GetServeContactPoint() const { return ServeContactPoint; }
    void SetRacketOrientation(FQuat ComponentRotation);
    void SetFootPlacement(FVector Left, FVector Right, float Weight, float Split, float LeftYaw=0,float RightYaw=0,float TravelYaw=0,float Bob=0,float Run=0,float ArmSwing=0,float LeftPitch=0,float RightPitch=0,FVector MotionLean=FVector::ZeroVector,float Brake=0);
    void SetHoldingBall(bool Value){bHoldingBall=Value;}
    bool bHoldingBall=false;
    void SetVolley(bool Value) { bRequestedVolley=Value; }
    bool bRequestedVolley=false;
    FVector RequestedFeet[2]={FVector(0,-23,6.7),FVector(0,23,6.7)};
    FVector RequestedMotionLean=FVector::ZeroVector;
    float RequestedBrake=0;
    float RequestedFootWeight=0,RequestedSplit=0,RequestedBob=0;
    float RequestedFootYaw[2]={0,0};
    float RequestedTravelYaw=0,RequestedRun=0,RequestedArmSwing=0,RequestedFootPitch[2]={0,0};
    const FTransform& GetRacketInHand() const { return RightGrip.RacketInHand; }

    /** Positive X is the athlete's right; positive Y is up. Values are clamped to
     * [-1,1]. Tracking loss fades the arm back to the authored pose. */
    UFUNCTION(BlueprintCallable, Category="CourtVision|Animation")
    void SetTrackedRacketTarget(FVector2D NormalizedWrist, float Confidence);

    UFUNCTION(BlueprintPure, Category="CourtVision|Animation")
    float GetSwingProgress() const { return ReportedSwingProgress; }
    float GetStrokeAnimationTime() const { return ReportedAnimationTime; }

    UFUNCTION(BlueprintPure, Category="CourtVision|Animation")
    bool IsSwingActive() const { return bReportedSwingActive; }

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
    friend struct FCVAthleteAnimProxy;

    // The instance owns strong references; proxy node pointers cannot outlive them.
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> ReadyClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> RunClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> ForehandClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> BackhandClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> ServeClip;

    CVGrip::FDefinition RightGrip, LeftGrip;
    FVector ServeContactPoint=FVector(5,30,240);
    FQuat RequestedRacketRotation = FQuat::Identity;
    float RequestedSpeed = 0.f;
    float RequestedPlayRate = 1.7f;
    float PreviousRemainingTime = 0.f;
    FVector RequestedContactTarget = FVector(40, 15, 100);
    FVector2D RequestedWrist = FVector2D::ZeroVector;
    float RequestedTrackingWeight = 0.f;
    bool bRequestedBackhand = false;
    bool bRequestedServe = false;
    uint32 SwingRequest = 0;
    uint32 ContactRequest = 0;
    bool bContactBackhand = false;
    uint32 ClipRevision = 0;

    float ReportedSwingProgress = 0.f;
    float ReportedAnimationTime = 0.f;
    bool bReportedSwingActive = false;
};

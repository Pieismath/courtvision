#include "CVAthleteAnim.h"
#include "CVStrokeMotion.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/BoneReference.h"
#include "AnimationRuntime.h"
#include "BonePose.h"

namespace
{
float ExpBlend(float Current, float Target, float DeltaSeconds, float Response)
{
    return FMath::Lerp(Current, Target, 1.f - FMath::Exp(-Response * DeltaSeconds));
}

float Smooth01(float Value)
{
    const float X = FMath::Clamp(Value, 0.f, 1.f);
    return X * X * (3.f - 2.f * X);
}

/** A small native graph node avoids an AnimGraphRuntime dependency. The Engine
 * blend utility carries bone transforms, curves and animation attributes. */
struct FCVBlendNode final : FAnimNode_Base
{
    FPoseLink A;
    FPoseLink B;
    float Alpha = 0.f;

    virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override
    {
        FAnimNode_Base::Initialize_AnyThread(Context);
        A.Initialize(Context);
        B.Initialize(Context);
    }

    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
    {
        A.CacheBones(Context);
        B.CacheBones(Context);
    }

    virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override
    {
        // Persistent players preserve gait phase; zero-weight inputs do not leak
        // into the output pose or generate visible transitions on activation.
        A.Update(Context.FractionalWeight(1.f - Alpha));
        B.Update(Context.FractionalWeight(Alpha));
    }

    virtual void Evaluate_AnyThread(FPoseContext& Output) override
    {
        if (Alpha <= ZERO_ANIMWEIGHT_THRESH)
        {
            A.Evaluate(Output);
            return;
        }
        if (Alpha >= 1.f - ZERO_ANIMWEIGHT_THRESH)
        {
            B.Evaluate(Output);
            return;
        }
        A.Evaluate(Output);
        FPoseContext Second(Output);
        B.Evaluate(Second);
        FAnimationPoseData Result(Output);
        const FAnimationPoseData Other(Second);
        FAnimationRuntime::BlendTwoPosesTogetherInPlace(Result, Other, 1.f - Alpha);
    }
};

/** Ground contacts are solved after stroke blending, so a swing cannot freeze
 * the legs or drag a planted shoe with the actor. */
struct FCVFootNode final : FAnimNode_Base
{
 FPoseLink Input; FBoneReference Hip[2],Knee[2],Foot[2],Pelvis;
 FVector Targets[2];FVector PelvisOffset=FVector::ZeroVector,PelvisBefore=FVector::ZeroVector;FQuat BodyDelta=FQuat::Identity;float Pitch[2]={0,0},Run=0,ArmSwing=0,Brake=0;FVector MotionLean=FVector::ZeroVector;float Yaw[2]={0,0},ContactHeight=115,TravelYaw=0;float Weight=0,Split=0,Bob=0,Speed=0,Phase=0,StrokeWeight=0;bool Back=false;
 FCVFootNode(){Pelvis.BoneName=TEXT("pelvis");for(int I=0;I<2;I++){
  const FString S=I?TEXT("_r"):TEXT("_l");Hip[I].BoneName=FName(TEXT("thigh")+S);Knee[I].BoneName=FName(TEXT("calf")+S);Foot[I].BoneName=FName(TEXT("foot")+S);
 }}
 void Initialize_AnyThread(const FAnimationInitializeContext& C) override {FAnimNode_Base::Initialize_AnyThread(C);Input.Initialize(C);}
 void CacheBones_AnyThread(const FAnimationCacheBonesContext& C) override {
  Input.CacheBones(C);const auto& B=C.AnimInstanceProxy->GetRequiredBones();Pelvis.Initialize(B);
  for(int I=0;I<2;I++){Hip[I].Initialize(B);Knee[I].Initialize(B);Foot[I].Initialize(B);}
 }
 void Update_AnyThread(const FAnimationUpdateContext& C) override {Input.Update(C);}
 void Evaluate_AnyThread(FPoseContext& O) override {
  Input.Evaluate(O);const auto& B=O.Pose.GetBoneContainer();if(Weight<.001f||!Pelvis.IsValidToEvaluate(B))return;
  FCSPose<FCompactPose> CS;CS.InitPose(O.Pose);
  auto P=Pelvis.GetCompactPoseIndex(B);FTransform Pel=CS.GetComponentSpaceTransform(P);
  auto Parent=CS.GetComponentSpaceTransform(O.Pose.GetParentBoneIndex(P));
  const float Load=CVStroke::Smooth(Phase/.3f)*(1-CVStroke::Smooth((Phase-.38f)/.24f))*StrokeWeight;
  const float Transfer=CVStroke::Smooth((Phase-.36f)/.22f)*(1-CVStroke::Smooth((Phase-.76f)/.24f))*StrokeWeight;
  float Crouch=FMath::Lerp(83.f,86.f,Run)-Load*5+Transfer*2-3.5f*Brake-FMath::Clamp((105-ContactHeight)*.22f,0.f,11.f)*CVStroke::ContactEnvelope(Phase)*StrokeWeight;
  PelvisBefore=Pel.GetLocation();
  BodyDelta=FQuat(FVector::UpVector,TravelYaw*FMath::Lerp(.9f,.35f,StrokeWeight)+(Back?-.5f*CVStroke::ContactEnvelope(Phase)*StrokeWeight:0.f));
  const FVector Lean=FQuat(FVector::UpVector,TravelYaw).RotateVector(FVector(3*Run,0,0))+MotionLean*(1-StrokeWeight*.7f);
  const FVector DesiredPelvis(PelvisBefore.X+Transfer*5+Lean.X,PelvisBefore.Y+(Back?-(Load*8+Transfer*7):Load*8-Transfer*5)+Lean.Y,PelvisBefore.Z);
  // Lower the pelvis only as needed to keep BOTH planted legs within reach.
  for(int I=0;I<2;I++)if(Hip[I].IsValidToEvaluate(B)&&Knee[I].IsValidToEvaluate(B)&&Foot[I].IsValidToEvaluate(B)){
   const FVector H0=CS.GetComponentSpaceTransform(Hip[I].GetCompactPoseIndex(B)).GetLocation();
   const FVector H=DesiredPelvis+BodyDelta.RotateVector(H0-PelvisBefore);
   const FVector K=CS.GetComponentSpaceTransform(Knee[I].GetCompactPoseIndex(B)).GetLocation();
   const FVector F=CS.GetComponentSpaceTransform(Foot[I].GetCompactPoseIndex(B)).GetLocation();
   const float Reach=FVector::Distance(H0,K)+FVector::Distance(K,F)-1.5f;
   const float Horizontal=FVector2D::Distance(FVector2D(H),FVector2D(Targets[I]));
   const float MaxHeight=Targets[I].Z+FMath::Sqrt(FMath::Max(400.f,Reach*Reach-Horizontal*Horizontal))-(H.Z-Pel.GetLocation().Z);
   Crouch=FMath::Min(Crouch,FMath::Max(61.f,MaxHeight-Bob));
  }
  Pel.SetRotation((BodyDelta*Pel.GetRotation()).GetNormalized());
  Pel.SetLocation(FVector(DesiredPelvis.X,DesiredPelvis.Y,FMath::Lerp(float(Pel.GetLocation().Z),Crouch+Split+Bob,Weight)));
  PelvisOffset=Pel.GetLocation()-CS.GetComponentSpaceTransform(P).GetLocation();
  O.Pose[P]=Pel.GetRelativeTransform(Parent);
  for(int I=0;I<2;I++){
   if(!Hip[I].IsValidToEvaluate(B)||!Knee[I].IsValidToEvaluate(B)||!Foot[I].IsValidToEvaluate(B))continue;
   FCSPose<FCompactPose> Legs;Legs.InitPose(O.Pose);
   auto U=Hip[I].GetCompactPoseIndex(B),L=Knee[I].GetCompactPoseIndex(B),F=Foot[I].GetCompactPoseIndex(B);
   FTransform H=Legs.GetComponentSpaceTransform(U),K=Legs.GetComponentSpaceTransform(L),A=Legs.GetComponentSpaceTransform(F);
   const auto HP=Legs.GetComponentSpaceTransform(O.Pose.GetParentBoneIndex(U));
   FVector OldK=K.GetLocation(),OldA=A.GetLocation(),Root=H.GetLocation();
   FVector T=FMath::Lerp(OldA,Targets[I]+FVector(0,0,FMath::Max(0.f,Split)),Weight);
   float L1=FVector::Distance(Root,OldK),L2=FVector::Distance(OldK,OldA);
   FVector D=(T-Root).GetSafeNormal();float Dist=FMath::Clamp(float((T-Root).Size()),FMath::Abs(L1-L2)+.1f,L1+L2-.1f);
   FVector Forward=FQuat(FVector::UpVector,Yaw[I]+FMath::FindDeltaAngleRadians(Yaw[I],TravelYaw)*.25f).RotateVector(FVector::ForwardVector);
   FVector Pole=Forward-D*FVector::DotProduct(D,Forward);Pole.Normalize();
   float Along=(L1*L1-L2*L2+Dist*Dist)/(2*Dist);
   FVector Joint=Root+D*Along+Pole*FMath::Sqrt(FMath::Max(0.f,L1*L1-Along*Along)),Ankle=Root+D*Dist;
   H.SetRotation((FQuat::FindBetweenVectors(OldK-Root,Joint-Root)*H.GetRotation()).GetNormalized());
   K.SetLocation(Joint);K.SetRotation((FQuat::FindBetweenVectors(OldA-OldK,Ankle-Joint)*K.GetRotation()).GetNormalized());
   // Reference foot orientation places the entire sole level on the court.
   FTransform Ref=B.GetRefPoseTransform(F);auto Anc=O.Pose.GetParentBoneIndex(F);
   while(Anc.IsValid()){Ref=Ref*B.GetRefPoseTransform(Anc);Anc=O.Pose.GetParentBoneIndex(Anc);}
   A.SetLocation(Ankle);A.SetRotation(FQuat::Slerp(A.GetRotation(),FQuat(FVector::UpVector,Yaw[I])*FQuat(FVector::RightVector,Pitch[I])*Ref.GetRotation(),Weight).GetNormalized());
   O.Pose[U]=H.GetRelativeTransform(HP);O.Pose[L]=K.GetRelativeTransform(H);O.Pose[F]=A.GetRelativeTransform(K);
  }
  O.Pose.NormalizeRotations();
 }
};

/** Solve both connected arm chains after all animation blending. The racket is
 * a rigid child of the dominant hand; the support hand follows its upper grip. */
struct FCVTrackedArmNode final : FAnimNode_Base
{
    FPoseLink Input;
    FBoneReference Upper, Lower, Hand, LeftUpper, LeftLower, LeftHand;
    FVector2D Target=FVector2D::ZeroVector;
    float Weight=0.f, ContactWeight=0.f, SupportWeight=0.f;
    FVector ContactTarget=FVector::ZeroVector;
    FVector ContactOffset=FVector::ZeroVector;
    uint32 ContactSerial=0,EvaluatedContact=0;
    bool Volley=false,Back=false,Serve=false,Planned=false,HoldingBall=false;float Progress=0,StrokeWeight=0,VolleyWeight=0;
    FVector AuthoredContact[2];
    FVector TorsoBase,TorsoUp,TorsoRight,TorsoForward;float TorsoHeight=45;
    FVector PreviousPole[2]={FVector::ZeroVector,FVector::ZeroVector};
    FVector FramePreviousPole[2],SolvedPole[2],FrameAuthoredUpper[2];
    FVector PreviousAxis[2]={FVector::ForwardVector,FVector::ForwardVector},FramePreviousAxis[2],SolvedAxis[2];float SolveDelta=1.f/60.f;
    const FCVFootNode* FootSource=nullptr;
    FQuat RacketRotation=FQuat::Identity;
    CVGrip::FDefinition RightGrip, LeftGrip;
    struct FFinger { FBoneReference Bone; FQuat Rotation; bool Left; };
    TArray<FFinger> Fingers;
    FCVTrackedArmNode()
    {
        Upper.BoneName=TEXT("upperarm_r"); Lower.BoneName=TEXT("lowerarm_r"); Hand.BoneName=TEXT("hand_r");
        LeftUpper.BoneName=TEXT("upperarm_l"); LeftLower.BoneName=TEXT("lowerarm_l"); LeftHand.BoneName=TEXT("hand_l");
    }
    virtual void Initialize_AnyThread(const FAnimationInitializeContext& C) override { FAnimNode_Base::Initialize_AnyThread(C);Input.Initialize(C); }
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& C) override
    {
        Input.CacheBones(C);const auto& B=C.AnimInstanceProxy->GetRequiredBones();
        for(auto* Ref:{&Upper,&Lower,&Hand,&LeftUpper,&LeftLower,&LeftHand})Ref->Initialize(B);
        Fingers.Reset();
        for(int Side=0;Side<2;++Side)for(const auto& Pair:(Side?LeftGrip:RightGrip).Fingers)
        { FFinger F;F.Bone.BoneName=Pair.Key;F.Bone.Initialize(B);F.Rotation=Pair.Value;F.Left=Side!=0;Fingers.Add(F); }
    }
    virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override {
        SolveDelta=Context.GetDeltaTime();
        for(int I=0;I<2;I++){FramePreviousPole[I]=PreviousPole[I];SolvedPole[I]=PreviousPole[I];FramePreviousAxis[I]=PreviousAxis[I];SolvedAxis[I]=PreviousAxis[I];}
        Input.Update(Context);
    }
    // Conservative shirt/arm envelope in the CURRENT rotated torso frame.
    // Clearance is independent of the racket socket and never rotates a wrist
    // to hide penetration. Both upper arm and forearm are tested, not just tips.
    float TorsoRadius(const FVector& P) const {
        const FVector V=P-TorsoBase;const float Z=FVector::DotProduct(V,TorsoUp);
        if(Z<7 || Z>TorsoHeight-2)return 2.f;
        const float X=FVector::DotProduct(V,TorsoForward),Y=FVector::DotProduct(V,TorsoRight);
        return FMath::Sqrt(FMath::Square(X/16.f)+FMath::Square(Y/22.f));
    }
    FVector ClearWrist(FVector P,int Side) const {
        const FVector V=P-TorsoBase;const float Z=FVector::DotProduct(V,TorsoUp);
        const float Vertical=Smooth01((Z-3)/8)*Smooth01((TorsoHeight+5-Z)/8);
        const float Y=FVector::DotProduct(V,TorsoRight),X=FVector::DotProduct(V,TorsoForward);
        // Across the chest, route the hand IN FRONT rather than allowing the
        // shortest shoulder-to-target line to cut through the body.
        const float Front=20.f*FMath::Sqrt(FMath::Max(0.f,1-FMath::Square(Y/28.f)));
        const float Across=Smooth01((-(Side? -Y:Y)-5)/25);
        const float SideBlend=FMath::Max(Across,1-Smooth01((FMath::Abs(Y)-24)/5));
        const float Delta=FMath::Max(0.f,FMath::Max(Front,Across*30.f)-X);
        return P+TorsoForward*Delta*Vertical*SideBlend;
    }
    float ArmClearanceCost(const FVector& A,const FVector& E,const FVector& W) const {
        float Cost=0;
        for(int J=1;J<=8;J++){
            const float T=J/8.f;
            const float R1=TorsoRadius(FMath::Lerp(A,E,T)),R2=TorsoRadius(FMath::Lerp(E,W,T));
            Cost+=(J>=3?FMath::Square(FMath::Max(0.f,1.04f-R1))+.045f*StrokeWeight*FMath::Square(FMath::Max(0.f,1.5f-R1)):0.f)
                +FMath::Square(FMath::Max(0.f,1.04f-R2))+.045f*StrokeWeight*FMath::Square(FMath::Max(0.f,1.5f-R2));
        }
        return Cost;
    }
    FTransform Solve(FPoseContext& Output,FBoneReference& UpperRef,FBoneReference& LowerRef,FBoneReference& HandRef,
        const FVector& TargetPosition,const FQuat& TargetRotation,float Alpha)
    {
        const auto& Bones=Output.Pose.GetBoneContainer();
        const auto U=UpperRef.GetCompactPoseIndex(Bones),L=LowerRef.GetCompactPoseIndex(Bones),H=HandRef.GetCompactPoseIndex(Bones);
        FCSPose<FCompactPose> CS;CS.InitPose(Output.Pose);
        const FTransform OU=CS.GetComponentSpaceTransform(U),OL=CS.GetComponentSpaceTransform(L),OH=CS.GetComponentSpaceTransform(H);
        const FTransform Parent=CS.GetComponentSpaceTransform(Output.Pose.GetParentBoneIndex(U));
        const FVector A=OU.GetLocation(),B=OL.GetLocation(),C=OH.GetLocation();
        const float L1=FVector::Distance(A,B),L2=FVector::Distance(B,C);
        const int Side=HandRef.BoneName==TEXT("hand_r")?0:1;
        FVector Desired=ClearWrist(FMath::Lerp(C,TargetPosition,Alpha),Side);
        const float Across=Smooth01((-(Side?-1.f:1.f)*FVector::DotProduct(Desired-TorsoBase,TorsoRight)-14.f)/14.f);
        const float ReachLimit=(L1+L2)*FMath::Lerp(.998f,.89f,Across);
        Desired=A+(Desired-A).GetClampedToMaxSize(ReachLimit);
        FVector D=(Desired-A).GetSafeNormal();
        const float RequestedDistance=FVector::Distance(A,Desired);
        // A tightly folded follow-through can pass close to the shoulder and
        // rotate the shoulder-wrist axis through a singularity. Bound that
        // axis only in the folded region, and retain a plausible elbow bend.
        // This preserves bone lengths and the hand socket, not a world racket.
        const float FoldWeight=Side==0&&!Serve?StrokeWeight:0.f;
        const float MinimumReach=(L1+L2)*.36f*FoldWeight;
        if(!FramePreviousAxis[Side].IsNearlyZero()){
            // Fade the angular bound by reach and action weight; a binary
            // distance threshold merely moved the snap to the threshold exit.
            const float FoldRate=FMath::Lerp(10.f,24.f,Smooth01((RequestedDistance-MinimumReach)/20.f));
            const float AxisRate=FMath::Lerp(60.f,FoldRate,FoldWeight);
            const float Angle=FMath::Acos(FMath::Clamp(float(FVector::DotProduct(FramePreviousAxis[Side],D)),-1.f,1.f));
            if(Angle>.001f){
                const float Fraction=FMath::Min(1.f,FMath::Max(0.f,SolveDelta)*AxisRate/Angle);
                D=FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(FramePreviousAxis[Side],D),Fraction).RotateVector(FramePreviousAxis[Side]).GetSafeNormal();
            }
        }
        const float Distance=FMath::Clamp(RequestedDistance,FMath::Max(FMath::Abs(L1-L2)+.1f,MinimumReach),L1+L2-.1f);
        // Choose the elbow plane that best supports the desired palm direction.
        // This moves rotation upstream into the shoulder/forearm instead of
        // bending a backwards-pointing forearm into a forced hand orientation.
        const FVector LocalDistal=HandRef.BoneName==TEXT("hand_r")?RightGrip.DistalAxis:LeftGrip.DistalAxis;
        const FQuat GoalRotation=FQuat::Slerp(OH.GetRotation(),TargetRotation,Alpha).GetNormalized();
        const FVector DesiredElbow=Desired-GoalRotation.RotateVector(LocalDistal)*L2;
        FVector AuthoredPole=FrameAuthoredUpper[Side];AuthoredPole-=D*FVector::DotProduct(AuthoredPole,D);AuthoredPole.Normalize();
        FVector Cue=DesiredElbow-A;Cue-=D*FVector::DotProduct(Cue,D);
        const float CueLength=Cue.Size();
        const FVector Transported=FQuat::FindBetweenNormals(FramePreviousAxis[Side],D).RotateVector(FramePreviousPole[Side]);
        FVector Pole=Transported-D*FVector::DotProduct(Transported,D);
        if(!Pole.Normalize())Pole=AuthoredPole;
        if(CueLength>.001f){
            Cue/=CueLength;
            // Near an arm-axis singularity, retain the previous elbow plane.
            // Bound rotation about the shoulder–wrist axis; repeated reach
            // passes in this evaluation all use the SAME previous-frame pole.
            const float Angle=FMath::Atan2(FVector::DotProduct(D,FVector::CrossProduct(Pole,Cue)),FVector::DotProduct(Pole,Cue));
            const float Limit=FMath::Max(0.f,SolveDelta)*8.f*Smooth01(CueLength/8.f);
            Pole=FQuat(D,FMath::Clamp(Angle,-Limit,Limit)).RotateVector(Pole).GetSafeNormal();
        }
        // Releasing the second hand must fade the elbow constraint as well
        // as the wrist target. A tiny grip weight must not fully rotate the arm.
        const float BlendAngle=FMath::Atan2(FVector::DotProduct(D,FVector::CrossProduct(AuthoredPole,Pole)),FVector::DotProduct(AuthoredPole,Pole));
        Pole=FQuat(D,BlendAngle*Alpha).RotateVector(AuthoredPole).GetSafeNormal();
        const float Along=(L1*L1-L2*L2+Distance*Distance)/(2.f*Distance);
        const float Radius=FMath::Sqrt(FMath::Max(0.f,L1*L1-Along*Along));
        const FVector Wrist=A+D*Distance;
        const FVector BasePole=Pole;
        // Search the continuous elbow circle for body clearance. A broad soft
        // envelope starts steering before visible mesh penetration. Temporal
        // preference keeps the solution on the same branch through recovery.
        float Best=MAX_flt,BestAngle=0;
        FVector Previous=Transported-D*FVector::DotProduct(Transported,D);
        if(!Previous.Normalize())Previous=Pole;
        auto Score=[&](float Angle){
            const FVector Candidate=FQuat(D,Angle).RotateVector(Previous);
            const FVector Elbow=A+D*Along+Candidate*Radius;
            const float ElbowHeight=FVector::DotProduct(Elbow-A,TorsoUp);
            const float WristHeight=FVector::DotProduct(Wrist-A,TorsoUp);
            const FVector Forearm=(Wrist-Elbow).GetSafeNormal();
            const float Bend=FVector::DotProduct(Forearm,GoalRotation.RotateVector(LocalDistal));
            return ArmClearanceCost(A,Elbow,Wrist)*180.f
                +18.f*FMath::Square(FMath::Max(0.f,.62f-Bend))
                +8.f*FMath::Square(FMath::Max(0.f,ElbowHeight-FMath::Max(0.f,WristHeight))/10.f)
                +FMath::Lerp(4.f,1.5f,Alpha)*(1-FVector::DotProduct(Candidate,AuthoredPole))
                +.12f*(1-FVector::DotProduct(Candidate,BasePole))
                +.25f*(1-FVector::DotProduct(Candidate,Previous));
        };
        const float MaxTurn=FramePreviousPole[Side].IsNearlyZero()?PI:FMath::Max(0.f,SolveDelta)*10.f;
        for(int J=-12;J<=12;J++){const float Angle=J*MaxTurn/12;const float Cost=Score(Angle);if(Cost<Best){Best=Cost;BestAngle=Angle;}}
        Pole=FQuat(D,BestAngle).RotateVector(Previous).GetSafeNormal();
        // Grip weight fades the wrist goal, not body clearance. Blending the
        // cleared elbow back to an intersecting source pose reintroduced torso
        // penetration during recovery. Solve the free arm too, so the continuous
        // elbow plane remains valid even when grip weight reaches zero.
        SolvedPole[Side]=Pole;SolvedAxis[Side]=D;
        const FVector E=A+D*Along+Pole*Radius;
        FTransform NU=OU,NL=OL,NH=OH;
        NU.SetRotation((FQuat::FindBetweenVectors(B-A,E-A)*OU.GetRotation()).GetNormalized());
        NL.SetLocation(E);NL.SetRotation((FQuat::FindBetweenVectors(C-B,Wrist-E)*OL.GetRotation()).GetNormalized());
        const FVector Axis=(Wrist-E).GetSafeNormal();
        const FVector Wanted=TargetRotation.RotateVector(LocalDistal).GetSafeNormal();
        FQuat Anatomical=TargetRotation;
        {
            const float Cos=FMath::Clamp(float(FVector::DotProduct(Axis,Wanted)),-1.f,1.f);
            const float Limit=FMath::DegreesToRadians(52.f);
            if(Cos<FMath::Cos(Limit)){
                FVector Tangent=Wanted-Axis*Cos;
                if(!Tangent.Normalize())Tangent=FVector::CrossProduct(Axis,Pole).GetSafeNormal();
                const FVector Within=Axis*FMath::Cos(Limit)+Tangent*FMath::Sin(Limit);
                Anatomical=(FQuat::FindBetweenNormals(Wanted,Within)*TargetRotation).GetNormalized();
            }
        }
        const FQuat RestHand=Bones.GetRefPoseTransform(H).GetRotation();
        const FQuat Neutral=(NL.GetRotation()*RestHand).GetNormalized();
        FQuat Delta=(Anatomical*Neutral.Inverse()).GetNormalized();if(Delta.W<0)Delta=Delta*-1.f;
        const FVector Q(Delta.X,Delta.Y,Delta.Z),V=Axis*FVector::DotProduct(Q,Axis);
        FQuat Twist(V.X,V.Y,V.Z,Delta.W);Twist.Normalize();
        NL.SetRotation((FQuat::Slerp(FQuat::Identity,Twist,Alpha)*NL.GetRotation()).GetNormalized());
        NH.SetLocation(Wrist);NH.SetRotation(FQuat::Slerp(OH.GetRotation(),Anatomical,Alpha).GetNormalized());

        Output.Pose[U]=NU.GetRelativeTransform(Parent);Output.Pose[L]=NL.GetRelativeTransform(NU);Output.Pose[H]=NH.GetRelativeTransform(NL);
        return NH;
    }
    virtual void Evaluate_AnyThread(FPoseContext& Output) override
    {
        Input.Evaluate(Output);const auto& Bones=Output.Pose.GetBoneContainer();
        if(Fingers.IsEmpty())for(int Side=0;Side<2;++Side)for(const auto& Pair:(Side?LeftGrip:RightGrip).Fingers)
        { FFinger F;F.Bone.BoneName=Pair.Key;F.Bone.Initialize(Bones);F.Rotation=Pair.Value;F.Left=Side!=0;Fingers.Add(F); }
        for(auto* Ref:{&Upper,&Lower,&Hand,&LeftUpper,&LeftLower,&LeftHand})if(!Ref->IsValidToEvaluate(Bones))return;
        FCSPose<FCompactPose> CS;CS.InitPose(Output.Pose);
        const FTransform Original=CS.GetComponentSpaceTransform(Hand.GetCompactPoseIndex(Bones));
        const FVector A=CS.GetComponentSpaceTransform(Upper.GetCompactPoseIndex(Bones)).GetLocation();
        const FVector Other=CS.GetComponentSpaceTransform(LeftUpper.GetCompactPoseIndex(Bones)).GetLocation();
        const FCompactPoseBoneIndex PelvisIndex=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetReferenceSkeleton().FindBoneIndex(TEXT("pelvis"))));
        TorsoBase=CS.GetComponentSpaceTransform(PelvisIndex).GetLocation();
        const FVector Chest=(A+Other)*.5f;
        TorsoUp=(Chest-TorsoBase).GetSafeNormal();TorsoRight=(A-Other).GetSafeNormal();
        TorsoForward=FVector::CrossProduct(TorsoRight,TorsoUp).GetSafeNormal();
        TorsoHeight=FVector::Distance(Chest,TorsoBase);
        FrameAuthoredUpper[0]=CS.GetComponentSpaceTransform(Lower.GetCompactPoseIndex(Bones)).GetLocation()-A;
        FrameAuthoredUpper[1]=CS.GetComponentSpaceTransform(LeftLower.GetCompactPoseIndex(Bones)).GetLocation()-Other;
        const FVector Offset=(A-Other).GetSafeNormal()*Target.X*32.f+FVector::UpVector*Target.Y*26.f;
        FVector Authored=Original.GetLocation()+Offset*Weight;
        const float ArmWeight=FMath::Max(StrokeWeight,VolleyWeight);
        const float Carry=FootSource&&!Serve&&!HoldingBall?FootSource->Run*(1-ArmWeight):0.f;
        const float Stride=FootSource?FootSource->ArmSwing:0.f;
        const FVector CarryWrist=Chest+TorsoForward*(27+Stride*4)+TorsoRight*23-TorsoUp*17;
        Authored=FMath::Lerp(Authored,CarryWrist,Carry);
        // The compact volley target has its own action envelope. Its body clip is
        // deliberately only 18%, but the wrist must enter/exit continuously.
        if(VolleyWeight>.001f){const float P=Progress;Authored=FMath::Lerp(Authored,FVector(28+18*Smooth01(P/.65f),Back?-26:26,118-8*Smooth01(P)),VolleyWeight);}
        FQuat Rotation=Serve?RacketRotation:CVStroke::Racket(Progress,Back,Volley,Planned?ContactTarget.Z:115);
        if(!Serve && ArmWeight<1.f)Rotation=FQuat::Slerp(CVStroke::Racket(0,false,false),Rotation,ArmWeight);
        if(FootSource&&Carry>0)Rotation=FQuat::Slerp(Rotation,FootSource->BodyDelta*CVStroke::Racket(0,false,false),Carry).GetNormalized();
        const FQuat HandRotation=(Rotation*RightGrip.RacketInHand.GetRotation().Inverse()).GetNormalized();
        const FQuat HitRotation=CVStroke::Racket(.52f,Back,Volley,ContactTarget.Z);
        const FQuat HitHand=(HitRotation*RightGrip.RacketInHand.GetRotation().Inverse()).GetNormalized();
        const FVector HitWrist=ContactTarget-HitRotation.RotateVector(FVector(0,0,47.5f))-HitHand.RotateVector(RightGrip.RacketInHand.GetLocation()*Original.GetScale3D());
        // The correction is planned during preparation and is continuous through
        // contact. A hit event no longer invents a new offset or seeks the pose.
        FVector SourceContact=AuthoredContact[Back?1:0];
        if(FootSource)SourceContact=FootSource->PelvisBefore+FootSource->BodyDelta.RotateVector(SourceContact-FootSource->PelvisBefore)+FootSource->PelvisOffset;
        ContactOffset=(HitWrist-SourceContact).GetClampedToMaxSize(50.f);
        const FVector Desired=Authored+ContactOffset*(Planned&&!Serve?ContactWeight:0.f);
        FTransform RH=Solve(Output,Upper,Lower,Hand,Desired,HandRotation,1.f);
        FTransform Racket=RightGrip.RacketInHand*RH;
        FTransform LH=LeftGrip.RacketInHand.Inverse()*Racket;
        if(SupportWeight>.01f)
        {
            const FVector Shoulder=CS.GetComponentSpaceTransform(LeftUpper.GetCompactPoseIndex(Bones)).GetLocation();
            const FVector Elbow=CS.GetComponentSpaceTransform(LeftLower.GetCompactPoseIndex(Bones)).GetLocation();
            const FVector Wrist=CS.GetComponentSpaceTransform(LeftHand.GetCompactPoseIndex(Bones)).GetLocation();
            const float Reach=FVector::Distance(Shoulder,Elbow)+FVector::Distance(Elbow,Wrist)-.2f;
            for(int Pass=0;Pass<4;++Pass)
            {
                const FVector FromShoulder=LH.GetLocation()-Shoulder;
                if(FromShoulder.Size()<=Reach)break;
                const FVector Correction=FromShoulder.GetSafeNormal()*Reach-FromShoulder;
                RH=Solve(Output,Upper,Lower,Hand,RH.GetLocation()+Correction*SupportWeight,HandRotation,1.f);
                Racket=RightGrip.RacketInHand*RH;LH=LeftGrip.RacketInHand.Inverse()*Racket;
            }
        }
        if(Carry>.001f){
            const FTransform Free=CS.GetComponentSpaceTransform(LeftHand.GetCompactPoseIndex(Bones));
            const FVector FreeWrist=Chest-TorsoRight*26-TorsoUp*24+TorsoForward*(14-Stride*18);
            Solve(Output,LeftUpper,LeftLower,LeftHand,FMath::Lerp(Free.GetLocation(),FreeWrist,Carry),(FQuat::FindBetweenNormals(Free.GetRotation().RotateVector(LeftGrip.DistalAxis),(TorsoForward*.9f+TorsoUp*.3f).GetSafeNormal())*Free.GetRotation()).GetNormalized(),1.f);
        }
        if(SupportWeight>ZERO_ANIMWEIGHT_THRESH || Carry<=.001f){
            FTransform Solved=Solve(Output,LeftUpper,LeftLower,LeftHand,LH.GetLocation(),LH.GetRotation(),SupportWeight);
            // Anatomical wrist limiting changes the handle-offset direction.
            // Re-solve the chain to the SAME upper-grip point after that limit;
            // never slide the hand bone or detach the racket to hide the gap.
            if(SupportWeight>.995f)for(int Pass=0;Pass<3;Pass++){
                const FVector Error=Racket.GetLocation()-(LeftGrip.RacketInHand*Solved).GetLocation();
                if(Error.SizeSquared()<.04f)break;
                Solved=Solve(Output,LeftUpper,LeftLower,LeftHand,Solved.GetLocation()+Error,LH.GetRotation(),1.f);
            }
        }
        for(int I=0;I<2;I++){PreviousPole[I]=SolvedPole[I];PreviousAxis[I]=SolvedAxis[I];}
        for(const auto& Finger:Fingers)if(Finger.Bone.IsValidToEvaluate(Bones))
        {
            auto& Pose=Output.Pose[Finger.Bone.GetCompactPoseIndex(Bones)];
            Pose.SetRotation(FQuat::Slerp(Pose.GetRotation(),Finger.Rotation,Finger.Left?FMath::Max(.18f,SupportWeight):1.f).GetNormalized());
        }
        Output.Pose.NormalizeRotations();
    }
};
}

struct FCVAthleteAnimProxy final : FAnimInstanceProxy
{
    FAnimNode_SequencePlayer_Standalone Ready;
    FAnimNode_SequencePlayer_Standalone Run;
    FAnimNode_SequencePlayer_Standalone Forehand;
    FAnimNode_SequencePlayer_Standalone Backhand;
    FAnimNode_SequencePlayer_Standalone Serve;
    FCVBlendNode Locomotion;
    FCVBlendNode StrokeChoice;
    FCVBlendNode ServeChoice;
    FCVBlendNode FullBody;
    FCVTrackedArmNode TrackedArm;
    FCVFootNode Footwork;

    uint32 SeenClipRevision = MAX_uint32;
    uint32 SeenSwingRequest = 0;
    uint32 SeenContactRequest = 0;
    bool bContactAligned = false;
    float Speed = 0.f;
    float StrokeRate = 1.7f;
    float StrokeElapsed = 0.f;
    float StrokeDuration = 0.f;
    float NormalizedStroke = 0.f;
    float DesiredTrackingWeight = 0.f;
    float SmoothedTrackingWeight = 0.f;
    FVector2D DesiredWrist = FVector2D::ZeroVector;
    float TargetStrokeChoice = 0.f;
    bool bStrokeActive = false;

    explicit FCVAthleteAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
    {
        Locomotion.A.SetLinkNode(&Ready);
        Locomotion.B.SetLinkNode(&Run);
        StrokeChoice.A.SetLinkNode(&Forehand);
        StrokeChoice.B.SetLinkNode(&Backhand);
        FullBody.A.SetLinkNode(&Locomotion);
        ServeChoice.A.SetLinkNode(&StrokeChoice);
        ServeChoice.B.SetLinkNode(&Serve);
        FullBody.B.SetLinkNode(&ServeChoice);
        Footwork.Input.SetLinkNode(&FullBody);
        TrackedArm.FootSource=&Footwork;
        TrackedArm.Input.SetLinkNode(&Footwork);
        Ready.SetLoopAnimation(true);
        Run.SetLoopAnimation(true);
        Forehand.SetLoopAnimation(false);
        Backhand.SetLoopAnimation(false);
        Serve.SetLoopAnimation(false);
    }

    virtual FAnimNode_Base* GetCustomRootNode() override { return &TrackedArm; }

    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override
    {
        OutNodes.Append({&Ready, &Run, &Forehand, &Backhand, &Serve,
                         &Locomotion, &StrokeChoice, &ServeChoice, &FullBody, &Footwork, &TrackedArm});
    }

    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        const UCVAthleteAnimInstance* Owner = CastChecked<UCVAthleteAnimInstance>(Instance);
        // UObject/gameplay access is confined to this game-thread handoff.
        if (SeenClipRevision != Owner->ClipRevision)
        {
            Ready.SetSequence(Owner->ReadyClip);
            Run.SetSequence(Owner->RunClip ? Owner->RunClip.Get() : Owner->ReadyClip.Get());
            Forehand.SetSequence(Owner->ForehandClip ? Owner->ForehandClip.Get() : Owner->ReadyClip.Get());
            Backhand.SetSequence(Owner->BackhandClip ? Owner->BackhandClip.Get() : Owner->ForehandClip.Get());
            Serve.SetSequence(Owner->ServeClip ? Owner->ServeClip.Get() : Owner->ForehandClip.Get());
            TrackedArm.RightGrip=Owner->RightGrip;TrackedArm.LeftGrip=Owner->LeftGrip;
            TrackedArm.AuthoredContact[0]=Owner->AuthoredContact[0];TrackedArm.AuthoredContact[1]=Owner->AuthoredContact[1];
            SeenClipRevision = Owner->ClipRevision;
        }
        Speed = Owner->RequestedSpeed;
        Footwork.Targets[0]=Owner->RequestedFeet[0];Footwork.Targets[1]=Owner->RequestedFeet[1];
        Footwork.Yaw[0]=Owner->RequestedFootYaw[0];Footwork.Yaw[1]=Owner->RequestedFootYaw[1];
        Footwork.Run=Owner->RequestedRun;Footwork.ArmSwing=Owner->RequestedArmSwing;Footwork.Brake=Owner->RequestedBrake;Footwork.MotionLean=Owner->RequestedMotionLean;Footwork.Pitch[0]=Owner->RequestedFootPitch[0];Footwork.Pitch[1]=Owner->RequestedFootPitch[1];Footwork.TravelYaw=Owner->RequestedTravelYaw;Footwork.Weight=Owner->RequestedFootWeight;Footwork.Split=Owner->RequestedSplit;Footwork.Bob=Owner->RequestedBob;Footwork.Speed=Speed;
        TrackedArm.HoldingBall=Owner->bHoldingBall;
        TrackedArm.Volley=Owner->bRequestedVolley && Owner->PreviousRemainingTime>0;TrackedArm.Back=Owner->bRequestedBackhand;
        DesiredWrist = Owner->RequestedWrist;
        TrackedArm.ContactTarget = Owner->RequestedContactTarget;Footwork.ContactHeight=Owner->RequestedContactTarget.Z;
        TrackedArm.RacketRotation = Owner->RequestedRacketRotation;
        TrackedArm.Planned=Owner->bPlannedContact;TrackedArm.Serve=Owner->bRequestedServe && Owner->PreviousRemainingTime>0;
        DesiredTrackingWeight = Owner->RequestedTrackingWeight;
        if (SeenSwingRequest != Owner->SwingRequest)
        {
            SeenSwingRequest = Owner->SwingRequest;
            bContactAligned = false;
            const UAnimSequence* Clip = Owner->bRequestedServe ? Owner->ServeClip.Get() : Owner->bRequestedBackhand ? Owner->BackhandClip.Get() : Owner->ForehandClip.Get();
            if (!Clip) return;
            StrokeRate = Owner->bRequestedServe ? 1.f : Owner->RequestedPlayRate;
            ServeChoice.Alpha = Owner->bRequestedServe ? 1.f : 0.f;
            StrokeDuration = Clip->GetPlayLength() / StrokeRate;
            StrokeElapsed = 0.f;
            NormalizedStroke = 0.f;
            bStrokeActive = StrokeDuration > SMALL_NUMBER;
            TargetStrokeChoice = Owner->bRequestedBackhand ? 1.f : 0.f;
            // When returning from idle there is no old stroke to crossfade.
            if (FullBody.Alpha < .025f) StrokeChoice.Alpha = TargetStrokeChoice;
            FAnimNode_SequencePlayer_Standalone& Player = Owner->bRequestedServe ? Serve : Owner->bRequestedBackhand ? Backhand : Forehand;
            Player.SetAccumulatedTime(0.f);
            Player.SetPlayRate(StrokeRate);
        }
        // Contact is an event on the existing stroke timeline. It must never
        // rewind the animation, switch clips, or force a full-body pose weight.
        if(SeenContactRequest!=Owner->ContactRequest){
            SeenContactRequest=Owner->ContactRequest;bContactAligned=true;
        }
    }

    virtual void Update(float DeltaSeconds) override
    {
        const float Dt = FMath::Max(0.f, DeltaSeconds);
        const float MovementWeight = Smooth01((Speed - 12.f) / 210.f);
        // The foot planner already supplies tennis locomotion and pelvis motion.
        // Keep the ready upper body instead of adding a generic running arm pump.
        Locomotion.Alpha = 0.f;
        Run.SetPlayRate(FMath::Clamp(Speed / 310.f, .65f, 1.9f));
        Ready.SetPlayRate(1.f);
        StrokeChoice.Alpha = ExpBlend(StrokeChoice.Alpha, TargetStrokeChoice, Dt, 24.f);

        float StrokeWeight = 0.f;
        if (bStrokeActive)
        {
            StrokeElapsed += Dt;
            NormalizedStroke = FMath::Clamp(StrokeElapsed / StrokeDuration, 0.f, 1.f);
            // Fast entry preserves responsiveness. Smooth recovery starts near
            // the final follow-through, before the source clip can snap to idle.
            const float BlendIn = Smooth01(StrokeElapsed / .16f);
            const float BlendOut = Smooth01((StrokeDuration - StrokeElapsed) / .20f);
            StrokeWeight = FMath::Min(BlendIn, BlendOut);
            if (StrokeElapsed >= StrokeDuration) bStrokeActive = false;
        }
        // Do not reset the outgoing body weight on a repeated stroke request.
        FullBody.Alpha = ExpBlend(FullBody.Alpha, TrackedArm.Volley?StrokeWeight*.18f:StrokeWeight, Dt, 32.f);
        TrackedArm.Progress=bStrokeActive?NormalizedStroke:1.f;
        TrackedArm.StrokeWeight=FullBody.Alpha;
        TrackedArm.VolleyWeight=ExpBlend(TrackedArm.VolleyWeight,TrackedArm.Volley?StrokeWeight:0.f,Dt,32.f);
        Footwork.Phase=NormalizedStroke;Footwork.StrokeWeight=FullBody.Alpha*(1-ServeChoice.Alpha);Footwork.Back=TargetStrokeChoice>.5f;
        SmoothedTrackingWeight = ExpBlend(SmoothedTrackingWeight, DesiredTrackingWeight, Dt, 14.f);
        // The non-dominant hand supports the upper grip in ready/recovery.
        // Release it for a forehand, volley or toss; a backhand keeps both hands
        // attached continuously instead of snapping between unrelated poses.
        const bool OneHanded = TargetStrokeChoice < .5f || TrackedArm.Volley || TrackedArm.Serve;
        const float GripWeight = TrackedArm.HoldingBall ? 0.f : bStrokeActive && OneHanded ? 1.f-Smooth01(StrokeElapsed/.16f)*(1.f-Smooth01((NormalizedStroke-.80f)/.20f)) : 1.f;
        TrackedArm.SupportWeight=ExpBlend(TrackedArm.SupportWeight,GripWeight*(1-Footwork.Run*(1-FullBody.Alpha)),Dt,28.f);
        TrackedArm.Target.X = ExpBlend(TrackedArm.Target.X, DesiredWrist.X, Dt, 22.f);
        TrackedArm.Target.Y = ExpBlend(TrackedArm.Target.Y, DesiredWrist.Y, Dt, 22.f);
        TrackedArm.Weight = SmoothedTrackingWeight * (1.f - FullBody.Alpha);
        TrackedArm.ContactWeight = bStrokeActive ? CVStroke::ContactEnvelope(NormalizedStroke) : 0.f;
    }

    virtual void PostUpdate(UAnimInstance* Instance) const override
    {
        FAnimInstanceProxy::PostUpdate(Instance);
        UCVAthleteAnimInstance* Owner = CastChecked<UCVAthleteAnimInstance>(Instance);
        Owner->ReportedSwingProgress = NormalizedStroke;
        Owner->ReportedAnimationTime = ServeChoice.Alpha>.5f ? Serve.GetAccumulatedTime() : TargetStrokeChoice>.5f ? Backhand.GetAccumulatedTime() : Forehand.GetAccumulatedTime();
        Owner->bReportedSwingActive = bStrokeActive;
    }
};

void UCVAthleteAnimInstance::ConfigureClips(UAnimSequence* Ready, UAnimSequence* Run,
    UAnimSequence* Forehand, UAnimSequence* Backhand, UAnimSequence* Serve)
{
    check(IsInGameThread());
    ReadyClip = Ready;
    RunClip = Run;
    ForehandClip = Forehand;
    BackhandClip = Backhand;
    ServeClip = Serve;
    ++ClipRevision;
}

void UCVAthleteAnimInstance::SetTennisState(float MovementSpeedCmS, float RemainingSwingSeconds,
    bool bBackhand, float SwingPlayRate, bool bServe)
{
    check(IsInGameThread());
    RequestedSpeed = FMath::IsFinite(MovementSpeedCmS) ? FMath::Clamp(FMath::Abs(MovementSpeedCmS), 0.f, 1800.f) : 0.f;
    const float Remaining = FMath::IsFinite(RemainingSwingSeconds) ? FMath::Max(RemainingSwingSeconds, 0.f) : 0.f;
    if (Remaining > 0.f && (PreviousRemainingTime <= 0.f || Remaining > PreviousRemainingTime + .035f))
        TriggerSwing(bBackhand, SwingPlayRate, bServe);
    PreviousRemainingTime = Remaining;
}

void UCVAthleteAnimInstance::TriggerSwing(bool bBackhand, float SwingPlayRate, bool bServe)
{
    check(IsInGameThread());
    bRequestedBackhand = bBackhand;
    bRequestedServe = bServe;
    RequestedPlayRate = FMath::IsFinite(SwingPlayRate) ? FMath::Clamp(SwingPlayRate, .5f, 3.f) : 1.7f;
    ++SwingRequest;
}

void UCVAthleteAnimInstance::ConfigureGrip(const USkeletalMesh* Mesh)
{
    RightGrip=CVGrip::Build(Mesh,true);LeftGrip=CVGrip::Build(Mesh,false);++ClipRevision;
    if(Mesh && ServeClip)
    {
        // Sample the imported pose, including FBX bone scale, rather than assuming
        // a wrist height from the Blender target that was reach-clamped on export.
        FMemMark Mark(FMemStack::Get());
        TArray<FBoneIndexType> Required;
        for(int I=0;I<Mesh->GetRefSkeleton().GetNum();++I)Required.Add(I);
        FBoneContainer Bones;Bones.InitializeTo(Required,UE::Anim::FCurveFilterSettings(),*Mesh);
        FCompactPose Pose;Pose.SetBoneContainer(&Bones);Pose.ResetToRefPose();
        FBlendedCurve Curve;Curve.InitFrom(Bones);
        UE::Anim::FStackAttributeContainer Attributes;
        FAnimationPoseData Data(Pose,Curve,Attributes);
        for(int Shot=0;Shot<2;Shot++){
            auto* Clip=Shot?BackhandClip.Get():ForehandClip.Get();
            Pose.ResetToRefPose();Clip->GetAnimationPose(Data,FAnimExtractContext(Clip->GetPlayLength()*.52));
            FCSPose<FCompactPose> CP;CP.InitPose(Pose);
            AuthoredContact[Shot]=CP.GetComponentSpaceTransform(Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh->GetRefSkeleton().FindBoneIndex(TEXT("hand_r"))))).GetLocation();
        }
        Pose.ResetToRefPose();ServeClip->GetAnimationPose(Data,FAnimExtractContext(.55));
        FCSPose<FCompactPose> CS;CS.InitPose(Pose);
        FTransform Hand=CS.GetComponentSpaceTransform(Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh->GetRefSkeleton().FindBoneIndex(TEXT("hand_r")))));
        Hand.SetRotation(FRotator(-18,0,0).Quaternion()*RightGrip.RacketInHand.GetRotation().Inverse());
        ServeContactPoint=(RightGrip.RacketInHand*Hand).TransformPosition(FVector(0,0,47.5f));
    }
}
void UCVAthleteAnimInstance::SetRacketOrientation(FQuat ComponentRotation)
{
    if(!ComponentRotation.ContainsNaN())RequestedRacketRotation=ComponentRotation.GetNormalized();
}

void UCVAthleteAnimInstance::SetContactTarget(FVector ComponentSpaceWrist)
{
    check(IsInGameThread());
    if (!ComponentSpaceWrist.ContainsNaN()) RequestedContactTarget = ComponentSpaceWrist.GetClampedToMaxSize(300.f);
}

void UCVAthleteAnimInstance::PrepareContact(FVector ComponentSpaceHead){
    SetContactTarget(ComponentSpaceHead);bPlannedContact=true;
}
void UCVAthleteAnimInstance::NotifyRacketContact(bool bBackhand)
{
    check(IsInGameThread());
    bContactBackhand = bBackhand;
    ++ContactRequest;
}

void UCVAthleteAnimInstance::SetTrackedRacketTarget(FVector2D NormalizedWrist, float Confidence)
{
    check(IsInGameThread());
    RequestedWrist.X = FMath::IsFinite(NormalizedWrist.X) ? FMath::Clamp(NormalizedWrist.X, -1., 1.) : 0.;
    RequestedWrist.Y = FMath::IsFinite(NormalizedWrist.Y) ? FMath::Clamp(NormalizedWrist.Y, -1., 1.) : 0.;
    RequestedTrackingWeight = FMath::IsFinite(Confidence) ? FMath::Clamp(Confidence, 0.f, 1.f) : 0.f;
}

FAnimInstanceProxy* UCVAthleteAnimInstance::CreateAnimInstanceProxy()
{
    return new FCVAthleteAnimProxy(this);
}

void UCVAthleteAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
    delete static_cast<FCVAthleteAnimProxy*>(InProxy);
}

void UCVAthleteAnimInstance::SetFootPlacement(FVector Left,FVector Right,float Weight,float Split,float LeftYaw,float RightYaw,float TravelYaw,float Bob,float Run,float ArmSwing,float LeftPitch,float RightPitch,FVector MotionLean,float Brake){
 RequestedMotionLean=MotionLean;RequestedBrake=Brake;RequestedRun=Run;RequestedArmSwing=ArmSwing;RequestedFootPitch[0]=LeftPitch;RequestedFootPitch[1]=RightPitch;RequestedTravelYaw=TravelYaw;RequestedBob=Bob;
 RequestedFootYaw[0]=LeftYaw;RequestedFootYaw[1]=RightYaw;
 RequestedFeet[0]=Left;RequestedFeet[1]=Right;RequestedFootWeight=Weight;RequestedSplit=Split;
}

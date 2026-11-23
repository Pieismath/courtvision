#pragma once
#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"

/** Grip coordinates are derived from the imported anatomy, in centimetres.
 * The racket's origin is its butt and its long axis is +Z. */
namespace CVGrip
{
struct FDefinition
{
    FTransform RacketInHand;
    FVector DistalAxis=FVector::ForwardVector;
    TMap<FName,FQuat> Fingers;
};
inline FDefinition Build(const USkeletalMesh* Mesh, bool Right)
{
    FDefinition Result;
    if (!Mesh) return Result;
    const auto& Ref=Mesh->GetRefSkeleton();
    TArray<FTransform> CS=Ref.GetRefBonePose();
    for(int I=0;I<CS.Num();++I)if(Ref.GetParentIndex(I)>=0)CS[I]=CS[I]*CS[Ref.GetParentIndex(I)];
    const FString Side=Right?TEXT("r"):TEXT("l");
    auto Index=[&](const FString& Name){return Ref.FindBoneIndex(FName(Name+TEXT("_")+Side));};
    const int H=Index(TEXT("hand"));
    if(H<0)return Result;
    auto Point=[&](const TCHAR* Name){return CS[Index(Name)].GetLocation();};
    const FVector Across=(Point(TEXT("index_01"))-Point(TEXT("pinky_01"))).GetSafeNormal();
    FVector Distal=(Point(TEXT("middle_01"))-CS[H].GetLocation()).GetSafeNormal();
    Distal=(Distal-Across*FVector::DotProduct(Distal,Across)).GetSafeNormal();
    // FBX's Blender -> Unreal conversion changes handedness. Both palm normals
    // point inward in the reference pose, not through the back of the hand.
    const FVector Palm=FVector::CrossProduct(Distal,Across)*(Right?1.f:-1.f);
    // The handle crosses the palm diagonally. A pure knuckle-width axis
    // forced an almost right-angle wrist bend on overhead strokes.
    const FVector Axis=(Across*.766044443f+Distal*.642787610f).GetSafeNormal();
    Result.DistalAxis=CS[H].InverseTransformVectorNoScale(Distal).GetSafeNormal();
    const FVector Center=(Point(TEXT("index_01"))+Point(TEXT("pinky_01")))*.5f+Palm*2.5f;
    const FQuat Rotation=FRotationMatrix::MakeFromZY(Axis,Distal).ToQuat();
    Result.RacketInHand=FTransform(Rotation,Center-Axis*(Right?5.f:13.f)).GetRelativeTransform(CS[H]);
    TArray<FTransform> Bent=CS;
    for(const TCHAR* Digit:{TEXT("index"),TEXT("middle"),TEXT("ring"),TEXT("pinky")})
    {
        for(int Segment=1;Segment<=3;++Segment)
        {
            const int B=Index(FString::Printf(TEXT("%s_%02d"),Digit,Segment));
            const int Parent=Ref.GetParentIndex(B);
            const FTransform Current=Ref.GetRefBonePose()[B]*Bent[Parent];
            const FVector Start=Current.GetLocation();
            // Read each digit direction from its reference child joint.
            FVector OriginalDirection;
            float Length=2.5f;
            if(Segment<3){const int Next=Index(FString::Printf(TEXT("%s_%02d"),Digit,Segment+1));OriginalDirection=CS[Next].GetLocation()-CS[B].GetLocation();Length=OriginalDirection.Size();}
            else {OriginalDirection=CS[B].GetLocation()-CS[Parent].GetLocation();Length=OriginalDirection.Size();}
            const FVector RefLocalDirection=CS[B].InverseTransformVectorNoScale(OriginalDirection.GetSafeNormal());
            FVector Radial=Start-Center;Radial-=Axis*FVector::DotProduct(Radial,Axis);
            const float Radius=FMath::Max(2.3f,Radial.Size());
            Radial=Radial.GetSafeNormal()*Radius;
            const float Angle=2.f*FMath::Asin(FMath::Clamp(Length/(2.f*Radius),0.f,.97f));
            // Curl toward the palm, around the octagonal handle, not a fixed
            // guessed local axis shared by every finger.
            const float Sign=Right?-1.f:1.f;
            const FVector NextRadial=FQuat(Axis,Angle*Sign).RotateVector(Radial);
            const FVector Direction=(NextRadial-Radial).GetSafeNormal();
            const FQuat Delta=FQuat::FindBetweenNormals(Current.TransformVectorNoScale(RefLocalDirection).GetSafeNormal(),Direction);
            Bent[B]=Current;Bent[B].SetRotation((Delta*Current.GetRotation()).GetNormalized());
            Result.Fingers.Add(Ref.GetBoneName(B),Bent[B].GetRelativeTransform(Bent[Parent]).GetRotation());
        }
    }
    // Oppose the thumb across the near side of the handle.
    for(int Segment=1;Segment<=3;++Segment)
    {
        const int B=Index(FString::Printf(TEXT("thumb_%02d"),Segment));const int Parent=Ref.GetParentIndex(B);
        const FTransform Current=Ref.GetRefBonePose()[B]*Bent[Parent];
        const int Next=Index(FString::Printf(TEXT("thumb_%02d"),FMath::Min(3,Segment+1)));
        const FVector RefDir=Segment<3?CS[Next].GetLocation()-CS[B].GetLocation():CS[B].GetLocation()-CS[Parent].GetLocation();
        const FVector Target=Center+Axis*(3.f-Segment*1.7f)-Distal*1.8f+Palm*(Segment==1?-.8f:1.f);
        const FQuat Delta=FQuat::FindBetweenNormals(Current.TransformVectorNoScale(CS[B].InverseTransformVectorNoScale(RefDir.GetSafeNormal())).GetSafeNormal(),(Target-Current.GetLocation()).GetSafeNormal());
        Bent[B]=Current;Bent[B].SetRotation((Delta*Current.GetRotation()).GetNormalized());
        Result.Fingers.Add(Ref.GetBoneName(B),Bent[B].GetRelativeTransform(Bent[Parent]).GetRotation());
    }
    return Result;
}
}

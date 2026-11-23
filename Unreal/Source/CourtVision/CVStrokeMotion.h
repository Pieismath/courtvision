#pragma once
#include "CoreMinimal.h"
// One phase drives the body clip, grip orientation, contact adjustment and load.
// Components use X toward the net, Y to the athlete's right, Z up (centimetres).
namespace CVStroke {
constexpr float Rate=.8f;
constexpr float SourceDuration=.883333333f;
constexpr float Duration=SourceDuration/Rate;
constexpr float ContactPhase=.52f;
constexpr float Preparation=Duration*ContactPhase;
inline float Smooth(float X){X=FMath::Clamp(X,0.f,1.f);return X*X*(3-2*X);}
inline FVector Curve(const float* Times,const FVector* P,int N,float T){
 for(int I=0;I<N-1;I++)if(T<=Times[I+1]){
  const int L=FMath::Max(0,I-1),R=FMath::Min(N-1,I+2);float H=Times[I+1]-Times[I],U=FMath::Clamp((T-Times[I])/H,0.f,1.f);
  FVector M0=I?(P[I+1]-P[L])/(Times[I+1]-Times[L]):FVector::ZeroVector;
  FVector M1=I+1<N-1?(P[R]-P[I])/(Times[R]-Times[I]):FVector::ZeroVector;
  return P[I]*(2*U*U*U-3*U*U+1)+M0*H*(U*U*U-2*U*U+U)+P[I+1]*(-2*U*U*U+3*U*U)+M1*H*(U*U*U-U*U);
 }
 return P[N-1];
}
inline FQuat Racket(float Phase,bool Back,bool Volley,float Height=115){
 const float S=Back?-1:1;
 const float Z=FMath::GetMappedRangeValueClamped(FVector2D(65,155),FVector2D(-.35,.38),Height);
 const float T[]={0,.24,.38,.52,.68,.84,1};
 const FVector P[]={FVector(.35,.12,.93),FVector(-.40,S*.55,.73),FVector(-.65,S*.72,-.16),FVector(.06,S*.97,Z),FVector(.25,-S*.50,.80),FVector(-.28,-S*.45,.85),FVector(.35,.12,.93)};
 FVector Direction=Curve(T,P,7,Phase).GetSafeNormal();
 if(Volley)Direction=FMath::Lerp(P[0],FVector(.5,S*.6,.65),Smooth(Phase/.45f)*(1-Smooth((Phase-.62f)/.38f))).GetSafeNormal();
 FQuat Result=FRotationMatrix::MakeFromZY(Direction,FVector::ForwardVector).ToQuat();
 // A low contact points down/out while the finish points up/across. Normalizing
 // the Cartesian spline between these almost-opposite directions creates a
 // velocity spike near zero length. Interpolate orientations on the unit sphere
 // for this height range, preserving the exact keyed contact orientation.
 const float LowWeight=Smooth((100.f-Height)/35.f);
 if(!Volley&&LowWeight>0.f){
  FQuat Q[7],A[7];
  for(int I=0;I<7;I++){
   Q[I]=FRotationMatrix::MakeFromZY(P[I].GetSafeNormal(),FVector::ForwardVector).ToQuat();
   if(I&&(Q[I-1]|Q[I])<0)Q[I]=Q[I]*-1.f;
  }
  for(int I=0;I<7;I++)FQuat::CalcTangents(Q[FMath::Max(0,I-1)],Q[I],Q[FMath::Min(6,I+1)],0.f,A[I]);
  int I=0;while(I<5&&Phase>T[I+1])++I;
  const float U=FMath::Clamp((Phase-T[I])/(T[I+1]-T[I]),0.f,1.f);
  Result=FQuat::Slerp(Result,FQuat::Squad(Q[I],A[I],Q[I+1],A[I+1],U),LowWeight).GetNormalized();
 }
 return Result;
}
inline float ContactEnvelope(float Phase){return Smooth((Phase-.15f)/.32f)*(1-Smooth((Phase-.62f)/.35f));}
}

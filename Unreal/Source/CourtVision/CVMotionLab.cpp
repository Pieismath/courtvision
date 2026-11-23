#include "CVTennis.h"
#include "CVAthleteAnim.h"
#include "CVStrokeMotion.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "DrawDebugHelpers.h"

void ACVTennisWorld::InitializeMotionLab(){
 MotionLab=FParse::Value(FCommandLine::Get(),TEXT("cvmotionlab="),MotionCase);
 if(!MotionLab)return;
 if(!FParse::Param(FCommandLine::Get(),TEXT("cvmotionfullquality"))){
  // Capture-only memory budget. Character mesh, skeleton and simulation are
  // unchanged; normal gameplay never uses this diagnostic render profile.
  auto* PC=GetWorld()->GetFirstPlayerController();
  for(const TCHAR* Cmd:{TEXT("r.Streaming.PoolSize 512"),TEXT("r.Shadow.Virtual.MaxPhysicalPages 512"),TEXT("sg.GlobalIlluminationQuality 1"),TEXT("sg.ReflectionQuality 0"),TEXT("r.ScreenPercentage 75")})PC->ConsoleCommand(Cmd);
 }

 MotionSuite=MotionCase==TEXT("suite")||MotionCase==TEXT("adjacent");
 MotionCases={TEXT("idle"),TEXT("ready"),TEXT("split_step"),TEXT("lateral_left"),TEXT("lateral_right"),TEXT("forward"),TEXT("backward_recovery"),TEXT("forehand_stationary"),TEXT("forehand_after_lateral"),TEXT("backhand_stationary"),TEXT("backhand_after_lateral"),TEXT("open_stance"),TEXT("semi_open_stance"),TEXT("neutral_stance"),TEXT("running_forehand"),TEXT("running_backhand"),TEXT("low_ball"),TEXT("high_ball"),TEXT("wide_reach"),TEXT("body_ball"),TEXT("forehand_to_backhand"),TEXT("backhand_to_forehand"),TEXT("shot_recovery"),TEXT("rapid_direction_change"),TEXT("serve"),TEXT("return_serve")};
 if(MotionCase==TEXT("adjacent"))MotionCases={TEXT("idle"),TEXT("ready"),TEXT("split_step"),TEXT("forehand_stationary"),TEXT("backhand_stationary"),TEXT("forehand_to_backhand"),TEXT("backhand_to_forehand"),TEXT("body_ball"),TEXT("serve"),TEXT("return_serve")};
 FParse::Value(FCommandLine::Get(),TEXT("cvmotionout="),MotionOutput);
 FParse::Value(FCommandLine::Get(),TEXT("cvmotioncamera="),MotionCamera);
 if(MotionOutput.IsEmpty())MotionOutput=FPaths::ProjectSavedDir()/TEXT("MotionLab")/MotionCase;
 IFileManager::Get().MakeDirectory(*(MotionOutput/TEXT("frames")),true);
 MotionFrames=(MotionCase==TEXT("rally")?1200:(MotionCase==TEXT("footwork")||MotionCase==TEXT("braking_course"))?720:(MotionCase==TEXT("depth_recovery")?600:360));
 if(MotionSuite)MotionFrames=MotionCases.Num()*240;
 FParse::Value(FCommandLine::Get(),TEXT("cvmotionframes="),MotionFrames);
 MotionOverlay=FParse::Param(FCommandLine::Get(),TEXT("cvmotionoverlay"));
 FApp::SetFixedDeltaTime(1./60);FApp::SetUseFixedTimeStep(true);
 MotionTelemetry=TEXT("frame,time,root_x,root_y,head_x,head_y,head_z,hand_x,hand_y,hand_z,lf_x,lf_y,lf_z,rf_x,rf_y,rf_z,left_planted,right_planted,progress,ball_x,ball_y,ball_z,rally\n");
 UE_LOG(LogTemp,Display,TEXT("CV_MOTION_LAB case=%s camera=%s frames=%d output=%s"),*MotionCase,*MotionCamera,MotionFrames,*MotionOutput);
}

void ACVTennisWorld::CaptureMotionFrame(){
 // Called before the next pose update: telemetry and the previously captured
 // image refer to the same completed skeletal evaluation.
 if(MotionFrame>0 && MotionFrame<=MotionFrames){
  auto* A=Athletes[0];const FVector R=A->GetComponentLocation();
  const FVector H=Rackets[0]->GetComponentTransform().TransformPosition(FVector(0,0,47.5));
  const FVector W=A->GetSocketLocation(TEXT("hand_r")),L=A->GetSocketLocation(TEXT("foot_l")),F=A->GetSocketLocation(TEXT("foot_r"));
  auto* Anim=CastChecked<UCVAthleteAnimInstance>(A->GetAnimInstance());
  FString BoneRow=FString::Printf(TEXT("{\"frame\":%d,\"scenario\":\"%s\",\"stance\":\"%s\",\"bones\":{"),MotionFrame-1,*MotionCase,*MotionStance);
  int Count=0;
  for(FName Name:{FName(TEXT("pelvis")),FName(TEXT("spine_02")),FName(TEXT("head")),FName(TEXT("upperarm_r")),FName(TEXT("lowerarm_r")),FName(TEXT("hand_r")),FName(TEXT("upperarm_l")),FName(TEXT("lowerarm_l")),FName(TEXT("hand_l")),FName(TEXT("thigh_l")),FName(TEXT("calf_l")),FName(TEXT("foot_l")),FName(TEXT("thigh_r")),FName(TEXT("calf_r")),FName(TEXT("foot_r"))}){
   FTransform B=A->GetSocketTransform(Name);FVector P=B.GetLocation();FQuat Q=B.GetRotation();
   BoneRow+=FString::Printf(TEXT("%s\"%s\":[%.4f,%.4f,%.4f,%.6f,%.6f,%.6f,%.6f]"),Count++?TEXT(","):TEXT(""),*Name.ToString(),P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W);
  }
  const auto Right=CVGrip::Build(A->GetSkeletalMeshAsset(),true),Left=CVGrip::Build(A->GetSkeletalMeshAsset(),false);
  const FTransform RG=Right.RacketInHand*A->GetSocketTransform(TEXT("hand_r")),LG=Left.RacketInHand*A->GetSocketTransform(TEXT("hand_l"));
  BoneRow+=FString::Printf(TEXT("},\"grip_gap\":%.5f,\"support_gap\":%.5f,\"ball_gap\":%.3f,\"foot_target_error\":[%.4f,%.4f]}\n"),FVector::Distance(RG.GetLocation(),Rackets[0]->GetComponentLocation()),FVector::Distance(LG.GetLocation(),Rackets[0]->GetComponentLocation()),FVector::Distance(H,Position(Ball.p)),FVector::Distance(L,FVector(Feet[0].feet[0].position.x,Feet[0].feet[0].position.y,Feet[0].feet[0].position.z)),FVector::Distance(F,FVector(Feet[0].feet[1].position.x,Feet[0].feet[1].position.y,Feet[0].feet[1].position.z)));
  // Planner diagnostics are recorded with the completed pose; root derivative
  // in telemetry provides independent velocity/acceleration measurements.
  BoneRow.RemoveFromEnd(TEXT("}\n"));
  BoneRow+=FString::Printf(TEXT(",\"gait\":{\"run\":%.5f,\"heading\":%.5f,\"bob\":%.5f,\"phase\":[%.5f,%.5f],\"yaw\":[%.5f,%.5f],\"pitch\":[%.5f,%.5f]}}\n"),Feet[0].runWeight,Feet[0].travelYaw,Feet[0].bob,Feet[0].feet[0].phase,Feet[0].feet[1].phase,Feet[0].feet[0].yaw,Feet[0].feet[1].yaw,Feet[0].feet[0].pitch,Feet[0].feet[1].pitch);
  MotionBones+=BoneRow;
  MotionTelemetry+=FString::Printf(TEXT("%d,%.6f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%.5f,%.3f,%.3f,%.3f,%d\n"),MotionFrame-1,(MotionFrame-1)/60.,R.X,R.Y,H.X,H.Y,H.Z,W.X,W.Y,W.Z,L.X,L.Y,L.Z,F.X,F.Y,F.Z,!Feet[0].feet[0].moving&&SplitTimer[0]<=0&&!(ServeStroke[0]&&SwingTimer[0]>0),!Feet[0].feet[1].moving&&SplitTimer[0]<=0&&!(ServeStroke[0]&&SwingTimer[0]>0),Anim->GetSwingProgress(),Ball.p.x*100,Ball.p.y*100,Ball.p.z*100,Rally);
 }
 if(MotionFrame>=MotionFrames){
  FFileHelper::SaveStringToFile(MotionBones,*(MotionOutput/TEXT("bones.jsonl")));
  FFileHelper::SaveStringToFile(MotionTelemetry,*(MotionOutput/TEXT("telemetry.csv")));
  UE_LOG(LogTemp,Display,TEXT("CV_MOTION_LAB_COMPLETE frames=%d rally=%d"),MotionFrames,Rally);
  FPlatformMisc::RequestExit(false);return;
 }
 if(MotionFrame>=0 && FApp::CanEverRender())FScreenshotRequest::RequestScreenshot(MotionOutput/TEXT("frames")/FString::Printf(TEXT("%06d.png"),MotionFrame),false,false);
}

bool ACVTennisWorld::TickMotionLab(float Dt){
 CaptureMotionFrame();
 if(MotionFrame>=MotionFrames)return true;
 const int AbsoluteFrame=MotionFrame++;
 const int Frame=MotionSuite&&AbsoluteFrame>=0?AbsoluteFrame%240:AbsoluteFrame;
 if(MotionSuite&&AbsoluteFrame>=0){
  MotionCase=MotionCases[FMath::Min(AbsoluteFrame/240,MotionCases.Num()-1)];
  if(Frame==0){
   MotionEvent=0;Rally=0;Live=false;Serving=false;ServePending=false;PlayerY=OpponentY=0;PointTimer=0;
   for(int I=0;I<2;I++){Depth[I]=DepthGoal[I]=1265;CourtVelocity[I]=FVector2D::ZeroVector;SwingTimer[I]=0;ServeStroke[I]=VolleyStroke[I]=false;Feet[I].reset({double(I?1265:-1265),0,0},I?-1:1);PreviousLocation[I]=FVector(I?1265:-1265,0,0);}
  }
 }
 const float T=Frame/60.f;
 if(Frame==-120){
  StartMatch(false);Menu=false;Paused=false;Demo=false;Serving=false;Live=false;
  PlayerY=OpponentY=0;Depth[0]=Depth[1]=1265;DepthGoal[0]=DepthGoal[1]=1265;
  PreviousLocation[0]=FVector(-1265,0,0);PreviousLocation[1]=FVector(1265,0,0);
 }
 if(MotionCase==TEXT("rally")){
  if(Frame==0){StartMatch(true);Demo=true;}
  if(Frame<0){Menu=false;Serving=false;Live=false;}
  DebugCamera=MotionCamera==TEXT("gameplay")?0:MotionCamera==TEXT("side")?1:MotionCamera==TEXT("rear")?2:3;
  return false;
 }
 const bool Back=MotionCase.Contains(TEXT("backhand"))&&!MotionCase.StartsWith(TEXT("forehand"));
 const bool Moving=MotionCase.Contains(TEXT("running"))||MotionCase.Contains(TEXT("lateral"))||MotionCase.Contains(TEXT("wide"));
 float GoalY=0,GoalDepth=1265;
 if(MotionCase==TEXT("braking_course")){
  // Held goals leave a full stationary recovery after both long runs. The last
  // pair reverses before arrival, after reaching the unchanged 440 cm/s limit.
  GoalY=T<.5?0:T<3?-420:T<5.5?420:T<6?420:T<7?-420:T<8?420:T<9?-420:0;
 }else if(MotionCase==TEXT("depth_recovery")){
  GoalDepth=T<.5?1000:T<3?1430:T<5?1265:T<7?350:1265;
  GoalY=T<.5?0:T<3?-260:T<5?0:T<7?100:0;
  if(Frame==0){Depth[0]=1000;PreviousLocation[0]=FVector(-1000,0,0);Feet[0].reset({-1000,0,0},1);}
 }else if(MotionCase==TEXT("footwork")){
  if(T>1&&T<3)GoalY=-250;else if(T>=3&&T<5)GoalY=250;else if(T>=5&&T<7)GoalDepth=930;else if(T>=9)GoalY=T<10?-180:180;
 }else if(MotionCase==TEXT("lateral_left"))GoalY=T>1&&T<3.4?-280:0;
 else if(MotionCase==TEXT("lateral_right"))GoalY=T>1&&T<3.4?280:0;
 else if(MotionCase==TEXT("serve"))GoalDepth=1240;
 else if(MotionCase==TEXT("forward"))GoalDepth=T>1?900:1265;
 else if(MotionCase==TEXT("backward_recovery")){if(T<0)Depth[0]=900;GoalDepth=T>1?1265:900;}
 else if(MotionCase==TEXT("rapid_direction_change"))GoalY=T<1?0:T<2?-270:T<3?270:T<4?-200:0;
 else if(Moving)GoalY=T>.4&&T<2.6?(Back?-1:1)*240:0;
 if(MotionCase==TEXT("shot_recovery"))GoalY=T<.4?0:T<2.1?150:0;
 if(MotionCase==TEXT("backward_recovery")&&Frame==0){Depth[0]=900;PreviousLocation[0]=FVector(-900,0,0);Feet[0].reset({-900,0,0},1);}
 FVector2D Desired(FMath::Clamp((GoalDepth-Depth[0])*7,-440.f,440.f),FMath::Clamp((GoalY-PlayerY)*7,-440.f,440.f));
 Desired=Desired.GetClampedToMaxSize(440);
 CourtVelocity[0]=FMath::Vector2DInterpConstantTo(CourtVelocity[0],Desired,Dt,1700);
 Depth[0]+=CourtVelocity[0].X*Dt;PlayerY+=CourtVelocity[0].Y*Dt;
 const bool Stroke=MotionCase.Contains(TEXT("forehand"))||MotionCase.Contains(TEXT("backhand"))||MotionCase.Contains(TEXT("stance"))||MotionCase==TEXT("low_ball")||MotionCase==TEXT("high_ball")||MotionCase==TEXT("wide_reach")||MotionCase==TEXT("body_ball")||MotionCase==TEXT("shot_recovery")||MotionCase==TEXT("return_serve")||MotionCase==TEXT("depth_recovery");
 const bool DepthCase=MotionCase==TEXT("depth_recovery");
 const bool Pair=MotionCase.Contains(TEXT("_to_"))||DepthCase;
 const float HitTime=DepthCase?2.6f:MotionCase.Contains(TEXT("after_lateral"))?1.95f:Moving?1.1f:1.45f;
 if(MotionCase==TEXT("split_step")&&Frame==60)SplitTimer[0]=.2;
 if(MotionCase==TEXT("return_serve")&&Frame==42)SplitTimer[0]=.2;
 if(MotionCase==TEXT("serve")){
  if(T<1){Serving=true;Live=false;Match.server=0;Ball.p={(-Depth[0]+40)/100.,(PlayerY-13)/100.,1.23};}
  if(Frame==60){Serving=true;Match.server=0;BeginServe(0,.6,0,.4);}
  if(ServePending)AdvanceServe(Dt);
  else if(Live)Tennis::flight(Ball,Dt);
 }
 if(Stroke){
  const float ShotGap=DepthCase?4.1f:1.8f;
  const int Shot=Pair&&T>=HitTime+ShotGap-CVStroke::Preparation?1:0;const float Impact=HitTime+Shot*ShotGap;
  const bool B=Shot?!Back:Back;
  const FVector Root(-Depth[0],PlayerY,0);
  const float Height=MotionCase==TEXT("low_ball")?64:MotionCase==TEXT("high_ball")?165:115;
  const float Lateral=MotionCase==TEXT("wide_reach")?110:MotionCase==TEXT("body_ball")?34:82;
  const FVector Contact=Root+FVector(35,(B?-1:1)*Lateral,Height);
  if(T>=Impact-CVStroke::Preparation&&MotionEvent==Shot*2){
   PrepareStroke(0,B,Contact,Impact-T+Dt);
   if(DepthCase&&Shot==1)VolleyStroke[0]=true;
   if(MotionCase==TEXT("semi_open_stance")){Feet[0].stance=(B?1:-1)*.4;MotionStance=TEXT("semi-open");}
   if(MotionCase==TEXT("neutral_stance")){Feet[0].stance=(B?1:-1)*.8;MotionStance=TEXT("neutral");}
   MotionEvent++;
  }
  if(T>=Impact&&MotionEvent==Shot*2+1){
   GroundContact[0]=Contact;Backhand[0]=B;
   CastChecked<UCVAthleteAnimInstance>(Athletes[0]->GetAnimInstance())->NotifyRacketContact(B);
   MotionEvent++;Rally++;
  }
  if(MotionEvent==Shot*2+1)GroundContact[0]=Contact;
  const float Flight=T-Impact;
  Ball.p={(Contact.X+(Flight<0?-Flight*1400:Flight*1700))/100.,Contact.Y/100.,(Contact.Z+(Flight<0?-Flight*120:Flight*450))/100.};
  BallMesh->SetVisibility(T>Impact-.6&&T<Impact+.65);
 }
 else BallMesh->SetVisibility(MotionCase==TEXT("serve"));
 for(int I=0;I<2;I++){SwingTimer[I]=FMath::Max(0.f,SwingTimer[I]-Dt);AnimateAthlete(I,Dt);EvaluateAthlete(I,Dt);}
 BallMesh->SetWorldLocation(Position(Ball.p));
 const FVector Root=Athletes[0]->GetComponentLocation(),Look=Root+FVector(0,0,105);
 FVector Cam=Root+(MotionCamera==TEXT("side")?FVector(0,450,165):MotionCamera==TEXT("rear")?FVector(-470,0,195):FVector(340,350,210));
 if(MotionCamera==TEXT("gameplay"))Cam=FVector(-2600,-80,1250);
 Camera->SetWorldLocation(Cam);Camera->SetWorldRotation((Look-Cam).Rotation());
 MotionDiagnostics(Dt);
 return true;
}

void ACVTennisWorld::MotionDiagnostics(float Dt){
 auto* PC=GetWorld()->GetFirstPlayerController();if(!PC)return;
 if(PC->WasInputKeyJustPressed(EKeys::F2))MotionOverlay=!MotionOverlay;
 if(!MotionLab&&PC->WasInputKeyJustPressed(EKeys::F3)){DebugRate=DebugRate==1?.5f:DebugRate==.5f?.25f:1.f;UGameplayStatics::SetGlobalTimeDilation(this,DebugRate);}
 if(PC->WasInputKeyJustPressed(EKeys::F4))DebugCamera=(DebugCamera+1)%4;
 for(int I=0;I<2;I++){
  auto* A=Athletes[I];auto* R=Rackets[I];
  const FVector Head=R->GetComponentTransform().TransformPosition(FVector(0,0,47.5));
  if(Dt>0){RacketHistory[I].Add(Head);if(RacketHistory[I].Num()>60)RacketHistory[I].RemoveAt(0);}
  if(!MotionOverlay)continue;
  for(int J=1;J<RacketHistory[I].Num();J++)DrawDebugLine(GetWorld(),RacketHistory[I][J-1],RacketHistory[I][J],FColor::Yellow,false,0,0,1);
  for(int B=1;B<A->GetNumBones();B++){
   const FName Name=A->GetBoneName(B),Parent=A->GetParentBone(Name);
   if(Name.ToString().Contains(TEXT("index"))||Name.ToString().Contains(TEXT("middle"))||Name.ToString().Contains(TEXT("pinky"))||Name.ToString().Contains(TEXT("ring"))||Name.ToString().Contains(TEXT("thumb")))continue;
   DrawDebugLine(GetWorld(),A->GetSocketLocation(Parent),A->GetSocketLocation(Name),FColor::Cyan,false,0,0,.8);
  }
  for(FName Bone:{FName(TEXT("hand_r")),FName(TEXT("hand_l")),FName(TEXT("pelvis")),FName(TEXT("spine_02")),FName(TEXT("clavicle_r"))}){
   const FTransform B=A->GetSocketTransform(Bone);DrawDebugCoordinateSystem(GetWorld(),B.GetLocation(),B.Rotator(),12,false,0,0,1);
  }
  DrawDebugCoordinateSystem(GetWorld(),R->GetComponentLocation(),R->GetComponentRotation(),18,false,0,0,1);
  DrawDebugSphere(GetWorld(),Head,3,8,FColor::Yellow,false,0);
  DrawDebugSphere(GetWorld(),GroundContact[I],4,10,FColor::Magenta,false,0);
  for(int F=0;F<2;F++){auto P=Feet[I].feet[F].position;DrawDebugSphere(GetWorld(),FVector(P.x,P.y,P.z),5,8,Feet[I].feet[F].moving?FColor::Orange:FColor::Green,false,0);}
  const FVector Root=A->GetComponentLocation()+FVector(0,0,10);
  DrawDebugDirectionalArrow(GetWorld(),Root,Root+A->GetForwardVector()*70,8,FColor::Blue,false,0,0,2);
  DrawDebugDirectionalArrow(GetWorld(),Root,Root+FVector((I?1:-1)*CourtVelocity[I].X,CourtVelocity[I].Y,0)*.25,8,FColor::Red,false,0,0,2);
  DrawDebugDirectionalArrow(GetWorld(),Root,Reception[I]+FVector(0,0,10),8,FColor::Purple,false,0,0,1);
  const auto* Anim=Cast<UCVAthleteAnimInstance>(A->GetAnimInstance());
  const TCHAR* Shot=ServeStroke[I]&&SwingTimer[I]>0?TEXT("serve"):Anim&&Anim->IsSwingActive()?(Backhand[I]?TEXT("backhand"):TEXT("forehand")):CourtVelocity[I].Size()>20?TEXT("footwork"):TEXT("ready");
  const TCHAR* Stance=CourtVelocity[I].Size()>270?TEXT("running"):FMath::Abs(Feet[I].stance)>.6?TEXT("neutral"):FMath::Abs(Feet[I].stance)>.15?TEXT("semi-open"):TEXT("open");
  DrawDebugString(GetWorld(),A->GetComponentLocation()+FVector(0,0,215),FString::Printf(TEXT("%s %s | phase %.2f | rate %.2f\nfeet %s/%s | IK feet %.2f | root code\nF2 overlay  F3 speed %.2fx  F4 camera  F5 pause  F6 step"),Shot,Stance,Anim?Anim->GetSwingProgress():0,StrokeRate[I],Feet[I].feet[0].moving?TEXT("free"):TEXT("plant"),Feet[I].feet[1].moving?TEXT("free"):TEXT("plant"),Anim?Anim->RequestedFootWeight:0.f,DebugRate),nullptr,FColor::White,0,true);
 }
 if(MotionOverlay){
  DrawDebugSphere(GetWorld(),Position(Ball.p),4,10,FColor::White,false,0);
  Tennis::Ball P=Ball;for(int J=0;J<24;J++)Tennis::flight(P,Tennis::Step);
  DrawDebugSphere(GetWorld(),Position(P.p),5,10,FColor::Orange,false,0);
 }
}

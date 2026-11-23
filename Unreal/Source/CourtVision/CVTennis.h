#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Actor.h"
#include "TennisPhysics.h"
#include "CVFootwork.h"
#include "CVTennis.generated.h"

class UStaticMeshComponent; class USkeletalMeshComponent; class UMaterialInstanceDynamic;
class UCameraComponent; class USoundWave; class FSocket;

UCLASS()
class COURTVISION_API ACVTennisWorld : public AActor {
 GENERATED_BODY()
public:
 ACVTennisWorld();
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 void SelectVenue(int Index);
 void StartMatch(bool Practice=false);
 void Swing(float Power=.6f,float Aim=0,float Spin=.4f);
 void ChangeView();
 void PlaySound(const TCHAR* Asset,float Volume=1,float Pitch=1);
 UStaticMeshComponent* AddMesh(const FString& Path,FVector Location,FVector Scale,UMaterialInterface* Mat=nullptr);
 void PollVision();
 void StepBall();
 void Strike(int Side,float Power,float Aim,float Spin);
 void BeginServe(int Side,float Power,float Aim,float Spin);
 void AdvanceServe(float Delta);
 void Point(int Winner,const FString& Why);
 void NewPoint();
 void AnimateAthlete(int Index,float Dt);
 void EvaluateAthlete(int Index,float Dt);
 void ResolveRacketContact();
 int PendingContactSide=-1;
 FVector PreviousRacketHead[2],ContactBallStart;
 float ContactFrameDt=0;
 bool DebugPaused=false,DebugStep=false;
 int PreparedSplitRally[2]={-1,-1};
 void PrepareStroke(int Index,bool Back,FVector Contact,float TimeToContact);
 float StrokeRate[2]={.8f,.8f};
 void MoveAthletes(float Dt);
 void InitializeMotionLab();
 bool TickMotionLab(float Dt);
 void CaptureMotionFrame();
 void MotionDiagnostics(float Dt);
 FString MotionCase,MotionOutput,MotionCamera=TEXT("diagonal");
 int MotionFrame=-120,MotionFrames=240,MotionEvent=0;
 bool MotionLab=false,MotionOverlay=false;
 float DebugRate=1;int DebugCamera=0;
 FString MotionState=TEXT("ready"),MotionStance=TEXT("open");
 TArray<FVector> RacketHistory[2];
 FString MotionTelemetry,MotionBones;
 bool MotionSuite=false;
 TArray<FString> MotionCases;

 FVector Position(Tennis::V3 P)const{return FVector(P.x*100,P.y*100,P.z*100);}
 FString PointText(int Who)const;
 FString VenueName()const;
 FString SurfaceName()const;
 FString Status=TEXT("WELCOME TO YOUR COURT");
 FString LastShot=TEXT("STEP INTO THE GAME");
 Tennis::Score Match;
 Tennis::Ball Ball;
 int Venue=0, LastHitter=-1,Bounces=0,Rally=0,BestRally=0,View=0;
 int ServeAttempt=0; bool Serving=true,Live=false,Menu=true,PracticeMode=false,Paused=false;
 bool VisionTracked=false,VisionCalibrated=false,VisionSynthetic=false;
 float Confidence=0,VisionAge=999,BodyX=0,RacketX=0,RacketY=0,LastPower=0,LastSpeed=0;
 float PlayerY=0,OpponentY=0,Clock=0,PointTimer=0,SwingTimer[2]={0,0};
 float ContactFlash=0,ScoreFlash=0,SmoothedFPS=60,VisionLatencyMS=0;
 bool SwingQueued=false; float QueuedPower=.6,QueuedAim=0,QueuedSpin=.4,SwingWindow=0;
 bool Demo=false; int DemoHits=0;
 float NextAim=0,InputIgnoreUntil=0;
 bool ReceiveBackhand[2]={false,false};
 bool Backhand[2]={false,false},ServeStroke[2]={false,false};
 bool ServePending=false; int ServeSide=0;
 float ServeElapsed=0,ServePower=.6,ServeAim=0,ServeSpin=.4,ServeAnchorY=0;
 FVector ServeContactPoint=FVector::ZeroVector;
 static constexpr float ServeContactTime=.55f;
 static constexpr float ServeDuration=1.2f;
 UPROPERTY() UCameraComponent* Camera;
 UPROPERTY() TArray<UStaticMeshComponent*> SurfaceMeshes;
 UPROPERTY() UStaticMeshComponent* BallMesh;
 UPROPERTY() TArray<USkeletalMeshComponent*> Athletes;
 UPROPERTY() TArray<UStaticMeshComponent*> Rackets;
 UPROPERTY() TArray<UMaterialInstanceDynamic*> CourtMats;
 UPROPERTY() TArray<UMaterialInstanceDynamic*> SurroundMats;
 UPROPERTY() UMaterialInstanceDynamic* SeatMat;
 UPROPERTY() UStaticMeshComponent* TargetMesh;
 UPROPERTY() TArray<UStaticMeshComponent*> Trail;
 UPROPERTY() class ADirectionalLight* SunLight;
 UPROPERTY() class APostProcessVolume* PostVolume;
 UPROPERTY() TArray<class ASpotLight*> FloodLights;
private:
 void BuildWorld();
 void BuildCrowd();
 float Accumulator=0,LastReceive=-999,LastAIStrike=-999;
 double LastSenderStamp=0; uint64 LastSequence=0,LastSwingId=0;
 FString VisionStreamId;
 FSocket* Socket=nullptr;
 int CurrentServeBox=1; float AimPhase=0;
 int PreparedSwingRally[2]={-1,-1};
 float Depth[2]={1265,1265},DepthGoal[2]={1265,1265};
 FVector2D CourtVelocity[2]={FVector2D::ZeroVector,FVector2D::ZeroVector};
 FVector Reception[2]={FVector::ZeroVector,FVector::ZeroVector};
 bool NetPlay[2]={false,false},ManualDepth=false,VolleyStroke[2]={false,false};
 float SplitTimer[2]={0,0};
 CVFootwork::Planner Feet[2];
 FVector PreviousLocation[2]={FVector::ZeroVector,FVector::ZeroVector};
 FVector GroundContact[2]={FVector::ZeroVector,FVector::ZeroVector};
 float LastFootstep[2]={-1,-1};
 UPROPERTY() TArray<class UInstancedStaticMeshComponent*> Crowd;
 UPROPERTY() class UAudioComponent* Ambience=nullptr;
 UPROPERTY() TMap<FString,USoundWave*> SoundCache;
 UPROPERTY() TArray<class UAnimSequence*> Animations;
 FVector CameraGoal;
};

UCLASS()
class COURTVISION_API ACVTennisHUD : public AHUD {
 GENERATED_BODY()
public:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void DrawHUD() override;
 virtual void NotifyHitBoxClick(FName BoxName) override;
 TSharedPtr<class SWidget> InputLayer;
 void Label(const FString& S,float X,float Y,float Size,FLinearColor Color=FLinearColor::White,bool Center=false);
 void Box(float X,float Y,float W,float H,FLinearColor C);
 float Scale=1; ACVTennisWorld* Game=nullptr;
};

UCLASS()
class COURTVISION_API ACVTennisGameMode : public AGameModeBase {
 GENERATED_BODY()
public:
 ACVTennisGameMode();
 virtual void BeginPlay() override;
};

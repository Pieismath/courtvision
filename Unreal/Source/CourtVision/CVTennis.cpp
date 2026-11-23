#include "CVTennis.h"
#include "CVAthleteAnim.h"
#include "CVStrokeMotion.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SpotLight.h"
#include "Components/SpotLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Canvas.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundWave.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Common/UdpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "CanvasItem.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"

namespace {
FLinearColor RGB(float R,float G,float B){return FLinearColor(R,G,B,1);}
const FLinearColor Ink(.009,.022,.024,.94),Cream(.91,.925,.85,1),Mint(.67,.90,.37,1),Muted(.53,.63,.61,1);
}
ACVTennisWorld::ACVTennisWorld(){
 PrimaryActorTick.bCanEverTick=true;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Scene"));
 Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("BroadcastCamera"));Camera->SetupAttachment(RootComponent);
 Camera->FieldOfView=58; Camera->SetWorldLocation(FVector(-2350,-280,980));Camera->SetWorldRotation((FVector(100,0,40)-Camera->GetComponentLocation()).Rotation());
}
UStaticMeshComponent* ACVTennisWorld::AddMesh(const FString& Path,FVector Location,FVector Size,UMaterialInterface* Mat){
 UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);if(!Mesh){UE_LOG(LogTemp,Warning,TEXT("Missing mesh %s"),*Path);return nullptr;}
 auto* C=NewObject<UStaticMeshComponent>(this);C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetMobility(EComponentMobility::Movable);C->SetupAttachment(RootComponent);C->RegisterComponent();C->SetWorldLocation(Location);C->SetWorldScale3D(Size);if(Mat)C->SetMaterial(0,Mat);return C;
}
void ACVTennisWorld::BuildWorld(){
 const bool MotionStage=FParse::Param(FCommandLine::Get(),TEXT("cvmotionstage"));
 auto* Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Court.M_Court"));
 auto* Solid=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Solid.M_Solid"));
 auto Mat=[&](FLinearColor Col,float Rough=.8f){auto* M=UMaterialInstanceDynamic::Create(Solid,this);if(M){M->SetVectorParameterValue(TEXT("Color"),Col);M->SetScalarParameterValue(TEXT("Roughness"),Rough);}return M;};
 auto* CMat=UMaterialInstanceDynamic::Create(Surface,this);auto* SMat=UMaterialInstanceDynamic::Create(Surface,this);CourtMats.Add(CMat);SurroundMats.Add(SMat);
 AddMesh(TEXT("/Engine/BasicShapes/Cube.Cube"),FVector(0,0,-16),FVector(95,95,.25),Mat(RGB(.09,.14,.058)));
 SurfaceMeshes.Add(AddMesh(TEXT("/Engine/BasicShapes/Cube.Cube"),FVector(0,0,-6),FVector(40.5,21.5,.12),SMat));
 SurfaceMeshes.Add(AddMesh(TEXT("/Engine/BasicShapes/Cube.Cube"),FVector(0,0,-.85),FVector(23.77,10.97,.017),CMat));
 auto* White=Mat(RGB(.88,.9,.80),.82);
 auto Line=[&](float X,float Y,float SX,float SY){AddMesh(TEXT("/Engine/BasicShapes/Cube.Cube"),FVector(X,Y,1),FVector(SX/100,SY/100,.014),White);};
 for(float X:{-1188.5f,1188.5f})Line(X,0,10,1097);
 for(float Y:{-548.5f,548.5f,-411.5f,411.5f})Line(0,Y,2377,5);
 for(float X:{-640.f,640.f})Line(X,0,5,823);
 Line(0,0,1280,5);for(float X:{-1180.f,1180.f})Line(X,0,12,5);
 if(!MotionStage)AddMesh(TEXT("/Game/Environment/Stadium.Stadium"),FVector::ZeroVector,FVector(1));
 AddMesh(TEXT("/Game/Environment/TennisNet.TennisNet"),FVector::ZeroVector,FVector(1));
 BallMesh=AddMesh(TEXT("/Game/Environment/TennisBall.TennisBall"),FVector(0,0,100),FVector(1));
 if(!BallMesh)BallMesh=AddMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"),FVector(0,0,100),FVector(.067),Mat(RGB(.63,.88,.028),.95));
 if(BallMesh)BallMesh->SetAffectDistanceFieldLighting(false);
 auto* TargetMat=Mat(RGB(.46,.69,.23));
 TargetMesh=AddMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),FVector(0,0,1.6),FVector(.20,.20,.006),TargetMat);
 if(TargetMesh){TargetMesh->SetCastShadow(false);TargetMesh->SetVisibility(false);}
 for(int I=0;I<5;I++){auto* T=AddMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"),FVector(0,0,-100),FVector(.020*(1.-I*.12)),Mat(RGB(.58,.67,.07),1));if(T){T->SetCastShadow(false);Trail.Add(T);}}
 for(const TCHAR* N:{TEXT("Ready"),TEXT("Forehand"),TEXT("Backhand"),TEXT("Run"),TEXT("Serve")}) Animations.Add(LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Athletes/AN_%s.AN_%s"),N,N)));
 USkeletalMesh* Human=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Athletes/Athlete.Athlete"));
 for(int I=0;I<2;I++){
  auto* Body=NewObject<USkeletalMeshComponent>(this);Body->SetupAttachment(RootComponent);Body->SetSkinnedAssetAndUpdate(Human);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);Body->RegisterComponent();Body->AddTickPrerequisiteActor(this);Body->SetComponentTickEnabled(false);Body->SetWorldLocation(FVector((I?1:-1)*Tennis::RecoveryDepth*100,0,0));Body->SetWorldRotation(FRotator(0,I?180:0,0));Athletes.Add(Body);
  Body->SetAnimInstanceClass(UCVAthleteAnimInstance::StaticClass());
  if(auto* Anim=Cast<UCVAthleteAnimInstance>(Body->GetAnimInstance())){Anim->ConfigureClips(Animations[0],Animations[3],Animations[1],Animations[2],Animations[4]);Anim->ConfigureGrip(Human);}
  if(Human){for(int Slot=0;Slot<Human->GetMaterials().Num();Slot++){const FString Name=Human->GetMaterials()[Slot].MaterialSlotName.ToString();if(auto* Skin=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Athletes/Materials/%s.%s"),*Name,*Name)))Body->SetMaterial(Slot,Skin);}}
  if(I==1){for(int Slot=0;Slot<Body->GetNumMaterials();Slot++){auto* Base=Body->GetMaterial(Slot);if(Base&&(Base->GetName().Contains(TEXT("Polo"))||Base->GetName().Contains(TEXT("Shorts")))){auto* Fabric=Body->CreateDynamicMaterialInstance(Slot);Fabric->SetVectorParameterValue(TEXT("ColorMultiplier"),RGB(.025,.13,.23));}}}

  auto* R=AddMesh(TEXT("/Game/Environment/Racket.Racket"),FVector(I?1130:-1160,60,80),FVector(1));Rackets.Add(R);
  if(R)R->SetAffectDistanceFieldLighting(false);
  if(R&&Body->GetBoneIndex(TEXT("hand_r"))>=0){R->AttachToComponent(Body,FAttachmentTransformRules::SnapToTargetNotIncludingScale,TEXT("hand_r"));R->SetRelativeTransform(CVGrip::Build(Human,true).RacketInHand);}
  PreviousLocation[I]=Body->GetComponentLocation();
 }
 auto* Sun=GetWorld()->SpawnActor<ADirectionalLight>();SunLight=Sun;Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);Sun->SetActorRotation(FRotator(-31,-52,0));Sun->GetLightComponent()->SetIntensity(65000);Sun->GetLightComponent()->SetLightColor(FLinearColor(1,.91,.76));Sun->GetLightComponent()->SetCastShadows(true);
 auto* Directional=Cast<UDirectionalLightComponent>(Sun->GetLightComponent());Directional->LightSourceAngle=.535;Directional->SetAtmosphereSunLight(true);
 GetWorld()->SpawnActor<ASkyAtmosphere>();
 auto* Sky=GetWorld()->SpawnActor<ASkyLight>();Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);Sky->GetLightComponent()->SetIntensity(1.65);Sky->GetLightComponent()->SetRealTimeCaptureEnabled(true);
 auto* Fog=GetWorld()->SpawnActor<AExponentialHeightFog>();Fog->GetComponent()->SetFogDensity(.0014);Fog->GetComponent()->SetFogHeightFalloff(.18);
 auto* Post=GetWorld()->SpawnActor<APostProcessVolume>();PostVolume=Post;Post->bUnbound=true;
 auto& P=Post->Settings;P.bOverride_AutoExposureMethod=true;P.AutoExposureMethod=AEM_Histogram;P.bOverride_AutoExposureMinBrightness=true;P.bOverride_AutoExposureMaxBrightness=true;P.AutoExposureMinBrightness=13;P.AutoExposureMaxBrightness=13;
 P.bOverride_BloomIntensity=true;P.BloomIntensity=.18;P.bOverride_VignetteIntensity=true;P.VignetteIntensity=.18;P.bOverride_MotionBlurAmount=true;P.MotionBlurAmount=0;P.bOverride_ColorSaturation=true;P.ColorSaturation=FVector4(1.05,1.04,1.0,1);
 P.bOverride_AmbientOcclusionIntensity=true;P.AmbientOcclusionIntensity=.65;P.bOverride_AmbientOcclusionRadius=true;P.AmbientOcclusionRadius=140;
 for(float X:{-2000.f,2000.f})for(float Y:{-1250.f,1250.f}){
  auto* Light=GetWorld()->SpawnActor<ASpotLight>();auto* S=Cast<USpotLightComponent>(Light->GetLightComponent());S->SetMobility(EComponentMobility::Movable);
  Light->SetActorLocation(FVector(X,Y,1150));Light->SetActorRotation((FVector(0,0,0)-Light->GetActorLocation()).Rotation());
  S->SetIntensityUnits(ELightUnits::Lumens);S->SetIntensity(3000000);S->SetAttenuationRadius(6000);S->SetInnerConeAngle(40);S->SetOuterConeAngle(66);S->SetLightColor(FLinearColor(.79,.88,1));S->SetSourceRadius(100);S->SetSoftSourceRadius(150);S->SetCastShadows(X*Y>0);S->SetVisibility(false);FloodLights.Add(Light);
 }
 if(!MotionStage)BuildCrowd();SelectVenue(0);
 for(const TCHAR* Name:{TEXT("Hit"),TEXT("Bounce"),TEXT("Point"),TEXT("Swing"),TEXT("Footstep"),TEXT("Applause"),TEXT("StadiumBed")}){
  const FString Path=FString::Printf(TEXT("/Game/Audio/%s.%s"),Name,Name);
  if(auto* Sound=LoadObject<USoundWave>(nullptr,*Path))SoundCache.Add(Path,Sound);
 }
}
void ACVTennisWorld::BuildCrowd(){
 for(int V=0;V<3;V++){
  auto* Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Crowd/Crowd_%d.Crowd_%d"),V,V));
  if(!Mesh)continue;
  auto* C=NewObject<UInstancedStaticMeshComponent>(this);C->SetupAttachment(RootComponent);
  C->SetStaticMesh(Mesh);C->SetNumCustomDataFloats(3);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  C->SetCastShadow(false);C->SetAffectDistanceFieldLighting(false);C->SetVisibleInRayTracing(false);C->SetCullDistances(7000,10000);C->RegisterComponent();Crowd.Add(C);
 }
 if(Crowd.Num()!=3)return;
 struct Stand {float X,Y,Angle,Length;int Blocks;};
 const Stand Stands[]={{0,12.4f,0,43,4},{0,-12.4f,PI,43,4},{22.1f,0,-PI/2,22,2},{-22.1f,0,PI/2,22,2}};
 const FLinearColor Colors[]={RGB(.08,.12,.20),RGB(.62,.56,.43),RGB(.24,.07,.045),RGB(.055,.16,.14),RGB(.34,.32,.30),RGB(.53,.57,.61),RGB(.14,.12,.22),RGB(.34,.20,.08)};
 FRandomStream Random(142);int Count=0;
 for(const auto& S:Stands)for(int Row=0;Row<9;Row++){
  const float V=.52f+Row*.87f,Z=.33f+Row*.44f,Width=S.Length/S.Blocks;
  for(int Block=0;Block<S.Blocks;Block++){
   const float Start=-S.Length/2+Block*Width+1,End=Start+Width-1.45f;const int Cols=int((End-Start)/.57f);
   for(int Col=0;Col<Cols;Col++){
    if(Random.FRand()>.47f)continue;
    const float U=Start+(Col+.5f)*(End-Start)/Cols;
    FVector P((S.X+U*FMath::Cos(S.Angle)-V*FMath::Sin(S.Angle))*100,(S.Y+U*FMath::Sin(S.Angle)+V*FMath::Cos(S.Angle))*100,Z*100);
    const float Yaw=FMath::RadiansToDegrees(S.Angle)-90;
    auto* C=Crowd[Random.RandRange(0,2)];const auto Color=Colors[Random.RandRange(0,7)];
    const int Index=C->AddInstance(FTransform(FRotator(0,Yaw+Random.FRandRange(-3,3),0),P,FVector(Random.FRandRange(.94f,1.02f))),false);
    C->SetCustomDataValue(Index,0,Color.R);C->SetCustomDataValue(Index,1,Color.G);C->SetCustomDataValue(Index,2,Color.B);Count++;
   }
  }
 }
 for(auto* C:Crowd)C->MarkRenderStateDirty();
 UE_LOG(LogTemp,Display,TEXT("CV_CROWD instances=%d variants=%d"),Count,Crowd.Num());
}
void ACVTennisWorld::BeginPlay(){
 Super::BeginPlay();BuildWorld();
 if(FParse::Param(FCommandLine::Get(),TEXT("cvservecapture"))||FParse::Param(FCommandLine::Get(),TEXT("cvrallycapture"))||FParse::Param(FCommandLine::Get(),TEXT("cvgripcapture"))||FParse::Param(FCommandLine::Get(),TEXT("cvmotioncapture"))){
  // Screenshot readback can stall rendering. Fixed time keeps these cinematic
  // pose inspections frame-accurate; court performance captures remain real time.
  FApp::SetFixedDeltaTime(1.0/60.0);FApp::SetUseFixedTimeStep(true);
 }
 if(!FParse::Param(FCommandLine::Get(),TEXT("cvselftest")))if(auto* Bed=SoundCache.FindRef(TEXT("/Game/Audio/StadiumBed.StadiumBed"))){Ambience=UGameplayStatics::SpawnSound2D(this,Bed,.22f,1.f,0.f,nullptr,false,false);}
 Socket=FUdpSocketBuilder(TEXT("CourtVisionPose")).AsNonBlocking().AsReusable().BoundToAddress(FIPv4Address(127,0,0,1)).BoundToPort(7779).WithReceiveBufferSize(65536);
 if(!Socket)UE_LOG(LogTemp,Error,TEXT("CourtVision cannot bind vision port 7779"));
 auto* PC=GetWorld()->GetFirstPlayerController();PC->SetViewTarget(this);PC->bShowMouseCursor=true;PC->bEnableClickEvents=false;PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
 GetWorld()->GetGameViewport()->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
 PC->ConsoleCommand(TEXT("sg.GlobalIlluminationQuality 2"));InitializeMotionLab();
 if(FParse::Param(FCommandLine::Get(),TEXT("cvselftest"))){
  checkf(Animations[4]&&FMath::IsNearlyEqual(Animations[4]->GetPlayLength(),ServeDuration,.02f),TEXT("Serve clip length must match timeline"));
  StartMatch(false);Swing(.6f,0,.3f);
  checkf(ServePending&&!Live,TEXT("Serve should begin with a toss"));
  AdvanceServe(.2f);const float TossTime=ServeElapsed;Swing(.9f,1,1);
  checkf(ServeElapsed==TossTime&&ServePower==.6f,TEXT("Repeated gesture restarted or changed a toss"));
  Paused=true;AdvanceServe(.2f);checkf(ServeElapsed==TossTime,TEXT("Paused toss advanced"));Paused=false;
  AdvanceServe(.35f);
  checkf(Live&&Serving&&LastHitter==0&&Rally==1,TEXT("Serve did not release at contact"));
  const auto Released=Ball.p;AdvanceServe(1.f);checkf(Ball.p.x==Released.x&&Rally==1,TEXT("Serve released twice"));
  Ball.p.x=-6;SwingTimer[0]=0;Swing(.6f,0,.3f);
  checkf(Ball.p.x==-6&&Rally==1,TEXT("Repeated swing restarted a live serve"));
  SwingQueued=false;Ball.p={-.01,0,.2};Ball.v={10,0,0};StepBall();
  checkf(!Live&&ServeAttempt==1&&Match.points[1]==0,TEXT("First fault did not preserve the point"));
  NewPoint();SwingTimer[0]=0;Swing(.6f,0,.3f);AdvanceServe(ServeContactTime);Ball.p={-.01,0,.2};Ball.v={10,0,0};StepBall();
  checkf(Match.points[1]==1&&ServeAttempt==0,TEXT("Double fault did not award receiver"));
  StartMatch(false);Ball.p={5,0,Tennis::Radius};Ball.v={0,0,-1};Live=true;Serving=false;LastHitter=0;Bounces=1;StepBall();
  checkf(Match.points[0]==1,TEXT("Second bounce did not award hitter"));
  // Animation regression continues over separate engine frames in Tick.
  // Unreal intentionally suppresses duplicate graph updates within one frame.
  for(auto* Athlete:Athletes)Athlete->SetComponentTickEnabled(false);
  return;
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("cvservecapture")))StartMatch(false);
 Demo=FParse::Param(FCommandLine::Get(),TEXT("cvdemo"))||FParse::Param(FCommandLine::Get(),TEXT("cvrallycapture"));if(Demo){StartMatch(true);Status=TEXT("AUTOMATED PLAYTEST");}
}
void ACVTennisWorld::EndPlay(const EEndPlayReason::Type Reason){if(Ambience)Ambience->Stop();if(Socket){Socket->Close();ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);Socket=nullptr;}Super::EndPlay(Reason);}
FString ACVTennisWorld::VenueName()const{const TCHAR* N[]={TEXT("THE GARDEN"),TEXT("NIGHT SESSION"),TEXT("TERRE ROUGE"),TEXT("SOUTHERN SUMMER")};return N[Venue];}
FString ACVTennisWorld::SurfaceName()const{const TCHAR* N[]={TEXT("LONDON  /  GRASS"),TEXT("NEW YORK  /  HARD"),TEXT("PARIS  /  CLAY"),TEXT("MELBOURNE  /  HARD")};return N[Venue];}
void ACVTennisWorld::SelectVenue(int N){
 Venue=FMath::Clamp(N,0,3);
 if(SunLight){SunLight->SetActorRotation(FRotator(Venue==1?12.f:Venue==2?-45.f:Venue==3?-56.f:-31.f,-52,0));SunLight->GetLightComponent()->SetIntensity(Venue==1?50:65000);SunLight->GetLightComponent()->SetLightColor(Venue==1?RGB(.40,.52,.9):RGB(1,.91,.76));}
 for(auto* F:FloodLights)if(F)F->GetLightComponent()->SetVisibility(Venue==1);
 const float Exposure[]={13.5f,10.3f,14.2f,14.2f};
 if(PostVolume){PostVolume->Settings.AutoExposureMinBrightness=Exposure[Venue];PostVolume->Settings.AutoExposureMaxBrightness=Exposure[Venue];}

 const FLinearColor A[]={RGB(.07,.175,.033),RGB(.018,.09,.24),RGB(.28,.065,.026),RGB(.02,.22,.42)};
 const FLinearColor B[]={RGB(.082,.20,.040),RGB(.020,.096,.25),RGB(.34,.10,.04),RGB(.025,.23,.43)};
 const FLinearColor S[]={RGB(.075,.18,.037),RGB(.028,.12,.092),RGB(.30,.075,.035),RGB(.032,.16,.24)};
 const TCHAR* TextureSet=Venue==0?TEXT("Grass"):Venue==2?TEXT("Clay"):TEXT("HardCourt");
 for(auto* M:{CourtMats.Num()?CourtMats[0]:nullptr,SurroundMats.Num()?SurroundMats[0]:nullptr})if(M){
  const TCHAR* Suffix[]={TEXT("BaseColor"),TEXT("Normal"),TEXT("Roughness")};const TCHAR* Params[]={TEXT("SurfaceColor"),TEXT("SurfaceNormal"),TEXT("SurfaceRoughness")};
  for(int I=0;I<3;I++){FString TexName=FString::Printf(TEXT("T_%s_%s"),TextureSet,Suffix[I]);if(auto* T=LoadObject<UTexture>(nullptr,*FString::Printf(TEXT("/Game/Textures/Environment/%s.%s"),*TexName,*TexName)))M->SetTextureParameterValue(Params[I],T);}
 }
 for(auto* M:CourtMats)if(M){M->SetVectorParameterValue(TEXT("ColorA"),A[Venue]);M->SetVectorParameterValue(TEXT("ColorB"),B[Venue]);M->SetScalarParameterValue(TEXT("Grass"),Venue==0?1:0);M->SetScalarParameterValue(TEXT("Roughness"),Venue==1?.76:.95);}
 for(auto* M:SurroundMats)if(M){M->SetVectorParameterValue(TEXT("ColorA"),S[Venue]);M->SetVectorParameterValue(TEXT("ColorB"),S[Venue]*1.1);M->SetScalarParameterValue(TEXT("Grass"),Venue==0?1:0);M->SetScalarParameterValue(TEXT("Roughness"),.96);}
}
void ACVTennisWorld::PollVision(){
 if(!Socket)return;uint32 Size=0;TSharedPtr<FJsonObject> Latest;int Count=0;bool BaselineSwing=false;
 while(Count++<128&&Socket->HasPendingData(Size)){
  TArray<uint8> Data;Data.SetNumUninitialized(FMath::Min(Size,16384u)+1);int32 Read=0;if(!Socket->Recv(Data.GetData(),Data.Num()-1,Read))break;Data[Read]=0;
  auto Reader=TJsonReaderFactory<>::Create(FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Data.GetData()))));TSharedPtr<FJsonObject> Json;
  if(!FJsonSerializer::Deserialize(Reader,Json)||!Json)continue;
  double Version=0,Sequence=0,Stamp=0;if(!Json->TryGetNumberField(TEXT("version"),Version)||Version!=1||!Json->TryGetNumberField(TEXT("seq"),Sequence)||!Json->TryGetNumberField(TEXT("timestamp"),Stamp))continue;
  if(!FMath::IsFinite(Sequence)||!FMath::IsFinite(Stamp)||Sequence<0||Sequence>9007199254740991.||Stamp<0)continue;
  FString Stream;Json->TryGetStringField(TEXT("streamId"),Stream);
  if((!Stream.IsEmpty()&&Stream!=VisionStreamId)||(!Latest&&Clock-LastReceive>1)){LastSequence=0;LastSwingId=0;LastSenderStamp=0;VisionStreamId=Stream;BaselineSwing=true;}
  if(uint64(Sequence)<=LastSequence||Stamp<LastSenderStamp)continue;LastSequence=uint64(Sequence);LastSenderStamp=Stamp;Latest=Json;
 }
 if(Latest){
  auto Num=[&](const TCHAR* Key,double Min,double Max){double V=0;Latest->TryGetNumberField(Key,V);return FMath::IsFinite(V)?float(FMath::Clamp(V,Min,Max)):0.f;};
  bool T=false,C=false;Latest->TryGetBoolField(TEXT("tracking"),T);Latest->TryGetBoolField(TEXT("calibrated"),C);LastReceive=Clock;
  VisionTracked=T;VisionCalibrated=C;Confidence=Num(TEXT("confidence"),0,1);BodyX=Num(TEXT("bodyX"),-1,1);RacketX=Num(TEXT("racketX"),-1,1);RacketY=Num(TEXT("racketY"),-1,1);
  if(!T){BodyX=RacketX=RacketY=0;Confidence=0;}
  FString Src;Latest->TryGetStringField(TEXT("source"),Src);VisionSynthetic=Src==TEXT("synthetic");
  double Sid=0;Latest->TryGetNumberField(TEXT("swingId"),Sid);if(!FMath::IsFinite(Sid)||Sid<0||Sid>9007199254740991.)Sid=0;bool SwingValid=true;Latest->TryGetBoolField(TEXT("swingValid"),SwingValid);
  if(BaselineSwing)LastSwingId=uint64(Sid);
  if(T&&C&&SwingValid&&Confidence>.3f&&uint64(Sid)>LastSwingId){LastSwingId=uint64(Sid);if(!Menu&&!Paused)Swing(Num(Latest->HasField(TEXT("swingSpeed"))?TEXT("swingSpeed"):TEXT("speed"),.15,1),Num(Latest->HasField(TEXT("swingAim"))?TEXT("swingAim"):TEXT("aimX"),-1,1),Num(Latest->HasField(TEXT("swingSpin"))?TEXT("swingSpin"):TEXT("spin"),-1,1));}
  else LastSwingId=FMath::Max(LastSwingId,uint64(Sid));
  // Sender reports measured inference time. Clock domains are never subtracted.
  VisionLatencyMS=Num(TEXT("inferenceMs"),0,1000);
 }
 VisionAge=Clock-LastReceive;if(VisionAge>.25){VisionTracked=false;BodyX=RacketX=RacketY=0;Confidence=0;}
}
void ACVTennisWorld::StartMatch(bool Practice){Match=Tennis::Score();PracticeMode=Practice;InputIgnoreUntil=Clock+.2f;Demo=FParse::Param(FCommandLine::Get(),TEXT("cvdemo"))||FParse::Param(FCommandLine::Get(),TEXT("cvcapture"))||FParse::Param(FCommandLine::Get(),TEXT("cvrallycapture"));Menu=false;Paused=false;BestRally=0;ServeAttempt=0;GetWorld()->GetFirstPlayerController()->bShowMouseCursor=false;NewPoint();}
void ACVTennisWorld::NewPoint(){
 LastHitter=-1;Bounces=0;Rally=0;Live=false;Serving=true;PointTimer=0;SwingQueued=false;
 ServePending=false;ServeElapsed=0;AimPhase=0;
 for(int I=0;I<2;I++){SwingTimer[I]=0;ServeStroke[I]=false;VolleyStroke[I]=false;PreparedSwingRally[I]=-1;NetPlay[I]=false;DepthGoal[I]=1265;CourtVelocity[I]=FVector2D::ZeroVector;}
 ManualDepth=false;
 CurrentServeBox=((Match.points[0]+Match.points[1])%2)?-1:1;
 if(PracticeMode){Match.server=1;CurrentServeBox=1;}
 PlayerY=Match.server==0?-CurrentServeBox*80:CurrentServeBox*190;
 OpponentY=Match.server==1?CurrentServeBox*80:-CurrentServeBox*170;
 for(int I=0;I<2;I++){const FVector Root((I?1:-1)*Depth[I],I?OpponentY:PlayerY,0);PreviousLocation[I]=Root;Feet[I].reset({Root.X,Root.Y,0},I?-1:1);}
 Ball={{Match.server==0?-11.5:11.5,Match.server==0?PlayerY/100.:OpponentY/100.,2.1},{0,0,0},{0,0,0}};
 Status=PracticeMode?TEXT("RALLY PRACTICE"):Match.server==0?(ServeAttempt?TEXT("SECOND SERVE  /  SWING TO SERVE"):TEXT("YOUR SERVE  /  SWING TO SERVE")):TEXT("RECEIVE SERVE");
}
void ACVTennisWorld::PlaySound(const TCHAR* Asset,float Volume,float Pitch){if(auto* S=SoundCache.FindRef(Asset))UGameplayStatics::PlaySound2D(this,S,Volume,Pitch);}
void ACVTennisWorld::Swing(float Power,float Aim,float Spin){
 if(Menu||Paused||PointTimer>0||Match.winner>=0||ServePending)return;
 if(Live && LastHitter!=0 && SwingTimer[0]>.18){
  QueuedPower=Power;QueuedAim=Aim;QueuedSpin=Spin;SwingQueued=true;SwingWindow=.45;return;
 }
 if(SwingTimer[0]>.18)return;
 if(auto* Anim=Cast<UCVAthleteAnimInstance>(Athletes[0]->GetAnimInstance()))Anim->ClearContactPlan();
 ServeStroke[0]=false;Backhand[0]=(Ball.p.y-PlayerY/100.)<-.12;SwingTimer[0]=CVStroke::Duration;QueuedPower=FMath::Clamp(Power,0.15f,1.f);QueuedAim=FMath::Clamp(Aim,-1.f,1.f);QueuedSpin=FMath::Clamp(Spin,-1.f,1.f);SwingQueued=true;SwingWindow=.30;
 if(Serving&&!Live&&Match.server==0){BeginServe(0,Power,Aim,Spin);return;}
 PlaySound(TEXT("/Game/Audio/Swing.Swing"),.38,.9+Power*.25);
}
void ACVTennisWorld::BeginServe(int Side,float Power,float Aim,float Spin){
 if(ServePending||Live||!Serving||Menu||Paused||PointTimer>0)return;
 ServePending=true;ServeSide=Side;ServeElapsed=0;
 ServePower=FMath::Clamp(Power,.15f,1.f);ServeAim=FMath::Clamp(Aim,-1.f,1.f);ServeSpin=FMath::Clamp(Spin,-1.f,1.f);
 ServeAnchorY=Side?OpponentY:PlayerY;
 const float Direction=Side?-1.f:1.f;
 const auto* Anim=CastChecked<UCVAthleteAnimInstance>(Athletes[Side]->GetAnimInstance());
 const FVector Local=Anim->GetServeContactPoint();
 ServeContactPoint=FVector(-Direction*1240+Direction*Local.X,ServeAnchorY+Direction*Local.Y,Local.Z);
 ServeStroke[Side]=true;Backhand[Side]=false;SwingTimer[Side]=ServeDuration;SwingQueued=false;
 Status=TEXT("TOSS  /  REACH  /  FOLLOW THROUGH");
 UE_LOG(LogTemp,Display,TEXT("CV_EVENT serve_start side=%d"),Side);
}
void ACVTennisWorld::AdvanceServe(float Delta){
 if(!ServePending||Menu||Paused||!FMath::IsFinite(Delta)||Delta<=0)return;
 const float Before=ServeElapsed;ServeElapsed=FMath::Min(ServeContactTime,ServeElapsed+Delta);
 if(Before<.40f&&ServeElapsed>=.40f)PlaySound(TEXT("/Game/Audio/Swing.Swing"),.38,1.1);
 const float Sign=ServeSide? -1.f:1.f;
 const float T=ServeElapsed;
 // A gravity-driven toss meets the authored overhead contact at 550 ms.
 const float TossSpeed=(ServeContactPoint.Z/100.f-1.23f+4.905f*ServeContactTime*ServeContactTime)/ServeContactTime;
 const float Alpha=T/ServeContactTime;
 Ball.p={FMath::Lerp(-Sign*12.4f+Sign*.4f,float(ServeContactPoint.X/100),Alpha),FMath::Lerp(ServeAnchorY/100.f-Sign*.13f,float(ServeContactPoint.Y/100),Alpha),1.23+TossSpeed*T-4.905*T*T};
 Ball.v={0,0,0};
 if(ServeElapsed>=ServeContactTime){ServePending=false;Strike(ServeSide,ServePower,ServeAim,ServeSpin);}
}
void ACVTennisWorld::Strike(int Side,float Power,float Aim,float Spin){
 const double Sign=Side==0?1:-1;const bool IsServe=Serving&&!Live;
 if(SwingTimer[Side]<=0)Backhand[Side]=(Ball.p.y-(Side?OpponentY:PlayerY)/100.)*Sign<-.12;
 Tennis::V3 Start=Ball.p;
 VolleyStroke[Side]=!IsServe&&Bounces==0;
 if(auto* Anim=Cast<UCVAthleteAnimInstance>(Athletes[Side]->GetAnimInstance()))Anim->SetVolley(VolleyStroke[Side]);
 if(IsServe){Backhand[Side]=false;}
 double TargetX=Sign*(IsServe?5.1:Tennis::rallyDepth(Power));
 double TargetY=IsServe?CurrentServeBox*Sign*(2.0+Aim*.75):Aim*3.50;
 double Duration=IsServe?1.07-Power*.12:1.62-Power*.32;
 Ball=IsServe?Tennis::shot(Start,{TargetX,TargetY,Tennis::Radius},Duration,Spin*90*Sign):VolleyStroke[Side]?Tennis::volleyShot(Start,Side,Aim):Tennis::rallyShot(Start,Side,Power,Aim,Spin);
 const int Receiver=1-Side;const auto Receive=Tennis::reception(Ball,Receiver?1:-1,Venue);
 Reception[Receiver]=Position(Receive);
 // Choose the side before positioning: do not run around every left-side ball.
 ReceiveBackhand[Receiver]=(Reception[Receiver].Y-(Receiver?OpponentY:PlayerY))*(Receiver?-1:1)<-25.f;
 if(PreparedSplitRally[Receiver]!=Rally)SplitTimer[Receiver]=.12f;
 if(!IsServe&&Depth[Side]<900)NetPlay[Side]=true;
 LastHitter=Side;Bounces=0;Live=true;Serving=IsServe;PointTimer=0;LastAIStrike=Clock;
 if(Side==0)SwingQueued=false;
 if(!IsServe){
  ServeStroke[Side]=false;
  if(Athletes.IsValidIndex(Side))if(auto* Anim=Cast<UCVAthleteAnimInstance>(Athletes[Side]->GetAnimInstance()))Anim->NotifyRacketContact(Backhand[Side]);
 }LastPower=Power;LastSpeed=Ball.v.length()*3.6;ContactFlash=1;Rally++;BestRally=FMath::Max(BestRally,Rally);
 LastShot=IsServe?TEXT("SERVE"):VolleyStroke[Side]?TEXT("VOLLEY"):Spin>.35?TEXT("TOPSPIN"):Spin<-.25?TEXT("SLICE"):TEXT("DRIVE");
 Status=TEXT("RALLY");PlaySound(TEXT("/Game/Audio/Hit.Hit"),.8,Side?.94:1.06);
 if(!IsServe)UE_LOG(LogTemp,Display,TEXT("CV_COURT_CONTACT side=%d body_depth_cm=%.1f ball_depth_cm=%.1f"),Side,FMath::Abs(Athletes[Side]->GetComponentLocation().X),FMath::Abs(float(Ball.p.x*100)));
 UE_LOG(LogTemp,Display,TEXT("CV_EVENT strike side=%d rally=%d speed=%.1f position=(%.2f,%.2f,%.2f)"),Side,Rally,LastSpeed,Ball.p.x,Ball.p.y,Ball.p.z);
}
void ACVTennisWorld::Point(int Winner,const FString& Why){
 if(PointTimer>0)return;Live=false;Serving=false;ServePending=false;SwingQueued=false;PointTimer=2.5;ScoreFlash=1;
 if(!PracticeMode)Match.point(Winner);
 Status=Why;LastShot=Winner==0?TEXT("YOUR POINT"):TEXT("OPPONENT'S POINT");ServeAttempt=0;
 if(Match.winner>=0){Status=Match.winner==0?TEXT("MATCH WON"):TEXT("MATCH COMPLETE");PointTimer=9999;}
 PlaySound(TEXT("/Game/Audio/Point.Point"),.38,Winner==0?1.1:.85);
 if(Rally>=4)PlaySound(TEXT("/Game/Audio/Applause.Applause"),.30,1.f);
 UE_LOG(LogTemp,Display,TEXT("CV_EVENT point winner=%d reason=%s"),Winner,*Why);
}
void ACVTennisWorld::StepBall(){
 const auto Before=Ball.p;Tennis::flight(Ball,Tennis::Step);
 if(Before.x*Ball.p.x<0&&std::abs(Ball.p.y)<6.5){
  double A=Before.x/(Before.x-Ball.p.x);double Z=Before.z+(Ball.p.z-Before.z)*A;double Y=Before.y+(Ball.p.y-Before.y)*A;
  if(Z<Tennis::netHeight(Y)+Tennis::Radius){
   if(Serving&&!PracticeMode&&ServeAttempt==0){ServeAttempt=1;Live=false;PointTimer=1.8;Status=TEXT("FAULT  /  SECOND SERVE");LastShot=TEXT("NET");}
   else Point(1-LastHitter,Serving?TEXT("DOUBLE FAULT"):TEXT("INTO THE NET"));
   return;
  }
 }
 if(Ball.p.z<=Tennis::Radius&&Ball.v.z<0){
  Tennis::bounce(Ball,Venue);PlaySound(TEXT("/Game/Audio/Bounce.Bounce"),.5,.95+Venue*.025);Bounces++;
  if(Bounces==1){
   const int Receive=LastHitter==0?1:-1;
   bool In=Serving&&!PracticeMode?Tennis::inServiceBox(Ball.p,Receive,CurrentServeBox*Receive):Tennis::inSingles(Ball.p)&&Ball.p.x*Receive>0;
   if(!In){
    if(Serving&&!PracticeMode&&ServeAttempt==0){ServeAttempt=1;Live=false;PointTimer=1.8;Status=TEXT("FAULT  /  SECOND SERVE");LastShot=TEXT("LONG / WIDE");}
    else Point(1-LastHitter,Serving?TEXT("DOUBLE FAULT"):TEXT("OUT"));return;
   }
   Serving=false;
  } else {Point(LastHitter,TEXT("SECOND BOUNCE"));return;}
 }
 // Contact follows each player's actual court position, including net volleys.
 for(int Side=0;Side<2;Side++){
  if(LastHitter==Side||Clock-LastAIStrike<.18f)continue;
  if(!Tennis::canContact(Ball,Side?1:-1,Depth[Side]/100.,(Side?OpponentY:PlayerY)/100.,Serving,Bounces))continue;
  if((Side==0&&(SwingQueued||Demo))||Side==1){PendingContactSide=Side;break;}
 }
 if(std::abs(Ball.p.x)>22||std::abs(Ball.p.y)>14||Ball.p.z<-.4)Point(1-LastHitter,TEXT("OUT"));
}
void ACVTennisWorld::ChangeView(){View=(View+1)%3;}
void ACVTennisWorld::MoveAthletes(float Dt){
 auto* PC=GetWorld()->GetFirstPlayerController();
 if(PC->WasInputKeyJustPressed(EKeys::N)){NetPlay[0]=!NetPlay[0];ManualDepth=false;}
 float Forward=(PC->IsInputKeyDown(EKeys::W)||PC->IsInputKeyDown(EKeys::Up)?1.f:0)-(PC->IsInputKeyDown(EKeys::Down)?1.f:0);
 if(Forward!=0){ManualDepth=true;NetPlay[0]=false;DepthGoal[0]=FMath::Clamp(DepthGoal[0]-Forward*420*Dt,180.f,1450.f);}
 for(int I=0;I<2;I++){
  const bool Receiving=Live&&LastHitter!=I;
  if(I||!ManualDepth){
   DepthGoal[I]=NetPlay[I]?320.f:Receiving?FMath::Clamp(float(FMath::Abs(Reception[I].X)+50),220.f,1390.f):1265.f;
  }
  if((Serving&&!Live&&Match.server==I)||(ServeStroke[I]&&SwingTimer[I]>.35f))DepthGoal[I]=1240;
  float& Y=I?OpponentY:PlayerY;
  const float StrikeOffset=(I?-1.f:1.f)*(ReceiveBackhand[I]?-68.f:68.f);
  float Target=I?(Receiving?float(Reception[I].Y)-StrikeOffset:0):Y;
  if(I==0){
   if(Demo)Target=Receiving?float(Reception[I].Y)-StrikeOffset:0;
   else if(VisionTracked&&VisionCalibrated)Target=BodyX*420;
   else Target=Y+NextAim*100;
  }
  Target=FMath::Clamp(Target,-480.f,480.f);
  float MaxSpeed=SwingTimer[I]>.05f&&SwingTimer[I]<.24f?200.f:440.f;
  FVector2D Desired(FMath::Clamp((DepthGoal[I]-Depth[I])*7,-MaxSpeed,MaxSpeed),FMath::Clamp((Target-Y)*7,-MaxSpeed,MaxSpeed));
  Desired=Desired.GetClampedToMaxSize(MaxSpeed);
  if(ServePending&&ServeSide==I)Desired=FVector2D::ZeroVector;
  CourtVelocity[I]=FMath::Vector2DInterpConstantTo(CourtVelocity[I],Desired,Dt,1700.f);
  Depth[I]=FMath::Clamp(Depth[I]+CourtVelocity[I].X*Dt,180.f,1450.f);
  Y=FMath::Clamp(Y+CourtVelocity[I].Y*Dt,-490.f,490.f);
 }
}
void ACVTennisWorld::EvaluateAthlete(int I,float Dt){
 auto* A=Athletes[I];
 A->TickAnimation(Dt,false);A->RefreshBoneTransforms();Rackets[I]->UpdateComponentToWorld();
}
void ACVTennisWorld::ResolveRacketContact(){
 if(PendingContactSide<0||!Live)return;
 const int I=PendingContactSide;PendingContactSide=-1;
 const auto* A=Cast<UCVAthleteAnimInstance>(Athletes[I]->GetAnimInstance());
 if(!A||!A->IsSwingActive()||A->GetSwingProgress()<.39f||A->GetSwingProgress()>.68f)return;
 const FVector Head=Rackets[I]->GetComponentTransform().TransformPosition(FVector(0,0,47.5));
 const FVector BallPoint=Position(Ball.p);
 // Continuous ball-versus-string-bed test in the racket's local XZ plane.
 // Include BOTH translations over the frame, rather than using a sphere that
 // can award a hit 18 cm in front of the strings.
 const FQuat Face=Rackets[I]->GetComponentQuat();
 const FVector Start=Face.UnrotateVector(ContactBallStart-PreviousRacketHead[I]);
 const FVector End=Face.UnrotateVector(BallPoint-Head);
 const float Denom=Start.Y-End.Y;
 const float Fraction=FMath::Abs(Denom)>.001f?FMath::Clamp(Start.Y/Denom,0.f,1.f):1.f;
 const FVector At=FMath::Lerp(Start,End,Fraction);
 if(FMath::Abs(At.Y)>Tennis::Radius*100+.5f || FMath::Square(At.X/16.55f)+FMath::Square(At.Z/21.25f)>1.f)return;
 const FVector Hit=FMath::Lerp(ContactBallStart,BallPoint,Fraction);
 Ball.p={Hit.X/100.,Hit.Y/100.,Hit.Z/100.};
 UE_LOG(LogTemp,Display,TEXT("CV_MOTION_CONTACT side=%d gap_cm=%.3f plane_cm=%.3f phase=%.4f fraction=%.3f back=%d"),I,At.Size(),FMath::Abs(At.Y),A->GetSwingProgress(),Fraction,Backhand[I]);
 Strike(I,I?.56f:Demo?.57f:QueuedPower,I?FMath::Sin(Clock*1.34)*.55f:Demo?FMath::Sin(Clock*.7)*.55f:QueuedAim,I?.4f:Demo?.46f:QueuedSpin);
 // Continue the outgoing flight for the unused fraction of this same frame.
 Tennis::flight(Ball,ContactFrameDt*(1-Fraction));
 if(!I)DemoHits++;
 Accumulator=0;
}
void ACVTennisWorld::PrepareStroke(int I,bool Back,FVector Contact,float TimeToContact){
 Backhand[I]=Back;ServeStroke[I]=false;GroundContact[I]=Contact;
 const float Speed=CourtVelocity[I].Size();
 const bool Forward=FMath::Abs(CourtVelocity[I].X)>FMath::Abs(CourtVelocity[I].Y)+70;
 MotionStance=Speed>270?TEXT("running"):Forward?TEXT("neutral"):Speed>100?TEXT("semi-open"):TEXT("open");
 Feet[I].stance=(Back?1:-1)*(Forward?.75:Speed>100?.35:0.);
 MotionState=Contact.Z<85?TEXT("low"):Contact.Z>145?TEXT("high"):Speed>270?TEXT("running"):TEXT("drive");
 StrokeRate[I]=FMath::Clamp(CVStroke::SourceDuration*CVStroke::ContactPhase/FMath::Max(.2f,TimeToContact),.65f,1.65f);
 SwingTimer[I]=CVStroke::SourceDuration/StrokeRate[I];
 if(auto* Anim=Cast<UCVAthleteAnimInstance>(Athletes[I]->GetAnimInstance()))
  Anim->PrepareContact(Athletes[I]->GetComponentTransform().InverseTransformPosition(Contact));
}
void ACVTennisWorld::AnimateAthlete(int I,float Dt){
 if(!Athletes.IsValidIndex(I))return;auto* A=Athletes[I];if(!A||!A->GetSkinnedAsset())return;
 const float Y=I?OpponentY:PlayerY;float X=(I?1:-1)*Depth[I];
 PreviousRacketHead[I]=Rackets[I]->GetComponentTransform().TransformPosition(FVector(0,0,47.5));
 A->SetWorldLocation(FVector(X,Y,0));A->SetWorldRotation(FRotator(0,I?180:0,0));
 const FVector Velocity=(A->GetComponentLocation()-PreviousLocation[I])/FMath::Max(Dt,.001f);
 const float LateralSpeed=Velocity.Size();PreviousLocation[I]=A->GetComponentLocation();
 const FVector Root=A->GetComponentLocation();const int BeforeSteps=Feet[I].landings;
 const auto* State=Cast<UCVAthleteAnimInstance>(A->GetAnimInstance());
 Feet[I].load=State&&State->IsSwingActive()?CVStroke::ContactEnvelope(State->GetSwingProgress()):0;
 if(SwingTimer[I]<=0)Feet[I].stance=0;
 // Integrate foot phases along the travelled path even on a long render frame.
 const int FootSteps=FMath::Clamp(FMath::CeilToInt(Dt*120),1,60);
 for(int Step=1;Step<=FootSteps;Step++){
  const FVector P=Root-Velocity*Dt*(1.f-float(Step)/FootSteps);
  Feet[I].update({P.X,P.Y,0},{Velocity.X,Velocity.Y,0},I?-1:1,Dt/FootSteps);
 }
 if(!Menu&&!ServePending&&Feet[I].landings>BeforeSteps){LastFootstep[I]=Clock;PlaySound(TEXT("/Game/Audio/Footstep.Footstep"),I?.10f:.18f,Venue==0?.86f:1.f);}
 SplitTimer[I]=FMath::Max(0.f,SplitTimer[I]-Dt);

 const FQuat ReadyLocal=FRotationMatrix::MakeFromZY(FVector(.2f,.15f,.97f),FVector::ForwardVector).ToQuat();
 FQuat RacketWorld=A->GetComponentQuat()*ReadyLocal;
 {
  if(ServeStroke[I]&&SwingTimer[I]>0){
   const float T=ServeDuration-SwingTimer[I];
   const float Times[]={0,.15,.30,.42,.55,.73,.95,1.2};
   const float Pitches[]={-18,-100,20,155,-18,-100,-80,-18};
   const float Rolls[]={65,20,0,0,0,-45,-60,65};
   float Pitch=-18,Roll=65;
   for(int K=0;K<7;K++)if(T>=Times[K]&&T<=Times[K+1]){
    float Alpha=FMath::Clamp((T-Times[K])/(Times[K+1]-Times[K]),0.f,1.f);Alpha=Alpha*Alpha*(3-2*Alpha);
    Pitch=FMath::Lerp(Pitches[K],Pitches[K+1],Alpha);Roll=FMath::Lerp(Rolls[K],Rolls[K+1],Alpha);break;
   }
   RacketWorld=FRotator(Pitch,I?180:0,Roll).Quaternion();
   if(T<.15f)RacketWorld=FQuat::Slerp(A->GetComponentQuat()*ReadyLocal,RacketWorld,FMath::Clamp(T/.15f,0.f,1.f));
   if(T>.95f)RacketWorld=FQuat::Slerp(RacketWorld,A->GetComponentQuat()*ReadyLocal,FMath::Clamp((T-.95f)/.25f,0.f,1.f));
  }
 }
 if(auto* Anim=Cast<UCVAthleteAnimInstance>(A->GetAnimInstance())){
  const auto& L=Feet[I].feet[0].position;const auto& R=Feet[I].feet[1].position;
  const float ServeFootWeight=ServeStroke[I]&&SwingTimer[I]>0?1-CVStroke::Smooth((ServeDuration-SwingTimer[I])/.16f)*(1-CVStroke::Smooth((.22f-SwingTimer[I])/.22f)):1.f;
  Anim->SetFootPlacement(A->GetComponentTransform().InverseTransformPosition(FVector(L.x,L.y,L.z)),A->GetComponentTransform().InverseTransformPosition(FVector(R.x,R.y,R.z)),ServeFootWeight,(SplitTimer[I]>0?3.f*FMath::Sin(PI*SplitTimer[I]/.20f):0.f),Feet[I].feet[0].yaw,Feet[I].feet[1].yaw,Feet[I].travelYaw,Feet[I].bob,Feet[I].runWeight,Feet[I].armSwing,Feet[I].feet[0].pitch,Feet[I].feet[1].pitch,A->GetComponentTransform().InverseTransformVector(FVector(Feet[I].lean.x,Feet[I].lean.y,0)),Feet[I].brake);
  Anim->SetHoldingBall(Serving&&!Live&&Match.server==I);
  Anim->SetVolley(VolleyStroke[I]);
  const FQuat Rotation=A->GetComponentQuat().Inverse()*RacketWorld;
  const FQuat HandRotation=Rotation*Anim->GetRacketInHand().GetRotation().Inverse();
  const FVector Contact=A->GetComponentTransform().InverseTransformPosition(GroundContact[I]);
  Anim->SetRacketOrientation(Rotation);
  Anim->SetContactTarget(Contact);
  Anim->SetTennisState(LateralSpeed,SwingTimer[I],Backhand[I],StrokeRate[I],ServeStroke[I]);
  Anim->SetTrackedRacketTarget(FVector2D(RacketX,RacketY),I==0&&VisionTracked&&VisionCalibrated?Confidence:0.f);
 }
}
void ACVTennisWorld::Tick(float Dt){
 Super::Tick(Dt);
 const float WorldDt=Dt;
 auto* DebugPC=GetWorld()->GetFirstPlayerController();
 if(DebugPC && DebugPC->WasInputKeyJustPressed(EKeys::F5))DebugPaused=!DebugPaused;
 DebugStep=DebugPC && DebugPC->WasInputKeyJustPressed(EKeys::F6);
 if(DebugPaused&&!DebugStep){MotionDiagnostics(0);return;}
 if(DebugPaused)Dt=1.f/60.f;
 Clock+=Dt;
 if(MotionLab && TickMotionLab(Dt))return;
 if(FParse::Param(FCommandLine::Get(),TEXT("cvselftest"))){
  static int TestPhase=0;static float ContactProgress=0,ContactAnimationTime=0;
  auto* Anim=CastChecked<UCVAthleteAnimInstance>(Athletes[0]->GetAnimInstance());
  const bool TestBackhand=TestPhase>=3;const int Step=TestPhase%3;
  if(Step==0){Anim->SetTennisState(0,0,TestBackhand);Anim->SetTennisState(0,CVStroke::Duration,TestBackhand,1.f);}
  if(Step==1){Anim->SetTennisState(0,.53f,TestBackhand,1.f);Anim->NotifyRacketContact(TestBackhand);}
  Anim->UpdateAnimation(Step==0?.35f:Step==1?.1f:.6f,false,UAnimInstance::EUpdateAnimationFlag::ForceParallelUpdate);
  Anim->ParallelUpdateAnimation();Anim->PostUpdateAnimation();
  if(Step==0){ContactProgress=Anim->GetSwingProgress();ContactAnimationTime=Anim->GetStrokeAnimationTime();checkf(ContactProgress>.3f&&ContactProgress<.5f,TEXT("Groundstroke failed to advance through preparation"));}
  if(Step==1){checkf(Anim->GetSwingProgress()>ContactProgress && Anim->GetSwingProgress()<ContactProgress+.15f,TEXT("Contact event jumped the stroke timeline"));checkf(Anim->GetStrokeAnimationTime()>ContactAnimationTime+.03f,TEXT("Animation graph stopped advancing its source clip"));}
  if(Step==2)checkf(!Anim->IsSwingActive(),TEXT("Groundstroke did not finish recovery"));
  if(++TestPhase==6){
   UE_LOG(LogTemp,Display,TEXT("CV_SELFTEST_PASS timed serve, duplicate gesture, paused toss, single release, no midflight restart, first fault, double fault, second bounce, forehand/backhand contact and recovery, source animation playback"));
   FPlatformMisc::RequestExit(false);
  }
  return;
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("cvmotioncapture"))){
  static int Stage=0;static float HitClock=0;
  if(Stage==0&&Clock>3){StartMatch(true);Demo=true;Serving=false;Live=false;ManualDepth=true;DepthGoal[0]=400;PlayerY=0;OpponentY=0;Stage=1;}
  if(Stage==1&&Clock>3.8f){FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Footwork_A.png"),false,false);UE_LOG(LogTemp,Display,TEXT("CV_MOTION_CAPTURE Footwork_A"));Stage=2;}
  if(Stage==2&&Clock>3.92f){FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Footwork_B.png"),false,false);UE_LOG(LogTemp,Display,TEXT("CV_MOTION_CAPTURE Footwork_B"));Stage=3;}
  if(Clock>3.5f&&Clock<4.5f){
   for(int F=0;F<2;F++){
    const auto& Foot=Feet[0].feet[F];FVector Actual=Athletes[0]->GetSocketLocation(F?TEXT("foot_r"):TEXT("foot_l"));
    UE_LOG(LogTemp,Display,TEXT("CV_FOOT_CHECK foot=%d moving=%d actual=%s target=(%.2f,%.2f,%.2f) error_cm=%.2f"),F,Foot.moving,*Actual.ToString(),Foot.position.x,Foot.position.y,Foot.position.z,FVector::Distance(Actual,FVector(Foot.position.x,Foot.position.y,Foot.position.z)));
   }
  }
  if(Stage==3&&Clock>7){Ball=Tennis::shot({7,0,1.2},{-7,0,Tennis::Radius},.9,0);Live=true;Serving=false;LastHitter=1;Bounces=0;LastAIStrike=-999;Reception[0]=FVector(-350,55,120);Stage=4;}
  if(Stage==4&&VolleyStroke[0]&&LastHitter==0){
   HitClock=Clock;FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Volley_Contact.png"),false,false);
   FVector Head=Rackets[0]->GetComponentTransform().TransformPosition(FVector(0,0,47.5f));
   UE_LOG(LogTemp,Display,TEXT("CV_MOTION_CAPTURE Volley_Contact depth_cm=%.2f gap_cm=%.2f landings=%d"),Depth[0],FVector::Distance(Head,GroundContact[0]),Feet[0].landings);Stage=5;
  }
  if(Stage==5&&Clock-HitClock>.12f){FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Volley_Recovery.png"),false,false);UE_LOG(LogTemp,Display,TEXT("CV_MOTION_CAPTURE Volley_Recovery"));Stage=6;}
  if(Stage==6&&Clock-HitClock>1)FPlatformMisc::RequestExit(false);
  if(Clock>16){UE_LOG(LogTemp,Error,TEXT("CV_MOTION_CAPTURE_FAILED missing volley"));FPlatformMisc::RequestExit(false);}
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("cvgripcapture"))){
  static int Stage=0;static float Started=0;
  Menu=false;Paused=false;Demo=false;Live=false;Serving=false;PlayerY=0;Camera->FieldOfView=40;
  if((Stage==0&&Clock>7.f)||(Stage==3&&Clock>11.f)){
   const bool Back=Stage==3;Started=Clock;Backhand[0]=Back;ServeStroke[0]=false;
   GroundContact[0]=Athletes[0]->GetComponentLocation()+FVector(35,Back?-82.f:82.f,125);
   SwingTimer[0]=Animations[Back?2:1]->GetPlayLength()/1.7f*.48f+Dt;
   CastChecked<UCVAthleteAnimInstance>(Athletes[0]->GetAnimInstance())->NotifyRacketContact(Back);
   ++Stage;
  }else if(Stage==1||Stage==4||((Stage==2||Stage==5)&&Clock-Started>.10f)){
   const bool Back=Stage>=4;const bool Contact=Stage==1||Stage==4;
   const FString Name=FString::Printf(TEXT("Grip_%s_%s.png"),Back?TEXT("Backhand"):TEXT("Forehand"),Contact?TEXT("Contact"):TEXT("Follow"));
   FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/Name,false,false);
   const auto Grip=CVGrip::Build(Cast<USkeletalMesh>(Athletes[0]->GetSkinnedAsset()),false);
   const FTransform LeftRacket=Grip.RacketInHand*Athletes[0]->GetSocketTransform(TEXT("hand_l"));
   const auto Right=CVGrip::Build(Cast<USkeletalMesh>(Athletes[0]->GetSkinnedAsset()),true);
   const FTransform RightRacket=Right.RacketInHand*Athletes[0]->GetSocketTransform(TEXT("hand_r"));
   UE_LOG(LogTemp,Display,TEXT("CV_GRIP_CAPTURE %s dominant_gap_cm=%.3f support_gap_cm=%.3f"),*Name,FVector::Distance(RightRacket.GetLocation(),Rackets[0]->GetComponentLocation()),FVector::Distance(LeftRacket.GetLocation(),Rackets[0]->GetComponentLocation()));
   UE_LOG(LogTemp,Display,TEXT("CV_RENDER_CHECK output=%dx%d screen_percentage=%.1f anti_aliasing=%d"),GetWorld()->GetGameViewport()->Viewport->GetSizeXY().X,GetWorld()->GetGameViewport()->Viewport->GetSizeXY().Y,IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))->GetFloat(),IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"))->GetInt());
   ++Stage;
  }
  if(Stage==6&&Clock>12.f)FPlatformMisc::RequestExit(false);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("cvrallycapture"))){
  static int Phase=0;static float ContactClock=0;
  bool Capture=false;
  if(Phase==0&&Live&&LastHitter==0&&Rally>=4){
   ContactClock=Clock;Capture=true;
   const FVector Center=Rackets[0]->GetComponentTransform().TransformPosition(FVector(0,0,47.5f));
   UE_LOG(LogTemp,Display,TEXT("CV_CONTACT_ALIGNMENT gap_cm=%.2f head=%s contact=%s wrist=%s timer=%.3f progress=%.3f"),FVector::Distance(Center,GroundContact[0]),*Center.ToString(),*GroundContact[0].ToString(),*Athletes[0]->GetSocketLocation(TEXT("hand_r")).ToString(),SwingTimer[0],CastChecked<UCVAthleteAnimInstance>(Athletes[0]->GetAnimInstance())->GetSwingProgress());
  }
  else if(Phase==1&&Clock-ContactClock>.12f)Capture=true;
  else if(Phase==2&&Clock-ContactClock>.65f)Capture=true;
  if(Capture){
   const TCHAR* Names[]={TEXT("Rally_Contact.png"),TEXT("Rally_FollowThrough.png"),TEXT("Rally_Recovery.png")};
   FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/Names[Phase],false,false);
   UE_LOG(LogTemp,Display,TEXT("CV_RALLY_CAPTURE %s"),Names[Phase]);Phase++;
  }
  if((Phase==3&&Clock-ContactClock>2.f)||Clock>30.f)FPlatformMisc::RequestExit(false);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("cvservecapture"))){
  static int Phase=0;
  if(Phase==0&&Clock>8){Swing(.6f,0,.3f);Phase=1;}
  const bool Load=Phase==1&&ServeElapsed>=.28f;
  const bool Contact=Phase==2&&!ServePending&&Live;
  const bool Recover=Phase==3&&Clock>9.f;
  if(Load||Contact||Recover){
   const TCHAR* Name=Load?TEXT("Serve_Load.png"):Contact?TEXT("Serve_Contact.png"):TEXT("Serve_Recovery.png");
   FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/Name,false,false);
   if(Contact){
    const FVector Head=Rackets[0]->GetComponentTransform().TransformPosition(FVector(0,0,47.5f));
    UE_LOG(LogTemp,Display,TEXT("CV_SERVE_ALIGNMENT gap_cm=%.3f head=%s target=%s"),FVector::Distance(Head,ServeContactPoint),*Head.ToString(),*ServeContactPoint.ToString());
   }
   UE_LOG(LogTemp,Display,TEXT("CV_SERVE_CAPTURE %s elapsed=%.3f timer=%.3f progress=%.3f wrist=%s"),Name,ServeElapsed,SwingTimer[0],CastChecked<UCVAthleteAnimInstance>(Athletes[0]->GetAnimInstance())->GetSwingProgress(),*Athletes[0]->GetSocketLocation(TEXT("hand_r")).ToString());Phase++;
  }
  if(Phase==4&&Clock>11.f)FPlatformMisc::RequestExit(false);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("cvcapture"))){
  static int Stage=0;static float SampleStart=0;static TArray<float> FrameTimes;
  if(Clock-SampleStart>3.f)FrameTimes.Add(Dt);
  const float Times[]={12,16,29,33,44,48,59,63,74};
  if(Stage<9&&Clock>Times[Stage]){
   if(Stage==1){Demo=true;StartMatch(true);}else if(Stage==3)SelectVenue(1);else if(Stage==5)SelectVenue(2);else if(Stage==7)SelectVenue(3);
   else{FString Dir=FPaths::ScreenShotDir();IFileManager::Get().MakeDirectory(*Dir,true);FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("Unreal_%d.png"),Stage),true,false);UE_LOG(LogTemp,Display,TEXT("CV_CAPTURE stage=%d fps=%.1f rally=%d best=%d"),Stage,SmoothedFPS,Rally,BestRally);}
   if(Stage%2==0&&FrameTimes.Num()>0){
    double Total=0;for(float Frame:FrameTimes)Total+=Frame;FrameTimes.Sort();
    UE_LOG(LogTemp,Display,TEXT("CV_PERF stage=%d frames=%d average_fps=%.1f p95_ms=%.1f worst_ms=%.1f"),Stage,FrameTimes.Num(),FrameTimes.Num()/Total,FrameTimes[FMath::Min(FrameTimes.Num()-1,int(FrameTimes.Num()*.95f))]*1000,FrameTimes.Last()*1000);
   }
   if(Stage%2==1){FrameTimes.Reset();SampleStart=Clock;}
   Stage++;
  }
  if(Stage>=9&&Clock>77.f&&FParse::Param(FCommandLine::Get(),TEXT("cvexitaftercapture")))FPlatformMisc::RequestExit(false);
 }
SmoothedFPS=FMath::FInterpTo(SmoothedFPS,1.f/FMath::Max(.001f,Dt),Dt,2);PollVision();
 auto* PC=GetWorld()->GetFirstPlayerController();if(!PC)return;
 if(PC->WasInputKeyJustPressed(EKeys::Escape)||PC->WasInputKeyJustPressed(EKeys::M)){Menu=!Menu;Paused=Menu;PC->bShowMouseCursor=Menu;}
 if(PC->WasInputKeyJustPressed(EKeys::Enter)){if(Menu)StartMatch(false);else if(Match.winner>=0)StartMatch(PracticeMode);}
 if(PC->WasInputKeyJustPressed(EKeys::P))StartMatch(true);
 if(PC->WasInputKeyJustPressed(EKeys::V))ChangeView();
 if(PC->WasInputKeyJustPressed(EKeys::One))SelectVenue(0);if(PC->WasInputKeyJustPressed(EKeys::Two))SelectVenue(1);if(PC->WasInputKeyJustPressed(EKeys::Three))SelectVenue(2);if(PC->WasInputKeyJustPressed(EKeys::Four))SelectVenue(3);
 if(PC->WasInputKeyJustPressed(EKeys::F1)){static bool Low=false;Low=!Low;PC->ConsoleCommand(Low?TEXT("r.ScreenPercentage 75"):TEXT("r.ScreenPercentage 100"));PC->ConsoleCommand(Low?TEXT("sg.ShadowQuality 2"):TEXT("sg.ShadowQuality 3"));PC->ConsoleCommand(Low?TEXT("sg.GlobalIlluminationQuality 2"):TEXT("sg.GlobalIlluminationQuality 3"));for(auto* C:Crowd)C->SetVisibility(!Low);}
 float Aim=(PC->IsInputKeyDown(EKeys::Right)||PC->IsInputKeyDown(EKeys::D)?1.f:0)-(PC->IsInputKeyDown(EKeys::Left)||PC->IsInputKeyDown(EKeys::A)?1.f:0);NextAim=Aim;
 if(PC->WasInputKeyJustPressed(EKeys::SpaceBar)||PC->WasInputKeyJustPressed(EKeys::LeftMouseButton)){if(!Menu&&Clock>InputIgnoreUntil)Swing(PC->IsInputKeyDown(EKeys::LeftShift)?.9f:.56f,Aim*.8,PC->IsInputKeyDown(EKeys::S)?-.55:.5);}
 if(!Menu&&!Paused){
  PendingContactSide=-1;ContactBallStart=Position(Ball.p);ContactFrameDt=Dt;
  MoveAthletes(Dt);
  if(PointTimer>0){PointTimer-=Dt;if(PointTimer<=0)NewPoint();}
  else if(Serving&&!Live){
   if(ServePending)AdvanceServe(Dt);
   else{
    const float Sign=Match.server?-1.f:1.f;
    Ball.p={-Sign*12.4+Sign*.40,(Match.server?OpponentY:PlayerY)/100.-Sign*.13,1.23};
    AimPhase+=Dt;if((Match.server==1||Demo)&&AimPhase>2.2){AimPhase=0;BeginServe(Match.server,.6,0,.32);}
   }
  }
  for(int Hitter=0;Hitter<2;Hitter++){
   const auto* A=Cast<UCVAthleteAnimInstance>(Athletes[Hitter]->GetAnimInstance());
   const float Until=ServePending&&ServeSide==Hitter?ServeContactTime-ServeElapsed:A&&A->IsSwingActive()?(CVStroke::ContactPhase-A->GetSwingProgress())*CVStroke::SourceDuration/StrokeRate[Hitter]:-1;
   if(Until>0&&Until<.12f&&PreparedSplitRally[1-Hitter]!=Rally){SplitTimer[1-Hitter]=.20f;PreparedSplitRally[1-Hitter]=Rally;}
  }
  // Prepare an opponent (or demo player's) backswing about one contact-phase
  // duration before arrival. One windup per incoming shot avoids cycling on a miss.
  if(Live&&PreparedSwingRally[1-LastHitter]!=Rally){
   const int Receiver=1-LastHitter;const float Sign=Receiver?1.f:-1.f;
   const auto Predicted=Tennis::predictContact(Ball,Receiver?1:-1,Depth[Receiver]/100.,DepthGoal[Receiver]/100.,CourtVelocity[Receiver].X/100.,Venue,Serving,Bounces);
   const double UntilContact=Predicted.time;
   if(UntilContact>0&&UntilContact<CVStroke::Preparation&&PreparedSwingRally[Receiver]!=Rally){
    PreparedSwingRally[Receiver]=Rally;VolleyStroke[Receiver]=Bounces==0&&!Serving&&Depth[Receiver]<900;
    const bool Back=(Predicted.point.y-(Receiver?OpponentY:PlayerY)/100.)*(-Sign)<-.12;
    PrepareStroke(Receiver,Back,Position(Predicted.point),UntilContact);
   }
  }

  if(Live){Accumulator+=FMath::Min(Dt,.1f);int Steps=0;while(Accumulator>=Tennis::Step&&Steps++<24&&Live){StepBall();Accumulator-=Tennis::Step;}}
  else Accumulator=0;
  if(SwingQueued){SwingWindow-=Dt;if(SwingWindow<=0){SwingQueued=false;LastShot=TEXT("SWING EARLIER / MOVE TO THE BALL");}}
 }
 for(int I=0;I<2;I++){
  if(Athletes.IsValidIndex(I))Athletes[I]->bPauseAnims=Paused;
  if(!Paused&&Dt>0){SwingTimer[I]=FMath::Max(0.f,SwingTimer[I]-Dt);AnimateAthlete(I,Dt);EvaluateAthlete(I,Dt);}
 }
 if(!Menu&&!Paused&&Dt>0)ResolveRacketContact();
 ContactFlash=FMath::Max(0.f,ContactFlash-Dt*3.4f);ScoreFlash=FMath::Max(0.f,ScoreFlash-Dt);
 if(BallMesh){BallMesh->SetWorldLocation(Position(Ball.p));BallMesh->AddLocalRotation(FRotator(Dt*Ball.spin.y*57,Dt*125,0));BallMesh->SetVisibility(!Menu);}
 for(int I=0;I<Trail.Num();I++){Trail[I]->SetVisibility(Live&&Ball.v.length()>12);Trail[I]->SetWorldLocation(Position(Ball.p-Ball.v*((I+1)*.008)));}
 if(TargetMesh){TargetMesh->SetVisibility(Live&&LastHitter==1);Tennis::Ball Prediction=Ball;for(int J=0;J<800&&Prediction.p.z>Tennis::Radius;J++)Tennis::flight(Prediction,Tennis::Step);TargetMesh->SetWorldLocation(FVector(Prediction.p.x*100,Prediction.p.y*100,2));}
 MotionDiagnostics(Dt);
 FVector Cam,Look;
 if(DebugCamera>0){const FVector R=Athletes[0]->GetComponentLocation();Cam=R+(DebugCamera==1?FVector(0,470,180):DebugCamera==2?FVector(-470,0,200):FVector(340,340,220));Look=R+FVector(0,0,100);}
 else if(FParse::Param(FCommandLine::Get(),TEXT("cvgripcapture"))){const bool Back=Clock>9.f;const FVector Root=Athletes[0]->GetComponentLocation();Cam=Root+FVector(240,Back?-330:330,210);Look=Root+FVector(25,Back?-15:15,133);}
 else if(FParse::Param(FCommandLine::Get(),TEXT("cvservecapture"))){Cam=FVector(-1500,-470,265);Look=FVector(-1170,0,145);}
 else if(FParse::Param(FCommandLine::Get(),TEXT("cvrallycapture"))){Cam=FVector(-1765,PlayerY+430,240);Look=FVector(-1265,PlayerY,120);}
 else if(FParse::Param(FCommandLine::Get(),TEXT("cvmotioncapture"))){Cam=FVector(-Depth[0]-400,430,235);Look=FVector(-Depth[0]+30,0,100);}
 else if(Menu){Cam=FVector(-1890,-850,960);Look=FVector(80,180,60);}
 else if(View==1){Cam=FVector(-1640,PlayerY*.24,480);Look=FVector(230,0,70);}
 else if(View==2){Cam=FVector(-1850,-1450,1150);Look=FVector(50,0,30);}
 else{Cam=FVector(-2700,-80+PlayerY*.08,1300);Look=FVector(-100,0,30);}
 if(FParse::Param(FCommandLine::Get(),TEXT("cvgripcapture"))){Camera->SetWorldLocation(Cam);Camera->SetWorldRotation((Look-Cam).Rotation());}
 else {Camera->SetWorldLocation(FMath::VInterpTo(Camera->GetComponentLocation(),Cam,Dt,2.8));Camera->SetWorldRotation(FMath::RInterpTo(Camera->GetComponentRotation(),(Look-Camera->GetComponentLocation()).Rotation(),Dt,3));}
}
FString ACVTennisWorld::PointText(int I)const{
 if(Match.tiebreak)return FString::FromInt(Match.points[I]);int P=Match.points[I],O=Match.points[1-I];if(P>=3&&O>=3)return P>O?TEXT("AD"):P<O?TEXT("40"):TEXT("40");const TCHAR* N[]={TEXT("0"),TEXT("15"),TEXT("30"),TEXT("40")};return N[FMath::Min(P,3)];
}
ACVTennisGameMode::ACVTennisGameMode(){DefaultPawnClass=nullptr;HUDClass=ACVTennisHUD::StaticClass();}
void ACVTennisGameMode::BeginPlay(){Super::BeginPlay();GetWorld()->SpawnActor<ACVTennisWorld>();}
void ACVTennisHUD::Label(const FString& S,float X,float Y,float Size,FLinearColor Color,bool Center){
 FSlateFontInfo Font=GEngine->GetLargeFont()->GetLegacySlateFontInfo();Font.Size=FMath::Max(8.f,Size*Scale*.82f);
 FCanvasTextItem Item(FVector2D(X*Scale,Y*Scale),FText::FromString(S),Font,Color);Item.bCentreX=Center;Item.EnableShadow(FLinearColor(0,0,0,.26),FVector2D(0,1));Canvas->DrawItem(Item);
}
void ACVTennisHUD::Box(float X,float Y,float W,float H,FLinearColor C){DrawRect(C,X*Scale,Y*Scale,W*Scale,H*Scale);}
void ACVTennisHUD::DrawHUD(){
 Super::DrawHUD();if(!Canvas||FParse::Param(FCommandLine::Get(),TEXT("cvservecapture"))||FParse::Param(FCommandLine::Get(),TEXT("cvrallycapture"))||FParse::Param(FCommandLine::Get(),TEXT("cvgripcapture"))||FParse::Param(FCommandLine::Get(),TEXT("cvmotioncapture")))return;if(!Game){TActorIterator<ACVTennisWorld> It(GetWorld());if(It)Game=*It;}if(!Game)return;
 Scale=Canvas->ClipY/900.f;const float W=Canvas->ClipX/Scale;
 if(Game->MotionLab){
  Box(15,12,590,44,FLinearColor(.01,.02,.02,.7));
  Label(FString::Printf(TEXT("%s  |  %s  |  frame %d  |  60 Hz capture"),*Game->MotionCase,*Game->MotionStance,FMath::Max(0,Game->MotionFrame-1)),25,22,17,Cream);
  return;
 }
 const bool Good=Game->VisionTracked&&Game->VisionCalibrated;
 const FString Tracking=Game->VisionSynthetic&&Game->VisionTracked?TEXT("SYNTHETIC INPUT"):Good?TEXT("VISION CONNECTED"):Game->VisionTracked?TEXT("CALIBRATION NEEDED"):TEXT("CAMERA OFFLINE");
 if(Game->Menu){
  Box(0,0,510,900,FLinearColor(.006,.020,.021,.91));Box(45,54,35,3,Mint);Label(TEXT("C O U R T V I S I O N"),94,42,19,Cream);
  Label(TEXT("YOUR BODY."),44,137,58,Cream);Label(TEXT("YOUR GAME."),44,204,58,Cream);
  Label(TEXT("REAL MOVEMENT.  REAL TENNIS."),48,296,16,Mint);
  Label(TEXT("An open court. A camera. You."),48,340,21,Cream);
  Label(TEXT("Move naturally. Find your timing."),48,374,19,Muted);
  Label(TEXT("Swing through the moment."),48,405,19,Muted);
  Label(TEXT("01  /  CHOOSE YOUR SURFACE"),48,482,15,Muted);
  Label(TEXT("ENTER  Play     P  Practice     1-4  Surface"),48,793,14,Muted);
  Label(TEXT("Open VisionBridge to connect your camera."),48,825,14,Muted);
  Label(Game->VenueName(),W-250,740,32,Cream,true);Label(Game->SurfaceName(),W-250,789,14,Cream,true);
 } else {
  Box(34,30,344,117,Ink);Box(34,30,4,117,Mint);Label(TEXT("C O U R T V I S I O N"),53,44,13,Muted);
  Label(TEXT("YOU"),55,74,20,Cream);Label(TEXT("RIVAL"),55,111,20,Cream);
  for(int I=0;I<2;I++){float Y=74+I*37;Label(FString::FromInt(Game->Match.sets[I]),218,Y,20,Muted);Label(FString::FromInt(Game->Match.games[I]),271,Y,20,Cream);Label(Game->PointText(I),332,Y,23,Mint,true);if(Game->Match.server==I)Box(174,Y+8,5,5,Mint);}
  Label(Game->VenueName(),W*.5,34,17,Cream,true);Label(Game->SurfaceName(),W*.5,63,12,Muted,true);
  if(Game->PracticeMode){Box(34,160,180,29,Ink);Label(TEXT("RALLY PRACTICE"),50,166,13,Mint);}
  Box(W-218,30,184,65,Ink);Label(TEXT("RALLY"),W-198,40,12,Muted);Label(FString::Printf(TEXT("%02d"),Game->Rally),W-198,59,26,Cream);Label(FString::Printf(TEXT("BEST %02d"),Game->BestRally),W-112,63,13,Mint);
  if(Game->PointTimer>0||!Game->Live){Box(W*.5-295,170,590,83,Ink);Label(Game->Status,W*.5,186,23,Cream,true);Label(Game->PointTimer>0?Game->LastShot:Game->ServePending?TEXT("RECOVER TO YOUR READY STANCE"):TEXT("SWING YOUR HAND  /  SPACE TO SWING"),W*.5,223,13,Mint,true);}
  Box(34,770,235,82,Ink);Label(Game->LastShot,50,782,14,Muted);Label(FString::Printf(TEXT("%03.0f"),Game->LastSpeed),50,807,31,Cream);Label(TEXT("KM/H"),120,824,12,Mint);
  Label(TEXT("A / D  Sidestep     W / DOWN  Depth     N  Net     SPACE  Swing     S  Slice     V  View     M  Menu"),W*.5,872,13,Cream,true);
  if(Game->ContactFlash>0){FLinearColor C=Mint;C.A=Game->ContactFlash*.8;Box(W*.5-25,735,50,2,C);}
 }
 Box(W-280,824,246,37,Ink);Box(W-263,839,6,6,Good?Mint:FLinearColor(.85,.55,.2,1));Label(Tracking,W-246,834,12,Cream);
 Label(FString::Printf(TEXT("%.0f FPS  /  %s"),Game->SmoothedFPS,Good?*FString::Printf(TEXT("POSE %.0f MS"),Game->VisionLatencyMS):TEXT("KEYBOARD READY")),W-278,878,11,Muted);
}
namespace {
class SCVMenuButtons final : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SCVMenuButtons) {} SLATE_ARGUMENT(TWeakObjectPtr<ACVTennisHUD>, Owner) SLATE_END_ARGS()
 void Construct(const FArguments& Args){
  Owner=Args._Owner;
  Style.SetNormal(FSlateColorBrush(FLinearColor::White));
  Style.SetHovered(FSlateColorBrush(FLinearColor(.82,.92,.82,1)));
  Style.SetPressed(FSlateColorBrush(FLinearColor(.62,.78,.65,1)));
  Style.SetNormalPadding(FMargin(0));Style.SetPressedPadding(FMargin(0));
  TSharedPtr<SCanvas> Layout;
  ChildSlot[SNew(SDPIScaler).DPIScale_Lambda([this](){return ViewScale;})[SAssignNew(Layout,SCanvas)]];
  Add(Layout,TEXT("venue0"),TEXT("GRASS"),48,520,196,0);
  Add(Layout,TEXT("venue1"),TEXT("NEW YORK"),256,520,196,1);
  Add(Layout,TEXT("venue2"),TEXT("CLAY"),48,579,196,2);
  Add(Layout,TEXT("venue3"),TEXT("MELBOURNE"),256,579,196,3);
  Add(Layout,TEXT("match"),TEXT("STEP ON COURT                         >"),48,660,404,-2);
  Add(Layout,TEXT("practice"),TEXT("RALLY PRACTICE"),48,718,404,-1);
 }
 virtual void Tick(const FGeometry& Geometry,double Time,float Delta)override{
  SCompoundWidget::Tick(Geometry,Time,Delta);
  ViewScale=FMath::Max(.1f,Geometry.GetLocalSize().Y/900.f);
 }
 void Add(const TSharedPtr<SCanvas>& Layout,FName Id,const TCHAR* Text,float X,float Y,float Width,int Venue){
  auto Active=[this,Venue](){return Venue==-2||(Venue>=0&&Owner.IsValid()&&Owner->Game&&Owner->Game->Venue==Venue);};
  FSlateFontInfo Font=GEngine->GetLargeFont()->GetLegacySlateFontInfo();Font.Size=14;
  Layout->AddSlot().Position(FVector2D(X,Y)).Size(FVector2D(Width,47))[
   SNew(SButton).ButtonStyle(&Style).ContentPadding(FMargin(18,0)).HAlign(HAlign_Left).VAlign(VAlign_Center)
   .IsFocusable(false).ClickMethod(EButtonClickMethod::MouseDown).AccessibleText(FText::FromString(Text))
   .ButtonColorAndOpacity_Lambda([Active](){return FSlateColor(Active()?Mint:FLinearColor(.018,.042,.043,.95));})
   .OnClicked_Lambda([this,Id](){if(Owner.IsValid())Owner->NotifyHitBoxClick(Id);return FReply::Handled();})[
    SNew(STextBlock).Text(FText::FromString(Text)).Font(Font)
    .ColorAndOpacity_Lambda([Active](){return FSlateColor(Active()?Ink:Cream);})
   ]
  ];
 }
 TWeakObjectPtr<ACVTennisHUD> Owner;
 FButtonStyle Style;
 float ViewScale=.8f;
};
}
void ACVTennisHUD::BeginPlay(){
 Super::BeginPlay();
 if(auto* Viewport=GetWorld()->GetGameViewport()){
  TWeakObjectPtr<ACVTennisHUD> WeakThis(this);
  InputLayer=SNew(SCVMenuButtons).Owner(WeakThis).Visibility_Lambda([WeakThis](){return WeakThis.IsValid()&&WeakThis->Game&&WeakThis->Game->Menu?EVisibility::SelfHitTestInvisible:EVisibility::Hidden;});
  Viewport->AddViewportWidgetContent(InputLayer.ToSharedRef(),100);
 }
}
void ACVTennisHUD::EndPlay(const EEndPlayReason::Type Reason){
 if(InputLayer.IsValid())if(auto* Viewport=GetWorld()->GetGameViewport())Viewport->RemoveViewportWidgetContent(InputLayer.ToSharedRef());
 InputLayer.Reset();Super::EndPlay(Reason);
}
void ACVTennisHUD::NotifyHitBoxClick(FName Id){
 if(!Game)return;UE_LOG(LogTemp,Display,TEXT("CV_UI click=%s"),*Id.ToString());const FString S=Id.ToString();if(S.StartsWith(TEXT("venue")))Game->SelectVenue(FCString::Atoi(*S.Right(1)));else if(Id==TEXT("match"))Game->StartMatch(false);else if(Id==TEXT("practice"))Game->StartMatch(true);
}

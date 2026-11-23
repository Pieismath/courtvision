#pragma once
#include "TennisPhysics.h"
// Code-driven root movement; world-space feet own lift/plant state and yaw.
// Landings are planned from actual velocity. A locked foot is never dragged.
namespace CVFootwork {
using Tennis::V3;
inline double smooth(double x){x=std::clamp(x,0.,1.);return x*x*(3-2*x);}
struct Foot {V3 position,start,landing;double phase=1;bool moving=false;double yaw=0,startYaw=0,landingYaw=0,duration=.20,lift=7,pitch=0,run=0;};
struct Planner {
 Foot feet[2]; V3 previous,previousVelocity,acceleration,lean; bool initialized=false;int next=0,landings=0;double sinceStep=1;
 double stance=0,load=0,bob=0,travelYaw=0,runWeight=0,stridePhase=0,brake=0,armSwing=0; // stance radians in athlete coordinates
 void reset(V3 root,double facing){
  for(int i=0;i<2;i++){feet[i]=Foot{};feet[i].position=root+V3{0,facing*(i?23.:-23.),6.7};}
  previous=root;previousVelocity=acceleration=lean=V3{};brake=armSwing=0;initialized=true;sinceStep=1;next=0;stance=load=bob=travelYaw=runWeight=stridePhase=0;
 }
 void update(V3 root,V3 velocity,double facing,double dt){
  if(!initialized||(root-previous).length()>180)reset(root,facing);
  previous=root;dt=std::clamp(dt,0.,.05);sinceStep+=dt;
  const double speed=std::hypot(velocity.x,velocity.y);
  if(dt<=0)return;
  const V3 rawAcceleration=(velocity-previousVelocity)*(1/dt);
  acceleration=acceleration+(rawAcceleration-acceleration)*(1-std::exp(-14*dt));
  previousVelocity=velocity;
  const double longitudinal=(acceleration.x*velocity.x+acceleration.y*velocity.y)/std::max(30.,speed);
  const double brakeTarget=std::clamp(-longitudinal/1200.,0.,1.)*smooth(speed/100.);
  brake+=(brakeTarget-brake)*(1-std::exp(-12*dt));
  // Mass shift follows acceleration, including the opposite shift on braking.
  V3 leanTarget=acceleration*.0035;double leanLength=std::hypot(leanTarget.x,leanTarget.y);
  if(leanLength>5)leanTarget=leanTarget*(5/leanLength);
  lean=lean+(leanTarget-lean)*(1-std::exp(-10*dt));
  const bool lateral=std::abs(velocity.y)>std::abs(velocity.x)*1.4 && speed>35;
  const int lead=velocity.y*facing>=0?1:0;
  // Shuffle only for small adjustments; sustained travel uses a narrow,
  // direction-facing running gait, including approaches and deep recovery.
  const double runTarget=smooth((speed-160)/160.);
  runWeight+=(runTarget-runWeight)*(1-std::exp(-9*dt));
  double heading=std::atan2(velocity.y*facing,velocity.x*facing);
  if(heading>3.05 && travelYaw<-.1)heading-=6.28318530718;
  if(heading< -3.05 && travelYaw>.1)heading+=6.28318530718;
  // Foot orientation owns recovery. Do not multiply heading by speed: doing
  // so spun the pelvis toward ready while the support shoe still faced away.
  const double desiredYaw=speed>160?heading:stance;
  const double average=feet[0].yaw+std::remainder(feet[1].yaw-feet[0].yaw,6.28318530718)*.5;
  const double bodyTarget=average+std::clamp(std::remainder(desiredYaw-average,6.28318530718),-.40,.40);
  const double turn=std::remainder(bodyTarget-travelYaw,6.28318530718);
  travelYaw+=std::clamp(turn*(1-std::exp(-12*dt)),-4.8*dt,4.8*dt);
  const bool crossover=runWeight>.35;
  const double angle=travelYaw;
  // Predict displacement only as far as the observed braking can carry us.
  // No root speed, acceleration, input or tactical target is modified.
  auto predict=[&](double time){
   double horizon=time;
   if(longitudinal< -80)horizon=std::min(horizon,speed/-longitudinal);
   V3 result=velocity*horizon+acceleration*(.5*horizon*horizon);
   if(speed<20)result=velocity*time;
   return result;
  };
  auto neutralAt=[&](int i){double half=(23-12*runWeight)*(i?1.:-1.);return root+V3{-std::sin(angle)*half*facing,std::cos(angle)*half*facing,6.7};};
  for(auto& foot:feet)if(foot.moving){
   foot.phase=std::min(1.,foot.phase+dt/foot.duration);
   // During acceleration, adapt a FREE shoe's landing to the distance the body
   // will actually travel. Freeze the landing before touchdown.
   if(foot.phase<.75){
    const int index=&foot-feet;
    V3 target=neutralAt(index)+predict((1-foot.phase)*foot.duration+.045);
    if(lateral&&!crossover){
     const auto& other=feet[1-index];const double otherY=(other.moving?other.landing.y:other.position.y)*facing;
     target.y=facing*(index?std::max(target.y*facing,otherY+24):std::min(target.y*facing,otherY-24));
    }
    target.z=6.7;
    foot.landing=foot.landing*(1-std::min(1.,dt*18))+target*std::min(1.,dt*18);
    // Reorient only the free foot and take the shortest angular path. Limit a
    // single redirection step; the next support exchange completes the turn.
    const double targetYaw=foot.startYaw+std::clamp(std::remainder(desiredYaw+(index?.10:-.10)-foot.startYaw,6.28318530718),-1.5,1.5);
    foot.landingYaw+=std::clamp(targetYaw-foot.landingYaw,-6*dt,6*dt);
   }
   double t=smooth(foot.phase),arc=std::sin(foot.phase*3.141592653589793);
   foot.position=foot.start*(1-t)+foot.landing*t;
   foot.position.z=6.7+arc*arc*foot.lift;
   // Heel recovery and dorsiflexion belong to the swing phase. Planted soles
   // retain their world pose; this does not rotate a supposedly locked foot.
   foot.pitch=foot.run*arc*(.72*(1-foot.phase)-.32*foot.phase);
   foot.yaw=foot.startYaw+(foot.landingYaw-foot.startYaw)*t;
   if(foot.phase>=1){foot.position=foot.landing;foot.yaw=foot.landingYaw;foot.moving=false;foot.pitch=0;++landings;}
  }
  bob=0;for(const auto& foot:feet)if(foot.moving)bob+=std::sin(foot.phase*3.141592653589793)*(1.5+runWeight*2);
  const double interval=.25-.035*runWeight;
  const V3 stride=feet[1].position-feet[0].position;
  const double armTarget=std::clamp(facing*(stride.x*std::cos(travelYaw)+stride.y*std::sin(travelYaw))/75.,-1.,1.);
  armSwing+=(armTarget-armSwing)*(1-std::exp(-16*dt));
  stridePhase+=dt/std::max(.3,interval*2);stridePhase-=std::floor(stridePhase);
  int release=-1;double worst=42+runWeight*4;
  if(speed>80)for(int i=0;i<2;i++)if(!feet[i].moving){
   const double reach=std::hypot(feet[i].position.x-root.x,feet[i].position.y-root.y);
   if(reach>worst){worst=reach;release=i;}
  }
  // Acceleration may exhaust a support leg before the steady stride interval.
  // Release that contact into a step; never drag its locked world position.
  if(release>=0)next=release;
  else if(sinceStep<interval || (load>.6&&speed<45))return;
  // Choose the leading shoe at movement onset, then close with the trailing shoe.
  if(release<0&&sinceStep>.32&&lateral)next=lead;
  for(int k=0;k<2;k++){
   int i=(next+k)%2;auto& foot=feet[i];const auto& other=feet[1-i];if(foot.moving)continue;
   V3 neutral=neutralAt(i);
   const double yaw=foot.yaw+std::clamp(std::remainder(desiredYaw+(i?.10:-.10)-foot.yaw,6.28318530718),-1.5,1.5);
   if((neutral-foot.position).length()<10 && std::abs(std::remainder(yaw-foot.yaw,6.28318530718))<.15)continue;
   foot.duration=.28+.025*runWeight-.07*brake;
   foot.run=runWeight;
   foot.lift=7+25*runWeight*(1-.6*brake);
   V3 landing=neutral+predict(.11+.025*runWeight);
   if(lateral&&!crossover){
    // Shuffle feet have disjoint lanes. The trailing shoe may close the stance,
    // but may not travel through the supporting shoe as the old predictor did.
    const double otherY=(other.moving?other.landing.y:other.position.y)*facing;
    landing.y=facing*(i?std::max(landing.y*facing,otherY+24):std::min(landing.y*facing,otherY-24));
   }
   // Avoid an arbitrarily long step on sharp reversals.
   V3 travel=landing-foot.position;double length=std::hypot(travel.x,travel.y);
   if(length>170)landing=foot.position+travel*(170/length);
   foot.start=foot.position;foot.landing=landing;foot.landing.z=6.7;
   foot.startYaw=foot.yaw;foot.landingYaw=yaw;
   foot.phase=0;foot.moving=true;next=1-i;sinceStep=0;break;
  }
 }
};
}

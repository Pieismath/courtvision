#include "../Unreal/Source/CourtVision/TennisPhysics.h"
#include "../Unreal/Source/CourtVision/CVFootwork.h"
#include <cassert>
#include <iostream>
using namespace Tennis;
int main(){
 static_assert(ContactPlane>Baseline && RecoveryDepth>ContactPlane,"Rally contact and stance must remain behind the baseline");
 Ball b{{0,0,2},{0,0,0},{0,0,0}};while(b.p.z>Radius)flight(b,Step);assert(b.v.z<0);double before=b.v.length();bounce(b,1);assert(b.v.z>0&&b.v.length()<before);
 for(int court=0;court<4;court++){Ball n{{0,0,Radius},{20,0,-8},{0,0,0}};bounce(n,court);assert(n.v.x<20&&n.v.z>0&&n.v.z<8);}
 for(int side:{-1,1})for(double power:{.15,.56,.9,1.})for(double aim:{-1.,0.,1.}){
  double d=1.62-power*.32;Ball n=rallyShot({-side*ContactPlane,0,1.0},side==1?0:1,power,aim,.5);double t=0;bool net=false;
  while(t<d){auto prev=n.p;double h=std::min(Step,d-t);flight(n,h);if(prev.x*n.p.x<0){assert(n.p.z>netHeight(n.p.y)+Radius);net=true;}t+=h;}
  assert(net&&std::abs(n.p.x-side*rallyDepth(power))<.05&&std::abs(n.p.z-Radius)<.05);
 }
 for(int court=0;court<4;court++){
  Ball rally=shot({-12.1,-.6,2.65},{5.1,2,Radius},.998,27);int hitter=0;
  for(int hit=0;hit<24;hit++){
   int bounces=0;bool returned=false;
   for(int step=0;step<900;step++){
    auto before=rally.p;flight(rally,Step);
    if(before.x*rally.p.x<0)assert(rally.p.z>netHeight(rally.p.y)+Radius);
    if(rally.p.z<=Radius&&rally.v.z<0){assert(inSingles(rally.p));bounce(rally,court);++bounces;}
    double direction=hitter==0?1:-1;
    if(rally.p.x*direction>ContactPlane&&rally.p.x*direction<13.4&&rally.p.z>.28&&rally.p.z<2.5){
     assert(bounces==1);hitter=1-hitter;returned=true;
     rally=rallyShot(rally.p,hitter,.57,.23,.4);break;
    }
   }
   assert(returned);
  }
 }
 assert(inSingles({11.885,4.115,Radius})&&!inSingles({12,0,Radius}));assert(inServiceBox({5,2,Radius},1,1));assert(!inServiceBox({5,-2,Radius},1,1));
 // A short ball must produce an in-court reception, not a baseline lock.
 for(int court=0;court<4;court++){
  auto shortBall=shot({-12,0,1.1},{3.5,0,Radius},1.6,-20);
  auto receive=reception(shortBall,1,court);assert(receive.x<10&&receive.x>3.5);
 }
 for(int side:{-1,1}){
  Ball incoming{{side*3.5,0,1.2},{side*10.,0,0},{}};
  assert(canContact(incoming,side,4,0,false,0));
  assert(!canContact(incoming,side,4,0,true,0)); // Never volley a serve.
  assert(!canContact(incoming,side,12.65,0,false,0)); // No remote racket hits.
  auto volley=volleyShot(incoming.p,side==1?1:0,.2);bool crossed=false;
  for(int j=0;j<600;j++){
   auto prev=volley.p;flight(volley,Step);
   if(prev.x*volley.p.x<0){assert(volley.p.z>netHeight(volley.p.y)+Radius);crossed=true;}
   if(volley.p.z<=Radius){assert(crossed&&inSingles(volley.p));break;}
  }
 }
 for(V3 velocity:{V3{300,0,0},V3{-300,0,0},V3{0,300,0},V3{0,-300,0}}){
  CVFootwork::Planner feet;V3 root{};feet.reset(root,1);double lift=0;int planted=0;
  for(int f=0;f<240;f++){
   auto old0=feet.feet[0],old1=feet.feet[1];root=root+velocity*(1./120);feet.update(root,velocity,1,1./120);
   for(int i=0;i<2;i++){
    auto old=i?old1:old0;auto now=feet.feet[i];lift=std::max(lift,now.position.z);
    if(!old.moving&&!now.moving){assert((old.position-now.position).length()<1e-8);planted++;}
    assert(now.position.z>=6.699&&std::isfinite(now.position.x));
   }
  }
  assert(feet.landings>=8&&lift>14&&planted>20);
 }
 // Small adjustments remain shuffles; covering court distance must enter a
 // running gait with heel recovery and a pelvis facing actual travel.
 for(V3 velocity:{V3{440,0,0},V3{-440,0,0},V3{0,440,0},V3{0,-440,0}}){
  CVFootwork::Planner gait;V3 root{};gait.reset(root,1);double peak=0;
  for(int f=0;f<360;f++){root=root+velocity*(1./120);gait.update(root,velocity,1,1./120);for(const auto& foot:gait.feet)peak=std::max(peak,foot.position.z);}
  assert(gait.runWeight>.95&&peak>30);
  const V3 heading{std::cos(gait.travelYaw),std::sin(gait.travelYaw),0};
  assert(heading.x*velocity.x+heading.y*velocity.y>430);
 }
 {CVFootwork::Planner gait;V3 root{};double peak=0;for(int f=0;f<240;f++){root.y+=80./120;gait.update(root,{0,80,0},1,1./120);for(const auto& foot:gait.feet)peak=std::max(peak,foot.position.z);}assert(gait.runWeight<.01&&peak<16);}
 // Arrival prediction must include a bounce; compare to independent rollout.
 for(int court=0;court<4;court++)for(int receiver:{-1,1}){
  auto incoming=shot({-receiver*12.1,0,2.43},{receiver*5.1,receiver*2.,Radius},.998,receiver*28.8);
  auto predicted=predictContact(incoming,receiver,12.65,12.65,0,court,true,0);
  assert(predicted.time>0);
  double elapsed=0;int bounces=0;
  while(incoming.p.x*receiver<12.30){flight(incoming,Step);elapsed+=Step;if(incoming.p.z<=Radius&&incoming.v.z<0){bounce(incoming,court);++bounces;}}
  assert(bounces==1&&std::abs(predicted.time-elapsed)<=Step+1e-6);
  assert(std::abs(predicted.point.x*receiver-12.30)<.001);
 }
 // A planted shoe must retain its yaw as the body accelerates and reverses.
 for(int facing:{-1,1}){
  CVFootwork::Planner feet;V3 root{},velocity{};feet.reset(root,facing);
  for(int f=0;f<1440;f++){
   const double goal=(f/240)%2?440.:-440.;velocity.y+=std::clamp(goal-velocity.y,-1700./120,1700./120);
   auto old0=feet.feet[0],old1=feet.feet[1];root=root+velocity*(1./120);feet.update(root,velocity,facing,1./120);
   for(int i=0;i<2;i++){const auto& old=i?old1:old0;const auto& now=feet.feet[i];if(!old.moving&&!now.moving){assert((old.position-now.position).length()<1e-8);assert(std::abs(old.yaw-now.yaw)<1e-8);}}
  }
 }
 // Run, brake to a held stop, then redirect. Recovery must settle rather
 // than perpetually stepping; the torso cannot unwind ahead of both shoes.
 for(int facing:{-1,1}){
  CVFootwork::Planner gait;V3 root{},v{};gait.reset(root,facing);double maxBrake=0;
  for(int frame=0;frame<960;frame++){
   double goal=frame<180?-440:frame<420?0:frame<600?440:0;
   v.y+=std::clamp(goal-v.y,-1700./120,1700./120);root=root+v*(1./120);
   const double oldYaw=gait.travelYaw;gait.update(root,v,facing,1./120);
   maxBrake=std::max(maxBrake,gait.brake);
   assert(std::abs(std::remainder(gait.travelYaw-oldYaw,2*3.141592653589793))<=4.8/120+1e-8);
   for(const auto& foot:gait.feet)assert(std::isfinite(foot.yaw)&&std::isfinite(foot.position.z));
   if(frame==419||frame==959){assert(!gait.feet[0].moving&&!gait.feet[1].moving);assert(std::abs(std::remainder(gait.travelYaw,2*3.141592653589793))<.12);}
  }
  assert(maxBrake>.35);
 }
 Score s;for(int i=0;i<3;i++){s.point(0);s.point(1);}s.point(0);assert(s.games[0]==0);s.point(1);assert(s.points[0]==s.points[1]);s.point(0);s.point(0);assert(s.games[0]==1&&s.server==1);
 Score tb;tb.games[0]=tb.games[1]=5;for(int i=0;i<4;i++)tb.point(0);for(int i=0;i<4;i++)tb.point(1);assert(tb.tiebreak);for(int i=0;i<6;i++){tb.point(0);tb.point(1);}tb.point(0);assert(tb.tiebreak);tb.point(0);assert(!tb.tiebreak&&tb.sets[0]==1);
 Score m;for(int set=0;set<2;set++)for(int game=0;game<6;game++)for(int p=0;p<4;p++)m.point(0);assert(m.winner==0);m.point(1);assert(m.points[1]==0);
 std::cout<<"PASS: planted-foot invariance, stepping in four directions, directional run versus shuffle, short-ball approach, legal net volleys, drag trajectories, spin, net clearance, four surface bounces, 96 baseline returns after one bounce, line calls, service boxes, deuce, tiebreak, best-of-three scoring\n";
}

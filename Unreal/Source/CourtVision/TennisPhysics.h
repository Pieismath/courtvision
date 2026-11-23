#pragma once
#include <cmath>
#include <algorithm>

namespace Tennis {
struct V3 {
 double x=0,y=0,z=0;
 V3 operator+(V3 b)const{return{x+b.x,y+b.y,z+b.z};}
 V3 operator-(V3 b)const{return{x-b.x,y-b.y,z-b.z};}
 V3 operator*(double s)const{return{x*s,y*s,z*s};}
 double length()const{return std::sqrt(x*x+y*y+z*z);}
};
inline V3 cross(V3 a,V3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
struct Surface {double restitution,friction;};
inline Surface surface(int n){return n==0?Surface{.68,.22}:n==2?Surface{.77,.34}:Surface{.74,.25};}
struct Ball {V3 p,v,spin;};
constexpr double Radius=.0335;
constexpr double Step=1.0/240.0;
// Metres from the net: rally contact and recovery stay behind the baseline.
constexpr double Baseline=11.885;
constexpr double ContactPlane=12.15;
constexpr double RecoveryDepth=12.65;
inline double rallyDepth(double power){return 7.5+power*1.2;}
inline void flight(Ball& b,double dt){
 const V3 a=V3{0,0,-9.81}+b.v*(-.018*b.v.length())+cross(b.spin,b.v)*.0012;
 b.p=b.p+b.v*dt+a*(.5*dt*dt); b.v=b.v+a*dt;
 b.spin=b.spin*std::exp(-.08*dt);
}
inline void bounce(Ball& b,int court){
 auto s=surface(court); const double normal=-b.v.z;
 const double speed=std::hypot(b.v.x,b.v.y);
 const double factor=std::max(.45,1.0-s.friction*(1+s.restitution)*normal/std::max(.1,speed));
 b.p.z=Radius; b.v.x*=factor; b.v.y*=factor; b.v.z=normal*s.restitution;
 b.v.x+=std::clamp(b.spin.y*.001,-.28,.28); b.v.y-=std::clamp(b.spin.x*.001,-.28,.28);
 b.spin=b.spin*.78;
}
inline double netHeight(double y){return .914+.156*std::pow(std::min(1.,std::abs(y)/6.4),1.65);}
inline bool inSingles(V3 p){return std::abs(p.x)<=11.885+Radius && std::abs(p.y)<=4.115+Radius;}
inline bool inServiceBox(V3 p,int receiver,int box){return p.x*receiver>0 && p.x*receiver<=6.40+Radius && p.y*box>=-Radius && p.y*box<=4.115+Radius;}
// Solve an actual drag/Magnus trajectory for a desired first-bounce location.
inline Ball shot(V3 start,V3 target,double duration,double topspin){
 Ball initial; initial.p=start; initial.spin={0,topspin,0};
 initial.v={(target.x-start.x)/duration,(target.y-start.y)/duration,(target.z-start.z)/duration+4.905*duration};
 for(int j=0;j<7;j++){
  Ball trial=initial; double t=0; while(t<duration){double h=std::min(Step,duration-t);flight(trial,h);t+=h;}
  auto error=target-trial.p;
  initial.v=initial.v+error*(1.12/duration);
 }
 return initial;
}
inline Ball rallyShot(V3 start,int hitter,double power,double aim,double spin){
 const double sign=hitter==0?1:-1;
 return shot(start,{sign*rallyDepth(power),aim*3.5,Radius},1.62-power*.32,spin*90*sign);
}
// Choose a reachable-height reception after the first bounce; deep balls keep
// baseline positioning while short balls bring the receiver into the court.
inline V3 reception(Ball ball,int receiver,int court){
 V3 best{receiver*ContactPlane,ball.p.y,1};double cost=1e9;int bounces=0;
 for(int i=0;i<1000;i++){
  flight(ball,Step);
  if(ball.p.z<=Radius&&ball.v.z<0){if(++bounces==2)break;bounce(ball,court);}
  if(bounces==1&&ball.p.x*receiver>1.5&&ball.p.z>.65&&ball.p.z<1.7){
   double score=std::abs(ball.p.x*receiver-ContactPlane);
   if(score<cost){cost=score;best=ball.p;}
  }
 }
 return best;
}
// Predict arrival using the actual drag/spin/bounce integration. A constant
// velocity estimate starts a swing too early when a bounce lies ahead.
struct ContactPrediction {V3 point;double time=-1;};
inline ContactPrediction predictContact(Ball ball,int receiver,double depth,double goal,double velocity,int court,bool serving,int bounces){
 for(int i=1;i<=600;i++){
  const V3 before=ball.p;const double oldDepth=depth;
  const double wanted=std::clamp((goal-depth)*7.,-4.4,4.4);
  velocity+=std::clamp(wanted-velocity,-17.*Step,17.*Step);depth+=velocity*Step;
  flight(ball,Step);
  if(ball.p.z<=Radius&&ball.v.z<0){if(++bounces>1)return {};bounce(ball,court);}
  const double a=before.x*receiver-(oldDepth-.35),b=ball.p.x*receiver-(depth-.35);
  if(a<=0&&b>=0){
   const bool valid=bounces==1||(!serving&&depth<9.);
   if(!valid)return {};
   const double t=std::clamp(-a/(b-a),0.,1.);
   return {before+(ball.p-before)*t,(i-1+t)*Step};
  }
 }
 return {};
}
inline bool canContact(Ball ball,int receiver,double bodyDepth,double lateral,bool serving,int bounces){
 const double ahead=bodyDepth-ball.p.x*receiver;
 const bool volley=bounces==0&&!serving&&bodyDepth<9.;
 return (bounces==1||volley)&&ahead>-.25&&ahead<.52&&
  std::abs(ball.p.y-lateral)<1.05&&ball.p.z>.35&&ball.p.z<2.35;
}
inline Ball volleyShot(V3 start,int hitter,double aim){
 double direction=hitter==0?1:-1;
 double duration=.62+std::abs(direction*8.8-start.x)*.028;
 return shot(start,{direction*8.8,aim*3.2,Radius},duration,-18*direction);
}
struct Score {
 int points[2]={0,0},games[2]={0,0},sets[2]={0,0};
 int server=0; bool tiebreak=false; int tieFirstServer=0; int winner=-1;
 void point(int p){
  if(winner>=0)return;
  ++points[p]; int o=1-p;
  if(tiebreak){
   if(points[p]>=7&&points[p]-points[o]>=2){++games[p];winSet(p);server=1-tieFirstServer;tiebreak=false;}
   else {int played=points[0]+points[1];server=((played+1)/2)%2?1-tieFirstServer:tieFirstServer;}
  } else if(points[p]>=4&&points[p]-points[o]>=2){
   ++games[p]; points[0]=points[1]=0;server=1-server;
   if(games[p]>=6&&games[p]-games[o]>=2)winSet(p);
   else if(games[0]==6&&games[1]==6){tiebreak=true;tieFirstServer=server;}
  }
 }
 void winSet(int p){++sets[p];games[0]=games[1]=points[0]=points[1]=0;if(sets[p]>=2)winner=p;}
};
}

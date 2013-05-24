
#ifndef MACROSEARCH_H
#define MACROSEARCH_H

#include <vector>
#include <deque>
#include <queue>  //for priority_queue
//#include "BWSAL/BuildType.h"
#include <BWAPI/UnitType.h>

const int FramePerMinute = 24*60;   //fastest game speed    //see BWAPI::Game.getFPS()


//typedef BWSAL::BuildType MoveType;
typedef BWAPI::UnitType  MoveType;

class QueuedMove
{
public:
   QueuedMove(int start, int end, int prev, MoveType aMove) : frameStarted(start), frameComplete(end), prevFrameComplete(prev), move(aMove) {}
   int frameStarted;
   int frameComplete;
   int prevFrameComplete;
   MoveType move;
};


class MacroSearch
{
public:

   MacroSearch();
   ~MacroSearch(){}

   //recursive
   std::vector<QueuedMove> FindMoves(int targetFrame);
   std::vector<MoveType> PossibleMoves(int targetFrame); //given the current game state
   void DoMove(MoveType aMove);
   void UndoMove(int previousFrame);

   int EvaluateState(); //provides a number to represent the "value" of the current game state

private:

   void FindMovesRecursive(int targetFrame);
   void ProcessQueuedEventsUntil(int targetFrame);

   int mMaxScore;
   std::vector<QueuedMove> mBestMoves;

   ////////// game state variables //////////
   //int CurrentGameFrame;
   int  mGameStateFrame;
   bool mForceAttacking; //true = attacking their base, false = defending ours (or sitting still)
   int  mMinerals;
   int  mGas;
   int  mCurrentFarm;
   int  mFarmCapacity;
   int  mMineralsPerMinute;  //resource rate
   int  mMaxTrainingCapacity;   //max farm worth of units can be trained at any given time
   std::vector<int> mUnitCounts; // 0=probe, 1=zealot, 2=dragoon, 3=templar, 4=dark templar, 5=archon, 6=dark archon, 7=observer, 8=shuttle, 9=reaver, 10=corsair, 11=scout, 12=carrier

   std::vector< std::vector<int> > mTrainingCompleteFrames;  //0=nexus, 1=pylon, 2=gateways, 3=assimilator, 4=cybercore, 5=forge, 6=robotfacility, 7=stargate, 8=citadel, 9=templar, 10=observatory, 11=arbiter, 12=fleetbeacon,  13=robotsupportbay

   std::vector<QueuedMove> mMoveStack;          //for undoing moves as tree is climbed back up
   std::priority_queue<QueuedMove> mEventQueue; //for processing building/training moves as game-tree is searched
   ////////// end game state variables //////////


   //manage when tech buildings finish (gateway, cybercore, citadel, etc...)
   //std::map<int, int>  mBuildingsComplete;   //Nexus=80, Robotics_Facility=81, Pylon=82, Assimilator=83, Observatory=84, Gateway=85, Photon_Cannon=86, Citadel_of_Adun=87, Cybernetics_Core=88, Templar_Archives=89, Forge=90, Stargate=91, Fleet_Beacon=92, Arbiter_Tribunal=93, Robotics_Support_Bay=94, Shield_Battery=95
};


#endif

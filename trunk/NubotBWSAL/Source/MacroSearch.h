
#include <vector>
#include <deque>
#include <queue>  //for priority_queue

#include "BWSAL/BuildType.h"

const int FramePerMinute = 28*60;


//typedef BWSAL::BuildType MoveType;
typedef BWAPI::UnitType  MoveType;

struct QueuedMove
{
   QueuedMove(int start, int end, int prev, MoveType aMove) : frameStarted(start), frameComplete(end), prevFrameComplete(prev), move(aMove) {}
   int frameStarted;
   int frameComplete;
   int prevFrameComplete;
   MoveType move;
};
bool operator< (const QueuedMove& a, const QueuedMove& b)
{
   return (a.frameComplete > b.frameComplete);
}
bool operator== (const QueuedMove& a, const QueuedMove& b)
{
   return (a.move.getID() == b.move.getID() &&
           a.frameComplete == b.frameComplete &&); //should be enough to check this, if type same & complete-time the same, then start time the same.  which bldg shouldn't matter
}


class MacroSearch
{
public:

   MacroSearch();
   ~MacroSearch(){}

   //recursive
   std::vector<MoveType> FindMoves(int currentFrame, int targetFrame);
   std::vector<MoveType> PossibleMoves(); //given the current game state
   void DoMove(MoveType aMove);
   void UndoMove(int previousFrame);

   int EvaluateState(); //provides a number to represent the "value" of the current game state

private:

   int cAttackId; //300


   ////////// game state variables //////////
   //int CurrentGameFrame;
   int GameStateFrame;
   bool ForceAttacking; //true = attacking their base, false = defending ours (or sitting still)
   int Minerals;
   int Gas;
   int CurrentFarm;
   int FarmCapacity;
   int MineralsPerMinute;  //resource rate
   int MaxTrainingCapacity;   //max farm worth of units can be trained at any given time
   std::vector<int> mUnitCounts; // 0=probe, 1=zealot, 2=dragoon, 3=templar, 4=dark templar, 5=archon, 6=dark archon, 7=observer, 8=shuttle, 9=reaver, 10=corsair, 11=scout, 12=carrier
   std::vector< std::vector<int> > mTrainingCompleteFrames;  //0=nexus, 1=gateways, 2=robotics facility, 3=stargate
   std::deque<QueuedMove> mMoveStack;           //for undoing moves as tree is climbed back up
   std::priority_queue<QueuedMove> mEventQueue; //for processing building/training moves as game-tree is searched
   ////////// end game state variables //////////
};



#include "MacroSearch.h"
//#include "BWSAL.h"
#include <algorithm>
#include <assert.h>

#define cATTACK_ID 300

  #ifndef max
    #define max std::max
  #endif
  #ifndef min
    #define min std::min
  #endif


QueuedMove::MemoryPool QueuedMove::mem; // instance of the static memory pool

template <typename T, unsigned int capacity>
MemoryPoolClass<T,capacity>::MemoryPoolClass()
   :  myUnusedIndex(capacity-1)
     ,myMemory(capacity * sizeof(T))
   //,myUnusedMemory[capacity]
{
   for (unsigned int i = 0; i < capacity; i++)
   {
      myUnusedMemory[i] = (T*)&myMemory[i*sizeof(T)];
   }
}


template <typename T, unsigned int capacity>
void* MemoryPoolClass<T,capacity>::alloc(size_t n)
{
   if ((sizeof(T) != n) || myUnusedIndex < 0)
   {
      throw std::bad_alloc();
   }
   else
   {
      return myUnusedMemory[myUnusedIndex--];
   }
}

template <typename T, unsigned int capacity>
void MemoryPoolClass<T,capacity>::free(void * ptr)
{
   if (myUnusedIndex < (capacity-1))
   {
      myUnusedMemory[++myUnusedIndex] = (T*)ptr;
   }
   else
   {
      throw std::exception("deleting too many from memory pool!");
   }
}



namespace
{
   int sDistanceToEnemy = 128*8;   //WAG (unit: pixels)
}

bool operator< (const QueuedMove& a, const QueuedMove& b)
{
   return (a.frameComplete > b.frameComplete ||
          a.prevFrameComplete > b.prevFrameComplete ||
         a.frameStarted > a.frameStarted);
}
bool operator== (const QueuedMove& a, const QueuedMove& b)
{
 //return (a.move.getID()  == b.move.getID() &&
   return (a.move          == b.move         &&
           a.frameComplete == b.frameComplete); //should be enough to check this, if type same & complete-time the same, then start time the same.  which bldg shouldn't matter
}
bool operator!= (const QueuedMove& a, const QueuedMove& b)
{
 //return (a.move.getID()  != b.move.getID() ||
   return (a.move          != b.move         ||
           a.frameComplete != b.frameComplete); //should be enough to check this, if type same & complete-time the same, then start time the same.  which bldg shouldn't matter
}


MacroSearch::MacroSearch()
   :
   //,CurrentGameTime(0.0)
   //,GameStateTime(0.0)
   //,CurrentGameFrame(0)
    mGameStateFrame(0)
   ,mForceAttacking(false)
   ,mForceMoving(false)
   ,mMinerals(50)                 //default starting mMinerals (enough for 1 worker)
   ,mGas(0)
   ,mCurrentFarm(4*2)             //4 starting probes
   ,mFarmCapacity(9*2)            //default starting farm capacity (9)
   ,mFarmBuilding(0)              //number of supply things that are building (pylon, overlord, depo)
   ,mMineralsPerMinute(4*42)      //default starting rate (4 workers * 42/minute)
   ,mMaxTrainingCapacity(2)       //nexus (1 probe at a time)
   ,mUnitCounts(12, 0)            //only 12 kinds of units?
   ,mTrainingCompleteFrames(14, std::vector<int>())   // 14 kinds of buildings?
   ,mMoveStack()
   ,mEventQueue()
   ,mSearchDepth(0)
 //,mPossibleMoves(30, std::vector<MoveType>())   //initialize with a possible depth of 30
   ,mPossibleMoves(30, std::vector<int>())   //initialize with a possible depth of 30
{
   mTrainingCompleteFrames[0].push_back(0);  //start with 1 nexus
   mUnitCounts[0] = 4;  //4 starting probes
   mEventQueue.clear();
 //mEventQueue.push_front(QueuedMove(2147483647, 2147483647, 2147483647, 2147483647, MoveType(-1)));   //must have empty (max int) node at the beginning, useful later
 //mEventQueue.push_front(QueuedMove(-1, -1, -1, -1, MoveType(-1)));   //must have empty (-1) node at the beginning, useful later
   mEventQueue.push_front(new QueuedMove(-1, -1, -1, -1, eNull_Move));   //must have empty (-1) node at the beginning, useful later
   mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
                                          //mLastEventIter points to the last event processed, should be in sync with game time
                                          //events complete at later times are towards the front of the queue (list)
}

std::vector<QueuedMove*> MacroSearch::FindMoves(int targetFrame)
{
   //BWAPI::UnitTypes::Protoss_Corsair;        //60
   //BWAPI::UnitTypes::Protoss_Dark_Templar;   //61
   //BWAPI::UnitTypes::Protoss_Dark_Archon;    //63
   //BWAPI::UnitTypes::Protoss_Probe;          //64
   //BWAPI::UnitTypes::Protoss_Zealot;         //65
   //BWAPI::UnitTypes::Protoss_Dragoon;        //66
   //BWAPI::UnitTypes::Protoss_High_Templar;   //67
   //BWAPI::UnitTypes::Protoss_Archon;         //68
   //BWAPI::UnitTypes::Protoss_Shuttle;        //69
   //BWAPI::UnitTypes::Protoss_Scout;          //70
   //BWAPI::UnitTypes::Protoss_Arbiter;        //71
   //BWAPI::UnitTypes::Protoss_Carrier;        //72
   //BWAPI::UnitTypes::Protoss_Interceptor;    //73
   //BWAPI::UnitTypes::Protoss_Reaver;         //83
   //BWAPI::UnitTypes::Protoss_Observer;       //84
   //BWAPI::UnitTypes::Protoss_Scarab;         //85

   //BWAPI::UnitTypes::Protoss_Nexus;                //154
   //BWAPI::UnitTypes::Protoss_Robotics_Facility;    //155
   //BWAPI::UnitTypes::Protoss_Pylon;                //156
   //BWAPI::UnitTypes::Protoss_Assimilator;          //157
   //BWAPI::UnitTypes::Protoss_Observatory;          //159
   //BWAPI::UnitTypes::Protoss_Gateway;              //160
   //BWAPI::UnitTypes::Protoss_Photon_Cannon;        //162
   //BWAPI::UnitTypes::Protoss_Citadel_of_Adun;      //163
   //BWAPI::UnitTypes::Protoss_Cybernetics_Core;     //164
   //BWAPI::UnitTypes::Protoss_Templar_Archives;     //165
   //BWAPI::UnitTypes::Protoss_Forge;                //166
   //BWAPI::UnitTypes::Protoss_Stargate;             //167
   //BWAPI::UnitTypes::Special_Stasis_Cell_Prison;   //168
   //BWAPI::UnitTypes::Protoss_Fleet_Beacon;         //169
   //BWAPI::UnitTypes::Protoss_Arbiter_Tribunal;     //170
   //BWAPI::UnitTypes::Protoss_Robotics_Support_Bay; //171
   //BWAPI::UnitTypes::Protoss_Shield_Battery;       //172

   mMaxScore = 0;
   mBestMoves.clear();
   mSearchDepth = -1;
   FindMovesRecursive(targetFrame);
   return mBestMoves;
}


void MacroSearch::FindMovesRecursive(int targetFrame)
{
   //std::vector<MoveType> moves = PossibleMoves(targetFrame);
   mSearchDepth++;
   //std::vector<MoveType>* movesPtr = UpdatePossibleMoves();
   std::vector<int>* movesPtr = UpdatePossibleMoves();

   //std::vector<MoveType>::iterator it;
   std::vector<int>::iterator it;
   for (it=movesPtr->begin(); it!=movesPtr->end(); it++)
   {
      //MoveType move = *it;
      int move = *it;
      if (DoMove(move, targetFrame))
      {
         FindMovesRecursive(targetFrame);
         //if (mGameStateFrame < targetFrame)
         //{
         //   FindMovesRecursive(targetFrame);
         //}
         UndoMove(); //undo last move on the stack
      }
      else
      {
         //save off this current game state as a leaf node of the DFS game tree search
         int score = EvaluateState();
         if (score > mMaxScore)
         {
            mMaxScore = score;
            mBestMoves = mMoveStack;
         }
      }
   }
   mSearchDepth--;
   return;
}

////given the current game state
//std::vector<MoveType> MacroSearch::PossibleMoves(int targetFrame)
//{
//   std::vector<MoveType> moves;
//
//   if (mCurrentFarm < (mFarmCapacity-1+16*mFarmBuilding) &&  //have (or will have) farm available
//       mTrainingCompleteFrames[2].size() >0) //and have a gateway to build it in
//   {
//      moves.push_back(BWAPI::UnitTypes::Protoss_Zealot);
//   }
//
//   if (mTrainingCompleteFrames[1].size()>0 || mFarmBuilding>0 || mFarmCapacity > 18)  //need pylon
//   {
//      moves.push_back(BWAPI::UnitTypes::Protoss_Gateway);
//   }
//
//   if ((mTrainingCompleteFrames[1].size()==0) ||
//       ((mFarmCapacity+16*mFarmBuilding-mCurrentFarm) <= (2*mMaxTrainingCapacity)) )
//   {
//      moves.push_back(BWAPI::UnitTypes::Protoss_Pylon);
//   }
//
//   //TODO: count mineral spots, for now... use 8
//   if (mUnitCounts[0] < 16 && //two per mineral spot
//       mCurrentFarm < (mFarmCapacity-1+16*mFarmBuilding) &&   //and have (or will have) farm available
//       mTrainingCompleteFrames[0].size() >0) //and have a nexus to build it in
//   {
//      moves.push_back(BWAPI::UnitTypes::Protoss_Probe);
//   }
//
//   if (mUnitCounts[1] > 1 && mForceMoving == false)   //dont do attack move again, if already moving
//   {
//      moves.push_back( BWAPI::UnitType(cATTACK_ID) );  //cATTACK_ID to represent ATTACK
//   }
//
//   return moves;
//}

//std::vector<MoveType>* MacroSearch::UpdatePossibleMoves()
std::vector<int>* MacroSearch::UpdatePossibleMoves()
{
   //mSearchDepth;  //int
   if (mSearchDepth >= (int)mPossibleMoves.size())
   {
      mPossibleMoves.resize(mSearchDepth+3); //just need +1, but give it a +3 in case we are going another couple levels
   }
   //std::vector<MoveType>* movesPtr = &mPossibleMoves[mSearchDepth];
   std::vector<int>* movesPtr = &mPossibleMoves[mSearchDepth];
   movesPtr->clear();


   if (mCurrentFarm < (mFarmCapacity-1+16*mFarmBuilding) &&  //have (or will have) farm available
       mTrainingCompleteFrames[2].size() >0) //and have a gateway to build it in
   {
      movesPtr->push_back(BWAPI::UnitTypes::Protoss_Zealot);
   }

   if (mTrainingCompleteFrames[1].size()>0 || mFarmBuilding>0 || mFarmCapacity > 18)  //need pylon
   {
      movesPtr->push_back(BWAPI::UnitTypes::Protoss_Gateway);
   }

   if ((mTrainingCompleteFrames[1].size()==0) ||
       ((mFarmCapacity+16*mFarmBuilding-mCurrentFarm) <= (2*mMaxTrainingCapacity)) )
   {
      movesPtr->push_back(BWAPI::UnitTypes::Protoss_Pylon);
   }

   //TODO: count mineral spots, for now... use 8
   if (mUnitCounts[0] < 16 && //two per mineral spot
       mCurrentFarm < (mFarmCapacity-1+16*mFarmBuilding) &&   //and have (or will have) farm available
       mTrainingCompleteFrames[0].size() >0) //and have a nexus to build it in
   {
      movesPtr->push_back(BWAPI::UnitTypes::Protoss_Probe);
   }

   if (mUnitCounts[1] > 1 && mForceMoving == false)   //dont do attack move again, if already moving
   {
      movesPtr->push_back( BWAPI::UnitType(cATTACK_ID) );  //cATTACK_ID to represent ATTACK
   }

   return movesPtr;
}


//bool MacroSearch::DoMove(MoveType aMove, int targetFrame)
bool MacroSearch::DoMove(int aMove, int targetFrame)
{
   //int price          = aMove.mineralPrice();
   //int buildTime      = aMove.buildTime();   //frames
   //int supplyRequired = aMove.supplyRequired();

   int moveStartTime = mGameStateFrame;
   int price = 0;
 //QueuedMove newMove;
   QueuedMove* newMovePtr;

   //switch (aMove.getID())
   switch (aMove)
   {
   case eProtoss_Probe: //probe //TODO: verify this ID //50, 300, 2
      {
         price              = 50;
         int buildTime      = 300;   //frames
         int supplyRequired = 2;

         int buildingIndex = 0;  //nexus
         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mGameStateFrame;
         if (mMinerals < price) {
            frameMineralAvailable += ((price-mMinerals)*FramePerMinute/mMineralsPerMinute);  //rough check, doesn't account for probes that could finish training by then
         }
         //find out at what time a production building is available
         //TODO: check all buildings of correct type, not just last one
         int frameBuildingAvailable = mTrainingCompleteFrames[buildingIndex][0];//.back();   //TODO find right nexus
         if (frameBuildingAvailable < mGameStateFrame) {
            frameBuildingAvailable = mGameStateFrame;
         }
         //find out at what time farm will be available
         int farmAvailable = mGameStateFrame;
         if ((mCurrentFarm+supplyRequired)>mFarmCapacity)
         {
            //find when pylon will be done
            farmAvailable = mTrainingCompleteFrames[1].front();
         }

         //0. find build time
         //start time is latest of these three times
         moveStartTime = max(frameMineralAvailable, max(frameBuildingAvailable, farmAvailable));
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }

         //2. adjust supply
         mCurrentFarm += supplyRequired;   //supply taken when unit training started

         ////QueuedMove newMove(moveStartTime, moveStartTime+buildTime, mTrainingCompleteFrames[buildingIndex][0], mGameStateFrame, aMove);  //TODO - find right nexus
         //newMove.frameStarted      = moveStartTime;
         //newMove.frameComplete     = moveStartTime+buildTime;
         //newMove.prevFrameComplete = mTrainingCompleteFrames[buildingIndex][0];  //TODO - find right nexus
         //newMove.prevGameTime      = mGameStateFrame;
         //newMove.move              = aMove;
         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, mTrainingCompleteFrames[buildingIndex][0], mGameStateFrame, aMove);

         //3. adjust training times
         mTrainingCompleteFrames[buildingIndex][0] = (moveStartTime+buildTime);//TODO - find right nexus
         //mUnitCounts[0]   ++; //probe   //TODO, increment this when unit done building
         //mMineralsPerMinute += 42;       //TODO, increment this when unit done building
      }break;

   case eProtoss_Zealot: //zealot //TODO: verify this ID  //100, 600, 4
      {
         price              = 100;
         int buildTime      = 600;   //frames
         int supplyRequired = 4;

         int buildingIndex = 2;  //gateway
         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mGameStateFrame;
         if (mMinerals < price) {
            frameMineralAvailable += ((price-mMinerals)*FramePerMinute/mMineralsPerMinute);
         }
         //find out at what time a production building is available
         int gateIndex = 0;
         int frameBuildingAvailable = mTrainingCompleteFrames[buildingIndex][gateIndex];
         for (int i=1; i<(int)mTrainingCompleteFrames[buildingIndex].size(); ++i)
         {
            if (mTrainingCompleteFrames[buildingIndex][i] < frameBuildingAvailable)
            {
               gateIndex = 1;
               frameBuildingAvailable = mTrainingCompleteFrames[buildingIndex][i];
            }
         }
         if (frameBuildingAvailable < mGameStateFrame) {
            frameBuildingAvailable = mGameStateFrame;
         }

         //find out at what time farm will be available
         int farmAvailable = mGameStateFrame;
         if ((mCurrentFarm+supplyRequired)>mFarmCapacity)
         {
            //find when pylon will be done
            farmAvailable = mTrainingCompleteFrames[1].front();
         }
         //0. find build time
         //start time is latest of these three times
         moveStartTime = max(frameMineralAvailable, max(frameBuildingAvailable, farmAvailable));
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }

         mCurrentFarm += supplyRequired;   //supply taken when unit building started

         ////QueuedMove newMove(moveStartTime, moveStartTime+buildTime, mTrainingCompleteFrames[buildingIndex][gateIndex], mGameStateFrame, aMove);
         //newMove.frameStarted      = moveStartTime;
         //newMove.frameComplete     = moveStartTime+buildTime;
         //newMove.prevFrameComplete = mTrainingCompleteFrames[buildingIndex][gateIndex];
         //newMove.prevGameTime      = mGameStateFrame;
         //newMove.move              = aMove;
         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, mTrainingCompleteFrames[buildingIndex][gateIndex], mGameStateFrame, aMove);

         mTrainingCompleteFrames[buildingIndex][gateIndex] = (moveStartTime+buildTime);
         //mUnitCounts[1]   ++; //zealot  //TODO, increment this when unit done building
      }break;

   case eProtoss_Pylon: //pylon  //TODO: verify this ID //100, 450, 0
      {
         price              = 100;
         int buildTime      = 450;   //frames

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mGameStateFrame;
         if (mMinerals < price) {
            frameMineralAvailable += ((price-mMinerals)*FramePerMinute/mMineralsPerMinute);
         }
         //start time is mineral time
         moveStartTime = frameMineralAvailable;
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }

         ////QueuedMove newMove(moveStartTime, moveStartTime+buildTime, 0, mGameStateFrame, aMove);
         //newMove.frameStarted      = moveStartTime;
         //newMove.frameComplete     = moveStartTime+buildTime;
         //newMove.prevFrameComplete = 0;
         //newMove.prevGameTime      = mGameStateFrame;
         //newMove.move              = aMove;
         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, 0, mGameStateFrame, aMove);

         mTrainingCompleteFrames[1].push_back(moveStartTime+buildTime);
         mFarmBuilding++;
         //mFarmCapacity += 16; //TODO, increment this when unit done building
      }break;

   case eProtoss_Gateway: //gateway   //TODO: verify this ID //150, 900, 0
      {
         price              = 150;
         int buildTime      = 900;   //frames

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mGameStateFrame;
         if (mMinerals < price) {
            frameMineralAvailable += ((price-mMinerals)*FramePerMinute/mMineralsPerMinute);
         }
         //find out at what time a pylon is available
         int framePreReqAvailable = mGameStateFrame;
         if (mFarmCapacity <= 18 && mFarmBuilding>0)
         {
            framePreReqAvailable = mTrainingCompleteFrames[1].front();
         }

         //start time is max of these two times
         moveStartTime = max(frameMineralAvailable, framePreReqAvailable);
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }

         ////QueuedMove newMove(moveStartTime, moveStartTime+buildTime, 0, mGameStateFrame, aMove);
         //newMove.frameStarted      = moveStartTime;
         //newMove.frameComplete     = moveStartTime+buildTime;
         //newMove.prevFrameComplete = 0;
         //newMove.prevGameTime      = mGameStateFrame;
         //newMove.move              = aMove;
         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, 0, mGameStateFrame, aMove);

         mTrainingCompleteFrames[2].push_back(moveStartTime+buildTime);

         //mMaxTrainingCapacity += 2;  //for a zealot, what is a goon? 2?   //TODO, increment this when unit done building
      }break;

   case cATTACK_ID:
      {
         price = 0;
         //mForceAttacking = true; //TODO set this when force arrives (travel time over)

         //sDistanceToEnemy;  //distance in pixels
         mForceMoving = true;
       //double speed = BWAPI::UnitTypes::Protoss_Zealot.topSpeed(); //pixels per frame
         double speed = 17.0; //TODO: THIS IS A WAG!!! REPLACE WITH REAL VALUE (pixels per frame)
         double dTime = ((double)sDistanceToEnemy) / speed;
         int time = (int)(dTime+0.5);  //frames
         if ((mGameStateFrame+time) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         ////QueuedMove newMove(mGameStateFrame, mGameStateFrame+time, 0, mGameStateFrame, aMove);
         //newMove.frameStarted      = moveStartTime;
         //newMove.frameComplete     = mGameStateFrame+time;
         //newMove.prevFrameComplete = 0;
         //newMove.prevGameTime      = mGameStateFrame;
         //newMove.move              = aMove;
         newMovePtr = new QueuedMove(moveStartTime, mGameStateFrame+time, 0, mGameStateFrame, aMove);
      }break;

   default:
      {
         //unknown move, shouldn't be here
         assert(false);
      }break;
   };

   int dt = moveStartTime - mGameStateFrame;
   //Adjust mMinerals
   mMinerals -= price;
   mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
   //TODO: adjust mGas too

   //AddMove(newMove);
   AddMove(newMovePtr);
   AdvanceQueuedEventsUntil(moveStartTime);  //adjusts mGameStateFrame too
   return true;
}


void MacroSearch::AddMove(QueuedMove* move)
{
   //put move on top of stack
   mMoveStack.push_back(move);

   //find the right insertion point in the ordered container
   //later moves go further back in the list
   if (mEventQueue.size() == 0)
   {
      mEventQueue.push_back(move);
   }
   else
   {
      //if (mEventQueue.size() > 12)
      //{
      //   int breakhere = 0;
      //   breakhere++;
      //}
      std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
      for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
      {
      }
      mEventQueue.insert(rit.base(), move);
   }

   ////for std::list<> put later events (larger frameComplete times) at the beginning of the list
   //std::list<QueuedMove>::iterator it = mEventQueue.begin();
   //for ( ; it!=mEventQueue.end(); it++)
   //{
   //   if (move.frameComplete >= it->frameComplete)
   //   {
   //      break;
   //   }
   //}
   //mEventQueue.insert(it, move);

   //std::list<QueuedMove>::iterator it;
   //for ( it=mLastEventIter ; it!=mEventQueue.end(); it++)
   //{
   //   if (move.frameComplete <= it->frameComplete)
   //   {
   //      break;
   //   }
   //}
   //mEventQueue.insert(it, move);
}

void MacroSearch::AdvanceQueuedEventsUntil(int targetFrame)
{
   assert(targetFrame >= mGameStateFrame);

   //later events are further back in the list
   mLastEventIter++;
   QueuedMove* movePtr = *mLastEventIter;
   while (mLastEventIter != mEventQueue.end() && movePtr->frameComplete <= targetFrame)
   {
      //process it, complete the move (finish building, training, or travelling)
    //switch (mLastEventIter->move.getID())
      switch (movePtr->move)
      {
      case eProtoss_Probe: //probe //TODO: verify this ID
         {
            mUnitCounts[0]++; //probe
            mMineralsPerMinute += 42;
            //just add in mineral change from this probe only
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mMinerals += ((42 * dt)/FramePerMinute);
         }break;
      case eProtoss_Zealot: //zealot //TODO: verify this ID
         {
            mUnitCounts[1]++; //zealot
         }break;
      case eProtoss_Pylon: //pylon  //TODO: verify this ID
         {
            mFarmCapacity += 16;
            mFarmBuilding--;
            //mTrainingCompleteFrames[1].erase(mTrainingCompleteFrames[1].begin());   //erase pylon time, its done now
         }break;
      case eProtoss_Gateway: //gateway   //TODO: verify this ID
         {
            mMaxTrainingCapacity += 2;  //zealot? //TODO: find largest unit buildable by this building
         }break;
      case cATTACK_ID:
         {
            mForceAttacking = true;
         }
      default:
         {
            //unknown move, shouldn't be here
         }break;
      };
      mLastEventIter++;
      movePtr = *mLastEventIter;
   }
   //decrement iterator & exit
   mLastEventIter--;
   mGameStateFrame = targetFrame;


   ////events the queue (list) are ordered largest (latest) time is towards the front
   ////mLastEventIter should point to the previous event in the queue that was completed (falls after new events that are towards the front)
   ////the event "before" this iterator will be the next to be processed
   ////rely on the empty first node of max time to break us out if we reach beginning
   //mLastEventIter--;
   //while (mLastEventIter->frameComplete <= targetFrame)
   //{
   //   //process it, complete the move (finish building, training, or travelling)
   //   switch (mLastEventIter->move.getID())
   //   {
   //   case eProtoss_Probe: //probe //TODO: verify this ID
   //      {
   //         mUnitCounts[0]++; //probe
   //         mMineralsPerMinute += 42;
   //      }break;
   //   case eProtoss_Zealot: //zealot //TODO: verify this ID
   //      {
   //         mUnitCounts[1]++; //zealot
   //      }break;
   //   case eProtoss_Pylon: //pylon  //TODO: verify this ID
   //      {
   //         mFarmCapacity += 16;
   //         mFarmBuilding--;
   //         mTrainingCompleteFrames[1].erase(mTrainingCompleteFrames[1].begin());   //erase pylon time, its done now
   //      }break;
   //   case eProtoss_Gateway: //gateway   //TODO: verify this ID
   //      {
   //         mMaxTrainingCapacity += 2;  //zealot? //TODO: find largest unit buildable by this building
   //      }break;
   //   case cATTACK_ID:
   //      {
   //         mForceAttacking = true;
   //      }
   //   default:
   //      {
   //         //unknown move, shouldn't be here
   //      }break;
   //   };
   //   mLastEventIter--;
   //}
   ////increment iterator & exit
   //mLastEventIter++;

   ////finish any units or buildings that should be completed by the targetFrame
   //while (mEventQueue.size()>0 &&
   //       mEventQueue.top().frameComplete <= targetFrame)
   //{
   //   int time = mEventQueue.top().frameComplete;
   //   QueuedMove qMove = mEventQueue.top();
   //   switch (qMove.move.getID()) {};
   //   mEventQueue.pop();
   //}
}

void MacroSearch::ReverseQueuedEventsUntil(int targetFrame)
{
   assert(targetFrame <= mGameStateFrame);

   //later events are further back in the queue
   QueuedMove* movePtr = *mLastEventIter;
   while (movePtr->frameComplete > targetFrame)
   {
      //process it, make the move incomplete & pending on the queue now
    //switch (mLastEventIter->move.getID())
      switch (movePtr->move)
      {
      case eProtoss_Probe: //probe //TODO: verify this ID
         {
            mUnitCounts[0]--; //probe
            mMineralsPerMinute -= 42;
            //adjust mineral change from this one probe only
            int dt = movePtr->frameComplete - mGameStateFrame; //should be negative
            mMinerals += ((42 * dt)/FramePerMinute);
         }break;
      case eProtoss_Zealot: //zealot //TODO: verify this ID
         {
            mUnitCounts[1]--; //zealot
         }break;
      case eProtoss_Pylon: //pylon  //TODO: verify this ID
         {
            mFarmCapacity -= 16;
            mFarmBuilding++;
         }break;
      case eProtoss_Gateway: //gateway   //TODO: verify this ID
         {
            mMaxTrainingCapacity -= 2;  //zealot? //TODO: find largest unit buildable by this building
         }break;
      case cATTACK_ID:
         {
            mForceAttacking = false;
         }
      default:
         {
            //unknown move, shouldn't be here
         }break;
      };
      mLastEventIter--;
      movePtr = *mLastEventIter;
   }


   ////events the queue (list) are ordered largest (latest) time is towards the front
   ////mLastEventIter should point to the previous event in the queue that was completed (falls after new events that are towards the front)
   ////the event "before" this iterator will be the next to be processed
   ////rely on the empty first node of max time to break us out if we reach beginning
   //while (mLastEventIter->frameComplete > targetFrame)
   //{
   //   //process it, make the move incomplete & pending on the queue now
   //   switch (mLastEventIter->move.getID())
   //   {
   //   case eProtoss_Probe: //probe //TODO: verify this ID
   //      {
   //         mUnitCounts[0]--; //probe
   //         mMineralsPerMinute -= 42;
   //      }break;
   //   case eProtoss_Zealot: //zealot //TODO: verify this ID
   //      {
   //         mUnitCounts[1]--; //zealot
   //      }break;
   //   case eProtoss_Pylon: //pylon  //TODO: verify this ID
   //      {
   //         mFarmCapacity -= 16;
   //         mFarmBuilding++;
   //      }break;
   //   case eProtoss_Gateway: //gateway   //TODO: verify this ID
   //      {
   //         mMaxTrainingCapacity -= 2;  //zealot? //TODO: find largest unit buildable by this building
   //      }break;
   //   case cATTACK_ID:
   //      {
   //         mForceAttacking = false;
   //      }
   //   default:
   //      {
   //         //unknown move, shouldn't be here
   //      }break;
   //   };
   //   mLastEventIter++;
   //}
}


void MacroSearch::UndoMove()
{
   assert(!mMoveStack.empty());
   QueuedMove* undoMove = mMoveStack.back();
   mMoveStack.pop_back();

   //after calling ReverseQueuedEventsUntil() the mGameStateFrame and the mLastEventIter are out of sync for a bit
   //reconcile this later
   // mLastEventIter will match with undoMove.prevGameTime (earlier than mGameStateFrame)
   ReverseQueuedEventsUntil(undoMove->prevGameTime);   //this takes off minerals from probes that finished during this "taken back" time

   ////check if this event we just popped off the stack was still incomplete (only in the queue, not fully processed yet)
   //if (mGameStateFrame < undoMove.frameComplete)
   //{
   //   //why does this matter?  it doesn't
   //}

   //if (mGameStateFrame >= undoMove.frameStarted &&  //should this line always be true? do we have to check here?
   //    mGameStateFrame < undoMove.frameComplete)
   //{
   //   std::deque<QueuedMove> tempStack;
   //   while (!(mEventQueue.top() == undoMove))
   //   {
   //      tempStack.push_back(mEventQueue.top());
   //      mEventQueue.pop();
   //   }
   //   mEventQueue.pop();
   //   while (tempStack.size() > 0)
   //   {
   //      mEventQueue.push(tempStack.back());
   //      tempStack.pop_back();
   //   }
   //}

   //int price          = undoMove.move.mineralPrice();
   //int buildTime      = undoMove.move.buildTime();   //frames
   //int supplyRequired = undoMove.move.supplyRequired();

   //0. find build time
   //1. adjust mMinerals
   //2. adjust supply
   //3. adjust training times
   //4. add event to queue
   //5. adjust time

   int price = 0; //each case statement sets the real price, minerals adjusted at bottom

 //switch (undoMove.move.getID())
   switch (undoMove->move)
   {
   case eProtoss_Probe: //probe //TODO: verify this ID
      {
         price              = 50;
         int buildTime      = 300;   //frames
         int supplyRequired = 2;

         int buildingIndex = 0;  //nexus
         //check for probe mining duration -> mineral rate changes because of probe
         //if (undoMove.frameComplete <= mGameStateFrame)
         //{
         //   //adjust time back to end of move
         //   int dt = undoMove.frameComplete - mGameStateFrame; //should be negative
         //   mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
         //   mGameStateFrame = undoMove.frameComplete;
         //   mMineralsPerMinute -= 42;  //this is safe now, we've backed up time to when probe finished
         //                              //otherwise dont do this, the mining rate wasn't updated yet
         //}
         //TODO: adjust mGas too
         mCurrentFarm -= supplyRequired;
         mTrainingCompleteFrames[buildingIndex][0] = undoMove->prevFrameComplete; //TODO - find right nexus
         //mUnitCounts[0]--; //probe
      }break;

   case eProtoss_Zealot: //zealot //TODO: verify this ID
      {
         price              = 100;
         int buildTime      = 600;   //frames
         int supplyRequired = 4;

         int buildingIndex = 2;  //gateway

         //TODO: adjust mGas too
         mCurrentFarm -= supplyRequired;
         int gateIndex = 0;
         for (int i=0; i<(int)mTrainingCompleteFrames[buildingIndex].size(); ++i)
         {
            if (mTrainingCompleteFrames[buildingIndex][i] == undoMove->frameComplete)
            {
               gateIndex = i;
               break;
            }
         }
         mTrainingCompleteFrames[buildingIndex][gateIndex] = undoMove->prevFrameComplete;
         //mUnitCounts[1]--; //zealot  //TODO, increment this when unit done building
      }break;

   case eProtoss_Pylon: //pylon  //TODO: verify this ID
      {
         price              = 100;
         int buildTime      = 450;   //frames
         mFarmBuilding--;
         //TODO: adjust mGas too
         mTrainingCompleteFrames[1].pop_back(); //should remove last added pylon
         //QueuedMove newMove(moveStartTime, moveStartTime+buildTime, 0, mGameStateFrame, aMove);
         //mFarmCapacity -= 16;
      }break;

   case eProtoss_Gateway: //gateway   //TODO: verify this ID
      {
         price              = 150;
         int buildTime      = 900;   //frames

         //TODO: adjust mGas too
         mTrainingCompleteFrames[2].pop_back(); //should remove last added gateway
         //mMaxTrainingCapacity -= 2;  //zealot? //TODO: find largest unit buildable by this building
      }break;

   case cATTACK_ID:
      {
         price = 0;
         mForceMoving = false;
         mForceAttacking = false;
      }break;

   default:
      {
         price = 0;
         //unknown move, shouldn't be here
         assert(false);
      }break;
   };
   mMinerals += price;
   int dt = undoMove->prevGameTime - mGameStateFrame;  //should be negative
   mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
   mGameStateFrame = undoMove->prevGameTime;

   //remove event from queue now
   std::list<QueuedMove*>::iterator it = mLastEventIter;  //events before mLastEventIter are incomplete & pending
   for (it++; (*it)->frameComplete <= undoMove->frameComplete; it++)
   {
      if (*it == undoMove)
      {
         //remove it
         delete *it;
         mEventQueue.erase(it);
         break;
      }
   }
}


int MacroSearch::EvaluateState()
{
   int score = mMinerals;
   score += mUnitCounts[0] * 50;
   score += mUnitCounts[1] * 200;   //doubly favor zealots
   score += mFarmCapacity * 10;
   score += mTrainingCompleteFrames[2].size() * 200;  //slightly favor gateways
   return score;
}

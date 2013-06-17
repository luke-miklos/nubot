
#include "MacroSearch.h"
//#include "BWSAL.h"
#include <algorithm>
#include <assert.h>

#include <sys/timeb.h>
int getMilliCount()
{
   timeb tb;
   ftime(&tb);
   int nCount = tb.millitm + (tb.time & 0xfffff) * 1000;
   return nCount;
}
int getMilliSpan(int nTimeStart)
{
   int nSpan = getMilliCount() - nTimeStart;
   if(nSpan < 0)
      nSpan += 0x100000 * 1000;
   return nSpan;
}


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
    mMaxScore(0)
   ,mBestMoves()
   //,mGameStateFrame(0)
   //,mForceAttacking(false)
   //,mForceMoving(false)
   //,mMinerals(50)                 //default starting mMinerals (enough for 1 worker)
   //,mGas(0)
   //,mCurrentFarm(4*2)             //4 starting probes
   //,mFarmCapacity(9*2)            //default starting farm capacity (9)
   //,mFarmBuilding(0)              //number of supply things that are building (pylon, overlord, depo)
   //,mMineralsPerMinute(4*MineralsPerMinute)      //default starting rate (4 workers * MineralsPerMinute/minute)
   //,mMaxTrainingCapacity(2)       //nexus (1 probe at a time)
   //,mUnitCounts(12, 0)            //only 12 kinds of units?
   //,mTrainingCompleteFrames(14, std::vector<int>())   // 14 kinds of buildings?
   ,mMyState()       //real state
   ,mEnemyState()    //real state
   ,mSearchState()   //fake search state (starts off search as copy of mMyState)
   ,mMoveStack()
   ,mEventQueue()
   ,mSearchDepth(0)
   ,mSearchLookAhead(0)
 //,mPossibleMoves(30, std::vector<MoveType>())   //initialize with a possible depth of 30
   ,mPossibleMoves(30, std::vector<int>())   //initialize with a possible depth of 30
{

   if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
   {
      mMyRace = ePROTOSS;
   }
   else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
   {
      mMyRace = eTERRAN;
   }
   else  //BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg
   {
      mMyRace = eZERG;
   }

   mEventQueue.clear();
 //mEventQueue.push_front(QueuedMove(2147483647, 2147483647, 2147483647, 2147483647, MoveType(-1)));   //must have empty (max int) node at the beginning, useful later
 //mEventQueue.push_front(QueuedMove(-1, -1, -1, -1, MoveType(-1)));   //must have empty (-1) node at the beginning, useful later
   mEventQueue.push_front(new QueuedMove(-1, -1, -1, -1, eNull_Move));   //must have empty (-1) node at the beginning, useful later
   mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
                                          //mLastEventIter points to the last event processed, should be in sync with game time
                                          //events complete at later times are towards the front of the queue (list)
}

//void MacroSearch::onUnitHide(BWAPI::Unit* unit)
//{
//}
//void MacroSearch::onUnitEvade(BWAPI::Unit* unit)   //when a build is cancelled (training stopped)
//{
//}
void MacroSearch::onUnitDestroy(BWAPI::Unit* unit)  //last event processed when a build/train is cancelled
{
   if (unit->getPlayer()->getID() == BWAPI::Broodwar->self()->getID() &&
       unit->isBeingConstructed() &&
       !unit->isCompleted())
   {
      int buildFrames = unit->getRemainingBuildTime();
      int gameFrame = BWAPI::Broodwar->getFrameCount();
      int endBuildTime = gameFrame + buildFrames - 1; //subtract one because current frame is included in the count

      //find appropriate building to cancel the construction from
      switch(unit->getType().getID())
      {
      case eProtoss_Probe:
         {
            //TODO - find right nexus
            //clear the nexus build time to be available now
            mMyState.mTrainingCompleteFrames[0][0] = BWAPI::Broodwar->getFrameCount();   //nexus
         }break;
      case eProtoss_Zealot:
         {
            for (int i=0; i<(int)mMyState.mTrainingCompleteFrames[2].size(); ++i)
            {
               if (mMyState.mTrainingCompleteFrames[2][i] == endBuildTime)
               {
                  //clear the gateway build to be available now
                  mMyState.mTrainingCompleteFrames[2][i] = gameFrame;
                  break;
               }
            }
         }break;
      case eTerran_SCV:
         {
            //TODO - find right cc
            //clear the cc build time to be available now
            mMyState.mTrainingCompleteFrames[0][0] = BWAPI::Broodwar->getFrameCount();   //nexus
         }break;
      case eTerran_Marine:
         {
            for (int i=0; i<(int)mMyState.mTrainingCompleteFrames[2].size(); ++i)
            {
               if (mMyState.mTrainingCompleteFrames[2][i] == endBuildTime)
               {
                  //clear the gateway build to be available now
                  mMyState.mTrainingCompleteFrames[2][i] = gameFrame;
                  break;
               }
            }
         }break;

      default:
         {
         }break;
      };
   }
}
//void MacroSearch::onUnitMorph(BWAPI::Unit* unit)
//{
//}
void MacroSearch::onUnitCreate(BWAPI::Unit* unit)   //first event processed when a build/train is started
{
   if (unit->getPlayer()->getID() == BWAPI::Broodwar->self()->getID() &&
       unit->isBeingConstructed() &&
       !unit->isCompleted())
   {
      //find appropriate building to start the construction in
      int buildFrames = unit->getRemainingBuildTime();
      int gameFrame = BWAPI::Broodwar->getFrameCount();
      int endBuildTime = gameFrame + buildFrames;

      switch(unit->getType().getID())
      {
      case eProtoss_Probe:
         {
            //TODO - find right nexus
            //clear the nexus build time to be available now
            mMyState.mTrainingCompleteFrames[0][0] = endBuildTime;   //nexus
         }break;
      case eProtoss_Zealot:
         {
            int gateIndex = 0;
            int lowest = mMyState.mTrainingCompleteFrames[2][gateIndex];
            for (int i=1; i<(int)mMyState.mTrainingCompleteFrames[2].size(); ++i)
            {
               if (mMyState.mTrainingCompleteFrames[2][i] < lowest)
               {
                  gateIndex = i;
                  lowest = mMyState.mTrainingCompleteFrames[2][i];
               }
            }
            mMyState.mTrainingCompleteFrames[2][gateIndex] = endBuildTime;
         }break;
      case eProtoss_Pylon:
         {
            mMyState.mTrainingCompleteFrames[1].push_back(endBuildTime);
         }break;
      case eProtoss_Gateway:
         {
            mMyState.mTrainingCompleteFrames[2].push_back(endBuildTime);
         }break;

      case eTerran_SCV:
         {
            //TODO - find right cc
            //clear the cc build time to be available now
            mMyState.mTrainingCompleteFrames[0][0] = endBuildTime;   //cc
         }break;
      case eTerran_Marine:
         {
            int raxIndex = 0;
            int lowest = mMyState.mTrainingCompleteFrames[2][raxIndex];
            for (int i=1; i<(int)mMyState.mTrainingCompleteFrames[2].size(); ++i)
            {
               if (mMyState.mTrainingCompleteFrames[2][i] < lowest)
               {
                  raxIndex = i;
                  lowest = mMyState.mTrainingCompleteFrames[2][i];
               }
            }
            mMyState.mTrainingCompleteFrames[2][raxIndex] = endBuildTime;
         }break;
      case eTerran_Supply_Depot:
         {
            mMyState.mTrainingCompleteFrames[1].push_back(endBuildTime);
         }break;
      case eTerran_Barracks:
         {
            mMyState.mTrainingCompleteFrames[2].push_back(endBuildTime);
         }break;


      default:
         {
         }break;
      };
   }
}
void MacroSearch::onUnitComplete(BWAPI::Unit* unit)
{
      int buildFrames = unit->getRemainingBuildTime();
      int gameFrame = BWAPI::Broodwar->getFrameCount();
      int endBuildTime = gameFrame + buildFrames;
      BWAPI::UnitType type = unit->getType();
}


//void MacroSearch::onUnitDiscover(BWAPI::Unit* unit)
//{
//}
//void MacroSearch::onUnitShow(BWAPI::Unit* unit)
//{
//}

void MacroSearch::onFrame()
{
   //BWAPI::Broodwar->drawTextScreen(0, -64, "Macro Searched ahead: %d ms", mSearchLookAhead);
   BWAPI::Broodwar->drawTextScreen(0,  64, "Macro Searched ahead: %d ms", mSearchLookAhead);

   //std::list<BWAPI::Event> events = BWAPI::Broodwar->getEvents();
   //std::list<BWAPI::Event>::iterator it = events.begin();
   //while (it != events.end())
   //{
   //   BWAPI::EventType::Enum type = it->getType();
   //   if (type >= BWAPI::EventType::UnitDiscover && 
   //       type <= BWAPI::EventType::UnitComplete && 
   //       type != BWAPI::EventType::SaveGame)
   //   {
   //      // if( BWAPI::Broodwar->self()->isEnemy( unit->getPlayer() ) )
   //      //BWAPI::Event ev = *it;
   //      BWAPI::Unit* unit = it->getUnit();
   //      if (unit->getPlayer()->getID() == BWAPI::Broodwar->self()->getID())
   //      {
   //         int framesTrain = unit->getRemainingTrainTime();
   //         int framesBuild = unit->getRemainingBuildTime();
   //         bool beingConstructed = unit->isBeingConstructed();
   //         bool completed = unit->isCompleted();
   //         std::list<BWAPI::UnitType> queue = unit->getTrainingQueue();

   //         switch (it->getType())
   //         {
   //         case BWAPI::EventType::UnitDiscover:
   //            {
   //            }break;
   //         case BWAPI::EventType::UnitEvade:
   //            {
   //            }break;
   //         case BWAPI::EventType::UnitDestroy:
   //            {
   //            }break;
   //         case BWAPI::EventType::UnitCreate:
   //            {
   //            }break;
   //         case BWAPI::EventType::UnitHide:
   //            {
   //            }break;
   //         case BWAPI::EventType::UnitShow:
   //            {
   //            }break;
   //         };
   //      }
   //   }
   //   it++;
   //}
}


std::vector<QueuedMove*> MacroSearch::FindMoves(int targetFrame, int maxMilliseconds)
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

    //BWAPI::UnitTypes::Terran_Marine;                  //0
    //BWAPI::UnitTypes::Terran_Ghost;                   //1
    //BWAPI::UnitTypes::Terran_Vulture;                 //2
    //BWAPI::UnitTypes::Terran_Goliath;                 //3
    //BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode;    //5
    //BWAPI::UnitTypes::Terran_SCV;                     //7
    //BWAPI::UnitTypes::Terran_Wraith;                  //8
    //BWAPI::UnitTypes::Terran_Science_Vessel;          //9
    //BWAPI::UnitTypes::Terran_Dropship;                //11
    //BWAPI::UnitTypes::Terran_Battlecruiser;           //12
    //BWAPI::UnitTypes::Terran_Vulture_Spider_Mine;     //13
    //BWAPI::UnitTypes::Terran_Nuclear_Missile;         //14
    //BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;   //30
    //BWAPI::UnitTypes::Terran_Firebat;                 //32
    //BWAPI::UnitTypes::Spell_Scanner_Sweep;            //33
    //BWAPI::UnitTypes::Terran_Medic;                   //34
    //BWAPI::UnitTypes::Terran_Civilian;                //15
    //BWAPI::UnitTypes::Terran_Valkyrie;                //58
    //BWAPI::UnitTypes::Terran_Command_Center;    //106
    //BWAPI::UnitTypes::Terran_Comsat_Station;    //107
    //BWAPI::UnitTypes::Terran_Nuclear_Silo;      //108
    //BWAPI::UnitTypes::Terran_Supply_Depot;      //109
    //BWAPI::UnitTypes::Terran_Refinery;          //110
    //BWAPI::UnitTypes::Terran_Barracks;          //111
    //BWAPI::UnitTypes::Terran_Academy;           //112
    //BWAPI::UnitTypes::Terran_Factory;           //113
    //BWAPI::UnitTypes::Terran_Starport;          //114
    //BWAPI::UnitTypes::Terran_Control_Tower;     //115
    //BWAPI::UnitTypes::Terran_Science_Facility;  //116
    //BWAPI::UnitTypes::Terran_Covert_Ops;        //117
    //BWAPI::UnitTypes::Terran_Physics_Lab;       //118
    //BWAPI::UnitTypes::Terran_Machine_Shop;      //120
    //BWAPI::UnitTypes::Terran_Engineering_Bay;   //122
    //BWAPI::UnitTypes::Terran_Armory;            //123
    //BWAPI::UnitTypes::Terran_Missile_Turret;    //124
    //BWAPI::UnitTypes::Terran_Bunker;            //125



   ////////////////////////////////////////////////////////////////////////////
   // populate my state with current game values
   BWAPI::Player* me = BWAPI::Broodwar->self();
   mMyState.mGameStateFrame = BWAPI::Broodwar->getFrameCount();
   mMyState.mMinerals = (float)me->minerals();
   mMyState.mGas      = (float)me->gas();
   mMyState.mCurrentFarm  = me->supplyUsed();
   mMyState.mFarmCapacity = me->supplyTotal();
   switch(mMyRace)
   {
   case ePROTOSS:
   {
      mMyState.mUnitCounts[0] = me->completedUnitCount(BWAPI::UnitTypes::Protoss_Probe);
      mMyState.mUnitCounts[1] = me->completedUnitCount(BWAPI::UnitTypes::Protoss_Zealot);
      mMyState.mFarmBuilding = me->incompleteUnitCount(BWAPI::UnitTypes::Protoss_Pylon);
      mMyState.mMaxTrainingCapacity = mMyState.mTrainingCompleteFrames[0].size() * 2 +
                                      mMyState.mTrainingCompleteFrames[2].size() * 4;
   }break;
   case eTERRAN:
   {
      mMyState.mUnitCounts[0] = me->completedUnitCount(BWAPI::UnitTypes::Terran_SCV);
      mMyState.mUnitCounts[1] = me->completedUnitCount(BWAPI::UnitTypes::Terran_Marine);
      mMyState.mFarmBuilding = me->incompleteUnitCount(BWAPI::UnitTypes::Terran_Supply_Depot);
      mMyState.mMaxTrainingCapacity = mMyState.mTrainingCompleteFrames[0].size() * 2 +
                                      mMyState.mTrainingCompleteFrames[2].size() * 2;
   }break;
   };
   mMyState.mMineralsPerMinute   = mMyState.mUnitCounts[0] * MineralsPerMinute;
   ////////////////////////////////////////////////////////////////////////////

   int startFrame = mMyState.mGameStateFrame;
   int startTimer = getMilliCount();
   int milliSecondsElapsed = 0;
   int mark1 = 0;
   int mark2 = 0;
   int projectedDuration = 0;

   int fullDuration = targetFrame - startFrame;
   int currentTarget = startFrame;  //start target only 1/4 second into future  
   // + (fullDuration/2) - (FramePerMinute/4);   //start at half the target & increment by quarter seconds
   while (projectedDuration <= maxMilliseconds && currentTarget < targetFrame)
   {
      currentTarget += (FramePerMinute/4);
      //start search from our current state
      mSearchState = mMyState;
      mMaxScore = 0;
      mBestMoves.clear();
      mSearchDepth = -1;
      FindMovesRecursive(currentTarget);

      mark2 = mark1;
      mark1 = milliSecondsElapsed;
      milliSecondsElapsed = getMilliSpan(startTimer);
      int elapsed1 = milliSecondsElapsed - mark1;
      int elapsed2 = mark1 - mark2;
      if (elapsed2 > 0)
      {
         projectedDuration = milliSecondsElapsed + elapsed1 * elapsed1 / elapsed2;
      }
      else
      {
         projectedDuration = milliSecondsElapsed + elapsed1 * 2;
      }
   }
   mSearchLookAhead = currentTarget - startFrame;
   return mBestMoves;
}


void MacroSearch::FindMovesRecursive(int targetFrame)
{
   //std::vector<MoveType> moves = PossibleMoves(targetFrame);
   mSearchDepth++;
   if (mSearchDepth > 1)
   {
      mSearchDepth = mSearchDepth;
   }

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
      else if (mMoveStack.size()>0)
      {
         //save off this current game state as a leaf node of the DFS game tree search
         int score = EvaluateState(targetFrame);
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
//      moves.push_back( BWAPI::UnitType(eAttack) );  //eAttack to represent ATTACK
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
   int mostCost = 0;

   if (mMyRace == ePROTOSS)
   {
	  int gateCnt = mSearchState.mTrainingCompleteFrames[2].size();

      if ((mSearchState.mCurrentFarm+4) <= (mSearchState.mFarmCapacity + 16*mSearchState.mFarmBuilding) &&  //have (or will have) farm available
          gateCnt >0) //and have a gateway to build it in
      {
       //movesPtr->push_back(BWAPI::UnitTypes::Protoss_Zealot);
         movesPtr->push_back(eProtoss_Zealot);
         mostCost = max(mostCost,100);
      }

	  int maxGate = ((int)mSearchState.mMineralsPerMinute)/(240+60);  //max spend rate = 2.4 zealots a minute, +6/10 pylon
      if ((mSearchState.mTrainingCompleteFrames[1].size()>0 || 
           mSearchState.mFarmBuilding>0                     || 
           mSearchState.mFarmCapacity > 18) &&
        //(mSearchState.mTrainingCompleteFrames[0].size()*4) > mSearchState.mTrainingCompleteFrames[2].size())
          (gateCnt == 0 || gateCnt < maxGate) )
      {
       //movesPtr->push_back(BWAPI::UnitTypes::Protoss_Gateway);
         movesPtr->push_back(eProtoss_Gateway);
         mostCost = max(mostCost,150);
      }

      if ((mSearchState.mTrainingCompleteFrames[1].size()==0) ||
          ((mSearchState.mFarmCapacity+16*mSearchState.mFarmBuilding-mSearchState.mCurrentFarm) <= (2*mSearchState.mMaxTrainingCapacity)) )
      {
       //movesPtr->push_back(BWAPI::UnitTypes::Protoss_Pylon);
         movesPtr->push_back(eProtoss_Pylon);
         mostCost = max(mostCost,100);
      }

      //TODO: count mineral spots, for now... use 8
      if (mSearchState.mUnitCounts[0] < 16 && //two per mineral spot
         (mSearchState.mCurrentFarm+2) <= (mSearchState.mFarmCapacity + 16*mSearchState.mFarmBuilding) && //and have (or will have) farm available
          mSearchState.mTrainingCompleteFrames[0].size() >0 &&    //and have a nexus to build it in
          ((mostCost==0)||((mostCost+50)>mSearchState.mMinerals)||mSearchState.mTrainingCompleteFrames[0][0]<=mSearchState.mGameStateFrame) ) //if we could instantly build something else, & a probe is already building, dont do a probe right now
      {
       //movesPtr->push_back(BWAPI::UnitTypes::Protoss_Probe);
         movesPtr->push_back(eProtoss_Probe);
      }
   }
   else if (mMyRace == eTERRAN)
   {
      //eTerran_Marine = 0,
      //eTerran_SCV = 7,
      //eTerran_Supply_Depot = 109,
      //eTerran_Barracks = 111,
	  int raxCnt = mSearchState.mTrainingCompleteFrames[2].size();

      if ((mSearchState.mCurrentFarm+2) <= (mSearchState.mFarmCapacity + 16*mSearchState.mFarmBuilding) &&  //have (or will have) farm available
          raxCnt >0) //and have a barracks to build it in
      {
         movesPtr->push_back(eTerran_Marine);
         mostCost = max(mostCost,50);
      }

	  int maxRax = ((int)mSearchState.mMineralsPerMinute)/(200+50);  //max spend rate = 4 marines a minute, +1/2 depot
    //if ((mSearchState.mTrainingCompleteFrames[0].size()*4) > mSearchState.mTrainingCompleteFrames[2].size())
      if (raxCnt==0 || raxCnt < maxRax)
      {
         movesPtr->push_back(eTerran_Barracks);
         mostCost = max(mostCost,150);
      }

      if ((mSearchState.mFarmCapacity+16*mSearchState.mFarmBuilding-mSearchState.mCurrentFarm) <= (2*mSearchState.mMaxTrainingCapacity))
      {
         movesPtr->push_back(eTerran_Supply_Depot);
         mostCost = max(mostCost,100);
      }

      //TODO: count mineral spots, for now... use 8
      if (mSearchState.mUnitCounts[0] < 17 && //two per mineral spot +1 for building
         (mSearchState.mCurrentFarm+2) <= (mSearchState.mFarmCapacity + 16*mSearchState.mFarmBuilding) && //and have (or will have) farm available
          mSearchState.mTrainingCompleteFrames[0].size() >0 &&    //and have a command center to build it in
          ((mostCost==0)||((mostCost+50)>mSearchState.mMinerals)||mSearchState.mTrainingCompleteFrames[0][0]<=mSearchState.mGameStateFrame) ) //if we could instantly build something else, & an scv is already building, dont do an scv right now
      {
         movesPtr->push_back(eTerran_SCV);
      }
   }
   else //(mMyRace == eZERG)
   {


   }

   //if (mSearchState.mUnitCounts[1] > 1 && mSearchState.mForceMoving == false)   //dont do attack move again, if already moving
   //{
   // //movesPtr->push_back( BWAPI::UnitType(cATTACK_ID) );  //cATTACK_ID to represent ATTACK
   //   movesPtr->push_back( eAttack );
   //   
   //}
   if (movesPtr->size() <= 0)
   {
      int breakhere = 1;
   }

   return movesPtr;
}


//bool MacroSearch::DoMove(MoveType aMove, int targetFrame)
bool MacroSearch::DoMove(int aMove, int targetFrame)
{
   //int price          = aMove.mineralPrice();
   //int buildTime      = aMove.buildTime();   //frames
   //int supplyRequired = aMove.supplyRequired();

   int moveStartTime = mSearchState.mGameStateFrame;
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

         //int buildingIndex = 0;  //nexus
         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);  //rough check, doesn't account for probes that could finish training by then
         }
         //find out at what time a production building is available
         //TODO: check all buildings of correct type, not just last one
         int frameBuildingAvailable = mSearchState.mTrainingCompleteFrames[0][0];//.back();   //TODO find right nexus
         if (frameBuildingAvailable < mSearchState.mGameStateFrame) {
            frameBuildingAvailable = mSearchState.mGameStateFrame;
         }
         //find out at what time farm will be available
         int farmAvailable = mSearchState.mGameStateFrame;
         if ((mSearchState.mCurrentFarm+supplyRequired)>mSearchState.mFarmCapacity)
         {
            //find when pylon will be done
            assert(mSearchState.mFarmBuilding>0);
            assert(mSearchState.mNextFarmDoneIndex < (int)mSearchState.mTrainingCompleteFrames[1].size());
            farmAvailable = mSearchState.mTrainingCompleteFrames[1][mSearchState.mNextFarmDoneIndex];
            //farmAvailable = mSearchState.mTrainingCompleteFrames[1].front();  //TODO - find right pylon! not necessarily front in vector
         }

         //0. find build time
         //start time is latest of these three times
         moveStartTime = max(frameMineralAvailable, max(frameBuildingAvailable, farmAvailable));
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }

         //2. adjust supply
         mSearchState.mCurrentFarm += supplyRequired;   //supply taken when unit training started

         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, mSearchState.mTrainingCompleteFrames[0][0], mSearchState.mGameStateFrame, aMove);

         //3. adjust training times
         mSearchState.mTrainingCompleteFrames[0][0] = (moveStartTime+buildTime);//TODO - find right nexus
         //mUnitCounts[0]   ++; //probe   //TODO, increment this when unit done building
         //mMineralsPerMinute += MineralsPerMinute;       //TODO, increment this when unit done building
      }break;

   case eProtoss_Zealot: //zealot //TODO: verify this ID  //100, 600, 4
      {
         price              = 100;
         int buildTime      = 600;   //frames
         int supplyRequired = 4;

         //int buildingIndex = 2;  //gateway
         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         //find out at what time a production building is available
         int gateIndex = 0;
         int frameBuildingAvailable = mSearchState.mTrainingCompleteFrames[2][gateIndex];
         for (int i=1; i<(int)mSearchState.mTrainingCompleteFrames[2].size(); ++i)
         {
            if (mSearchState.mTrainingCompleteFrames[2][i] < frameBuildingAvailable)
            {
               gateIndex = i;
               frameBuildingAvailable = mSearchState.mTrainingCompleteFrames[2][i];
            }
         }
         if (frameBuildingAvailable < mSearchState.mGameStateFrame) {
            frameBuildingAvailable = mSearchState.mGameStateFrame;
         }

         int farmAvailable = mSearchState.mGameStateFrame;
         if ((mSearchState.mCurrentFarm+supplyRequired)>mSearchState.mFarmCapacity)
         {
            //find when pylon will be done
            assert(mSearchState.mFarmBuilding>0);
            assert(mSearchState.mNextFarmDoneIndex < (int)mSearchState.mTrainingCompleteFrames[1].size());
            farmAvailable = mSearchState.mTrainingCompleteFrames[1][mSearchState.mNextFarmDoneIndex];
            //farmAvailable = mSearchState.mTrainingCompleteFrames[1].front();  //TODO - find right pylon! not necessarily front in vector
         }

         //0. find build time
         //start time is latest of these three times
         moveStartTime = max(frameMineralAvailable, max(frameBuildingAvailable, farmAvailable));
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }

         mSearchState.mCurrentFarm += supplyRequired;   //supply taken when unit building started

         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, mSearchState.mTrainingCompleteFrames[2][gateIndex], mSearchState.mGameStateFrame, aMove);

         mSearchState.mTrainingCompleteFrames[2][gateIndex] = (moveStartTime+buildTime);
         //mUnitCounts[1]   ++; //zealot  //TODO, increment this when unit done building
      }break;

   case eProtoss_Pylon: //pylon  //TODO: verify this ID //100, 450, 0
      {
         price              = 100;
         int buildTime      = 450;   //frames

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         //start time is mineral time
         moveStartTime = frameMineralAvailable;
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }

         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, 0, mSearchState.mGameStateFrame, aMove);

         mSearchState.mTrainingCompleteFrames[1].push_back(moveStartTime+buildTime);
         mSearchState.mFarmBuilding++;
         //mFarmCapacity += 16; //TODO, increment this when unit done building
      }break;

   case eProtoss_Gateway: //gateway   //TODO: verify this ID //150, 900, 0
      {
         price              = 150;
         int buildTime      = 900;   //frames

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         //find out at what time a pylon is available
         int framePreReqAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mFarmCapacity <= 18 && mSearchState.mFarmBuilding>0)
         {
            framePreReqAvailable = mSearchState.mTrainingCompleteFrames[1].front(); //first pylon complete should work...
         }

         //start time is max of these two times
         moveStartTime = max(frameMineralAvailable, framePreReqAvailable);
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }

         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, 0, mSearchState.mGameStateFrame, aMove);

         mSearchState.mTrainingCompleteFrames[2].push_back(moveStartTime+buildTime);

         //mMaxTrainingCapacity += 2;  //for a zealot, what is a goon? 2?   //TODO, increment this when unit done building
      }break;

   //case eAttack:
   //   {
   //      price = 0;
   //      //mForceAttacking = true; //TODO set this when force arrives (travel time over)

   //      //sDistanceToEnemy;  //distance in pixels
   //      mSearchState.mForceMoving = true;
   //    //double speed = BWAPI::UnitTypes::Protoss_Zealot.topSpeed(); //pixels per frame
   //      double speed = 17.0; //TODO: THIS IS A WAG!!! REPLACE WITH REAL VALUE (pixels per frame)
   //      double dTime = ((double)sDistanceToEnemy) / speed;
   //      int time = (int)(dTime+0.5);  //frames
   //      if ((mSearchState.mGameStateFrame+time) > targetFrame)
   //      {
   //         return false;  //no need to search this move, its past our end search time
   //      }
   //      ////QueuedMove newMove(mGameStateFrame, mGameStateFrame+time, 0, mGameStateFrame, aMove);
   //      //newMove.frameStarted      = moveStartTime;
   //      //newMove.frameComplete     = mGameStateFrame+time;
   //      //newMove.prevFrameComplete = 0;
   //      //newMove.prevGameTime      = mGameStateFrame;
   //      //newMove.move              = aMove;
   //      newMovePtr = new QueuedMove(moveStartTime, mSearchState.mGameStateFrame+time, 0, mSearchState.mGameStateFrame, aMove);
   //   }break;


   case eTerran_SCV: //scv //TODO: verify this ID //50, 300, 2
      {
         price              = 50;
         int buildTime      = 300;   //frames
         int supplyRequired = 2;

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);  //rough check, doesn't account for scv that could finish training by then
         }
         //find out at what time a production building is available
         //TODO: check all buildings of correct type, not just last one
         int frameBuildingAvailable = mSearchState.mTrainingCompleteFrames[0][0];//.back();   //TODO find right cc
         if (frameBuildingAvailable < mSearchState.mGameStateFrame) {
            frameBuildingAvailable = mSearchState.mGameStateFrame;
         }
         //find out at what time farm will be available
         int farmAvailable = mSearchState.mGameStateFrame;
         if ((mSearchState.mCurrentFarm+supplyRequired)>mSearchState.mFarmCapacity)
         {
            //find when pylon will be done
            assert(mSearchState.mFarmBuilding>0);
            assert(mSearchState.mNextFarmDoneIndex < (int)mSearchState.mTrainingCompleteFrames[1].size());
            farmAvailable = mSearchState.mTrainingCompleteFrames[1][mSearchState.mNextFarmDoneIndex];
            //farmAvailable = mSearchState.mTrainingCompleteFrames[1].front();  //TODO - find right pylon! not necessarily front in vector
         }
         //0. find build time
         //start time is latest of these three times
         moveStartTime = max(frameMineralAvailable, max(frameBuildingAvailable, farmAvailable));
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         //2. adjust supply
         mSearchState.mCurrentFarm += supplyRequired;   //supply taken when unit training started
         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, mSearchState.mTrainingCompleteFrames[0][0], mSearchState.mGameStateFrame, aMove);
         //3. adjust training times
         mSearchState.mTrainingCompleteFrames[0][0] = (moveStartTime+buildTime);//TODO - find right cc
         //mUnitCounts[0]   ++; //probe   //TODO, increment this when unit done building
         //mMineralsPerMinute += MineralsPerMinute;       //TODO, increment this when unit done building
      }break;

   case eTerran_Marine: //marin //TODO: verify this ID  //100, 600, 4
      {
         price              = 50;
         int buildTime      = 360; //frames
         int supplyRequired = 2;

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         //find out at what time a production building is available
         int raxIndex = 0;
         int frameBuildingAvailable = mSearchState.mTrainingCompleteFrames[2][raxIndex];
         for (int i=1; i<(int)mSearchState.mTrainingCompleteFrames[2].size(); ++i)
         {
            if (mSearchState.mTrainingCompleteFrames[2][i] < frameBuildingAvailable)
            {
               raxIndex = i;
               frameBuildingAvailable = mSearchState.mTrainingCompleteFrames[2][i];
            }
         }
         //if (frameBuildingAvailable < mSearchState.mGameStateFrame) {
         //   frameBuildingAvailable = mSearchState.mGameStateFrame;
         //}

         int farmAvailable = mSearchState.mGameStateFrame;
         if ((mSearchState.mCurrentFarm+supplyRequired)>mSearchState.mFarmCapacity)
         {
            //find when depot will be done
            assert(mSearchState.mFarmBuilding>0);
            assert(mSearchState.mNextFarmDoneIndex < (int)mSearchState.mTrainingCompleteFrames[1].size());
            farmAvailable = mSearchState.mTrainingCompleteFrames[1][mSearchState.mNextFarmDoneIndex];
            //farmAvailable = mSearchState.mTrainingCompleteFrames[1].front();  //TODO - find right pylon! not necessarily front in vector
         }

         //0. find build time
         //start time is latest of these three times
         moveStartTime = max(frameMineralAvailable, max(frameBuildingAvailable, farmAvailable));
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         mSearchState.mCurrentFarm += supplyRequired;   //supply taken when unit building started
         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, mSearchState.mTrainingCompleteFrames[2][raxIndex], mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[2][raxIndex] = (moveStartTime+buildTime);
         //mUnitCounts[1]   ++; //zealot  //TODO, increment this when unit done building
      }break;

   case eTerran_Supply_Depot: //depot //TODO: verify this ID //100, 450, 0
      {
         price              = 100;
         int buildTime      = 600; //frames

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         //start time is mineral time
         moveStartTime = frameMineralAvailable;
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }

         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, 0, mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[1].push_back(moveStartTime+buildTime);
         mSearchState.mFarmBuilding++;
         //mFarmCapacity += 16; //TODO, increment this when unit done building
      }break;

   case eTerran_Barracks: //rax //TODO: verify this ID //150, 900, 0
      {
         price              = 150;
         int buildTime      = 1200; //frames

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int moveStartTime = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            moveStartTime += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, 0, mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[2].push_back(moveStartTime+buildTime);
         //mMaxTrainingCapacity += 2;  //for a zealot, what is a goon? 2?   //TODO, increment this when unit done building
      }break;


   default:
      {
         //unknown move, shouldn't be here
         assert(false);
      }break;
   };

   int dt = moveStartTime - mSearchState.mGameStateFrame;
   //Adjust mSearchState.mMinerals
   mSearchState.mMinerals -= price;
   mSearchState.mMinerals += ((mSearchState.mMineralsPerMinute * dt)/FramePerMinute);
   //TODO: adjust mGas too

   //AddMove(newMove);
   AddMove(newMovePtr);
   AdvanceQueuedEventsUntil(moveStartTime);  //adjusts mSearchState.mGameStateFrame too
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
   assert(targetFrame >= mSearchState.mGameStateFrame);

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
            mSearchState.mUnitCounts[0]++; //probe
            mSearchState.mMineralsPerMinute += MineralsPerMinute;
            //just add in mineral change from this probe only
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;
      case eProtoss_Zealot: //zealot //TODO: verify this ID
         {
            mSearchState.mUnitCounts[1]++; //zealot
         }break;
      case eProtoss_Pylon: //pylon  //TODO: verify this ID
         {
            mSearchState.mFarmCapacity += 16;
            mSearchState.mFarmBuilding--;
            mSearchState.mNextFarmDoneIndex++;
         }break;
      case eProtoss_Gateway: //gateway   //TODO: verify this ID
         {
            mSearchState.mMaxTrainingCapacity += 4;  //zealot? //TODO: find largest unit buildable by this building
         }break;
      //case eAttack:
      //   {
      //      mSearchState.mForceAttacking = true;
      //   }

      case eTerran_SCV: //scv //TODO: verify this ID
         {
            mSearchState.mUnitCounts[0]++; //scv
            mSearchState.mMineralsPerMinute += MineralsPerMinute;
            //just add in mineral change from this scv only
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;
      case eTerran_Marine: //marine //TODO: verify this ID
         {
            mSearchState.mUnitCounts[1]++; //marine
         }break;
      case eTerran_Supply_Depot: //depot //TODO: verify this ID
         {
            mSearchState.mFarmCapacity += 16;
            mSearchState.mFarmBuilding--;
            mSearchState.mNextFarmDoneIndex++;
         }break;
      case eTerran_Barracks: //rax  //TODO: verify this ID
         {
            mSearchState.mMaxTrainingCapacity += 2;  //marine? //TODO: find largest unit buildable by this building
         }break;

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
   mSearchState.mGameStateFrame = targetFrame;


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
   //         mMineralsPerMinute += MineralsPerMinute;
   //      }break;
   //   case eProtoss_Zealot: //zealot //TODO: verify this ID
   //      {
   //         mUnitCounts[1]++; //zealot
   //      }break;
   //   case eProtoss_Pylon: //pylon  //TODO: verify this ID
   //      {
   //         mFarmCapacity += 16;
   //         mSearchState.mFarmBuilding--;
   //         mTrainingCompleteFrames[1].erase(mTrainingCompleteFrames[1].begin());   //erase pylon time, its done now
   //      }break;
   //   case eProtoss_Gateway: //gateway   //TODO: verify this ID
   //      {
   //         mMaxTrainingCapacity += 4;  //zealot? //TODO: find largest unit buildable by this building
   //      }break;
   //   case eAttack:
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
   assert(targetFrame <= mSearchState.mGameStateFrame);

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
            mSearchState.mUnitCounts[0]--; //probe
            mSearchState.mMineralsPerMinute -= MineralsPerMinute;
            //adjust mineral change from this one probe only
            int dt = movePtr->frameComplete - mSearchState.mGameStateFrame; //should be negative
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;
      case eProtoss_Zealot: //zealot //TODO: verify this ID
         {
            mSearchState.mUnitCounts[1]--; //zealot
         }break;
      case eProtoss_Pylon: //pylon  //TODO: verify this ID
         {
            mSearchState.mFarmCapacity -= 16;
            mSearchState.mFarmBuilding++;
            mSearchState.mNextFarmDoneIndex--;
         }break;
      case eProtoss_Gateway: //gateway   //TODO: verify this ID
         {
            mSearchState.mMaxTrainingCapacity -= 4;  //zealot? //TODO: find largest unit buildable by this building
         }break;
      //case eAttack:
      //   {
      //      mSearchState.mForceAttacking = false;
      //   }
      case eTerran_SCV: //scv //TODO: verify this ID
         {
            mSearchState.mUnitCounts[0]--; //scv
            mSearchState.mMineralsPerMinute -= MineralsPerMinute;
            //adjust mineral change from this one scv only
            int dt = movePtr->frameComplete - mSearchState.mGameStateFrame; //should be negative
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;
      case eTerran_Marine: //marine //TODO: verify this ID
         {
            mSearchState.mUnitCounts[1]--; //marine
         }break;
      case eTerran_Supply_Depot: //depot  //TODO: verify this ID
         {
            mSearchState.mFarmCapacity -= 16;
            mSearchState.mFarmBuilding++;
            mSearchState.mNextFarmDoneIndex--;
         }break;
      case eTerran_Barracks: //rax //TODO: verify this ID
         {
            mSearchState.mMaxTrainingCapacity -= 2;  //marine? //TODO: find largest unit buildable by this building
         }break;
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
   //         mMineralsPerMinute -= MineralsPerMinute;
   //      }break;
   //   case eProtoss_Zealot: //zealot //TODO: verify this ID
   //      {
   //         mUnitCounts[1]--; //zealot
   //      }break;
   //   case eProtoss_Pylon: //pylon  //TODO: verify this ID
   //      {
   //         mFarmCapacity -= 16;
   //         mSearchState.mFarmBuilding++;
   //      }break;
   //   case eProtoss_Gateway: //gateway   //TODO: verify this ID
   //      {
   //         mMaxTrainingCapacity -= 4;  //zealot? //TODO: find largest unit buildable by this building
   //      }break;
   //   case eAttack:
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

   //after calling ReverseQueuedEventsUntil() the mSearchState.mGameStateFrame and the mLastEventIter are out of sync for a bit
   //reconcile this later
   // mLastEventIter will match with undoMove.prevGameTime (earlier than mSearchState.mGameStateFrame)
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

         //int buildingIndex = 0;  //nexus
         //check for probe mining duration -> mineral rate changes because of probe
         //if (undoMove.frameComplete <= mGameStateFrame)
         //{
         //   //adjust time back to end of move
         //   int dt = undoMove.frameComplete - mGameStateFrame; //should be negative
         //   mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
         //   mGameStateFrame = undoMove.frameComplete;
         //   mMineralsPerMinute -= MineralsPerMinute;  //this is safe now, we've backed up time to when probe finished
         //                              //otherwise dont do this, the mining rate wasn't updated yet
         //}
         //TODO: adjust mGas too
         mSearchState.mCurrentFarm -= supplyRequired;
         mSearchState.mTrainingCompleteFrames[0][0] = undoMove->prevFrameComplete; //TODO - find right nexus
         //mUnitCounts[0]--; //probe
      }break;

   case eProtoss_Zealot: //zealot //TODO: verify this ID
      {
         price              = 100;
         int buildTime      = 600;   //frames
         int supplyRequired = 4;

         //int buildingIndex = 2;  //gateway

         //TODO: adjust mGas too
         mSearchState.mCurrentFarm -= supplyRequired;
         int gateIndex = 0;
         for (int i=0; i<(int)mSearchState.mTrainingCompleteFrames[2].size(); ++i)
         {
            if (mSearchState.mTrainingCompleteFrames[2][i] == undoMove->frameComplete)
            {
               gateIndex = i;
               break;
            }
         }
         mSearchState.mTrainingCompleteFrames[2][gateIndex] = undoMove->prevFrameComplete;
         //mUnitCounts[1]--; //zealot  //TODO, increment this when unit done building
      }break;

   case eProtoss_Pylon: //pylon  //TODO: verify this ID
      {
         price              = 100;
         int buildTime      = 450;   //frames
         mSearchState.mFarmBuilding--;
         //TODO: adjust mGas too
         mSearchState.mTrainingCompleteFrames[1].pop_back(); //should remove last added pylon
         //QueuedMove newMove(moveStartTime, moveStartTime+buildTime, 0, mGameStateFrame, aMove);
         //mFarmCapacity -= 16;
      }break;

   case eProtoss_Gateway: //gateway   //TODO: verify this ID
      {
         price              = 150;
         int buildTime      = 900;   //frames

         //TODO: adjust mGas too
         mSearchState.mTrainingCompleteFrames[2].pop_back(); //should remove last added gateway
         //mMaxTrainingCapacity -= 4;  //zealot? //TODO: find largest unit buildable by this building
      }break;

   //case eAttack:
   //   {
   //      price = 0;
   //      mSearchState.mForceMoving = false;
   //      mSearchState.mForceAttacking = false;
   //   }break;

   case eTerran_SCV: //scv //TODO: verify this ID
      {
         price              = 50;
         int buildTime      = 300;  //frames
         int supplyRequired = 2;
         mSearchState.mCurrentFarm -= supplyRequired;
         mSearchState.mTrainingCompleteFrames[0][0] = undoMove->prevFrameComplete; //TODO - find right cc
      }break;

   case eTerran_Marine: //marine //TODO: verify this ID
      {
         price              = 100;
         int buildTime      = 360;  //frames
         int supplyRequired = 2;
         mSearchState.mCurrentFarm -= supplyRequired;
         int raxIndex = 0;
         for (int i=0; i<(int)mSearchState.mTrainingCompleteFrames[2].size(); ++i)
         {
            if (mSearchState.mTrainingCompleteFrames[2][i] == undoMove->frameComplete)
            {
               raxIndex = i;
               break;
            }
         }
         mSearchState.mTrainingCompleteFrames[2][raxIndex] = undoMove->prevFrameComplete;
      }break;

   case eTerran_Supply_Depot: //depot  //TODO: verify this ID
      {
         price              = 100;
         int buildTime      = 600;  //frames
         mSearchState.mFarmBuilding--;
         mSearchState.mTrainingCompleteFrames[1].pop_back(); //should remove last added pylon
      }break;

   case eTerran_Barracks: //rax   //TODO: verify this ID
      {
         price              = 150;
         int buildTime      = 1200;  //frames
         mSearchState.mTrainingCompleteFrames[2].pop_back(); //should remove last added gateway
      }break;

   default:
      {
         price = 0;
         //unknown move, shouldn't be here
         assert(false);
      }break;
   };
   mSearchState.mMinerals += price;
   int dt = undoMove->prevGameTime - mSearchState.mGameStateFrame;  //should be negative
   mSearchState.mMinerals += ((mSearchState.mMineralsPerMinute * dt)/FramePerMinute);
   mSearchState.mGameStateFrame = undoMove->prevGameTime;

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


int MacroSearch::EvaluateState(int targetFrame)
{
   int score = (int)(mSearchState.mMinerals + 0.5f);
   score += mSearchState.mUnitCounts[0] * 50;
   if (mSearchState.mTrainingCompleteFrames[0][0] > mSearchState.mGameStateFrame) {
      score += 50;   //count the worker that's building
   }
   score += mSearchState.mTrainingCompleteFrames[1].size() * 100;

   switch (mMyRace)
   {
   case ePROTOSS:
   {
      score += mSearchState.mUnitCounts[1] * 500;   //favor zealots + x5 cost
      score += mSearchState.mTrainingCompleteFrames[2].size() * 300;  //favor gateways + x2 cost
      //std::vector<int>::iterator it;
      //for (it  = mSearchState.mTrainingCompleteFrames[2].begin();
      //     it != mSearchState.mTrainingCompleteFrames[2].end();
      //     it ++)
      //{
      //   int complete = *it;
      //}
   }break;
   case eTERRAN:
   {
      score += mSearchState.mUnitCounts[1] * 250;   //favor marines + x5 cost
      score += mSearchState.mTrainingCompleteFrames[2].size() * 300;  //favor barracks + x2 cost
   }break;
   case eZERG:
   {

   }break;
   };
   return score;
}

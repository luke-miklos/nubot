
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
    mDiminishingReturns(true)
   ,mMaxScore(0)
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
   ,mMaxDepth(-1)
   ,mSearchDepth(0)
   ,mSearchLookAhead(0)
   ,mPossibleMoves(60, std::vector<int>())   //initialize with a possible depth of 60
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
   //if (unit->getPlayer()->getID() == BWAPI::Broodwar->self()->getID() &&
   //    unit->isBeingConstructed() &&
   //    !unit->isCompleted())
   //{
   //   int buildFrames = unit->getRemainingBuildTime();
   //   int gameFrame = BWAPI::Broodwar->getFrameCount();
   //   int endBuildTime = gameFrame + buildFrames - 1; //subtract one because current frame is included in the count

   //   //find appropriate building to cancel the construction from
   //   switch(unit->getType().getID())
   //   {
   //   case eProtoss_Probe:
   //      {
   //         //TODO - find right nexus
   //         //clear the nexus build time to be available now
   //         mMyState.mTrainingCompleteFrames[0][0] = BWAPI::Broodwar->getFrameCount();   //nexus
   //      }break;
   //   case eProtoss_Zealot:
   //      {
   //         for (int i=0; i<(int)mMyState.mTrainingCompleteFrames[2].size(); ++i)
   //         {
   //            if (mMyState.mTrainingCompleteFrames[2][i] == endBuildTime)
   //            {
   //               //clear the gateway build to be available now
   //               mMyState.mTrainingCompleteFrames[2][i] = gameFrame;
   //               break;
   //            }
   //         }
   //      }break;
   //   case eTerran_SCV:
   //      {
   //         //TODO - find right cc
   //         //clear the cc build time to be available now
   //         mMyState.mTrainingCompleteFrames[0][0] = BWAPI::Broodwar->getFrameCount();   //nexus
   //      }break;
   //   case eTerran_Marine:
   //      {
   //         for (int i=0; i<(int)mMyState.mTrainingCompleteFrames[2].size(); ++i)
   //         {
   //            if (mMyState.mTrainingCompleteFrames[2][i] == endBuildTime)
   //            {
   //               //clear the gateway build to be available now
   //               mMyState.mTrainingCompleteFrames[2][i] = gameFrame;
   //               break;
   //            }
   //         }
   //      }break;

   //   default:
   //      {
   //      }break;
   //   };
   //}
}

void MacroSearch::onUnitMorph(BWAPI::Unit* unit)
{
   ////void onUnitMorph(Unit* unit);
   ////BWAPI calls this when an accessible unit changes type, such as from a Zerg Drone to a Zerg Hatchery, or from a Terran Siege Tank Tank Mode to Terran Siege Tank Siege Mode. This is not called when the type changes to or from UnitTypes::Unknown (which happens when a unit is transitioning to or from inaccessibility). 
   //if (unit->getPlayer()->getID() == BWAPI::Broodwar->self()->getID() &&
   //    unit->isBeingConstructed() &&
   //    !unit->isCompleted())
   //{
   //   int type = unit->getType().getID();
   // //if (unit->isMorphing() && type == 36)
   //   if (unit->isMorphing())
   //   {
   //      //zerg unit about to become something else
   //      type = unit->getBuildType().getID();
   //   }

   //   //find appropriate building to start the construction in
   //   int buildFrames = unit->getRemainingBuildTime();
   //   int gameFrame = BWAPI::Broodwar->getFrameCount();
   //   int endBuildTime = gameFrame + buildFrames;

   //   //if(mMyRace == eZERG)
   //   //{
   //   //   std::vector<Hatchery>::iterator it = mMyState.mLarva.begin();
   //   //   for (; it!= mMyState.mLarva.end(); it++)
   //   //   {
   //   //      while (it->NumLarva < 3 && it->NextLarvaSpawnFrame <= gameFrame)
   //   //      {
   //   //         it->NumLarva++;
   //   //         it->NextLarvaSpawnFrame += LarvaFrameTime;
   //   //      }
   //   //   }
   //   //}

   //   switch(type)
   //   {
   //   case eZerg_Drone:
   //      {
   //         //////TODO - find right cc
   //         //////clear the cc build time to be available now
   //         ////mMyState.mTrainingCompleteFrames[0][0] = endBuildTime;   //cc
   //         //int hatchIndex = 0;  //TODO: find right hatchery
   //         //if (mMyState.mLarva[hatchIndex].NumLarva == 3)
   //         //{
   //         //   mMyState.mLarva[hatchIndex].NextLarvaSpawnFrame = gameFrame + LarvaFrameTime;
   //         //}
   //         //mMyState.mLarva[hatchIndex].NumLarva--;

   //         QueuedMove* move = new QueuedMove(gameFrame, endBuildTime, 0, gameFrame, eZerg_Drone);
   //         std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
   //         for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
   //         {
   //         }
   //         mEventQueue.insert(rit.base(), move);
   //         //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet

   //      }break;
   //   case eZerg_Zergling:
   //      {
   //         //int hatchIndex = 0;  //TODO: find right hatchery
   //         //if (mMyState.mLarva[hatchIndex].NumLarva == 3)
   //         //{
   //         //   mMyState.mLarva[hatchIndex].NextLarvaSpawnFrame = gameFrame + LarvaFrameTime;
   //         //}
   //         //mMyState.mLarva[hatchIndex].NumLarva--;

   //         QueuedMove* move = new QueuedMove(gameFrame, endBuildTime, 0, gameFrame, eZerg_Zergling);
   //         std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
   //         for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
   //         {
   //         }
   //         mEventQueue.insert(rit.base(), move);
   //         //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet

   //      }break;
   //   case eZerg_Overlord:
   //      {
   //         mMyState.mTrainingCompleteFrames[1].push_back(endBuildTime);
   //         //int hatchIndex = 0;  //TODO: find right hatchery
   //         //if (mMyState.mLarva[hatchIndex].NumLarva == 3)
   //         //{
   //         //   mMyState.mLarva[hatchIndex].NextLarvaSpawnFrame = gameFrame + LarvaFrameTime;
   //         //}
   //         //mMyState.mLarva[hatchIndex].NumLarva--;
   //      }break;
   //   case eZerg_Hatchery:
   //      {
   //         mMyState.mTrainingCompleteFrames[0].push_back(endBuildTime);
   //      }break;
   //   case eZerg_Spawning_Pool:
   //      {
   //         mMyState.mTrainingCompleteFrames[2].push_back(endBuildTime);
   //      }break;

   //   default:
   //      {
   //      }break;
   //   };
   //}


}

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

      //if(mMyRace == eZERG)
      //{
      //   std::vector<Hatchery>::iterator it = mMyState.mLarva.begin();
      //   for (; it!= mMyState.mLarva.end(); it++)
      //   {
      //      while (it->NumLarva < 3 && it->NextLarvaSpawnFrame <= gameFrame)
      //      {
      //         it->NumLarva++;
      //         it->NextLarvaSpawnFrame += LarvaFrameTime;
      //      }
      //   }
      //}

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
      case eProtoss_Dragoon:
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
      case eProtoss_Nexus:
         {
            mMyState.mTrainingCompleteFrames[0].push_back(endBuildTime);
         }break;
      case eProtoss_Pylon:
         {
            mMyState.mTrainingCompleteFrames[1].push_back(endBuildTime);
         }break;
      case eProtoss_Gateway:
         {
            mMyState.mTrainingCompleteFrames[2].push_back(endBuildTime);
         }break;
      case eProtoss_Assimilator:
         {
            mMyState.mTrainingCompleteFrames[3].push_back(endBuildTime);
         }break;
      case eProtoss_Cybernetics_Core:
         {
            mMyState.mTrainingCompleteFrames[4].push_back(endBuildTime);
         }break;
      case eProtoss_Forge:
         {
            mMyState.mTrainingCompleteFrames[5].push_back(endBuildTime);
         }break;
      case eProtoss_Photon_Cannon:
         {
            mMyState.mTrainingCompleteFrames[6].push_back(endBuildTime);
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

      case eTerran_Firebat:
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

      case eTerran_Medic:
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

      case eTerran_Refinery:
         {
            mMyState.mTrainingCompleteFrames[3].push_back(endBuildTime);
         }break;
      case eTerran_Academy:
         {
            mMyState.mTrainingCompleteFrames[4].push_back(endBuildTime);
         }break;
      case eTerran_Command_Center:
         {
            mMyState.mTrainingCompleteFrames[0].push_back(endBuildTime);
         }break;


    //case ZERG -> see onUnitMorph()

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
   BWAPI::Broodwar->drawTextScreen(0,  64, "Macro Searched ahead: %d frames", mSearchLookAhead);

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


std::vector<int> MacroSearch::FindMoves(int targetFrame, int maxMilliseconds, int maxDepth)
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

   mMaxDepth = maxDepth;

   //clear out event queue except for first null move
   //std::list<QueuedMove*>::iterator it;
   while (mEventQueue.size() > 1)
   {
      //it = mEventQueue.back();
      //delete *it;
      delete mEventQueue.back();
      mEventQueue.pop_back();
   }
   mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet

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
      mMyState.mUnitCounts[2] = me->completedUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);
      mMyState.mFarmBuilding = me->incompleteUnitCount(BWAPI::UnitTypes::Protoss_Pylon);
      mMyState.mMaxTrainingCapacity = mMyState.mTrainingCompleteFrames[0].size() * 2 +
                                      mMyState.mTrainingCompleteFrames[2].size() * 4;

      //must resest these for event queing below
      mMyState.mTrainingCompleteFrames[0].resize(me->completedUnitCount(BWAPI::UnitTypes::Protoss_Nexus),            0);
      mMyState.mTrainingCompleteFrames[1].resize(me->completedUnitCount(BWAPI::UnitTypes::Protoss_Pylon),            0);
      mMyState.mTrainingCompleteFrames[2].resize(me->completedUnitCount(BWAPI::UnitTypes::Protoss_Gateway),          0);
      mMyState.mGasBuildingsDone = me->completedUnitCount(BWAPI::UnitTypes::Protoss_Assimilator);
      mMyState.mTrainingCompleteFrames[3].resize(mMyState.mGasBuildingsDone,                                         0);
      mMyState.mTrainingCompleteFrames[4].resize(me->completedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core), 0);
      mMyState.mTrainingCompleteFrames[5].resize(me->completedUnitCount(BWAPI::UnitTypes::Protoss_Forge),            0);
      mMyState.mTrainingCompleteFrames[6].resize(me->completedUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon),    0);

      std::set<BWAPI::Unit*> units = me->getUnits();
      std::set<BWAPI::Unit*>::iterator it = units.begin();
      int raxIndex = 0;
      int ccIndex = 0;
      for (;it!=units.end(); it++)
	   {
         BWAPI::Unit * unit = *it;

         if (unit->isBeingConstructed())
         {
            int buildFrames = unit->getRemainingBuildTime();
            int gameFrame = mMyState.mGameStateFrame;
            int endBuildTime = gameFrame + buildFrames;
            int startBuildTime = endBuildTime - unit->getBuildType().buildTime();

            switch (unit->getBuildType().getID())
            {
            case eProtoss_Pylon:
            {
               mMyState.mTrainingCompleteFrames[1].push_back(endBuildTime);
               mMyState.mFarmBuilding++;

               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eProtoss_Pylon);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eProtoss_Probe:
            {
               mMyState.mTrainingCompleteFrames[0][ccIndex++] = endBuildTime;
               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eProtoss_Probe);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eProtoss_Zealot:
            {
               mMyState.mTrainingCompleteFrames[2][raxIndex++] = endBuildTime;
               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eProtoss_Zealot);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eProtoss_Dragoon:
            {
               mMyState.mTrainingCompleteFrames[2][raxIndex++] = endBuildTime;
               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eProtoss_Dragoon);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eProtoss_Nexus:
            {
               mMyState.mTrainingCompleteFrames[0].push_back(endBuildTime);
               //mMyState.mMineralsPerMinute -= MineralsPerMinute;   //calculated below
               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eProtoss_Nexus);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eProtoss_Gateway:
            {
               mMyState.mTrainingCompleteFrames[2].push_back(endBuildTime);
               //no event needed, no extra processing done for finishing or un-finishing this building
            }break;

            case eProtoss_Assimilator:
            {
               mMyState.mTrainingCompleteFrames[3].push_back(endBuildTime);
               //mMyState.mMineralsPerMinute -= MineralsPerMinute;   //calculated below
               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eTerran_Refinery);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
            }break;

            case eProtoss_Cybernetics_Core:
            {
               mMyState.mTrainingCompleteFrames[4].push_back(endBuildTime);
               //no event needed, no extra processing done for finishing or un-finishing this building
            }break;

            case eProtoss_Forge:
            {
               mMyState.mTrainingCompleteFrames[5].push_back(endBuildTime);
               //no event needed, no extra processing done for finishing or un-finishing this building
            }break;

            case eProtoss_Photon_Cannon:
            {
               mMyState.mTrainingCompleteFrames[6].push_back(endBuildTime);
               //no event needed, no extra processing done for finishing or un-finishing this building
            }break;

            };
         }
      }


   }break;
   case eTERRAN:
   {
      mMyState.mUnitCounts[0] = me->completedUnitCount(BWAPI::UnitTypes::Terran_SCV);
      mMyState.mUnitCounts[1] = me->completedUnitCount(BWAPI::UnitTypes::Terran_Marine);
      mMyState.mUnitCounts[2] = me->completedUnitCount(BWAPI::UnitTypes::Terran_Firebat);
      mMyState.mUnitCounts[3] = me->completedUnitCount(BWAPI::UnitTypes::Terran_Medic);
      mMyState.mFarmBuilding = me->incompleteUnitCount(BWAPI::UnitTypes::Terran_Supply_Depot);
      mMyState.mMaxTrainingCapacity = mMyState.mTrainingCompleteFrames[0].size() * 2 +
                                      mMyState.mTrainingCompleteFrames[2].size() * 2;

      //must resest these for event queing below
      mMyState.mTrainingCompleteFrames[0].resize(me->completedUnitCount(BWAPI::UnitTypes::Terran_Command_Center), 0);
      mMyState.mTrainingCompleteFrames[1].resize(me->completedUnitCount(BWAPI::UnitTypes::Terran_Supply_Depot),   0);
      mMyState.mTrainingCompleteFrames[2].resize(me->completedUnitCount(BWAPI::UnitTypes::Terran_Barracks),       0);
      mMyState.mGasBuildingsDone = me->completedUnitCount(BWAPI::UnitTypes::Terran_Refinery);
      mMyState.mTrainingCompleteFrames[3].resize(mMyState.mGasBuildingsDone,                                      0);
      mMyState.mTrainingCompleteFrames[4].resize(me->completedUnitCount(BWAPI::UnitTypes::Terran_Academy),        0);

      std::set<BWAPI::Unit*> units = me->getUnits();
      std::set<BWAPI::Unit*>::iterator it = units.begin();
      int raxIndex = 0;
      int ccIndex = 0;
      for (;it!=units.end(); it++)
	   {
         BWAPI::Unit * unit = *it;

         if (unit->isBeingConstructed())
         {
            int buildFrames = unit->getRemainingBuildTime();
            int gameFrame = mMyState.mGameStateFrame;
            int endBuildTime = gameFrame + buildFrames;
            int startBuildTime = endBuildTime - unit->getBuildType().buildTime();

            switch (unit->getBuildType().getID())
            {
            case eTerran_Supply_Depot:
            {
               mMyState.mTrainingCompleteFrames[1].push_back(endBuildTime);
               mMyState.mFarmBuilding++;

               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eTerran_Supply_Depot);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eTerran_SCV:
            {
               mMyState.mTrainingCompleteFrames[0][ccIndex++] = endBuildTime;
               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eTerran_SCV);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eTerran_Marine:
            {
               mMyState.mTrainingCompleteFrames[2][raxIndex++] = endBuildTime;
               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eTerran_Marine);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eTerran_Firebat:
            {
               mMyState.mTrainingCompleteFrames[2][raxIndex++] = endBuildTime;
               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eTerran_Firebat);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eTerran_Medic:
            {
               mMyState.mTrainingCompleteFrames[2][raxIndex++] = endBuildTime;
               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eTerran_Medic);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eTerran_Command_Center:
            {
               mMyState.mTrainingCompleteFrames[0].push_back(endBuildTime);

               mMyState.mUnitCounts[0]--; //scv
               //mMyState.mMineralsPerMinute -= MineralsPerMinute;   //calculated below

               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eTerran_Command_Center);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eTerran_Barracks:
            {
               mMyState.mTrainingCompleteFrames[2].push_back(endBuildTime);
               //no event needed, no extra processing done for finishing or un-finishing this building
            }break;

            case eTerran_Refinery:
            {
               mMyState.mTrainingCompleteFrames[3].push_back(endBuildTime);

               mMyState.mUnitCounts[0]--; //scv
               //mMyState.mMineralsPerMinute -= MineralsPerMinute;   //calculated below

               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eTerran_Refinery);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);

            }break;

            case eTerran_Academy:
            {
               mMyState.mTrainingCompleteFrames[4].push_back(endBuildTime);
               //no event needed, no extra processing done for finishing or un-finishing this building
            }break;

            };
         }
      }



   }break;
   case eZERG:
   {
      //Unit* getHatchery() const;
      ////For Zerg Larva, this returns the Hatchery, Lair, or Hive unit this Larva was spawned from. For all other unit types this function returns NULL.
      //std::set<Unit*> getLarva() const;
      ////Returns the set of larva spawned by this unit. If the unit has no larva, or is not a Hatchery, Lair, or Hive, this function returns an empty set. Equivalent to clicking "Select Larva" from the Starcraft GUI. 
      mMyState.mUnitCounts[0] = me->completedUnitCount(BWAPI::UnitTypes::Zerg_Drone);
      mMyState.mUnitCounts[1] = me->completedUnitCount(BWAPI::UnitTypes::Zerg_Zergling);
      mMyState.mFarmBuilding = me->incompleteUnitCount(BWAPI::UnitTypes::Zerg_Overlord);
      mMyState.mMaxTrainingCapacity = mMyState.mTrainingCompleteFrames[0].size() * 2;

      //me->getUnits();
      //std::set<Unit*> hatcheryUnit->getLarva();

      mMyState.mTrainingCompleteFrames[0].resize(me->completedUnitCount(BWAPI::UnitTypes::Zerg_Hatchery));
      mMyState.mTrainingCompleteFrames[1].resize(me->completedUnitCount(BWAPI::UnitTypes::Zerg_Overlord));

      std::set<BWAPI::Unit*> units = me->getUnits();
      std::set<BWAPI::Unit*>::iterator it = units.begin();
      int hatchIndex = 0;
      for (;it!=units.end(); it++)
	   {
         BWAPI::Unit * unit = *it;
		   // if the unit is completed
		   if (unit->isCompleted() && unit->getType() == BWAPI::UnitTypes::Zerg_Hatchery)
		   {
            while (hatchIndex >= (int)mMyState.mLarva.size())
            {
               mMyState.mLarva.push_back(Hatchery(0, 0));   //hatchery values should be overwritten below
            }
            mMyState.mLarva[hatchIndex].NumLarva = unit->getLarva().size();
            if (mMyState.mLarva[hatchIndex].NumLarva != 3)
            {
               mMyState.mLarva[hatchIndex].NextLarvaSpawnFrame = unit->getRemainingTrainTime() + mMyState.mGameStateFrame;
            }
            hatchIndex++;
         }

         if (unit->isMorphing()) // && unit->getType().getID() == eZerg_Egg)
         {
            int buildFrames = unit->getRemainingBuildTime();
            int gameFrame = mMyState.mGameStateFrame;
            int endBuildTime = gameFrame + buildFrames;
            int startBuildTime = endBuildTime - unit->getBuildType().buildTime();

            switch (unit->getBuildType().getID())
            {
            case eZerg_Overlord:
            {
               mMyState.mTrainingCompleteFrames[1].push_back(endBuildTime);
               mMyState.mFarmBuilding++;

               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eZerg_Overlord);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eZerg_Drone:
            {
               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eZerg_Drone);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eZerg_Zergling:
            {
               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eZerg_Zergling);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eZerg_Hatchery:
            {
               mMyState.mTrainingCompleteFrames[0].push_back(endBuildTime);

               QueuedMove* move = new QueuedMove(startBuildTime, endBuildTime, 0, gameFrame, eZerg_Hatchery);
               std::list<QueuedMove*>::reverse_iterator rit = mEventQueue.rbegin();
               for ( ; rit != mEventQueue.rend() && move->frameComplete < (*rit)->frameComplete; rit++)
               {
               }
               mEventQueue.insert(rit.base(), move);
               //mLastEventIter = mEventQueue.begin();  //all events in the queue before this are not "complete" yet
            }break;

            case eZerg_Spawning_Pool:
            {
               mMyState.mTrainingCompleteFrames[2].push_back(endBuildTime);
               //no event needed, no extra processing done for finishing or un-finishing this building
            }break;
            };
         }
      }

      //if(mMyRace == eZERG)
      //{
      //   std::vector<Hatchery>::iterator it = mMyState.mLarva.begin();
      //   for (; it!= mMyState.mLarva.end(); it++)
      //   {
      //      while (it->NumLarva < 3 && it->NextLarvaSpawnFrame <= mMyState.mGameStateFrame)
      //      {
      //         it->NumLarva++;
      //         it->NextLarvaSpawnFrame += LarvaFrameTime;
      //      }
      //   }
      //}


   }break;
   };
   mMyState.mMineralsPerMinute   = mMyState.mUnitCounts[0] * MineralsPerMinute;
   ////////////////////////////////////////////////////////////////////////////

   int startFrame = mMyState.mGameStateFrame;
   if (maxMilliseconds > 0)
   {
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
   }
   else
   {
      mSearchState = mMyState;
      mMaxScore = 0;
      mBestMoves.clear();
      mSearchDepth = -1;
      FindMovesRecursive(targetFrame);
      mSearchLookAhead = targetFrame - startFrame;
   }

   return mBestMoves;
}


void MacroSearch::FindMovesRecursive(int targetFrame)
{
   mSearchDepth++;
   if (mMaxDepth > 0 && mSearchDepth > mMaxDepth)
   {
      mSearchDepth--;
      //move forward time until last move finishes
      int temp = mSearchState.mGameStateFrame;
      AdvanceQueuedEventsUntil(mMoveStack.back()->frameComplete);
      mSearchState.mGameStateFrame = mMoveStack.back()->frameComplete;
      //then evaluate state
      EvaluateState();
      ReverseQueuedEventsUntil(temp);
      mSearchState.mGameStateFrame = temp;
      return;
   }

   std::vector<int>* movesPtr = PossibleMoves();
   std::vector<int>::iterator it;
   for (it=movesPtr->begin(); it!=movesPtr->end(); it++)
   {
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
         int temp = mSearchState.mGameStateFrame;
         AdvanceQueuedEventsUntil(targetFrame); //just to make sure everything gets scored
         mSearchState.mGameStateFrame = targetFrame;
         //save off this current game state as a leaf node of the DFS game tree search
         EvaluateState();
         ReverseQueuedEventsUntil(temp);  //now go back so rest of search can continute correctly
         mSearchState.mGameStateFrame = temp;
      }
   }
   mSearchDepth--;
   return;
}


std::vector<int>* MacroSearch::PossibleMoves()
{
   //mSearchDepth;  //int
   if (mSearchDepth >= (int)mPossibleMoves.size())
   {
      mPossibleMoves.resize(mSearchDepth+3); //just need +1, but give it a +3 in case we are going another couple levels
   }
   std::vector<int>* movesPtr = &mPossibleMoves[mSearchDepth];
   movesPtr->clear();
   int mostCost = 0;

   if (mMyRace == ePROTOSS)
   {
      int gateCnt = mSearchState.mTrainingCompleteFrames[2].size();

      if ((mSearchState.mCurrentFarm+4) <= (mSearchState.mFarmCapacity + 16*mSearchState.mFarmBuilding) &&  //have (or will have) farm available
          gateCnt >0) //and have a gateway to build it in
      {
         movesPtr->push_back(eProtoss_Zealot);
         mostCost = max(mostCost,100);
      }

      if ((mSearchState.mCurrentFarm+4) <= (mSearchState.mFarmCapacity + 16*mSearchState.mFarmBuilding) &&  //have (or will have) farm available
          gateCnt >0 && //and have a gateway to build it in
          mSearchState.mTrainingCompleteFrames[4].size() > 0 &&  //have cycore
          mSearchState.mTrainingCompleteFrames[3].size() > 0)     //have assimilator
      {
         movesPtr->push_back(eProtoss_Dragoon);
         mostCost = max(mostCost,150);
      }

      int maxGate = ((int)mSearchState.mMineralsPerMinute)/(240+60);  //max spend rate = 2.4 zealots a minute, +6/10 pylon
      if ((mSearchState.mTrainingCompleteFrames[1].size()>0 || 
           mSearchState.mFarmBuilding>0                     || 
           mSearchState.mFarmCapacity > 18) &&
        //(mSearchState.mTrainingCompleteFrames[0].size()*4) > mSearchState.mTrainingCompleteFrames[2].size())
          (gateCnt == 0 || gateCnt < maxGate) )
      {
         movesPtr->push_back(eProtoss_Gateway);
         mostCost = max(mostCost,150);
      }

      if ((mSearchState.mTrainingCompleteFrames[1].size()==0) ||
          ((mSearchState.mFarmCapacity+16*mSearchState.mFarmBuilding-mSearchState.mCurrentFarm) <= (2*mSearchState.mMaxTrainingCapacity)) )
      {
         movesPtr->push_back(eProtoss_Pylon);
         mostCost = max(mostCost,100);
      }

      if (mSearchState.mTrainingCompleteFrames[3].size() <= 0 && //if dont have assimilator yet
          mSearchState.mUnitCounts[0] >= 9)                      //have at least 9 workers
      {
         movesPtr->push_back(eProtoss_Assimilator);
         mostCost = max(mostCost,100);
      }

      if (mSearchState.mTrainingCompleteFrames[2].size() > 0 &&   //have rax
          mSearchState.mTrainingCompleteFrames[4].size() <= 0)    //dont have cyber core yet
      {
         movesPtr->push_back(eProtoss_Cybernetics_Core);
         mostCost = max(mostCost,200);
      }

      //wait to build an extra CC until you have at least 12 workers per existing CC
      if (mSearchState.mUnitCounts[0] >= (int)(12*mSearchState.mTrainingCompleteFrames[0].size()))
      {
         movesPtr->push_back(eProtoss_Nexus);
         mostCost = max(mostCost,400);
      }

      if ((mSearchState.mTrainingCompleteFrames[1].size()>0 || mSearchState.mFarmBuilding>0) && //have pylon
          mSearchState.mTrainingCompleteFrames[5].size()<=0)   //no forge yet
      {
         movesPtr->push_back(eProtoss_Forge);
         mostCost = max(mostCost,150);
      }

      if ((mSearchState.mTrainingCompleteFrames[1].size()>0 || mSearchState.mFarmBuilding>0) && //have pylon
          mSearchState.mTrainingCompleteFrames[5].size()>0)   //have forge
      {
         movesPtr->push_back(eProtoss_Photon_Cannon);
         mostCost = max(mostCost,150);
      }

      //TODO: count mineral spots, for now... use 8
      if (mSearchState.mUnitCounts[0] < 20 && //two per mineral spot + 3 gas + 1 scout
         (mSearchState.mCurrentFarm+2) <= (mSearchState.mFarmCapacity + 16*mSearchState.mFarmBuilding) && //and have (or will have) farm available
          mSearchState.mTrainingCompleteFrames[0].size() >0 &&    //and have a nexus to build it in
          ((mostCost==0)||((mostCost+50)>mSearchState.mMinerals)||mSearchState.mTrainingCompleteFrames[0][0]<=mSearchState.mGameStateFrame) ) //if we could instantly build something else, & a probe is already building, dont do a probe right now
      {
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
      int maxRax = ((int)mSearchState.mMineralsPerMinute)/(200+50);  //max spend rate = 4 marines a minute, +1/2 depot
      int usableFarm = mSearchState.mFarmCapacity + 16*mSearchState.mFarmBuilding;

      if ((mSearchState.mCurrentFarm+2) <= usableFarm &&  //have (or will have) farm available
          mSearchState.mTrainingCompleteFrames[4].size() > 0 && //have academy (means should have rax)
          mSearchState.mGasBuildingsDone > 0)   //have refinery
        //mSearchState.mTrainingCompleteFrames[3].size() > 0)   //have refinery
      {
         if (mSearchState.mUnitCounts[2] < mSearchState.mUnitCounts[1])
         {
            movesPtr->push_back(eTerran_Firebat);
            mostCost = max(mostCost,75);
         }
         if (mSearchState.mUnitCounts[3] < mSearchState.mUnitCounts[1])
         {
            movesPtr->push_back(eTerran_Medic);
            mostCost = max(mostCost,75);
         }
      }

      if ((mSearchState.mCurrentFarm+2) <= usableFarm &&  //have (or will have) farm available
          raxCnt >0) //and have a barracks to build it in
      {
         movesPtr->push_back(eTerran_Marine);
         mostCost = max(mostCost,50);
      }

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

      if (mSearchState.mTrainingCompleteFrames[3].size() <= 0 && //if dont have refinery yet
          mSearchState.mUnitCounts[0] >= 9)                      //have at least 9 workers
      {
         movesPtr->push_back(eTerran_Refinery);
         mostCost = max(mostCost,100);
      }

      if (mSearchState.mTrainingCompleteFrames[2].size() > 0 &&   //have rax
          mSearchState.mTrainingCompleteFrames[4].size() <= 0)    //dont have academy yet
      {
         movesPtr->push_back(eTerran_Academy);
         mostCost = max(mostCost,200);
      }

      //wait to build an extra CC until you have at least 12 workers per existing CC
      if (mSearchState.mUnitCounts[0] >= (int)(12*mSearchState.mTrainingCompleteFrames[0].size()))
      {
         movesPtr->push_back(eTerran_Command_Center);
         mostCost = max(mostCost,400);
      }

      //TODO: count mineral spots, for now... use 8
      if (mSearchState.mUnitCounts[0] < 20 && //two per mineral spot +1 for building + 3 gas
         (mSearchState.mCurrentFarm+2) <= usableFarm &&  //and have (or will have) farm available
          mSearchState.mTrainingCompleteFrames[0].size() >0 &&    //and have a command center to build it in
          ((mostCost==0)||((mostCost+50)>mSearchState.mMinerals)||mSearchState.mTrainingCompleteFrames[0][0]<=mSearchState.mGameStateFrame) ) //if we could instantly build something else, & an scv is already building, dont do an scv right now
      {
         movesPtr->push_back(eTerran_SCV);
      }
   }
   else //(mMyRace == eZERG)
   {
      //eZerg_Zergling      =  37,
      //eZerg_Drone         =  41,
      //eZerg_Overlord      =  42,
      //eZerg_Spawning_Pool = 142,
      int larvaCnt = 0;
      for (int i=0; i<(int)mSearchState.mLarva.size(); ++i)
      {
         larvaCnt += mSearchState.mLarva[i].NumLarva;
      }

      int poolCnt = mSearchState.mTrainingCompleteFrames[2].size();

      if ((mSearchState.mCurrentFarm+2) <= (mSearchState.mFarmCapacity + 16*mSearchState.mFarmBuilding) &&  //have (or will have) farm available
          poolCnt >0) //and have a pool to enable lings
      {
         movesPtr->push_back(eZerg_Zergling);
         //mostCost = max(mostCost,50);
      }

     //int maxRax = ((int)mSearchState.mMineralsPerMinute)/(200+50);  //max spend rate = 4 marines a minute, +1/2 depot
    //if ((mSearchState.mTrainingCompleteFrames[0].size()*4) > mSearchState.mTrainingCompleteFrames[2].size())
      if (poolCnt<=0 && mSearchState.mUnitCounts[0]>0)// || raxCnt < maxRax)
      {
         movesPtr->push_back(eZerg_Spawning_Pool);
         //mostCost = max(mostCost,200);
      }

      if ((mSearchState.mFarmCapacity+16*mSearchState.mFarmBuilding-mSearchState.mCurrentFarm) <= (2*mSearchState.mMaxTrainingCapacity))
      {
         movesPtr->push_back(eZerg_Overlord);
         //mostCost = max(mostCost,100);
      }

      int hatchCnt = mSearchState.mTrainingCompleteFrames[0].size();
      int maxHatch = ((int)mSearchState.mMineralsPerMinute)/(268);  //max spend rate = 4.2857 drones a minute, plus enough farm for that many
      if ((hatchCnt==0 || hatchCnt < maxHatch) && mSearchState.mUnitCounts[0]>0)
      {
         movesPtr->push_back(eZerg_Hatchery);
         //mostCost = max(mostCost,300);
      }

      //TODO: count mineral spots, for now... use 8
      if (mSearchState.mUnitCounts[0] < 17 && //two per mineral spot +1 for building
         (mSearchState.mCurrentFarm+2) <= (mSearchState.mFarmCapacity + 16*mSearchState.mFarmBuilding) && //and have (or will have) farm available
          mSearchState.mTrainingCompleteFrames[0].size() >0)   //  &&   //and have a hatchery to build it from
          //((mostCost==0)||((mostCost+50)>mSearchState.mMinerals))) //if we could instantly build something else, & a drone is already building, dont do a drone right now
      {
         movesPtr->push_back(eZerg_Drone);
      }
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


bool MacroSearch::DoMove(int aMove, int targetFrame)
{
   //int price          = aMove.mineralPrice();
   //int buildTime      = aMove.buildTime();   //frames
   //int supplyRequired = aMove.supplyRequired();

   int moveStartTime = mSearchState.mGameStateFrame;
   int price = 0;
   int gasprice = 0;
 //QueuedMove newMove;
   QueuedMove* newMovePtr;

   //switch (aMove.getID())
   switch (aMove)
   {
   case eProtoss_Probe:
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

   case eProtoss_Zealot:
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

   case eProtoss_Dragoon:
      {
         price              = BWAPI::UnitTypes::Protoss_Dragoon.mineralPrice();
         gasprice           = BWAPI::UnitTypes::Protoss_Dragoon.gasPrice();
         int buildTime      = BWAPI::UnitTypes::Protoss_Dragoon.buildTime();
         int supplyRequired = 4;
         supplyRequired = BWAPI::UnitTypes::Protoss_Dragoon.supplyRequired();

         //int buildingIndex = 2;  //gateway
         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }

         int frameGasAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mGas < gasprice) {
          //frameGasAvailable += (int)((gasprice-mSearchState.mGas)*FramePerMinute/(GasPerMinute*mSearchState.mTrainingCompleteFrames[3].size()));  //assumes refineries have 3 workers each
            frameGasAvailable += (int)((gasprice-mSearchState.mGas)*FramePerMinute/(GasPerMinute*mSearchState.mGasBuildingsDone));  //assumes assimilators have 3 workers each
         }
         moveStartTime = max(frameMineralAvailable, frameGasAvailable);

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
         moveStartTime = max(moveStartTime, frameBuildingAvailable);

         if ((mSearchState.mCurrentFarm+supplyRequired)>mSearchState.mFarmCapacity)
         {
            //find when pylon will be done
            assert(mSearchState.mFarmBuilding>0);
            assert(mSearchState.mNextFarmDoneIndex < (int)mSearchState.mTrainingCompleteFrames[1].size());
            int farmAvailable = mSearchState.mTrainingCompleteFrames[1][mSearchState.mNextFarmDoneIndex];
            //farmAvailable = mSearchState.mTrainingCompleteFrames[1].front();  //TODO - find right pylon! not necessarily front in vector
            moveStartTime = max(moveStartTime, farmAvailable);
         }

         //0. find build time
         //start time is latest of these three times
         //moveStartTime = max(frameMineralAvailable, max(frameBuildingAvailable, farmAvailable));
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }

         mSearchState.mCurrentFarm += supplyRequired;   //supply taken when unit building started

         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, mSearchState.mTrainingCompleteFrames[2][gateIndex], mSearchState.mGameStateFrame, aMove);

         mSearchState.mTrainingCompleteFrames[2][gateIndex] = (moveStartTime+buildTime);
         //mUnitCounts[1]   ++; //zealot  //TODO, increment this when unit done building
      }break;

   case eProtoss_Pylon:
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

   case eProtoss_Gateway:
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

   case eProtoss_Nexus:
      {
         price              = 400;
         int buildTime      = BWAPI::UnitTypes::Protoss_Nexus.buildTime();

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

         mSearchState.mTrainingCompleteFrames[0].push_back(moveStartTime+buildTime);


         //TODO what else?

      }break;

   case eProtoss_Cybernetics_Core:
      {
         price              = 200;
         price              = BWAPI::UnitTypes::Protoss_Cybernetics_Core.mineralPrice();
         int buildTime      = BWAPI::UnitTypes::Protoss_Cybernetics_Core.buildTime();

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

         mSearchState.mTrainingCompleteFrames[4].push_back(moveStartTime+buildTime);


         //TODO what else?


      }break;

   case eProtoss_Assimilator:
      {
         price              = 100;
         price              = BWAPI::UnitTypes::Protoss_Assimilator.mineralPrice();
         int buildTime      = BWAPI::UnitTypes::Protoss_Assimilator.buildTime();

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

         mSearchState.mTrainingCompleteFrames[3].push_back(moveStartTime+buildTime);

         //TODO what else?
      }break;

   case eProtoss_Forge:
      {
         price              = 150;
         price              = BWAPI::UnitTypes::Protoss_Forge.mineralPrice();
         int buildTime      = BWAPI::UnitTypes::Protoss_Forge.buildTime();

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

         mSearchState.mTrainingCompleteFrames[5].push_back(moveStartTime+buildTime);


         //TODO what else?


      }break;

   case eProtoss_Photon_Cannon:
      {
         price              = 150;
         price              = BWAPI::UnitTypes::Protoss_Photon_Cannon.mineralPrice();
         int buildTime      = BWAPI::UnitTypes::Protoss_Photon_Cannon.buildTime();

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

         mSearchState.mTrainingCompleteFrames[6].push_back(moveStartTime+buildTime);


         //TODO what else?

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


   case eTerran_SCV:
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
         int frameComplete = moveStartTime+buildTime;
         if (frameComplete > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         //2. adjust supply
         mSearchState.mCurrentFarm += supplyRequired;   //supply taken when unit training started
         newMovePtr = new QueuedMove(moveStartTime, frameComplete, mSearchState.mTrainingCompleteFrames[0][0], mSearchState.mGameStateFrame, aMove);
         //3. adjust training times
         mSearchState.mTrainingCompleteFrames[0][0] = frameComplete;//TODO - find right cc
         //mUnitCounts[0]   ++; //probe   //TODO, increment this when unit done building
         //mMineralsPerMinute += MineralsPerMinute;       //TODO, increment this when unit done building
      }break;

   case eTerran_Marine:
      {
         price              = 50;
         int buildTime      = 360; //frames
         int supplyRequired = 2;

         //find out at what time mMinerals are available
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
         moveStartTime = max(frameMineralAvailable, frameBuildingAvailable);

         if ((mSearchState.mCurrentFarm+supplyRequired)>mSearchState.mFarmCapacity)
         {
            //find when depot will be done
            assert(mSearchState.mFarmBuilding>0);
            assert(mSearchState.mNextFarmDoneIndex < (int)mSearchState.mTrainingCompleteFrames[1].size());
            moveStartTime = max(moveStartTime, mSearchState.mTrainingCompleteFrames[1][mSearchState.mNextFarmDoneIndex]);
         }

         //0. find build time
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         mSearchState.mCurrentFarm += supplyRequired;   //supply taken when unit building started
         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, mSearchState.mTrainingCompleteFrames[2][raxIndex], mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[2][raxIndex] = (moveStartTime+buildTime);
      }break;


   case eTerran_Firebat:
      {
         price              = 50;   //BWAPI::UnitTypes::Terran_Firebat.mineralPrice();//50?
         gasprice           = 25;   //BWAPI::UnitTypes::Terran_Firebat.gasPrice();    //25?
         int buildTime      = 360;  //BWAPI::UnitTypes::Terran_Firebat.buildTime();   //360 frames
         int supplyRequired = 2;

         //find out at what time resources are available
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         int frameGasAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mGas < gasprice) {
          //frameGasAvailable += (int)((gasprice-mSearchState.mGas)*FramePerMinute/(GasPerMinute*mSearchState.mTrainingCompleteFrames[3].size()));  //assumes refineries have 3 workers each
            frameGasAvailable += (int)((gasprice-mSearchState.mGas)*FramePerMinute/(GasPerMinute*mSearchState.mGasBuildingsDone));  //assumes refineries have 3 workers each
         }
         moveStartTime = max(frameMineralAvailable, frameGasAvailable);

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
         moveStartTime = max(moveStartTime, frameBuildingAvailable);

         if ((mSearchState.mCurrentFarm+supplyRequired)>mSearchState.mFarmCapacity)
         {
            //find when depot will be done
            assert(mSearchState.mFarmBuilding>0);
            assert(mSearchState.mNextFarmDoneIndex < (int)mSearchState.mTrainingCompleteFrames[1].size());
            moveStartTime = max(moveStartTime, mSearchState.mTrainingCompleteFrames[1][mSearchState.mNextFarmDoneIndex]);
         }

         //tech available
         moveStartTime = max(moveStartTime, mSearchState.mTrainingCompleteFrames[4].front());

         //moveStartTime = max(frameMineralAvailable, max(frameBuildingAvailable, farmAvailable));
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         mSearchState.mCurrentFarm += supplyRequired;   //supply taken when unit building started
         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, mSearchState.mTrainingCompleteFrames[2][raxIndex], mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[2][raxIndex] = (moveStartTime+buildTime);
      }break;

   case eTerran_Medic:
      {
         price              = 50;   //BWAPI::UnitTypes::Terran_Medic.mineralPrice();//50?
         gasprice           = 25;   //BWAPI::UnitTypes::Terran_Medic.gasPrice();    //25?
         int buildTime      = 450;  //BWAPI::UnitTypes::Terran_Medic.buildTime();   //450 frames
         int supplyRequired = 2;

         //find out at what time resources are available
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         int frameGasAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mGas < gasprice) {
          //frameGasAvailable += (int)((gasprice-mSearchState.mGas)*FramePerMinute/(GasPerMinute*mSearchState.mTrainingCompleteFrames[3].size()));  //assumes refineries have 3 workers each
            frameGasAvailable += (int)((gasprice-mSearchState.mGas)*FramePerMinute/(GasPerMinute*mSearchState.mGasBuildingsDone));  //assumes refineries have 3 workers each
         }
         moveStartTime = max(frameMineralAvailable, frameGasAvailable);

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
         moveStartTime = max(moveStartTime, frameBuildingAvailable);

         if ((mSearchState.mCurrentFarm+supplyRequired)>mSearchState.mFarmCapacity)
         {
            //find when depot will be done
            assert(mSearchState.mFarmBuilding>0);
            assert(mSearchState.mNextFarmDoneIndex < (int)mSearchState.mTrainingCompleteFrames[1].size());
            moveStartTime = max(moveStartTime, mSearchState.mTrainingCompleteFrames[1][mSearchState.mNextFarmDoneIndex]);
         }

         //tech available
         moveStartTime = max(moveStartTime, mSearchState.mTrainingCompleteFrames[4].front());

         //moveStartTime = max(frameMineralAvailable, max(frameBuildingAvailable, farmAvailable));
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         mSearchState.mCurrentFarm += supplyRequired;   //supply taken when unit building started
         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, mSearchState.mTrainingCompleteFrames[2][raxIndex], mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[2][raxIndex] = (moveStartTime+buildTime);
      }break;

   case eTerran_Refinery:
      {
         price              = 100;
         int buildTime      = BWAPI::UnitTypes::Terran_Refinery.buildTime(); //frames

         //find out at what time mMinerals are available
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         //start time is mineral time
         moveStartTime = frameMineralAvailable;
         int frameComplete = moveStartTime + buildTime;
         if (frameComplete > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         newMovePtr = new QueuedMove(moveStartTime, frameComplete, 0, mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[3].push_back(frameComplete);

         //adjust minerals ourselves here, & then take out scv from production
         int dt = moveStartTime - mSearchState.mGameStateFrame;
         mSearchState.mMinerals += ((mSearchState.mMineralsPerMinute * dt)/FramePerMinute);
         mSearchState.mGas      += (GasPerMinute*mSearchState.mGasBuildingsDone*dt/FramePerMinute);

         mSearchState.mGameStateFrame = moveStartTime;
         //take out an scv, to perform construction
         mSearchState.mUnitCounts[0]--; //scv
         mSearchState.mMineralsPerMinute -= MineralsPerMinute;

     }break;

   case eTerran_Academy:
      {
         price              = 200;
         int buildTime      = BWAPI::UnitTypes::Terran_Academy.buildTime(); //frames

         //find out at what time mMinerals are available
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
         int frameComplete = moveStartTime + buildTime;
         newMovePtr = new QueuedMove(moveStartTime, frameComplete, 0, mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[4].push_back(frameComplete);

         //adjust minerals ourselves here, & then take out scv from production
         int dt = moveStartTime - mSearchState.mGameStateFrame;
         mSearchState.mMinerals += ((mSearchState.mMineralsPerMinute * dt)/FramePerMinute);
         mSearchState.mGas      += (GasPerMinute*mSearchState.mGasBuildingsDone*dt/FramePerMinute);
         mSearchState.mGameStateFrame = moveStartTime;
         //take out an scv, to perform construction
         mSearchState.mUnitCounts[0]--; //scv
         mSearchState.mMineralsPerMinute -= MineralsPerMinute;

     }break;


   case eTerran_Command_Center:
      {
         price              = 400;
         int buildTime      = BWAPI::UnitTypes::Terran_Command_Center.buildTime(); //frames

         //find out at what time mMinerals are available
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
         int frameComplete = moveStartTime + buildTime;
         newMovePtr = new QueuedMove(moveStartTime, frameComplete, 0, mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[0].push_back(frameComplete);

         //adjust minerals ourselves here, & then take out scv from production
         int dt = moveStartTime - mSearchState.mGameStateFrame;
         mSearchState.mMinerals += ((mSearchState.mMineralsPerMinute * dt)/FramePerMinute);
         mSearchState.mGas      += (GasPerMinute*mSearchState.mGasBuildingsDone*dt/FramePerMinute);
         mSearchState.mGameStateFrame = moveStartTime;
         //take out an scv, to perform construction
         mSearchState.mUnitCounts[0]--; //scv
         mSearchState.mMineralsPerMinute -= MineralsPerMinute;

     }break;

   case eTerran_Supply_Depot:
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
         int frameComplete = moveStartTime + buildTime;
         newMovePtr = new QueuedMove(moveStartTime, frameComplete, 0, mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[1].push_back(frameComplete);
         mSearchState.mFarmBuilding++;
         //mFarmCapacity += 16; //TODO, increment this when unit done building

         //adjust minerals ourselves here, & then take out scv from production

         int dt = moveStartTime - mSearchState.mGameStateFrame;
         mSearchState.mMinerals += ((mSearchState.mMineralsPerMinute * dt)/FramePerMinute);
         mSearchState.mGas      += (GasPerMinute*mSearchState.mGasBuildingsDone*dt/FramePerMinute);
         mSearchState.mGameStateFrame = moveStartTime;
         //take out an scv, to perform construction
         mSearchState.mUnitCounts[0]--; //scv
         mSearchState.mMineralsPerMinute -= MineralsPerMinute;

     }break;

   case eTerran_Barracks:
      {
         price              = 150;
         int buildTime      = 1200; //frames

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         moveStartTime = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            moveStartTime += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         int frameComplete = moveStartTime + buildTime;
         newMovePtr = new QueuedMove(moveStartTime, frameComplete, 0, mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[2].push_back(frameComplete);

         //adjust minerals ourselves here, & then take out scv from production
         int dt = moveStartTime - mSearchState.mGameStateFrame;
         mSearchState.mMinerals += ((mSearchState.mMineralsPerMinute * dt)/FramePerMinute);
         mSearchState.mGas      += (GasPerMinute*mSearchState.mGasBuildingsDone*dt/FramePerMinute);
         mSearchState.mGameStateFrame = moveStartTime;
         //take out an scv, to perform construction
         mSearchState.mUnitCounts[0]--; //scv
         mSearchState.mMineralsPerMinute -= MineralsPerMinute;

      }break;


   case eZerg_Drone:
      {
         price              = 50;
         int buildTime      = 300;   //frames
         int supplyRequired = 2;
         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);  //rough check, doesn't account for drone that could finish training by then
         }
         //find best hatchery to build from
         int hatchIndex = 0;
         for (int i=1; i<(int)mSearchState.mLarva.size(); ++i)
         {
            if (mSearchState.mLarva[i].NumLarva > mSearchState.mLarva[hatchIndex].NumLarva ||
                (mSearchState.mLarva[i].NumLarva == mSearchState.mLarva[hatchIndex].NumLarva &&
                 mSearchState.mLarva[i].NextLarvaSpawnFrame < mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame))
            {
               hatchIndex = i;
            }
         }
         //find out at what time a larva is available
         int frameLarvaAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mLarva[hatchIndex].NumLarva <= 0)
         {
            //calculate time
            frameLarvaAvailable = max(mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame, mSearchState.mGameStateFrame);  //just in case NextLarvaSpawnFrame is way in the past & a new larva will spawn
         }
         //find out at what time farm will be available
         int farmAvailable = mSearchState.mGameStateFrame;
         if ((mSearchState.mCurrentFarm+supplyRequired)>mSearchState.mFarmCapacity)
         {
            //find when ovie will be done
            assert(mSearchState.mFarmBuilding>0);
            assert(mSearchState.mNextFarmDoneIndex < (int)mSearchState.mTrainingCompleteFrames[1].size());
            farmAvailable = mSearchState.mTrainingCompleteFrames[1][mSearchState.mNextFarmDoneIndex];
         }
         //0. find build time
         //start time is latest of these three times
         moveStartTime = max(frameMineralAvailable, max(frameLarvaAvailable, farmAvailable));
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         //adjust larva times
         AdvanceLarvaUntil(moveStartTime);
         //adjust supply
         mSearchState.mCurrentFarm += supplyRequired;   //supply taken when unit training started
         newMovePtr = new QueuedMove(moveStartTime, moveStartTime+buildTime, mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame, mSearchState.mGameStateFrame, aMove);

         if (mSearchState.mLarva[hatchIndex].NumLarva == 3)
         {
            mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame = moveStartTime + LarvaFrameTime;
         }
         mSearchState.mLarva[hatchIndex].NumLarva--;
         //mUnitCounts[0]   ++; //drone //increment this when unit done building
         //mMineralsPerMinute += MineralsPerMinute;       //increment this when unit done building
      }break;

   case eZerg_Zergling:
      {
         price              = 50;
         int buildTime      = 420; //frames
         int supplyRequired = 2;

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);  //rough check, doesn't account for drone that could finish training by then
         }

         //find best hatchery to build from
         int hatchIndex = 0;
         for (int i=1; i<(int)mSearchState.mLarva.size(); ++i)
         {
            if (mSearchState.mLarva[i].NumLarva > mSearchState.mLarva[hatchIndex].NumLarva ||
                (mSearchState.mLarva[i].NumLarva == mSearchState.mLarva[hatchIndex].NumLarva &&
                     mSearchState.mLarva[i].NextLarvaSpawnFrame < mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame))
            {
               hatchIndex = i;
            }
         }

         //find out at what time a larva is available
         int frameLarvaAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mLarva[hatchIndex].NumLarva <= 0) //TODO find right hatchery
         {
            //calculate time
            frameLarvaAvailable = max(mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame, mSearchState.mGameStateFrame);  //just in case NextLarvaSpawnFrame is way in the past & a new larva will spawn
         }
         //find out at what time farm will be available
         int farmAvailable = mSearchState.mGameStateFrame;
         if ((mSearchState.mCurrentFarm+supplyRequired)>mSearchState.mFarmCapacity)
         {
            //find when ovie will be done
            assert(mSearchState.mFarmBuilding>0);
            assert(mSearchState.mNextFarmDoneIndex < (int)mSearchState.mTrainingCompleteFrames[1].size());
            farmAvailable = mSearchState.mTrainingCompleteFrames[1][mSearchState.mNextFarmDoneIndex];
         }
         //find out when pool done
         int techDone = mSearchState.mTrainingCompleteFrames[2][0];
         //0. find build time
         //start time is latest of these four times
         moveStartTime = max(max(frameMineralAvailable,techDone), max(frameLarvaAvailable, farmAvailable));
         int frameComplete = moveStartTime + buildTime;
         if (frameComplete > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         //adjust larva times
         AdvanceLarvaUntil(moveStartTime);
         //adjust supply
         mSearchState.mCurrentFarm += supplyRequired;   //supply taken when unit training started
         newMovePtr = new QueuedMove(moveStartTime, frameComplete, mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame, mSearchState.mGameStateFrame, aMove);
         if (mSearchState.mLarva[hatchIndex].NumLarva == 3)
         {
            mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame = moveStartTime + LarvaFrameTime;
         }
         mSearchState.mLarva[hatchIndex].NumLarva--;
         //mUnitCounts[0]   ++; //drone //increment this when unit done building
         //mMineralsPerMinute += MineralsPerMinute;       //increment this when unit done building
      }break;

   case eZerg_Overlord:
      {
         price              = 100;
         int buildTime      = 600; //frames

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            frameMineralAvailable += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }

         //find best hatchery to build from
         int hatchIndex = 0;
         for (int i=1; i<(int)mSearchState.mLarva.size(); ++i)
         {
            if (mSearchState.mLarva[i].NumLarva > mSearchState.mLarva[hatchIndex].NumLarva ||
                (mSearchState.mLarva[i].NumLarva == mSearchState.mLarva[hatchIndex].NumLarva &&
                     mSearchState.mLarva[i].NextLarvaSpawnFrame < mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame))
            {
               hatchIndex = i;
            }
         }

         //find out at what time a larva is available
         int frameLarvaAvailable = mSearchState.mGameStateFrame;
         if (mSearchState.mLarva[hatchIndex].NumLarva <= 0) //TODO find right hatchery
         {
            //calculate time
            frameLarvaAvailable = max(mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame, mSearchState.mGameStateFrame);  //just in case NextLarvaSpawnFrame is way in the past & a new larva will spawn
         }

         //start time is max of these two
         moveStartTime = max(frameMineralAvailable, frameLarvaAvailable);
         int frameComplete = moveStartTime + buildTime;
         if (frameComplete > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         //adjust larva times
         AdvanceLarvaUntil(moveStartTime);

         newMovePtr = new QueuedMove(moveStartTime, frameComplete, mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame, mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[1].push_back(frameComplete);
         mSearchState.mFarmBuilding++;

         if (mSearchState.mLarva[hatchIndex].NumLarva == 3)
         {
            mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame = moveStartTime + LarvaFrameTime;
         }
         mSearchState.mLarva[hatchIndex].NumLarva--;

         //mFarmCapacity += 16; //TODO, increment this when unit done building
     }break;


   case eZerg_Hatchery:
      {
         price = 300;
         int buildTime = BWAPI::UnitTypes::Zerg_Hatchery.buildTime();   //frames

         moveStartTime = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            moveStartTime += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         int frameComplete = moveStartTime + buildTime;
         newMovePtr = new QueuedMove(moveStartTime, frameComplete, 0, mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[0].push_back(frameComplete);

         //adjust minerals ourselves here, & then take out drone from production
         int dt = moveStartTime - mSearchState.mGameStateFrame;
         mSearchState.mMinerals += ((mSearchState.mMineralsPerMinute * dt)/FramePerMinute);
         mSearchState.mGameStateFrame = moveStartTime;
         //take out an scv, to perform construction
         mSearchState.mUnitCounts[0]--; //drone
         mSearchState.mMineralsPerMinute -= MineralsPerMinute;

      }break;

   case eZerg_Spawning_Pool:
      {
         price              = 200;
         int buildTime      = 1200; //frames

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         moveStartTime = mSearchState.mGameStateFrame;
         if (mSearchState.mMinerals < price) {
            moveStartTime += (int)((price-mSearchState.mMinerals)*FramePerMinute/mSearchState.mMineralsPerMinute);
         }
         if ((moveStartTime+buildTime) > targetFrame)
         {
            return false;  //no need to search this move, its past our end search time
         }
         int frameComplete = moveStartTime + buildTime;
         newMovePtr = new QueuedMove(moveStartTime, frameComplete, 0, mSearchState.mGameStateFrame, aMove);
         mSearchState.mTrainingCompleteFrames[2].push_back(frameComplete);

         //adjust minerals ourselves here, & then take out drone from production
         int dt = moveStartTime - mSearchState.mGameStateFrame;
         mSearchState.mMinerals += ((mSearchState.mMineralsPerMinute * dt)/FramePerMinute);
         mSearchState.mGameStateFrame = moveStartTime;
         //take out an scv, to perform construction
         mSearchState.mUnitCounts[0]--; //drone
         mSearchState.mMineralsPerMinute -= MineralsPerMinute;
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

   mSearchState.mGas -= gasprice;
   mSearchState.mGas += (GasPerMinute*mSearchState.mGasBuildingsDone*dt/FramePerMinute);

   AddMove(newMovePtr);
   //assert(moveStartTime >= mSearchState.mGameStateFrame);
   AdvanceQueuedEventsUntil(moveStartTime);
   mSearchState.mGameStateFrame = moveStartTime;
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
   //later events are further back in the list
   mLastEventIter++;
   while (mLastEventIter != mEventQueue.end() && (*mLastEventIter)->frameComplete <= targetFrame)
   {
      QueuedMove* movePtr = *mLastEventIter;
      //process it, complete the move (finish building, training, or travelling)
    //switch (mLastEventIter->move.getID())
      switch (movePtr->move)
      {
      case eProtoss_Probe:
         {
            mSearchState.mUnitCounts[0]++; //probe
            mSearchState.mMineralsPerMinute += MineralsPerMinute;
            //just add in mineral change from this probe only
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;
      case eProtoss_Zealot:
         {
            mSearchState.mUnitCounts[1]++; //zealot
         }break;
      case eProtoss_Pylon:
         {
            mSearchState.mFarmCapacity += 16;
            mSearchState.mFarmBuilding--;
            mSearchState.mNextFarmDoneIndex++;
         }break;
      case eProtoss_Gateway:
         {
            mSearchState.mMaxTrainingCapacity += 4;  //zealot? //TODO: find largest unit buildable by this building
         }break;
      //case eAttack:
      //   {
      //      mSearchState.mForceAttacking = true;
      //   }
      case eProtoss_Dragoon:
         {
            mSearchState.mUnitCounts[2]++; //dragoon
         }break;

      case eProtoss_Assimilator:
         {
            mSearchState.mGasBuildingsDone++;
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mSearchState.mGas += (GasPerMinute * dt / FramePerMinute);  //add gas in now
            mSearchState.mMineralsPerMinute -= (3*MineralsPerMinute);   //take 3 probes away from minerals to mine gas
            //just add in mineral change from these probes only
            mSearchState.mMinerals -= ((3*MineralsPerMinute * dt)/FramePerMinute);  //just take out mineral change from the three probes put on gas only
         }break;

      case eProtoss_Photon_Cannon:
         {
            //mSearchState.mTrainingCompleteFrames[6].pop_back(); //should remove last added cannon
         }break;

      case eProtoss_Cybernetics_Core:
         {
            //mSearchState.mTrainingCompleteFrames[4].pop_back(); //should remove last added cycore
         }break;

      case eProtoss_Nexus:
         {
            mSearchState.mMaxTrainingCapacity += 2;  //probe?
            //mSearchState.mTrainingCompleteFrames[0].pop_back(); //should remove last added nexus
         }break;

      case eProtoss_Forge:
         {
            //mSearchState.mTrainingCompleteFrames[5].pop_back(); //should remove last added forge
         }break;


      case eTerran_SCV:
         {
            mSearchState.mUnitCounts[0]++; //scv
            mSearchState.mMineralsPerMinute += MineralsPerMinute;
            //just add in mineral change from this scv only
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;
      case eTerran_Marine:
         {
            mSearchState.mUnitCounts[1]++; //marine
         }break;
      case eTerran_Firebat:
         {
            mSearchState.mUnitCounts[2]++; //firebat
         }break;
      case eTerran_Medic:
         {
            mSearchState.mUnitCounts[3]++; //medic
         }break;
      case eTerran_Supply_Depot:
         {
            mSearchState.mFarmCapacity += 16;
            mSearchState.mFarmBuilding--;
            mSearchState.mNextFarmDoneIndex++;
            //add back in the scv, its done building
            mSearchState.mUnitCounts[0]++; //scv
            mSearchState.mMineralsPerMinute += MineralsPerMinute;
            //just add in mineral change from this scv only
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;

      case eTerran_Command_Center:
         {
            mSearchState.mMaxTrainingCapacity += 2;  //scv?
            //add back in the scv, its done building
            mSearchState.mUnitCounts[0]++; //scv
            mSearchState.mMineralsPerMinute += MineralsPerMinute;
            //just add in mineral change from this scv only
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;

      case eTerran_Refinery:
         {
            mSearchState.mGasBuildingsDone++;
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mSearchState.mGas += (GasPerMinute * dt / FramePerMinute);  //add gas in now
            mSearchState.mUnitCounts[0]++; //add back in the scv, its done building
            //mSearchState.mMineralsPerMinute -= (3*MineralsPerMinute);   //take 3 scv away from minerals to mine gas
            //mSearchState.mMineralsPerMinute += MineralsPerMinute;       //add back in the scv, its done building
            mSearchState.mMineralsPerMinute -= (2*MineralsPerMinute);   //combined
            //just add in mineral change from these scv only
            //mSearchState.mMinerals += ((1*MineralsPerMinute * dt)/FramePerMinute);  //add in minerals from scv who is done building now
            //mSearchState.mMinerals -= ((3*MineralsPerMinute * dt)/FramePerMinute);  //just take out mineral change from the three scv put on gas only
            mSearchState.mMinerals -= ((2*MineralsPerMinute * dt)/FramePerMinute);  //combined
         }break;

      case eTerran_Academy:
         {
            //add back in the scv, its done building
            mSearchState.mUnitCounts[0]++; //scv
            mSearchState.mMineralsPerMinute += MineralsPerMinute;
            //just add in mineral change from this scv only
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;

      case eTerran_Barracks:
         {
            mSearchState.mMaxTrainingCapacity += 2;  //marine? //TODO: find largest unit buildable by this building

            //add back in the scv, its done building
            mSearchState.mUnitCounts[0]++; //scv
            mSearchState.mMineralsPerMinute += MineralsPerMinute;
            //just add in mineral change from this scv only
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;

      case eZerg_Drone:
         {
            mSearchState.mUnitCounts[0]++; //drone
            mSearchState.mMineralsPerMinute += MineralsPerMinute;
            //just add in mineral change from this drone only
            int dt = targetFrame - movePtr->frameComplete; //should be positive
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;
      case eZerg_Zergling:
         {
            mSearchState.mUnitCounts[1]++; //zergling
         }break;
      case eZerg_Overlord:
         {
            mSearchState.mFarmCapacity += 16;
            mSearchState.mFarmBuilding--;
            mSearchState.mNextFarmDoneIndex++;
         }break;
      case eZerg_Hatchery:
         {
            //we have more larva to use now
            mSearchState.mLarva.push_back(Hatchery(1, movePtr->frameComplete + LarvaFrameTime));
            mSearchState.mMaxTrainingCapacity += 2;
         }break;
      case eZerg_Spawning_Pool:
         {
            //nothing?
         }break;

      default:
         {
            //unknown move, shouldn't be here
         }break;
      };
      mLastEventIter++;
   }
   //decrement iterator & exit
   mLastEventIter--;
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
      case eProtoss_Probe:
         {
            mSearchState.mUnitCounts[0]--; //probe
            mSearchState.mMineralsPerMinute -= MineralsPerMinute;
            //adjust mineral change from this one probe only
            int dt = movePtr->frameComplete - mSearchState.mGameStateFrame; //should be negative
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;
      case eProtoss_Zealot:
         {
            mSearchState.mUnitCounts[1]--; //zealot
         }break;
      case eProtoss_Pylon:
         {
            mSearchState.mFarmCapacity -= 16;
            mSearchState.mFarmBuilding++;
            mSearchState.mNextFarmDoneIndex--;
         }break;
      case eProtoss_Gateway:
         {
            mSearchState.mMaxTrainingCapacity -= 4;  //zealot? //TODO: find largest unit buildable by this building
         }break;

      case eProtoss_Dragoon:
         {
            mSearchState.mUnitCounts[2]--; //dragoon
         }break;

      case eProtoss_Assimilator:
         {
            //whatever changes we make to 'mMineralsPerMinute' or 'mGasBuildingsDone' here will be added into the WHOLE calculation in UndoMove()
            //if we reduce either rate,   then take out their production for 'dt' here, because it wont be taken out in UndoMove()
            //if we increase either rate, then add back in their production for 'dt' here, because too much will be taken out in UndoMove()
            mSearchState.mGasBuildingsDone--;
            int dt = movePtr->frameComplete - mSearchState.mGameStateFrame; //should be negative
            mSearchState.mGas += (GasPerMinute * dt / FramePerMinute);  //(negative dt)
            mSearchState.mMineralsPerMinute += (3*MineralsPerMinute);   //take 3 probes back from gas to mine minerals
            mSearchState.mMinerals -= ((3*MineralsPerMinute * dt)/FramePerMinute);  //(negative dt)
         }break;

      case eProtoss_Photon_Cannon:
         {
            //mSearchState.mTrainingCompleteFrames[6].pop_back(); //should remove last added cannon
         }break;

      case eProtoss_Cybernetics_Core:
         {
            //mSearchState.mTrainingCompleteFrames[4].pop_back(); //should remove last added cycore
         }break;

      case eProtoss_Nexus:
         {
            mSearchState.mMaxTrainingCapacity -= 2;  //probe?
            //mSearchState.mTrainingCompleteFrames[0].pop_back(); //should remove last added nexus
         }break;

      case eProtoss_Forge:
         {
            //mSearchState.mTrainingCompleteFrames[5].pop_back(); //should remove last added forge
         }break;


      //case eAttack:
      //   {
      //      mSearchState.mForceAttacking = false;
      //   }
      case eTerran_SCV:
         {
            mSearchState.mUnitCounts[0]--; //scv
            mSearchState.mMineralsPerMinute -= MineralsPerMinute;
            //adjust mineral change from this one scv only
            int dt = movePtr->frameComplete - mSearchState.mGameStateFrame; //should be negative
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;
      case eTerran_Marine:
         {
            mSearchState.mUnitCounts[1]--; //marine
         }break;

      case eTerran_Firebat:
         {
            mSearchState.mUnitCounts[2]--; //firebat
         }break;
      case eTerran_Medic:
         {
            mSearchState.mUnitCounts[3]--; //medic
         }break;
      case eTerran_Command_Center:
         {
            mSearchState.mMaxTrainingCapacity -= 2;  //scv?
            //whatever changes we make to 'mMineralsPerMinute' or 'mGasBuildingsDone' here will be added into the WHOLE calculation in UndoMove()
            //if we reduce either rate,   then take out their production for 'dt' here, because it wont be taken out in UndoMove()
            //if we increase either rate, then add back in their production for 'dt' here, because too much will be taken out in UndoMove()
            int dt = movePtr->frameComplete - mSearchState.mGameStateFrame; //should be negative
            //take out an scv, to perform construction
            mSearchState.mUnitCounts[0]--; //scv
            mSearchState.mMineralsPerMinute -= MineralsPerMinute;
            //adjust mineral change from this one scv only (because we changed mMineralsPerMinute)
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute); //negative dt
         }break;
      case eTerran_Refinery:
         {
            int dt = movePtr->frameComplete - mSearchState.mGameStateFrame; //should be negative
            //whatever changes we make to 'mMineralsPerMinute' or 'mGasBuildingsDone' here will be added into the WHOLE calculation in UndoMove()
            //if we reduce either rate,   then take out their production for 'dt' here, because it wont be taken out in UndoMove()
            //if we increase either rate, then add back in their production for 'dt' here, because too much will be taken out in UndoMove()
            mSearchState.mGasBuildingsDone--;
            mSearchState.mGas += (GasPerMinute * dt / FramePerMinute);  //(negative dt)

            mSearchState.mUnitCounts[0]--;                           //take out an scv, to perform construction
            mSearchState.mMineralsPerMinute += (2*MineralsPerMinute);//combined
            mSearchState.mMinerals -= ((2*MineralsPerMinute * dt)/FramePerMinute);  //combined (negative dt)
         }break;
      case eTerran_Academy:
         {
            //whatever changes we make to 'mMineralsPerMinute' or 'mGasBuildingsDone' here will be added into the WHOLE calculation in UndoMove()
            //if we reduce either rate,   then take out their production for 'dt' here, because it wont be taken out in UndoMove()
            //if we increase either rate, then add back in their production for 'dt' here, because too much will be taken out in UndoMove()
            int dt = movePtr->frameComplete - mSearchState.mGameStateFrame; //should be negative
            //take out an scv, to perform construction
            mSearchState.mUnitCounts[0]--; //scv
            mSearchState.mMineralsPerMinute -= MineralsPerMinute;
            //adjust mineral change from this one scv only (because we changed mMineralsPerMinute)
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute); //negative dt
         }break;
      case eTerran_Supply_Depot:
         {
            mSearchState.mFarmCapacity -= 16;
            mSearchState.mFarmBuilding++;
            mSearchState.mNextFarmDoneIndex--;

            //whatever changes we make to 'mMineralsPerMinute' or 'mGasBuildingsDone' here will be added into the WHOLE calculation in UndoMove()
            //if we reduce either rate,   then take out their production for 'dt' here, because it wont be taken out in UndoMove()
            //if we increase either rate, then add back in their production for 'dt' here, because too much will be taken out in UndoMove()
            int dt = movePtr->frameComplete - mSearchState.mGameStateFrame; //should be negative
            //take out an scv, to perform construction
            mSearchState.mUnitCounts[0]--; //scv
            mSearchState.mMineralsPerMinute -= MineralsPerMinute;
            //adjust mineral change from this one scv only
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute); //negative dt
         }break;
      case eTerran_Barracks:
         {
            mSearchState.mMaxTrainingCapacity -= 2;  //marine? //TODO: find largest unit buildable by this building

            //whatever changes we make to 'mMineralsPerMinute' or 'mGasBuildingsDone' here will be added into the WHOLE calculation in UndoMove()
            //if we reduce either rate,   then take out their production for 'dt' here, because it wont be taken out in UndoMove()
            //if we increase either rate, then add back in their production for 'dt' here, because too much will be taken out in UndoMove()
            int dt = movePtr->frameComplete - mSearchState.mGameStateFrame; //should be negative
            //take out an scv, to perform construction
            mSearchState.mUnitCounts[0]--; //scv
            mSearchState.mMineralsPerMinute -= MineralsPerMinute;
            //adjust mineral change from this one scv only (because we changed mMineralsPerMinute)
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute); //negative dt
         }break;


      case eZerg_Drone:
         {
            mSearchState.mUnitCounts[0]--; //drone
            mSearchState.mMineralsPerMinute -= MineralsPerMinute;
            //adjust mineral change from this one drone only
            int dt = movePtr->frameComplete - mSearchState.mGameStateFrame; //should be negative
            mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
         }break;
      case eZerg_Zergling:
         {
            mSearchState.mUnitCounts[1]--; //zergling
         }break;
      case eZerg_Overlord:
         {
            mSearchState.mFarmCapacity -= 16;
            mSearchState.mFarmBuilding++;
            mSearchState.mNextFarmDoneIndex--;
         }break;
      case eZerg_Hatchery:
         {
            //we have more larva to use now
            mSearchState.mLarva.pop_back();
            mSearchState.mMaxTrainingCapacity -= 2;
         }break;
      case eZerg_Spawning_Pool:
         {
            //nothing?
         }break;

      default:
         {
            //unknown move, shouldn't be here
         }break;
      };
      mLastEventIter--;
      movePtr = *mLastEventIter;
   }
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

   //int price          = undoMove.move.mineralPrice();
   //int buildTime      = undoMove.move.buildTime();   //frames
   //int supplyRequired = undoMove.move.supplyRequired();

   int price = 0; //each case statement sets the real price, minerals adjusted at bottom
   int gasprice = 0;
   switch (undoMove->move)
   {
   case eProtoss_Probe:
	   {
         price              = 50;
       //int buildTime      = 300;   //frames
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

   case eProtoss_Zealot:
      {
         price              = 100;
       //int buildTime      = 600;   //frames
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

   case eProtoss_Pylon:
      {
         price              = 100;
       //int buildTime      = 450;   //frames
         mSearchState.mFarmBuilding--;
         //TODO: adjust mGas too
         mSearchState.mTrainingCompleteFrames[1].pop_back(); //should remove last added pylon
         //QueuedMove newMove(moveStartTime, moveStartTime+buildTime, 0, mGameStateFrame, aMove);
         //mFarmCapacity -= 16;
      }break;

   case eProtoss_Gateway:
      {
         price              = 150;
       //int buildTime      = 900;   //frames
         //TODO: adjust mGas too
         mSearchState.mTrainingCompleteFrames[2].pop_back(); //should remove last added gateway
         //mMaxTrainingCapacity -= 4;  //zealot? //TODO: find largest unit buildable by this building
      }break;

   case eProtoss_Dragoon:
      {
         price              = BWAPI::UnitTypes::Protoss_Dragoon.mineralPrice();
         gasprice           = BWAPI::UnitTypes::Protoss_Dragoon.gasPrice();
       //int buildTime      = BWAPI::UnitTypes::Protoss_Dragoon.buildTime();
         int supplyRequired = 4;
         supplyRequired = BWAPI::UnitTypes::Protoss_Dragoon.supplyRequired();

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

   case eProtoss_Assimilator:
      {
         price              = 100;
       //int buildTime      = BWAPI::UnitTypes::Protoss_Assimilator.buildTime();
         //TODO: adjust mGas too
         mSearchState.mTrainingCompleteFrames[3].pop_back(); //should remove last added assimilator
      }break;

   case eProtoss_Photon_Cannon:
      {
         price              = 150;
         //int buildTime      = BWAPI::UnitTypes::Protoss_Photon_Cannon.buildTime();
         //TODO: adjust mGas too
         mSearchState.mTrainingCompleteFrames[6].pop_back(); //should remove last added cannon
      }break;

   case eProtoss_Cybernetics_Core:
      {
         price              = 200;
         price              = BWAPI::UnitTypes::Protoss_Cybernetics_Core.mineralPrice();
       //int buildTime      = BWAPI::UnitTypes::Protoss_Cybernetics_Core.buildTime();
         //TODO: adjust mGas too
         mSearchState.mTrainingCompleteFrames[4].pop_back(); //should remove last added cycore
      }break;

   case eProtoss_Nexus:
      {
         price              = 400;
       //int buildTime      = BWAPI::UnitTypes::Protoss_Nexus.buildTime();
         //TODO: adjust mGas too
         mSearchState.mTrainingCompleteFrames[0].pop_back(); //should remove last added nexus
      }break;

   case eProtoss_Forge:
      {
         price              = 150;
       //int buildTime      = BWAPI::UnitTypes::Protoss_Forge.buildTime();
         //TODO: adjust mGas too
         mSearchState.mTrainingCompleteFrames[5].pop_back(); //should remove last added forge
      }break;

   //eProtoss_Dragoon = 66,
   //eProtoss_Assimilator = 157,
   //eProtoss_Photon_Cannon = 162,
   //eProtoss_Cybernetics_Core = 164,
   //eProtoss_Nexus = 154,
   //eProtoss_Forge = 166,

   //case eAttack:
   //   {
   //      price = 0;
   //      mSearchState.mForceMoving = false;
   //      mSearchState.mForceAttacking = false;
   //   }break;

   case eTerran_SCV:
      {
         price              = 50;
       //int buildTime      = 300;  //frames
         int supplyRequired = 2;
         mSearchState.mCurrentFarm -= supplyRequired;
         mSearchState.mTrainingCompleteFrames[0][0] = undoMove->prevFrameComplete; //TODO - find right cc
      }break;

   case eTerran_Marine:
      {
         price              = 50;
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


   case eTerran_Firebat:
      {
         price              = 50;   //BWAPI::UnitTypes::Terran_Firebat.mineralPrice();//50?
         gasprice           = 25;   //BWAPI::UnitTypes::Terran_Firebat.gasPrice();    //25?
       //int buildTime      = 360;  //BWAPI::UnitTypes::Terran_Firebat.buildTime();   //360 frames
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

   case eTerran_Medic:
      {
         price              = 50;   //BWAPI::UnitTypes::Terran_Medic.mineralPrice();//50?
         gasprice           = 25;   //BWAPI::UnitTypes::Terran_Medic.gasPrice();    //25?
       //int buildTime      = 450;  //BWAPI::UnitTypes::Terran_Medic.buildTime();   //450 frames
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

   case eTerran_Refinery:
      {
         price              = 100;
         //int buildTime      = BWAPI::UnitTypes::Terran_Refinery.buildTime(); //frames

         mSearchState.mTrainingCompleteFrames[3].pop_back(); //should remove last added refinery

         //add back in an scv, no longer needed for this construction
         mSearchState.mUnitCounts[0]++; //scv
         mSearchState.mMineralsPerMinute += MineralsPerMinute;
         //adjust mineral change from this one scv only
         //subtraction below will take his production into account when it shouldn't, so add it in fake here
         int dt = mSearchState.mGameStateFrame - undoMove->frameStarted;  //should be positive
         mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
     }break;

   case eTerran_Academy:
      {
         price              = 200;

			mSearchState.mTrainingCompleteFrames[4].pop_back(); //should remove last added academy

			//add back in an scv, no longer needed for this construction
			mSearchState.mUnitCounts[0]++; //scv
			mSearchState.mMineralsPerMinute += MineralsPerMinute;
			//adjust mineral change from this one scv only
			//subtraction below will take his production into account when it shouldn't, so add it in fake here
			int dt = mSearchState.mGameStateFrame - undoMove->frameStarted;  //should be positive
			mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
     }break;

   case eTerran_Command_Center:
      {
         price              = 400;
			mSearchState.mTrainingCompleteFrames[0].pop_back(); //should remove last added cc

			//add back in an scv, no longer needed for this construction
			mSearchState.mUnitCounts[0]++; //scv
			mSearchState.mMineralsPerMinute += MineralsPerMinute;
			//adjust mineral change from this one scv only
			//subtraction below will take his production into account when it shouldn't, so add it in fake here
			int dt = mSearchState.mGameStateFrame - undoMove->frameStarted;  //should be positive
			mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
     }break;

   case eTerran_Supply_Depot:
      {
         price              = 100;
         mSearchState.mFarmBuilding--;
         mSearchState.mTrainingCompleteFrames[1].pop_back(); //should remove last added depot

         //add back in an scv, no longer needed for this construction
         mSearchState.mUnitCounts[0]++; //scv
         mSearchState.mMineralsPerMinute += MineralsPerMinute;
         //adjust mineral change from this one scv only
         //subtraction below will take his production into account when it shouldn't, so add it in fake here
         int dt = mSearchState.mGameStateFrame - undoMove->frameStarted;  //should be positive
         mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
      }break;

   case eTerran_Barracks:
      {
		   price              = 150;
			mSearchState.mTrainingCompleteFrames[2].pop_back(); //should remove last added rax

			//add back in an scv, no longer needed for this construction
			mSearchState.mUnitCounts[0]++; //scv
			mSearchState.mMineralsPerMinute += MineralsPerMinute;
			//adjust mineral change from this one scv only
			//subtraction below will take his production into account when it shouldn't, so add it in fake here
			int dt = mSearchState.mGameStateFrame - undoMove->frameStarted;  //should be positive
			mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);

      }break;

   case eZerg_Drone:
      {
         price              = 50;
         int supplyRequired = 2;
         mSearchState.mCurrentFarm -= supplyRequired;

         //find proper hatchery to undo build from
         int hatchIndex = 0;
         int leastLarva = 4;
         for (int i=0; i<(int)mSearchState.mLarva.size(); ++i)
         {
            if (mSearchState.mLarva[i].NumLarva < leastLarva &&
                (mSearchState.mLarva[i].NextLarvaSpawnFrame == undoMove->prevFrameComplete ||
                 mSearchState.mLarva[i].NextLarvaSpawnFrame == (undoMove->frameStarted + LarvaFrameTime)) )
            {
               hatchIndex = i;
            }
         }
         //if (mSearchState.mLarva[hatchIndex].NumLarva == 3)
         //{
         //   mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame = moveStartTime + LarvaFrameTime;
         //}
         //mSearchState.mLarva[hatchIndex].NumLarva--;
         mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame = undoMove->prevFrameComplete;  //could already be true, but not always
         mSearchState.mLarva[hatchIndex].NumLarva++;
         //call ReverseLarvaUntil() below
      }break;

   case eZerg_Zergling:
      {
         price              = 50;
         int supplyRequired = 2;
         mSearchState.mCurrentFarm -= supplyRequired;

         //find proper hatchery to undo build from
         int hatchIndex = 0;
         int leastLarva = 4;
         for (int i=0; i<(int)mSearchState.mLarva.size(); ++i)
         {
            if (mSearchState.mLarva[i].NumLarva < leastLarva &&
                (mSearchState.mLarva[i].NextLarvaSpawnFrame == undoMove->prevFrameComplete ||
                 mSearchState.mLarva[i].NextLarvaSpawnFrame == (undoMove->frameStarted + LarvaFrameTime)) )
            {
               hatchIndex = i;
            }
         }

         mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame = undoMove->prevFrameComplete;  //could already be true, but not always
         mSearchState.mLarva[hatchIndex].NumLarva++;
         //call ReverseLarvaUntil() below
      }break;

   case eZerg_Overlord:
      {
         price              = 100;
         mSearchState.mFarmBuilding--;
         mSearchState.mTrainingCompleteFrames[1].pop_back(); //should remove last added ovie

         //find proper hatchery to undo build from
         int hatchIndex = 0;
         int leastLarva = 4;
         for (int i=0; i<(int)mSearchState.mLarva.size(); ++i)
         {
            if (mSearchState.mLarva[i].NumLarva < leastLarva &&
                (mSearchState.mLarva[i].NextLarvaSpawnFrame == undoMove->prevFrameComplete ||
                 mSearchState.mLarva[i].NextLarvaSpawnFrame == (undoMove->frameStarted + LarvaFrameTime)) )
            {
               hatchIndex = i;
            }
         }

         mSearchState.mLarva[hatchIndex].NextLarvaSpawnFrame = undoMove->prevFrameComplete;  //could already be true, but not always
         mSearchState.mLarva[hatchIndex].NumLarva++;
         //call ReverseLarvaUntil() below
      }break;

   case eZerg_Hatchery:
      {
         price              = 300;
			mSearchState.mTrainingCompleteFrames[0].pop_back(); //should remove last added hatchery
			//add back in a drone, no longer needed for this construction
			mSearchState.mUnitCounts[0]++; //drone
			mSearchState.mMineralsPerMinute += MineralsPerMinute;
			//adjust mineral change from this one drone only
			//subtraction below will take his production into account when it shouldn't, so add it in fake here
			int dt = mSearchState.mGameStateFrame - undoMove->frameStarted;  //should be positive
			mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
      }break;

   case eZerg_Spawning_Pool:
      {
         price              = 200;
			mSearchState.mTrainingCompleteFrames[2].pop_back(); //should remove last added pool
			//add back in a drone, no longer needed for this construction
			mSearchState.mUnitCounts[0]++; //drone
			mSearchState.mMineralsPerMinute += MineralsPerMinute;
			//adjust mineral change from this one drone only
			//subtraction below will take his production into account when it shouldn't, so add it in fake here
			int dt = mSearchState.mGameStateFrame - undoMove->frameStarted;  //should be positive
			mSearchState.mMinerals += ((MineralsPerMinute * dt)/FramePerMinute);
      }break;

   default:
      {
         price = 0;
         //unknown move, shouldn't be here
         assert(false);
      }break;
   };
   mSearchState.mMinerals += price;
   mSearchState.mGas      += gasprice;
   int dt = undoMove->prevGameTime - mSearchState.mGameStateFrame;  //should be negative
   mSearchState.mMinerals += ((mSearchState.mMineralsPerMinute * dt)/FramePerMinute);
   mSearchState.mGas += (GasPerMinute * mSearchState.mGasBuildingsDone * dt / FramePerMinute); //assumes refineries have 3 workers each

   if (mMyRace == eZERG)
   {
      ReverseLarvaUntil(undoMove->prevGameTime);   //go all the way back to the previous move start time (prevGameTime)
   }

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

//assumes "mSearchState.mGameStateFrame" will be set to "targetFrame" after this method is called
void MacroSearch::ReverseLarvaUntil(int targetFrame)
{
   std::vector<Hatchery>::iterator it = mSearchState.mLarva.begin();
   for (; it!= mSearchState.mLarva.end(); it++)
   {
      if (mSearchState.mGameStateFrame > it->NextLarvaSpawnFrame)
      {
         //roll back time to right when previous larva spawned, next doesn't change, but count does
         it->NumLarva--;
      }
      while ( (it->NumLarva > 0) && ((it->NextLarvaSpawnFrame - LarvaFrameTime) > targetFrame) )
      {
         it->NextLarvaSpawnFrame -= LarvaFrameTime;
         it->NumLarva--;
      }
   }
}


//assumes "mSearchState.mGameStateFrame" will be set to "targetFrame" after this method is called
void MacroSearch::AdvanceLarvaUntil(int targetFrame)
{
   std::vector<Hatchery>::iterator it = mSearchState.mLarva.begin();
   for (; it!= mSearchState.mLarva.end(); it++)
   {
      while (it->NumLarva < 3 && it->NextLarvaSpawnFrame <= targetFrame)
      {
         it->NumLarva++;
         if (it->NumLarva < 3)
         {
            it->NextLarvaSpawnFrame += LarvaFrameTime;
         }
      }
   }
}


void MacroSearch::EvaluateState()
{
   if (mMoveStack.size()>0)
   {
      float score = mSearchState.mMinerals;
      score += mSearchState.mUnitCounts[0] * 50;
      score += mSearchState.mMineralsPerMinute;
      score += mSearchState.mTrainingCompleteFrames[1].size() * 100; //supply providers

      switch (mMyRace)
      {
      case ePROTOSS:
      {
         if (mSearchState.mTrainingCompleteFrames[4].size() > 0)
         {
            score += mSearchState.mGas * 0.642857f; //scale down because of increase mining rate (make equivalent with minerals)
            score += 200; //cybercore
         }
         //score += mSearchState.mUnitCounts[1] * 500;   //favor zealots + x5 cost
         //score += mSearchState.mTrainingCompleteFrames[2].size() * 300;  //favor gateways + x2 cost
         score += mSearchState.mUnitCounts[1] * 100;
         score += mSearchState.mUnitCounts[2] * 175 * 1.25;  //125 min, 50 gas   //account for longer build time
         score += mSearchState.mTrainingCompleteFrames[0].size() * 400; //nexus
         score += mSearchState.mTrainingCompleteFrames[2].size() * 150; //gateway
         score += mSearchState.mTrainingCompleteFrames[3].size() * 100; //assimilator
         score += mSearchState.mTrainingCompleteFrames[5].size() * 150; //forge
         score += mSearchState.mTrainingCompleteFrames[6].size() * 150; //cannons

         if (mDiminishingReturns)   //linear diminishing returns for now
         {
            //zealots worth nothing after we have 50 of them
            score -= min(mSearchState.mUnitCounts[1] * mSearchState.mUnitCounts[1], 2500);
            ////dragoons worth nothing after we have 50 of them
            //score -= min(1.75f * mSearchState.mUnitCounts[2] * mSearchState.mUnitCounts[2], 4375.0f);
            //cannons worth nothing after we have 10 of them
            score -= min(7.5f * mSearchState.mTrainingCompleteFrames[6].size() * mSearchState.mTrainingCompleteFrames[6].size(), 750.0f);
         }

      }break;
      case eTERRAN:
      {
       //score += mSearchState.mUnitCounts[1] * 250;   //favor marines + x5 cost
       //score += mSearchState.mUnitCounts[1] * 100;   //favor marines + x2 cost
         score += mSearchState.mUnitCounts[1] * 50;   //favor marines at cost
         score += min(mSearchState.mUnitCounts[2],(mSearchState.mUnitCounts[1]+2)/3)   * 300;   //favor bats    + x4 cost (but only for 1/3 as many rines as we have)
         score += min(mSearchState.mUnitCounts[3],(2*mSearchState.mUnitCounts[1]+2)/3) * 375;   //favor medics  + x4 cost (but only for 2/3 as many rines as we have) (account for 25% longer build time)
       //score += mSearchState.mTrainingCompleteFrames[0].size() * 400;  //favor cc at cost
       //score += mSearchState.mTrainingCompleteFrames[0].size() * 200;  //favor cc at half cost
         score += mSearchState.mTrainingCompleteFrames[2].size() * 150;  //favor barracks at cost
         score += mSearchState.mTrainingCompleteFrames[3].size() * 100;   //refinery
         if (mSearchState.mTrainingCompleteFrames[4].size() > 0)
         {
            score += mSearchState.mGas * 0.642857f; //scale down because of increase mining rate (make equivalent with minerals)
            score += 200;   //academy
         }
      }break;
      case eZERG:
      {
         score += mSearchState.mUnitCounts[1] * 250;   //favor lings + x5 cost
         score += mSearchState.mTrainingCompleteFrames[2].size() * 400;  //favor pool + x2 cost
         score += mSearchState.mTrainingCompleteFrames[0].size() * 300;  //favor hatchery at cost
      }break;
      };
      //return score;

      score /= (mSearchState.mGameStateFrame - mMyState.mGameStateFrame);  //take into account how long this score took to accumulate
      if (score > mMaxScore)
      {
         mMaxScore = score;
         mBestMoves.clear();
         for (std::vector<QueuedMove*>::iterator it=mMoveStack.begin(); it!=mMoveStack.end(); it++)
         {
            mBestMoves.push_back((*it)->move);
         }
      }
   }
}

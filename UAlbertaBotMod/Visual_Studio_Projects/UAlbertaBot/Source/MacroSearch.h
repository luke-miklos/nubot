
#ifndef MACROSEARCH_H
#define MACROSEARCH_H

#include <vector>
#include <deque>
#include <queue>  //for priority_queue
#include <list>
//#include "BWSAL/BuildType.h"
#include <BWAPI/UnitType.h>
#include <BWAPI.h>

const int FramePerMinute = 24*60;   //fastest game speed    //see BWAPI::Game.getFPS()
const float MineralsPerMinute = 64.8f; //matches  "MINERALS_PER_WORKER_PER_FRAME = 0.045" assuming 24 frames a second
//const float MineralsPerMinute = 50.0f; //compromise

enum RACEID
{
   ePROTOSS,
   eTERRAN,
   eZERG
};

enum MOVEID
{
   eNull_Move = -1,

   eTerran_Marine       =   0,
   eTerran_SCV          =   7,
   eTerran_Supply_Depot = 109,
   eTerran_Barracks     = 111,

   eZerg_Zergling      =  37,
   eZerg_Drone         =  41,
   eZerg_Overlord      =  42,
   eZerg_Spawning_Pool = 142,

   eProtoss_Corsair = 60,
   eProtoss_Dark_Templar = 61,
   eProtoss_Dark_Archon = 63,
   eProtoss_Probe = 64,
   eProtoss_Zealot = 65,
   eProtoss_Dragoon = 66,
   eProtoss_High_Templar = 67,
   eProtoss_Archon = 68,
   eProtoss_Shuttle = 69,
   eProtoss_Scout = 70,
   eProtoss_Arbiter = 71,
   eProtoss_Carrier = 72,
   eProtoss_Interceptor = 73,
   eProtoss_Reaver = 83,
   eProtoss_Observer = 84,
   eProtoss_Scarab = 85,
   eProtoss_Nexus = 154,
   eProtoss_Robotics_Facility = 155,
   eProtoss_Pylon = 156,
   eProtoss_Assimilator = 157,
   eProtoss_Observatory = 159,
   eProtoss_Gateway = 160,
   eProtoss_Photon_Cannon = 162,
   eProtoss_Citadel_of_Adun = 163,
   eProtoss_Cybernetics_Core = 164,
   eProtoss_Templar_Archives = 165,
   eProtoss_Forge = 166,
   eProtoss_Stargate = 167,
   eSpecial_Stasis_Cell_Prison = 168,
   eProtoss_Fleet_Beacon = 169,
   eProtoss_Arbiter_Tribunal = 170,
   eProtoss_Robotics_Support_Bay = 171,
   eProtoss_Shield_Battery = 172,
   eAttack = 300
};

//typedef BWSAL::BuildType MoveType;
//typedef BWAPI::UnitType  MoveType;


template <typename T, unsigned int capacity>
class MemoryPoolClass
{
public:
    MemoryPoolClass();
    void * alloc(size_t n);
    void free(void * ptr);
    ////these go in the deriving class
    //void* operator new(std::size_t size) { }
    //void operator delete(void* ptr) { }
private:
   int myUnusedIndex;
   std::vector<char>  myMemory;
   T* myUnusedMemory[capacity];
};

//template <typename T, class Strategy> class memory_pool
//{
//    private:
//        Strategy s;
//    public:
//        memory_pool()
//        {
//            s.init();
//        }
// 
//        void * alloc(unsigned int n)
//        {
//            if (sizeof(T) != n) throw std::bad_alloc();
//            return s.allocate();
//        }
// 
//        void free(void * p)
//        {
//            s.deallocate(p);
//        }
//};


class QueuedMove
{
    private:
        //typedef memory_pool<QueuedMove, mempool_malloc<QueuedMove> > MemoryPool;
        typedef MemoryPoolClass<QueuedMove, 500> MemoryPool;
        static MemoryPool mem;

public:

   QueuedMove()
      : frameStarted(-1),
        frameComplete(-1),
        prevFrameComplete(-1),
        prevGameTime(-1),
        move(-1)
   {
   }

   QueuedMove(int start, int end, int prevComplete, int prevTime, int aMove)
      : frameStarted(start),
        frameComplete(end),
        prevFrameComplete(prevComplete),
        prevGameTime(prevTime),
        move(aMove)
   {
   }

   // cleanup
   virtual ~QueuedMove()
   {
   }

   //void* operator new(size_t);
   //void operator delete(void*, size_t);

   // operator 'new' and 'delete' overloaded to redirect any memory management, in this case delegated to the memory pool
   void * operator new(size_t n)
   {
      return mem.alloc(n);
   }
   void operator delete(void * p, size_t n)
   {
      mem.free(p);
   } 
   void operator delete(void * p)
   {
      mem.free(p);
   } 

   //MoveType move;
   int move;
   int frameStarted;
   int frameComplete;
   int prevFrameComplete;
   int prevGameTime;
};


class GameState
{
public:
   GameState()
      :   mGameStateFrame(0)
         //,mForceAttacking(false)
         //,mForceMoving(false)
         ,mMinerals(50)                 //default starting mMinerals (enough for 1 worker)
         ,mGas(0)
         ,mCurrentFarm(4*2)             //4 starting probes
         ,mFarmCapacity(9*2)            //default starting farm capacity (9)
         ,mFarmBuilding(0)              //number of supply things that are building (pylon, overlord, depo)
         ,mMineralsPerMinute(4*MineralsPerMinute)  //default starting rate (4 workers * MineralsPerMinute/minute)
         ,mMaxTrainingCapacity(2)       //nexus (1 probe at a time)
         ,mNextFarmDoneIndex(0)
         ,mUnitCounts(12, 0)            //only 12 kinds of units?
         ,mTrainingCompleteFrames(14, std::vector<int>())   // 14 kinds of buildings?
   {
      mTrainingCompleteFrames[0].push_back(0);  //start with 1 nexus
      mUnitCounts[0] = 4;  //4 starting probes
   }
   int   mGameStateFrame;
   //bool  mForceAttacking;   //true = attacking their base, false = defending ours (or sitting still)
   //bool  mForceMoving;      //true = moving to attack
   float mMinerals;
   float mGas;
   int   mCurrentFarm;
   int   mFarmCapacity;
   int   mFarmBuilding;
   float mMineralsPerMinute;  //resource rate
   int   mMaxTrainingCapacity;   //max farm worth of units can be trained at any given time
   int   mNextFarmDoneIndex;
   std::vector<int> mUnitCounts; // 0=probe, 1=zealot, 2=dragoon, 3=templar, 4=dark templar, 5=archon, 6=dark archon, 7=observer, 8=shuttle, 9=reaver, 10=corsair, 11=scout, 12=carrier
                                 // 0=scv, 1=marine, 2=firebat, 3=ghost, 4=vulture, 5=tank, 6=goliath, 7=
							     // TODO: fix-> for now, decrement unit counts for scv when building something
   std::vector< std::vector<int> > mTrainingCompleteFrames;  //0=nexus, 1=pylon, 2=gateways, 3=assimilator, 4=cybercore, 5=forge, 6=robotfacility, 7=stargate, 8=citadel, 9=templar, 10=observatory, 11=arbiter, 12=fleetbeacon,  13=robotsupportbay
                                                             //0=command center, 1=supply depot, 2=barracks, 3=refinery, 4=factory, 5=engineering bay, 6=armory, 7=starport, 8=academy, 9=science, 10=
};



class MacroSearch : public BWAPI::AIModule
{
public:

   MacroSearch();
   ~MacroSearch(){}

   //virtual void onUnitHide(BWAPI::Unit* unit);   //when unit goes invisible
   //virtual void onUnitEvade(BWAPI::Unit* unit);  //inaccessible?
   virtual void onUnitDestroy( BWAPI::Unit* unit );   //unit dies/destroyed
   //virtual void onUnitMorph(BWAPI::Unit* unit);  //unit changes type (drone to extractor, etc)
   virtual void onUnitCreate(BWAPI::Unit* unit);
   virtual void onUnitComplete(BWAPI::Unit* unit);
   //virtual void onUnitDiscover( BWAPI::Unit* unit );  //unit is created (training started)
   //virtual void onUnitShow(BWAPI::Unit* unit);   //when unit becomes visible

   virtual void onFrame();

   //recursive
   std::vector<QueuedMove*> FindMoves(int targetFrame, int maxMilliseconds = 3);
   //std::vector<MoveType> PossibleMoves(int targetFrame); //given the current game state
   //std::vector<MoveType*>* UpdatePossibleMoves();
   std::vector<int>* UpdatePossibleMoves();
   //bool DoMove(MoveType aMove, int targetFrame);
   bool DoMove(int aMove, int targetFrame);
   void UndoMove();

   int SearchLookAhead() { return mSearchLookAhead; }

   int EvaluateState(int targetFrame); //provides a number to represent the "value" of the current game state

private:

   void FindMovesRecursive(int targetFrame);

   void AdvanceQueuedEventsUntil(int targetFrame);
   void ReverseQueuedEventsUntil(int targetFrame);

   int mMaxScore;
   std::vector<QueuedMove*> mBestMoves;

   RACEID mMyRace;

   ////////////////////////////////////////////////////////////////////////////
   ////                    game state variables                            ////
   GameState mMyState;        //real state
   GameState mEnemyState;     //real state
   GameState mSearchState;    //search state (starts off search as copy of mMyState)
   ////////////////////////////////////////////////////////////////////////////


   ////////////////////////////////////////////////////////////////////////////
   ////             search variables, stacks, queues, etc...               ////
   std::vector<QueuedMove*> mMoveStack;          //for undoing moves as tree is climbed back up
   //std::priority_queue<QueuedMove> mEventQueue; //for processing building/training moves as game-tree is searched
   std::list<QueuedMove*> mEventQueue;
   std::list<QueuedMove*>::iterator mLastEventIter;
   int                                 mSearchDepth;
   int                                 mSearchLookAhead;
   //std::vector<std::vector<MoveType> > mPossibleMoves;
   std::vector<std::vector<int> >      mPossibleMoves;
   //std::list	//stable iterators, not ordered
   //std::set	//stable iterators, ordered on insert (value is the key, all must be unique)
   void AddMove(QueuedMove* move);
   ////////////////////////////////////////////////////////////////////////////

   //manage when tech buildings finish (gateway, cybercore, citadel, etc...)
   //std::map<int, int>  mBuildingsComplete;   //Nexus=80, Robotics_Facility=81, Pylon=82, Assimilator=83, Observatory=84, Gateway=85, Photon_Cannon=86, Citadel_of_Adun=87, Cybernetics_Core=88, Templar_Archives=89, Forge=90, Stargate=91, Fleet_Beacon=92, Arbiter_Tribunal=93, Robotics_Support_Bay=94, Shield_Battery=95
};


#endif


#ifndef MACROSEARCH_H
#define MACROSEARCH_H

#include <vector>
#include <deque>
#include <queue>  //for priority_queue
#include <list>
//#include "BWSAL/BuildType.h"
#include <BWAPI/UnitType.h>

const int FramePerMinute = 24*60;   //fastest game speed    //see BWAPI::Game.getFPS()

enum MOVEID
{
   eNull_Move = -1,
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
   eProtoss_Shield_Battery = 172
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

   int frameStarted;
   int frameComplete;
   int prevFrameComplete;
   int prevGameTime;
   //MoveType move;
   int move;
};


class MacroSearch
{
public:

   MacroSearch();
   ~MacroSearch(){}

   //recursive
   std::vector<QueuedMove*> FindMoves(int targetFrame);
   //std::vector<MoveType> PossibleMoves(int targetFrame); //given the current game state
   //std::vector<MoveType*>* UpdatePossibleMoves();
   std::vector<int>* UpdatePossibleMoves();
   //bool DoMove(MoveType aMove, int targetFrame);
   bool DoMove(int aMove, int targetFrame);
   void UndoMove();

   int EvaluateState(); //provides a number to represent the "value" of the current game state

private:

   void FindMovesRecursive(int targetFrame);

   void AdvanceQueuedEventsUntil(int targetFrame);
   void ReverseQueuedEventsUntil(int targetFrame);

   int mMaxScore;
   std::vector<QueuedMove*> mBestMoves;

   ////////// game state variables //////////
   //int CurrentGameFrame;
   int  mGameStateFrame;
   bool mForceAttacking;   //true = attacking their base, false = defending ours (or sitting still)
   bool mForceMoving;      //true = moving to attack
   int  mMinerals;
   int  mGas;
   int  mCurrentFarm;
   int  mFarmCapacity;
   int  mFarmBuilding;
   int  mMineralsPerMinute;  //resource rate
   int  mMaxTrainingCapacity;   //max farm worth of units can be trained at any given time
   std::vector<int> mUnitCounts; // 0=probe, 1=zealot, 2=dragoon, 3=templar, 4=dark templar, 5=archon, 6=dark archon, 7=observer, 8=shuttle, 9=reaver, 10=corsair, 11=scout, 12=carrier

   std::vector< std::vector<int> > mTrainingCompleteFrames;  //0=nexus, 1=pylon, 2=gateways, 3=assimilator, 4=cybercore, 5=forge, 6=robotfacility, 7=stargate, 8=citadel, 9=templar, 10=observatory, 11=arbiter, 12=fleetbeacon,  13=robotsupportbay

   std::vector<QueuedMove*> mMoveStack;          //for undoing moves as tree is climbed back up
   //std::priority_queue<QueuedMove> mEventQueue; //for processing building/training moves as game-tree is searched
   std::list<QueuedMove*> mEventQueue;
   std::list<QueuedMove*>::iterator mLastEventIter;

   int                                 mSearchDepth;
   //std::vector<std::vector<MoveType> > mPossibleMoves;
   std::vector<std::vector<int> >      mPossibleMoves;

   //std::list	//stable iterators, not ordered
   //std::set	//stable iterators, ordered on insert (value is the key, all must be unique)
   void AddMove(QueuedMove* move);

   ////////// end game state variables //////////


   //manage when tech buildings finish (gateway, cybercore, citadel, etc...)
   //std::map<int, int>  mBuildingsComplete;   //Nexus=80, Robotics_Facility=81, Pylon=82, Assimilator=83, Observatory=84, Gateway=85, Photon_Cannon=86, Citadel_of_Adun=87, Cybernetics_Core=88, Templar_Archives=89, Forge=90, Stargate=91, Fleet_Beacon=92, Arbiter_Tribunal=93, Robotics_Support_Bay=94, Shield_Battery=95
};


#endif


#include "MacroSearch.h"
//#include "BWSAL.h"
#include <algorithm>

#define cATTACK_ID 300

  #ifndef max
    #define max std::max
  #endif
  #ifndef min
    #define min std::min
  #endif


namespace
{
   int sDistanceToEnemy = 128*8;   //WAG (unit: pixels)
}

bool operator< (const QueuedMove& a, const QueuedMove& b)
{
   return (a.frameComplete > b.frameComplete);
}
bool operator== (const QueuedMove& a, const QueuedMove& b)
{
   return (a.move.getID() == b.move.getID() &&
           a.frameComplete == b.frameComplete); //should be enough to check this, if type same & complete-time the same, then start time the same.  which bldg shouldn't matter
}


MacroSearch::MacroSearch()
   :
   //,CurrentGameTime(0.0)
   //,GameStateTime(0.0)
   //,CurrentGameFrame(0)
    mGameStateFrame(0)
   ,mForceAttacking(false)
   ,mMinerals(50)                 //default starting mMinerals (enough for 1 worker)
   ,mGas(0)
   ,mCurrentFarm(4*2)             //4 starting probes
   ,mFarmCapacity(9*2)            //default starting farm capacity (9)
   ,mMineralsPerMinute(4*42)      //default starting rate (4 workers * 42/minute)
   ,mMaxTrainingCapacity(2)       //nexus (1 probe at a time)
   ,mUnitCounts(12, 0)            //only 12 kinds of units?
   ,mTrainingCompleteFrames(14, std::vector<int>())   // 14 kinds of buildings?
   ,mMoveStack()
{
   mTrainingCompleteFrames[0].push_back(0);  //start with 1 nexus
   mUnitCounts[0] = 4;  //4 starting probes
}

std::vector<QueuedMove> MacroSearch::FindMoves(int targetFrame)
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
   FindMovesRecursive(targetFrame);
   return mBestMoves;
}


void MacroSearch::FindMovesRecursive(int targetFrame)
{
   std::vector<MoveType> moves = PossibleMoves(targetFrame);
   std::vector<MoveType>::iterator it;
   for (it=moves.begin(); it!=moves.end(); it++)
   {
      MoveType move = *it;
      int tempFrame = mGameStateFrame;
      DoMove(move);
      if (mGameStateFrame < targetFrame)
      {
         FindMovesRecursive(targetFrame);
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
         UndoMove(tempFrame); //undo last move on the stack
         mGameStateFrame = tempFrame;
         return;  //none of the other events in "moves" array will score differently here
      }
      UndoMove(tempFrame); //undo last move on the stack
      mGameStateFrame = tempFrame;
   }
   //return mMoveStack;
   //return mBestMoves;
   return;
}

//given the current game state
std::vector<MoveType> MacroSearch::PossibleMoves(int targetFrame)
{
   std::vector<MoveType> moves;
   //TODO: count mineral spots, for now... use 8
   if (mUnitCounts[0] < 16 && //two per mineral spot
       mCurrentFarm < mFarmCapacity &&   //and have pylons
       mTrainingCompleteFrames[0].size() >0) //and have a nexus to build it in
   {
      moves.push_back(BWAPI::UnitTypes::Protoss_Probe);
   }

   if (mCurrentFarm < mFarmCapacity &&  //have pylons
       mTrainingCompleteFrames[2].size() >0) //and have a gateway to build it in
   {
      moves.push_back(BWAPI::UnitTypes::Protoss_Zealot);
   }

   if ((mTrainingCompleteFrames[1].size()==0) ||
       ((mFarmCapacity-mCurrentFarm) <= (2*mMaxTrainingCapacity)) )
   {
      moves.push_back(BWAPI::UnitTypes::Protoss_Pylon);
   }

   if (mTrainingCompleteFrames[1].size()>0)  //need pylon
   {
      moves.push_back(BWAPI::UnitTypes::Protoss_Gateway);
   }

   if (mUnitCounts[1] > 1)
   {
      moves.push_back( BWAPI::UnitType(cATTACK_ID) );  //cATTACK_ID to represent ATTACK
   }

   return moves;
}

void MacroSearch::DoMove(MoveType aMove)
{
   //int price          = aMove.mineralPrice();
   //int buildTime      = aMove.buildTime();   //frames
   //int supplyRequired = aMove.supplyRequired();

   switch (aMove.getID())
   {
   case 64: //probe //TODO: verify this ID //50, 300, 2
      {
         int price          = 50;
         int buildTime      = 300;   //frames
         int supplyRequired = 2;

         int buildingIndex = 0;  //nexus
         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mGameStateFrame;
         if (mMinerals < price) {
            frameMineralAvailable += ((price-mMinerals)*FramePerMinute/mMineralsPerMinute);
         }
         //find out at what time a production building is available
         //TODO: check all buildings of correct type, not just last one
         int frameBuildingAvailable = mTrainingCompleteFrames[buildingIndex][0];//.back();   //TODO find right nexus
         if (frameBuildingAvailable < mGameStateFrame) {
            frameBuildingAvailable = mGameStateFrame;
         }
         //0. find build time
         //start time is latest of these two times
         int moveStartTime = max(frameMineralAvailable, frameBuildingAvailable);

         //advance game state to this time & adjust values
         int dt = moveStartTime - mGameStateFrame;
         //1. adjust mMinerals
         mMinerals -= price;
         mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust mGas too

         //2. adjust supply
         mCurrentFarm += supplyRequired;   //supply taken when unit training started

         QueuedMove newMove(moveStartTime, moveStartTime+buildTime, mTrainingCompleteFrames[buildingIndex][0], aMove);  //TODO - find right nexus
         //3. adjust training times
         mTrainingCompleteFrames[buildingIndex][0] = (moveStartTime+buildTime);//TODO - find right nexus
         //4. add event to queue
         mMoveStack.push_back(newMove);
         mEventQueue.push(newMove);
         //mUnitCounts[0]   ++; //probe   //TODO, increment this when unit done building
         //mMineralsPerMinute += 42;       //TODO, increment this when unit done building
         //5. adjust time
         mGameStateFrame = moveStartTime;
      }break;

   case 65: //zealot //TODO: verify this ID  //100, 600, 4
      {
         int price          = 100;
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
         //TODO: check all buildings of correct type, not just last one
         int frameBuildingAvailable = mTrainingCompleteFrames[buildingIndex][0]; //TODO - find right gateway
         if (frameBuildingAvailable < mGameStateFrame) {
            frameBuildingAvailable = mGameStateFrame;
         }
         //start time is latest of these two times
         int moveStartTime = max(frameMineralAvailable, frameBuildingAvailable);

         //advance game state to this time & adjust values
         int dt = moveStartTime - mGameStateFrame;
         mMinerals -= price;
         mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust mGas too
         mCurrentFarm += supplyRequired;   //supply taken when unit building started

         //1. proper use of mTrainingCompleteFrames[buildingIndex]
         //2. proper creationg of QueuedMove structs
         //3. undo moves
         //4. use proper building (search gateways)

         QueuedMove newMove(moveStartTime, moveStartTime+buildTime, mTrainingCompleteFrames[buildingIndex][0], aMove);  //TODO - find right gateway
         mTrainingCompleteFrames[buildingIndex][0] = (moveStartTime+buildTime);  //TODO - find right gateway
         mMoveStack.push_back(newMove);
         mEventQueue.push(newMove);

         //mUnitCounts[1]   ++; //zealot  //TODO, increment this when unit done building
         mGameStateFrame = moveStartTime;
      }break;

   case 156: //pylon  //TODO: verify this ID //100, 450, 0
      {
         int price          = 100;
         int buildTime      = 450;   //frames

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mGameStateFrame;
         if (mMinerals < price) {
            frameMineralAvailable += ((price-mMinerals)*FramePerMinute/mMineralsPerMinute);
         }
         //start time is mineral time
         int moveStartTime = frameMineralAvailable;

         //advance game state to this time & adjust values
         int dt = moveStartTime - mGameStateFrame;
         mMinerals -= price;
         mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust mGas too
         mGameStateFrame = moveStartTime;
         QueuedMove newMove(moveStartTime, moveStartTime+buildTime, 0, aMove);
         mTrainingCompleteFrames[1].push_back(moveStartTime+buildTime);
         mEventQueue.push(newMove);
         mMoveStack.push_back(newMove);
         //mFarmCapacity += 16; //TODO, increment this when unit done building
      }break;

   case 160: //gateway   //TODO: verify this ID //150, 900, 0
      {
         int price          = 150;
         int buildTime      = 900;   //frames

         //find out at what time mMinerals are available
         //TODO: search for mGas time too
         int frameMineralAvailable = mGameStateFrame;
         if (mMinerals < price) {
            frameMineralAvailable += ((price-mMinerals)*FramePerMinute/mMineralsPerMinute);
         }
         //find out at what time a pylon is available
         int framePreReqAvailable = mTrainingCompleteFrames[1].front();

         //start time is max of these two times
         int moveStartTime = max(frameMineralAvailable, framePreReqAvailable);

         //advance game state to this time & adjust values
         int dt = moveStartTime - mGameStateFrame;
         mMinerals -= price;
         mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust mGas too
         mGameStateFrame = moveStartTime;
         QueuedMove newMove(moveStartTime, moveStartTime+buildTime, 0, aMove);
         mTrainingCompleteFrames[2].push_back(moveStartTime+buildTime);
         mEventQueue.push(newMove);
         mMoveStack.push_back(newMove);

         //mMaxTrainingCapacity += 2;  //for a zealot, what is a goon? 2?   //TODO, increment this when unit done building
      }break;

   case cATTACK_ID:
      {
         //mForceAttacking = true; //TODO set this when force arrives (travel time over)

         //sDistanceToEnemy;  //distance in pixels
         double speed = BWAPI::UnitTypes::Protoss_Zealot.topSpeed(); //pixels per frame
         double dTime = ((double)sDistanceToEnemy) / speed;
         int time = (int)(dTime+0.5);  //frames
         QueuedMove newMove(mGameStateFrame, mGameStateFrame+time, 0, aMove);
         mEventQueue.push(newMove);
         mMoveStack.push_back(newMove);
      }break;

   default:
      {
         //unknown move, shouldn't be here
      }break;
   };

   ProcessQueuedEventsUntil(mGameStateFrame);
}

void MacroSearch::ProcessQueuedEventsUntil(int targetFrame)
{
   //finish any units or buildings that should be completed by the targetFrame
   while (mEventQueue.size()>0 &&
          mEventQueue.top().frameComplete <= targetFrame)
   {
      int time = mEventQueue.top().frameComplete;
      QueuedMove qMove = mEventQueue.top();
      switch (qMove.move.getID())
      {
      case 64: //probe //TODO: verify this ID
         {
            mUnitCounts[0]++; //probe
            mMineralsPerMinute += 42;
         }break;
      case 65: //zealot //TODO: verify this ID
         {
            mUnitCounts[1]++; //zealot
         }break;
      case 156: //pylon  //TODO: verify this ID
         {
            mFarmCapacity += 16;
         }break;
      case 160: //gateway   //TODO: verify this ID
         {
            //mTrainingCompleteFrames[2].push_back(time);  //add new gateway (available to use at 'time')   //already done
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
      mEventQueue.pop();
   }
}



void MacroSearch::UndoMove(int previousFrame)
{
   //MoveType move = mMoveStack.back();
   //mMoveStack.pop_back();
   //if (move != aMove)
   //{
   //   //these should match, unknown error
   //}
   if (mMoveStack.empty())
   {
      return;
   }

   QueuedMove undoMove = mMoveStack.back();
   mMoveStack.pop_back();

   //check if this event we just popped off the stack will be in the time-sorted event queue
   undoMove.frameComplete;
   undoMove.prevFrameComplete;

   if (mGameStateFrame >= undoMove.frameStarted &&  //should this line always be true? do we have to check here?
       mGameStateFrame < undoMove.frameComplete)
   {
      std::deque<QueuedMove> tempStack;
      while (!(mEventQueue.top() == undoMove))
      {
         tempStack.push_back(mEventQueue.top());
         mEventQueue.pop();
      }
      mEventQueue.pop();
      while (tempStack.size() > 0)
      {
         mEventQueue.push(tempStack.back());
         tempStack.pop_back();
      }
   }

   //int price          = undoMove.move.mineralPrice();
   //int buildTime      = undoMove.move.buildTime();   //frames
   //int supplyRequired = undoMove.move.supplyRequired();

   //0. find build time
   //1. adjust mMinerals
   //2. adjust supply
   //3. adjust training times
   //4. add event to queue
   //5. adjust time

   switch (undoMove.move.getID())
   {
   case 64: //probe //TODO: verify this ID
      {
         int price          = 50;
         int buildTime      = 300;   //frames
         int supplyRequired = 2;

         int buildingIndex = 0;  //nexus
         //check for probe mining duration -> mineral rate changes because of probe
         if (undoMove.frameComplete <= mGameStateFrame)
         {
            //adjust time back to end of move
            int dt = undoMove.frameComplete - mGameStateFrame; //should be negative
            mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
            mGameStateFrame = undoMove.frameComplete;
            mMineralsPerMinute -= 42;  //this is safe now, we've backed up time to when probe finished
                                       //otherwise dont do this, the mining rate wasn't updated yet
         }
         int moveStartTime = undoMove.frameComplete - buildTime;
         int dt = moveStartTime - mGameStateFrame;  //should be negative
         //reverse game state to the beginning time of the move & adjust values
         mMinerals += price;
         mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust mGas too
         mCurrentFarm -= supplyRequired;
         mTrainingCompleteFrames[buildingIndex][0] = undoMove.prevFrameComplete; //TODO - find right nexus
         mUnitCounts[0]--; //probe
         mGameStateFrame = moveStartTime;
      }break;

   case 65: //zealot //TODO: verify this ID
      {
         int price          = 100;
         int buildTime      = 600;   //frames
         int supplyRequired = 4;

         int buildingIndex = 2;  //gateway
         int moveStartTime = undoMove.frameComplete - buildTime;

         //advance game state to this time & adjust values
         int dt = moveStartTime - mGameStateFrame; //should be negative
         mMinerals += price;
         mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust mGas too
         mCurrentFarm -= supplyRequired;
         mTrainingCompleteFrames[buildingIndex][0] = undoMove.prevFrameComplete; //TODO - find right gateway
         mUnitCounts[1]--; //zealot  //TODO, increment this when unit done building
         mGameStateFrame = moveStartTime;
      }break;

   case 156: //pylon  //TODO: verify this ID
      {
         int price          = 100;
         int buildTime      = 450;   //frames

         int moveStartTime = undoMove.frameComplete - buildTime;
         //advance game state to this time & adjust values
         int dt = moveStartTime - mGameStateFrame; //should be negative
         mMinerals += price;
         mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust mGas too
         mGameStateFrame = moveStartTime;
         mTrainingCompleteFrames[1].pop_back(); //should remove last added pylon

         //QueuedMove newMove(moveStartTime, moveStartTime+buildTime, 0, aMove);
         //mEventQueue.push(newMove);
         //mMoveStack.push_back(newMove);

         mFarmCapacity -= 16;
      }break;

   case 160: //gateway   //TODO: verify this ID
      {
         int price          = 150;
         int buildTime      = 900;   //frames

         int moveStartTime = undoMove.frameComplete - buildTime;
         //advance game state to this time & adjust values
         int dt = moveStartTime - mGameStateFrame; //should be negative
         mMinerals += price;
         mMinerals += ((mMineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust mGas too
         mGameStateFrame = moveStartTime;
         mTrainingCompleteFrames[2].pop_back(); //should remove last added gateway
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

   //////also check the event queue and take off any events that start after the new GameStateTime
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

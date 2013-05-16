
#include "MacroSearch.h"
#include "BWSAL.h"
#include <algorithm>

MacroSearch::MacroSearch()
   :
    cAttackId(300)
   //,CurrentGameTime(0.0)
   //,GameStateTime(0.0)
   //,CurrentGameFrame(0)
   ,GameStateFrame(0)
   ,ForceAttacking(false)
   ,Minerals(50)                 //default starting minerals (enough for 1 worker)
   ,Gas(0)
   ,CurrentFarm(4)               //4 starting probes
   ,FarmCapacity(9)              //default starting farm capacity?
   ,MineralsPerMinute(4*42)      //default starting rate (4 workers * 42/minute)
   ,MaxTrainingCapacity(1)       //nexus (1 probe at a time)
   ,mUnitCounts(12)              //only 12 kinds of units?
   ,mTrainingCompleteFrames(4, std::vector<int>()) //only  4 kinds of buildings?
   ,mMoveStack()
{
   mUnitCounts[0] = 4;  //4 starting probes
}

std::vector<MoveType> MacroSearch::FindMoves(int currentFrame, int targetFrame)
{
   std::vector<MoveType> moves = PossibleMoves();
   std::vector<MoveType>::iterator it;
   for (it=moves.begin(); it!=moves.end(); it++)
   {
      MoveType move = *it;
      int tempFrame = GameStateFrame;
      DoMove(move);
      if (GameStateFrame < targetFrame)
      {
         FindMoves(GameStateFrame, targetFrame);
      }
      else
      {
         //save off this current game state as a leaf node of the DFS game tree search
      }
      UndoMove(tempFrame); //undo last move on the stack
      GameStateFrame = tempFrame;
   }
   //return mMoveStack;
   return moves;   //this aint right either
}

//given the current game state
std::vector<MoveType> MacroSearch::PossibleMoves()
{
   std::vector<MoveType> moves;
   //TODO: count mineral spots, for now... use 8
   if (mUnitCounts[0] < 16 && //two per mineral spot
       CurrentFarm < FarmCapacity &&   //and have pylons
       mTrainingCompleteFrames[0].size() >0) //and have a nexus to build it in
   {
      moves.push_back(BWAPI::UnitTypes::Protoss_Probe);
   }

   if (CurrentFarm < FarmCapacity)  //have pylons
   {
      moves.push_back(BWAPI::UnitTypes::Protoss_Zealot);
   }

   if ((2*(FarmCapacity-CurrentFarm)) <= MaxTrainingCapacity)
   {
      moves.push_back(BWAPI::UnitTypes::Protoss_Pylon);
   }

   if (FarmCapacity > 9)
   {
      moves.push_back(BWAPI::UnitTypes::Protoss_Gateway);
   }
   return moves;
}

void MacroSearch::DoMove(MoveType aMove)
{
   int price          = aMove.mineralPrice();
   int buildTime      = aMove.buildTime();   //frames
   int supplyRequired = aMove.supplyRequired();

   switch (aMove.getID())
   {
   case 32: //probe //TODO: verify this ID
      {
         //0=nexus, 1=gateways, 2=robotics facility, 3=stargate
         int buildingIndex = 0;
         //find out at what time minerals are available
         //TODO: search for gas time too
         int frameMineralAvailable = GameStateFrame;
         if (Minerals < price) {
            frameMineralAvailable += ((price-Minerals)*FramePerMinute/MineralsPerMinute);
         }
         //find out at what time a production building is available
         //TODO: check all buildings of correct type, not just last one
         int frameBuildingAvailable = mTrainingCompleteFrames[buildingIndex][0];//.back();   //TODO find right nexus
         if (frameBuildingAvailable < GameStateFrame) {
            frameBuildingAvailable = GameStateFrame;
         }
         //start time is latest of these two times
         int moveStartTime = max(frameMineralAvailable, frameBuildingAvailable);

         //advance game state to this time & adjust values
         int dt = moveStartTime - GameStateFrame;
         Minerals -= price;
         Minerals += ((MineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust gas too
         CurrentFarm += supplyRequired;   //supply taken when unit training started

         QueuedMove newMove(moveStartTime, moveStartTime+buildTime, mTrainingCompleteFrames[buildingIndex][0], aMove);  //TODO find right nexus
         mTrainingCompleteFrames[buildingIndex][0] = (moveStartTime+buildTime);//.push_back(moveStartTime+buildTime);
         mMoveStack.push_back(newMove);
         mEventQueue.push(newMove);
         //mUnitCounts[0]   ++; //probe   //TODO, increment this when unit done building
         //MineralsPerMinute += 42;       //TODO, increment this when unit done building
         GameStateFrame = moveStartTime;
      }break;

   case 33: //zealot //TODO: verify this ID
      {
         //0=nexus, 1=gateways, 2=robotics facility, 3=stargate
         int buildingIndex = 1;
         //find out at what time minerals are available
         //TODO: search for gas time too
         int frameMineralAvailable = GameStateFrame;
         if (Minerals < price) {
            frameMineralAvailable += ((price-Minerals)*FramePerMinute/MineralsPerMinute);
         }
         //find out at what time a production building is available
         //TODO: check all buildings of correct type, not just last one
         int frameBuildingAvailable = mTrainingCompleteFrames[buildingIndex][0]; //.back();  //TODO find right gateway
         if (frameBuildingAvailable < GameStateFrame) {
            frameBuildingAvailable = GameStateFrame;
         }
         //start time is latest of these two times
         int moveStartTime = max(frameMineralAvailable, frameBuildingAvailable);

         //advance game state to this time & adjust values
         int dt = moveStartTime - GameStateFrame;
         Minerals -= price;
         Minerals += ((MineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust gas too
         CurrentFarm += supplyRequired;   //supply taken when unit building started

         //1. proper use of mTrainingCompleteFrames[buildingIndex]
         //2. proper creationg of QueuedMove structs
         //3. undo moves
         //4. use proper building (search gateways)

         QueuedMove newMove(moveStartTime, moveStartTime+buildTime, mTrainingCompleteFrames[buildingIndex][0], aMove);  //TODO find right gateway
         mTrainingCompleteFrames[buildingIndex][0] = (moveStartTime+buildTime);//.push_back(moveStartTime+buildTime);
         mMoveStack.push_back(newMove);
         mEventQueue.push(newMove);

         //mUnitCounts[1]   ++; //zealot  //TODO, increment this when unit done building
         GameStateFrame = moveStartTime;
      }break;

   case 82: //pylon  //TODO: verify this ID
      {
         //find out at what time minerals are available
         //TODO: search for gas time too
         int frameMineralAvailable = GameStateFrame;
         if (Minerals < price) {
            frameMineralAvailable += ((price-Minerals)*FramePerMinute/MineralsPerMinute);
         }
         //start time is mineral time
         int moveStartTime = frameMineralAvailable;

         //advance game state to this time & adjust values
         int dt = moveStartTime - GameStateFrame;
         Minerals -= price;
         Minerals += ((MineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust gas too
         GameStateFrame = moveStartTime;
         mEventQueue.push(QueuedMove(moveStartTime+buildTime,aMove));
         //FarmCapacity += move.supplyProvided(); //TODO, increment this when unit done building
      }break;

   case 85: //gateway   //TODO: verify this ID
      {
         //find out at what time minerals are available
         //TODO: search for gas time too
         int frameMineralAvailable = GameStateFrame;
         if (Minerals < price) {
            frameMineralAvailable += ((price-Minerals)*FramePerMinute/MineralsPerMinute);
         }
         //start time is mineral time
         int moveStartTime = frameMineralAvailable;

         //advance game state to this time & adjust values
         int dt = moveStartTime - GameStateFrame;
         Minerals -= price;
         Minerals += ((MineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust gas too
         GameStateFrame = moveStartTime;
         mEventQueue.push(QueuedMove(moveStartTime+buildTime,aMove));

         //MaxTrainingCapacity += 1;  //for a zealot, what is a goon? 2?   //TODO, increment this when unit done building
      }break;
   default:
      {
         //unknown move, shouldn't be here
      }break;
   };

   //finish any units or buildings that have completed by the new GameStateFrame
   while (mEventQueue.size()>0 &&
          mEventQueue.top().frameComplete <= GameStateFrame)
   {
      int time = mEventQueue.top().frameComplete;
      MoveType move = mEventQueue.top().move;
      switch (move.getID())
      {
      case 32: //probe //TODO: verify this ID
         {
            mUnitCounts[0]++; //probe
            MineralsPerMinute += 42;
         }break;
      case 33: //zealot //TODO: verify this ID
         {
            mUnitCounts[1]++; //zealot
         }break;
      case 82: //pylon  //TODO: verify this ID
         {
            FarmCapacity += move.supplyProvided(); //should be 8 ?
         }break;
      case 85: //gateway   //TODO: verify this ID
         {
            mTrainingCompleteFrames[1].push_back(time);  //add new gateway (available to use at 'time')
            MaxTrainingCapacity += 1;  //zealot? //TODO: find largest unit buildable by this building
         }break;
      default:
         {
            //unknown move, shouldn't be here
         }break;
      };
      mEventQueue.pop();
   }
}

void MacroSearch::UndoMove(previousFrame)
{
   //MoveType move = mMoveStack.back();
   //mMoveStack.pop_back();
   //if (move != aMove)
   //{
   //   //these should match, unknown error
   //}

   QueuedMove undoMove = mMoveStack.back();
   mMoveStack.pop_back();

   //check if this event we just popped off the stack will be in the time-sorted event queue
   undoMove.frameComplete;
   undoMove.prevFrameComplete;

   if (GameStateFrame >= undoMove.frameStarted &&  //should this line always be true? do we have to check here?
       GameStateFrame < undoMove.frameComplete)
   {
      std::deque<QueuedMove> tempStack;
      while (mEventQueue.top() != undoMove)
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


   int price          = undoMove.move.mineralPrice();
   int buildTime      = undoMove.move.buildTime();   //frames
   int supplyRequired = undoMove.move.supplyRequired();


   switch (undoMove.move.getID())
   {
   case 32: //probe //TODO: verify this ID
      {
         //undoMove.
         //int dt = GameStateFrame - previousFrame;
         //GameStateFrame = previousFrame;
         //mTrainingCompleteFrames[0].pop_back();
         //CurrentFarm      -= BWAPI::UnitTypes::Protoss_Probe.supplyRequired();
         //Minerals         -= ((MineralsPerMinute * dt)/30);  //30 Hz update rate?
         //Minerals         += BWAPI::UnitTypes::Protoss_Probe.mineralPrice();
         //mUnitCounts[0]   --; //probe
         //MineralsPerMinute -= 42;

         //0=nexus, 1=gateways, 2=robotics facility, 3=stargate
         int buildingIndex = 0;

         //start time is latest of these two times
         int moveStartTime = undoMove.frameComplete - buildTime;
         int dt = moveStartTime - GameStateFrame;  //should be negative

         //reverse game state to the beginning time of the move & adjust values
         Minerals += price;
         Minerals -= ((MineralsPerMinute * dt)/FramePerMinute);
         MineralsPerMinute -= 42;
         //TODO: adjust gas too
         CurrentFarm -= supplyRequired;
         mTrainingCompleteFrames[buildingIndex].[0] = undoMove.prevFrameComplete;
         mUnitCounts[0]--; //probe

         GameStateFrame = moveStartTime;
      }break;

   case 33: //zealot //TODO: verify this ID
      {
         //0=nexus, 1=gateways, 2=robotics facility, 3=stargate
         int buildingIndex = 1;
         int moveStartTime = 0;//?????  // max(frameMineralAvailable, frameBuildingAvailable);

         //advance game state to this time & adjust values
         int dt = moveStartTime - GameStateFrame;
         Minerals -= price;
         Minerals += ((MineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust gas too
         CurrentFarm += supplyRequired;   //supply taken when unit building started
         mTrainingCompleteFrames[buildingIndex].push_back(moveStartTime+buildTime);
         GameStateFrame = moveStartTime;
         //mEventQueue.push(QueuedMove(moveStartTime+buildTime,aMove));
         //mUnitCounts[1]   ++; //zealot  //TODO, increment this when unit done building
      }break;

   case 82: //pylon  //TODO: verify this ID
      {
         //find out at what time minerals are available
         //TODO: search for gas time too
         int frameMineralAvailable = GameStateFrame;
         if (Minerals < price) {
            frameMineralAvailable += ((price-Minerals)*FramePerMinute/MineralsPerMinute);
         }
         //start time is mineral time
         int moveStartTime = frameMineralAvailable;

         //advance game state to this time & adjust values
         int dt = moveStartTime - GameStateFrame;
         Minerals -= price;
         Minerals += ((MineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust gas too
         GameStateFrame = moveStartTime;
         //mEventQueue.push(QueuedMove(moveStartTime+buildTime,aMove));
         //FarmCapacity += move.supplyProvided(); //TODO, increment this when unit done building
      }break;

   case 85: //gateway   //TODO: verify this ID
      {
         //find out at what time minerals are available
         //TODO: search for gas time too
         int frameMineralAvailable = GameStateFrame;
         if (Minerals < price) {
            frameMineralAvailable += ((price-Minerals)*FramePerMinute/MineralsPerMinute);
         }
         //start time is mineral time
         int moveStartTime = frameMineralAvailable;

         //advance game state to this time & adjust values
         int dt = moveStartTime - GameStateFrame;
         Minerals -= price;
         Minerals += ((MineralsPerMinute * dt)/FramePerMinute);
         //TODO: adjust gas too
         GameStateFrame = moveStartTime;
         //mEventQueue.push(QueuedMove(moveStartTime+buildTime,aMove));

         //mTrainingCompleteFrames[1].push_back(time);  //TODO, increment this when unit done building
         //MaxTrainingCapacity += 1;  //zealot?          //TODO, increment this when unit done building
      }break;
   default:
      {
         //unknown move, shouldn't be here
      }break;
   };

   //////also check the event queue and take off any events that start after the new GameStateTime
}


int MacroSearch::EvaluateState()
{
   int score = Minerals;
   score += mUnitCounts[0] * 50;
   score += mUnitCounts[0] * 200;   //doubly favor zealots
   score += FarmCapacity * 10;
   score += mTrainingCompleteFrames[1].size() * 200;  //slightly favor gateways
   return score;
}

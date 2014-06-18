
#include "BWAPI.h"
#include "PerceivedUnit.h"
#include "BWAPI/UnitCommandType.h"

#include "Common.h"

PerceivedUnit::PerceivedUnit(BWAPI::Unit* aBaseUnit)
   : Unit()
   , mCommands()
   , mBaseUnit(aBaseUnit)
   , mCurrentPerceivedFrame(BWAPI::Broodwar->getFrameCount())
{
   //guarantee an entry in the history
   mStateHistory.push_front(UnitState(mBaseUnit, mCurrentPerceivedFrame));

   //give it FRAME_LAG # fake past commands
   BWAPI::UnitCommand fake = BWAPI::UnitCommand::move(mBaseUnit, mBaseUnit->getPosition());
   for (int prevFrame=mCurrentPerceivedFrame-1;prevFrame>=(mCurrentPerceivedFrame-FRAME_LAG); prevFrame--)
   {
      mCommands.push_back(std::pair<int, BWAPI::UnitCommand>(prevFrame, fake));
   }
}


void PerceivedUnit::Update(int currentGameFrame)
{
   UnitState prevState = mStateHistory.front();
   UnitState curState(mBaseUnit, currentGameFrame);
   curState.SetActive(prevState);
   mStateHistory.push_front(curState);
   //always just keep FRAME_LAG # states in the history queue
   while(mStateHistory.size() > FRAME_LAG)
   {
      mStateHistory.pop_back();
   }
   mStateFuture.clear();

   //extrapolate unit forward in time FRAME_LAG # frames from current game position

   ////check if a user command was sent in within the last LAG frames
   ////if so, use it
   //int lastCommandFrame = mBaseUnit->getLastCommandFrame();
   //BWAPI::UnitCommand lastCommand = mBaseUnit->getLastCommand();
   //std::deque<std::pair<int, BWAPI::UnitCommand>>::iterator it;
   //for (it = mCommands.begin(); it != mCommands.end(); it++)
   //{
   //   if (it->first >= lastCommandFrame)
   //   {
   //      it->second = lastCommand;
   //   }
   //}

   //new commands pushed in the front, old commands popped off the back
   //commands should never be empty (fake ones put in on unit creation)
   if (mCommands.front().first < (currentGameFrame-1))
   {
      //no command received last frame
      if (curState.active)
      {
         //duplicate the previous command if we are still acting it out
         mCommands.push_front(std::pair<int, BWAPI::UnitCommand>(currentGameFrame-1, mCommands.front().second));
      }
      else
      {
         //put in a fake place holder command at our current position
         BWAPI::UnitCommand fake = BWAPI::UnitCommand::move(mBaseUnit, curState.pos);
         mCommands.push_front(std::pair<int, BWAPI::UnitCommand>(currentGameFrame-1, fake));
      }
   }
   //always just keep FRAME_LAG # commands in the command queue
   while(mCommands.size() > FRAME_LAG)
   {
      mCommands.pop_back();
   }

   //int curCmdFrame = currentGameFrame - FRAME_LAG;
   //iterate over the 4- commands & project our state forward in time
   for (ReverseCommandIterator rit = mCommands.rbegin(); rit != mCommands.rend(); rit++)
   {
      //do sanity checks with command & future frames?
      UnitState nextState(curState);
      nextState.frame += 1;

      int cmdFrame = rit->first;
      BWAPI::UnitCommand command = rit->second;

      BWAPI::Position p2;
      if (command.getType() == BWAPI::UnitCommandTypes::Attack_Unit) {
         p2 = command.getTargetPosition();
         p2 = command.getTarget()->getPosition();
      } else {
         p2 = command.getTargetPosition();
      }

      BWAPI::Position p1 = curState.pos;
      //if we are already in position, ignore the command & decellerate if necessary
      if (p1 == p2)
      {
         nextState.velX = 0;
         nextState.velY = 0;
         nextState.posX = (double)p2.x(); //just in case the double precision position was off a bit
         nextState.posY = (double)p2.y(); //just in case the double precision position was off a bit
         nextState.SetActive(curState);
      }
      else
      {
         double speed = mBaseUnit->getType().topSpeed(); //pixels/frame

         //invert Y for atan2()
         double cmdAngleRadians = atan2(double(p2.y()-p1.y()),double(p2.x()-p1.x()));
         //bwapi gives us turn radius in degrees, I think, doh!
         double turnRadiusRadians = (M_PI/180.0) * (double)mBaseUnit->getType().turnRadius();
         //compute difference angle & bound to within [-pi,pi]
         double angleDiffRadians = (cmdAngleRadians - curState.angle);
         while (angleDiffRadians >  M_PI) { angleDiffRadians -= (2*M_PI); }
         while (angleDiffRadians < -M_PI) { angleDiffRadians += (2*M_PI); }

         if (abs(angleDiffRadians) > turnRadiusRadians)
         {
            //this frame is only turning, no moving
            double turnRateRadians = (M_PI/180.0) * GetTurnRate(this); //degrees/frame
            if (abs(angleDiffRadians)>turnRateRadians)
            {
               //will be turning the full turnRate this frame
               nextState.angle += (angleDiffRadians>0)?(turnRateRadians):(-turnRateRadians);
            }
            else
            {
               //will be turning only to target angle (diff > turn radius, but diff < turn rate)
               nextState.angle = cmdAngleRadians;
            }
            while (nextState.angle > (2*M_PI)) { nextState.angle -= (2*M_PI); }
            while (nextState.angle < 0) { nextState.angle += (2*M_PI); }
            nextState.velX = speed * cos(nextState.angle);
            nextState.velY = speed * sin(nextState.angle);
         }
         else if (curState.HasVelocity())
         {
            //can move towards target position (p2) this frame
            //double speed = mBaseUnit->getType().topSpeed(); //pixels/frame
            nextState.angle = cmdAngleRadians;
            while (nextState.angle > (2*M_PI)) { nextState.angle -= (2*M_PI); }
            while (nextState.angle < 0) { nextState.angle += (2*M_PI); }
            nextState.velX = speed * cos(nextState.angle);
            nextState.velY = speed * sin(nextState.angle);

            double dX = (double)p2.x() - curState.posX;
            double dY = (double)p2.y() - curState.posY;
            double cmdDistSquared = dX*dX + dY*dY;
            //double cmdDist = sqrt(cmdDistSquared);
            if (cmdDistSquared > (speed*speed))
            {
               //move at "full speed" this frame
               nextState.posX += nextState.velX;
               nextState.posY += nextState.velY;
               nextState.pos.x() = (int)nextState.posX;  //yes, actually truncate it - no rounding
               nextState.pos.y() = (int)nextState.posY;  //yes, actually truncate it - no rounding
            }
            else
            {
               //arrive at destination this frame
               nextState.posX = (double)p2.x(); //just in case the double precision position was off a bit
               nextState.posY = (double)p2.y(); //just in case the double precision position was off a bit
               nextState.pos = p2;
            }
         }
         else
         {
            //no moving, just turning &/or accelerating
         }
      }
      mStateFuture.push_back(nextState);
      curState = nextState;
   }

   //debugging?
   if(DEBUG_DRAW_PERCEPTION)
   {
      //draw a line from current pos to future position
      BWAPI::Broodwar->drawLineMap(mBaseUnit->getPosition().x(),
                                   mBaseUnit->getPosition().y(), 
                                   mStateFuture.back().pos.x(),
                                   mStateFuture.back().pos.y(),
                                   BWAPI::Colors::Orange);
   }
}


//static 
double PerceivedUnit::GetTurnRate(BWAPI::Unit* aUnit) //degrees per frame
{
   if (aUnit->getType() == BWAPI::UnitTypes::Terran_Marine ||
       aUnit->getType() == BWAPI::UnitTypes::Terran_Medic  ||
       aUnit->getType() == BWAPI::UnitTypes::Terran_Firebat)
   {
      return 56.25;   //   0.9817 radians
   }
   else
   {
      return (double)aUnit->getType().turnRadius();
   }
}


BWAPI::Position PerceivedUnit::getPosition() const
{
   return mStateFuture.back().pos;
}


BWAPI::TilePosition PerceivedUnit::getTilePosition() const
{
   return BWAPI::TilePosition(this->getPosition());
}


bool PerceivedUnit::issueCommand(BWAPI::UnitCommand command)
{
   if (!mBaseUnit->canIssueCommand(command))
   {
      return false;
   }

   if (command.getType() == BWAPI::UnitCommandTypes::Move ||
       command.getType() == BWAPI::UnitCommandTypes::Attack_Move ||
       command.getType() == BWAPI::UnitCommandTypes::Attack_Unit)
   {
      //the game will not start executing whatever command is issued here until FRAME_LAG # frames later
      //save off this command until its either been completed or overwritten
      //only remove commands from the queue later when this unit is updated
      int frame = BWAPI::Broodwar->getFrameCount();
      //check if this new command replaces a previous one issued this same frame
      if (mCommands.size() > 0 && mCommands.front().first == frame)
      {
         //replace current command (short cut to save processing ???)
         mCommands.front().second = command;
      }
      else
      {
         //add this new command to the front of the FIFO
         mCommands.push_front(std::pair<int, BWAPI::UnitCommand>(frame, command));
      }
   }
  
   return  mBaseUnit->issueCommand(command);;
}



bool PerceivedUnit::attack(BWAPI::Position target, bool shiftQueueCommand)
{
   if (shiftQueueCommand == false)
   {
      //store it
      return issueCommand(BWAPI::UnitCommand::attack(mBaseUnit,target,false));
      //return mBaseUnit->attack(target);
   }
   return false;
}

bool PerceivedUnit::attack(BWAPI::Unit* target, bool shiftQueueCommand)
{
   if (shiftQueueCommand == false)
   {
      //store it
      return issueCommand(BWAPI::UnitCommand::attack(mBaseUnit,target,false));
      //return mBaseUnit->attack(target);
   }
   return false;
}

bool PerceivedUnit::move(BWAPI::Position target, bool shiftQueueCommand)
{
   if (shiftQueueCommand == false)
   {
      //store it
      return issueCommand(BWAPI::UnitCommand::move(mBaseUnit,target,false));
      //return mBaseUnit->move(target);
   }
   return false;
}




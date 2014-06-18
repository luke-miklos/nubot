

#ifndef PERCEIVED_UNIT_H
#define PERCEIVED_UNIT_H

//#include "BWAPI.h"

#include "BWAPI/Client/UnitData.h"
#include "BWAPI/Order.h"
#include "BWAPI/Position.h"
#include "BWAPI/Region.h"
#include "BWAPI/TechType.h"
#include "BWAPI/TilePosition.h"
#include "BWAPI/Unit.h"
#include "BWAPI/UnitCommand.h"
#include "BWAPI/UnitType.h"
#include "BWAPI/UpgradeType.h"
#include "BWAPI/WeaponType.h"

#include <deque>
#include <utility>
#include <list>


#define  FRAME_LAG  4

const bool DEBUG_DRAW_PERCEPTION = true;
const bool DEBUG_LOG_PERCEPTION  = true;


struct UnitState
{
   UnitState(BWAPI::Unit* unit, int aFrame)
   {
      frame = aFrame;
      angle = unit->getAngle();
      velX  = unit->getVelocityX();
      velY  = unit->getVelocityY();
      pos   = unit->getPosition();
      posX  = (double)pos.x();
      posY  = (double)pos.y();
   }
   void SetActive(UnitState prev)
   {
      if (prev.pos   != pos   ||                //moved since last frame
          prev.angle != angle ||                //turned since last frame
          !prev.HasVelocity() && HasVelocity()) //accelerated since last frame
      {
         active = true;
      }
      else
      {
         active = false;
      }
   }
   bool HasVelocity()
   {
      return (velX != 0 || velY != 0);
   }

   int frame;           //game frame count
   double angle;        //radians
   double velX;         //pixles/frame
   double velY;         //pixles/frame
   BWAPI::Position pos; //pixles x,y
   double posX;         //pixels
   double posY;         //pixels
   bool active;         //set to indicate whether the unit is moving or turning
};


class PerceivedUnit : public BWAPI::Unit
{
//private:
public:
   //should hold  FRAME_LAG + 1  commands at the most
   std::deque<std::pair<int, BWAPI::UnitCommand>> mCommands; //FIFO queue of command & game frame it was issued in (new commands in front, old out the back)
   typedef std::deque<std::pair<int, BWAPI::UnitCommand>>::iterator         CommandIterator;
   typedef std::deque<std::pair<int, BWAPI::UnitCommand>>::reverse_iterator ReverseCommandIterator;

   BWAPI::Unit*    mBaseUnit;
   int             mCurrentPerceivedFrame;

   std::deque<UnitState> mStateHistory;   //truth history
   std::deque<UnitState> mStateFuture;    //extrapolated guess at future - perception (back has FRAME_LAG guess at position)

protected:
   virtual ~PerceivedUnit() {};

public:
   //must use the composition design pattern, we can not copy the base class pointer
   PerceivedUnit(BWAPI::Unit* aBaseUnit);
   //extrapolate unit forward 'FRAME_LAG' frames
   void Update(int currentGameFrame);

   static double GetTurnRate(BWAPI::Unit* aUnit);  //degrees per frame

   //overwrite methods having to do with giving movement related commands, and receiving position related data
   //queries we are overriding (return a computed result instead of game result)
   virtual BWAPI::Position getPosition() const; //
   virtual BWAPI::TilePosition getTilePosition() const;  //
   virtual double getAngle() const { return mBaseUnit->getAngle(); }
   virtual double getVelocityX() const { return mBaseUnit->getVelocityX(); }
   virtual double getVelocityY() const { return mBaseUnit->getVelocityY(); }
   virtual int getLeft() const { return mBaseUnit->getLeft(); }
   virtual int getTop() const { return mBaseUnit->getTop(); }
   virtual int getRight() const { return mBaseUnit->getRight(); }
   virtual int getBottom() const { return mBaseUnit->getBottom(); }
   virtual int getLastCommandFrame() const { return mBaseUnit->getLastCommandFrame(); }
   virtual BWAPI::UnitCommand getLastCommand() const { return mBaseUnit->getLastCommand(); }
   virtual BWAPI::Unit* getTarget() const { return mBaseUnit->getTarget(); }
   virtual BWAPI::Position getTargetPosition() const { return mBaseUnit->getTargetPosition(); }
   virtual BWAPI::Order getOrder() const { return mBaseUnit->getOrder(); }
   virtual BWAPI::Order getSecondaryOrder() const { return mBaseUnit->getSecondaryOrder(); }
   virtual BWAPI::Unit* getOrderTarget() const { return mBaseUnit->getOrderTarget(); }
   virtual BWAPI::Position getOrderTargetPosition() const { return mBaseUnit->getOrderTargetPosition(); }
   //commands we are overriding (store the command in the queue)
   virtual bool issueCommand(BWAPI::UnitCommand command);   //
   virtual bool attack(BWAPI::Position target, bool shiftQueueCommand = false);  //
   virtual bool attack(BWAPI::Unit* target, bool shiftQueueCommand = false);  //
   virtual bool move(BWAPI::Position target, bool shiftQueueCommand = false); //
   virtual bool patrol(BWAPI::Position target, bool shiftQueueCommand = false) { return mBaseUnit->patrol(target, shiftQueueCommand); }
   virtual bool holdPosition(bool shiftQueueCommand = false) { return mBaseUnit->holdPosition(shiftQueueCommand); }
   virtual bool stop(bool shiftQueueCommand = false) { return mBaseUnit->stop(shiftQueueCommand); }
   virtual bool follow(BWAPI::Unit* target, bool shiftQueueCommand = false) { return mBaseUnit->follow(target, shiftQueueCommand); }
   virtual bool rightClick(BWAPI::Position target, bool shiftQueueCommand = false) { return mBaseUnit->rightClick(target, shiftQueueCommand); }
   virtual bool rightClick(BWAPI::Unit* target, bool shiftQueueCommand = false) { return mBaseUnit->rightClick(target, shiftQueueCommand); }

   //all others are pass through
   //queries (const)
   virtual int getID() const { return mBaseUnit->getID(); }
   virtual int getReplayID() const { return mBaseUnit->getReplayID(); }
   virtual BWAPI::Player* getPlayer() const { return mBaseUnit->getPlayer(); }
   virtual BWAPI::UnitType getType() const { return mBaseUnit->getType(); }
   virtual BWAPI::Region *getRegion() const { return mBaseUnit->getRegion(); }
   virtual int getHitPoints() const { return mBaseUnit->getHitPoints(); }
   virtual int getShields() const { return mBaseUnit->getShields(); }
   virtual int getEnergy() const { return mBaseUnit->getEnergy(); }
   virtual int getResources() const { return mBaseUnit->getResources(); }
   virtual int getResourceGroup() const { return mBaseUnit->getResourceGroup(); }
   virtual int getDistance(BWAPI::Unit* target) const { return mBaseUnit->getDistance(target); }
   virtual int getDistance(BWAPI::Position target) const { return mBaseUnit->getDistance(target); }
   virtual bool hasPath(BWAPI::Unit* target) const { return mBaseUnit->hasPath(target); }
   virtual bool hasPath(BWAPI::Position target) const { return mBaseUnit->hasPath(target); }
   virtual BWAPI::Player *getLastAttackingPlayer() const { return mBaseUnit->getLastAttackingPlayer(); }
   virtual int getUpgradeLevel(BWAPI::UpgradeType upgrade) const { return mBaseUnit->getUpgradeLevel(upgrade); }
   virtual BWAPI::UnitType getInitialType() const { return mBaseUnit->getInitialType(); }
   virtual BWAPI::Position getInitialPosition() const { return mBaseUnit->getInitialPosition(); }
   virtual BWAPI::TilePosition getInitialTilePosition() const { return mBaseUnit->getInitialTilePosition(); }
   virtual int getInitialHitPoints() const { return mBaseUnit->getInitialHitPoints(); }
   virtual int getInitialResources() const { return mBaseUnit->getInitialResources(); }
   virtual int getKillCount() const { return mBaseUnit->getKillCount(); }
   virtual int getAcidSporeCount() const { return mBaseUnit->getAcidSporeCount(); }
   virtual int getInterceptorCount() const { return mBaseUnit->getInterceptorCount(); }
   virtual int getScarabCount() const { return mBaseUnit->getScarabCount(); }
   virtual int getSpiderMineCount() const { return mBaseUnit->getSpiderMineCount(); }
   virtual int getGroundWeaponCooldown() const { return mBaseUnit->getGroundWeaponCooldown(); }
   virtual int getAirWeaponCooldown() const { return mBaseUnit->getAirWeaponCooldown(); }
   virtual int getSpellCooldown() const { return mBaseUnit->getSpellCooldown(); }
   virtual int getDefenseMatrixPoints() const { return mBaseUnit->getDefenseMatrixPoints(); }
   virtual int getDefenseMatrixTimer() const { return mBaseUnit->getDefenseMatrixTimer(); }
   virtual int getEnsnareTimer() const { return mBaseUnit->getEnsnareTimer(); }
   virtual int getIrradiateTimer() const { return mBaseUnit->getIrradiateTimer(); }
   virtual int getLockdownTimer() const { return mBaseUnit->getLockdownTimer(); }
   virtual int getMaelstromTimer() const { return mBaseUnit->getMaelstromTimer(); }
   virtual int getOrderTimer() const { return mBaseUnit->getOrderTimer(); }
   virtual int getPlagueTimer() const { return mBaseUnit->getPlagueTimer(); }
   virtual int getRemoveTimer() const { return mBaseUnit->getRemoveTimer(); }
   virtual int getStasisTimer() const { return mBaseUnit->getStasisTimer(); }
   virtual int getStimTimer() const { return mBaseUnit->getStimTimer(); }
   virtual BWAPI::UnitType getBuildType() const { return mBaseUnit->getBuildType(); }
   virtual std::list<BWAPI::UnitType > getTrainingQueue() const { return mBaseUnit->getTrainingQueue(); }
   virtual BWAPI::TechType getTech() const { return mBaseUnit->getTech(); }
   virtual BWAPI::UpgradeType getUpgrade() const { return mBaseUnit->getUpgrade(); }
   virtual int getRemainingBuildTime() const { return mBaseUnit->getRemainingBuildTime(); }
   virtual int getRemainingTrainTime() const { return mBaseUnit->getRemainingTrainTime(); }
   virtual int getRemainingResearchTime() const { return mBaseUnit->getRemainingResearchTime(); }
   virtual int getRemainingUpgradeTime() const { return mBaseUnit->getRemainingUpgradeTime(); }
   virtual BWAPI::Unit* getBuildUnit() const { return mBaseUnit->getBuildUnit(); }
   virtual BWAPI::Position getRallyPosition() const { return mBaseUnit->getRallyPosition(); }
   virtual BWAPI::Unit* getRallyUnit() const { return mBaseUnit->getRallyUnit(); }
   virtual BWAPI::Unit* getAddon() const { return mBaseUnit->getAddon(); }
   virtual BWAPI::Unit* getNydusExit() const { return mBaseUnit->getNydusExit(); }
   virtual BWAPI::Unit* getPowerUp() const { return mBaseUnit->getPowerUp(); }
   virtual BWAPI::Unit* getTransport() const { return mBaseUnit->getTransport(); }
   virtual std::set<BWAPI::Unit*> getLoadedUnits() const { return mBaseUnit->getLoadedUnits(); }
   virtual BWAPI::Unit* getCarrier() const { return mBaseUnit->getCarrier(); }
   virtual std::set<BWAPI::Unit*> getInterceptors() const { return mBaseUnit->getInterceptors(); }
   virtual BWAPI::Unit* getHatchery() const { return mBaseUnit->getHatchery(); }
   virtual std::set<BWAPI::Unit*> getLarva() const { return mBaseUnit->getLarva(); }
   virtual std::set<BWAPI::Unit*>& getUnitsInRadius(int radius) const { return mBaseUnit->getUnitsInRadius(radius); }
   virtual std::set<BWAPI::Unit*>& getUnitsInWeaponRange(BWAPI::WeaponType weapon) const { return mBaseUnit->getUnitsInWeaponRange(weapon); }
   virtual void* getClientInfo() const { return mBaseUnit->getClientInfo(); }
   virtual bool exists() const { return mBaseUnit->exists(); }
   virtual bool hasNuke() const { return mBaseUnit->hasNuke(); }
   virtual bool isAccelerating() const { return mBaseUnit->isAccelerating(); }
   virtual bool isAttacking() const { return mBaseUnit->isAttacking(); }
   virtual bool isAttackFrame() const { return mBaseUnit->isAttackFrame(); }
   virtual bool isBeingConstructed() const { return mBaseUnit->isBeingConstructed(); }
   virtual bool isBeingGathered() const { return mBaseUnit->isBeingGathered(); }
   virtual bool isBeingHealed() const { return mBaseUnit->isBeingHealed(); }
   virtual bool isBlind() const { return mBaseUnit->isBlind(); }
   virtual bool isBraking() const { return mBaseUnit->isBraking(); }
   virtual bool isBurrowed() const { return mBaseUnit->isBurrowed(); }
   virtual bool isCarryingGas() const { return mBaseUnit->isCarryingGas(); }
   virtual bool isCarryingMinerals() const { return mBaseUnit->isCarryingMinerals(); }
   virtual bool isCloaked() const { return mBaseUnit->isCloaked(); }
   virtual bool isCompleted() const { return mBaseUnit->isCompleted(); }
   virtual bool isConstructing() const { return mBaseUnit->isConstructing(); }
   virtual bool isDefenseMatrixed() const { return mBaseUnit->isDefenseMatrixed(); }
   virtual bool isDetected() const { return mBaseUnit->isDetected(); }
   virtual bool isEnsnared() const { return mBaseUnit->isEnsnared(); }
   virtual bool isFollowing() const { return mBaseUnit->isFollowing(); }
   virtual bool isGatheringGas() const { return mBaseUnit->isGatheringGas(); }
   virtual bool isGatheringMinerals() const { return mBaseUnit->isGatheringMinerals(); }
   virtual bool isHallucination() const { return mBaseUnit->isHallucination(); }
   virtual bool isHoldingPosition() const { return mBaseUnit->isHoldingPosition(); }
   virtual bool isIdle() const { return mBaseUnit->isIdle(); }
   virtual bool isInterruptible() const { return mBaseUnit->isInterruptible(); }
   virtual bool isInvincible() const { return mBaseUnit->isInvincible(); }
   virtual bool isInWeaponRange(BWAPI::Unit *target) const { return mBaseUnit->isInWeaponRange(target); }
   virtual bool isIrradiated() const { return mBaseUnit->isIrradiated(); }
   virtual bool isLifted() const { return mBaseUnit->isLifted(); }
   virtual bool isLoaded() const { return mBaseUnit->isLoaded(); }
   virtual bool isLockedDown() const { return mBaseUnit->isLockedDown(); }
   virtual bool isMaelstrommed() const { return mBaseUnit->isMaelstrommed(); }
   virtual bool isMorphing() const { return mBaseUnit->isMorphing(); }
   virtual bool isMoving() const { return mBaseUnit->isMoving(); }
   virtual bool isParasited() const { return mBaseUnit->isParasited(); }
   virtual bool isPatrolling() const { return mBaseUnit->isPatrolling(); }
   virtual bool isPlagued() const { return mBaseUnit->isPlagued(); }
   virtual bool isRepairing() const { return mBaseUnit->isRepairing(); }
   virtual bool isResearching() const { return mBaseUnit->isResearching(); }
   virtual bool isSelected() const { return mBaseUnit->isSelected(); }
   virtual bool isSieged() const { return mBaseUnit->isSieged(); }
   virtual bool isStartingAttack() const { return mBaseUnit->isStartingAttack(); }
   virtual bool isStasised() const { return mBaseUnit->isStasised(); }
   virtual bool isStimmed() const { return mBaseUnit->isStimmed(); }
   virtual bool isStuck() const { return mBaseUnit->isStuck(); }
   virtual bool isTraining() const { return mBaseUnit->isTraining(); }
   virtual bool isUnderAttack() const { return mBaseUnit->isUnderAttack(); }
   virtual bool isUnderDarkSwarm() const { return mBaseUnit->isUnderDarkSwarm(); }
   virtual bool isUnderDisruptionWeb() const { return mBaseUnit->isUnderDisruptionWeb(); }
   virtual bool isUnderStorm() const { return mBaseUnit->isUnderStorm(); }
   virtual bool isUnpowered() const { return mBaseUnit->isUnpowered(); }
   virtual bool isUpgrading() const { return mBaseUnit->isUpgrading(); }
   virtual bool isVisible() const { return mBaseUnit->isVisible(); }
   virtual bool isVisible(BWAPI::Player* player) const { return mBaseUnit->isVisible(player); }
   virtual bool canIssueCommand(BWAPI::UnitCommand command) const { return mBaseUnit->canIssueCommand(command); }
   //commands (non const)
   virtual void setClientInfo(void* clientinfo) { return mBaseUnit->setClientInfo(clientinfo); }
   virtual bool build(BWAPI::TilePosition target, BWAPI::UnitType type) { return mBaseUnit->build(target, type); }
   virtual bool buildAddon(BWAPI::UnitType type) { return mBaseUnit->buildAddon(type); }
   virtual bool train(BWAPI::UnitType type) { return mBaseUnit->train(type); }
   virtual bool morph(BWAPI::UnitType type) { return mBaseUnit->morph(type); }
   virtual bool research(BWAPI::TechType tech) { return mBaseUnit->research(tech); }
   virtual bool upgrade(BWAPI::UpgradeType upgrade) { return mBaseUnit->upgrade(upgrade); }
   virtual bool setRallyPoint(BWAPI::Position target) { return mBaseUnit->setRallyPoint(target); }
   virtual bool setRallyPoint(BWAPI::Unit* target) { return mBaseUnit->setRallyPoint(target); }
   virtual bool gather(BWAPI::Unit* target, bool shiftQueueCommand = false) { return mBaseUnit->gather(target, shiftQueueCommand); }
   virtual bool returnCargo(bool shiftQueueCommand = false) { return mBaseUnit->returnCargo(shiftQueueCommand); }
   virtual bool repair(BWAPI::Unit* target, bool shiftQueueCommand = false) { return mBaseUnit->repair(target, shiftQueueCommand); }
   virtual bool burrow() { return mBaseUnit->burrow(); }
   virtual bool unburrow() { return mBaseUnit->unburrow(); }
   virtual bool cloak() { return mBaseUnit->cloak(); }
   virtual bool decloak() { return mBaseUnit->decloak(); }
   virtual bool siege() { return mBaseUnit->siege(); }
   virtual bool unsiege() { return mBaseUnit->unsiege(); }
   virtual bool lift() { return mBaseUnit->lift(); }
   virtual bool land(BWAPI::TilePosition target) { return mBaseUnit->land(target); }
   virtual bool load(BWAPI::Unit* target, bool shiftQueueCommand = false) { return mBaseUnit->load(target, shiftQueueCommand); }
   virtual bool unload(BWAPI::Unit* target) { return mBaseUnit->unload(target); }
   virtual bool unloadAll(bool shiftQueueCommand = false) { return mBaseUnit->unloadAll(); }
   virtual bool unloadAll(BWAPI::Position target, bool shiftQueueCommand = false) { return mBaseUnit->unloadAll(); }
   virtual bool haltConstruction() { return mBaseUnit->haltConstruction(); }
   virtual bool cancelConstruction() { return mBaseUnit->cancelConstruction(); }
   virtual bool cancelAddon() { return mBaseUnit->cancelAddon(); }
   virtual bool cancelTrain(int slot = -2) { return mBaseUnit->cancelTrain(); }
   virtual bool cancelMorph() { return mBaseUnit->cancelMorph(); }
   virtual bool cancelResearch() { return mBaseUnit->cancelResearch(); }
   virtual bool cancelUpgrade() { return mBaseUnit->cancelUpgrade(); }
   virtual bool useTech(BWAPI::TechType tech) { return mBaseUnit->useTech(tech); }
   virtual bool useTech(BWAPI::TechType tech, BWAPI::Position target) { return mBaseUnit->useTech(tech, target); }
   virtual bool useTech(BWAPI::TechType tech, BWAPI::Unit* target) { return mBaseUnit->useTech(tech, target); }
   virtual bool placeCOP(BWAPI::TilePosition target) { return mBaseUnit->placeCOP(target); }
};


#endif

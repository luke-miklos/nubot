#include "Common.h"
#include "ProductionManager.h"

#include "..\..\StarcraftBuildOrderSearch\Source\starcraftsearch\ActionSet.hpp"
#include "..\..\StarcraftBuildOrderSearch\Source\starcraftsearch\DFBBStarcraftSearch.hpp"
#include "..\..\StarcraftBuildOrderSearch\Source\starcraftsearch\StarcraftData.hpp"

//#define BOADD(N, T, B) for (int i=0; i<N; ++i) { queue.queueAsLowestPriority(MetaType(T), B); }

#define GOAL_ADD(G, M, N) G.push_back(std::pair<MetaType, int>(M, N))

ProductionManager::ProductionManager() 
   : initialBuildSet(false)
   , reservedMinerals(0)
   , reservedGas(0)
   , assignedWorkerForThisBuilding(false)
   , haveLocationForThisBuilding(false)
   , enemyCloakedDetected(false)
   , rushDetected(false)
   , mLastCommandLatencyFrameDone(0)
{
   //populateTypeCharMap();

   //setBuildOrder(StarcraftBuildOrderSearchManager::Instance().getOpeningBuildOrder());
   if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
   {
      setBuildOrder(std::vector<MetaType>(1,(MetaType(BWAPI::UnitTypes::Protoss_Probe))));
   }
   else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
   {
      setBuildOrder(std::vector<MetaType>(1,(MetaType(BWAPI::UnitTypes::Terran_SCV))));
   }
   else  // BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg
   {
      setBuildOrder(std::vector<MetaType>(1,(MetaType(BWAPI::UnitTypes::Zerg_Drone))));
   }
}

void ProductionManager::setBuildOrder(const std::vector<MetaType> & buildOrder)
{
   // clear the current build order
   queue.clearAll();

   // for each item in the results build order, add it
   for (size_t i(0); i<buildOrder.size(); ++i)
   {
      // queue the item
      queue.queueAsLowestPriority(buildOrder[i], true);
   }
}

//void ProductionManager::testBuildOrderSearch(const std::vector< std::pair<MetaType, UnitCountType> > & goal)
//{   
//   std::vector<MetaType> buildOrder = StarcraftBuildOrderSearchManager::Instance().findBuildOrder(goal);
//
//   // set the build order
//   setBuildOrder(buildOrder);
//}

void ProductionManager::update() 
{
   // check the queue for stuff we can build
   manageBuildOrderQueue();

   int frame = BWAPI::Broodwar->getFrameCount();

   int minerals = BWAPI::Broodwar->self()->minerals();
   int largest = queue.largestMineralPrice();
   // if nothing is currently building, get a new goal from the strategy manager
   //or if we have a ton of minerals, replan
   if ((queue.size() == 0 || (minerals > (largest+200))) && frame > 100 && frame > mLastCommandLatencyFrameDone)
   {
      mLastCommandLatencyFrameDone = frame + 3*24;   //dont search again for at least 3 seconds

      //BWAPI::Broodwar->drawTextScreen(150, 10, "Nothing left to build, new search!");

      //const std::vector< std::pair<MetaType, UnitCountType> > newGoal = StrategyManager::Instance().getBuildOrderGoal();
      //testBuildOrderSearch(newGoal);

      //use our macro search instead of their strategy manager
      int frame = BWAPI::Broodwar->getFrameCount();
      //std::vector<QueuedMove*> moves = mMacroSearch.FindMoves(frame+3600, 5000);   //2.5 minute look ahead (2.5*60*24), only search for 5 sec real-time
      //std::vector<QueuedMove*> moves = mMacroSearch.FindMoves(frame+3600, 150);   //2.5 minute look ahead (2.5*60*24), only search for 150 ms real-time
    //std::vector<QueuedMove*> moves = mMacroSearch.FindMoves(frame+2880, -1);   //2 minute look ahead (2.5*60*24), no real-time limit
      //std::vector<QueuedMove*> moves = mMacroSearch.FindMoves(frame+3600, 2000);   //2.5 minute look ahead (2.5*60*24), 2-second time limit
    //std::vector<QueuedMove*> moves = mMacroSearch.FindMoves(frame+2880, -1, 20);   //2 minute look ahead (2.5*60*24), no real-time limit, search depth 20
    //std::vector<QueuedMove*> moves = mMacroSearch.FindMoves(frame+2880, 5000, 6);
      std::vector<int> moves = mMacroSearch.FindMoves(frame+2880, -1, 6);

      int N = moves.size();
      //N = (N>4)?(4):(N);
      if (N>0)
      {
         queue.clearAll();
         for (int i=0; i<N; ++i)
         {
          //queue.queueAsLowestPriority(MetaType(BWAPI::UnitType(moves[i]->move)), true);
            queue.queueAsLowestPriority(MetaType(BWAPI::UnitType(moves[i])), false);
         }
      }

   }

   // if they have cloaked units get a new goal asap
   if (!enemyCloakedDetected && InformationManager::Instance().enemyHasCloakedUnits())
   {
      if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
      {
         if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon) < 2)
         {
            queue.queueAsHighestPriority(MetaType(BWAPI::UnitTypes::Protoss_Photon_Cannon), true);
            queue.queueAsHighestPriority(MetaType(BWAPI::UnitTypes::Protoss_Photon_Cannon), true);
         }

         if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Forge) == 0)
         {
            queue.queueAsHighestPriority(MetaType(BWAPI::UnitTypes::Protoss_Forge), true);
         }

         BWAPI::Broodwar->printf("Enemy Cloaked Unit Detected!");
         enemyCloakedDetected = true;
      }
      else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
      {
         if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Terran_Missile_Turret) < 2)
         {
            queue.queueAsHighestPriority(MetaType(BWAPI::UnitTypes::Terran_Missile_Turret), true);
            queue.queueAsHighestPriority(MetaType(BWAPI::UnitTypes::Terran_Missile_Turret), true);
         }
         if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) == 0)
         {
            queue.queueAsHighestPriority(MetaType(BWAPI::UnitTypes::Terran_Engineering_Bay), true);
         }
         BWAPI::Broodwar->printf("Enemy Cloaked Unit Detected!");
         enemyCloakedDetected = true;
      }
      else  //zerg has detectors
      {

      }
   }


//   if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(447, 17, "\x07 %d", BuildingManager::Instance().getReservedMinerals());
}



void ProductionManager::onUnitCreate(BWAPI::Unit * unit)
{
   mMacroSearch.onUnitCreate(unit);

   ////set rally point to region choke point
   //if (unit->getType().canProduce() && unit->getType().isBuilding() && !unit->getType().isResourceDepot())
   //{
   //   BWTA::Region* region = BWTA::getRegion(unit->getTilePosition());
   //   if (region != NULL)
   //   {
   //      BWAPI::Position rallyPoint = (*region->getChokepoints().begin())->getCenter();
   //      unit->setRallyPoint(rallyPoint);
   //   }
   //}
}

void ProductionManager::onUnitMorph(BWAPI::Unit * unit)
{
   mMacroSearch.onUnitMorph(unit);
}

void ProductionManager::onUnitDestroy(BWAPI::Unit * unit)
{
   // we don't care if it's not our unit
   if (!unit || unit->getPlayer() != BWAPI::Broodwar->self())
   {
      return;
   }
      
   int need = BWAPI::Broodwar->self()->supplyUsed();
   int have = BWAPI::Broodwar->self()->supplyTotal();
   // if it's a worker or a building, we need to re-search for the current goal
   if ((unit->getType().isWorker() && !WorkerManager::Instance().isWorkerScout(unit)) || unit->getType().isBuilding() || 
       (unit->getType().supplyProvided() > 0 && (have<=(need+1))))
   {
      BWAPI::Broodwar->printf("Critical unit died, re-searching build order");

      //if (unit->getType() != BWAPI::UnitTypes::Zerg_Drone)
      //{
      //   testBuildOrderSearch(StrategyManager::Instance().getBuildOrderGoal());
      //}
      int frame = BWAPI::Broodwar->getFrameCount();
      //std::vector<QueuedMove*> moves = mMacroSearch.FindMoves(frame+3600, 5000);   //2.5 minute look ahead (2.5*60*24), only search for 5 sec real-time
      //std::vector<QueuedMove*> moves = mMacroSearch.FindMoves(frame+3600, 150);   //2.5 minute look ahead (2.5*60*24), only search for 150 ms real-time
      //std::vector<QueuedMove*> moves = mMacroSearch.FindMoves(frame+2880, 2000, 20);   //2.0 minute look ahead (2.5*60*24),2 sec limit, search depth 20
    //std::vector<QueuedMove*> moves = mMacroSearch.FindMoves(frame+2880, 5000, 6);   //2.0 minute look ahead (2.5*60*24),2 sec limit, search depth 20
      std::vector<int> moves = mMacroSearch.FindMoves(frame+2880, -1, 6);   //2.0 minute look ahead (2.5*60*24),2 sec limit, search depth 20
      //std::vector<QueuedMove*> moves = mMacroSearch.FindMoves(frame+3600, 2000);   //2.5 minute look ahead (2.5*60*24), 2-second time limit

      //if (moves.size()>0)
      //{
      //   queue.clearAll();
      //   queue.queueAsHighestPriority(MetaType(BWAPI::UnitType(moves[0]->move)), true);
      //   if (moves.size()>1)
      //   {
      //      queue.queueAsLowestPriority(MetaType(BWAPI::UnitType(moves[1]->move)), true);
      //   }
      //}
      int N = moves.size();
      //N = (N>4)?(4):(N);
      if (N>0)
      {
         queue.clearAll();
         for (int i=0; i<N; ++i)
         {
          //queue.queueAsLowestPriority(MetaType(BWAPI::UnitType(moves[i]->move)), true);
            queue.queueAsLowestPriority(MetaType(BWAPI::UnitType(moves[i])), false);
         }
      }
   }
}

void ProductionManager::manageBuildOrderQueue() 
{
   // if there is nothing in the queue, oh well
   if (queue.isEmpty()) 
   {
      return;
   }

   // the current item to be used
   //queue.setNumSkipped(0);
   BuildOrderItem<PRIORITY_TYPE> currentItem = queue.getHighestPriorityItem();

   int size = sizeof(BuildOrderItem<PRIORITY_TYPE>);

   // while there is still something left in the queue
   while (!queue.isEmpty()) 
   {
      // this is the unit which can produce the currentItem
      BWAPI::Unit * producer = selectUnitOfType(currentItem.metaType.whatBuilds());

      // check to see if we can make it right now
      bool canMake = canMakeNow(producer, currentItem.metaType);

      //TODO do we restrict this?
      // if we try to build too many refineries manually remove it
      if (currentItem.metaType.isRefinery() && (BWAPI::Broodwar->self()->allUnitCount(BWAPI::Broodwar->self()->getRace().getRefinery() >= 3)))
      {
         queue.removeCurrentHighestPriorityItem();
         break;
      }

      // if the next item in the list is a building and we can't yet make it
      if (currentItem.metaType.isBuilding() && !(producer && canMake))
      {
         // construct a temporary building object
         Building b(currentItem.metaType.unitType, BWAPI::Broodwar->self()->getStartLocation());

         // set the producer as the closest worker, but do not set its job yet
         producer = WorkerManager::Instance().getBuilder(b, false);

         // predict the worker movement to that building location
         predictWorkerMovement(b);
      }

      // if we can make the current item
      if (producer && canMake) 
      {
         // create it
         createMetaType(producer, currentItem.metaType);
         assignedWorkerForThisBuilding = false;
         haveLocationForThisBuilding = false;

         // and remove it from the queue
         queue.removeCurrentHighestPriorityItem();

         //store this time so we dont perform any new searches until this is over
       //mLastCommandLatencyFrameDone = BWAPI::Broodwar->getFrameCount() + BWAPI::Broodwar->getLatencyFrames() + 1;
         mLastCommandLatencyFrameDone = BWAPI::Broodwar->getFrameCount() + 12;

         // don't actually loop around in here
         break;
      }
      // otherwise, if we can skip the current item
      else if (queue.canSkipItem())
      {
         // skip it
         queue.skipItem();

         // and get the next one
         currentItem = queue.getNextHighestPriorityItem();            
      }
      else 
      {
         // so break out
         break;
      }
   }
}

bool ProductionManager::canMakeNow(BWAPI::Unit * producer, MetaType t)
{
   bool canMake = meetsReservedResources(t);
   if (canMake)
   {
      if (t.isUnit())
      {
         canMake = BWAPI::Broodwar->canMake(producer, t.unitType);
      }
      else if (t.isTech())
      {
         canMake = BWAPI::Broodwar->canResearch(producer, t.techType);
      }
      else if (t.isUpgrade())
      {
         canMake = BWAPI::Broodwar->canUpgrade(producer, t.upgradeType);
      }
      else
      {   
         assert(false);
      }
   }

   return canMake;
}

bool ProductionManager::detectBuildOrderDeadlock()
{
   // if the queue is empty there is no deadlock
   if (queue.size() == 0 || BWAPI::Broodwar->self()->supplyTotal() >= 390)
   {
      return false;
   }

   // are any supply providers being built currently
   bool supplyInProgress =      BuildingManager::Instance().isBeingBuilt(BWAPI::Broodwar->self()->getRace().getCenter()) || 
                        BuildingManager::Instance().isBeingBuilt(BWAPI::Broodwar->self()->getRace().getSupplyProvider());

   // does the current item being built require more supply
   int supplyCost         = queue.getHighestPriorityItem().metaType.supplyRequired();
   int supplyAvailable      = std::max(0, BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed());

   // if we don't have enough supply and none is being built, there's a deadlock
   if ((supplyAvailable < supplyCost) && !supplyInProgress)
   {
      return true;
   }

   return false;
}

// When the next item in the queue is a building, this checks to see if we should move to it
// This function is here as it needs to access prodction manager's reserved resources info
void ProductionManager::predictWorkerMovement(const Building & b)
{
   // get a possible building location for the building
   if (!haveLocationForThisBuilding)
   {
      predictedTilePosition         = BuildingManager::Instance().getBuildingLocation(b);
   }

   if (predictedTilePosition != BWAPI::TilePositions::None)
   {
      haveLocationForThisBuilding      = true;
   }
   else
   {
      return;
   }
   
   // draw a box where the building will be placed
   int x1 = predictedTilePosition.x() * 32;
   int x2 = x1 + (b.type.tileWidth()) * 32;
   int y1 = predictedTilePosition.y() * 32;
   int y2 = y1 + (b.type.tileHeight()) * 32;
   if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Blue, false);

   // where we want the worker to walk to
   BWAPI::Position walkToPosition   = BWAPI::Position(x1 + (b.type.tileWidth()/2)*32, y1 + (b.type.tileHeight()/2)*32);

   // compute how many resources we need to construct this building
   int mineralsRequired             = std::max(0, b.type.mineralPrice() - getFreeMinerals());
   int gasRequired                  = std::max(0, b.type.gasPrice() - getFreeGas());

   // get a candidate worker to move to this location
   BWAPI::Unit * moveWorker         = WorkerManager::Instance().getMoveWorker(walkToPosition);

   // Conditions under which to move the worker: 
   //      - there's a valid worker to move
   //      - we haven't yet assigned a worker to move to this location
   //      - the build position is valid
   //      - we will have the required resources by the time the worker gets there
   if (moveWorker && haveLocationForThisBuilding && !assignedWorkerForThisBuilding && (predictedTilePosition != BWAPI::TilePositions::None) &&
      WorkerManager::Instance().willHaveResources(mineralsRequired, gasRequired, moveWorker->getDistance(walkToPosition)) )
   {
      // we have assigned a worker
      assignedWorkerForThisBuilding = true;

      // tell the worker manager to move this worker
      WorkerManager::Instance().setMoveWorker(mineralsRequired, gasRequired, walkToPosition);
   }
}

void ProductionManager::performCommand(BWAPI::UnitCommandType t) {

   // if it is a cancel construction, it is probably the extractor trick
   if (t == BWAPI::UnitCommandTypes::Cancel_Construction)
   {
      BWAPI::Unit * extractor = NULL;
      std::set<BWAPI::Unit*>::const_iterator it = BWAPI::Broodwar->self()->getUnits().begin();
      for (; it != BWAPI::Broodwar->self()->getUnits().end(); it++)
      {
         BWAPI::Unit * unit = *it;
         if (unit->getType() == BWAPI::UnitTypes::Zerg_Extractor)
         {
            extractor = unit;
         }
      }

      if (extractor)
      {
         extractor->cancelConstruction();
      }
   }
}

int ProductionManager::getFreeMinerals()
{
   return BWAPI::Broodwar->self()->minerals() - BuildingManager::Instance().getReservedMinerals();
}

int ProductionManager::getFreeGas()
{
   return BWAPI::Broodwar->self()->gas() - BuildingManager::Instance().getReservedGas();
}

// return whether or not we meet resources, including building reserves
bool ProductionManager::meetsReservedResources(MetaType type) 
{
   // return whether or not we meet the resources
   return (type.mineralPrice() <= getFreeMinerals()) && (type.gasPrice() <= getFreeGas());
}

// this function will check to see if all preconditions are met and then create a unit
void ProductionManager::createMetaType(BWAPI::Unit * producer, MetaType t) 
{
   if (!producer)
   {
      return;
   }

   // TODO: special case of evolved zerg buildings needs to be handled

   // if we're dealing with a building
   if (t.isUnit() && t.unitType.isBuilding() 
      && t.unitType != BWAPI::UnitTypes::Zerg_Lair 
      && t.unitType != BWAPI::UnitTypes::Zerg_Hive
      && t.unitType != BWAPI::UnitTypes::Zerg_Greater_Spire)
   {
      // send the building task to the building manager
      BuildingManager::Instance().addBuildingTask(t.unitType, BWAPI::Broodwar->self()->getStartLocation());
   }
   // if we're dealing with a non-building unit
   else if (t.isUnit()) 
   {
      // if the race is zerg, morph the unit
      if (t.unitType.getRace() == BWAPI::Races::Zerg) {
         producer->morph(t.unitType);

      // if not, train the unit
      } else {
         producer->train(t.unitType);
      }
   }
   // if we're dealing with a tech research
   else if (t.isTech())
   {
      producer->research(t.techType);
   }
   else if (t.isUpgrade())
   {
      //Logger::Instance().log("Produce Upgrade: " + t.getName() + "\n");
      producer->upgrade(t.upgradeType);
   }
   else
   {   
      // critical error check
//      assert(false);

      //Logger::Instance().log("createMetaType error: " + t.getName() + "\n");
   }
   
}

// selects a unit of a given type
BWAPI::Unit * ProductionManager::selectUnitOfType(BWAPI::UnitType type, bool leastTrainingTimeRemaining, BWAPI::Position closestTo) {

   // if we have none of the unit type, return NULL right away
   if (BWAPI::Broodwar->self()->completedUnitCount(type) == 0) 
   {
      return NULL;
   }

   BWAPI::Unit * unit = NULL;

   // if we are concerned about the position of the unit, that takes priority
   if (closestTo != BWAPI::Position(0,0))
   {
      double minDist(1000000);
      std::set<BWAPI::Unit*>::const_iterator it = BWAPI::Broodwar->self()->getUnits().begin();
      for (; it != BWAPI::Broodwar->self()->getUnits().end(); it++)
      {
         BWAPI::Unit * u = *it;
         if (u->getType() == type)
         {
            double distance = u->getDistance(closestTo);
            if (!unit || distance < minDist)
            {
               unit = u;
               minDist = distance;
            }
         }
      }
      // if it is a building and we are worried about selecting the unit with the least
      // amount of training time remaining
   }
   else if (type.isBuilding() && leastTrainingTimeRemaining)
   {
      std::set<BWAPI::Unit*>::const_iterator it = BWAPI::Broodwar->self()->getUnits().begin();
      for (; it != BWAPI::Broodwar->self()->getUnits().end(); it++)
      {
         BWAPI::Unit * u = *it;
         if (u->getType() == type && u->isCompleted() && !u->isTraining() && !u->isLifted() &&!u->isUnpowered())
         {
            return u;
         }
      }
      // otherwise just return the first unit we come across
   }
   else
   {
      std::set<BWAPI::Unit*>::const_iterator it = BWAPI::Broodwar->self()->getUnits().begin();
      for (; it != BWAPI::Broodwar->self()->getUnits().end(); it++)
      {
         BWAPI::Unit * u = *it;
         if (u->getType() == type && u->isCompleted() && u->getHitPoints() > 0 && !u->isLifted() &&!u->isUnpowered()) 
         {
            return u;
         }
      }
   }

   // return what we've found so far
   return NULL;
}

void ProductionManager::onSendText(std::string text)
{
   //MetaType typeWanted(BWAPI::UnitTypes::None);
   //int numWanted = 0;

   //if (text.compare("clear") == 0)
   //{
   //   searchGoal.clear();
   //}
   //else if (text.compare("search") == 0)
   //{
   //   testBuildOrderSearch(searchGoal);
   //   searchGoal.clear();
   //}
   //else if (text[0] >= 'a' && text[0] <= 'z')
   //{
   //   MetaType typeWanted = typeCharMap[text[0]];
   //   text = text.substr(2,text.size());
   //   numWanted = atoi(text.c_str());

   //   searchGoal.push_back(std::pair<MetaType, int>(typeWanted, numWanted));
   //}
}

//void ProductionManager::populateTypeCharMap()
//{
//   typeCharMap['p'] = MetaType(BWAPI::UnitTypes::Protoss_Probe);
//   typeCharMap['z'] = MetaType(BWAPI::UnitTypes::Protoss_Zealot);
//   typeCharMap['d'] = MetaType(BWAPI::UnitTypes::Protoss_Dragoon);
//   typeCharMap['t'] = MetaType(BWAPI::UnitTypes::Protoss_Dark_Templar);
//   typeCharMap['c'] = MetaType(BWAPI::UnitTypes::Protoss_Corsair);
//   typeCharMap['e'] = MetaType(BWAPI::UnitTypes::Protoss_Carrier);
//   typeCharMap['h'] = MetaType(BWAPI::UnitTypes::Protoss_High_Templar);
//   typeCharMap['n'] = MetaType(BWAPI::UnitTypes::Protoss_Photon_Cannon);
//   typeCharMap['a'] = MetaType(BWAPI::UnitTypes::Protoss_Arbiter);
//   typeCharMap['r'] = MetaType(BWAPI::UnitTypes::Protoss_Reaver);
//   typeCharMap['o'] = MetaType(BWAPI::UnitTypes::Protoss_Observer);
//   typeCharMap['s'] = MetaType(BWAPI::UnitTypes::Protoss_Scout);
//   typeCharMap['l'] = MetaType(BWAPI::UpgradeTypes::Leg_Enhancements);
//   typeCharMap['v'] = MetaType(BWAPI::UpgradeTypes::Singularity_Charge);
//}

void ProductionManager::drawProductionInformation(int x, int y)
{
   // fill prod with each unit which is under construction
   std::vector<BWAPI::Unit *> prod;
   std::set<BWAPI::Unit*>::const_iterator it = BWAPI::Broodwar->self()->getUnits().begin();
   for (; it != BWAPI::Broodwar->self()->getUnits().end(); it++)
   {
      BWAPI::Unit * unit = *it;
      if (unit->isBeingConstructed())
      {
         prod.push_back(unit);
      }
   }
   
   // sort it based on the time it was started
   std::sort(prod.begin(), prod.end(), CompareWhenStarted());

   if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x, y, "\x04 Build Order Info:");
   if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04UNIT NAME");

   size_t reps = prod.size() < 10 ? prod.size() : 10;

   y += 40;
   int yy = y;

   // for each unit in the queue
   for (size_t i(0); i<reps; i++) {

      std::string prefix = "\x07";

      yy += 10;

      BWAPI::UnitType t = prod[i]->getType();

      if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x, yy, "%s%s", prefix.c_str(), t.getName().c_str());
      if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x+150, yy, "%s%6d", prefix.c_str(), prod[i]->getRemainingBuildTime());
   }

   queue.drawQueueInformation(x, yy+10);
}

ProductionManager & ProductionManager::Instance() {

   static ProductionManager instance;
   return instance;
}
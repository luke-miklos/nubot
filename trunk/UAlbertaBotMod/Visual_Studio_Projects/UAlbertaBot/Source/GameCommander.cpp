#include "Common.h"
#include "GameCommander.h"


GameCommander::GameCommander() : numWorkerScouts(0), currentScout(NULL)
{

}

void GameCommander::update()
{
	timerManager.startTimer(TimerManager::All);
	

	// economy and base managers
	timerManager.startTimer(TimerManager::Worker);
	// populate the unit vectors we will pass into various managers
	populateUnitVectors();
	WorkerManager::Instance().update();
	timerManager.stopTimer(TimerManager::Worker);

	timerManager.startTimer(TimerManager::Production);
	ProductionManager::Instance().update();
	timerManager.stopTimer(TimerManager::Production);

	timerManager.startTimer(TimerManager::Building);
	BuildingManager::Instance().update();
	timerManager.stopTimer(TimerManager::Building);

	// combat and scouting managers
	timerManager.startTimer(TimerManager::Combat);
	combatCommander.update(combatUnits);
	timerManager.stopTimer(TimerManager::Combat);

	timerManager.startTimer(TimerManager::Scout);
	scoutManager.update(scoutUnits);
	timerManager.stopTimer(TimerManager::Scout);

	// utility managers
	timerManager.startTimer(TimerManager::InformationManager);
	InformationManager::Instance().update();
	timerManager.stopTimer(TimerManager::InformationManager);

	timerManager.startTimer(TimerManager::MapGrid);
	MapGrid::Instance().update();
	timerManager.stopTimer(TimerManager::MapGrid);

	timerManager.startTimer(TimerManager::MapTools);
	MapTools::Instance().update();
	timerManager.stopTimer(TimerManager::MapTools);

	timerManager.startTimer(TimerManager::Search);
	StarcraftBuildOrderSearchManager::Instance().update(35 - timerManager.getTotalElapsed());
	timerManager.stopTimer(TimerManager::Search);
		
	timerManager.stopTimer(TimerManager::All);

	drawDebugInterface();
}

void GameCommander::drawDebugInterface()
{
	timerManager.displayTimers(490, 225);
	
	//StarcraftBuildOrderSearchManager::Instance().drawSearchInformation(10, 240);
	//BuildingManager::Instance().drawBuildingInformation(200,50);
	ProductionManager::Instance().drawProductionInformation(10, 30);
	InformationManager::Instance().drawUnitInformation(425,30);

	combatCommander.drawSquadInformation(200, 30);

	// draw position of mouse cursor
	if (Options::Debug::DRAW_UALBERTABOT_DEBUG)
	{
		BWAPI::Broodwar->drawTextScreen(20, 20, "Frame: %7d\nTime: %4dm %3ds", BWAPI::Broodwar->getFrameCount(), BWAPI::Broodwar->getFrameCount()/(24*60), (BWAPI::Broodwar->getFrameCount()/24)%60);
		int mouseX = BWAPI::Broodwar->getMousePosition().x() + BWAPI::Broodwar->getScreenPosition().x();
		int mouseY = BWAPI::Broodwar->getMousePosition().y() + BWAPI::Broodwar->getScreenPosition().y();
		BWAPI::Broodwar->drawTextMap(mouseX + 20, mouseY, " %d %d", mouseX, mouseY);
	}
}

// assigns units to various managers
void GameCommander::populateUnitVectors()
{
	// filter our units for those which are valid and usable
	setValidUnits();

	// set each type of unit
	setScoutUnits();
	setCombatUnits();
	setWorkerUnits();
}

// validates units as usable for distribution to various managers
void GameCommander::setValidUnits()
{
	validUnits.clear();

	// make sure the unit is completed and alive and usable
	BOOST_FOREACH(BWAPI::Unit * unit, BWAPI::Broodwar->self()->getUnits())
	{
		if (isValidUnit(unit))
		{	
			validUnits.push_back(UnitToAssign(unit));
		}
	}
}

// selects which units will be scouting
// currently only selects the worker scout after first pylon built
// this does NOT take that worker away from worker manager, but it still works
// TODO: take this worker away from worker manager in a clever way
void GameCommander::setScoutUnits()
{
	// add anything that can attack or heal to the combat force
	for (size_t i(0); i<validUnits.size(); ++i)
	{
		// if this unit is already assigned, skip it
		if (validUnits[i].isAssigned)
		{
			continue;
		}

		// if we have just built our first suply provider, set the worker to a scout
		if (numWorkerScouts == 0)
		{
			// get the first supply provider we come across in our units, this should be the first one we make
			BWAPI::Unit * supplyProvider = getFirstSupplyProvider();

			// if it exists
			if (supplyProvider)
			{
				// increase the number of worker scouts so this won't happen again
				numWorkerScouts++;

				// grab the closest worker to the supply provider to send to scout
				int workerScoutIndex = getClosestWorkerToTarget(supplyProvider->getPosition());

				// if we find a worker (which we should) add it to the scout vector
				if (workerScoutIndex != -1)
				{
					scoutUnits.push_back(validUnits[workerScoutIndex].unit);
					validUnits[workerScoutIndex].isAssigned = true;
					WorkerManager::Instance().setScoutWorker(validUnits[workerScoutIndex].unit);
				}
			}
		}
	}
}

// sets combat units to be passed to CombatCommander
void GameCommander::setCombatUnits()
{
	combatUnits.clear();

	// add anything that can attack or heal to the combat force
	for (size_t i(0); i<validUnits.size(); ++i)
	{
		// if this unit is already assigned, skip it
		if (validUnits[i].isAssigned)
		{
			continue;
		}

		if (isCombatUnit(validUnits[i].unit))		
		{	
			combatUnits.push_back(validUnits[i].unit);
			validUnits[i].isAssigned = true;
		}
	}

	// emergency situation, enemy is in our base and we have no combat units
	// add our workers to the combat force
	if (combatUnits.empty() && StrategyManager::Instance().defendWithWorkers())
	{
		// add anything that can attack or heal to the combat force
		for (size_t i(0); i<validUnits.size(); ++i)
		{
			// if this unit is already assigned, skip it
			if (validUnits[i].isAssigned)
			{
				continue;
			}

			// if it's a worker
			if (validUnits[i].unit->getType().isWorker())			
			{	
				// only assign it to combat if it's not building something
				if (!WorkerManager::Instance().isBuilder(validUnits[i].unit))
				{
					combatUnits.push_back(validUnits[i].unit);
					validUnits[i].isAssigned = true;
				}
			}
		}
	}
}

bool GameCommander::isCombatUnit(BWAPI::Unit * unit) const
{
	assert(unit != NULL);

	// no workers or buildings allowed
	if (unit && unit->getType().isWorker() || unit->getType().isBuilding())
	{
		return false;
	}

	// check for various types of combat units
	if (unit->getType().canAttack() || 
		unit->getType() == BWAPI::UnitTypes::Terran_Medic ||
		unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar ||
		unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
	{
		return true;
	}
		
	return false;
}

BWAPI::Unit * GameCommander::getFirstSupplyProvider()
{
	BWAPI::Unit * supplyProvider = NULL;

	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
	{
		BOOST_FOREACH(BWAPI::Unit * unit, BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Spawning_Pool)
			{
				supplyProvider = unit;
			}
		}
	}
	else
	{
		
		BOOST_FOREACH(BWAPI::Unit * unit, BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType() == BWAPI::Broodwar->self()->getRace().getSupplyProvider())
			{
				supplyProvider = unit;
			}
		}
	}

	return supplyProvider;
}

void GameCommander::setWorkerUnits()
{
	workerUnits.clear();

	// add all workers that have yet to be assigned
	for (size_t i(0); i<validUnits.size(); ++i)
	{
		// if this unit is already assigned, skip it
		if (validUnits[i].isAssigned)
		{
			continue;
		}

		if (validUnits[i].unit->getType().isWorker())			
		{	
			workerUnits.push_back(validUnits[i].unit);
			validUnits[i].isAssigned = true;
		}
	}

}

bool GameCommander::isValidUnit(BWAPI::Unit * unit)
{
	if (!unit)
	{
		return false;
	}

	if (unit->isCompleted() 
		&& unit->getHitPoints() > 0 
		&& unit->exists() 
		&& unit->getType() != BWAPI::UnitTypes::Unknown 
		&& unit->getPosition().x() != BWAPI::Positions::Unknown.x() 
		&& unit->getPosition().y() != BWAPI::Positions::Unknown.y()) 
	{
		return true;
	}
	else
	{
		return false;
	}
}

void GameCommander::onUnitShow(BWAPI::Unit * unit)			
{ 
	InformationManager::Instance().onUnitShow(unit); 
	WorkerManager::Instance().onUnitShow(unit);
}

void GameCommander::onUnitHide(BWAPI::Unit * unit)			
{ 
	InformationManager::Instance().onUnitHide(unit); 
}

void GameCommander::onUnitCreate(BWAPI::Unit * unit)		
{ 
	InformationManager::Instance().onUnitCreate(unit); 
	ProductionManager::Instance().onUnitCreate(unit);
}

void GameCommander::onUnitRenegade(BWAPI::Unit * unit)		
{ 
	InformationManager::Instance().onUnitRenegade(unit); 
}

void GameCommander::onUnitDestroy(BWAPI::Unit * unit)		
{ 	
	ProductionManager::Instance().onUnitDestroy(unit);
	WorkerManager::Instance().onUnitDestroy(unit);
	InformationManager::Instance().onUnitDestroy(unit); 
}

void GameCommander::onUnitMorph(BWAPI::Unit * unit)		
{ 
	InformationManager::Instance().onUnitMorph(unit);
	WorkerManager::Instance().onUnitMorph(unit);
}

void GameCommander::onSendText(std::string text)
{
	ProductionManager::Instance().onSendText(text);

	if (text.compare("0") == 0)
	{
		BWAPI::Broodwar->setLocalSpeed(0);
	}
	else if (atoi(text.c_str()) > 0)
	{
		BWAPI::Broodwar->setLocalSpeed(atoi(text.c_str()));
	}
}

int GameCommander::getClosestUnitToTarget(BWAPI::UnitType type, BWAPI::Position target)
{
	int closestUnitIndex = -1;
	double closestDist = 100000;

	for (size_t i(0); i < validUnits.size(); ++i)
	{
		if (validUnits[i].unit->getType() == type )
		{
			double dist = validUnits[i].unit->getDistance(target);
			if (dist < closestDist)
			{
				closestUnitIndex = i;
				closestDist = dist;
			}
		}
	}

	return closestUnitIndex;
}

int GameCommander::getClosestWorkerToTarget(BWAPI::Position target)
{
	int closestUnitIndex = -1;
	double closestDist = 100000;

	for (size_t i(0); i < validUnits.size(); ++i)
	{
		if (validUnits[i].unit->getType().isWorker() && WorkerManager::Instance().isFree(validUnits[i].unit))
		{
			double dist = validUnits[i].unit->getDistance(target);
			if (dist < closestDist)
			{
				closestUnitIndex = i;
				closestDist = dist;
			}
		}
	}

	return closestUnitIndex;
}
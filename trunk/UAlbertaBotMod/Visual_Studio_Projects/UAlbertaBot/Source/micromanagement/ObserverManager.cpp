#include "Common.h"
#include "ObserverManager.h"

ObserverManager::ObserverManager() : unitClosestToEnemy(NULL) { }

void ObserverManager::executeMicro(const UnitVector & targets) 
{
	const UnitVector & observers = getUnits();

	if (observers.empty())
	{
		return;
	}

	for (size_t i(0); i<targets.size(); ++i)
	{
		// do something here if there's targets
	}

	cloakedUnitMap.clear();
	UnitVector cloakedUnits;

	// figure out targets
   std::set<BWAPI::Unit*>::const_iterator it = BWAPI::Broodwar->enemy()->getUnits().begin();
   for (; it != BWAPI::Broodwar->enemy()->getUnits().end(); it++)
   {
      BWAPI::Unit * unit = *it;
		// conditions for targeting
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
			unit->getType() == BWAPI::UnitTypes::Terran_Wraith) 
		{
			cloakedUnits.push_back(unit);
			cloakedUnitMap[unit] = false;
		}
	}

	bool observerInBattle = false;

	// for each observer
   UnitVector::const_iterator oit = observers.begin();
   for (; oit != observers.end(); oit++)
   {
      BWAPI::Unit * observer = *oit;

      // if we need to regroup, move the observer to that location
		if (!observerInBattle && unitClosestToEnemy && unitClosestToEnemy->getPosition().isValid())
		{
			smartMove(observer, unitClosestToEnemy->getPosition());
			observerInBattle = true;
		}
		// otherwise there is no battle or no closest to enemy so we don't want our observer to die
		// send him to scout around the map
		else
		{
			BWAPI::Position explorePosition = MapGrid::Instance().getLeastExplored();
			smartMove(observer, explorePosition);
		}
	}
}


BWAPI::Unit * ObserverManager::closestCloakedUnit(const UnitVector & cloakedUnits, BWAPI::Unit * observer)
{
	BWAPI::Unit * closestCloaked = NULL;
	double closestDist = 100000;

   UnitVector::const_iterator it = cloakedUnits.begin();
   for (; it != cloakedUnits.end(); it++)
   {
      BWAPI::Unit * unit = *it;

      // if we haven't already assigned an observer to this cloaked unit
		if (!cloakedUnitMap[unit])
		{
			double dist = unit->getDistance(observer);

			if (!closestCloaked || (dist < closestDist))
			{
				closestCloaked = unit;
				closestDist = dist;
			}
		}
	}

	return closestCloaked;
}
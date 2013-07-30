#include "Common.h"
#include "DetectorManager.h"

DetectorManager::DetectorManager() : unitClosestToEnemy(NULL) { }

void DetectorManager::executeMicro(const UnitVector & targets) 
{
	const UnitVector & detectorUnits = getUnits();

	if (detectorUnits.empty())
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
      BWAPI::Unit* unit = *it;
		// conditions for targeting
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
			unit->getType() == BWAPI::UnitTypes::Terran_Wraith) 
		{
			cloakedUnits.push_back(unit);
			cloakedUnitMap[unit] = false;
		}
	}

	bool detectorUnitInBattle = false;

	// for each detectorUnit
   UnitVector::const_iterator dit = detectorUnits.begin();
   for (; dit != detectorUnits.end(); dit++)
   {
      BWAPI::Unit * detectorUnit = *dit;
		// if we need to regroup, move the detectorUnit to that location
		if (!detectorUnitInBattle && unitClosestToEnemy && unitClosestToEnemy->getPosition().isValid())
		{
			smartMove(detectorUnit, unitClosestToEnemy->getPosition());
			detectorUnitInBattle = true;
		}
		// otherwise there is no battle or no closest to enemy so we don't want our detectorUnit to die
		// send him to scout around the map
		else
		{
			BWAPI::Position explorePosition = MapGrid::Instance().getLeastExplored();
			smartMove(detectorUnit, explorePosition);
		}
	}
}


BWAPI::Unit * DetectorManager::closestCloakedUnit(const UnitVector & cloakedUnits, BWAPI::Unit * detectorUnit)
{
	BWAPI::Unit * closestCloaked = NULL;
	double closestDist = 100000;

   UnitVector::const_iterator it = cloakedUnits.begin();
   for (; it != cloakedUnits.end(); it++)
   {
      BWAPI::Unit * unit = *it;
		// if we haven't already assigned an detectorUnit to this cloaked unit
		if (!cloakedUnitMap[unit])
		{
			double dist = unit->getDistance(detectorUnit);

			if (!closestCloaked || (dist < closestDist))
			{
				closestCloaked = unit;
				closestDist = dist;
			}
		}
	}

	return closestCloaked;
}
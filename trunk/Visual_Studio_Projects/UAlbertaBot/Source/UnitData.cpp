#include "Common.h"
#include "UnitData.h"

UnitData::UnitData() 
	: mineralsLost(0)
	, gasLost(0)
	, hasCloakedUnit(false)
	, hasDetector(false)
{
	int maxTypeID(0);
	BOOST_FOREACH (BWAPI::UnitType t, BWAPI::UnitTypes::allUnitTypes())
	{
		maxTypeID = maxTypeID > t.getID() ? maxTypeID : t.getID();
	}

	numDeadUnits	= std::vector<int>(maxTypeID + 1, 0);
	numUnits		= std::vector<int>(maxTypeID + 1, 0);
}

void UnitData::updateUnit(BWAPI::Unit * unit)
{
	if (!unit) { return; }
	
	// if the unit exists, update it
	if (unitMap.find(unit) != unitMap.end())
	{
		UnitInfo & ui = unitMap.find(unit)->second;

		ui.lastPosition = unit->getPosition();
		ui.lastHealth = unit->getHitPoints() + unit->getShields();
		ui.unitID = unit->getID();
		ui.type = unit->getType();

		return;
	}
	// otherwise create it
	else
	{
		numUnits[unit->getType().getID()]++;
		unitMap[unit] = UnitInfo(unit->getID(), unit, unit->getPosition(), unit->getType());
	
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar
			|| unit->getType() == BWAPI::UnitTypes::Terran_Wraith
			|| unit->getType() == BWAPI::UnitTypes::Zerg_Lurker
			|| unit->getType() == BWAPI::UnitTypes::Terran_Ghost)
		{
			hasCloakedUnit = true;
		}

		if (unit->getType().isDetector())
		{
			hasDetector = true;
		}
	}
}

void UnitData::removeUnit(BWAPI::Unit * unit)
{
	if (!unit) { return; }

	mineralsLost += unit->getType().mineralPrice();
	gasLost += unit->getType().gasPrice();
	numUnits[unit->getType().getID()]--;
	numDeadUnits[unit->getType().getID()]++;
		
	unitMap.erase(unit);
}

// removes bad units from the vector
// can only remove one unit per call
void UnitData::removeBadUnits()
{
	for (UIMapIter iter(unitMap.begin()); iter != unitMap.end();)
	{
		if (badUnitInfo(iter->second))
		{
			numUnits[iter->second.type.getID()]--;
			iter = unitMap.erase(iter);
		}
		else
		{
			iter++;
		}
	}
}

const bool UnitData::badUnitInfo(const UnitInfo & ui) const
{
	// Cull away any refineries/assimilators/extractors that were destroyed and reverted to vespene geysers
	if(ui.unit && ui.unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
	{
		return true;
	}

	// If the unit is a building and we can currently see its position
	if(ui.type.isBuilding() && BWAPI::Broodwar->isVisible(ui.lastPosition.x()/32, ui.lastPosition.y()/32) && !ui.unit->isVisible())
	{
		return true;
	}

	return false;
}

void UnitData::getCloakedUnits(std::vector<UnitInfo> & v) const 
{
	FOR_EACH_UIMAP_CONST(iter, unitMap)
	{
		const UnitInfo & ui(iter->second);

		if (ui.canCloak())
		{
			v.push_back(ui);
		}
	}
}

void UnitData::getDetectorUnits(std::vector<UnitInfo> & v) const 
{
	FOR_EACH_UIMAP_CONST(iter, unitMap)
	{
		const UnitInfo & ui(iter->second);

		if (ui.isDetector())
		{
			v.push_back(ui);
		}
	}
}

void UnitData::getFlyingUnits(std::vector<UnitInfo> & v) const 
{
	FOR_EACH_UIMAP_CONST(iter, unitMap)
	{
		const UnitInfo & ui(iter->second);

		if (ui.isFlyer())
		{
			v.push_back(ui);
		}
	}
}
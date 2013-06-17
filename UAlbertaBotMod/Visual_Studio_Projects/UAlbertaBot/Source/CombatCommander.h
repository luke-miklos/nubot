#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"
#include "InformationManager.h"
#include "StrategyManager.h"

class CombatCommander
{
	SquadData			squadData;

	bool attacking;
	bool foundEnemy;

	bool attackSent;
	
	int selfTotalDeadUnits;
	int numUnitsNeededForAttack;

	std::string currentAction;

	BWAPI::Position enemyExpansion;

	void	assignDefenders(std::set<BWAPI::Unit *> & unitsToAssign);
	void	assignAttackRegion(BWTA::Region * enemyRegion, SquadOrder & order);
	void	assignAttackVisibleUnits(SquadOrder & order);
	void	assignAttackKnownBuildings(SquadOrder & order);
	void	assignAttackExplore(SquadOrder & order);

	bool	isBuildingAtBaselocation(BWTA::BaseLocation * baseLocation);
	bool	squadUpdateFrame();

	int		getNumType(UnitVector & units, BWAPI::UnitType type);

	BWAPI::Unit * findClosestDefender(std::set<BWAPI::Unit *> & enemyUnitsInRegion, const std::set<BWAPI::Unit *> & units);

	BWTA::Region * getClosestEnemyRegion();


	BWAPI::Position getDefendLocation();

public:

	void update(const UnitVector & allCombatUnits);

	CombatCommander();

	bool enemyInOurBase();

	void onRemoveUnit(BWAPI::Unit * unit);
	void onUnitShow(BWAPI::Unit * unit);
	void onUnitCreate(BWAPI::Unit * unit);
	void onUnitHide(BWAPI::Unit * unit);
	void onUnitMorph(BWAPI::Unit * unit);
	void onUnitRenegade(BWAPI::Unit * unit);
	void handleEvent(int eventType);

	void drawSquadInformation(int x, int y);
};
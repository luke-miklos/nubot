#pragma once

#include "Common.h"
#include "micromanagement/MeleeManager.h"
#include "micromanagement/RangedManager.h"
#include "micromanagement/DetectorManager.h"
#include "micromanagement/TransportManager.h"
#include "SquadOrder.h"
#include "DistanceMap.hpp"
#include "StrategyManager.h"
#include "CombatSimulation.h"

class ZealotManager;
class DarkTemplarManager;
class DragoonManager;
class ObserverManager;

class MeleeManager;
class RangedManager;
class DetectorManager;

class Squad
{
	UnitVector			units;

	void				updateUnits();
	DistanceMap			distanceMap;

	std::string			regroupStatus;

	int					lastFrameRegroup;

	std::map<BWAPI::Unit *, bool> nearEnemy;

	bool				squadObserverNear(BWAPI::Position p);

public:

	int					lastRegroup;
	SquadOrder			order;

	MeleeManager		meleeManager;
	RangedManager		rangedManager;
	DetectorManager		detectorManager;
	TransportManager	transportManager;

						Squad(const UnitVector & units, SquadOrder order);
						Squad() {}
						~Squad() {}

	BWAPI::Position		calcCenter();
	BWAPI::Position		calcRegroupPosition();

	void				clear();
	void				update();

	void				setManagerUnits();
	void				setNearEnemyUnits();
	void				setAllUnits();
	
	void				setUnits(const UnitVector & u)	{ units = u; }
	const UnitVector &	getUnits() const				{ return units; } 
	const SquadOrder &	getSquadOrder()	const			{ return order; }
	bool				unitNearEnemy(BWAPI::Unit * unit);
	bool				needsToRegroup();
	BWAPI::Unit *		getRegroupUnit();
	int					squadUnitsNear(BWAPI::Position p);

	BWAPI::Unit *		unitClosestToEnemy();

	void				setSquadOrder(const SquadOrder & so);
};

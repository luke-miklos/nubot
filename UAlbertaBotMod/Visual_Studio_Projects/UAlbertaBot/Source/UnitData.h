#pragma once

#include "Common.h"
#include "BWTA.h"

struct UnitInfo
{
	// we need to store all of this data because if the unit is not visible, we
	// can't reference it from the unit pointer

	int					unitID;
	int					lastHealth;
	BWAPI::Unit *		unit;
	BWAPI::Position		lastPosition;
	BWAPI::UnitType		type;

	bool canCloak() const
	{
		return type == BWAPI::UnitTypes::Protoss_Dark_Templar
			|| type == BWAPI::UnitTypes::Terran_Wraith
			|| type == BWAPI::UnitTypes::Zerg_Lurker;
	}

	bool isFlyer() const
	{
		return type.isFlyer();
	}

	bool isDetector() const
	{
		return type.isDetector();
	}

	UnitInfo()
		: unitID(0)
		, lastHealth(0)
		, unit(NULL)
		, lastPosition(BWAPI::Positions::None)
		, type(BWAPI::UnitTypes::None)
	{

	}

	UnitInfo(int id, BWAPI::Unit * u, BWAPI::Position last, BWAPI::UnitType t) 
	{
		unitID = id;
		unit = u;
		lastHealth = u->getHitPoints() + u->getShields();
		lastPosition = last;
		type = t;
	}

	const bool operator == (BWAPI::Unit * unit)
	{
		return unitID == unit->getID();
	}

	const bool operator == (const UnitInfo & rhs)
	{
		return (unitID == rhs.unitID);
	}

	const bool operator < (const UnitInfo & rhs)
	{
		return (unitID < rhs.unitID);
	}
};

typedef std::vector<UnitInfo> UnitInfoVector;
typedef std::map<BWAPI::Unit *, UnitInfo> UIMap;
typedef UIMap::iterator UIMapIter;
typedef UIMap::const_iterator ConstUIMapIter;
#define FOR_EACH_UIMAP(ITER, MAP) for (UIMapIter ITER(MAP.begin()); ITER != MAP.end(); ITER++)
#define FOR_EACH_UIMAP_CONST(ITER, MAP) for (ConstUIMapIter ITER(MAP.begin()); ITER != MAP.end(); ITER++)

class UnitData {

	std::map<BWAPI::Unit *, UnitInfo>		unitMap;

	std::vector<int>						numDeadUnits;
	std::vector<int>						numUnits;

	int										mineralsLost;
	int										gasLost;

	bool									hasCloakedUnit;
	bool									hasDetector;

	const bool badUnitInfo(const UnitInfo & ui) const;

public:

	UnitData();
	~UnitData() {}

	void	updateUnit(BWAPI::Unit * unit);
	void	removeUnit(BWAPI::Unit * unit);
	void	removeBadUnits();

	void	getCloakedUnits(UnitInfoVector & v)				const;
	void	getDetectorUnits(UnitInfoVector & v)			const;
	void	getFlyingUnits(UnitInfoVector & v)				const;

	int		getGasLost()									const	{ return gasLost; }
	int		getMineralsLost()								const	{ return mineralsLost; }
	int		getNumUnits(BWAPI::UnitType t)					const	{ return numUnits[t.getID()]; }
	int		getNumDeadUnits(BWAPI::UnitType t)				const	{ return numDeadUnits[t.getID()]; }
	bool	hasCloakedUnits()								const	{ return hasCloakedUnit || numUnits[BWAPI::UnitTypes::Protoss_Citadel_of_Adun] > 0; }
	bool	hasDetectorUnits()								const	{ return hasDetector; }
	const	std::map<BWAPI::Unit *, UnitInfo> & getUnits()	const	{ return unitMap; }
};
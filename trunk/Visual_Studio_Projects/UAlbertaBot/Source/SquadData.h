#pragma once

#include "Common.h"
#include "Squad.h"

class SquadData
{
	// All squads. Indexed by SquadOrder enum
	std::vector<Squad>	squads;

	void				updateAllSquads();
	int					getNumType(const UnitVector & units, BWAPI::UnitType type);

public:

	SquadData();
	~SquadData() {}

	void				clearSquadData();

	void				addSquad(Squad & squad);
	void				drawSquadInformation(int x, int y);

	void				update();
	void				setSquad(const Squad & squad);
	void				setRegroup();
};

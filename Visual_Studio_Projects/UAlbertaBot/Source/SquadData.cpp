#include "Common.h"
#include "SquadData.h"

SquadData::SquadData() : squads(std::vector<Squad>((int)SquadOrder::SquadOrderTypes))
{
	
}

void SquadData::update()
{
	// send the update command to all squads
	updateAllSquads();
}

void SquadData::clearSquadData()
{
	BOOST_FOREACH(Squad & squad, squads)
	{
		squad.clear();
	}
}

void SquadData::addSquad(Squad & squad)
{
	squads.push_back(squad);
}

void SquadData::updateAllSquads()
{
	BOOST_FOREACH (Squad & squad, squads)
	{
		squad.update();
	}
}

void SquadData::setSquad(const Squad & squad)
{
	int index = (int)squad.getSquadOrder().type;

	squads[index].setUnits(squad.getUnits());
	squads[index].setSquadOrder(squad.getSquadOrder());
}

void SquadData::drawSquadInformation(int x, int y) {

	BWAPI::UnitType types[6] = {
		BWAPI::UnitTypes::Zerg_Mutalisk, 
		BWAPI::UnitTypes::Zerg_Hydralisk, 
		BWAPI::UnitTypes::Zerg_Zergling,
		BWAPI::UnitTypes::Zerg_Scourge, 
		BWAPI::UnitTypes::Zerg_Drone, 
		BWAPI::UnitTypes::Zerg_Overlord
	};

	if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x, y, "\x04Squads");
	if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04ORDER");
	if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x+100, y+20, "\x04SIZE");
	if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x+150, y+20, "\x04LOCATION");

	int yspace = 0;

	// for each unit in the queue
	for (size_t i(0); i<squads.size(); i++) {

		const UnitVector & units = squads[i].getUnits();
		const SquadOrder & order = squads[i].getSquadOrder();

		if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x, y+40+((yspace)*10), "\x03%s", order.getOrderString().c_str());
		if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x+100, y+40+((yspace)*10), "\x03%d", units.size());
		if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x+150, y+40+((yspace++)*10), "\x03(%d,%d)", order.position.x(), order.position.y());

		if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawCircleMap(order.position.x(), order.position.y(), 10, BWAPI::Colors::Green, true);
		if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawCircleMap(order.position.x(), order.position.y(), 400, BWAPI::Colors::Green, false);
		if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawCircleMap(order.position.x(), order.position.y(), 100, BWAPI::Colors::Green, false);

		for (int j=0; j<6; j++) {

			int num = getNumType(units, types[j]);
			if (num > 0) {
				if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x+20, y+40+((yspace)*10), "\x04%s", types[j].getName().c_str());
				if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextScreen(x+100, y+40+((yspace++)*10), "\x04%d", num);
			}
		}
	}
}

int SquadData::getNumType(const UnitVector & units, BWAPI::UnitType type) 
{
	int sum = 0;
	for (size_t i(0); i<units.size(); i++) 
	{
		if (units[i]->getType() == type) 
		{
			sum++;
		}
	}

	return sum;
}
/* 
 +----------------------------------------------------------------------+
 | UAlbertaBot                                                          |
 +----------------------------------------------------------------------+
 | University of Alberta - AIIDE StarCraft Competition                  |
 +----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------+
 | Author: David Churchill <dave.churchill@gmail.com>                   |
 +----------------------------------------------------------------------+
*/

#include "Common.h"
#include "UAlbertaBotModule.h"


BWAPI::AIModule * __NewAIModule()
{
	return new UAlbertaBotModule();
}

UAlbertaBotModule::UAlbertaBotModule()  {}
UAlbertaBotModule::~UAlbertaBotModule() {}

void UAlbertaBotModule::onStart()
{
	BWAPI::Broodwar->setLocalSpeed(0);

	BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);
	//BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);

	if (Options::Modules::USING_GAMECOMMANDER)
	{
		BWTA::readMap();
		BWTA::analyze();
	}
	
	if (Options::Modules::USING_MICRO_SEARCH)
	{
		Search::StarcraftData::init();
		MicroSearch::Hash::initHash();
		micro.onStart();
	}
}

void UAlbertaBotModule::onEnd(bool isWinner) 
{
	std::stringstream result;
	std::string win = isWinner ? "win" : "lose";

	result << "Game against " << BWAPI::Broodwar->enemy()->getName() << " " << win << " with strategy " << StrategyManager::Instance().getCurrentStrategy() << "\n";

	Logger::Instance().log(result.str());

	StrategyManager::Instance().onEnd(isWinner);
}

void UAlbertaBotModule::onFrame()
{
	if (Options::Modules::USING_GAMECOMMANDER) 
	{ 
		gameCommander.update(); 
	}
	
	if (Options::Modules::USING_ENHANCED_INTERFACE)
	{
		eui.update();
	}

	if (Options::Modules::USING_MICRO_SEARCH)
	{
		micro.update();
	}

	//Visualizer::Instance().setBWAPIState();
	//Visualizer::Instance().onFrame();
}

void UAlbertaBotModule::onUnitDestroy(BWAPI::Unit * unit)
{
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitDestroy(unit); }
	if (Options::Modules::USING_ENHANCED_INTERFACE) { eui.onUnitDestroy(unit); }
}

void UAlbertaBotModule::onUnitMorph(BWAPI::Unit * unit)
{
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitMorph(unit); }
}

void UAlbertaBotModule::onSendText(std::string text) 
{ 
	BWAPI::Broodwar->sendText(text.c_str());
	BWAPI::Broodwar->setLocalSpeed(atoi(text.c_str()));
}

void UAlbertaBotModule::onUnitCreate(BWAPI::Unit * unit)
{ 
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitCreate(unit); }
}

void UAlbertaBotModule::onUnitShow(BWAPI::Unit * unit)
{ 
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitShow(unit); }
}

void UAlbertaBotModule::onUnitHide(BWAPI::Unit * unit)
{ 
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitHide(unit); }
}

void UAlbertaBotModule::onUnitRenegade(BWAPI::Unit * unit)
{ 
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitRenegade(unit); }
}

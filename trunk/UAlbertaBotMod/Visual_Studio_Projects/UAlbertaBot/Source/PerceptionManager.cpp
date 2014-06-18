

#include "PerceptionManager.h"
#include "PerceivedUnit.h"

//static
PerceptionManager* PerceptionManager::mInstancePtr = 0;


//static
PerceptionManager& PerceptionManager::GetInstance()
{
   if (mInstancePtr == 0)
   {
      mInstancePtr = new PerceptionManager();
   }
   return *mInstancePtr;
}


void PerceptionManager::Update(int currentGameFrame, BWAPI::Player* aPlayer)
{
   const std::set<BWAPI::Unit*>& units = getPlayerUnits(aPlayer);
   for(std::set<BWAPI::Unit*>::const_iterator cit = units.begin(); cit != units.end(); cit++)
   {
      PerceivedUnit* unit = (PerceivedUnit*)*cit;
      unit->Update(currentGameFrame);
   }
}


//virtual
void PerceptionManager::onUnitShow(BWAPI::Unit* unit)
{
   if (mUnitMap.find(unit->getID()) == mUnitMap.end())
   {
      //new unit, create a perception & add it
      BWAPI::Unit* newPerception = new PerceivedUnit(unit);
      mUnitPerceptions.insert(newPerception);
      mUnitMap[unit->getID()] = newPerception;
      mPlayerUnits[unit->getPlayer()].insert(newPerception);
    }
}
//virtual void onUnitCreate(BWAPI::Unit* unit);
//virtual void onUnitMorph(BWAPI::Unit* unit);


//virtual 
void PerceptionManager::onUnitDestroy(BWAPI::Unit * unit)
{
   //placeholder, what else?
   BWAPI::Unit* temp = mUnitMap[unit->getID()];
   mUnitPerceptions.erase(temp);
   mPlayerUnits[unit->getPlayer()].erase(temp);
   mUnitMap.erase(unit->getID());
}


//virtual
const std::set<BWAPI::Unit*>& PerceptionManager::getUnits() const
{
   return mUnitPerceptions;
}


//virtual
const std::set<BWAPI::Unit*>& PerceptionManager::getPlayerUnits(BWAPI::Player* aPlayer)
{
   std::map<BWAPI::Player*, std::set<BWAPI::Unit*> >::iterator it = mPlayerUnits.find(aPlayer);
   if (it != mPlayerUnits.end())
   {
      return it->second;
   }
   return std::set<BWAPI::Unit*>();
}


//virtual
PerceivedUnit* PerceptionManager::getPerceivedUnit(BWAPI::Unit* aUnit)
{
   std::map<int, BWAPI::Unit*>::iterator it = mUnitMap.find(aUnit->getID());
   if (it != mUnitMap.end())
   {
      return (PerceivedUnit*)it->second;
   }
   else
   {
      //add one
      BWAPI::Unit* newPerception = new PerceivedUnit(aUnit);
      mUnitPerceptions.insert(newPerception);
      mUnitMap[aUnit->getID()] = newPerception;
      mPlayerUnits[aUnit->getPlayer()].insert(newPerception);
      return (PerceivedUnit*)newPerception;
   }
}


PerceptionManager::PerceptionManager()
   : mUnitPerceptions()
   , mPlayerUnits()
   , mUnitMap()
{

}

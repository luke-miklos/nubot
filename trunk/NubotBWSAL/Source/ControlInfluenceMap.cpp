
#include "ControlInfluenceMap.h"

ControlInfluenceMap::ControlInfluenceMap(int mapWidth, int mapHeight)
   : InfluenceMap(mapWidth, mapHeight)
{
}

ControlInfluenceMap::~ControlInfluenceMap()
{
}

void ControlInfluenceMap::UpdateInfluence()
{
   ClearMap();

   std::set< BWAPI::Unit* > units = BWAPI::Broodwar->self()->getUnits();
   for (std::set< BWAPI::Unit* >::iterator it = units.begin(); it != units.end(); it++)
   {
      BWAPI::Unit* unit = *it;
      if (unit->exists())
      {
         // Use pixel position here as it is more accurate, gives unit center
         BWAPI::Position pixelPos = unit->getPosition();
         // Set the influence for the unit based on unis sight range
         SetInfluence(pixelPos, unit->getType().sightRange(), 1.0);
      }
   } // End my units

   std::set< BWAPI::Unit* > enemyUnits = BWAPI::Broodwar->enemy()->getUnits();
   for (std::set< BWAPI::Unit* >::iterator it = enemyUnits.begin(); it != enemyUnits.end(); it++)
   {
      BWAPI::Unit* unit = *it;
      if (unit->exists())
      {
         // Use pixel position here as it is more accurate, gives unit center
         BWAPI::Position pixelPos = unit->getPosition();
         // Set the influence for the unit based on unis sight range
         SetInfluence(pixelPos, unit->getType().sightRange(), -1.0);
      }
   } // End enemy units
}
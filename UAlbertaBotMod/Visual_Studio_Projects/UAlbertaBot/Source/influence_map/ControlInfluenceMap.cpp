
#include "ControlInfluenceMap.h"

ControlInfluenceMap::ControlInfluenceMap(int mapWidth, int mapHeight)
   : InfluenceMap(mapWidth, mapHeight, 2)
{
}

ControlInfluenceMap::~ControlInfluenceMap()
{
}

void ControlInfluenceMap::UpdateInfluence(UnitData* myUnits, UnitData* enemyUnits)
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

   std::set< BWAPI::Unit* > enemies = BWAPI::Broodwar->enemy()->getUnits();
   for (std::set< BWAPI::Unit* >::iterator it = enemies.begin(); it != enemies.end(); it++)
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

   /* This code doesn't work
      Many artifacts from starting/creating/hiding units

   std::map<BWAPI::Unit *, UnitInfo> units = myUnits->getUnits();
   for (std::map<BWAPI::Unit *, UnitInfo>::iterator it = units.begin(); it != units.end(); it++)
   {
      UnitInfo ui = it->second;
      // Need to check 3 conditions for setting influence
      // 1. previousPosition = None - new unit, stamp initial influence
      // 2. lastPosition != previousPosition - unit moved, unstamp previous location, stamp new location 
      // 3. lastPosition = previousPosition - unit didn't move, influence already stamped, do nothing
      if (ui.previousPosition == BWAPI::Positions::None)
      {
         // Use pixel position here as it is more accurate, gives unit center
         // Set the initial influence for the unit based on unit's sight range
         SetInfluence(ui.lastPosition, ui.type.sightRange(), 1.0);
      }
      else if (ui.lastPosition.x()/TILE_SIZE != ui.previousPosition.x()/TILE_SIZE ||
               ui.lastPosition.y()/TILE_SIZE != ui.previousPosition.y()/TILE_SIZE) // Need to guard against None/Invalid/Unknown?
      {
         // Remove the influence from the previous last seen position
         SetInfluence(ui.previousPosition, ui.type.sightRange(), -1.0);
         // Set the influence for the new last seen position
         SetInfluence(ui.lastPosition, ui.type.sightRange(), 1.0);
      }
      // else we don't need to update influence
   } // End my units

   // Enemy units work the same way, just reverse the positive/negative values
   std::map<BWAPI::Unit *, UnitInfo> enemies = enemyUnits->getUnits();
   for (std::map<BWAPI::Unit *, UnitInfo>::iterator it = enemies.begin(); it != enemies.end(); it++)
   {
      UnitInfo ui = it->second;
      if (ui.previousPosition == BWAPI::Positions::None)
      {
         BWAPI::Position pixelPos = ui.lastPosition;
         SetInfluence(pixelPos, ui.type.sightRange(), -1.0);
      }
      else if (ui.lastPosition.x()/TILE_SIZE != ui.previousPosition.x()/TILE_SIZE ||
               ui.lastPosition.y()/TILE_SIZE != ui.previousPosition.y()/TILE_SIZE)
      {
         BWAPI::Position pixelPos = ui.previousPosition;
         SetInfluence(pixelPos, ui.type.sightRange(), 1.0);

         pixelPos = ui.lastPosition;
         SetInfluence(pixelPos, ui.type.sightRange(), -1.0);
      }
   } // End enemy units
   */
}
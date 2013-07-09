
#pragma once
#include "InfluenceMap.h"

// Defines a "control" influence map that apoximates what areas
// on thte map are controlled by which side. Influence values 
// set as 1.0 for each tile that is in a unit's vision range.

class ControlInfluenceMap : public InfluenceMap
{
public:

   // Create a new control influence map
   // mapWidth - map width in build tiles
   // mapHeight - map height in build tiles
   ControlInfluenceMap(int mapWidth, int mapHeight);
   ~ControlInfluenceMap();

   // Store influence of unit vision range based on tile location
   virtual void UpdateInfluence();
};


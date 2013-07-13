#pragma once

#include "Common.h"
#include "BWAPI.h"

class SquadOrder
{
public:

   enum { None, Attack, Defend, Regroup, SquadOrderTypes };

   int                  type;
   BWAPI::Position      position;
 //BWAPI::TilePosition  position;
   int                  radius;
   std::string          status;

   SquadOrder() 
      : type(None)
   {
   }

   SquadOrder(int type, BWAPI::Position position, int radius, std::string stat = "Default") 
 //SquadOrder(int type, BWAPI::TilePosition position, int radius, std::string stat = "Default") 
      : type(type)
      , position(position)
      , radius(radius) 
      , status(stat)
   {
   }

   std::string getStatus() const 
   {
      return status;
   }

   BWAPI::Position getPosition()
   {
      return BWAPI::Position(position);
   }
};

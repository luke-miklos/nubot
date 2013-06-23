

#ifndef FLOWFIELD_HPP
#define FLOWFIELD_HPP

#include "BWAPI.h"
#include <Common.h>

#include <vector>
#include <map>


//BWAPI::Position is always in units of pixels.
//Position (0,0) is top left corner of map; Position (c,r) is lower right corner of map.

//this flow field uses walkable tiles, which are always 8x8 pixels large
//the flow field stores angles as chars, for space


//conversions:
//    pixel          -> walkable tile: /8
//    pixel          -> build tile:    /32
//    walkable tile  -> pixel:         *8
//    walkable tile  -> build tile:    /4
//    build tile     -> pixel:         *32
//    build tile     -> walkable tile: *4

class FlowField
{

public:

   static FlowField* Instance();

   void DrawFlowFieldOnMap(BWAPI::TilePosition pos);
   void DrawFloodFillOnMap(BWAPI::TilePosition pos);
   void DrawWalkable();

   //returns false if flow field not found for tgt location, or if tgt is not reachable
   bool GetFlowFromTo(BWAPI::Position pos, BWAPI::TilePosition tgt, double& angle);

private:

   typedef struct TargetField
   {
      TargetField()
      {
         int size = BWAPI::Broodwar->mapHeight() * BWAPI::Broodwar->mapWidth();
         cells  = std::vector<char>(size, 0);
         moveTo = std::vector<char>(size, 0);
      }
      TargetField(int size)
      {
         cells  = std::vector<char>(size, 0);
         moveTo = std::vector<char>(size, 0);
      }

      //these accessor use 'cells'
      void   setAngle(int i, double angle);
      double getAngle(int i);
      bool   isMovable(int i);
      //these use 'moveTo'
      bool   unExpanded(int i)             { return ((i != -1) && (moveTo[i] == 0)); };
      char   direction(int i)              { return moveTo[i]; }

      std::vector<char> cells;      //stores char version of the angle
      std::vector<char> moveTo;     //stores a U, D, L, or R char for move direction 
   };

   void Initialize();
   inline int index(int col, int row)     { return row * cols + col; }  // return the index of the 1D array from (row,col)

   bool isWalkable(int i)                 { return walkable[i];          }
   bool isWalkable(int c, int r)          { return walkable[index(c,r)]; }
   void setWalkable(int c, int r, bool w) { walkable[index(c,r)] = w;    }

   bool isPathable(int c1, int r1, int c2, int r2);
   
   void resetFringe() { std::fill(fringe.begin(), fringe.end(), 0); }   // reset the fringe

   TargetField* GenerateFields(BWAPI::TilePosition pos);

   FlowField(int r, int c);   //in walkable tiles
   static FlowField* mInstance;

   std::map<BWAPI::TilePosition, TargetField*> mFields; //one field for every possible starting location

   int rows, cols;
   std::vector<bool> walkable;   // values are 1 or 0 for walkable or not walkable
   std::vector<int>  fringe;     // the fringe vector which is used as a sort of 'open list'
};


#endif

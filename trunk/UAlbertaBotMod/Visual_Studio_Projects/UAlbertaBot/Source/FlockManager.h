

#ifndef FLOCKMANAGER_H
#define FLOCKMANAGER_H

#include "micromanagement/MicroManager.h"
#include "Common.h"




/* ****************************************************************************
   Terran infantry appear to have "four" speeds, based on the distance to 
   the commanded move-to target position.
   target = 0-4  pixels away -> ~1/4 speed
   target = 5-8  pixels away -> ~2/4 speed
   target = 9-12 pixels away -> ~3/4 speed
   target = 13+  pixels away -> full speed
**************************************************************************** */


class FlockManager : public MicroManager
{
   typedef struct Neighbors
   {
      Neighbors() : forward(0), back(0), left(0), right(0) {}
      BWAPI::Position CenterOfMass();
      BWAPI::Unit* forward;
      BWAPI::Unit* back;
      BWAPI::Unit* left;
      BWAPI::Unit* right;
   } Neighbors;

   void InitializeFlock();

   //flow field is scale 1.0, all others are relative to it
   float separationScale;
   float alignmentScale;
   float cohesionScale;

   int round(double val) { return int((val>0.0)?(val+0.5):(val-0.5)); }

   class SortedUnit
   {
   public:
      SortedUnit(BWAPI::Unit* up) : unitPtr(up), key(0.0f) { }
      BWAPI::Unit* unitPtr;
      float key;  //distance alone some orthogonal line
      bool operator < (SortedUnit& rhs) { return key < rhs.key; }
   };

   std::map<BWAPI::Unit*,int>            mOutOfRangeMap;
   std::map<BWAPI::Unit*,int>::iterator  mRit;

public:

   FlockManager() : separationScale(0.25f), alignmentScale(0.25f), cohesionScale(0.25f), drawLines(true) {};
	~FlockManager() {}
	virtual void executeMicro(const UnitVector & targets);

	//BWAPI::Unit * chooseTarget(BWAPI::Unit * meleeUnit, const UnitVector & targets, std::map<BWAPI::Unit *, int> & numTargeting);
	//BWAPI::Unit * closestMeleeUnit(BWAPI::Unit * target, std::set<BWAPI::Unit *> & meleeUnitToAssign);
	//int getAttackPriority(BWAPI::Unit * unit);
	//BWAPI::Unit * getTarget(BWAPI::Unit * meleeUnit, UnitVector & targets);

   std::vector<std::vector<SortedUnit> > mFlockRows;   //index 0 = front
   std::vector<std::vector<Neighbors> >  mNeighbors;
   std::map<BWAPI::Unit*, int>           mDistMatrixIndexMap;

   std::vector<BWAPI::Unit*>             mCluster;

   void ClearOutOfRange() { mOutOfRangeMap.clear(); }
   void SetOutOfRange(BWAPI::Unit* unit) { mOutOfRangeMap[unit] = 1; }
   bool IsOutOfRange(BWAPI::Unit* unit) { mRit = mOutOfRangeMap.find(unit); return (mRit != mOutOfRangeMap.end()); }
   bool IsInRange(BWAPI::Unit* unit) { mRit = mOutOfRangeMap.find(unit); return (mRit == mOutOfRangeMap.end()); }

   static std::vector<std::vector<int> > mDistMatrix;
   static int mLastN;
   bool drawLines;
};



#endif
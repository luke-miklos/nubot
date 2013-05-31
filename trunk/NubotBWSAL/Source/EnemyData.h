
#ifndef ENEMYDATA_H
#define ENEMYDATA_H

#include "BWTA.h"
#include "BWAPI.h"

class EnemyData;

EnemyData* EnemyData::mInstancePtr = 0;

class EnemyData
{
public:

   static EnemyData* GetInstance()
   {
      if (mInstancePtr==0)
      {
         mInstancePtr = new EnemyData();
      }
      return mInstancePtr;
   }

   static void Destroy()
   {
      delete mInstancePtr;
      mInstancePtr = 0;
   }

   BWAPI::TilePosition GetLikelyEnemyLocation()
   {
      std::set<BWAPI::TilePosition> starts = BWAPI::Broodwar->getStartLocations();

      foreach( BWTA::BaseLocation * bl, BWTA::getStartLocations() )
      {
         if ( m_myStartLocation->getGroundDistance( bl ) > 0 && bl != m_myStartLocation )
         {
            //startLocations.insert( bl );
         }
      }



   }


private:

   EnemyData()
   {
      mUnknownStartingSpots = BWAPI::Broodwar->getStartLocations();
   }

   static EnemyData* mInstancePtr;

   std::set<BWAPI::TilePosition> mUnknownStartingSpots;
   std::set<BWAPI::TilePosition> mKnownBases;

};


#endif


#include "BWAPI/Game.h"
#include "BWAPI/Unit.h"
#include "BWAPI/Player.h"

#include "PerceivedUnit.h"

#include <set>
#include <map>


class PerceptionManager
{
public:
   static PerceptionManager& GetInstance();

   void Update(int currentGameFrame, BWAPI::Player* aPlayer = BWAPI::Broodwar->self());

   //event based set management (add to our perception when a new unit shows itself / is created
   virtual void onUnitShow(BWAPI::Unit* unit);
   //virtual void onUnitCreate(BWAPI::Unit* unit);
   //virtual void onUnitMorph(BWAPI::Unit* unit);
	virtual void onUnitDestroy(BWAPI::Unit* unit);

   //main access method
   virtual const std::set<BWAPI::Unit*>& getUnits() const;

   virtual const std::set<BWAPI::Unit*>& getPlayerUnits(BWAPI::Player* aPlayer);

   virtual PerceivedUnit* getPerceivedUnit(BWAPI::Unit* aUnit);

private:

   PerceptionManager();
   static PerceptionManager* mInstancePtr;

   std::set<BWAPI::Unit*>                            mUnitPerceptions;
   std::map<BWAPI::Player*, std::set<BWAPI::Unit*> > mPlayerUnits;
   std::map<int, BWAPI::Unit*>                       mUnitMap;
};

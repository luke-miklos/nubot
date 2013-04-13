
#include <BWAPI.h>
#include <BWSAL.h>
#include <set>


class BattleEstimator
{
public:
   enum EstimateMethod
   {
      cUNIT_COST,       //simplistic
      cUNIT_DPS,        //simplistic (ground only dps)
      cUNIT_DPS_HEALTH, //better (still only ground, takes into account unit size & health)
      cCOMPLEX,         //best, taking into account air/ground & special units
      cDEFAULT          //use whatever the defaul is
   };
   static BattleEstimator& GetInstance();
   float EstimateBattle(BWSAL::UnitGroup& a, BWSAL::UnitGroup& b, EstimateMethod method = cDEFAULT);	//returns > 1 for 'a' better, < 1 for 'b' better
   void SetEstimateMethod(EstimateMethod aMethod) { mMethod = aMethod; }
   EstimateMethod GetEstimateMethod() { return mMethod; }
private:
   BattleEstimator();
   static BattleEstimator* mInstance;
   EstimateMethod mMethod;
};
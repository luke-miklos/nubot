
#include "BattleEstimator.h"

BattleEstimator* BattleEstimator::mInstance = 0;

BattleEstimator::BattleEstimator()
{
}

BattleEstimator& BattleEstimator::GetInstance()
{
   if (mInstance == 0)
   {
      mInstance = new BattleEstimator();
   }
   return *mInstance;
}

//returns > 1 for 'a' better, < 1 for 'b' better
float BattleEstimator::EstimateBattle(BWSAL::UnitGroup& a, BWSAL::UnitGroup& b, EstimateMethod method)
{
	if (a.size() == 0) {
		if (b.size() == 0) {
			return 1.0f;
		}
		else {
			return 0.001f;
		}
	} else if (b.size() == 0) {
		return 1000.0f;
	}

	if (method == cDEFAULT) {
		method = mMethod;
	}

	std::set< BWAPI::Unit* >::iterator it;

	switch (method)
	{
	case cUNIT_COST:
		{
			int aCost = 0;
			int bCost = 0;
			for (it = a.begin(); it != a.end(); it++)
			{
				BWAPI::UnitType t = (*it)->getType();
				aCost += t.mineralPrice();
				aCost += t.gasPrice();
			}
			for (it = b.begin(); it != b.end(); it++)
			{
				BWAPI::UnitType t = (*it)->getType();
				bCost += t.mineralPrice();
				bCost += t.gasPrice();
			}
			return (float)aCost/(float)bCost;	//divide by zero should be handled by 'size()' check at top
		}break;

	case cUNIT_DPS:	//ground dps only
		{
			float aDPS = 0;
			float bDPS = 0;
			for (it = a.begin(); it != a.end(); it++)
			{
				//BWAPI::UnitType t = (*it)->getType();
				//BWAPI::WeaponType w = t.groundWeapon();
				BWAPI::WeaponType w = (*it)->getType().groundWeapon();
				if (w.targetsGround())
				{
					aDPS += ((float)w.damageAmount()/(float)w.damageCooldown());
					//w.damageType();	//for size
				}
			}
			for (it = b.begin(); it != b.end(); it++)
			{
				BWAPI::WeaponType w = (*it)->getType().groundWeapon();
				if (w.targetsGround())
				{
					bDPS += ((float)w.damageAmount()/(float)w.damageCooldown());
				}
			}
			//the 'size()' check at the top doesn't handle the case where all known units cant attack ground or dont attack
			if ((aDPS+bDPS)<0.000001) {
				return 1;
			} else if (bDPS < 0.000001) {
				return 1000.0f;
			} else if (aDPS < 0.000001) {
				return 0.001f;
			} else {
				return aDPS/bDPS;
			}
		}break;

	case cUNIT_DPS_HEALTH:
		{
			//just an estimate, dont have to simulate the whole battle (rough heuristic)
			//calculate ratios of target size types (example: enemy force 0.5 small, 0.3 medium, 0.2 large)
			//scale damage values according to how damage types do against target ratios
			int Small  = BWAPI::UnitSizeTypes::Small.getID();
			int Medium = BWAPI::UnitSizeTypes::Medium.getID();
			int Large  = BWAPI::UnitSizeTypes::Large.getID();
			int Normal     = BWAPI::DamageTypes::Normal.getID();
			int Concussive = BWAPI::DamageTypes::Concussive.getID();
			int Explosive  = BWAPI::DamageTypes::Explosive.getID();

			int aSmall  = 0;
			int aMedium = 0;
			int aLarge  = 0;
			float aNormalDPS     = 0;
			float aConcussiveDPS = 0;
			float aExplosiveDPS  = 0;

			//iterate over 'a' units & save off total health by unit type, and total dps by damage types
			for (it = a.begin(); it != a.end(); it++)
			{
				BWAPI::UnitType t = (*it)->getType();
				BWAPI::UnitSizeType z = t.size();
				BWAPI::WeaponType w = t.groundWeapon();

				if (z==Small) {
					aSmall += t.maxHitPoints();
					aSmall += t.maxShields();
				} else if (z==Medium) {
					aMedium += t.maxHitPoints();
					aMedium += t.maxShields();
				} else if (z==Large) {
					aLarge += t.maxHitPoints();
					aLarge += t.maxShields();
				}

				if (w.targetsGround())
				{
					if (w.damageType()==Normal) {
						aNormalDPS += ((float)w.damageAmount()/(float)w.damageCooldown());
					} else if (w.damageType()==Concussive) {
						aConcussiveDPS += ((float)w.damageAmount()/(float)w.damageCooldown());
					} else if (w.damageType()==Explosive) {
						aExplosiveDPS += ((float)w.damageAmount()/(float)w.damageCooldown());
					}
				}
			}
			//calculate ratios from raw totals
			int aTotalHealth = aSmall + aMedium + aLarge;
			float aSmallRatio = (float)aSmall/(float)aTotalHealth;
			float aMediumRatio = (float)aMedium/(float)aTotalHealth;
			float aLargeRatio = (float)aLarge/(float)aTotalHealth;

			int bSmall  = 0;
			int bMedium = 0;
			int bLarge  = 0;
			float bNormalDPS     = 0;
			float bConcussiveDPS = 0;
			float bExplosiveDPS  = 0;

			//iterate over 'b' units & save off total health by unit type, and total dps by damage types
			for (it = b.begin(); it != b.end(); it++)
			{
				BWAPI::UnitType t = (*it)->getType();
				BWAPI::UnitSizeType z = t.size();
				BWAPI::WeaponType w = t.groundWeapon();

				if (z==Small) {
					bSmall += t.maxHitPoints();
					bSmall += t.maxShields();
				} else if (z==Medium) {
					bMedium += t.maxHitPoints();
					bMedium += t.maxShields();
				} else if (z==Large) {
					bLarge += t.maxHitPoints();
					bLarge += t.maxShields();
				}

				if (w.targetsGround())
				{
					if (w.damageType()==Normal) {
						bNormalDPS += ((float)w.damageAmount()/(float)w.damageCooldown());
					} else if (w.damageType()==Concussive) {
						bConcussiveDPS += ((float)w.damageAmount()/(float)w.damageCooldown());
					} else if (w.damageType()==Explosive) {
						bExplosiveDPS += ((float)w.damageAmount()/(float)w.damageCooldown());
					}
				}
			}
			//calculate ratios from raw totals
			int bTotalHealth = bSmall + bMedium + bLarge;
			float bSmallRatio = (float)bSmall/(float)bTotalHealth;
			float bMediumRatio = (float)bMedium/(float)bTotalHealth;
			float bLargeRatio = (float)bLarge/(float)bTotalHealth;

			//use ratios to calculate average effective DPS against entire enemy force composition
			float aEffectiveDPS = aNormalDPS + aConcussiveDPS*(bSmallRatio + 0.5f*bMediumRatio + 0.25f*bLargeRatio) + aExplosiveDPS*(0.5f*bSmallRatio + 0.75f*bMediumRatio + bLargeRatio);
			float bEffectiveDPS = bNormalDPS + bConcussiveDPS*(aSmallRatio + 0.5f*aMediumRatio + 0.25f*aLargeRatio) + bExplosiveDPS*(0.5f*aSmallRatio + 0.75f*aMediumRatio + aLargeRatio);

			if (aEffectiveDPS < 0.000001) {
				if (bEffectiveDPS < 0.000001) {
					return 1.0f;
				} else {
					return 0.001f;
				}
			} else if (bEffectiveDPS < 0.000001) {
				return 1000.0f;
			}
			//use effective dps against total health to find how long they each live
			float aTime = (float)aTotalHealth / bEffectiveDPS;
			float bTime = (float)bTotalHealth / aEffectiveDPS;
			return (aTime / bTime);
		}break;

	case cCOMPLEX:
	default:
		{
			return 1.0f;
		}break;
	};
}


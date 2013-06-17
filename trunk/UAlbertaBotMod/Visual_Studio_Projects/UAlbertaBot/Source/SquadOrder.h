#pragma once

#include "Common.h"

class SquadOrder
{
public:

	enum Type { None, Attack, Defend, Regroup, SquadOrderTypes };

	Type				type;
	BWAPI::Position		position;
	int					radius;

	SquadOrder() : type(None) {}
	SquadOrder(Type type, BWAPI::Position position, int radius) :
		type(type), position(position), radius(radius) {}

	std::string getOrderString() const {

		if (type == SquadOrder::Attack)					{ return "Attack"; } 
		else if (type == SquadOrder::Defend)			{ return "Defend"; } 
		else if (type == SquadOrder::Regroup)			{ return "Regroup"; } 

		return "Not Formed";
	}
};

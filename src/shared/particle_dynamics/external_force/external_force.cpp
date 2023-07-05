#include "external_force.h"

namespace SPH {
//=================================================================================================//
	ExternalForce::ExternalForce() {}
//=================================================================================================//
	Gravity::Gravity(Vecd global_acceleration, Vecd reference_position)
		: ExternalForce(), global_acceleration_(global_acceleration),
        global_acceleration_device_(hostToDeviceVecd(global_acceleration_)),
		zero_potential_reference_(reference_position) {}
	//=================================================================================================//
	Vecd Gravity::InducedAcceleration(Vecd& position)
	{
        return global_acceleration_;
	}
	//=================================================================================================//
	Real Gravity::getPotential(Vecd& position)
	{
		return InducedAcceleration(position).dot(zero_potential_reference_ - position);
	}
	//=================================================================================================//
}
//=================================================================================================//
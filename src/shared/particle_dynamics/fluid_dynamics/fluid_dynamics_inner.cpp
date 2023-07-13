#include "fluid_dynamics_inner.hpp"

namespace SPH
{
//=====================================================================================================//
namespace fluid_dynamics
{
//=================================================================================================//
FluidInitialCondition::
    FluidInitialCondition(SPHBody &sph_body)
    : LocalDynamics(sph_body), FluidDataSimple(sph_body),
      pos_(particles_->pos_), vel_(particles_->vel_) {}
//=================================================================================================//
BaseDensitySummationInner::BaseDensitySummationInner(BaseInnerRelation &inner_relation)
    : LocalDynamics(inner_relation.getSPHBody()), FluidDataInner(inner_relation),
      rho_(particles_->rho_), mass_(particles_->mass_),
      rho0_(sph_body_.base_material_->ReferenceDensity()),
      inv_sigma0_(1.0 / sph_body_.sph_adaptation_->LatticeNumberDensity())
{
    particles_->registerVariable(rho_sum_, "DensitySummation");
}
//=================================================================================================//
void BaseDensitySummationInner::update(size_t index_i, Real dt)
{
    rho_[index_i] = rho_sum_[index_i];
}
//=================================================================================================//
DensitySummationInner::DensitySummationInner(BaseInnerRelation &inner_relation)
    : BaseDensitySummationInner(inner_relation),
      W0_(sph_body_.sph_adaptation_->getKernel()->W0(ZeroVecd)),
      device_proxy(this, W0_, inner_configuration_device_ ? inner_configuration_device_->data() : nullptr, particles_, rho0_, inv_sigma0_) {}
//=================================================================================================//
DensitySummationInnerAdaptive::
    DensitySummationInnerAdaptive(BaseInnerRelation &inner_relation)
    : BaseDensitySummationInner(inner_relation),
      sph_adaptation_(*sph_body_.sph_adaptation_),
      kernel_(*sph_adaptation_.getKernel()),
      h_ratio_(*particles_->getVariableByName<Real>("SmoothingLengthRatio")) {}
//=================================================================================================//
BaseViscousAccelerationInner::BaseViscousAccelerationInner(BaseInnerRelation &inner_relation)
    : LocalDynamics(inner_relation.getSPHBody()), FluidDataInner(inner_relation),
      rho_(particles_->rho_), vel_(particles_->vel_), acc_prior_(particles_->acc_prior_),
      mu_(DynamicCast<Fluid>(this, particles_->getBaseMaterial()).ReferenceViscosity()),
      smoothing_length_(sph_body_.sph_adaptation_->ReferenceSmoothingLength()) {}
//=================================================================================================//
TransportVelocityCorrectionInner::
    TransportVelocityCorrectionInner(BaseInnerRelation &inner_relation, Real coefficient)
    : LocalDynamics(inner_relation.getSPHBody()), FluidDataInner(inner_relation),
      pos_(particles_->pos_), surface_indicator_(*particles_->getVariableByName<int>("SurfaceIndicator")),
      smoothing_length_sqr_(pow(sph_body_.sph_adaptation_->ReferenceSmoothingLength(), 2)),
      coefficient_(coefficient) {}
//=================================================================================================//
TransportVelocityCorrectionInnerAdaptive::
    TransportVelocityCorrectionInnerAdaptive(BaseInnerRelation &inner_relation, Real coefficient)
    : LocalDynamics(inner_relation.getSPHBody()), FluidDataInner(inner_relation),
      sph_adaptation_(*sph_body_.sph_adaptation_),
      pos_(particles_->pos_), surface_indicator_(*particles_->getVariableByName<int>("SurfaceIndicator")),
      smoothing_length_sqr_(pow(sph_body_.sph_adaptation_->ReferenceSmoothingLength(), 2)),
      coefficient_(coefficient) {}
//=================================================================================================//
AcousticTimeStepSize::AcousticTimeStepSize(SPHBody &sph_body, Real acousticCFL)
    : LocalDynamicsReduce<Real, ReduceMax>(sph_body, Real(0)),
      FluidDataSimple(sph_body), fluid_(DynamicCast<Fluid>(this, particles_->getBaseMaterial())), rho_(particles_->rho_),
      p_(*particles_->getVariableByName<Real>("Pressure")), vel_(particles_->vel_),
      smoothing_length_min_(sph_body.sph_adaptation_->MinimumSmoothingLength()),
      acousticCFL_(acousticCFL), device_proxy(this, particles_) {}
//=================================================================================================//
Real AcousticTimeStepSize::reduce(size_t index_i, Real dt)
{
    return decltype(device_proxy)::Kernel::reduce(index_i, dt, fluid_, p_.data(), rho_.data(), vel_.data(),
                   [](Fluid& fluid, Real p_i, Real rho_i) { return fluid.getSoundSpeed(p_i, rho_i); });
}
//=================================================================================================//
Real AcousticTimeStepSize::outputResult(Real reduced_value)
{
    // since the particle does not change its configuration in pressure relaxation step
    // I chose a time-step size according to Eulerian method
    return acousticCFL_ * smoothing_length_min_ / (reduced_value + TinyReal);
}
//=================================================================================================//
AdvectionTimeStepSizeForImplicitViscosity::
    AdvectionTimeStepSizeForImplicitViscosity(SPHBody &sph_body, Real U_max, Real advectionCFL)
    : LocalDynamicsReduce<Real, ReduceMax>(sph_body, U_max * U_max),
      FluidDataSimple(sph_body), vel_(particles_->vel_),
      smoothing_length_min_(sph_body.sph_adaptation_->MinimumSmoothingLength()),
      advectionCFL_(advectionCFL), device_proxy(this, particles_) {}
//=================================================================================================//
Real AdvectionTimeStepSizeForImplicitViscosity::reduce(size_t index_i, Real dt)
{
    return AdvectionTimeStepSizeForImplicitViscosityKernel::reduce(index_i, dt, vel_.data());
}
//=================================================================================================//
Real AdvectionTimeStepSizeForImplicitViscosity::outputResult(Real reduced_value)
{
    Real speed_max = sqrt(reduced_value);
    return advectionCFL_ * smoothing_length_min_ / (speed_max + TinyReal);
}
//=================================================================================================//
AdvectionTimeStepSize::AdvectionTimeStepSize(SPHBody &sph_body, Real U_max, Real advectionCFL)
    : AdvectionTimeStepSizeForImplicitViscosity(sph_body, U_max, advectionCFL),
      fluid_(DynamicCast<Fluid>(this, particles_->getBaseMaterial()))
{
    Real viscous_speed = fluid_.ReferenceViscosity() / fluid_.ReferenceDensity() / smoothing_length_min_;
    reference_ = SMAX(viscous_speed * viscous_speed, reference_);
}
//=================================================================================================//
Real AdvectionTimeStepSize::reduce(size_t index_i, Real dt)
{
    return AdvectionTimeStepSizeForImplicitViscosity::reduce(index_i, dt);
}
//=================================================================================================//
VorticityInner::VorticityInner(BaseInnerRelation &inner_relation)
    : LocalDynamics(inner_relation.getSPHBody()), FluidDataInner(inner_relation),
      vel_(particles_->vel_)
{
    particles_->registerVariable(vorticity_, "VorticityInner");
    particles_->addVariableToWrite<AngularVecd>("VorticityInner");
}
//=================================================================================================//
BaseIntegration::BaseIntegration(BaseInnerRelation &inner_relation)
    : LocalDynamics(inner_relation.getSPHBody()), FluidDataInner(inner_relation),
      fluid_(DynamicCast<Fluid>(this, particles_->getBaseMaterial())), rho_(particles_->rho_),
      p_(*particles_->getVariableByName<Real>("Pressure")), drho_dt_(*particles_->registerSharedVariable<Real>("DensityChangeRate")), pos_(particles_->pos_), vel_(particles_->vel_),
      acc_(particles_->acc_), acc_prior_(particles_->acc_prior_) {}
//=================================================================================================//
Oldroyd_BIntegration1stHalf ::
    Oldroyd_BIntegration1stHalf(BaseInnerRelation &inner_relation)
    : Integration1stHalfDissipativeRiemann(inner_relation)
{
    particles_->registerVariable(tau_, "ElasticStress");
    particles_->registerVariable(dtau_dt_, "ElasticStressChangeRate");
    particles_->registerSortableVariable<Matd>("ElasticStress");
    particles_->addVariableToRestart<Matd>("ElasticStress");
}
//=================================================================================================//
void Oldroyd_BIntegration1stHalf::initialization(size_t index_i, Real dt)
{
    Integration1stHalfDissipativeRiemann::initialization(index_i, dt);

    tau_[index_i] += dtau_dt_[index_i] * dt * 0.5;
}
//=================================================================================================//
Oldroyd_BIntegration2ndHalf::
    Oldroyd_BIntegration2ndHalf(BaseInnerRelation &inner_relation)
    : Integration2ndHalfDissipativeRiemann(inner_relation),
      oldroyd_b_fluid_(DynamicCast<Oldroyd_B_Fluid>(this, particles_->getBaseMaterial())),
      tau_(*particles_->getVariableByName<Matd>("ElasticStress")),
      dtau_dt_(*particles_->getVariableByName<Matd>("ElasticStressChangeRate"))
{
    mu_p_ = oldroyd_b_fluid_.ReferencePolymericViscosity();
    lambda_ = oldroyd_b_fluid_.getReferenceRelaxationTime();
}
//=================================================================================================//
void Oldroyd_BIntegration2ndHalf::update(size_t index_i, Real dt)
{
    Integration2ndHalfDissipativeRiemann::update(index_i, dt);

    tau_[index_i] += dtau_dt_[index_i] * dt * 0.5;
}
//=================================================================================================//
} // namespace fluid_dynamics
  //=====================================================================================================//
} // namespace SPH
  //=========================================================================================================//

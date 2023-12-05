#ifndef CONTINUUM_DYNAMICS_INNER_H
#define CONTINUUM_DYNAMICS_INNER_H
#include "constraint_dynamics.h"
#include "continuum_particles.h"
#include "fluid_integration.hpp"
#include "fluid_time_step.h"
#include "general_continuum.h"
#include "riemann_solver_extra.h"

namespace SPH
{
namespace continuum_dynamics
{
typedef DataDelegateSimple<ContinuumParticles> ContinuumDataSimple;
typedef DataDelegateInner<ContinuumParticles> ContinuumDataInner;

typedef DataDelegateSimple<PlasticContinuumParticles> PlasticContinuumDataSimple;
typedef DataDelegateInner<PlasticContinuumParticles> PlasticContinuumDataInner;

class ContinuumInitialCondition : public LocalDynamics, public PlasticContinuumDataSimple
{
  public:
    explicit ContinuumInitialCondition(SPHBody &sph_body);
    virtual ~ContinuumInitialCondition(){};

  protected:
    StdLargeVec<Vecd> &pos_, &vel_;
    StdLargeVec<Mat3d> &stress_tensor_3D_;
};

/**
 * @class BaseIntegration
 * @brief Pure abstract base class for all fluid relaxation schemes
 */
class BaseRelaxation : public LocalDynamics, public ContinuumDataInner
{
  public:
    explicit BaseRelaxation(BaseInnerRelation &inner_relation);
    virtual ~BaseRelaxation(){};

  protected:
    GeneralContinuum &continuum_;
    StdLargeVec<Real> &rho_, &p_, &drho_dt_;
    StdLargeVec<Vecd> &pos_, &vel_, &force_, &force_prior_;
};
/**
 * @class Integration1stHalf
 * @brief Pressure relaxation scheme with the mostly used Riemann solver.
 */
template <class FluidDynamicsType>
class BaseIntegration1stHalf : public FluidDynamicsType
{
  public:
    explicit BaseIntegration1stHalf(BaseInnerRelation &inner_relation);
    virtual ~BaseIntegration1stHalf(){};
    void update(size_t index_i, Real dt = 0.0);

  protected:
    StdLargeVec<Vecd> &acc_shear_;
};
using Integration1stHalf = BaseIntegration1stHalf<fluid_dynamics::Integration1stHalfInnerNoRiemann>;
using Integration1stHalfRiemann = BaseIntegration1stHalf<fluid_dynamics::Integration1stHalfInnerRiemann>;

/**
 * @class ShearAccelerationRelaxation
 */
class ShearAccelerationRelaxation : public BaseRelaxation
{
  public:
    explicit ShearAccelerationRelaxation(BaseInnerRelation &inner_relation);
    virtual ~ShearAccelerationRelaxation(){};
    void interaction(size_t index_i, Real dt = 0.0);

  protected:
    Real G_, smoothing_length_;
    StdLargeVec<Matd> &shear_stress_;
    StdLargeVec<Vecd> &acc_shear_;
};
/**
 * @class ShearStressRelaxation
 */
class ShearStressRelaxation : public BaseRelaxation
{
  public:
    explicit ShearStressRelaxation(BaseInnerRelation &inner_relation);
    virtual ~ShearStressRelaxation(){};
    void initialization(size_t index_i, Real dt = 0.0);
    void interaction(size_t index_i, Real dt = 0.0);
    void update(size_t index_i, Real dt = 0.0);

  protected:
    StdLargeVec<Matd> &shear_stress_, &shear_stress_rate_, &velocity_gradient_, &strain_tensor_, &strain_tensor_rate_;
    StdLargeVec<Real> &von_mises_stress_, &von_mises_strain_, &Vol_;
    StdLargeVec<Matd> &B_;
};

/**
 * @class BaseMotionConstraint
 */
template <class DynamicsIdentifier>
class BaseMotionConstraint : public BaseLocalDynamics<DynamicsIdentifier>, public ContinuumDataSimple
{
  public:
    explicit BaseMotionConstraint(DynamicsIdentifier &identifier)
        : BaseLocalDynamics<DynamicsIdentifier>(identifier), ContinuumDataSimple(identifier.getSPHBody()),
          pos_(particles_->pos_), pos0_(particles_->pos0_),
          n_(particles_->n_), n0_(particles_->n0_),
          vel_(particles_->vel_), force_(particles_->force_){};

    virtual ~BaseMotionConstraint(){};

  protected:
    StdLargeVec<Vecd> &pos_, &pos0_;
    StdLargeVec<Vecd> &n_, &n0_;
    StdLargeVec<Vecd> &vel_, &force_;
};
/**@class FixConstraint
 * @brief Constraint with zero velocity.
 */
template <class DynamicsIdentifier>
class FixConstraint : public BaseMotionConstraint<DynamicsIdentifier>
{
  public:
    explicit FixConstraint(DynamicsIdentifier &identifier)
        : BaseMotionConstraint<DynamicsIdentifier>(identifier){};
    virtual ~FixConstraint(){};

    void update(size_t index_i, Real dt = 0.0) { this->vel_[index_i] = Vecd::Zero(); };
};
using FixBodyConstraint = FixConstraint<SPHBody>;
using FixBodyPartConstraint = FixConstraint<BodyPartByParticle>;

/**
 * @class FixedInAxisDirection
 * @brief Constrain the velocity of a solid body part.
 */
class FixedInAxisDirection : public BaseMotionConstraint<BodyPartByParticle>
{
  public:
    FixedInAxisDirection(BodyPartByParticle &body_part, Vecd constrained_axises = Vecd::Zero());
    virtual ~FixedInAxisDirection(){};
    void update(size_t index_i, Real dt = 0.0);

  protected:
    Matd constrain_matrix_;
};

/**
 * @class ConstrainSolidBodyMassCenter
 * @brief Constrain the mass center of a solid body.
 */
class ConstrainSolidBodyMassCenter : public LocalDynamics, public ContinuumDataSimple
{
  private:
    Real total_mass_;
    Matd correction_matrix_;
    Vecd velocity_correction_;
    StdLargeVec<Vecd> &vel_;
    ReduceDynamics<QuantityMoment<Vecd>> compute_total_momentum_;

  protected:
    virtual void setupDynamics(Real dt = 0.0) override;

  public:
    explicit ConstrainSolidBodyMassCenter(SPHBody &sph_body, Vecd constrain_direction = Vecd::Ones());
    virtual ~ConstrainSolidBodyMassCenter(){};

    void update(size_t index_i, Real dt = 0.0);
};

class BaseRelaxationPlastic : public LocalDynamics, public PlasticContinuumDataInner
{
  public:
    explicit BaseRelaxationPlastic(BaseInnerRelation &inner_relation);
    virtual ~BaseRelaxationPlastic(){};
    Matd reduceTensor(Mat3d tensor_3d);
    Mat3d increaseTensor(Matd tensor_2d);

  protected:
    PlasticContinuum &plastic_continuum_;
    StdLargeVec<Real> &rho_, &p_, &drho_dt_, & mass_;
    StdLargeVec<Vecd> &pos_, &vel_, &force_, &force_prior_;
    StdLargeVec<Mat3d> &stress_tensor_3D_, &strain_tensor_3D_, &stress_rate_3D_, &strain_rate_3D_;
    StdLargeVec<Mat3d> &elastic_strain_tensor_3D_, &elastic_strain_rate_3D_;
};

template <class RiemannSolverType>
class BaseStressRelaxation1stHalf : public BaseRelaxationPlastic
{
    public:
        explicit BaseStressRelaxation1stHalf(BaseInnerRelation& inner_relation);
        virtual ~BaseStressRelaxation1stHalf() {};
        RiemannSolverType riemann_solver_;
        void initialization(size_t index_i, Real dt = 0.0);
        void interaction(size_t index_i, Real dt = 0.0);
        void update(size_t index_i, Real dt = 0.0);

    protected:
        virtual Vecd computeNonConservativeForce(size_t index_i);
        StdLargeVec<Matd>& velocity_gradient_;
};

using StressRelaxation1stHalf = BaseStressRelaxation1stHalf<NoRiemannSolver>;
using StressRelaxation1stHalfRiemann = BaseStressRelaxation1stHalf<AcousticRiemannSolverExtra>;

template <class RiemannSolverType>
class BaseStressRelaxation2ndHalf : public BaseRelaxationPlastic
{
    public:
        explicit BaseStressRelaxation2ndHalf(BaseInnerRelation& inner_relation);
        virtual ~BaseStressRelaxation2ndHalf() {};
        RiemannSolverType riemann_solver_;
        void initialization(size_t index_i, Real dt = 0.0);
        void interaction(size_t index_i, Real dt = 0.0);
        void update(size_t index_i, Real dt = 0.0);

    protected:
        StdLargeVec<Matd>& velocity_gradient_;
        StdLargeVec<Real>& acc_deviatoric_plastic_strain_, & vertical_stress_;
        StdLargeVec<Real>& Vol_, & mass_;
        Real E_, nu_;
};
using StressRelaxation2ndHalf = BaseStressRelaxation2ndHalf<NoRiemannSolver>;
using StressRelaxation2ndHalfRiemann = BaseStressRelaxation2ndHalf<AcousticRiemannSolverExtra>;
/**
 * @class StressDiffusion
 */
class StressDiffusion : public BaseRelaxationPlastic
{
  public:
    explicit StressDiffusion(BaseInnerRelation &inner_relation);
    virtual ~StressDiffusion(){};
    void interaction(size_t index_i, Real dt = 0.0);

  protected:
    Real zeta_ = 0.1, fai_; // diffusion coefficient
    Real smoothing_length_, sound_speed_;
};
} // namespace continuum_dynamics
} // namespace SPH
#endif // CONTINUUM_DYNAMICS_INNER_H
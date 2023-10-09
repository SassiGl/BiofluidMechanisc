/* ---------------------------------------------------------------------------*
 *            SPHinXsys: 2D oscillation beam example-one body version           *
 * ----------------------------------------------------------------------------*
 * This is the one of the basic test cases, also the first case for            *
 * understanding SPH method for solid simulation.                              *
 * In this case, the constraint of the beam is implemented with                *
 * internal constrained subregion.                                             *
 * ----------------------------------------------------------------------------*/
#include "sphinxsys.h"
using namespace SPH;
//------------------------------------------------------------------------------
// global parameters for the case
//------------------------------------------------------------------------------
Real PL = 0.2;  // beam length
Real PH = 0.02; // for thick plate; =0.01 for thin plate
Real SL = 0.06; // depth of the insert
// reference particle spacing
Real resolution_ref = PH / 10.0;
Real BW = resolution_ref * 4; // boundary width, at least three particles
/** Domain bounds of the system. */
BoundingBox system_domain_bounds(Vec2d(-SL - BW, -PL / 2.0),
                                 Vec2d(PL + 3.0 * BW, PL / 2.0));
//----------------------------------------------------------------------
//	Material properties of the solid.
//----------------------------------------------------------------------
Real rho0_s = 1.0e3;         // reference density
Real Youngs_modulus = 2.0e6; // reference Youngs modulus
Real poisson = 0.3975;       // Poisson ratio
//----------------------------------------------------------------------
//	Parameters for initial condition on velocity
//----------------------------------------------------------------------
Real kl = 1.875;
Real M = sin(kl) + sinh(kl);
Real N = cos(kl) + cosh(kl);
Real Q = 2.0 * (cos(kl) * sinh(kl) - sin(kl) * cosh(kl));
Real vf = 0.05;
Real R = PL / (0.5 * Pi);
//----------------------------------------------------------------------
//	Geometric shapes used in the system.
//----------------------------------------------------------------------
// a beam base shape
std::vector<Vecd> beam_base_shape{
    Vecd(-SL - BW, -PH / 2 - BW), Vecd(-SL - BW, PH / 2 + BW), Vecd(0.0, PH / 2 + BW),
    Vecd(0.0, -PH / 2 - BW), Vecd(-SL - BW, -PH / 2 - BW)};
// a beam shape
std::vector<Vecd> beam_shape{
    Vecd(-SL, -PH / 2), Vecd(-SL, PH / 2), Vecd(PL, PH / 2), Vecd(PL, -PH / 2), Vecd(-SL, -PH / 2)};
// Beam observer location
StdVec<Vecd> observation_location = {Vecd(PL, 0.0)};
//----------------------------------------------------------------------
//	Define the beam body
//----------------------------------------------------------------------
class Beam : public MultiPolygonShape
{
  public:
    explicit Beam(const std::string &shape_name) : MultiPolygonShape(shape_name)
    {
        multi_polygon_.addAPolygon(beam_base_shape, ShapeBooleanOps::add);
        multi_polygon_.addAPolygon(beam_shape, ShapeBooleanOps::add);
    }
};
//----------------------------------------------------------------------
//	application dependent initial condition
//----------------------------------------------------------------------
class BeamInitialCondition
    : public solid_dynamics::ElasticDynamicsInitialCondition
{
  public:
    explicit BeamInitialCondition(SPHBody &sph_body)
        : solid_dynamics::ElasticDynamicsInitialCondition(sph_body){};

    void update(size_t index_i, Real dt)
    {
        /** initial velocity profile */
        Real x = pos_[index_i][0] / PL;
        if (x > 0.0)
        {
            vel_[index_i][1] = vf * particles_->elastic_solid_.ReferenceSoundSpeed() *
                               (M * (cos(kl * x) - cosh(kl * x)) - N * (sin(kl * x) - sinh(kl * x))) / Q;
        }
    };
};
//----------------------------------------------------------------------
//	define the beam base which will be constrained.
//----------------------------------------------------------------------
MultiPolygon createBeamConstrainShape()
{
    MultiPolygon multi_polygon;
    multi_polygon.addAPolygon(beam_base_shape, ShapeBooleanOps::add);
    multi_polygon.addAPolygon(beam_shape, ShapeBooleanOps::sub);
    return multi_polygon;
};
//------------------------------------------------------------------------------
// the main program
//------------------------------------------------------------------------------
int main(int ac, char *av[])
{
    //----------------------------------------------------------------------
    //	Build up the environment of a SPHSystem with global controls.
    //----------------------------------------------------------------------
    SPHSystem sph_system(system_domain_bounds, resolution_ref);
    /** Tag for computation start with relaxed body fitted particles distribution. */
    sph_system.setRunParticleRelaxation(false);
    sph_system.setReloadParticles(true);
#ifdef BOOST_AVAILABLE
    // handle command line arguments
    sph_system.handleCommandlineOptions(ac, av);
    IOEnvironment io_environment(sph_system);
#endif //----------------------------------------------------------------------
       //	Creating body, materials and particles.
       //----------------------------------------------------------------------
    SolidBody beam_body(sph_system, makeShared<Beam>("BeamBody"));
    beam_body.defineBodyLevelSetShape()->writeLevelSet(io_environment);

    beam_body.defineParticlesAndMaterial<ElasticSolidParticles, SaintVenantKirchhoffSolid>(rho0_s, Youngs_modulus, poisson);
    (!sph_system.RunParticleRelaxation() && sph_system.ReloadParticles())
        ? beam_body.generateParticles<ParticleGeneratorReload>(io_environment, beam_body.getName())
        : beam_body.generateParticles<ParticleGeneratorLattice>();

    ObserverBody beam_observer(sph_system, "BeamObserver");
    beam_observer.defineAdaptationRatios(1.15, 2.0);
    beam_observer.generateParticles<ObserverParticleGenerator>(observation_location);

    beam_body.addBodyStateForRecording<Matd>("CorrectionMatrix");
    beam_body.addBodyStateForRecording<Matd>("CorrectionMatrixWithLevelSet");
    beam_body.addBodyStateForRecording<Real>("VolumetricMeasure");
     //----------------------------------------------------------------------
    //	Run particle relaxation for body-fitted distribution if chosen.
    //----------------------------------------------------------------------
    if (sph_system.RunParticleRelaxation())
    {
        //----------------------------------------------------------------------
        //	Define body relation map used for particle relaxation.
        //----------------------------------------------------------------------
        InnerRelation beam_body_inner(beam_body);
        //----------------------------------------------------------------------
        //	Methods used for particle relaxation.
        //----------------------------------------------------------------------
        /** Random reset the insert body particle position. */
        SimpleDynamics<RandomizeParticlePosition> random_insert_body_particles(beam_body);
        /** Write the body state to Vtp file. */
        BodyStatesRecordingToPlt write_insert_body_to_vtp(io_environment, { &beam_body });
        /** Write the particle reload files. */
        ReloadParticleIO write_particle_reload_files(io_environment, { &beam_body });
        /** A  Physics relaxation step. */
        InteractionWithUpdate<relax_dynamics::CorrectedConfigurationInnerWithLevelSet> configuration_beam_body(beam_body_inner, true);
        relax_dynamics::RelaxationStepInner relaxation_step_inner_explicit(beam_body_inner, true);
        relax_dynamics::RelaxationStepByCMInner relaxation_step_cm_inner_explicit(beam_body_inner, true);
        relax_dynamics::RelaxationStepImplicitInner relaxation_step_inner(beam_body_inner, true);
        relax_dynamics::RelaxationStepByCMImplicitInner relaxation_step_cm_inner(beam_body_inner, true);
        InteractionDynamics<relax_dynamics::CheckCorrectedZeroOrderConsistency> check_zero_order_consistency(beam_body_inner, true);
        InteractionDynamics<relax_dynamics::UpdateParticleKineticEnergy> update_kinetic_energy(beam_body_inner);
        //----------------------------------------------------------------------
        //	Particle relaxation starts here.
        //----------------------------------------------------------------------
        random_insert_body_particles.exec(0.25);
        relaxation_step_inner.SurfaceBounding().exec();
        write_insert_body_to_vtp.writeToFile(0);
        //----------------------------------------------------------------------
        //	Relax particles of the insert body.
        //----------------------------------------------------------------------
        int ite_p = 0;
        while (ite_p < 20000)
        {
            configuration_beam_body.exec();
            //relaxation_step_inner_explicit.exec();
            relaxation_step_cm_inner_explicit.exec();
            //relaxation_step_inner.exec();
            //relaxation_step_cm_inner.exec();
            ite_p += 1;
            if (ite_p % 200 == 0)
            {
                check_zero_order_consistency.exec();
                update_kinetic_energy.exec();
                std::cout << std::fixed << std::setprecision(9) << "Relaxation steps for the inserted body N = " << ite_p << "\n";
                write_insert_body_to_vtp.writeToFile(ite_p);
            }
        }
        std::cout << "The physics relaxation process of inserted body finish !" << std::endl;
        /** Output results. */
        write_particle_reload_files.writeToFile(0);
        return 0;
    }
    //----------------------------------------------------------------------
    //	Define body relation map.
    //	The contact map gives the topological connections between the bodies.
    //	Basically the the range of bodies to build neighbor particle lists.
    //  Generally, we first define all the inner relations, then the contact relations.
    //  At last, we define the complex relaxations by combining previous defined
    //  inner and contact relations.
    //----------------------------------------------------------------------
    InnerRelation beam_body_inner(beam_body);
    ContactRelation beam_observer_contact(beam_observer, {&beam_body});
    //-----------------------------------------------------------------------------
    // this section define all numerical methods will be used in this case
    //-----------------------------------------------------------------------------
    SimpleDynamics<BeamInitialCondition> beam_initial_velocity(beam_body);
    // corrected strong configuration
    InteractionWithUpdate<CorrectedConfigurationInner> beam_corrected_configuration(beam_body_inner);
    InteractionWithUpdate<relax_dynamics::CorrectedConfigurationInnerWithLevelSet> beam_corrected_configuration_level_set(beam_body_inner, true);
    // time step size calculation
    ReduceDynamics<solid_dynamics::AcousticTimeStepSize> computing_time_step_size(beam_body);
    // stress relaxation for the beam
    Dynamics1Level<solid_dynamics::Integration1stHalfPK2> stress_relaxation_first_half(beam_body_inner);
    Dynamics1Level<solid_dynamics::Integration2ndHalf> stress_relaxation_second_half(beam_body_inner);
    // clamping a solid body part. This is softer than a direct constraint
    BodyRegionByParticle beam_base(beam_body, makeShared<MultiPolygonShape>(createBeamConstrainShape()));
    SimpleDynamics<solid_dynamics::FixBodyPartConstraint> constraint_beam_base(beam_base);
    //-----------------------------------------------------------------------------
    // outputs
    //-----------------------------------------------------------------------------
    BodyStatesRecordingToPlt write_beam_states(io_environment, sph_system.real_bodies_);
    RegressionTestEnsembleAverage<ObservedQuantityRecording<Vecd>>
        write_beam_tip_displacement("Position", io_environment, beam_observer_contact);
    //----------------------------------------------------------------------
    //	Setup computing and initial conditions.
    //----------------------------------------------------------------------
    sph_system.initializeSystemCellLinkedLists();
    sph_system.initializeSystemConfigurations();
    beam_initial_velocity.exec();
    beam_corrected_configuration.exec();
    beam_corrected_configuration_level_set.exec();
    //----------------------------------------------------------------------
    //	Setup computing time-step controls.
    //----------------------------------------------------------------------
    int ite = 0;
    Real T0 = 2.0;
    Real end_time = T0;
    // time step size for output file
    Real output_interval = 0.005 * T0;
    Real Dt = 0.1 * output_interval; /**< Time period for data observing */
    Real dt = 0.0;                   // default acoustic time step sizes

    // statistics for computing time
    TickCount t1 = TickCount::now();
    TimeInterval interval;
    //-----------------------------------------------------------------------------
    // from here the time stepping begins
    //-----------------------------------------------------------------------------
    write_beam_states.writeToFile(0);
    write_beam_tip_displacement.writeToFile(0);

    // computation loop starts
    while (GlobalStaticVariables::physical_time_ < end_time)
    {
        Real integration_time = 0.0;
        // integrate time (loop) until the next output time
        while (integration_time < output_interval)
        {

            Real relaxation_time = 0.0;
            while (relaxation_time < Dt)
            {
                stress_relaxation_first_half.exec(dt);
                constraint_beam_base.exec();
                stress_relaxation_second_half.exec(dt);

                ite++;
                dt = computing_time_step_size.exec();
                relaxation_time += dt;
                integration_time += dt;
                GlobalStaticVariables::physical_time_ += dt;

                if (ite % 100 == 0)
                {
                    std::cout << "N=" << ite << " Time: "
                              << GlobalStaticVariables::physical_time_ << "	dt: "
                              << dt << "\n";
                }
            }
        }

        write_beam_tip_displacement.writeToFile(ite);

        TickCount t2 = TickCount::now();
        write_beam_states.writeToFile();
        TickCount t3 = TickCount::now();
        interval += t3 - t2;
    }
    TickCount t4 = TickCount::now();

    TimeInterval tt;
    tt = t4 - t1 - interval;
    std::cout << "Total wall time for computation: " << tt.seconds() << " seconds." << std::endl;

    if (sph_system.GenerateRegressionData())
    {
        // The lift force at the cylinder is very small and not important in this case.
        write_beam_tip_displacement.generateDataBase(Vec2d(1.0e-2, 1.0e-2), Vec2d(1.0e-2, 1.0e-2));
    }
    else
    {
        write_beam_tip_displacement.testResult();
    }
    return 0;
}

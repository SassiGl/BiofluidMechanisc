#include "sphinxsys.h"
using namespace SPH;

//----------------------------------------------------------------------
//	Basic geometry parameters and numerical setup.
//----------------------------------------------------------------------
const Real unit_scale = 0.01;                    // cm to m
const Real diameter = 1.2 * unit_scale;          /**<Inlet and outlet diameter. */
const Real throat_diameter = 0.4 * unit_scale;   /**<Minimum diameter. */
const Real upstream_length = 8.7 * unit_scale;   /**<Length before sudden expansion. */
const Real downstream_length = 8.6 * unit_scale; /**<Length after sudden expansion. */
const Real wall_thickness = 0.24 * unit_scale;   /**<Extension of wall. */
const Real shell_thickness = 0.06 * unit_scale;
const Real resolution_fluid = diameter / Real(20); /**< Global reference resolution. */
const Real resolution_shell = shell_thickness;
const Real inflow_length = 10 * resolution_fluid; /**< Inflow region. */

/** Domain bounds of the system. */
BoundingBox system_domain_bounds(Vec3d(-upstream_length - wall_thickness, -diameter - shell_thickness, -diameter - shell_thickness),
                                 Vec3d(downstream_length + wall_thickness, diameter + shell_thickness, diameter + shell_thickness));
//----------------------------------------------------------------------
//	Global parameters on the fluid properties
//----------------------------------------------------------------------
const Real rho0_f = 1056.0;                                   // blood density
const Real mu_f = 3.5e-3;                                     // blood viscosity
const Real Re = 50;                                           /**< Reynolds number. */
const Real U_f_throat = Re * mu_f / rho0_f / throat_diameter; /**< Average velocity at throat. */
const Real c_f = 20.0 * U_f_throat;                           /**< Speed of sound. */
const Real U_f = U_f_throat * pow(throat_diameter / diameter, 2);
//----------------------------------------------------------------------
//	define geometry of SPH bodies
//----------------------------------------------------------------------
const std::string path_to_fluid_file = "./input/FDA_nozzle_fluid.stl";
class FluidShape : public ComplexShape
{
  public:
    explicit FluidShape(const std::string &shape_name) : ComplexShape(shape_name)
    {
        add<TriangleMeshShapeSTL>(path_to_fluid_file, Vecd::Zero(), 1.0);
    }
};
const Vec3d buffer_halfsize(0.5 * inflow_length, 0.5 * diameter, 0.5 * diameter);
const Vec3d buffer_translation(-upstream_length + 0.5 * inflow_length, 0.0, 0.0);
//----------------------------------------------------------------------
//	Inflow velocity
//----------------------------------------------------------------------
struct InflowVelocity
{
    Real u_ref_, t_ref_;
    AlignedBoxShape &aligned_box_;
    Vecd halfsize_;

    template <class BoundaryConditionType>
    InflowVelocity(BoundaryConditionType &boundary_condition)
        : u_ref_(U_f), t_ref_(0.5),
          aligned_box_(boundary_condition.getAlignedBox()),
          halfsize_(aligned_box_.HalfSize()) {}

    Vecd operator()(Vecd &position, Vecd &velocity)
    {
        Vecd target_velocity = Vecd::Zero();
        Real run_time = GlobalStaticVariables::physical_time_;
        Real u_ave = run_time < t_ref_ ? 0.5 * u_ref_ * (1.0 - cos(Pi * run_time / t_ref_)) : u_ref_;
        Real radius2 = position[1] * position[1] + position[2] * position[2];
        target_velocity[0] = 2.0 * u_ave * (1.0 - radius2 / halfsize_[1] / halfsize_[1]);

        return target_velocity;
    }
};

int main(int ac, char *av[])
{
    //----------------------------------------------------------------------
    //	Build up the environment of a SPHSystem with global controls.
    //----------------------------------------------------------------------
    SPHSystem sph_system(system_domain_bounds, resolution_fluid);
    sph_system.handleCommandlineOptions(ac, av); // handle command line arguments
    IOEnvironment io_environment(sph_system);
    //----------------------------------------------------------------------
    //	Creating body, materials and particles.
    //----------------------------------------------------------------------
    FluidBody fluid_block(sph_system, makeShared<FluidShape>("fluid"));
    fluid_block.defineParticlesAndMaterial<BaseParticles, WeaklyCompressibleFluid>(rho0_f, c_f, mu_f);
    fluid_block.generateParticles<ParticleGeneratorLattice>();

    SolidBody wall_boundary(sph_system, makeShared<DefaultShape>("shell_wall_boundary"));
    wall_boundary.defineAdaptationRatios(1.15, resolution_fluid / resolution_shell);
    wall_boundary.defineParticlesAndMaterial<ShellParticles, SaintVenantKirchhoffSolid>(1.0, 1.0, 0.0); // dummy material parameters
    wall_boundary.generateParticles<ParticleGeneratorReload>(io_environment, wall_boundary.getName());
    //----------------------------------------------------------------------
    //	Define body relation map.
    //	The contact map gives the topological connections between the bodies.
    //	Basically the the range of bodies to build neighbor particle lists.
    //----------------------------------------------------------------------
    InnerRelation fluid_block_inner(fluid_block);
    // Must construct ShellCurvature before ShellContactRelation
    InnerRelation wall_boundary_inner(wall_boundary);
    SimpleDynamics<thin_structure_dynamics::ShellCurvature> shell_curvature(wall_boundary_inner);
    /** Must construct ShellContactRelation first! Otherwise ComplexRelation creates ContactRelation*/
    ShellContactRelation fluid_wall_contact(fluid_block, {&wall_boundary});
    ComplexRelation fluid_block_complex(fluid_block_inner, fluid_wall_contact);
    //----------------------------------------------------------------------
    //	Define the main numerical methods used in the simulation.
    //	Note that there may be data dependence on the constructors of these methods.
    //----------------------------------------------------------------------
    /** Initialize particle acceleration. */
    SimpleDynamics<TimeStepInitialization> initialize_a_fluid_step(fluid_block);
    /** Evaluation of density by summation approach. */
    InteractionWithUpdate<fluid_dynamics::DensitySummationComplex> update_density_by_summation(fluid_block_complex);
    /** Time step size without considering sound wave speed. */
    ReduceDynamics<fluid_dynamics::AdvectionTimeStepSize> get_fluid_advection_time_step_size(fluid_block, 1.5 * U_f);
    /** Time step size with considering sound wave speed. */
    ReduceDynamics<fluid_dynamics::AcousticTimeStepSize> get_fluid_time_step_size(fluid_block);
    /** Pressure relaxation using verlet time stepping. */
    /** Here, we do not use Riemann solver for pressure as the flow is viscous. */
    Dynamics1Level<fluid_dynamics::FluidShellIntegration1stHalfRiemann> pressure_relaxation(fluid_block_complex);
    Dynamics1Level<fluid_dynamics::FluidShellIntegration2ndHalf> density_relaxation(fluid_block_complex);
    /** viscous acceleration and transport velocity correction can be combined because they are independent dynamics. */
    InteractionDynamics<fluid_dynamics::TransportVelocityCorrectionComplex<AllParticles>> transport_correction(fluid_block_complex);
    InteractionDynamics<fluid_dynamics::ViscousAccelerationWithShell> viscous_acceleration(fluid_block_complex);
    /** Computing vorticity in the flow. */
    InteractionDynamics<fluid_dynamics::VorticityInner> compute_vorticity(fluid_block_complex.getInnerRelation());
    /** Inflow boundary condition. */
    BodyAlignedBoxByCell inflow_buffer(
        fluid_block, makeShared<AlignedBoxShape>(Transform(Vec3d(buffer_translation)), buffer_halfsize));
    SimpleDynamics<fluid_dynamics::InflowVelocityCondition<InflowVelocity>> parabolic_inflow(inflow_buffer);
    /** Periodic BCs in x direction. */
    PeriodicConditionUsingCellLinkedList periodic_condition(fluid_block, fluid_block.getBodyShapeBounds(), xAxis);
    /** Wall boundary configuration correction*/
    InteractionDynamics<thin_structure_dynamics::ShellCorrectConfiguration> wall_corrected_configuration(wall_boundary_inner);
    //----------------------------------------------------------------------
    //	Define the methods for I/O operations and observations of the simulation.
    //----------------------------------------------------------------------
    fluid_block.addBodyStateForRecording<Real>("Pressure");
    wall_boundary.addBodyStateForRecording<Real>("MeanCurvature");
    BodyStatesRecordingToVtp write_real_body_states(io_environment, sph_system.real_bodies_);
    //----------------------------------------------------------------------
    //	Prepare the simulation with cell linked list, configuration
    //	and case specified initial condition if necessary.
    //----------------------------------------------------------------------
    /** initialize cell linked lists for all bodies. */
    sph_system.initializeSystemCellLinkedLists();
    /** periodic condition applied after the mesh cell linked list build up
     * but before the configuration build up. */
    periodic_condition.update_cell_linked_list_.exec();
    /** initialize configurations for all bodies. */
    sph_system.initializeSystemConfigurations();
    /** initial curvature*/
    // wall_corrected_configuration.exec();
    // shell_curvature.compute_initial_curvature();
    // fluid_wall_contact.updateConfiguration();
    //----------------------------------------------------------------------
    //	Setup computing and initial conditions.
    //----------------------------------------------------------------------
    size_t number_of_iterations = 0;
    int screen_output_interval = 10;
    Real end_time = 0.5;
    Real output_interval = end_time / 100.0;
    //----------------------------------------------------------------------
    //	Statistics for CPU time
    //----------------------------------------------------------------------
    TickCount t1 = TickCount::now();
    TimeInterval interval;
    //----------------------------------------------------------------------
    //	First output before the main loop.
    //----------------------------------------------------------------------
    write_real_body_states.writeToFile();
    //----------------------------------------------------------------------
    //	Main loop starts here.
    //----------------------------------------------------------------------
    while (GlobalStaticVariables::physical_time_ < end_time)
    {
        Real integration_time = 0.0;
        /** Integrate time (loop) until the next output time. */
        while (integration_time < output_interval)
        {
            initialize_a_fluid_step.exec();
            Real Dt = get_fluid_advection_time_step_size.exec();
            update_density_by_summation.exec();
            viscous_acceleration.exec();
            transport_correction.exec();

            size_t inner_ite_dt = 0;
            Real relaxation_time = 0.0;
            while (relaxation_time < Dt)
            {
                Real dt = SMIN(get_fluid_time_step_size.exec(), Dt);
                /** Fluid pressure relaxation */
                pressure_relaxation.exec(dt);
                /** velocity */
                parabolic_inflow.exec();
                /** Fluid density relaxation */
                density_relaxation.exec(dt);

                relaxation_time += dt;
                integration_time += dt;
                GlobalStaticVariables::physical_time_ += dt;

                inner_ite_dt++;
            }

            if (number_of_iterations % screen_output_interval == 0)
            {
                std::cout << std::fixed << std::setprecision(9) << "N=" << number_of_iterations << "	Time = "
                          << GlobalStaticVariables::physical_time_
                          << "	Dt = " << Dt << "	Dt / dt = " << inner_ite_dt << "\n";
            }
            number_of_iterations++;

            /** fluid block configuration and periodic condition. */
            periodic_condition.bounding_.exec();

            fluid_block.updateCellLinkedListWithParticleSort(100);
            periodic_condition.update_cell_linked_list_.exec();
            fluid_block_complex.updateConfiguration();
        }

        TickCount t2 = TickCount::now();
        /** write run-time observation into file */
        compute_vorticity.exec();
        write_real_body_states.writeToFile();
        TickCount t3 = TickCount::now();
        interval += t3 - t2;
    }
    TickCount t4 = TickCount::now();

    TimeInterval tt;
    tt = t4 - t1 - interval;
    std::cout << "Total wall time for computation: " << tt.seconds() << " seconds." << std::endl;

    return 0;
}

/**
 * @file 	2d_turbulent_channel.cpp
 * @brief 	2D_turbulent_channel flow with K-Epsilon two equations RANS model.
 * @details This is the one of the basic test cases.
 * @author 	Xiangyu Hu
 */
#include "sphinxsys.h"
#include "2d_turbulent_channel.h"

#include "k-epsilon_turbulent_model_complex.h"
#include "k-epsilon_turbulent_model_complex.hpp"
using namespace SPH;

int main(int ac, char* av[])
{
	//----------------------------------------------------------------------
	//	Build up the environment of a SPHSystem with global controls.
	//----------------------------------------------------------------------
	SPHSystem system(system_domain_bounds, resolution_ref);
	system.handleCommandlineOptions(ac, av);
	IOEnvironment io_environment(system);
	//----------------------------------------------------------------------
	//	Creating body, materials and particles.cd
	//----------------------------------------------------------------------
	FluidBody water_block(system, makeShared<WaterBlock>("WaterBody"));
	water_block.defineParticlesAndMaterial<FluidParticles, WeaklyCompressibleFluid>(rho0_f, c_f, mu_f);
	water_block.generateParticles<ParticleGeneratorLattice>();

	SolidBody wall_boundary(system, makeShared<WallBoundary>("Wall"));
	wall_boundary.defineParticlesAndMaterial<SolidParticles, Solid>();
	wall_boundary.generateParticles<ParticleGeneratorLattice>();

	ObserverBody fluid_observer(system, "FluidObserver");
	for (int j = 0; j < num_observer_points_x; ++j)
	{
		for (int i = 0; i < num_observer_points; ++i)
		{
			observation_locations.push_back(Vecd(x_observe_start + j * observe_spacing_x,
				i * observe_spacing + 0.5 * resolution_ref));
		}
	}
	fluid_observer.generateParticles<ObserverParticleGenerator>(observation_locations);
	//----------------------------------------------------------------------
	//	Define body relation map.
	//	The contact map gives the topological connections between the bodies.
	//	Basically the the range of bodies to build neighbor particle lists.
	//----------------------------------------------------------------------
	InnerRelation water_block_inner(water_block);
	ComplexRelation water_block_complex_relation(water_block_inner, { &wall_boundary });
	ContactRelation fluid_observer_contact(fluid_observer, { &water_block });
	//----------------------------------------------------------------------
	//	Define the main numerical methods used in the simulation.
	//	Note that there may be data dependence on the constructors of these methods.
	//----------------------------------------------------------------------
	
	//Attention! the original one does not use Riemann solver for pressure
	Dynamics1Level<fluid_dynamics::Integration1stHalfRiemannWithWall> pressure_relaxation(water_block_complex_relation);
	//Attention! the original one does use Riemann solver for density
	Dynamics1Level<fluid_dynamics::Integration2ndHalfWithWall> density_relaxation(water_block_complex_relation);
	
	//Turbulent model 
	InteractionWithUpdate<fluid_dynamics::K_TurtbulentModelWithWall, SequencedPolicy> k_equation_relaxation(water_block_complex_relation);
	InteractionWithUpdate<fluid_dynamics::E_TurtbulentModelWithWall, SequencedPolicy> epsilon_equation_relaxation(water_block_complex_relation);
	InteractionDynamics<fluid_dynamics::TurbulentKineticEnergyAccelerationWithWall, SequencedPolicy> turbulent_kinetic_energy_acceleration(water_block_complex_relation);


	InteractionDynamics<fluid_dynamics::ViscousAccelerationWithWall> viscous_acceleration(water_block_complex_relation);
	
	
	
	
	
	InteractionDynamics<fluid_dynamics::TransportVelocityCorrectionComplex> transport_velocity_correction(water_block_complex_relation);
	InteractionWithUpdate<fluid_dynamics::SpatialTemporalFreeSurfaceIdentificationComplex>
		inlet_outlet_surface_particle_indicator(water_block_complex_relation);
	InteractionWithUpdate<fluid_dynamics::DensitySummationFreeStreamComplex> update_density_by_summation(water_block_complex_relation);
	water_block.addBodyStateForRecording<Real>("Pressure");		   // output for debug
	water_block.addBodyStateForRecording<int>("SurfaceIndicator"); // output for debug

	/** Define the external force. */
	//TimeDependentAcceleration gravity(Vec2d(0.0, 0.0)); 
	//SharedPtr<TimeDependentAcceleration> gravity_ptr = makeShared<TimeDependentAcceleration>(Vecd(0.0, 0.0));
	SimpleDynamics<TimeStepInitialization> initialize_a_fluid_step(water_block);
	ReduceDynamics<fluid_dynamics::AdvectionTimeStepSize> get_fluid_advection_time_step_size(water_block, U_f);
	ReduceDynamics<fluid_dynamics::AcousticTimeStepSize> get_fluid_time_step_size(water_block);
	SimpleDynamics<NormalDirectionFromBodyShape> wall_boundary_normal_direction(wall_boundary);

	BodyAlignedBoxByParticle emitter(
		water_block, makeShared<AlignedBoxShape>(Transform2d(Vec2d(emitter_translation)), emitter_halfsize));
	SimpleDynamics<fluid_dynamics::EmitterInflowInjection> emitter_inflow_injection(emitter, 10, 0);

	BodyAlignedBoxByCell emitter_buffer(
		water_block, makeShared<AlignedBoxShape>(Transform2d(Vec2d(inlet_buffer_translation)), inlet_buffer_halfsize));
	SimpleDynamics<fluid_dynamics::InflowVelocityCondition<InflowVelocity>> emitter_buffer_inflow_condition(emitter_buffer);

	Vec2d disposer_up_halfsize = Vec2d(0.5 * BW, 0.55 * DH);
	Vec2d disposer_up_translation = Vec2d(DL - BW, -0.05 * DH) + disposer_up_halfsize;
	BodyAlignedBoxByCell disposer_up(
		water_block, makeShared<AlignedBoxShape>(Transform2d(Vec2d(disposer_up_translation)), disposer_up_halfsize));
	SimpleDynamics<fluid_dynamics::DisposerOutflowDeletion> disposer_up_outflow_deletion(disposer_up, xAxis);
;
	//----------------------------------------------------------------------
	//	Define the methods for I/O operations and observations of the simulation.
	//----------------------------------------------------------------------
	BodyStatesRecordingToVtp write_body_states(io_environment, system.real_bodies_);
	//----------------------------------------------------------------------
	//	Prepare the simulation with cell linked list, configuration
	//	and case specified initial condition if necessary.
	//----------------------------------------------------------------------
	system.initializeSystemCellLinkedLists();
	system.initializeSystemConfigurations();
	wall_boundary_normal_direction.exec();
	//----------------------------------------------------------------------
	//	Setup computing and initial conditions.
	//----------------------------------------------------------------------
	size_t number_of_iterations = system.RestartStep();
	int screen_output_interval = 100;
	Real end_time = 100.0;
	Real output_interval = end_time / 200.0; /**< Time stamps for output of body states. */
	Real dt = 0.0;							 /**< Default acoustic time step sizes. */
	//----------------------------------------------------------------------
	//	Statistics for CPU time
	//----------------------------------------------------------------------
	TickCount t1 = TickCount::now();
	TimeInterval interval;
	//----------------------------------------------------------------------
	//	First output before the main loop.
	//----------------------------------------------------------------------
	write_body_states.writeToFile();
	//----------------------------------------------------------------------------------------------------
	//	Main loop starts here.
	//----------------------------------------------------------------------------------------------------
	while (GlobalStaticVariables::physical_time_ < end_time)
	{
		Real integration_time = 0.0;
		/** Integrate time (loop) until the next output time. */
		while (integration_time < output_interval)
		{
			initialize_a_fluid_step.exec();
			Real Dt = get_fluid_advection_time_step_size.exec();
			inlet_outlet_surface_particle_indicator.exec();
			update_density_by_summation.exec();
			viscous_acceleration.exec();
			transport_velocity_correction.exec();

			/** Dynamics including pressure relaxation. */
			Real relaxation_time = 0.0;
			while (relaxation_time < Dt)
			{
				dt = SMIN(get_fluid_time_step_size.exec(), Dt - relaxation_time);
				
				turbulent_kinetic_energy_acceleration.exec();
				
				pressure_relaxation.exec(dt);
				emitter_buffer_inflow_condition.exec();
				density_relaxation.exec(dt);

				k_equation_relaxation.exec(dt);
				epsilon_equation_relaxation.exec(dt);

				relaxation_time += dt;
				integration_time += dt;
				GlobalStaticVariables::physical_time_ += dt;
			}

			if (number_of_iterations % screen_output_interval == 0)
			{
				std::cout << std::fixed << std::setprecision(9) << "N=" << number_of_iterations << "	Time = "
					<< GlobalStaticVariables::physical_time_
					<< "	Dt = " << Dt << "	dt = " << dt << "\n";
			}
			number_of_iterations++;

			/** inflow injection*/
			emitter_inflow_injection.exec();
			disposer_up_outflow_deletion.exec();

			/** Update cell linked list and configuration. */
			water_block.updateCellLinkedListWithParticleSort(100);
			water_block_complex_relation.updateConfiguration();
		}

		TickCount t2 = TickCount::now();
		write_body_states.writeToFile();
		TickCount t3 = TickCount::now();
		interval += t3 - t2;
	}
	TickCount t4 = TickCount::now();

	TimeInterval tt;
	tt = t4 - t1 - interval;
	std::cout << "Total wall time for computation: " << tt.seconds()
		<< " seconds." << std::endl;

	return 0;
}

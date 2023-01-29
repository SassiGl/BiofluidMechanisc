/**
 * @file 	airfoil_2d.cpp
 * @brief 	This is the test of using levelset to generate body fitted SPH particles.
 * @details	We use this case to test the particle generation and relaxation with a complex geometry (2D).
 *			Before the particles are generated, we clean the sharp corners and other unresolvable surfaces.
 * @author 	Yongchuan Yu and Xiangyu Hu
 */
#include "sphinxsys.h"
using namespace SPH;
//----------------------------------------------------------------------
//	Set the file path to the data file.
//----------------------------------------------------------------------
std::string airfoil_flap_front = "./input/airfoil_flap_front.dat";
std::string airfoil_wing = "./input/airfoil_wing.dat";
std::string airfoil_flap_rear = "./input/airfoil_flap_rear.dat";
//----------------------------------------------------------------------
//	Basic geometry parameters and numerical setup.
//----------------------------------------------------------------------
Real DL = 1.25;				/**< airfoil length rear part. */
Real DL1 = 0.25;			/**< airfoil length front part. */
Real DH = 0.25;				/**< airfoil height. */
Real resolution_ref = 0.02; /**< Reference resolution. */
BoundingBox system_domain_bounds(Vec2d(-DL1, -DH), Vec2d(DL, DH));
//----------------------------------------------------------------------
//	import model as a complex shape
//----------------------------------------------------------------------
class ImportModel : public MultiPolygonShape
{
public:
	explicit ImportModel(const std::string &import_model_name) : MultiPolygonShape(import_model_name)
	{
		multi_polygon_.addAPolygonFromFile(airfoil_flap_front, ShapeBooleanOps::add);
		multi_polygon_.addAPolygonFromFile(airfoil_wing, ShapeBooleanOps::add);
		multi_polygon_.addAPolygonFromFile(airfoil_flap_rear, ShapeBooleanOps::add);
	}
};
//----------------------------------------------------------------------
//	Main program starts here.
//----------------------------------------------------------------------
int main(int ac, char *av[])
{
	//----------------------------------------------------------------------
	//	Build up -- a SPHSystem
	//----------------------------------------------------------------------
	SPHSystem system(system_domain_bounds, resolution_ref);
	system.setRunParticleRelaxation(true); // tag to run particle relaxation when no commandline option
#ifdef BOOST_AVAILABLE
	system.handleCommandlineOptions(ac, av);
#endif
	IOEnvironment io_environment(system);
	//----------------------------------------------------------------------
	//	Creating body, materials and particles.
	//----------------------------------------------------------------------
	RealBody airfoil(system, makeShared<ImportModel>("AirFoil"));
	airfoil.defineAdaptation<ParticleRefinementNearSurface>(1.15, 1.0, 3);
	airfoil.defineBodyLevelSetShape()->cleanLevelSet()->writeLevelSet(io_environment);
	airfoil.defineParticlesAndMaterial();
	airfoil.generateParticles<ParticleGeneratorMultiResolution>();
	airfoil.addBodyStateForRecording<Real>("SmoothingLengthRatio");
	//----------------------------------------------------------------------
	//	Define body relation map.
	//	The contact map gives the topological connections between the bodies,
	//	basically, in the the range of bodies to build neighbor particle lists.
	//----------------------------------------------------------------------
	AdaptiveInnerRelation airfoil_inner(airfoil);
	//----------------------------------------------------------------------
	//	Methods used for particle relaxation.
	//----------------------------------------------------------------------
	SimpleDynamics<RandomizeParticlePosition> random_airfoil_particles(airfoil);
	relax_dynamics::RelaxationStepInner relaxation_step_inner(airfoil_inner, true);
	SimpleDynamics<relax_dynamics::UpdateSmoothingLengthRatioByShape> update_smoothing_length_ratio(airfoil);
	//----------------------------------------------------------------------
	//	Define outputs functions.
	//----------------------------------------------------------------------
	BodyStatesRecordingToVtp airfoil_recording_to_vtp(io_environment, {&airfoil});
	MeshRecordingToPlt cell_linked_list_recording(io_environment, airfoil.getCellLinkedList());
	RegressionTestDynamicTimeWarping<ReducedQuantityRecording<ReduceAverage<Summation2Norm<Vecd>>>>
		airfoil_residue_force_recording(io_environment, airfoil, "Acceleration");
	//----------------------------------------------------------------------
	//	Prepare the simulation with cell linked list, configuration
	//	and case specified initial condition if necessary.
	//----------------------------------------------------------------------
	random_airfoil_particles.parallel_exec(0.25);
	relaxation_step_inner.SurfaceBounding().parallel_exec();
	update_smoothing_length_ratio.parallel_exec();
	system.updateSystemCellLinkedLists();
	system.updateSystemRelations();
	//----------------------------------------------------------------------
	//	First output before the simulation.
	//----------------------------------------------------------------------
	airfoil_recording_to_vtp.writeToFileByStep();
	cell_linked_list_recording.writeToFileByStep();
	//----------------------------------------------------------------------
	//	Particle relaxation time stepping start here.
	//----------------------------------------------------------------------
	while (system.TotalSteps() < 2000)
	{
		update_smoothing_length_ratio.parallel_exec();
		relaxation_step_inner.parallel_exec();
		system.accumulateTotalSteps();

		airfoil_recording_to_vtp.writeToFileByStep();
		airfoil_residue_force_recording.writeToFileByStep();
		system.monitorSteps("AirFoilResidueForce", airfoil_residue_force_recording.ResultValue());

		system.updateSystemCellLinkedLists();
		system.updateSystemRelations();
	}
	std::cout << "The physics relaxation process finished !" << std::endl;

	if (system.generate_regression_data_)
	{
		airfoil_residue_force_recording.generateDataBase(1.0e-3);
	}
	else
	{
		airfoil_residue_force_recording.newResultTest();
	}

	return 0;
}

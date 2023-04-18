#ifndef PARTICLE_DYNAMICS_DIFFUSION_REACTION_WITH_BOUNDARY_HPP
#define PARTICLE_DYNAMICS_DIFFUSION_REACTION_WITH_BOUNDARY_HPP

#include "particle_dynamics_diffusion_reaction_with_boundary.h"

namespace SPH
{
	//=================================================================================================//
	template <class DiffusionReactionParticlesType>
	DiffusionReactionInitialConditionWithBoundary<DiffusionReactionParticlesType>::
		DiffusionReactionInitialConditionWithBoundary(SPHBody& sph_body)
		: DiffusionReactionInitialCondition<DiffusionReactionParticlesType>(sph_body),
	heat_flux_(this->particles_->heat_flux_), convection_(this->particles_->convection_), T_infinity_(this->particles_->T_infinity_) {}
	//=================================================================================================//
	template <class DiffusionReactionParticlesType, class ContactDiffusionReactionParticlesType>
	RelaxationOfAllDiffusionSpeciesWithDirichlet<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>::
		RelaxationOfAllDiffusionSpeciesWithDirichlet(ComplexRelation& complex_relation)
		: RelaxationOfAllDiffusionSpeciesInner<DiffusionReactionParticlesType>(complex_relation.getInnerRelation()),
		DiffusionReactionContactData<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>(complex_relation.getContactRelation())
	{
		contact_gradient_species_.resize(this->contact_particles_.size());

		StdVec<std::string>& all_species_names = this->particles_->AllSpeciesNames();
		for (size_t m = 0; m < this->all_diffusions_.size(); ++m)
		{
			size_t l = this->all_diffusions_[m]->gradient_species_index_;
			std::string& inner_species_name_m = all_species_names[l];
			for (size_t k = 0; k != this->contact_particles_.size(); ++k)
			{
				size_t contact_species_index_k_m = this->contact_particles_[k]->AllSpeciesIndexMap()[inner_species_name_m];
				StdVec<std::string>& all_contact_species_names_k = this->contact_particles_[k]->AllSpeciesNames();
				std::string& contact_species_name_k_m = all_contact_species_names_k[contact_species_index_k_m];

				if (inner_species_name_m != contact_species_name_k_m)
				{
					std::cout << "\n Error: inner species '" << inner_species_name_m
						<< "' and contact species '" << contact_species_name_k_m << "' not match! " << std::endl;
					std::cout << __FILE__ << ':' << __LINE__ << std::endl;
					exit(1);
				}
				StdVec<StdLargeVec<Real>>& all_contact_species_k = this->contact_particles_[k]->all_species_;

				contact_gradient_species_[k].push_back(&all_contact_species_k[contact_species_index_k_m]);
			}
		}
	}
	//=================================================================================================//
	template <class DiffusionReactionParticlesType, class ContactDiffusionReactionParticlesType>
	void RelaxationOfAllDiffusionSpeciesWithDirichlet<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>::
		getDiffusionChangeRateWithDirichlet(size_t particle_i, size_t particle_j, Vecd& e_ij,
			Real surface_area_ij, const StdVec<StdLargeVec<Real>*>& gradient_species_k)
	{
		for (size_t m = 0; m < this->all_diffusions_.size(); ++m)
		{
			Real diff_coff_ij =
				this->all_diffusions_[m]->getInterParticleDiffusionCoff(particle_i, particle_j, e_ij);
			Real phi_ij = (*this->diffusion_species_[m])[particle_i] - (*gradient_species_k[m])[particle_j];
			(*this->diffusion_dt_[m])[particle_i] += diff_coff_ij * phi_ij * surface_area_ij;
		}
	}
	//=================================================================================================//
	template <class DiffusionReactionParticlesType, class ContactDiffusionReactionParticlesType>
	void RelaxationOfAllDiffusionSpeciesWithDirichlet<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>::
		interaction(size_t index_i, Real dt)
	{
		RelaxationOfAllDiffusionSpeciesInner<DiffusionReactionParticlesType>::interaction(index_i, dt);
		DiffusionReactionParticlesType* particles = this->particles_;

		for (size_t k = 0; k < this->contact_configuration_.size(); ++k)
		{
			StdVec<StdLargeVec<Real>*>& gradient_species_k = this->contact_gradient_species_[k];

			Neighborhood& contact_neighborhood = (*this->contact_configuration_[k])[index_i];
			for (size_t n = 0; n != contact_neighborhood.current_size_; ++n)
			{
				size_t index_j = contact_neighborhood.j_[n];
				Real r_ij_ = contact_neighborhood.r_ij_[n];
				Real dW_ijV_j_ = contact_neighborhood.dW_ijV_j_[n];
				Vecd& e_ij = contact_neighborhood.e_ij_[n];

				const Vecd& grad_ijV_j = particles->getKernelGradient(index_i, index_j, dW_ijV_j_, e_ij);
				Real area_ij = 2.0 * grad_ijV_j.dot(e_ij) / r_ij_;
				getDiffusionChangeRateWithDirichlet(index_i, index_j, e_ij, area_ij, gradient_species_k);
			}
		}
	}
	//=================================================================================================//
	template <class DiffusionReactionParticlesType, class ContactDiffusionReactionParticlesType>
	RelaxationOfAllDiffusionSpeciesWithNeumann<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>::
		RelaxationOfAllDiffusionSpeciesWithNeumann(ComplexRelation& complex_relation)
		: RelaxationOfAllDiffusionSpeciesWithDirichlet<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>(complex_relation),
		n_(this->particles_->normal_vector_)
	{
		//contact_gradient_species_.resize(this->contact_particles_.size());
		contact_Vol_.resize(this->contact_particles_.size());
		contact_heat_flux_.resize(this->contact_particles_.size());
		contact_n_.resize(this->contact_particles_.size());

		for (size_t m = 0; m < this->all_diffusions_.size(); ++m)
		{
			for (size_t k = 0; k != this->contact_particles_.size(); ++k)
			{

				contact_n_.push_back(&this->contact_particles_[k]->normal_vector_);
				contact_Vol_.push_back(&this->contact_particles_[k]->Vol_);
				contact_heat_flux_.push_back(&this->contact_particles_[k]->heat_flux_);
			}
		}
	}
	//=================================================================================================//
	template <class DiffusionReactionParticlesType, class ContactDiffusionReactionParticlesType>
	void RelaxationOfAllDiffusionSpeciesWithNeumann<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>::
		getDiffusionChangeRateWithNeumann(size_t particle_i, size_t particle_j,
			Real surface_area_ij_Neumann, StdLargeVec<Real>& heat_flux_k)
	{
		for (size_t m = 0; m < this->all_diffusions_.size(); ++m)
		{
			(*this->diffusion_dt_[m])[particle_i] += surface_area_ij_Neumann * heat_flux_k[particle_j];
		}
	}
	//=================================================================================================//
	template <class DiffusionReactionParticlesType, class ContactDiffusionReactionParticlesType>
	void RelaxationOfAllDiffusionSpeciesWithNeumann<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>::
		interaction(size_t index_i, Real dt)
	{
		RelaxationOfAllDiffusionSpeciesInner<DiffusionReactionParticlesType>::interaction(index_i, dt);
		DiffusionReactionParticlesType* particles = this->particles_;

		for (size_t k = 0; k < this->contact_configuration_.size(); ++k)
		{
			StdVec<StdLargeVec<Real>*>& gradient_species_k = this->contact_gradient_species_[k];

			StdLargeVec<Real>& heat_flux_ = *(contact_heat_flux_[k]);
			StdLargeVec<Vecd>& n_k = *(contact_n_[k]);

			Neighborhood& contact_neighborhood = (*this->contact_configuration_[k])[index_i];
			for (size_t n = 0; n != contact_neighborhood.current_size_; ++n)
			{
				size_t index_j = contact_neighborhood.j_[n];
				Real r_ij_ = contact_neighborhood.r_ij_[n];
				Real dW_ijV_j_ = contact_neighborhood.dW_ijV_j_[n];
				Vecd& e_ij = contact_neighborhood.e_ij_[n];

				const Vecd& grad_ijV_j = particles->getKernelGradient(index_i, index_j, dW_ijV_j_, e_ij);
				Vecd n_ij = n_[index_i] - n_k[index_j];
				Real area_ij_Neumann = grad_ijV_j.dot(n_ij);
				getDiffusionChangeRateWithNeumann(index_i, index_j, area_ij_Neumann, heat_flux_);
			}
		}
	}
	//=================================================================================================//
	template <class DiffusionReactionParticlesType, class ContactDiffusionReactionParticlesType>
	RelaxationOfAllDiffusionSpeciesWithRobin<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>::
		RelaxationOfAllDiffusionSpeciesWithRobin(ComplexRelation& complex_relation)
		: RelaxationOfAllDiffusionSpeciesWithDirichlet<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>(complex_relation),
		n_(this->particles_->normal_vector_)
	{
		//contact_gradient_species_.resize(this->contact_particles_.size());
		contact_Vol_.resize(this->contact_particles_.size());
		contact_convection_.resize(this->contact_particles_.size());
		contact_T_infinity_.resize(this->contact_particles_.size());
		contact_n_.resize(this->contact_particles_.size());


		for (size_t m = 0; m < this->all_diffusions_.size(); ++m)
		{
			for (size_t k = 0; k != this->contact_particles_.size(); ++k)
			{
				contact_n_.push_back(&this->contact_particles_[k]->normal_vector_);
				contact_Vol_.push_back(&this->contact_particles_[k]->Vol_);
				contact_convection_.push_back(&this->contact_particles_[k]->convection_);
				contact_T_infinity_.push_back(&this->contact_particles_[k]->T_infinity_);
			}
		}
	}
	//=================================================================================================//
	template <class DiffusionReactionParticlesType, class ContactDiffusionReactionParticlesType>
	void RelaxationOfAllDiffusionSpeciesWithRobin<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>::
		getDiffusionChangeRateWithRobin(size_t particle_i, size_t particle_j,
			Real surface_area_ij_Robin, StdLargeVec<Real>& convection_k, StdLargeVec<Real>& T_infinity_k)
	{
		for (size_t m = 0; m < this->all_diffusions_.size(); ++m)
		{
			Real phi_ij = T_infinity_k[particle_j] - (*this->diffusion_species_[m])[particle_i];
			(*this->diffusion_dt_[m])[particle_i] += convection_k[particle_j] * phi_ij * surface_area_ij_Robin;
		}
	}
	//=================================================================================================//
	template <class DiffusionReactionParticlesType, class ContactDiffusionReactionParticlesType>
	void RelaxationOfAllDiffusionSpeciesWithRobin<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>::
		interaction(size_t index_i, Real dt)
	{
		RelaxationOfAllDiffusionSpeciesInner<DiffusionReactionParticlesType>::interaction(index_i, dt);
		DiffusionReactionParticlesType* particles = this->particles_;

		for (size_t k = 0; k < this->contact_configuration_.size(); ++k)
		{
			StdVec<StdLargeVec<Real>*>& gradient_species_k = this->contact_gradient_species_[k];

			StdLargeVec<Vecd>& n_k = *(contact_n_[k]);

			StdLargeVec<Real>& convection_ = *(contact_convection_[k]);
			StdLargeVec<Real>& T_infinity_ = *(contact_T_infinity_[k]);

			Neighborhood& contact_neighborhood = (*this->contact_configuration_[k])[index_i];
			for (size_t n = 0; n != contact_neighborhood.current_size_; ++n)
			{
				size_t index_j = contact_neighborhood.j_[n];
				Real r_ij_ = contact_neighborhood.r_ij_[n];
				Real dW_ijV_j_ = contact_neighborhood.dW_ijV_j_[n];
				Vecd& e_ij = contact_neighborhood.e_ij_[n];

				const Vecd& grad_ijV_j = particles->getKernelGradient(index_i, index_j, dW_ijV_j_, e_ij);
				Vecd n_ij = n_[index_i] - n_k[index_j];
				Real area_ij_Robin = grad_ijV_j.dot(n_ij);
				getDiffusionChangeRateWithRobin(index_i, index_j, area_ij_Robin, convection_, T_infinity_);
			}
		}
	}
	//=================================================================================================//
	template <class DiffusionReactionParticlesType, class ContactDiffusionReactionParticlesType>
	UpdateUnitVectorNormalToBoundary<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>::
		UpdateUnitVectorNormalToBoundary(ComplexRelation& complex_relation)
		: LocalDynamics(complex_relation.getInnerRelation().getSPHBody()),
		DiffusionReactionInnerData<DiffusionReactionParticlesType>(complex_relation.getInnerRelation()),
		DiffusionReactionContactData<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>(complex_relation.getContactRelation()),
		normal_vector_(this->particles_->normal_vector_)
	{}
	//=================================================================================================//
	template <class DiffusionReactionParticlesType, class ContactDiffusionReactionParticlesType>
	void UpdateUnitVectorNormalToBoundary<DiffusionReactionParticlesType, ContactDiffusionReactionParticlesType>::
		interaction(size_t index_i, Real dt)
	{
		for (size_t k = 0; k != this->contact_configuration_.size(); ++k)
		{
			Neighborhood& contact_neighborhood = (*this->contact_configuration_[k])[index_i];
			for (size_t n = 0; n != contact_neighborhood.current_size_; ++n)
			{
				Real& dW_ijV_j_ = contact_neighborhood.dW_ijV_j_[n];
				Vecd& e_ij_ = contact_neighborhood.e_ij_[n];
				normal_vector_[index_i] += dW_ijV_j_ * e_ij_;
			}
		}
		normal_vector_[index_i] = normal_vector_[index_i] / (normal_vector_[index_i].norm() + TinyReal);
	}
}
#endif // PARTICLE_DYNAMICS_DIFFUSION_REACTION_WITH_BOUNDARY_HPP
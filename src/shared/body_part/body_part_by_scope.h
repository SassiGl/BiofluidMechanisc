/* ------------------------------------------------------------------------- *
 *                                SPHinXsys                                  *
 * ------------------------------------------------------------------------- *
 * SPHinXsys (pronunciation: s'finksis) is an acronym from Smoothed Particle *
 * Hydrodynamics for industrial compleX systems. It provides C++ APIs for    *
 * physical accurate simulation and aims to model coupled industrial dynamic *
 * systems including fluid, solid, multi-body dynamics and beyond with SPH   *
 * (smoothed particle hydrodynamics), a meshless computational method using  *
 * particle discretization.                                                  *
 *                                                                           *
 * SPHinXsys is partially funded by German Research Foundation               *
 * (Deutsche Forschungsgemeinschaft) DFG HU1527/6-1, HU1527/10-1,            *
 *  HU1527/12-1 and HU1527/12-4.                                             *
 *                                                                           *
 * Portions copyright (c) 2017-2023 Technical University of Munich and       *
 * the authors' affiliations.                                                *
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may   *
 * not use this file except in compliance with the License. You may obtain a *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.        *
 *                                                                           *
 * ------------------------------------------------------------------------- */
/**
 * @file 	body_part_by_scope.h
 * @brief 	This is the base classes of body parts.
 * @details	There two main type of body parts. One is part by particle.
 * @author	Xiangyu Hu
 */

#ifndef BODY_PART_BY_SCOPE_H
#define BODY_PART_BY_SCOPE_H

#include "base_body.h"

namespace SPH
{
template <class RangeType, class ScopeType>
class ParticleRange : public RangeType, public ScopeType
{
  public:
    template <typename... Args>
    ParticleRange(const ScopeType &scope, Args &&... args)
        : RangeType(std::forward<Args>(args)...),
          ScopeType(scope){};
};

template <typename ParticleScope>
class BodyPartByScope : public BodyPart
{
  public:
    BodyPartByScope(SPHBody &sph_body, std::string body_part_name)
        : BodyPart(sph_body, body_part_name),
          particle_scope_(&sph_body.getBaseParticles()){};

    template <typename Range>
    ParticleRange<Range, ParticleScope> LoopRange()
    {
        return ParticleRange<Range, ParticleScope>(particle_scope_, sph_body_.LoopRange());
    };

  protected:
    ParticleScope particle_scope_;
};
} // namespace SPH
#endif // BODY_PART_BY_SCOPE_H

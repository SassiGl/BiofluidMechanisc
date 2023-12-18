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
 * @file    memory_transfer.h
 * @brief   TBD.
 * @author  Alberto Guarnieri and Xiangyu Hu
 */
#ifndef SPHINXSYS_MEMORY_TRANSFER_H
#define SPHINXSYS_MEMORY_TRANSFER_H

#include "execution_event.h"
#include "execution_instance.h"

namespace SPH
{
/* SYCL memory transfer utilities */
template <class T>
inline T *allocateDeviceData(std::size_t size)
{
    return sycl::malloc_device<T>(size, execution::execution_instance.getQueue());
}

template <class T>
inline T *allocateSharedData(std::size_t size)
{
    return sycl::malloc_shared<T>(size, execution::execution_instance.getQueue());
}

template <class T>
inline void freeDeviceData(T *device_mem)
{
    sycl::free(device_mem, execution::execution_instance.getQueue());
}

} // namespace SPH

#endif // SPHINXSYS_MEMORY_TRANSFER_H

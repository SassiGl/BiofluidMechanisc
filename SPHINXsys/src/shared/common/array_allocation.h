/* -------------------------------------------------------------------------*
*								SPHinXsys									*
* --------------------------------------------------------------------------*
* SPHinXsys (pronunciation: s'finksis) is an acronym from Smoothed Particle	*
* Hydrodynamics for industrial compleX systems. It provides C++ APIs for	*
* physical accurate simulation and aims to model coupled industrial dynamic *
* systems including fluid, solid, multi-body dynamics and beyond with SPH	*
* (smoothed particle hydrodynamics), a meshless computational method using	*
* particle discretization.													*
*																			*
* SPHinXsys is partially funded by German Research Foundation				*
* (Deutsche Forschungsgemeinschaft) DFG HU1527/6-1, HU1527/10-1				*
* and HU1527/12-1.															*
*                                                                           *
* Portions copyright (c) 2017-2020 Technical University of Munich and		*
* the authors' affiliations.												*
*                                                                           *
* Licensed under the Apache License, Version 2.0 (the "License"); you may   *
* not use this file except in compliance with the License. You may obtain a *
* copy of the License at http://www.apache.org/licenses/LICENSE-2.0.        *
*                                                                           *
* --------------------------------------------------------------------------*/
#ifndef ARRAY_ALLOCATION_H
#define ARRAY_ALLOCATION_H

#include "small_vectors.h"

namespace SPH {
	//-------------------------------------------------------------------------------------------------
	//Allocate and deallocate 3d array
	//-------------------------------------------------------------------------------------------------
	template<class T>
	void Allocate3dArray(T*** &matrix, Vec3u res)
	{
		matrix = new T**[res[0]];
		for (size_t i = 0; i < res[0]; i++) {
			matrix[i] = new T*[res[1]];
			for (size_t j = 0; j < res[1]; j++) {
				matrix[i][j] = new T[res[2]];
			}
		}
	}

	template<class T>
	void Delete3dArray(T*** matrix, Vec3u res)
	{
		for (size_t i = 0; i < res[0]; i++) {
			for (size_t j = 0; j < res[1]; j++) {
				delete[] matrix[i][j];
			}
			delete[] matrix[i];
		}
		delete[] matrix;
	}
	//-------------------------------------------------------------------------------------------------
	//							Allocate 2d array
	//-------------------------------------------------------------------------------------------------
	template<class T>
	void Allocate2dArray(T** &matrix, Vec2u res)
	{
		matrix = new T*[res[0]];
		for (size_t i = 0; i < res[0]; i++) {
			matrix[i] = new T[res[1]];
		}
	}
	template<class T>
	void Delete2dArray(T** matrix, Vec2u res)
	{
		for (size_t i = 0; i < res[0]; i++) {
			delete[] matrix[i];
		}
		delete[] matrix;
	}

	//-------------------------------------------------------------------------------------------------
	//							Allocate 1d array
	//-------------------------------------------------------------------------------------------------
	template<class T>
	void Allocate1dArray(T* &matrix, size_t res)
	{
		matrix = new T[res];
	}
	template<class T>
	void Delete1dArray(T* matrix, size_t res)
	{
		delete[] matrix;
	}

}

#endif //ARRAY_ALLOCATION_H

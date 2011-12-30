/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2010, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 *
 */
// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2010 Gael Guennebaud <gael.guennebaud@inria.fr>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

// The computeRoots function included in this is based on materials
// covered by the following copyright and license:
// 
// Geometric Tools, LLC
// Copyright (c) 1998-2010
// Distributed under the Boost Software License, Version 1.0.
// 
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#ifndef PCL_CUDA_EIGEN_H_
#define PCL_CUDA_EIGEN_H_

#include <pcl/cuda/point_cloud.h>
#include <pcl/cuda/cutil_math.h>

#in

#include <limits>
#include <float.h>

namespace pcl
{
  namespace cuda
  {
  
    inline __host__ __device__ bool isMuchSmallerThan (float x, float y)
    {
      float prec_sqr = FLT_EPSILON * FLT_EPSILON; // copied from <eigen>/include/Eigen/src/Core/NumTraits.h
      return x * x <= prec_sqr * y * y;
    }
  
    inline __host__ __device__ float3 unitOrthogonal (const float3& src)
    {
      float3 perp;
      /* Let us compute the crossed product of *this with a vector
       * that is not too close to being colinear to *this.
       */
  
      /* unless the x and y coords are both close to zero, we can
       * simply take ( -y, x, 0 ) and normalize it.
       */
      if((!isMuchSmallerThan(src.x, src.z))
      || (!isMuchSmallerThan(src.y, src.z)))
      {   
        float invnm = 1.0f / sqrtf (src.x*src.x + src.y*src.y);
        perp.x = -src.y*invnm;
        perp.y = src.x*invnm;
        perp.z = 0.0f;
      }   
      /* if both x and y are close to zero, then the vector is close
       * to the z-axis, so it's far from colinear to the x-axis for instance.
       * So we take the crossed product with (1,0,0) and normalize it.
       */
      else
      {   
        float invnm = 1.0f / sqrtf (src.z*src.z + src.y*src.y);
        perp.x = 0.0f;
        perp.y = -src.z*invnm;
        perp.z = src.y*invnm;
      }   
  
      return perp;
    }   
  
    inline __host__ __device__ void computeRoots2 (const float& b, const float& c, float3& roots)
  	{
  		roots.x = 0.0f;
  		float d = b * b - 4.0f * c;
  		if (d < 0.0f) // no real roots!!!! THIS SHOULD NOT HAPPEN!
  			d = 0.0f;
  
  		float sd = sqrt (d);
  		
  		roots.z = 0.5f * (b + sd);
  		roots.y = 0.5f * (b - sd);
  	}
  
    inline __host__ __device__ void swap (float& a, float& b)
    {
      float c(a); a=b; b=c;
    }
  
  	
  //  template<typename Matrix, typename Roots>
    inline __host__ __device__ void 
    computeRoots (const CovarianceMatrix& m, float3& roots)
    {
      // The characteristic equation is x^3 - c2*x^2 + c1*x - c0 = 0.  The
      // eigenvalues are the roots to this equation, all guaranteed to be
      // real-valued, because the matrix is symmetric.
      float  c0 =          m.data[0].x*m.data[1].y*m.data[2].z 
                  + 2.0f * m.data[0].y*m.data[0].z*m.data[1].z 
                         - m.data[0].x*m.data[1].z*m.data[1].z 
                         - m.data[1].y*m.data[0].z*m.data[0].z 
                         - m.data[2].z*m.data[0].y*m.data[0].y;
      float  c1 = m.data[0].x*m.data[1].y - 
                  m.data[0].y*m.data[0].y + 
                  m.data[0].x*m.data[2].z - 
                  m.data[0].z*m.data[0].z + 
                  m.data[1].y*m.data[2].z - 
                  m.data[1].z*m.data[1].z;
      float  c2 = m.data[0].x + m.data[1].y + m.data[2].z;
  
  
  		if (fabs(c0) < FLT_EPSILON) // one root is 0 -> quadratic equation
  			computeRoots2 (c2, c1, roots);
  		else
  		{
  		  const float  s_inv3 = 1.0f/3.0f;
  		  const float  s_sqrt3 = sqrtf (3.0f);
  		  // Construct the parameters used in classifying the roots of the equation
  		  // and in solving the equation for the roots in closed form.
  		  float c2_over_3 = c2 * s_inv3;
  		  float a_over_3 = (c1 - c2 * c2_over_3) * s_inv3;
  		  if (a_over_3 > 0.0f)
  		    a_over_3 = 0.0f;
  
  		  float half_b = 0.5f * (c0 + c2_over_3 * (2.0f * c2_over_3 * c2_over_3 - c1));
  
  		  float q = half_b * half_b + a_over_3 * a_over_3 * a_over_3;
  		  if (q > 0.0f)
  		    q = 0.0f;
  
  		  // Compute the eigenvalues by solving for the roots of the polynomial.
  		  float rho = sqrtf (-a_over_3);
  		  float theta = std::atan2 (sqrtf (-q), half_b) * s_inv3;
  		  float cos_theta = cos (theta);
  		  float sin_theta = sin (theta);
  		  roots.x = c2_over_3 + 2.f * rho * cos_theta;
  		  roots.y = c2_over_3 - rho * (cos_theta + s_sqrt3 * sin_theta);
  		  roots.z = c2_over_3 - rho * (cos_theta - s_sqrt3 * sin_theta);
  
  		  // Sort in increasing order.
  		  if (roots.x >= roots.y)
  		    swap (roots.x, roots.y);
  		  if (roots.y >= roots.z)
  		  {
  		    swap (roots.y, roots.z);
  		    if (roots.x >= roots.y)
  		      swap (roots.x, roots.y);
  		  }
  		  
  		  if (roots.x <= 0.0f) // eigenval for symetric positive semi-definite matrix can not be negative! Set it to 0
  			  computeRoots2 (c2, c1, roots);
  		}
    }
  
    inline __host__ __device__ void 
    eigen33 (const CovarianceMatrix& mat, CovarianceMatrix& evecs, float3& evals)
    {
      evals = evecs.data[0] = evecs.data[1] = evecs.data[2] = make_float3 (0.0f, 0.0f, 0.0f);
  
      // Scale the matrix so its entries are in [-1,1].  The scaling is applied
      // only when at least one matrix entry has magnitude larger than 1.
  
      //Scalar scale = mat.cwiseAbs ().maxCoeff ();
      float3 scale_tmp = fmaxf (fmaxf (fabs (mat.data[0]), fabs (mat.data[1])), fabs (mat.data[2]));
      float scale = fmaxf (fmaxf (scale_tmp.x, scale_tmp.y), scale_tmp.z);
      if (scale <= FLT_MIN)
      	scale = 1.0f;
      
      CovarianceMatrix scaledMat;
      scaledMat.data[0] = mat.data[0] / scale;
      scaledMat.data[1] = mat.data[1] / scale;
      scaledMat.data[2] = mat.data[2] / scale;
  
      // Compute the eigenvalues
      computeRoots (scaledMat, evals);
      
  		if ((evals.z-evals.x) <= FLT_EPSILON)
  		{
  			// all three equal
  			evecs.data[0] = make_float3 (1.0f, 0.0f, 0.0f);
  			evecs.data[1] = make_float3 (0.0f, 1.0f, 0.0f);
  			evecs.data[2] = make_float3 (0.0f, 0.0f, 1.0f);
  		}
  		else if ((evals.y-evals.x) <= FLT_EPSILON)
  		{
  			// first and second equal
  			CovarianceMatrix tmp;
  			tmp.data[0] = scaledMat.data[0];
  			tmp.data[1] = scaledMat.data[1];
  			tmp.data[2] = scaledMat.data[2];
  
        tmp.data[0].x -= evals.z;
        tmp.data[1].y -= evals.z;
        tmp.data[2].z -= evals.z;
  
  			float3 vec1 = cross (tmp.data[0], tmp.data[1]);
  			float3 vec2 = cross (tmp.data[0], tmp.data[2]);
  			float3 vec3 = cross (tmp.data[1], tmp.data[2]);
  
  			float len1 = dot (vec1, vec1);
  			float len2 = dot (vec2, vec2);
  			float len3 = dot (vec3, vec3);
  
  			if (len1 >= len2 && len1 >= len3)
  			 	evecs.data[2] = vec1 / sqrtf (len1);
  			else if (len2 >= len1 && len2 >= len3)
  		 		evecs.data[2] = vec2 / sqrtf (len2);
  			else
  				evecs.data[2] = vec3 / sqrtf (len3);
  		
  			evecs.data[1] = unitOrthogonal (evecs.data[2]); 
  			evecs.data[0] = cross (evecs.data[1], evecs.data[2]);
  		}
  		else if ((evals.z-evals.y) <= FLT_EPSILON)
  		{
  			// second and third equal
  			CovarianceMatrix tmp;
  			tmp.data[0] = scaledMat.data[0];
  			tmp.data[1] = scaledMat.data[1];
  			tmp.data[2] = scaledMat.data[2];
        tmp.data[0].x -= evals.x;
        tmp.data[1].y -= evals.x;
        tmp.data[2].z -= evals.x;
  
  			float3 vec1 = cross (tmp.data[0], tmp.data[1]);
  			float3 vec2 = cross (tmp.data[0], tmp.data[2]);
  			float3 vec3 = cross (tmp.data[1], tmp.data[2]);
  
  			float len1 = dot (vec1, vec1);
  			float len2 = dot (vec2, vec2);
  			float len3 = dot (vec3, vec3);
  
  			if (len1 >= len2 && len1 >= len3)
  			 	evecs.data[0] = vec1 / sqrtf (len1);
  			else if (len2 >= len1 && len2 >= len3)
  		 		evecs.data[0] = vec2 / sqrtf (len2);
  			else
  				evecs.data[0] = vec3 / sqrtf (len3);
  		
  			evecs.data[1] = unitOrthogonal (evecs.data[0]);
  			evecs.data[2] = cross (evecs.data[0], evecs.data[1]);
  		}
  		else
  		{
  			CovarianceMatrix tmp;
  			tmp.data[0] = scaledMat.data[0];
  			tmp.data[1] = scaledMat.data[1];
  			tmp.data[2] = scaledMat.data[2];
        tmp.data[0].x -= evals.z;
        tmp.data[1].y -= evals.z;
        tmp.data[2].z -= evals.z;
  
  			float3 vec1 = cross (tmp.data[0], tmp.data[1]);
  			float3 vec2 = cross (tmp.data[0], tmp.data[2]);
  			float3 vec3 = cross (tmp.data[1], tmp.data[2]);
  
  			float len1 = dot (vec1, vec1);
  			float len2 = dot (vec2, vec2);
  			float len3 = dot (vec3, vec3);
  
  			float mmax[3];
  		  unsigned int min_el = 2;
  		  unsigned int max_el = 2;
  		  if (len1 >= len2 && len1 >= len3)
  		  {
  		    mmax[2] = len1;
  		    evecs.data[2] = vec1 / sqrtf (len1);
  		  }
  		  else if (len2 >= len1 && len2 >= len3)
  		  {
  		    mmax[2] = len2;
  		    evecs.data[2] = vec2 / sqrtf (len2);
  		  }
  		  else
  		  {
  		    mmax[2] = len3;
  		    evecs.data[2] = vec3 / sqrtf (len3);
  		  }
  
  			tmp.data[0] = scaledMat.data[0];
  			tmp.data[1] = scaledMat.data[1];
  			tmp.data[2] = scaledMat.data[2];
        tmp.data[0].x -= evals.y;
        tmp.data[1].y -= evals.y;
        tmp.data[2].z -= evals.y;
  
  			vec1 = cross (tmp.data[0], tmp.data[1]);
  			vec2 = cross (tmp.data[0], tmp.data[2]);
  			vec3 = cross (tmp.data[1], tmp.data[2]);
  
  			len1 = dot (vec1, vec1);
  			len2 = dot (vec2, vec2);
  			len3 = dot (vec3, vec3);
  		  if (len1 >= len2 && len1 >= len3)
  		  {
  		    mmax[1] = len1;
  		    evecs.data[1] = vec1 / sqrtf (len1);
  		    min_el = len1 <= mmax[min_el]? 1: min_el;
  		    max_el = len1 > mmax[max_el]? 1: max_el;
  		  }
  		  else if (len2 >= len1 && len2 >= len3)
  		  {
  		    mmax[1] = len2;
  		    evecs.data[1] = vec2 / sqrtf (len2);
  		    min_el = len2 <= mmax[min_el]? 1: min_el;
  		    max_el = len2 > mmax[max_el]? 1: max_el;
  		  }
  		  else
  		  {
  		    mmax[1] = len3;
  		    evecs.data[1] = vec3 / sqrtf (len3);
  		    min_el = len3 <= mmax[min_el]? 1: min_el;
  		    max_el = len3 > mmax[max_el]? 1: max_el;
  		  }
  		  
  			tmp.data[0] = scaledMat.data[0];
  			tmp.data[1] = scaledMat.data[1];
  			tmp.data[2] = scaledMat.data[2];
        tmp.data[0].x -= evals.x;
        tmp.data[1].y -= evals.x;
        tmp.data[2].z -= evals.x;
  
  			vec1 = cross (tmp.data[0], tmp.data[1]);
  			vec2 = cross (tmp.data[0], tmp.data[2]);
  			vec3 = cross (tmp.data[1], tmp.data[2]);
  
  			len1 = dot (vec1, vec1);
  			len2 = dot (vec2, vec2);
  			len3 = dot (vec3, vec3);
  		  if (len1 >= len2 && len1 >= len3)
  		  {
  		    mmax[0] = len1;
  		    evecs.data[0] = vec1 / sqrtf (len1);
  		    min_el = len3 <= mmax[min_el]? 0: min_el;
  		    max_el = len3 > mmax[max_el]? 0: max_el;
  		  }
  		  else if (len2 >= len1 && len2 >= len3)
  		  {
  		    mmax[0] = len2;
  		    evecs.data[0] = vec2 / sqrtf (len2);
  		    min_el = len3 <= mmax[min_el]? 0: min_el;
  		    max_el = len3 > mmax[max_el]? 0: max_el; 		
  		  }
  		  else
  		  {
  		    mmax[0] = len3;
  		    evecs.data[0] = vec3 / sqrtf (len3);
  		    min_el = len3 <= mmax[min_el]? 0: min_el;
  		    max_el = len3 > mmax[max_el]? 0: max_el;	  
  		  }
  		  
  		  unsigned mid_el = 3 - min_el - max_el;
  		  evecs.data[min_el] = normalize (cross (evecs.data[(min_el+1)%3], evecs.data[(min_el+2)%3] ));
  			evecs.data[mid_el] = normalize (cross (evecs.data[(mid_el+1)%3], evecs.data[(mid_el+2)%3] ));
  		}
  	  // Rescale back to the original size.
  	  evals *= scale;
    }
  
    /** \brief Simple kernel to add two points. */
    struct AddPoints
    {
      __inline__ __host__ __device__ float3
      operator () (float3 lhs, float3 rhs)
      {
        return lhs + rhs;
      }
    };
  
    /** \brief Adds two matrices element-wise. */
    struct AddCovariances
    {
      __inline__ __host__ __device__
      CovarianceMatrix
      operator () (CovarianceMatrix lhs, CovarianceMatrix rhs)
      {
        CovarianceMatrix ret;
        ret.data[0] = lhs.data[0] + rhs.data[0];
        ret.data[1] = lhs.data[1] + rhs.data[1];
        ret.data[2] = lhs.data[2] + rhs.data[2];
        return ret;
      }
    };
  
    /** \brief Simple kernel to convert a PointXYZRGB to float3. Relies the cast operator of PointXYZRGB. */
    struct convert_point_to_float3
    {
      __inline__ __host__ __device__ float3
      operator () (const PointXYZRGB& pt) {return pt;}
    };
  
    /** \brief Kernel to compute a ``covariance matrix'' for a single point. */
    struct ComputeCovarianceForPoint
    {
      float3 centroid_;
      __inline__ __host__ __device__
      ComputeCovarianceForPoint (const float3& centroid) : centroid_ (centroid) {}
  
      __inline__ __host__ __device__ CovarianceMatrix
      operator () (const PointXYZRGB& point)
      {
        CovarianceMatrix cov;
        float3 pt = point - centroid_;
        cov.data[1].y = pt.y * pt.y; 
        cov.data[1].z = pt.y * pt.z; 
        cov.data[2].z = pt.z * pt.z; 
  
        pt *= pt.x; 
        cov.data[0].x = pt.x; 
        cov.data[0].y = pt.y; 
        cov.data[0].z = pt.z; 
        return cov;
      }
    };
  
    /** \brief Computes a centroid for a given range of points. */
    template <class IteratorT>
    void compute3DCentroid (IteratorT begin, IteratorT end, float3& centroid)
    {
      // Compute Centroid:
      centroid.x = centroid.y = centroid.z = 0;
      // we need a way to iterate over the inliers in the point cloud.. permutation_iterator to the rescue
      centroid = transform_reduce (begin, end, convert_point_to_float3 (), centroid, AddPoints ());
      centroid /= (float) (end - begin);
    }
  
    /** \brief Computes a covariance matrix for a given range of points. */
    template <class IteratorT>
    void computeCovariance (IteratorT begin, IteratorT end, CovarianceMatrix& cov, float3 centroid)
    {
      cov.data[0] = make_float3 (0.0f, 0.0f, 0.0f);
      cov.data[1] = make_float3 (0.0f, 0.0f, 0.0f);
      cov.data[2] = make_float3 (0.0f, 0.0f, 0.0f);
  
      cov = transform_reduce (begin, end,
                              ComputeCovarianceForPoint (centroid),
                              cov,
                              AddCovariances ());
  
      // fill in the lower triangle (symmetry)
      cov.data[1].x = cov.data[0].y; 
      cov.data[2].x = cov.data[0].z; 
      cov.data[2].y = cov.data[1].z; 
  
      // divide by number of inliers
      cov.data[0] /= (float) (end - begin);
      cov.data[1] /= (float) (end - begin);
      cov.data[2] /= (float) (end - begin);
    }
  
    /** Kernel to compute a radius neighborhood given a organized point cloud (aka range image cloud) */
    template <typename CloudPtr>
    class OrganizedRadiusSearch
    {
    public:
      OrganizedRadiusSearch (const CloudPtr &input, float focalLength, float sqr_radius)
        : points_(thrust::raw_pointer_cast (&input->points[0]))
        , focalLength_(focalLength)
        , width_ (input->width)
        , height_ (input->height)
        , sqr_radius_ (sqr_radius)
      {}
  
      //////////////////////////////////////////////////////////////////////////////////////////////
      // returns float4: .x = min_x, .y = max_x, .z = min_y, .w = max_y
      // note: assumes the projection of a point falls onto the image lattice! otherwise, min_x might be bigger than max_x !!!
      inline __host__ __device__
      int4
      getProjectedRadiusSearchBox (const float3& point_arg)
      {
        int4 res;
        float r_quadr, z_sqr;
        float sqrt_term_y, sqrt_term_x, norm;
        float x_times_z, y_times_z;
  
        // see http://www.wolframalpha.com/input/?i=solve+%5By%2Fsqrt%28f^2%2By^2%29*c-f%2Fsqrt%28f^2%2By^2%29*b%2Br%3D%3D0%2C+f%3D1%2C+y%5D
        // where b = p_q_arg.y, c = p_q_arg.z, r = radius_arg, f = focalLength_
  
        r_quadr = sqr_radius_ * sqr_radius_;
        z_sqr = point_arg.z * point_arg.z;
  
        sqrt_term_y = sqrt (point_arg.y * point_arg.y * sqr_radius_ + z_sqr * sqr_radius_ - r_quadr);
        sqrt_term_x = sqrt (point_arg.x * point_arg.x * sqr_radius_ + z_sqr * sqr_radius_ - r_quadr);
        //sqrt_term_y = sqrt (point_arg.y * point_arg.y * sqr_radius_ + z_sqr * sqr_radius_ - r_quadr);
        //sqrt_term_x = sqrt (point_arg.x * point_arg.x * sqr_radius_ + z_sqr * sqr_radius_ - r_quadr);
        norm = 1.0f / (z_sqr - sqr_radius_);
  
        x_times_z = point_arg.x * point_arg.z;
        y_times_z = point_arg.y * point_arg.z;
  
        float4 bounds;
        bounds.x = (x_times_z - sqrt_term_x) * norm;
        bounds.y = (x_times_z + sqrt_term_x) * norm;
        bounds.z = (y_times_z - sqrt_term_y) * norm;
        bounds.w = (y_times_z + sqrt_term_y) * norm;
  
        // determine 2-D search window
        bounds *= focalLength_;
        bounds.x += width_ / 2.0f;
        bounds.y += width_ / 2.0f;
        bounds.z += height_ / 2.0f;
        bounds.w += height_ / 2.0f;
  
        res.x = (int)floor (bounds.x); 
        res.y = (int)ceil  (bounds.y);
        res.z = (int)floor (bounds.z);
        res.w = (int)ceil  (bounds.w);
  
        // clamp the coordinates to fit to depth image size
        res.x = clamp (res.x, 0, width_-1);
        res.y = clamp (res.y, 0, width_-1);
        res.z = clamp (res.z, 0, height_-1);
        res.w = clamp (res.w, 0, height_-1);
        return res;
      }
  
      //////////////////////////////////////////////////////////////////////////////////////////////
      inline __host__ __device__
      int radiusSearch (const float3 &query_pt, int k_indices[], int max_nnn)
      {
        // bounds.x = min_x, .y = max_x, .z = min_y, .w = max_y
        int4 bounds = getProjectedRadiusSearchBox(query_pt);
  
        int nnn = 0;
        // iterate over all pixels in the rectangular region
        for (int x = bounds.x; (x <= bounds.y) & (nnn < max_nnn); ++x)
        {
          for (int y = bounds.z; (y <= bounds.w) & (nnn < max_nnn); ++y)
          {
            int idx = y * width_ + x;
  
            if (isnan (points_[idx].x))
              continue;
  
            float3 point_dif = points_[idx] - query_pt;
  
            // check distance and add to results
            if (dot (point_dif, point_dif) <= sqr_radius_)
            {
              k_indices[nnn] = idx;
              ++nnn;
            }
          }
        }
  
        return nnn;
      }
  
      //////////////////////////////////////////////////////////////////////////////////////////////
      inline __host__ __device__
      int computeCovarianceOnline (const float3 &query_pt, CovarianceMatrix &cov, float sqrt_desired_nr_neighbors)
      {
        // bounds.x = min_x, .y = max_x, .z = min_y, .w = max_y
        //
        //sqr_radius_ = query_pt.z * (0.2f / 4.0f);
        //sqr_radius_ *= sqr_radius_;
        int4 bounds = getProjectedRadiusSearchBox(query_pt);

        // This implements a fixed window size in image coordinates (pixels)
        //int2 proj_point = make_int2 ( query_pt.x/(query_pt.z/focalLength_)+width_/2.0f, query_pt.y/(query_pt.z/focalLength_)+height_/2.0f);
        //int window_size = 1;
        //int4 bounds = make_int4 (
        //    proj_point.x - window_size,
        //    proj_point.x + window_size,
        //    proj_point.y - window_size,
        //    proj_point.y + window_size
        //    );
        
        // clamp the coordinates to fit to depth image size
        bounds.x = clamp (bounds.x, 0, width_-1);
        bounds.y = clamp (bounds.y, 0, width_-1);
        bounds.z = clamp (bounds.z, 0, height_-1);
        bounds.w = clamp (bounds.w, 0, height_-1);
        //int4 bounds = getProjectedRadiusSearchBox(query_pt);
  
        // number of points in rectangular area
        //int boundsarea = (bounds.y-bounds.x) * (bounds.w-bounds.z);
        //float skip = max (sqrtf ((float)boundsarea) / sqrt_desired_nr_neighbors, 1.0);
        float skipX = max (sqrtf ((float)bounds.y-bounds.x) / sqrt_desired_nr_neighbors, 1.0f);
        float skipY = max (sqrtf ((float)bounds.w-bounds.z) / sqrt_desired_nr_neighbors, 1.0f);
        skipX = 1;
        skipY = 1;
  
        cov.data[0] = make_float3(0,0,0);
        cov.data[1] = make_float3(0,0,0);
        cov.data[2] = make_float3(0,0,0);
        float3 centroid = make_float3(0,0,0);
        int nnn = 0;
        // iterate over all pixels in the rectangular region
        for (float y = (float) bounds.z; y <= bounds.w; y += skipY)
        {
          for (float x = (float) bounds.x; x <= bounds.y; x += skipX)
          {
            // find index in point cloud from x,y pixel positions
            int idx = ((int)y) * width_ + ((int)x);
  
            // ignore invalid points
            if (isnan (points_[idx].x) | isnan (points_[idx].y) | isnan (points_[idx].z))
              continue;
  
            float3 point_dif = points_[idx] - query_pt;
            
            // check distance and update covariance matrix
            if (dot (point_dif, point_dif) <= sqr_radius_)
            {
              ++nnn;
              float3 demean_old = points_[idx] - centroid;
              centroid += demean_old / (float) nnn;
              float3 demean_new = points_[idx] - centroid;
  
              cov.data[1].y += demean_new.y * demean_old.y; 
              cov.data[1].z += demean_new.y * demean_old.z; 
              cov.data[2].z += demean_new.z * demean_old.z; 
  
              demean_old *= demean_new.x; 
              cov.data[0].x += demean_old.x; 
              cov.data[0].y += demean_old.y; 
              cov.data[0].z += demean_old.z; 
            }
          }
        }
  
        cov.data[1].x = cov.data[0].y; 
        cov.data[2].x = cov.data[0].z; 
        cov.data[2].y = cov.data[1].z;
        cov.data[0] /= (float) nnn; 
        cov.data[1] /= (float) nnn; 
        cov.data[2] /= (float) nnn;
        return nnn;
      }

      //////////////////////////////////////////////////////////////////////////////////////////////
      inline __host__ __device__
      float3 computeCentroid (const float3 &query_pt, CovarianceMatrix &cov, float sqrt_desired_nr_neighbors)
      {
        // bounds.x = min_x, .y = max_x, .z = min_y, .w = max_y
        //
        //sqr_radius_ = query_pt.z * (0.2f / 4.0f);
        //sqr_radius_ *= sqr_radius_;
        int4 bounds = getProjectedRadiusSearchBox(query_pt);

        // This implements a fixed window size in image coordinates (pixels)
        //int2 proj_point = make_int2 ( query_pt.x/(query_pt.z/focalLength_)+width_/2.0f, query_pt.y/(query_pt.z/focalLength_)+height_/2.0f);
        //int window_size = 1;
        //int4 bounds = make_int4 (
        //    proj_point.x - window_size,
        //    proj_point.x + window_size,
        //    proj_point.y - window_size,
        //    proj_point.y + window_size
        //    );
        
        // clamp the coordinates to fit to depth image size
        bounds.x = clamp (bounds.x, 0, width_-1);
        bounds.y = clamp (bounds.y, 0, width_-1);
        bounds.z = clamp (bounds.z, 0, height_-1);
        bounds.w = clamp (bounds.w, 0, height_-1);
  
        // number of points in rectangular area
        //int boundsarea = (bounds.y-bounds.x) * (bounds.w-bounds.z);
        //float skip = max (sqrtf ((float)boundsarea) / sqrt_desired_nr_neighbors, 1.0);
        float skipX = max (sqrtf ((float)bounds.y-bounds.x) / sqrt_desired_nr_neighbors, 1.0f);
        float skipY = max (sqrtf ((float)bounds.w-bounds.z) / sqrt_desired_nr_neighbors, 1.0f);
 
        skipX = 1;
        skipY = 1;
        float3 centroid = make_float3(0,0,0);
        int nnn = 0;
        // iterate over all pixels in the rectangular region
        for (float y = (float) bounds.z; y <= bounds.w; y += skipY)
        {
          for (float x = (float) bounds.x; x <= bounds.y; x += skipX)
          {
            // find index in point cloud from x,y pixel positions
            int idx = ((int)y) * width_ + ((int)x);
  
            // ignore invalid points
            if (isnan (points_[idx].x) | isnan (points_[idx].y) | isnan (points_[idx].z))
              continue;
  
            float3 point_dif = points_[idx] - query_pt;
            
            // check distance and update covariance matrix
            if (dot (point_dif, point_dif) <= sqr_radius_)
            {
              centroid += points_[idx];
              ++nnn;
            }
          }
        }
  
        return centroid / (float) nnn;
      }
  
      float focalLength_;
      const PointXYZRGB *points_;
      int width_, height_;
      float sqr_radius_;
    };
  
  } // namespace
} // namespace

#endif  //#ifndef PCL_CUDA_EIGEN_H_

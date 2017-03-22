/* Copyright (c) 2008-2017 the MRtrix3 contributors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * MRtrix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * For more details, see http://www.mrtrix.org/.
 */


#ifndef __dwi_svr_recon_h__
#define __dwi_svr_recon_h__


#include <vector>
#include <Eigen/Dense>
#include <Eigen/Sparse>

#include "header.h"
#include "math/SH.h"


namespace MR {
  namespace DWI {
    class ReconMatrix;
    class ReconMatrixAdjoint;
  }
}

namespace Eigen {
  namespace internal {
    // ReconMatrix inherits its traits from SparseMatrix
    template<>
    struct traits<MR::DWI::ReconMatrix> : public Eigen::internal::traits<Eigen::SparseMatrix<float> >
    {};

    template<>
    struct traits<MR::DWI::ReconMatrixAdjoint> : public Eigen::internal::traits<Eigen::SparseMatrix<float> >
    {};
  }
}


namespace MR
{
  namespace DWI
  {

    class ReconMatrix : public Eigen::EigenBase<ReconMatrix>
    {  MEMALIGN(ReconMatrix);
    public:
      // Required typedefs, constants, and method:
      typedef float Scalar;
      typedef float RealScalar;
      typedef int StorageIndex;
      enum {
        ColsAtCompileTime = Eigen::Dynamic,
        MaxColsAtCompileTime = Eigen::Dynamic,
        IsRowMajor = false
      };
      Eigen::Index rows() const { return M.rows(); }
      Eigen::Index cols() const { return M.cols()*Y.cols(); }


      template<typename Rhs>
      Eigen::Product<ReconMatrix,Rhs,Eigen::AliasFreeProduct> operator*(const Eigen::MatrixBase<Rhs>& x) const {
        return Eigen::Product<ReconMatrix,Rhs,Eigen::AliasFreeProduct>(*this, x.derived());
      }

      // Custom API:
      ReconMatrix(const Header& in, const Eigen::MatrixXf& grad, const int lmax)
        : lmax(lmax),
          nxy (in.size(0)*in.size(1)), nz (in.size(2)), nv (in.size(3)),
          M(nxy*nz*nv, nxy*nz),
          Y(nz*nv, Math::SH::NforL(lmax))
      {
        init_M(in);
        init_Y(in, grad);
      }

      const Eigen::SparseMatrix<float>& getM() const { return M; }
      const Eigen::MatrixXf& getY() const { return Y; }

      inline const size_t get_grad_idx(const size_t idx) const { return idx / nxy; }

      const ReconMatrixAdjoint adjoint() const;

    private:
      const int lmax;
      const size_t nxy, nz, nv;
      Eigen::SparseMatrix<float> M;
      Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic> Y;


      void init_M(const Header& in)
      {
        DEBUG("initialise M");
        // Identity matrix for now.
        // TODO: complete with proper voxel weights later
        size_t i = 0, j = 0;
        for (size_t v = 0; v < in.size(3); v++) {
          j = 0;
          for (size_t z = 0; z < in.size(2); z++)
            for (size_t xy = 0; xy < nxy; xy++, i++, j++)
              M.insert(i,j) = 1.0;
        }
      }

      void init_Y(const Header& in, const Eigen::MatrixXf& grad)
      {
        DEBUG("initialise Y");
        assert (in.size(3) == grad.rows());     // one gradient per volume
        Eigen::Vector3f vec;
        Eigen::VectorXf delta;
        for (size_t i = 0; i < in.size(3); i++) {
          vec = {grad(i, 0), grad(i, 1), grad(i, 2)};
          for (size_t j = 0; j < in.size(2); j++) {
            // TODO: rotate vector with motion parameters
            Y.row(i*in.size(2)+j) = Math::SH::delta(delta, vec, lmax);
          }
        }
      }

    };


    class ReconMatrixAdjoint : public Eigen::EigenBase<ReconMatrixAdjoint>
    {  MEMALIGN(ReconMatrixAdjoint);
    public:
      // Required typedefs, constants, and method:
      typedef float Scalar;
      typedef float RealScalar;
      typedef int StorageIndex;
      enum {
        ColsAtCompileTime = Eigen::Dynamic,
        MaxColsAtCompileTime = Eigen::Dynamic,
        IsRowMajor = false
      };
      Eigen::Index rows() const { return R.cols(); }
      Eigen::Index cols() const { return R.rows(); }

      template<typename Rhs>
      Eigen::Product<ReconMatrixAdjoint,Rhs,Eigen::AliasFreeProduct> operator*(const Eigen::MatrixBase<Rhs>& x) const {
        return Eigen::Product<ReconMatrixAdjoint,Rhs,Eigen::AliasFreeProduct>(*this, x.derived());
      }

      ReconMatrixAdjoint(const ReconMatrix& R)
        : R(R)
      {  }

      const ReconMatrix& R;

    };



  }
}


// Implementation of ReconMatrix * Eigen::DenseVector though a specialization of internal::generic_product_impl:
namespace Eigen {
  namespace internal {

    template<typename Rhs>
    struct generic_product_impl<MR::DWI::ReconMatrix, Rhs, SparseShape, DenseShape, GemvProduct>
      : generic_product_impl_base<MR::DWI::ReconMatrix,Rhs,generic_product_impl<MR::DWI::ReconMatrix,Rhs> >
    {
      typedef typename Product<MR::DWI::ReconMatrix,Rhs>::Scalar Scalar;

      template<typename Dest>
      static void scaleAndAddTo(Dest& dst, const MR::DWI::ReconMatrix& lhs, const Rhs& rhs, const Scalar& alpha)
      {
        // This method should implement "dst += alpha * lhs * rhs" inplace
        assert(alpha==Scalar(1) && "scaling is not implemented");

        TRACE;
        auto Y = lhs.getY();
        size_t nc = Y.cols();
        size_t nxyz = lhs.getM().cols();
        VectorXf r (lhs.rows());

        for (size_t j = 0; j < nc; j++) {
          r = lhs.getM() * rhs.segment(j*nxyz, nxyz);
          for (size_t i = 0; i < lhs.rows(); i++)
            dst[i] += r[i] * Y(lhs.get_grad_idx(i), j);
        }

      }

    };


    template<typename Rhs>
    struct generic_product_impl<MR::DWI::ReconMatrixAdjoint, Rhs, SparseShape, DenseShape, GemvProduct>
      : generic_product_impl_base<MR::DWI::ReconMatrixAdjoint,Rhs,generic_product_impl<MR::DWI::ReconMatrixAdjoint,Rhs> >
    {
      typedef typename Product<MR::DWI::ReconMatrixAdjoint,Rhs>::Scalar Scalar;

      template<typename Dest>
      static void scaleAndAddTo(Dest& dst, const MR::DWI::ReconMatrixAdjoint& lhs, const Rhs& rhs, const Scalar& alpha)
      {
        // This method should implement "dst += alpha * lhs * rhs" inplace
        assert(alpha==Scalar(1) && "scaling is not implemented");

        TRACE;
        auto Y = lhs.R.getY();
        size_t nc = Y.cols();
        size_t nxyz = lhs.R.getM().cols();
        VectorXf r (lhs.cols());

        for (size_t j = 0; j < nc; j++) {
          r = rhs;
          for (size_t i = 0; i < lhs.cols(); i++)
            r[i] *= Y(lhs.R.get_grad_idx(i), j);
          dst.segment(j*nxyz, nxyz) += lhs.R.getM().adjoint() * r;
        }

      }

    };


  }
}


#endif


/* Copyright (c) 2008-2017 the MRtrix3 contributors.
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


#include <Eigen/Dense>
#include <unsupported/Eigen/MatrixFunctions>
#include <unsupported/Eigen/LevenbergMarquardt>

#include "command.h"
#include "image.h"
#include "transform.h"
#include "interp/linear.h"


using namespace MR;
using namespace App;


void usage ()
{
  AUTHOR = "Daan Christiaens (daan.christiaens@kcl.ac.uk)";

  SYNOPSIS = "Asymmetric rigid registration.";

  ARGUMENTS
    + Argument ("target",
                "the target image.").type_image_in ()
    + Argument ("moving",
                "the moving image.").type_image_in ()
    + Argument ("T",
                "the transformation matrix.").type_file_out ();

  OPTIONS
    + Option ("mask", "image mask.")
      + Argument ("image").type_image_in ()
    + Option ("maxiter", "maximum no. iterations.")
      + Argument ("n").type_integer(0)
    + Option ("init", "initial 4x4 transformation matrix.")
      + Argument ("T0").type_file_in ();

}


using value_type = float;


/* Exponential Lie mapping on SE(3). */
Eigen::Matrix<value_type, 4, 4> se3exp(const Eigen::VectorXf& v)
{
  Eigen::Matrix<value_type, 4, 4> A, T; A.setZero();
  A(0,3) = v[0]; A(1,3) = v[1]; A(2,3) = v[2];
  A(2,1) = v[3]; A(1,2) = -v[3];
  A(0,2) = v[4]; A(2,0) = -v[4];
  A(1,0) = v[5]; A(0,1) = -v[5];
  T = A.exp();
  return T;
}


/* Logarithmic Lie mapping on SE(3). */
Eigen::Matrix<value_type, 6, 1> se3log(const Eigen::Matrix<value_type, 4, 4>& T)
{
  Eigen::Matrix<value_type, 4, 4> A = T.log();
  Eigen::Matrix<value_type, 6, 1> v;
  v[0] = A(0,3); v[1] = A(1,3); v[2] = A(2,3);
  v[3] = (A(2,1) - A(1,2)) / 2;
  v[4] = (A(0,2) - A(2,0)) / 2;
  v[5] = (A(1,0) - A(0,1)) / 2;
  return v;
}


struct RegistrationFunctor : Eigen::DenseFunctor<value_type>
{
public:

  RegistrationFunctor(const Image<value_type>& target, const Image<value_type>& moving, const Image<bool>& mask)
    : m (0), n (6),
      T0 (target),
      mask (mask),
      target (target),
      moving (moving, 0.0f),
      Dmoving (moving, 0.0f)
  {
    DEBUG("Constructing LM registration functor.");
    m = calcMaskSize();
  }

  int operator()(const InputType& x, ValueType& fvec)
  {
    // get transformation matrix
    Eigen::Transform<value_type, 3, Eigen::Affine> T1 (se3exp(x));
    // interpolate and compute error
    Eigen::Vector3f trans;
    size_t i = 0;
    for (auto l = Loop(0,3)(target); l; l++) {
      if (!isInMask()) continue;
      trans = T1 * getScanPos();
      moving.scanner(trans);
      fvec[i] = target.value() - moving.value();
      i++;
    }
    return 0;
  }

  int df(const InputType& x, JacobianType& fjac)
  {
    // get transformation matrix
    Eigen::Transform<value_type, 3, Eigen::Affine> T1 (se3exp(x));
    // Allocate 3 x 6 Jacobian
    Eigen::Matrix<value_type, 3, 6> J;
    J.setIdentity();
    J *= -1;
    // compute image gradient and Jacobian
    Eigen::Vector3f trans;
    Eigen::RowVector3f grad;
    size_t i = 0;
    for (auto l = Loop(0,3)(target); l; l++) {
      if (!isInMask()) continue;
      trans = T1 * getScanPos();
      Dmoving.scanner(trans);
      grad = Dmoving.gradient_wrt_scanner().template cast<value_type>();
      J(2,4) = trans[0]; J(1,5) = -trans[0];
      J(0,5) = trans[1]; J(2,3) = -trans[1];
      J(1,3) = trans[2]; J(0,4) = -trans[2];
      fjac.row(i) = 2.0f * grad * J;
      i++;
    }
    return 0;
  }

  size_t values() const { return m; }
  size_t inputs() const { return n;}

private:
  size_t m, n;
  Transform T0;
  Image<bool> mask;
  Image<value_type> target;
  Interp::LinearInterp<Image<value_type>, Interp::LinearInterpProcessingType::Value> moving;
  Interp::LinearInterp<Image<value_type>, Interp::LinearInterpProcessingType::Derivative> Dmoving;

  inline size_t calcMaskSize()
  {
    if (!mask) return target.size(0)*target.size(1)*target.size(2);
    size_t count = 0;
    for (auto l = Loop(0,3)(mask); l; l++) {
      if (mask.value()) count++;
    }
    return count;
  }

  inline bool isInMask() {
    if (mask.valid()) {
      assign_pos_of(target).to(mask);
      return mask.value();
    } else {
      return true;
    }
  }

  inline Eigen::Vector3f getScanPos() {
    Eigen::Vector3f vox, scan;
    assign_pos_of(target).to(vox);
    scan = T0.voxel2scanner.template cast<value_type>() * vox;
    return scan;
  }

};


void run ()
{
  auto target = Image<value_type>::open(argument[0]);
  auto moving = Image<value_type>::open(argument[1]);

  auto mask = Image<bool>();
  auto opt = get_options("mask");
  if (opt.size()) {
    mask = Image<bool>::open(opt[0][0]);
    check_dimensions(target, mask, 0, 3);
  }

  Eigen::VectorXf x (6);
  x.setZero();
  opt = get_options("init");
  if (opt.size()) {
    Eigen::MatrixXf T = load_matrix<value_type>(opt[0][0]);
    x = se3log(T.block<4,4>(0,0));
    DEBUG("Initialization = [ " + str(x[0]) + " " + str(x[1]) + " " + str(x[2]) + " "
                                + str(x[3]) + " " + str(x[4]) + " " + str(x[5]) + " ]");
  }

  RegistrationFunctor F (target, moving, mask);
  Eigen::LevenbergMarquardt<RegistrationFunctor> LM (F);

  opt = get_options("maxiter");
  if (opt.size()) {
    LM.setMaxfev(opt[0][0]);
  }

  INFO("Start LM optimization.");
  LM.minimize(x);
  switch (LM.info()) {
    case Eigen::ComputationInfo::Success:
      INFO("LM optimization successful."); break;
    case Eigen::ComputationInfo::NoConvergence:
      INFO("LM optimization did not converge in given no. iterations."); break;
    default:
      throw Exception("LM optimization error."); break;
  }
  INFO("Optimum = [ " + str(x[0]) + " " + str(x[1]) + " " + str(x[2]) + " "
                      + str(x[3]) + " " + str(x[4]) + " " + str(x[5]) + " ]");
  INFO("Residual error = " + str(LM.fnorm()));

  save_matrix(se3exp(x), argument[2]);

}



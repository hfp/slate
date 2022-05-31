// Copyright (c) 2017-2022, University of Tennessee. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// This program is free software: you can redistribute it and/or modify it under
// the terms of the BSD 3-Clause license. See the accompanying LICENSE file.

#include "slate/slate.hh"
#include "auxiliary/Debug.hh"
#include "slate/Matrix.hh"
#include "slate/HermitianMatrix.hh"
#include "slate/Tile_blas.hh"
#include "internal/internal.hh"

namespace slate {

template <typename scalar_t>
bool iterRefConverged(std::vector<scalar_t>& colnorms_R,
                      std::vector<scalar_t>& colnorms_X,
                      scalar_t cte)
{
    assert(colnorms_X.size() == colnorms_R.size());
    bool value = true;
    int64_t size = colnorms_X.size();

    for (int64_t i = 0; i < size; i++) {
        if (colnorms_R[i] > colnorms_X[i] * cte) {
            value = false;
            break;
        }
    }

    return value;
}

//------------------------------------------------------------------------------
/// Distributed parallel iterative-refinement Cholesky factorization and solve.
///
/// Computes the solution to a system of linear equations
/// \[
///     A X = B,
/// \]
/// where $A$ is an n-by-n Hermitian positive definite matrix and $X$ and $B$
/// are n-by-nrhs matrices.
///
/// posvMixed first factorizes the matrix using potrf in low precision (single)
/// and uses this factorization within an iterative refinement procedure to
/// produce a solution with high precision (double) normwise backward error
/// quality (see below). If the approach fails, the method falls back to a
/// high precision (double) factorization and solve.
///
/// The iterative refinement is not going to be a winning strategy if
/// the ratio of low-precision performance over high-precision performance is
/// too small. A reasonable strategy should take the number of right-hand
/// sides and the size of the matrix into account. This might be automated
/// in the future. Up to now, we always try iterative refinement.
///
/// The iterative refinement process is stopped if iter > itermax or
/// for all the RHS, $1 \le j \le nrhs$, we have:
///     $\norm{r_j}_{inf} < \sqrt{n} \norm{x_j}_{inf} \norm{A}_{inf} \epsilon,$
/// where:
/// - iter is the number of the current iteration in the iterative refinement
///    process
/// - $\norm{r_j}_{inf}$ is the infinity-norm of the residual, $r_j = Ax_j - b_j$
/// - $\norm{x_j}_{inf}$ is the infinity-norm of the solution
/// - $\norm{A}_{inf}$ is the infinity-operator-norm of the matrix $A$
/// - $\epsilon$ is the machine epsilon.
///
/// The value itermax is fixed to 30.
///
//------------------------------------------------------------------------------
/// @tparam scalar_hi
///     One of double, std::complex<double>.
///
/// @tparam scalar_lo
///     One of float, std::complex<float>.
//------------------------------------------------------------------------------
/// @param[in,out] A
///     On entry, the n-by-n Hermitian positive definite matrix $A$.
///     On exit, if iterative refinement has been successfully used
///     (return value = 0 and iter >= 0, see description below), then $A$ is
///     unchanged. If high precision (double) factorization has been used
///     (return value = 0 and iter < 0, see description below), then the
///     array $A$ contains the factor $U$ or $L$ from the Cholesky
///     factorization $A = U^H U$ or $A = L L^H$.
///     If scalar_t is real, $A$ can be a SymmetricMatrix object.
///
/// @param[in] B
///     On entry, the n-by-nrhs right hand side matrix $B$.
///
/// @param[out] X
///     On exit, if return value = 0, the n-by-nrhs solution matrix $X$.
///
/// @param[out] iter
///     The number of the iterations in the iterative refinement
///     process, needed for the convergence. If failed, it is set
///     to be -(1+itermax), where itermax = 30.
///
/// @param[in] opts
///     Additional options, as map of name = value pairs. Possible options:
///     - Option::Lookahead:
///       Number of panels to overlap with matrix updates.
///       lookahead >= 0. Default 1.
///     - Option::Target:
///       Implementation to target. Possible values:
///       - HostTask:  OpenMP tasks on CPU host [default].
///       - HostNest:  nested OpenMP parallel for loop on CPU host.
///       - HostBatch: batched BLAS on CPU host.
///       - Devices:   batched BLAS on GPU device.
///
/// TODO: return value
/// @retval 0 successful exit
/// @retval >0 for return value = $i$, the leading minor of order $i$ of $A$ is not
///         positive definite, so the factorization could not
///         be completed, and the solution has not been computed.
///
/// @ingroup posv
///
template <typename scalar_hi, typename scalar_lo>
void posvMixed( HermitianMatrix<scalar_hi>& A,
                Matrix<scalar_hi>& B,
                Matrix<scalar_hi>& X,
                int& iter,
                Options const& opts)
{
    // XXX This is only used for the memory management and may be inconsistent
    // with the routines called in this routine.
    Target target = get_option( opts, Option::Target, Target::HostTask );

    // Assumes column major
    const Layout layout = Layout::ColMajor;

    bool converged = false;
    const int itermax = 30;
    using real_hi = blas::real_type<scalar_hi>;
    const real_hi eps = std::numeric_limits<real_hi>::epsilon();
    const scalar_hi one_hi      = 1.0;
    iter = 0;

    assert( B.mt() == A.mt() );

    // workspace
    auto R    = B.emptyLike();
    auto A_lo = A.template emptyLike<scalar_lo>();
    auto X_lo = X.template emptyLike<scalar_lo>();

    std::vector<real_hi> colnorms_X( X.n() );
    std::vector<real_hi> colnorms_R( R.n() );

    // insert local tiles
    X_lo.insertLocalTiles( target );
    R.   insertLocalTiles( target );
    A_lo.insertLocalTiles( target );

    if (target == Target::Devices) {
        #pragma omp parallel
        #pragma omp master
        {
            #pragma omp task default(none) shared(A) firstprivate(layout)
            {
                A.tileGetAndHoldAllOnDevices( LayoutConvert( layout ) );
            }
            #pragma omp task default(none) shared(B) firstprivate(layout)
            {
                B.tileGetAndHoldAllOnDevices( LayoutConvert( layout ) );
            }
            #pragma omp task default(none) shared(X) firstprivate(layout)
            {
                X.tileGetAndHoldAllOnDevices( LayoutConvert( layout ) );
            }
        }
    }

    // norm of A
    real_hi Anorm = norm( Norm::Inf, A, opts );

    // stopping criteria
    real_hi cte = Anorm * eps * std::sqrt( A.n() );

    // Convert B from high to low precision, store result in X_lo.
    copy( B, X_lo, opts );

    // Convert A from high to low precision, store result in A_lo.
    copy( A, A_lo, opts );

    // Compute the Cholesky factorization of A_lo.
    potrf(  A_lo, opts );

    // Solve the system A_lo * X_lo = B_lo.
    potrs( A_lo, X_lo, opts );

    // Convert X_lo to high precision.
    copy( X_lo, X, opts );

    // Compute R = B - A * X.
    slate::copy( B, R, opts );
    hemm<scalar_hi>(
        Side::Left,
        -one_hi, A,
                 X,
        one_hi,  R, opts );

    // Check whether the nrhs normwise backward error satisfies the
    // stopping criterion. If yes, set iter=0 and return.
    colNorms( Norm::Max, X, colnorms_X.data(), opts );
    colNorms( Norm::Max, R, colnorms_R.data(), opts );

    if (iterRefConverged<real_hi>( colnorms_R, colnorms_X, cte) ) {
        iter = 0;
        converged = true;
    }

    // iterative refinement
    for (int iiter = 0; iiter < itermax && ! converged; iiter++) {
        // Convert R from high to low precision, store result in X_lo.
        copy( R, X_lo, opts );

        // Solve the system A_lo * X_lo = R_lo.
        potrs( A_lo, X_lo, opts );

        // Convert X_lo back to double precision and update the current iterate.
        copy( X_lo, R, opts );
        add<scalar_hi>(
              one_hi, R,
              one_hi, X, opts );

        // Compute R = B - A * X.
        slate::copy( B, R, opts );
        hemm<scalar_hi>(
            Side::Left,
            -one_hi, A,
                     X,
            one_hi,  R, opts );


        // Check whether nrhs normwise backward error satisfies the
        // stopping criterion. If yes, set iter = iiter > 0 and return.
        colNorms( Norm::Max, X, colnorms_X.data(), opts );
        colNorms( Norm::Max, R, colnorms_R.data(), opts );

        if (iterRefConverged<real_hi>( colnorms_R, colnorms_X, cte )) {
            iter = iiter+1;
            converged = true;
        }
    }

    if (! converged) {
        // If we are at this place of the code, this is because we have performed
        // iter = itermax iterations and never satisfied the stopping criterion,
        // set up the iter flag accordingly and follow up with double precision
        // routine.
        iter = -itermax - 1;

        // Compute the Cholesky factorization of A.
        potrf( A, opts );

        // Solve the system A * X = B.
        slate::copy( B, X, opts );
        potrs( A, X, opts );
    }

    if (target == Target::Devices) {
        // clear instead of release due to previous hold
        A.clearWorkspace();
        B.clearWorkspace();
        X.clearWorkspace();
    }

    // todo: return value for errors?
}

//------------------------------------------------------------------------------
// Explicit instantiations.
template <>
void posvMixed<double>(
    HermitianMatrix<double>& A,
    Matrix<double>& B,
    Matrix<double>& X,
    int& iter,
    Options const& opts)
{
    posvMixed<double, float>( A, B, X, iter, opts );
}

template <>
void posvMixed< std::complex<double> >(
    HermitianMatrix< std::complex<double> >& A,
    Matrix< std::complex<double> >& B,
    Matrix< std::complex<double> >& X,
    int& iter,
    Options const& opts)
{
    posvMixed<std::complex<double>, std::complex<float>>( A, B, X, iter, opts );
}

} // namespace slate

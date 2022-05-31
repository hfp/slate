// Copyright (c) 2017-2022, University of Tennessee. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// This program is free software: you can redistribute it and/or modify it under
// the terms of the BSD 3-Clause license. See the accompanying LICENSE file.

#include "slate/slate.hh"
#include "internal/internal.hh"

namespace slate {

// specialization namespace differentiates, e.g.,
// internal::hemm from internal::specialization::hemm
namespace internal {
namespace specialization {

//------------------------------------------------------------------------------
/// @internal
/// Distributed parallel Hermitian matrix-matrix multiplication.
/// Generic implementation for any target.
/// Dependencies enforce the following behavior:
/// - bcast communications are serialized,
/// - hemm operations are serialized,
/// - bcasts can get ahead of hemms by the value of lookahead.
/// Note A, B, and C are passed by value, so we can transpose if needed
/// (for side = right) without affecting caller.
/// @ingroup hemm_specialization
///
/// ColMajor layout is assumed
///
template <Target target, typename scalar_t>
void hemmC( slate::internal::TargetType<target>,
            Side side,
            scalar_t alpha, HermitianMatrix<scalar_t> A,
            Matrix<scalar_t> B,
            scalar_t beta,  Matrix<scalar_t> C,
            int64_t lookahead)
{
    // Due to the symmetry, each off diagonal tile is sent twice, once as part
    // of A and once as art of A^T. In principle, this could be avoided by
    // sending each tile only once and retaining it until it is used twice.
    // This would, however, violate the upper bound on the size of communication
    // buffers.
    // The same happens in the symm routine.
    // See also the implementation remarks in the BaseMatrix::listBcast routine.

    using blas::conj;
    using BcastListTag = typename Matrix<scalar_t>::BcastListTag;

    // Constants
    const scalar_t one = 1.0;

    // Assumes column major
    const Layout layout = Layout::ColMajor;

    // if on right, change to left by transposing A, B, C to get
    // op(C) = op(A)*op(B)
    if (side == Side::Right) {
        A = conjTranspose( A );
        B = conjTranspose( B );
        C = conjTranspose( C );
        alpha = conj( alpha );
        beta  = conj( beta );
    }

    // B and C are mt-by-nt, A is mt-by-mt (assuming side = left)
    assert( A.mt() == B.mt() );
    assert( A.nt() == B.mt() );
    assert( B.mt() == C.mt() );
    assert( B.nt() == C.nt() );

    // OpenMP needs pointer  types, but vectors are exception safe
    std::vector<uint8_t> bcast_vector( A.nt() );
    std::vector<uint8_t>  gemm_vector( A.nt() );
    uint8_t* bcast = bcast_vector.data();
    uint8_t* gemm  =  gemm_vector.data();

    if (target == Target::Devices) {
        C.allocateBatchArrays();
        C.reserveDeviceWorkspace();
    }

    #pragma omp parallel
    #pragma omp master
    {
        omp_set_nested( 1 );
        if (A.uplo() == Uplo::Lower) {
            // ----------------------------------------
            // Left, Lower/NoTrans or Upper/ConjTrans case

            // send 1st block col of A and block row of B
            #pragma omp task depend(out:bcast[0])
            {
                // broadcast A(i, 0) to ranks owning block row C(i, :)
                BcastListTag bcast_list_A;
                for (int64_t i = 0; i < A.mt(); ++i)
                    bcast_list_A.push_back(
                        {i, 0, {C.sub( i, i, 0, C.nt()-1 )}, i} );
                A.template listBcastMT<target>( bcast_list_A, layout );

                // broadcast B(0, j) to ranks owning block col C(:, j)
                BcastListTag bcast_list_B;
                for (int64_t j = 0; j < B.nt(); ++j)
                    bcast_list_B.push_back(
                        {0, j, {C.sub( 0, C.mt()-1, j, j )}, j} );
                B.template listBcastMT<target>( bcast_list_B, layout );
            }

            // send next lookahead block cols of A and block rows of B
            for (int64_t k = 1; k < lookahead+1 && k < A.nt(); ++k) {
                #pragma omp task depend(in:bcast[k-1]) \
                                 depend(out:bcast[k])
                {
                    // broadcast A(k, i) or A(i, k)
                    // to ranks owning block row C(i, :)
                    BcastListTag bcast_list_A;
                    for (int64_t i = 0; i < k && i < A.mt(); ++i) {
                        bcast_list_A.push_back(
                            {k, i, {C.sub( i, i, 0, C.nt()-1 )}, i} );
                    }
                    for (int64_t i = k; i < A.mt(); ++i) {
                        bcast_list_A.push_back(
                            {i, k, {C.sub( i, i, 0, C.nt()-1 )}, i} );
                    }
                    A.template listBcastMT<target>( bcast_list_A, layout );

                    // broadcast B(k, j) to ranks owning block col C(0:k, j)
                    BcastListTag bcast_list_B;
                    for (int64_t j = 0; j < B.nt(); ++j) {
                        bcast_list_B.push_back(
                            {k, j, {C.sub( 0, C.mt()-1, j, j )}, j} );
                    }
                    B.template listBcastMT<target>( bcast_list_B, layout );
                }
            }

            // multiply alpha A(:, 0) B(0, :), which is (hemm / gemm):
            // C(0, :)      = alpha [ A(0, 0)      B(0, :) ] + beta C(0, :)
            // C(1:mt-1, :) = alpha [ A(1:mt-1, 0) B(0, :) ] + beta C(1:mt-1, :)
            #pragma omp task depend(in:bcast[0]) \
                             depend(out:gemm[0])
            {
                internal::hemm<Target::HostTask>(
                    Side::Left,
                    alpha, A.sub( 0, 0 ),
                           B.sub( 0, 0, 0, B.nt()-1 ),
                    beta,  C.sub( 0, 0, 0, C.nt()-1 ) );

                if (A.mt()-1 > 0) {
                    internal::gemm<target>(
                        alpha, A.sub( 1, A.mt()-1, 0, 0 ),
                               B.sub( 0, 0, 0, B.nt()-1 ),
                        beta,  C.sub( 1, C.mt()-1, 0, C.nt()-1 ),
                        layout );
                }
            }

            for (int64_t k = 1; k < A.nt(); ++k) {

                // send next block col of A and block row of B
                if (k+lookahead < A.nt()) {
                    #pragma omp task depend(in:gemm[k-1]) \
                                     depend(in:bcast[k+lookahead-1]) \
                                     depend(out:bcast[k+lookahead])
                    {
                        // broadcast A(k+la, i) or A(i, k+la)
                        // to ranks owning block row C(i, :)
                        BcastListTag bcast_list_A;
                        for (int64_t i = 0; i < k+lookahead; ++i) {
                            bcast_list_A.push_back(
                                {k+lookahead, i,
                                    {C.sub( i, i, 0, C.nt()-1 )}, i} );
                        }
                        for (int64_t i = k+lookahead; i < A.mt(); ++i) {
                            bcast_list_A.push_back(
                                {i, k+lookahead,
                                    {C.sub( i, i, 0, C.nt()-1 )}, i} );
                        }
                        A.template listBcastMT<target>( bcast_list_A, layout );

                        // broadcast B(k+la, j) to ranks
                        // owning block col C(0:k+la, j)
                        BcastListTag bcast_list_B;
                        for (int64_t j = 0; j < B.nt(); ++j) {
                            bcast_list_B.push_back(
                                {k+lookahead, j,
                                    {C.sub( 0, C.mt()-1, j, j )}, j} );
                        }
                        B.template listBcastMT<target>( bcast_list_B, layout );
                    }
                }

                // multiply alpha A(:, k) B(k, :), which is:
                // C(0:k-1, :)    += alpha [ A(k, 0:k-1)^H  B(k, :) ]  gemm
                // C(k, :)        += alpha [ A(k, k)        B(k, :) ]  hemm
                // C(k+1:mt-1, :) += alpha [ A(k+1:mt-1, k) B(k, :) ]  gemm
                #pragma omp task depend(in:bcast[k]) \
                                 depend(in:gemm[k-1]) \
                                 depend(out:gemm[k])
                {
                    auto Arow_k = A.sub(k, k, 0, k-1);
                    internal::gemm<target>(
                        alpha,  conjTranspose( Arow_k ),
                                B.sub( k, k, 0, B.nt()-1 ),
                        one,    C.sub( 0, k-1, 0, C.nt()-1 ),
                        layout );

                    internal::hemm<Target::HostTask>(
                        Side::Left,
                        alpha,  A.sub( k, k ),
                                B.sub( k, k, 0, B.nt()-1 ),
                        one,    C.sub( k, k, 0, C.nt()-1 ) );

                    if (A.mt()-1 > k) {
                        internal::gemm<target>(
                            alpha,  A.sub( k+1, A.mt()-1, k, k ),
                                    B.sub( k, k, 0, B.nt()-1 ),
                            one,    C.sub( k+1, C.mt()-1, 0, C.nt()-1 ),
                            layout );
                    }
                }
            }
        }
        else {
            // ----------------------------------------
            // Left, Upper/NoTrans or Lower/ConjTrans case

            // send 1st block col (row) of A and block row of B
            #pragma omp task depend(out:bcast[0])
            {
                // broadcast A(i, 0) to ranks owning block row C(i, :)
                BcastListTag bcast_list_A;
                for (int64_t i = 0; i < A.mt(); ++i)
                    bcast_list_A.push_back(
                        {0, i, {C.sub( i, i, 0, C.nt()-1 )}, i} );
                A.template listBcastMT<target>( bcast_list_A, layout );

                BcastListTag bcast_list_B;
                // broadcast B(0, j) to ranks owning block col C(:, j)
                for (int64_t j = 0; j < B.nt(); ++j)
                    bcast_list_B.push_back(
                        {0, j, {C.sub( 0, C.mt()-1, j, j )}, j} );
                B.template listBcastMT<target>( bcast_list_B, layout );
            }

            // send next lookahead block cols of A and block rows of B
            for (int64_t k = 1; k < lookahead+1 && k < A.nt(); ++k) {
                #pragma omp task depend(in:bcast[k-1]) \
                                 depend(out:bcast[k])
                {
                    // broadcast A(k, i) or A(i, k)
                    // to ranks owning block row C(i, :)
                    BcastListTag bcast_list_A;
                    for (int64_t i = 0; i < k && i < A.mt(); ++i) {
                        bcast_list_A.push_back(
                            {i, k, {C.sub( i, i, 0, C.nt()-1 )}, i} );
                    }
                    for (int64_t i = k; i < A.mt(); ++i) {
                        bcast_list_A.push_back(
                            {k, i, {C.sub( i, i, 0, C.nt()-1 )}, i} );
                    }
                    A.template listBcastMT<target>( bcast_list_A, layout );

                    // broadcast B(k, j) to ranks owning block col C(0:k, j)
                    BcastListTag bcast_list_B;
                    for (int64_t j = 0; j < B.nt(); ++j) {
                        bcast_list_B.push_back(
                            {k, j, {C.sub( 0, C.mt()-1, j, j )}, j} );
                    }
                    B.template listBcastMT<target>( bcast_list_B, layout );
                }
            }

            // multiply alpha A(:, 0) B(0, :), which is (hemm / gemm):
            // C(0, :)      = alpha [ A(0, 0)      B(0, :) ] + beta C(0, :)
            // C(1:mt-1, :) = alpha [ A(1:mt-1, 0) B(0, :) ] + beta C(1:mt-1, :)
            #pragma omp task depend(in:bcast[0]) \
                             depend(out:gemm[0])
            {
                internal::hemm<Target::HostTask>(
                    Side::Left,
                    alpha, A.sub( 0, 0 ),
                           B.sub( 0, 0, 0, B.nt()-1 ),
                    beta,  C.sub( 0, 0, 0, C.nt()-1 ) );

                if (A.mt()-1 > 0) {
                    auto Arow_k = A.sub( 0, 0, 1, A.mt()-1 );
                    internal::gemm<target>(
                        alpha, conjTranspose( Arow_k ),
                               B.sub( 0, 0, 0, B.nt()-1 ),
                        beta,  C.sub( 1, C.mt()-1, 0, C.nt()-1 ),
                        layout );
                }
            }

            for (int64_t k = 1; k < A.nt(); ++k) {

                // send next block col of A and block row of B
                if (k+lookahead < A.nt()) {
                    #pragma omp task depend(in:gemm[k-1]) \
                                     depend(in:bcast[k+lookahead-1]) \
                                     depend(out:bcast[k+lookahead])
                    {
                        // broadcast A(k+la, i) or A(i, k+la)
                        // to ranks owning block row C(i, :)
                        BcastListTag bcast_list_A;
                        for (int64_t i = 0; i < k+lookahead; ++i) {
                            bcast_list_A.push_back(
                                {i, k+lookahead,
                                    {C.sub( i, i, 0, C.nt()-1 )}, i} );
                        }
                        for (int64_t i = k+lookahead; i < A.mt(); ++i) {
                            bcast_list_A.push_back(
                                {k+lookahead, i,
                                    {C.sub( i, i, 0, C.nt()-1 )}, i} );
                        }
                        A.template listBcastMT<target>( bcast_list_A, layout );

                        // broadcast B(k+la, j) to ranks
                        // owning block col C(0:k+la, j)
                        BcastListTag bcast_list_B;
                        for (int64_t j = 0; j < B.nt(); ++j) {
                            bcast_list_B.push_back(
                                {k+lookahead, j,
                                    {C.sub( 0, C.mt()-1, j, j )}, j} );
                        }
                        B.template listBcastMT<target>( bcast_list_B, layout );
                    }
                }

                // multiply alpha A(:, k) B(k, :), which is:
                // C(0:k-1, :)    += alpha [ A(k, 0:k-1)^H  B(k, :) ]  gemm
                // C(k, :)        += alpha [ A(k, k)        B(k, :) ]  hemm
                // C(k+1:mt-1, :) += alpha [ A(k+1:mt-1, k) B(k, :) ]  gemm
                #pragma omp task depend(in:bcast[k]) \
                                 depend(in:gemm[k-1]) \
                                 depend(out:gemm[k])
                {
                    internal::gemm<target>(
                        alpha,  A.sub( 0, k-1, k, k ),
                                B.sub( k, k, 0, B.nt()-1 ),
                        one,    C.sub( 0, k-1, 0, C.nt()-1 ),
                        layout );

                    internal::hemm<Target::HostTask>(
                        Side::Left,
                        alpha,  A.sub( k, k ),
                                B.sub( k, k, 0, B.nt()-1 ),
                        one,    C.sub( k, k, 0, C.nt()-1 ) );

                    if (A.mt()-1 > k) {
                        auto Arow_k = A.sub( k, k, k+1, A.mt()-1 );
                        internal::gemm<target>(
                            alpha,  conjTranspose( Arow_k ),
                                    B.sub( k, k, 0, B.nt()-1 ),
                            one,    C.sub( k+1, C.mt()-1, 0, C.nt()-1 ),
                            layout );
                    }
                }
            }
        }

        #pragma omp taskwait
        C.tileUpdateAllOrigin();
    }

    C.releaseWorkspace();
}

} // namespace specialization
} // namespace internal

//------------------------------------------------------------------------------
/// Version with target as template parameter.
/// @ingroup hemm_specialization
///
template <Target target, typename scalar_t>
void hemmC( Side side,
            scalar_t alpha, HermitianMatrix<scalar_t>& A,
                            Matrix<scalar_t>& B,
            scalar_t beta,  Matrix<scalar_t>& C,
            Options const& opts)
{
    int64_t lookahead = get_option<int64_t>( opts, Option::Lookahead, 1 );

    internal::specialization::hemmC( internal::TargetType<target>(),
                                     side,
                                     alpha, A,
                                            B,
                                     beta,  C,
                                     lookahead );
}

//------------------------------------------------------------------------------
/// Distributed parallel Hermitian matrix-matrix multiplication.
/// Performs one of the matrix-matrix operations
/// \[
///     C = \alpha A B + \beta C
/// \]
/// or
/// \[
///     C = \alpha B A + \beta C
/// \]
/// where alpha and beta are scalars, A is a Hermitian matrix and B and
/// C are m-by-n matrices.
///
//------------------------------------------------------------------------------
/// @tparam scalar_t
///         One of float, double, std::complex<float>, std::complex<double>.
//------------------------------------------------------------------------------
/// @param[in] side
///         Whether the Hermitian matrix A appears on the left or right:
///         - Side::Left:  $C = \alpha A B + \beta C$
///         - Side::Right: $C = \alpha B A + \beta C$
///
/// @param[in] alpha
///         The scalar alpha.
///
/// @param[in] A
///         - If side = left,  the m-by-m Hermitian matrix A;
///         - if side = right, the n-by-n Hermitian matrix A.
///
/// @param[in] B
///         The m-by-n matrix B.
///
/// @param[in] beta
///         The scalar beta.
///
/// @param[in,out] C
///         On entry, the m-by-n matrix C.
///         On exit, overwritten by the result
///         $\alpha A B + \beta C$ or $\alpha B A + \beta C$.
///
/// @param[in] opts
///         Additional options, as map of name = value pairs. Possible options:
///         - Option::Lookahead:
///           Number of blocks to overlap communication and computation.
///           lookahead >= 0. Default 1.
///         - Option::Target:
///           Implementation to target. Possible values:
///           - HostTask:  OpenMP tasks on CPU host [default].
///           - HostNest:  nested OpenMP parallel for loop on CPU host.
///           - HostBatch: batched BLAS on CPU host.
///           - Devices:   batched BLAS on GPU device.
///
/// @ingroup hemm
///
template <typename scalar_t>
void hemmC( Side side,
            scalar_t alpha, HermitianMatrix<scalar_t>& A,
                            Matrix<scalar_t>& B,
            scalar_t beta,  Matrix<scalar_t>& C,
            Options const& opts)
{
    Target target = get_option( opts, Option::Target, Target::HostTask );

    switch (target) {
        case Target::Host:
        case Target::HostTask:
            hemmC<Target::HostTask>( side, alpha, A, B, beta, C, opts );
            break;
        case Target::HostNest:
            hemmC<Target::HostNest>( side, alpha, A, B, beta, C, opts );
            break;
        case Target::HostBatch:
            hemmC<Target::HostBatch>( side, alpha, A, B, beta, C, opts );
            break;
        case Target::Devices:
            hemmC<Target::Devices>( side, alpha, A, B, beta, C, opts );
            break;
    }
}

//------------------------------------------------------------------------------
// Explicit instantiations.
template
void hemmC<float>(
    Side side,
    float alpha, HermitianMatrix<float>& A,
                 Matrix<float>& B,
    float beta,  Matrix<float>& C,
    Options const& opts);

template
void hemmC<double>(
    Side side,
    double alpha, HermitianMatrix<double>& A,
                  Matrix<double>& B,
    double beta,  Matrix<double>& C,
    Options const& opts);

template
void hemmC< std::complex<float> >(
    Side side,
    std::complex<float> alpha, HermitianMatrix< std::complex<float> >& A,
                               Matrix< std::complex<float> >& B,
    std::complex<float> beta,  Matrix< std::complex<float> >& C,
    Options const& opts);

template
void hemmC< std::complex<double> >(
    Side side,
    std::complex<double> alpha, HermitianMatrix< std::complex<double> >& A,
                                Matrix< std::complex<double> >& B,
    std::complex<double> beta,  Matrix< std::complex<double> >& C,
    Options const& opts);

} // namespace slate

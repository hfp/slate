//------------------------------------------------------------------------------
// Copyright (c) 2017, University of Tennessee
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the University of Tennessee nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL UNIVERSITY OF TENNESSEE BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//------------------------------------------------------------------------------
// This research was supported by the Exascale Computing Project (17-SC-20-SC),
// a collaborative effort of two U.S. Department of Energy organizations (Office
// of Science and the National Nuclear Security Administration) responsible for
// the planning and preparation of a capable exascale ecosystem, including
// software, applications, hardware, advanced system engineering and early
// testbed platforms, in support of the nation's exascale computing imperative.
//------------------------------------------------------------------------------
// For assistance with SLATE, email <slate-user@icl.utk.edu>.
// You can also join the "SLATE User" Google group by going to
// https://groups.google.com/a/icl.utk.edu/forum/#!forum/slate-user,
// signing in with your Google credentials, and then clicking "Join group".
//------------------------------------------------------------------------------

#ifndef SLATE_HH
#define SLATE_HH

#include "slate/Matrix.hh"
#include "slate/HermitianMatrix.hh"
#include "slate/SymmetricMatrix.hh"
#include "slate/TriangularMatrix.hh"

#include "slate/BandMatrix.hh"
#include "slate/TriangularBandMatrix.hh"
#include "slate/HermitianBandMatrix.hh"

#include "slate/types.hh"

//------------------------------------------------------------------------------
/// @namespace slate
/// SLATE's top-level namespace.
///
namespace slate {

//------------------------------------------------------------------------------
// Auxiliary
template <typename src_matrix_type, typename dst_matrix_type>
void copy(src_matrix_type& A, dst_matrix_type& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <typename scalar_t>
void set(scalar_t offdiag_value, scalar_t diag_value, Matrix<scalar_t>& A,
         const std::map<Option, Value>& opts = std::map<Option, Value>());

template <typename scalar_t>
void set(scalar_t value, Matrix<scalar_t>& A,
         const std::map<Option, Value>& opts = std::map<Option, Value>())
{
    set(value, value, A, opts);
}

//------------------------------------------------------------------------------
// Level 3 BLAS and LAPACK auxiliary

//-----------------------------------------
// gbmm()
template <typename scalar_t>
void gbmm(scalar_t alpha, BandMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void gbmm(scalar_t alpha, BandMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// geadd()
template <typename scalar_t>
void geadd(scalar_t alpha, Matrix<scalar_t>& A,
           scalar_t beta,  Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void geadd(scalar_t alpha, Matrix<scalar_t>& A,
           scalar_t beta,  Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// gemm()
template <typename scalar_t>
void gemm(scalar_t alpha, Matrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void gemm(scalar_t alpha, Matrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// hbmm()
template <typename scalar_t>
void hbmm(blas::Side side,
          scalar_t alpha, HermitianBandMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void hbmm(blas::Side side,
          scalar_t alpha, HermitianBandMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// hemm()
template <Target target, typename scalar_t>
void hemm(blas::Side side,
          scalar_t alpha, HermitianMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <typename scalar_t>
void hemm(blas::Side side,
          scalar_t alpha, HermitianMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-symmetric matrices to hemm;
// disabled for complex
template <typename scalar_t>
void hemm(Side side,
          scalar_t alpha, SymmetricMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>(),
          enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    HermitianMatrix<scalar_t> AH(A);
    hemm(side, alpha, AH, B, beta, C, opts);
}

//-----------------------------------------
// herk()
template <typename scalar_t>
void herk(blas::real_type<scalar_t> alpha, Matrix<scalar_t>& A,
          blas::real_type<scalar_t> beta,  HermitianMatrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void herk(blas::real_type<scalar_t> alpha, Matrix<scalar_t>& A,
          blas::real_type<scalar_t> beta,  HermitianMatrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-symmetric matrices to herk;
// disabled for complex
template <typename scalar_t>
void herk(blas::real_type<scalar_t> alpha, Matrix<scalar_t>& A,
          blas::real_type<scalar_t> beta,  SymmetricMatrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>(),
          enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    HermitianMatrix<scalar_t> CH(C);
    herk(alpha, A, beta, CH, opts);
}

//-----------------------------------------
// her2k()
template <typename scalar_t>
void her2k(scalar_t alpha,                 Matrix<scalar_t>& A,
                                           Matrix<scalar_t>& B,
           blas::real_type<scalar_t> beta, HermitianMatrix<scalar_t>& C,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void her2k(scalar_t alpha,                 Matrix<scalar_t>& A,
                                           Matrix<scalar_t>& B,
           blas::real_type<scalar_t> beta, HermitianMatrix<scalar_t>& C,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-symmetric matrices to her2k;
// disabled for complex
template <typename scalar_t>
void her2k(scalar_t alpha,                  Matrix<scalar_t>& A,
                                            Matrix<scalar_t>& B,
           blas::real_type<scalar_t> beta,  SymmetricMatrix<scalar_t>& C,
           const std::map<Option, Value>& opts = std::map<Option, Value>(),
           enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    HermitianMatrix<scalar_t> CH(C);
    her2k(alpha, A, B, beta, CH, opts);
}

//-----------------------------------------
// symm()
template <typename scalar_t>
void symm(blas::Side side,
          scalar_t alpha, SymmetricMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void symm(blas::Side side,
          scalar_t alpha, SymmetricMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-Hermitian matrices to symm;
// disabled for complex
template <typename scalar_t>
void symm(Side side,
          scalar_t alpha, HermitianMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          scalar_t beta,  Matrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>(),
          enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    SymmetricMatrix<scalar_t> AS(A);
    symm(side, alpha, AS, B, beta, C, opts);
}

//-----------------------------------------
// syrk()
template <typename scalar_t>
void syrk(scalar_t alpha, Matrix<scalar_t>& A,
          scalar_t beta,  SymmetricMatrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void syrk(scalar_t alpha, Matrix<scalar_t>& A,
          scalar_t beta,  SymmetricMatrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-Hermitian matrices to syrk;
// disabled for complex
template <typename scalar_t>
void syrk(scalar_t alpha, Matrix<scalar_t>& A,
          scalar_t beta,  HermitianMatrix<scalar_t>& C,
          const std::map<Option, Value>& opts = std::map<Option, Value>(),
          enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    SymmetricMatrix<scalar_t> CS(C);
    syrk(alpha, A, beta, CS, opts);
}

//-----------------------------------------
// syr2k()
template <typename scalar_t>
void syr2k(scalar_t alpha, Matrix<scalar_t>& A,
                           Matrix<scalar_t>& B,
           scalar_t beta,  SymmetricMatrix<scalar_t>& C,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void syr2k(scalar_t alpha, Matrix<scalar_t>& A,
                           Matrix<scalar_t>& B,
           scalar_t beta,  SymmetricMatrix<scalar_t>& C,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-Hermitian matrices to syr2k;
// disabled for complex
template <typename scalar_t>
void syr2k(scalar_t alpha, Matrix<scalar_t>& A,
                           Matrix<scalar_t>& B,
           scalar_t beta,  HermitianMatrix<scalar_t>& C,
           const std::map<Option, Value>& opts = std::map<Option, Value>(),
           enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    SymmetricMatrix<scalar_t> CS(C);
    syr2k(alpha, A, B, beta, CS, opts);
}

//-----------------------------------------
// tbsm()
template <typename scalar_t>
void tbsm(blas::Side side,
          scalar_t alpha, TriangularBandMatrix<scalar_t>& A, Pivots& pivots,
                          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void tbsm(blas::Side side,
          scalar_t alpha, TriangularBandMatrix<scalar_t>& A, Pivots& pivots,
                          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <typename scalar_t>
void tbsm(blas::Side side,
          scalar_t alpha, TriangularBandMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void tbsm(blas::Side side,
          scalar_t alpha, TriangularBandMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// trmm()
template <Target target, typename scalar_t>
void trmm(blas::Side side,
          scalar_t alpha, TriangularMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <typename scalar_t>
void trmm(blas::Side side,
          scalar_t alpha, TriangularMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// trsm()
template <typename scalar_t>
void trsm(blas::Side side,
          scalar_t alpha, TriangularMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void trsm(blas::Side side,
          scalar_t alpha, TriangularMatrix<scalar_t>& A,
                          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// trtri()
template <typename scalar_t>
void trtri(TriangularMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void trtri(TriangularMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// trtrm()
template <typename scalar_t>
void trtrm(TriangularMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void trtrm(TriangularMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//------------------------------------------------------------------------------
// Norms

//-----------------------------------------
// norm()
template <typename matrix_type>
blas::real_type<typename matrix_type::value_type>
norm(Norm norm, matrix_type& A,
     const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename matrix_type>
blas::real_type<typename matrix_type::value_type>
norm(Norm norm, matrix_type& A,
     const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// all cols max norm
template <typename matrix_type>
void colNorms(Norm norm, matrix_type& A,
              blas::real_type<typename matrix_type::value_type>* values,
              const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename matrix_type>
void colNorms(Norm norm, matrix_type& A,
              blas::real_type<typename matrix_type::value_type>* values,
              const std::map<Option, Value>& opts = std::map<Option, Value>());

//------------------------------------------------------------------------------
// Factorizations, etc.

//------------------------------------------------------------------------------
// Band LU
// gbsv
template <typename scalar_t>
void gbsv(BandMatrix<scalar_t>& A, Pivots& pivots,
          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void gbsv(BandMatrix<scalar_t>& A, Pivots& pivots,
          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// gbtrf
template <typename scalar_t>
void gbtrf(BandMatrix<scalar_t>& A, Pivots& pivots,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void gbtrf(BandMatrix<scalar_t>& A, Pivots& pivots,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// gbtrs
template <typename scalar_t>
void gbtrs(BandMatrix<scalar_t>& A, Pivots& pivots,
           Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void gbtrs(BandMatrix<scalar_t>& A, Pivots& pivots,
           Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//------------------------------------------------------------------------------
// LU
// gesv
template <typename scalar_t>
void gesv(Matrix<scalar_t>& A, Pivots& pivots,
          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void gesv(Matrix<scalar_t>& A, Pivots& pivots,
          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <typename scalar_t>
void gesvMixed( Matrix<scalar_t>& A, Pivots& pivots,
                Matrix<scalar_t>& B,
                Matrix<scalar_t>& X,
                int& iter,
                const std::map<Option, Value>& opts = std::map<Option, Value>());

template <typename scalar_hi, typename scalar_lo>
void gesvMixed( Matrix<scalar_hi>& A, Pivots& pivots,
                Matrix<scalar_hi>& B,
                Matrix<scalar_hi>& X,
                int& iter,
                const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// getrf
template <typename scalar_t>
void getrf(Matrix<scalar_t>& A, Pivots& pivots,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void getrf(Matrix<scalar_t>& A, Pivots& pivots,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// getrs
template <typename scalar_t>
void getrs(Matrix<scalar_t>& A, Pivots& pivots,
           Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void getrs(Matrix<scalar_t>& A, Pivots& pivots,
           Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// getri
template <Target target, typename scalar_t>
void getri(Matrix<scalar_t>& A, Pivots& pivots,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <typename scalar_t>
void getri(Matrix<scalar_t>& A, Pivots& pivots,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void getri(Matrix<scalar_t>& A, Pivots& pivots,
           Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <typename scalar_t>
void getri(Matrix<scalar_t>& A, Pivots& pivots,
           Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//------------------------------------------------------------------------------
// QR
// auxiliary type for T factors
template <typename scalar_t>
using TriangularFactors = std::vector< Matrix<scalar_t> >;

// gels
template <typename scalar_t>
void gels(Matrix<scalar_t>& A,
          TriangularFactors<scalar_t>& T,
          Matrix<scalar_t>& BX,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// geqrf
template <typename scalar_t>
void geqrf(Matrix<scalar_t>& A,
           TriangularFactors<scalar_t>& T,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void geqrf(Matrix<scalar_t>& A,
           TriangularFactors<scalar_t>& T,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// gelqf
template <typename scalar_t>
void gelqf(Matrix<scalar_t>& A,
           TriangularFactors<scalar_t>& T,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void gelqf(Matrix<scalar_t>& A,
           TriangularFactors<scalar_t>& T,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// unmqr
template <typename scalar_t>
void unmqr(Side side, Op op,
           Matrix<scalar_t>& A,
           TriangularFactors<scalar_t>& T,
           Matrix<scalar_t>& C,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void unmqr(Side side, Op op,
           Matrix<scalar_t>& A,
           TriangularFactors<scalar_t>& T,
           Matrix<scalar_t>& C,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// unmlq
template <typename scalar_t>
void unmlq(Side side, Op op,
           Matrix<scalar_t>& A,
           TriangularFactors<scalar_t>& T,
           Matrix<scalar_t>& C,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void unmlq(Side side, Op op,
           Matrix<scalar_t>& A,
           TriangularFactors<scalar_t>& T,
           Matrix<scalar_t>& C,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//------------------------------------------------------------------------------
// Band Cholesky
// pbsv
template <typename scalar_t>
void pbsv(HermitianBandMatrix<scalar_t>& A, Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void pbsv(HermitianBandMatrix<scalar_t>& A, Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// pbtrf
template <typename scalar_t>
void pbtrf(HermitianBandMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void pbtrf(HermitianBandMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// pbtrs
template <typename scalar_t>
void pbtrs(HermitianBandMatrix<scalar_t>& A, Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void pbtrs(HermitianBandMatrix<scalar_t>& A, Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());


//------------------------------------------------------------------------------
// Cholesky
// posv
template <typename scalar_t>
void posv(HermitianMatrix<scalar_t>& A, Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void posv(HermitianMatrix<scalar_t>& A, Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-symmetric matrices to potrf;
// disabled for complex
template <typename scalar_t>
void posv(SymmetricMatrix<scalar_t>& A, Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>(),
          enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    HermitianMatrix<scalar_t> AH(A);
    posv(AH, B, opts);
}

template <typename scalar_t>
void posvMixed( HermitianMatrix<scalar_t>& A,
                Matrix<scalar_t>& B,
                Matrix<scalar_t>& X,
                int& iter,
                const std::map<Option, Value>& opts = std::map<Option, Value>());

template <typename scalar_hi, typename scalar_lo>
void posvMixed( HermitianMatrix<scalar_hi>& A,
                Matrix<scalar_hi>& B,
                Matrix<scalar_hi>& X,
                int& iter,
                const std::map<Option, Value>& opts = std::map<Option, Value>());

// todo: forward real-symmetric matrices to posvMixed?

//-----------------------------------------
// potrf
template <typename scalar_t>
void potrf(HermitianMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void potrf(HermitianMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-symmetric matrices to potrf;
// disabled for complex
template <typename scalar_t>
void potrf(SymmetricMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>(),
           enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    HermitianMatrix<scalar_t> AH(A);
    potrf(AH, opts);
}

//-----------------------------------------
// potrs
template <typename scalar_t>
void potrs(HermitianMatrix<scalar_t>& A, Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void potrs(HermitianMatrix<scalar_t>& A, Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-symmetric matrices to potrs;
// disabled for complex
template <typename scalar_t>
void potrs(SymmetricMatrix<scalar_t>& A, Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>(),
           enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    HermitianMatrix<scalar_t> AH(A);
    potrs(AH, B, opts);
}

//-----------------------------------------
// potri
template <typename scalar_t>
void potri(HermitianMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void potri(HermitianMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

// todo: forward real-symmetric matrices to potri?

//------------------------------------------------------------------------------
// Symmetric indefinite -- block Aasen's
// hesv
template <typename scalar_t>
void hesv(HermitianMatrix<scalar_t>& A, Pivots& pivots,
               BandMatrix<scalar_t>& T, Pivots& pivots2,
                   Matrix<scalar_t>& H,
          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void hesv(HermitianMatrix<scalar_t>& A, Pivots& pivots,
               BandMatrix<scalar_t>& T, Pivots& pivots2,
                   Matrix<scalar_t>& H,
          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-symmetric matrices to potrf;
// disabled for complex
template <typename scalar_t>
void hesv(SymmetricMatrix<scalar_t>& A, Pivots& pivots,
               BandMatrix<scalar_t>& T, Pivots& pivots2,
                   Matrix<scalar_t>& H,
          Matrix<scalar_t>& B,
          const std::map<Option, Value>& opts = std::map<Option, Value>(),
          enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    HermitianMatrix<scalar_t> AH(A);
    hesv(AH, B, opts);
}

//-----------------------------------------
// hetrf
template <typename scalar_t>
void hetrf(HermitianMatrix<scalar_t>& A, Pivots& pivots,
                BandMatrix<scalar_t>& T, Pivots& pivots2,
                    Matrix<scalar_t>& H,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void hetrf(HermitianMatrix<scalar_t>& A, Pivots& pivots,
                BandMatrix<scalar_t>& T, Pivots& pivots2,
                    Matrix<scalar_t>& H,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-symmetric matrices to hetrf;
// disabled for complex
template <typename scalar_t>
void hetrf(SymmetricMatrix<scalar_t>& A, Pivots& pivots,
                BandMatrix<scalar_t>& T, Pivots& pivots2,
                    Matrix<scalar_t>& H,
           const std::map<Option, Value>& opts = std::map<Option, Value>(),
           enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    HermitianMatrix<scalar_t> AH(A);
    hetrf(AH, T, H, opts);
}

//-----------------------------------------
// hetrs
template <typename scalar_t>
void hetrs(HermitianMatrix<scalar_t>& A, Pivots& pivots,
                BandMatrix<scalar_t>& T, Pivots& pivots2,
                    Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void hetrs(HermitianMatrix<scalar_t>& A, Pivots& pivots,
                BandMatrix<scalar_t>& T, Pivots& pivots2,
                    Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

// forward real-symmetric matrices to hetrs;
// disabled for complex
template <typename scalar_t>
void hetrs(SymmetricMatrix<scalar_t>& A, Pivots& pivots,
                BandMatrix<scalar_t>& T, Pivots& pivots2,
                    Matrix<scalar_t>& B,
           const std::map<Option, Value>& opts = std::map<Option, Value>(),
           enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    HermitianMatrix<scalar_t> AH(A);
    hetrf(AH, T, B, opts);
}

//------------------------------------------------------------------------------
// SVD

//-----------------------------------------
template <typename scalar_t>
void gesvd(Matrix<scalar_t> A,
           std::vector< blas::real_type<scalar_t> >& S,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// ge2tb
template <typename scalar_t>
void ge2tb(Matrix<scalar_t>& A,
            TriangularFactors<scalar_t>& TU,
            TriangularFactors<scalar_t>& TV,
            const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void ge2tb(Matrix<scalar_t>& A,
            TriangularFactors<scalar_t>& TU,
            TriangularFactors<scalar_t>& TV,
            const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// Bulge Chasing: TriangularBand to Bi-diagonal
// tb2bd()
template <typename scalar_t>
void tb2bd(TriangularBandMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());


//-----------------------------------------
// Bi-diagonal SVD
//template <typename scalar_t>
//void bdsqr(std::vector< blas::real_type<scalar_t> >& D,
//           std::vector< blas::real_type<scalar_t> >& E,
//           const std::map<Option, Value>& opts = std::map<Option, Value>());
template <typename scalar_t>
void bdsqr(lapack::Job jobu, lapack::Job jobvt,
           std::vector< blas::real_type<scalar_t> >& D,
           std::vector< blas::real_type<scalar_t> >& E,
           Matrix<scalar_t>& U,
           Matrix<scalar_t>& VT,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//------------------------------------------------------------------------------
// symmetric/Hermitian eigenvalue decomposition

//-----------------------------------------
template <typename scalar_t>
void heev( HermitianMatrix<scalar_t>& A,
           std::vector< blas::real_type<scalar_t> >& W,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//------------------------------------------------------------------------------
// Generalized symmetric/Hermitian eigenvalue decomposition
template <typename scalar_t>
void hegv( int type,
           lapack::Job jobz,
           HermitianMatrix<scalar_t>& A,
           HermitianMatrix<scalar_t>& B,           
           std::vector< blas::real_type<scalar_t> >& W,
           Matrix<scalar_t>& V,           
           const std::map<Option, Value>& opts);

//-----------------------------------------
// forward real-symmetric matrices to heev;
// disabled for complex
template <typename scalar_t>
void syev( SymmetricMatrix<scalar_t>& A,
           std::vector< blas::real_type<scalar_t> >& W,
           const std::map<Option, Value>& opts = std::map<Option, Value>(),
           enable_if_t< ! is_complex<scalar_t>::value >* = nullptr)
{
    HermitianMatrix<scalar_t> AH(A);
    heev(AH, W, opts);
}

//-----------------------------------------
// he2hb()
template <typename scalar_t>
void he2hb(HermitianMatrix<scalar_t>& A,
           TriangularFactors<scalar_t>& T,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

template <Target target, typename scalar_t>
void he2hb(HermitianMatrix<scalar_t>& A,
           TriangularFactors<scalar_t>& T,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// unmtr_he2hb()
template <typename scalar_t>
void unmtr_he2hb(
    Side side, Op op, HermitianMatrix< scalar_t >& A,
    TriangularFactors< scalar_t > T,
    Matrix< scalar_t >& B,
    const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// hb2st()
template <typename scalar_t>
void hb2st(HermitianBandMatrix<scalar_t>& A,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// sterf()
template <typename scalar_t>
void sterf(std::vector< scalar_t >& D,
           std::vector< scalar_t >& E,
           const std::map<Option, Value>& opts = std::map<Option, Value>());

//-----------------------------------------
// steqr2()
template <typename scalar_t>
void steqr2(lapack::Job job,
            std::vector< blas::real_type<scalar_t> >& D,
            std::vector< blas::real_type<scalar_t> >& E,
            Matrix<scalar_t>& Z,
            const std::map<Option, Value>& opts = std::map<Option, Value>());

} // namespace slate

#endif // SLATE_HH

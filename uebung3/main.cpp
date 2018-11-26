static char help[] = "Standard symmetric eigenproblem corresponding to the Laplacian operator in 1 dimension.\n\n"
  "The command line options are:\n"
  "  -n <n>, where <n> = number of grid subdivisions = matrix dimension.\n\n";

#include <slepceps.h>
#include <stdio.h>
#include <string>
#include <cstdarg>
#include <memory>
#include "hamilton.h"
#include "lambda.h"
#include "diagonalize.h"


#undef __FUNCT__
#define __FUNCT__ "main"

std::string format(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    #ifndef _MSC_VER
        size_t size = std::snprintf( nullptr, 0, format, args) + 1; // Extra space for '\0'
        std::unique_ptr<char[]> buf( new char[ size ] );
        std::vsnprintf( buf.get(), size, format, args);
        return std::string(buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
    #else
        int size = _vscprintf(format, args);
        std::string result(++size, 0);
        vsnprintf_s((char*)result.data(), size, _TRUNCATE, format, args);
        return result;
    #endif
    va_end(args);
}

int main(int argc,char **argv)
{
  Mat            A,D, D_1,S,H;           /* problem matrix */
  EPS            eps;         /* eigenproblem solver context */
  EPSType        type;
  PetscReal      error,tol,re,im;
  PetscScalar    kr,ki;//, *v, *vi;
  Vec            xr,xi;
  PetscInt       n=8,i,Istart,Iend,nev=8,maxit,its,nconv;
  PetscErrorCode ierr;
  double a[4] = {1.0, 2.0, 3.0, 4.0};
  double Ax[2] = {0.0, 1.0};
  double Ay[2] = {0.0, 0.0};
  double Az[2] = {0.0, 0.0};
  double Kx[2] = {0.0, 1.0};
  double Ky[2] = {0.0, 0.0};
  double Kz[2] = {0.0, 0.0};
  int N_nuclei = 2;
  int N_particles = 1;
  int size = n;


  SlepcInitialize(&argc,&argv,(char*)0,help);

  ierr = PetscOptionsGetInt(NULL,NULL,"-n",&n,NULL);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"\n1-D Laplacian Eigenproblem, n=%D\n\n",n);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Compute the operator matrix that defines the eigensystem, Ax=kx
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = MatCreate(PETSC_COMM_WORLD,&A);CHKERRQ(ierr);
  ierr = MatCreate(PETSC_COMM_WORLD,&D);CHKERRQ(ierr);
  ierr = MatCreate(PETSC_COMM_WORLD,&S);CHKERRQ(ierr);
  ierr = MatSetSizes(A,PETSC_DECIDE,PETSC_DECIDE,n,n);CHKERRQ(ierr);
  ierr = MatSetSizes(D,PETSC_DECIDE,PETSC_DECIDE,n,n);CHKERRQ(ierr);
  ierr = MatSetSizes(S,PETSC_DECIDE,PETSC_DECIDE,n,n);CHKERRQ(ierr);
  ierr = MatSetFromOptions(A);CHKERRQ(ierr);
  ierr = MatSetFromOptions(D);CHKERRQ(ierr);
  ierr = MatSetFromOptions(S);CHKERRQ(ierr);
  ierr = MatSetUp(A);CHKERRQ(ierr);
  ierr = MatSetUp(D);CHKERRQ(ierr);
  ierr = MatSetUp(S);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(A,&Istart,&Iend);CHKERRQ(ierr);

  diagonalize(&D, &S, a, Ax, size);

  for (i=Istart;i<Iend;i++) {
    for (int j = Istart; j < Iend; j++) {
      double hamiltonvalue = hamiltonian(&a[i % 4], &a[j % 4], &Ax[i/4], &Ay[i/4],
        &Az[i/4], &Ax[j/4], &Ay[j/4], &Az[j/4], Kx, Ky, Kz, N_nuclei, N_particles);
      ierr = MatSetValue(A,i,j,hamiltonvalue,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  MatHermitianTranspose(D, MAT_INITIAL_MATRIX, &D_1);
  MatMatMatMult(D,A,D_1, MAT_INITIAL_MATRIX, PETSC_DEFAULT, &H);
  MatCopy(H, A, DIFFERENT_NONZERO_PATTERN);
  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  ierr = MatCreateVecs(A,NULL,&xr);CHKERRQ(ierr);
  ierr = MatCreateVecs(A,NULL,&xi);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                Create the eigensolver and set various options
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /*
     Create eigensolver context
  */
  ierr = EPSCreate(PETSC_COMM_WORLD,&eps);CHKERRQ(ierr);

  /*
     Set operators. In this case, it is a standard eigenvalue problem
  */
  ierr = EPSSetOperators(eps,A,NULL);CHKERRQ(ierr);
  ierr = EPSSetProblemType(eps,EPS_HEP);CHKERRQ(ierr);

  /*
     Set solver parameters at runtime
  */
  ierr = EPSSetFromOptions(eps);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                      Solve the eigensystem
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = EPSSolve(eps);CHKERRQ(ierr);
  /*
     Optional: Get some information from the solver and display it
  */
  ierr = EPSGetIterationNumber(eps,&its);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Number of iterations of the method: %D\n",its);CHKERRQ(ierr);
  ierr = EPSGetType(eps,&type);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Solution method: %s\n\n",type);CHKERRQ(ierr);
  ierr = EPSGetDimensions(eps,&nev,NULL,NULL);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Number of requested eigenvalues: %D\n",nev);CHKERRQ(ierr);
  ierr = EPSGetTolerances(eps,&tol,&maxit);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Stopping condition: tol=%.4g, maxit=%D\n",(double)tol,maxit);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                    Display solution and clean up
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /*
     Get number of converged approximate eigenpairs
  */
  ierr = EPSGetConverged(eps,&nconv);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Number of converged eigenpairs: %D\n\n",nconv);CHKERRQ(ierr);

  if (nconv>0) {
    /*
       Display eigenvalues and relative errors
    */
    ierr = PetscPrintf(PETSC_COMM_WORLD,
         "           k          ||Ax-kx||/||kx||\n"
         "   ----------------- ------------------\n");CHKERRQ(ierr);

    for (i=0;i<nconv;i++) {
      /*
        Get converged eigenpairs: i-th eigenvalue is stored in kr (real part) and
        ki (imaginary part)
      */
      // xr und xi sind der Eigenvektor (real + komplex)!
      ierr = EPSGetEigenpair(eps,i,&kr,&ki,xr,xi);CHKERRQ(ierr);
      /*
         Compute the relative error associated to each eigenpair
      */
      ierr = EPSComputeError(eps,i,EPS_ERROR_RELATIVE,&error);CHKERRQ(ierr);
      const PetscScalar *v;
      const PetscScalar *vi;

      VecGetArrayRead(xr, &v);
      VecGetArrayRead(xi, &vi);

#if defined(PETSC_USE_COMPLEX)
      re = PetscRealPart(kr);
      im = PetscImaginaryPart(kr);
#else
      re = kr;
      im = ki;
#endif
      if (im!=0.0) {
        ierr = PetscPrintf(PETSC_COMM_WORLD," %9f%+9fi %12g\n",(double)re,(double)im,(double)error);CHKERRQ(ierr);
      } else {
        ierr = PetscPrintf(PETSC_COMM_WORLD,"   %12f       %12g\n",(double)re,(double)error);CHKERRQ(ierr);
      }

    }
    ierr = PetscPrintf(PETSC_COMM_WORLD,"\n");CHKERRQ(ierr);
  }

  /*
     Free work space
  */
  ierr = EPSDestroy(&eps);CHKERRQ(ierr);
  ierr = MatDestroy(&A);CHKERRQ(ierr);
  ierr = MatDestroy(&D);CHKERRQ(ierr);
  ierr = MatDestroy(&S);CHKERRQ(ierr);
  ierr = VecDestroy(&xr);CHKERRQ(ierr);
  ierr = VecDestroy(&xi);CHKERRQ(ierr);
  ierr = SlepcFinalize();
  return ierr;
}

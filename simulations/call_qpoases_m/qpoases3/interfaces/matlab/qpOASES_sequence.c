/*
 *	This file is part of qpOASES.
 *
 *	qpOASES -- An Implementation of the Online Active Set Strategy.
 *	Copyright (C) 2007-2014 by Hans Joachim Ferreau, Andreas Potschka,
 *	Christian Kirches et al. All rights reserved.
 *
 *	qpOASES is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation; either
 *	version 2.1 of the License, or (at your option) any later version.
 *
 *	qpOASES is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *	See the GNU Lesser General Public License for more details.
 *
 *	You should have received a copy of the GNU Lesser General Public
 *	License along with qpOASES; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


/**
 *	\file interfaces/matlab/qpOASES_sequence.c
 *	\author Hans Joachim Ferreau, Christian Kirches, Aude Perrin
 *	\version 3.0embedded
 *	\date 2007-2014
 *
 *	Interface for Matlab(R) that enables to call qpOASES as a MEX function
 *  (variant for QPs general constraints).
 *
 */



#include <qpOASES.hpp>


using namespace qpOASES;

#include <qpOASES_matlab_utils.cpp>
#include <vector>


/*
 * QProblem instance class
 */
class QProblemInstance {
private:
		static int s_nexthandle;
public:
	QProblemInstance ( int nV, int nC );
	~QProblemInstance ();
	
	void deleteQProblemMatrices ();
	
	int handle;
//	int nV;
//	int nC;
	SQProblem* globalQP;
	QProblemB* globalQPB;
	SymmetricMatrix* H;
	Matrix* A;
	sparse_int_t *Hdiag; 
	sparse_int_t *Hir; 
	sparse_int_t *Hjc; 
	sparse_int_t *Air; 
	sparse_int_t *Ajc;
	real_t *Hv;
	real_t *Av;
};

int QProblemInstance::s_nexthandle = 1;

QProblemInstance::QProblemInstance ( int _nV, int _nC )
{
	handle = s_nexthandle++;
//	nV = _nV;
//	nC = _nC;
	
	if ( _nC > 0 )
	{
		globalQP = new SQProblem( _nV,_nC );
		globalQPB = 0;
	}
	else
	{
		globalQP = 0;
		globalQPB = new QProblemB( _nV );
	}

	H = 0;
	A = 0;
	Hdiag = 0; 
	Hir = 0; 
	Hjc = 0; 
	Air = 0; 
	Ajc = 0;
	Hv = 0;
	Av = 0;
}	

QProblemInstance::~QProblemInstance ()
{		
	deleteQProblemMatrices ();

	if ( globalQP != 0 )
	{
		delete globalQP;
		globalQP = 0;
	}

	if ( globalQPB != 0 )
	{
		delete globalQPB;
		globalQPB = 0;
	}
}

void QProblemInstance::deleteQProblemMatrices ()
{
	if ( H != 0 )
	{
		delete H;
		H = 0;
	}

	if (Hv != 0)
	{
		delete[] Hv;
		Hv = 0;
	}
	
	if (Hdiag != 0)
	{
		delete[] Hdiag;
		Hdiag = 0;
	}
	
	if (Hjc != 0)
	{
		delete[] Hjc;
		Hjc = 0;
	}
	
	if (Hir != 0)
	{
		delete[] Hir;
		Hir = 0;
	}
	
	if ( A != 0 )
	{
		delete A;
		A = 0;
	}

	if (Av != 0)
	{
		delete[] Av;
		Av = 0;
	}
	
	if (Ajc != 0)
	{
		delete[] Ajc;
		Ajc = 0;
	}
	
	if (Air != 0)
	{
		delete[] Air;
		Air = 0;
	}	
}


/* 
 * global pointer to QP objects 
 */

static std::vector<QProblemInstance *> g_instances;

/*
 *	a l l o c a t e Q P r o b l e m I n s t a n c e
 */
int allocateQProblemInstance( int nV, int nC, Options *options )
{
	QProblemInstance *inst = new QProblemInstance (nV, nC);

	if ( nC > 0 )
		inst->globalQP->setOptions ( *options );
	else
		inst->globalQPB->setOptions ( *options );

	g_instances.push_back (inst);
	return inst->handle;
	return 0;
}

/*
 *  g e t Q P r o b l e m I n s t a n c e
 */
QProblemInstance * getQProblemInstance ( int handle )
{
	unsigned int ii;
	// TODO: this may become slow ...
	for (ii = 0; ii < g_instances.size (); ++ii)
		if (g_instances[ii]->handle == handle)
			return g_instances[ii];
	return 0;
}

/*
 *	d e l e t e Q P r o b l e m I n s t a n c e
 */
void deleteQProblemInstance( int handle )
{
	QProblemInstance *instance = getQProblemInstance (handle);
	if (instance != 0) {
		for (std::vector<QProblemInstance*>::iterator itor = g_instances.begin ();
		     itor != g_instances.end (); ++itor)
		     if ((*itor)->handle == handle) {
				g_instances.erase (itor);
				break;
			}
		delete instance;
	}
}




/*
 *	i n i t S B
 */
int initSB(	int handle, 
			SymmetricMatrix *H, real_t* g,
			const real_t* const lb, const real_t* const ub,
			int nWSR, const real_t* const x0, Options* options,
			int nOutputs, mxArray* plhs[],
			double* guessedBounds
			)
{
	/* 1) setup initial QP. */
	QProblemB *globalQP = getQProblemInstance (handle)->globalQPB;

	if ( globalQP == 0 )
	{
		myMexErrMsgTxt( "ERROR (qpOASES): Invalid handle to QP instance!" );
		return -1;
	}

	globalQP->setOptions ( *options );
	
	/* 2) Solve initial QP. */
	returnValue returnvalue;
	int nV = globalQP->getNV ();
	
	/* 3) Fill the working set. */
	Bounds bounds(nV);
	if (guessedBounds != 0) {
		for (int i = 0; i < nV; i++) {
			if ( isEqual(guessedBounds[i],-1.0) == BT_TRUE ) {
				bounds.setupBound(i, ST_LOWER);
			} else if ( isEqual(guessedBounds[i],1.0) == BT_TRUE ) {
				bounds.setupBound(i, ST_UPPER);
			} else if ( isEqual(guessedBounds[i],0.0) == BT_TRUE ) {
				bounds.setupBound(i, ST_INACTIVE);
			} else {
				char msg[200];
				snprintf(msg, 199,
						"ERROR (qpOASES): Only {-1, 0, 1} allowed for status of bounds!");
				myMexErrMsgTxt(msg);
				return -1;
			}
		}
	}

	if (x0 == 0 && guessedBounds == 0)
		returnvalue = globalQP->init(H, g, lb, ub, nWSR, 0);
	else
		returnvalue = globalQP->init(H, g, lb, ub, nWSR, 0, x0, 0,
				guessedBounds != 0 ? &bounds : 0);

	/* 3) Assign lhs arguments. */
	obtainOutputs(	0,globalQP,returnvalue,nWSR,
					nOutputs,plhs,nV,0,handle );

	return 0;
}



/*
 *	i n i t
 */
int init(	int handle, 
			SymmetricMatrix *H, real_t* g, Matrix *A,
			const real_t* const lb, const real_t* const ub, const real_t* const lbA, const real_t* const ubA,
			int nWSR, const real_t* const x0, Options* options,
			int nOutputs, mxArray* plhs[],
			double* guessedBounds, double* guessedConstraints
			)
{
	/* 1) setup initial QP. */
	SQProblem *globalQP = getQProblemInstance (handle)->globalQP;

	if ( globalQP == 0 )
	{
		myMexErrMsgTxt( "ERROR (qpOASES): Invalid handle to QP instance!" );
		return -1;
	}

	globalQP->setOptions ( *options );
	
	/* 2) Solve initial QP. */
	returnValue returnvalue;
	int nV = globalQP->getNV ();
	int nC = globalQP->getNC ();
	
	/* 3) Fill the working set. */
	Bounds bounds(nV);
	Constraints constraints(nC);
	if (guessedBounds != 0) {
		for (int i = 0; i < nV; i++) {
			if ( isEqual(guessedBounds[i],-1.0) == BT_TRUE ) {
				bounds.setupBound(i, ST_LOWER);
			} else if ( isEqual(guessedBounds[i],1.0) == BT_TRUE ) {
				bounds.setupBound(i, ST_UPPER);
			} else if ( isEqual(guessedBounds[i],0.0) == BT_TRUE ) {
				bounds.setupBound(i, ST_INACTIVE);
			} else {
				char msg[200];
				snprintf(msg, 199,
						"ERROR (qpOASES): Only {-1, 0, 1} allowed for status of bounds!");
				myMexErrMsgTxt(msg);
				return -1;
			}
		}
	}

	if (guessedConstraints != 0) {
		for (int i = 0; i < nC; i++) {
			if ( isEqual(guessedConstraints[i],-1.0) == BT_TRUE ) {
				constraints.setupConstraint(i, ST_LOWER);
			} else if ( isEqual(guessedConstraints[i],1.0) == BT_TRUE ) {
				constraints.setupConstraint(i, ST_UPPER);
			} else if ( isEqual(guessedConstraints[i],0.0) == BT_TRUE ) {
				constraints.setupConstraint(i, ST_INACTIVE);
			} else {
				char msg[200];
				snprintf(msg, 199,
						"ERROR (qpOASES): Only {-1, 0, 1} allowed for status of constraints!");
				myMexErrMsgTxt(msg);
				return -1;
			}
		}
	}

	if (x0 == 0 && guessedBounds == 0 && guessedConstraints == 0)
		returnvalue = globalQP->init(H, g, A, lb, ub, lbA, ubA, nWSR, 0);
	else
		returnvalue = globalQP->init(H, g, A, lb, ub, lbA, ubA, nWSR, 0, x0, 0,
				guessedBounds != 0 ? &bounds : 0,
				guessedConstraints != 0 ? &constraints : 0);

	/* 3) Assign lhs arguments. */
	obtainOutputs(	0,globalQP,returnvalue,nWSR,
					nOutputs,plhs,nV,nC,handle );

	return 0;
}



/*
 *	h o t s t a r t S B
 */
int hotstartSB(	int handle,
                const real_t* const g,
				const real_t* const lb, const real_t* const ub,
				int nWSR, Options* options,
				int nOutputs, mxArray* plhs[]
				)
{
	QProblemB *globalQP = getQProblemInstance (handle)->globalQPB;

	if ( globalQP == 0 )
	{
		myMexErrMsgTxt( "ERROR (qpOASES): QP needs to be initialised first!" );
		return -1;
	}

	/* 1) Solve QP with given options. */
	globalQP->setOptions( *options );
	returnValue returnvalue = globalQP->hotstart( g,lb,ub, nWSR,0 );

	/* 2) Assign lhs arguments. */
	obtainOutputs(	0,globalQP,returnvalue,nWSR,
					nOutputs,plhs,0,0 );

	return 0;
}


/*
 *	h o t s t a r t
 */
int hotstart(	int handle,
                const real_t* const g,
				const real_t* const lb, const real_t* const ub,
				const real_t* const lbA, const real_t* const ubA,
				int nWSR, Options* options,
				int nOutputs, mxArray* plhs[]
				)
{
	QProblem *globalQP = getQProblemInstance (handle)->globalQP;

	if ( globalQP == 0 )
	{
		myMexErrMsgTxt( "ERROR (qpOASES): QP needs to be initialised first!" );
		return -1;
	}

	/* 1) Solve QP with given options. */
	globalQP->setOptions( *options );
	returnValue returnvalue = globalQP->hotstart( g,lb,ub,lbA,ubA, nWSR,0 );

	/* 2) Assign lhs arguments. */
	obtainOutputs(	0,globalQP,returnvalue,nWSR,
					nOutputs,plhs,0,0 );

	return 0;
}


/*
 *	h o t s t a r t V M
 */
int hotstartVM(	int handle,
                    SymmetricMatrix *H, real_t* g, Matrix *A,
					const real_t* const lb, const real_t* const ub, const real_t* const lbA, const real_t* const ubA,
					int nWSR, Options* options,
					int nOutputs, mxArray* plhs[]
					)
{
	SQProblem *globalQP = getQProblemInstance (handle)->globalQP;

	if ( globalQP == 0 )
	{
		myMexErrMsgTxt( "ERROR (qpOASES): QP needs to be initialised first!" );
		return -1;
	}

	/* 1) Solve QP. */
	globalQP->setOptions( *options );
	returnValue returnvalue = globalQP->hotstart( H,g,A,lb,ub,lbA,ubA, nWSR,0 );

	if (returnvalue != SUCCESSFUL_RETURN)
	{
		myMexErrMsgTxt( "ERROR (qpOASES): Hotstart failed." );
		return -1;
	}

	/* 2) Assign lhs arguments. */
	obtainOutputs(	0,globalQP,returnvalue,nWSR,
					nOutputs,plhs,0,0 );

	return 0;
}


/*
 *	m e x F u n c t i o n
 */
void mexFunction( int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[] )
{
	unsigned int i;

	/* inputs */
	char typeString[2];
	real_t *H_for=0, *H_mem=0, *g=0, *A_for=0, *A_mem=0, *lb=0, *ub=0, *lbA=0, *ubA=0, *x0=0;

	double *guessedBoundsAndConstraints = 0;
	double *guessedBounds = 0, *guessedConstraints = 0;

	BooleanType isSimplyBoundedQp = BT_FALSE;

	Options options;
	options.printLevel = PL_LOW;
	#ifdef __DEBUG__
	options.printLevel = PL_HIGH;
	#endif
	#ifdef __SUPPRESSANYOUTPUT__
	options.printLevel = PL_NONE;
	#endif

	/* dimensions */
	unsigned int nV=0, nC=0, handle=0;
	int nWSRin;
	QProblemInstance *globalQP = 0;

	/* I) CONSISTENCY CHECKS: */
	/* 1) Ensure that qpOASES is called with a feasible number of input arguments. */
	if ( ( nrhs < 6 ) || ( nrhs > 11 ) )
	{
		if ( nrhs != 2 )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid number of input arguments!\nType 'help qpOASES_sequence' for further information." );
			return;
		}
	}
	
	/* 2) Ensure that first input is a string ... */
	if ( mxIsChar( prhs[0] ) != 1 )
	{
		myMexErrMsgTxt( "ERROR (qpOASES): First input argument must be a string!" );
		return;
	}

	mxGetString( prhs[0], typeString, 2 );

	/*    ... and if so, check if it is an allowed one. */
	if ( ( strcmp( typeString,"i" ) != 0 ) && ( strcmp( typeString,"I" ) != 0 ) &&
		 ( strcmp( typeString,"h" ) != 0 ) && ( strcmp( typeString,"H" ) != 0 ) &&
		 ( strcmp( typeString,"m" ) != 0 ) && ( strcmp( typeString,"M" ) != 0 ) &&
		 ( strcmp( typeString,"e" ) != 0 ) && ( strcmp( typeString,"E" ) != 0 ) &&
		 ( strcmp( typeString,"c" ) != 0 ) && ( strcmp( typeString,"C" ) != 0 ) )
	{
		myMexErrMsgTxt( "ERROR (qpOASES): Undefined first input argument!\nType 'help qpOASES_sequence' for further information." );
		return;
	}


	/* II) SELECT RESPECTIVE QPOASES FUNCTION CALL: */
	/* 1) Init (without or with initial guess for primal solution). */
	if ( ( strcmp( typeString,"i" ) == 0 ) || ( strcmp( typeString,"I" ) == 0 ) )
	{
		/* consistency checks */
		if ( ( nlhs < 2 ) || ( nlhs > 7 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid number of output arguments!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		if ( ( nrhs < 5 ) || ( nrhs > 11 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid number of input arguments!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		/* determine whether is it a simply bounded QP */
		if ( ( nrhs < 8 ) || ( ( nrhs == 8 ) && ( !mxIsEmpty(prhs[5]) ) && ( mxIsStruct(prhs[5]) ) ) )
			isSimplyBoundedQp = BT_TRUE;
		else
			isSimplyBoundedQp = BT_FALSE;


		if ( isSimplyBoundedQp == BT_TRUE )
		{
			/* ensure that data is given in double precision */
			if ( ( mxIsDouble( prhs[1] ) == 0 ) ||
				 ( mxIsDouble( prhs[2] ) == 0 ) )
			{
				myMexErrMsgTxt( "ERROR (qpOASES): All data has to be provided in double precision!" );
				return;
			}

			/* Check inputs dimensions and assign pointers to inputs. */
			nV = mxGetM( prhs[1] ); /* row number of Hessian matrix */
			nC = 0; /* row number of constraint matrix */

			if ( mxGetN( prhs[1] ) != nV )
			{
				myMexErrMsgTxt( "ERROR (qpOASES): Input dimension mismatch!" );
				return;
			}

			if ( smartDimensionCheck( &g,nV,1, BT_FALSE,prhs,2 ) != SUCCESSFUL_RETURN )
				return;

			if ( smartDimensionCheck( &lb,nV,1, BT_TRUE,prhs,3 ) != SUCCESSFUL_RETURN )
				return;

			if ( smartDimensionCheck( &ub,nV,1, BT_TRUE,prhs,4 ) != SUCCESSFUL_RETURN )
				return;

			/* default value for nWSR */
			nWSRin = 5*nV;

			/* Check whether x0 and options are specified .*/
			if ( nrhs > 5 )
			{
				if ((!mxIsEmpty(prhs[5])) && (mxIsStruct(prhs[5])))
						setupOptions(&options, prhs[5], nWSRin);

				if (nrhs > 6)
				{
					if ( smartDimensionCheck( &x0,nV,1, BT_TRUE,prhs,6 ) != SUCCESSFUL_RETURN )
					return;				

					if (nrhs > 7)
					{
						if (smartDimensionCheck(&guessedBoundsAndConstraints,
								nV, 1, BT_TRUE, prhs, 7) != SUCCESSFUL_RETURN)
							return;
					}
				}
			}
		}
		else
		{
			/* ensure that data is given in double precision */
			if ( ( mxIsDouble( prhs[1] ) == 0 ) ||
				 ( mxIsDouble( prhs[2] ) == 0 ) ||
				 ( mxIsDouble( prhs[3] ) == 0 ) )
			{
				myMexErrMsgTxt( "ERROR (qpOASES): All data has to be provided in double precision!" );
				return;
			}
		
			/* Check inputs dimensions and assign pointers to inputs. */
			nV = mxGetM( prhs[1] ); /* row number of Hessian matrix */
			nC = mxGetM( prhs[3] ); /* row number of constraint matrix */

			if ( ( mxGetN( prhs[1] ) != nV ) || ( ( mxGetN( prhs[3] ) != 0 ) && ( mxGetN( prhs[3] ) != nV ) ) )
			{
				myMexErrMsgTxt( "ERROR (qpOASES): Input dimension mismatch!" );
				return;
			}
		
			if ( smartDimensionCheck( &g,nV,1, BT_FALSE,prhs,2 ) != SUCCESSFUL_RETURN )
				return;

			if ( smartDimensionCheck( &lb,nV,1, BT_TRUE,prhs,4 ) != SUCCESSFUL_RETURN )
				return;

			if ( smartDimensionCheck( &ub,nV,1, BT_TRUE,prhs,5 ) != SUCCESSFUL_RETURN )
				return;

			if ( smartDimensionCheck( &lbA,nC,1, BT_TRUE,prhs,6 ) != SUCCESSFUL_RETURN )
				return;
			
			if ( smartDimensionCheck( &ubA,nC,1, BT_TRUE,prhs,7 ) != SUCCESSFUL_RETURN )
				return;

			/* default value for nWSR */
			nWSRin = 5*(nV+nC);

			/* Check whether x0 and options are specified .*/
			if ( nrhs > 8 )
			{
				if ((!mxIsEmpty(prhs[8])) && (mxIsStruct(prhs[8])))
						setupOptions(&options, prhs[8], nWSRin);

				if (nrhs > 9)
				{
					if ( smartDimensionCheck( &x0,nV,1, BT_TRUE,prhs,9 ) != SUCCESSFUL_RETURN )
					return;				

					if (nrhs > 10)
					{
						if (smartDimensionCheck(&guessedBoundsAndConstraints,
								nV + nC, 1, BT_TRUE, prhs, 10) != SUCCESSFUL_RETURN)
							return;
					}
				}
			}
		}

		/* allocate instance */
		handle = allocateQProblemInstance( nV,nC, &options );	
		globalQP = getQProblemInstance ( handle );

		/* check for sparsity */
		if ( mxIsSparse( prhs[1] ) != 0 )
		{
			mwIndex *oct_ir = mxGetIr(prhs[1]);
			mwIndex *oct_jc = mxGetJc(prhs[1]);
			double *v = (double*)mxGetPr(prhs[1]);
			long nfill = 0;
			unsigned long i, j;

			/* copy indices to avoid 64/32-bit integer confusion */
			/* also add explicit zeros on diagonal for regularization strategy */
			/* copy values, too */
			globalQP->Hir = new sparse_int_t[oct_jc[nV] + nV];
			globalQP->Hjc = new sparse_int_t[nV+1];
			globalQP->Hv = new real_t[oct_jc[nV] + nV];
			for (j = 0; j < nV; j++) 
			{
				globalQP->Hjc[j] = oct_jc[j] + nfill;
				/* fill up to diagonal */
				for (i = oct_jc[j]; i < oct_jc[j+1] && oct_ir[i] <= j; i++) 
				{
					globalQP->Hir[i + nfill] = oct_ir[i];
					globalQP->Hv[i + nfill] = v[i];
				}
				/* possibly add zero diagonal element */
				if (i >= oct_jc[j+1] || oct_ir[i] > j)
				{
					globalQP->Hir[i + nfill] = j;
					globalQP->Hv[i + nfill] = 0.0;
					nfill++;
				}
				/* fill up to diagonal */
				for (; i < oct_jc[j+1]; i++) 
				{
					globalQP->Hir[i + nfill] = oct_ir[i];
					globalQP->Hv[i + nfill] = v[i];
				}
			}
			globalQP->Hjc[nV] = oct_jc[nV] + nfill;

			SymSparseMat *sH;
			globalQP->H = sH = new SymSparseMat(nV, nV, globalQP->Hir, globalQP->Hjc, globalQP->Hv);
			globalQP->Hdiag = sH->createDiagInfo();
		}
		else
		{
			H_for = (real_t*) mxGetPr( prhs[1] );
			H_mem = new real_t[nV*nV];
			for( i=0; i<nV*nV; ++i )
				H_mem[i] = H_for[i];
			globalQP->H = new SymDenseMat( nV, nV, nV, H_mem );
			globalQP->H->doFreeMemory();
		}

		/* Convert constraint matrix A from FORTRAN to C style
		 * (not necessary for H as it should be symmetric!). */
		if ( nC > 0 )
		{
			/* Check for sparsity. */
			if ( mxIsSparse( prhs[3] ) != 0 )
			{
				mwIndex *oct_ir = mxGetIr(prhs[3]);
				mwIndex *oct_jc = mxGetJc(prhs[3]);
				double *v = (double*)mxGetPr(prhs[3]);

				/* copy indices to avoid 64/32-bit integer confusion */
				globalQP->Air = new sparse_int_t[oct_jc[nV]];
				globalQP->Ajc = new sparse_int_t[nV+1];
				for (unsigned long i = 0; i < oct_jc[nV]; i++) globalQP->Air[i] = oct_ir[i];
				for (unsigned long i = 0; i < nV + 1; i++) globalQP->Ajc[i] = oct_jc[i];

				/* copy values, too */
				globalQP->Av = new real_t[globalQP->Ajc[nV]];
				for (long i = 0; i < globalQP->Ajc[nV]; i++) globalQP->Av[i] = v[i];

				globalQP->A = new SparseMatrix(nC, nV, globalQP->Air, globalQP->Ajc, globalQP->Av);
			}
			else
			{
				/* Convert constraint matrix A from FORTRAN to C style
				* (not necessary for H as it should be symmetric!). */
				A_for = (real_t*) mxGetPr( prhs[3] );
				A_mem = new real_t[nC*nV];
				convertFortranToC( A_for,nV,nC, A_mem );
				globalQP->A = new DenseMatrix(nC, nV, nV, A_mem );
				globalQP->A->doFreeMemory();
			}
		}

		/* Create output vectors and assign pointers to them. */
		allocateOutputs( nlhs,plhs, nV,nC,1,handle );

		/* Call qpOASES. */
		if ( isSimplyBoundedQp == BT_TRUE )
		{
			initSB(	handle,
					globalQP->H,g,
					lb,ub,
					nWSRin,x0,&options,
					nlhs,plhs,
					guessedBounds
					);
		}
		else
		{
			init(	handle,
					globalQP->H,g,globalQP->A,
					lb,ub,lbA,ubA,
					nWSRin,x0,&options,
					nlhs,plhs,
					guessedBounds, guessedConstraints
					);
		}

		return;
	}

	/* 2) Hotstart. */
	if ( ( strcmp( typeString,"h" ) == 0 ) || ( strcmp( typeString,"H" ) == 0 ) )
	{
		/* consistency checks */
		if ( ( nlhs < 1 ) || ( nlhs > 6 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid number of output arguments!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		if ( ( nrhs < 5 ) || ( nrhs > 8 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid number of input arguments!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		/* determine whether is it a simply bounded QP */
		if ( nrhs < 7 )
			isSimplyBoundedQp = BT_TRUE;
		else
			isSimplyBoundedQp = BT_FALSE;


		if ( ( mxIsDouble( prhs[1] ) == false ) || ( mxGetM( prhs[1] ) != 1 ) || ( mxGetN( prhs[1] ) != 1 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Expecting a handle to QP object as second argument!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		/* get QP instance */
		handle = (unsigned int)mxGetScalar( prhs[1] );
		globalQP = getQProblemInstance ( handle );
		if ( globalQP == 0 )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid handle to QP instance!" );
			return;
		}


		/* Check inputs dimensions and assign pointers to inputs. */
		if ( isSimplyBoundedQp == BT_TRUE )
		{
			nV = globalQP->globalQPB->getNV( );
			nC = 0;

			if ( smartDimensionCheck( &g,nV,1, BT_FALSE,prhs,2 ) != SUCCESSFUL_RETURN )
				return;

			if ( smartDimensionCheck( &lb,nV,1, BT_TRUE,prhs,3 ) != SUCCESSFUL_RETURN )
				return;

			if ( smartDimensionCheck( &ub,nV,1, BT_TRUE,prhs,4 ) != SUCCESSFUL_RETURN )
				return;

			/* default value for nWSR */
			nWSRin = 5*nV;

			/* Check whether options are specified .*/
			if ( nrhs == 6 )
				if ( ( !mxIsEmpty( prhs[5] ) ) && ( mxIsStruct( prhs[5] ) ) )
					setupOptions( &options,prhs[5],nWSRin );
		}
		else
		{
			nV = globalQP->globalQP->getNV( );
			nC = globalQP->globalQP->getNC( );

			if ( smartDimensionCheck( &g,nV,1, BT_FALSE,prhs,2 ) != SUCCESSFUL_RETURN )
				return;

			if ( smartDimensionCheck( &lb,nV,1, BT_TRUE,prhs,3 ) != SUCCESSFUL_RETURN )
				return;

			if ( smartDimensionCheck( &ub,nV,1, BT_TRUE,prhs,4 ) != SUCCESSFUL_RETURN )
				return;

			if ( smartDimensionCheck( &lbA,nC,1, BT_TRUE,prhs,5 ) != SUCCESSFUL_RETURN )
				return;

			if ( smartDimensionCheck( &ubA,nC,1, BT_TRUE,prhs,6 ) != SUCCESSFUL_RETURN )
				return;

			/* default value for nWSR */
			nWSRin = 5*(nV+nC);

			/* Check whether options are specified .*/
			if ( nrhs == 8 )
				if ( ( !mxIsEmpty( prhs[7] ) ) && ( mxIsStruct( prhs[7] ) ) )
					setupOptions( &options,prhs[7],nWSRin );
		}

		/* Create output vectors and assign pointers to them. */
		allocateOutputs( nlhs,plhs, nV,nC );

		/* call qpOASES */
		if ( isSimplyBoundedQp == BT_TRUE )
		{
			hotstartSB(	handle, g,
						lb,ub,
						nWSRin,&options,
						nlhs,plhs
						);
		}
		else
		{
			hotstart(	handle, g,
						lb,ub,lbA,ubA,
						nWSRin,&options,
						nlhs,plhs
						);
		}

		return;
	}

	/* 3) Modify matrices. */
	if ( ( strcmp( typeString,"m" ) == 0 ) || ( strcmp( typeString,"M" ) == 0 ) )
	{
		/* consistency checks */
		if ( ( nlhs < 1 ) || ( nlhs > 6 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid number of output arguments!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		if ( ( nrhs < 9 ) || ( nrhs > 10 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid number of input arguments!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		if ( ( mxIsDouble( prhs[1] ) == false ) || ( mxGetM( prhs[1] ) != 1 ) || ( mxGetN( prhs[1] ) != 1 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Expecting a handle to QP object as second argument!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		/* ensure that data is given in double precision */
		if ( ( mxIsDouble( prhs[2] ) == 0 ) ||
			 ( mxIsDouble( prhs[3] ) == 0 ) ||
			 ( mxIsDouble( prhs[4] ) == 0 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): All data has to be provided in real_t precision!" );
			return;
		}

		/* get QP instance */
		handle = (unsigned int)mxGetScalar( prhs[1]);
		globalQP = getQProblemInstance ( handle );
		if ( globalQP == 0 )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid handle to QP instance!" );
			return;
		}

		/* Check inputs dimensions and assign pointers to inputs. */
		nV = mxGetM( prhs[2] ); /* row number of Hessian matrix */
		nC = mxGetM( prhs[4] ); /* row number of constraint matrix */
		
		/* Check that dimensions are consistent with existing QP instance */
		if (nV != (unsigned int) globalQP->globalQP->getNV () || nC != (unsigned int) globalQP->globalQP->getNC ())
		{
			myMexErrMsgTxt( "ERROR (qpOASES): QP dimensions must be constant during a sequence! Try creating a new QP instance instead." );
			return;
		}

		if ( ( mxGetN( prhs[2] ) != nV ) || ( ( mxGetN( prhs[4] ) != 0 ) && ( mxGetN( prhs[4] ) != nV ) ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Input dimension mismatch!" );
			return;
		}

		if ( smartDimensionCheck( &g,nV,1, BT_FALSE,prhs,3 ) != SUCCESSFUL_RETURN )
			return;

		if ( smartDimensionCheck( &lb,nV,1, BT_TRUE,prhs,5 ) != SUCCESSFUL_RETURN )
			return;

		if ( smartDimensionCheck( &ub,nV,1, BT_TRUE,prhs,6 ) != SUCCESSFUL_RETURN )
			return;

		if ( smartDimensionCheck( &lbA,nC,1, BT_TRUE,prhs,7 ) != SUCCESSFUL_RETURN )
			return;

		if ( smartDimensionCheck( &ubA,nC,1, BT_TRUE,prhs,8 ) != SUCCESSFUL_RETURN )
			return;

		/* default value for nWSR */
		nWSRin = 5*(nV+nC);

		/* Check whether options are specified .*/
		if ( nrhs > 9 )
			if ( ( !mxIsEmpty( prhs[9] ) ) && ( mxIsStruct( prhs[9] ) ) )
				setupOptions( &options,prhs[9],nWSRin );

		globalQP->deleteQProblemMatrices( );

		/* check for sparsity */
		if ( mxIsSparse( prhs[2] ) != 0 )
		{
			mwIndex *oct_ir = mxGetIr(prhs[2]);
			mwIndex *oct_jc = mxGetJc(prhs[2]);
			double *v = (double*)mxGetPr(prhs[2]);
			long nfill = 0;
			unsigned long i, j;

			/* copy indices to avoid 64/32-bit integer confusion */
			/* also add explicit zeros on diagonal for regularization strategy */
			/* copy values, too */
			globalQP->Hir = new sparse_int_t[oct_jc[nV] + nV];
			globalQP->Hjc = new sparse_int_t[nV+1];
			globalQP->Hv = new real_t[oct_jc[nV] + nV];
			for (j = 0; j < nV; j++) 
			{
				globalQP->Hjc[j] = oct_jc[j] + nfill;
				/* fill up to diagonal */
				for (i = oct_jc[j]; i < oct_jc[j+1] && oct_ir[i] <= j; i++) 
				{
					globalQP->Hir[i + nfill] = oct_ir[i];
					globalQP->Hv[i + nfill] = v[i];
				}
				/* possibly add zero diagonal element */
				if (i >= oct_jc[j+1] || oct_ir[i] > j)
				{
					globalQP->Hir[i + nfill] = j;
					globalQP->Hv[i + nfill] = 0.0;
					nfill++;
				}
				/* fill up to diagonal */
				for (; i < oct_jc[j+1]; i++) 
				{
					globalQP->Hir[i + nfill] = oct_ir[i];
					globalQP->Hv[i + nfill] = v[i];
				}
			}
			globalQP->Hjc[nV] = oct_jc[nV] + nfill;

			SymSparseMat *sH;
			globalQP->H = sH = new SymSparseMat(nV, nV, globalQP->Hir, globalQP->Hjc, globalQP->Hv);
			globalQP->Hdiag = sH->createDiagInfo();
		}
		else
		{
			H_for = (real_t*) mxGetPr( prhs[2] );
			H_mem = new real_t[nV*nV];
			for( i=0; i<nV*nV; ++i )
				H_mem[i] = H_for[i];
			globalQP->H = new SymDenseMat( nV, nV, nV, H_mem );
			globalQP->H->doFreeMemory();
		}

		/* Convert constraint matrix A from FORTRAN to C style
		 * (not necessary for H as it should be symmetric!). */
		if ( nC > 0 )
		{
			/* Check for sparsity. */
			if ( mxIsSparse( prhs[4] ) != 0 )
			{
				mwIndex *oct_ir = mxGetIr(prhs[4]);
				mwIndex *oct_jc = mxGetJc(prhs[4]);
				double *v = (double*)mxGetPr(prhs[4]);

				/* copy indices to avoid 64/32-bit integer confusion */
				globalQP->Air = new sparse_int_t[oct_jc[nV]];
				globalQP->Ajc = new sparse_int_t[nV+1];
				for (unsigned long i = 0; i < oct_jc[nV]; i++) globalQP->Air[i] = oct_ir[i];
				for (unsigned long i = 0; i < nV + 1; i++) globalQP->Ajc[i] = oct_jc[i];

				/* copy values, too */
				globalQP->Av = new real_t[globalQP->Ajc[nV]];
				for (long i = 0; i < globalQP->Ajc[nV]; i++) globalQP->Av[i] = v[i];

				globalQP->A = new SparseMatrix(nC, nV, globalQP->Air, globalQP->Ajc, globalQP->Av);
			}
			else
			{
				/* Convert constraint matrix A from FORTRAN to C style
				* (not necessary for H as it should be symmetric!). */
				A_for = (real_t*) mxGetPr( prhs[4] );
				A_mem = new real_t[nC*nV];
				convertFortranToC( A_for,nV,nC, A_mem );
				globalQP->A = new DenseMatrix(nC, nV, nV, A_mem );
				globalQP->A->doFreeMemory();
			}
		}

		/* Create output vectors and assign pointers to them. */
		allocateOutputs( nlhs,plhs, nV,nC );

		/* Call qpOASES */
		hotstartVM(	handle, globalQP->H,g,globalQP->A,
					lb,ub,lbA,ubA,
					nWSRin,&options,
					nlhs,plhs
					);

		return;
	}

	/* 4) Solve current equality constrained QP. */
	if ( ( strcmp( typeString,"e" ) == 0 ) || ( strcmp( typeString,"E" ) == 0 ) )
	{
		/* consistency checks */
		if ( ( nlhs < 1 ) || ( nlhs > 3 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid number of output arguments!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		if ( ( nrhs < 7 ) || ( nrhs > 8 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid number of input arguments!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		if ( ( mxIsDouble( prhs[1] ) == false ) || ( mxGetM( prhs[1] ) != 1 ) || ( mxGetN( prhs[1] ) != 1 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Expecting a handle to QP object as second argument!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		/* get QP instance */
		handle = (unsigned int)mxGetScalar( prhs[1] );
		globalQP = getQProblemInstance ( handle );
		if ( globalQP == 0 )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid handle to QP instance!" );
			return;
		}

		/* Check inputs dimensions and assign pointers to inputs. */
		int nRHS = mxGetN(prhs[2]);
		nV = globalQP->globalQP->getNV( );
		nC = globalQP->globalQP->getNC( );
		real_t *x_out, *y_out;

		if ( smartDimensionCheck( &g,nV,nRHS, BT_FALSE,prhs,2 ) != SUCCESSFUL_RETURN )
			return;

		if ( smartDimensionCheck( &lb,nV,nRHS, BT_TRUE,prhs,3 ) != SUCCESSFUL_RETURN )
			return;

		if ( smartDimensionCheck( &ub,nV,nRHS, BT_TRUE,prhs,4 ) != SUCCESSFUL_RETURN )
			return;

		if ( smartDimensionCheck( &lbA,nC,nRHS, BT_TRUE,prhs,5 ) != SUCCESSFUL_RETURN )
			return;

		if ( smartDimensionCheck( &ubA,nC,nRHS, BT_TRUE,prhs,6 ) != SUCCESSFUL_RETURN )
			return;

		/* Check whether options are specified .*/
		if ( (  nrhs == 8 ) && ( !mxIsEmpty( prhs[7] ) ) && ( mxIsStruct( prhs[7] ) ) )
		{
			nWSRin = 5*(nV+nC);
			setupOptions( &options,prhs[7],nWSRin );
			globalQP->globalQP->setOptions( options );
		}

		/* Create output vectors and assign pointers to them. */
		plhs[0] = mxCreateDoubleMatrix( nV, nRHS, mxREAL );
		x_out = mxGetPr(plhs[0]);
		if (nlhs >= 2)
		{
			plhs[1] = mxCreateDoubleMatrix( nV + nC, nRHS, mxREAL );
			y_out = mxGetPr(plhs[1]);

			if (nlhs >= 3) {
				plhs[2] = mxCreateDoubleMatrix(nV + nC, nRHS, mxREAL);
				double* workingSet = mxGetPr(plhs[2]);

				globalQP->globalQP->getWorkingSet(workingSet);
			}
		}
		else
			y_out = new real_t[nV+nC];

		/* Solve equality constrained QP */
		returnValue returnvalue = globalQP->globalQP->solveCurrentEQP( nRHS,g,lb,ub,lbA,ubA, x_out,y_out );

		if (nlhs < 2)
			delete[] y_out;

		if (returnvalue != SUCCESSFUL_RETURN)
		{
			char msg[200];
			msg[199] = 0;
			snprintf(msg, 199, "ERROR (qpOASES): Couldn't solve current EQP (code %d)!", returnvalue);
			myMexErrMsgTxt(msg);
			return;
		}

		return;
	}

	/* 5) Cleanup. */
	if ( ( strcmp( typeString,"c" ) == 0 ) || ( strcmp( typeString,"C" ) == 0 ) )
	{		
		/* consistency checks */
		if ( nlhs != 0 )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid number of output arguments!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		if ( nrhs != 2 )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Invalid number of input arguments!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		if ( ( mxIsDouble( prhs[1] ) == false ) || ( mxGetM( prhs[1] ) != 1 ) || ( mxGetN( prhs[1] ) != 1 ) )
		{
			myMexErrMsgTxt( "ERROR (qpOASES): Expecting a handle to QP object as second argument!\nType 'help qpOASES_sequence' for further information." );
			return;
		}

		/* Cleanup SQProblem instance. */
		handle =(unsigned int)mxGetScalar( prhs[1] );
		deleteQProblemInstance ( handle );
		
		return;
	}

}

/*
 *	end of file
 */

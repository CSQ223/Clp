// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.


#include <cstdio>

#include "CoinPragma.hpp"
#include "CoinIndexedVector.hpp"
#include "CoinHelperFunctions.hpp"

#include "ClpSimplex.hpp"
#include "ClpFactorization.hpp"
#include "ClpQuadraticObjective.hpp"
#include "ClpNonLinearCost.hpp"
// at end to get min/max!
#include "ClpGubMatrix.hpp"
#include "ClpMessage.hpp"
#define CLP_DEBUG
//#############################################################################
// Constructors / Destructor / Assignment
//#############################################################################

//-------------------------------------------------------------------
// Default Constructor 
//-------------------------------------------------------------------
ClpGubMatrix::ClpGubMatrix () 
  : ClpPackedMatrix(),
    sumDualInfeasibilities_(0.0),
    sumPrimalInfeasibilities_(0.0),
    sumOfRelaxedDualInfeasibilities_(0.0),
    sumOfRelaxedPrimalInfeasibilities_(0.0),
    start_(NULL),
    end_(NULL),
    lower_(NULL),
    upper_(NULL),
    status_(NULL),
    backward_(NULL),
    backToPivotRow_(NULL),
    changeCost_(NULL),
    keyVariable_(NULL),
    next_(NULL),
    toIndex_(NULL),
    fromIndex_(NULL),
    numberDualInfeasibilities_(0),
    numberPrimalInfeasibilities_(0),
    numberSets_(0),
    saveNumber_(0),
    possiblePivotKey_(0),
    gubSlackIn_(-1),
    firstGub_(0),
    lastGub_(0),
    gubType_(0)
{
  setType(11);
}

//-------------------------------------------------------------------
// Copy constructor 
//-------------------------------------------------------------------
ClpGubMatrix::ClpGubMatrix (const ClpGubMatrix & rhs) 
: ClpPackedMatrix(rhs)
{  
  numberSets_ = rhs.numberSets_;
  saveNumber_ = rhs.saveNumber_;
  possiblePivotKey_ = rhs.possiblePivotKey_;
  gubSlackIn_ = rhs.gubSlackIn_;
  start_ = ClpCopyOfArray(rhs.start_,numberSets_);
  end_ = ClpCopyOfArray(rhs.end_,numberSets_);
  lower_ = ClpCopyOfArray(rhs.lower_,numberSets_);
  upper_ = ClpCopyOfArray(rhs.upper_,numberSets_);
  status_ = ClpCopyOfArray(rhs.status_,numberSets_);
  int numberColumns = getNumCols();
  backward_ = ClpCopyOfArray(rhs.backward_,numberColumns);
  backToPivotRow_ = ClpCopyOfArray(rhs.backToPivotRow_,numberColumns);
  changeCost_ = ClpCopyOfArray(rhs.changeCost_,getNumRows());
  keyVariable_ = ClpCopyOfArray(rhs.keyVariable_,numberSets_);
  next_ = ClpCopyOfArray(rhs.next_,numberColumns+numberSets_);
  toIndex_ = ClpCopyOfArray(rhs.toIndex_,numberSets_);
  fromIndex_ = ClpCopyOfArray(rhs.fromIndex_,getNumRows()+1);
  sumDualInfeasibilities_ = rhs. sumDualInfeasibilities_;
  sumPrimalInfeasibilities_ = rhs.sumPrimalInfeasibilities_;
  sumOfRelaxedDualInfeasibilities_ = rhs.sumOfRelaxedDualInfeasibilities_;
  sumOfRelaxedPrimalInfeasibilities_ = rhs.sumOfRelaxedPrimalInfeasibilities_;
  numberDualInfeasibilities_ = rhs.numberDualInfeasibilities_;
  numberPrimalInfeasibilities_ = rhs.numberPrimalInfeasibilities_;
  firstGub_ = rhs.firstGub_;
  lastGub_ = rhs.lastGub_;
  gubType_ = rhs.gubType_;
}

//-------------------------------------------------------------------
// assign matrix (for space reasons)
//-------------------------------------------------------------------
ClpGubMatrix::ClpGubMatrix (CoinPackedMatrix * rhs) 
  : ClpPackedMatrix(rhs),
    sumDualInfeasibilities_(0.0),
    sumPrimalInfeasibilities_(0.0),
    sumOfRelaxedDualInfeasibilities_(0.0),
    sumOfRelaxedPrimalInfeasibilities_(0.0),
    start_(NULL),
    end_(NULL),
    lower_(NULL),
    upper_(NULL),
    status_(NULL),
    backward_(NULL),
    backToPivotRow_(NULL),
    changeCost_(NULL),
    keyVariable_(NULL),
    next_(NULL),
    toIndex_(NULL),
    fromIndex_(NULL),
    numberDualInfeasibilities_(0),
    numberPrimalInfeasibilities_(0),
    numberSets_(0),
    saveNumber_(0),
    possiblePivotKey_(0),
    gubSlackIn_(-1),
    firstGub_(0),
    lastGub_(0),
    gubType_(0)
{  
  setType(11);
}

/* This takes over ownership (for space reasons) and is the
   real constructor*/
ClpGubMatrix::ClpGubMatrix(ClpPackedMatrix * matrix, int numberSets,
			   const int * start, const int * end,
			   const double * lower, const double * upper,
			   const unsigned char * status)
  : ClpPackedMatrix(matrix->matrix()),
    sumDualInfeasibilities_(0.0),
    sumPrimalInfeasibilities_(0.0),
    sumOfRelaxedDualInfeasibilities_(0.0),
    sumOfRelaxedPrimalInfeasibilities_(0.0),
    numberDualInfeasibilities_(0),
    numberPrimalInfeasibilities_(0),
    saveNumber_(0),
    possiblePivotKey_(0),
    gubSlackIn_(-1)
{
  numberSets_ = numberSets;
  start_ = ClpCopyOfArray(start,numberSets_);
  end_ = ClpCopyOfArray(end,numberSets_);
  lower_ = ClpCopyOfArray(lower,numberSets_);
  upper_ = ClpCopyOfArray(upper,numberSets_);
  // Check valid and ordered
  int last=-1;
  int numberColumns = matrix_->getNumCols();
  int numberRows = matrix_->getNumRows();
  backward_ = new int[numberColumns];
  backToPivotRow_ = new int[numberColumns];
  changeCost_ = new double [numberRows];
  keyVariable_ = new int[numberSets_];
  // signal to need new ordering
  next_ = NULL;
  for (int iColumn=0;iColumn<numberColumns;iColumn++) 
    backward_[iColumn]=-1;

  int iSet;
  for (iSet=0;iSet<numberSets_;iSet++) {
    if (start_[iSet]<0||start_[iSet]>=numberColumns)
      throw CoinError("Index out of range","constructor","ClpGubMatrix");
    if (end_[iSet]<0||end_[iSet]>numberColumns)
      throw CoinError("Index out of range","constructor","ClpGubMatrix");
    if (end_[iSet]<=start_[iSet])
      throw CoinError("Empty or negative set","constructor","ClpGubMatrix");
    if (start_[iSet]<last)
      throw CoinError("overlapping or non-monotonic sets","constructor","ClpGubMatrix");
    last=end_[iSet];
    int j;
    for (j=start_[iSet];j<end_[iSet];j++)
      backward_[j]=iSet;
  }
  // Find type of gub
  firstGub_=numberColumns+1;
  lastGub_=-1;
  int i;
  for (i=0;i<numberColumns;i++) {
    if (backward_[i]>=0) {
      firstGub_ = min(firstGub_,i);
      lastGub_ = max(lastGub_,i);
    }
  }
  gubType_=0;
  // adjust lastGub_
  if (lastGub_>0)
    lastGub_++;
  for (i=firstGub_;i<lastGub_;i++) {
    if (backward_[i]<0) {
      gubType_=1;
      printf("interior non gub %d\n",i);
      break;
    }
  }
  if (status) {
    status_ = ClpCopyOfArray(status,numberSets_);
  } else {
    status_= new unsigned char [numberSets_];
    memset(status_,0,numberSets_);
    int i;
    for (i=0;i<numberSets_;i++) {
      // make slack key
      setStatus(i,ClpSimplex::basic);
    }
  }
}

ClpGubMatrix::ClpGubMatrix (const CoinPackedMatrix & rhs) 
  : ClpPackedMatrix(rhs),
    sumDualInfeasibilities_(0.0),
    sumPrimalInfeasibilities_(0.0),
    sumOfRelaxedDualInfeasibilities_(0.0),
    sumOfRelaxedPrimalInfeasibilities_(0.0),
    start_(NULL),
    end_(NULL),
    lower_(NULL),
    upper_(NULL),
    status_(NULL),
    backward_(NULL),
    backToPivotRow_(NULL),
    changeCost_(NULL),
    keyVariable_(NULL),
    next_(NULL),
    toIndex_(NULL),
    fromIndex_(NULL),
    numberDualInfeasibilities_(0),
    numberPrimalInfeasibilities_(0),
    numberSets_(0),
    saveNumber_(0),
    possiblePivotKey_(0),
    gubSlackIn_(-1),
    firstGub_(0),
    lastGub_(0),
    gubType_(0)
{  
  setType(11);
  
}

//-------------------------------------------------------------------
// Destructor 
//-------------------------------------------------------------------
ClpGubMatrix::~ClpGubMatrix ()
{
  delete [] start_;
  delete [] end_;
  delete [] lower_;
  delete [] upper_;
  delete [] status_;
  delete [] backward_;
  delete [] backToPivotRow_;
  delete [] changeCost_;
  delete [] keyVariable_;
  delete [] next_;
  delete [] toIndex_;
  delete [] fromIndex_;
}

//----------------------------------------------------------------
// Assignment operator 
//-------------------------------------------------------------------
ClpGubMatrix &
ClpGubMatrix::operator=(const ClpGubMatrix& rhs)
{
  if (this != &rhs) {
    ClpPackedMatrix::operator=(rhs);
    delete [] start_;
    delete [] end_;
    delete [] lower_;
    delete [] upper_;
    delete [] status_;
    delete [] backward_;
    delete [] backToPivotRow_;
    delete [] changeCost_;
    delete [] keyVariable_;
    delete [] next_;
    delete [] toIndex_;
    delete [] fromIndex_;
    numberSets_ = rhs.numberSets_;
    saveNumber_ = rhs.saveNumber_;
    possiblePivotKey_ = rhs.possiblePivotKey_;
    gubSlackIn_ = rhs.gubSlackIn_;
    start_ = ClpCopyOfArray(rhs.start_,numberSets_);
    end_ = ClpCopyOfArray(rhs.end_,numberSets_);
    lower_ = ClpCopyOfArray(rhs.lower_,numberSets_);
    upper_ = ClpCopyOfArray(rhs.upper_,numberSets_);
    status_ = ClpCopyOfArray(rhs.status_,numberSets_);
    int numberColumns = getNumCols();
    backward_ = ClpCopyOfArray(rhs.backward_,numberColumns);
    backToPivotRow_ = ClpCopyOfArray(rhs.backToPivotRow_,numberColumns);
    changeCost_ = ClpCopyOfArray(rhs.changeCost_,getNumRows());
    keyVariable_ = ClpCopyOfArray(rhs.keyVariable_,numberSets_);
    next_ = ClpCopyOfArray(rhs.next_,numberColumns+numberSets_);
    toIndex_ = ClpCopyOfArray(rhs.toIndex_,numberSets_);
    fromIndex_ = ClpCopyOfArray(rhs.fromIndex_,getNumRows()+1);
    sumDualInfeasibilities_ = rhs. sumDualInfeasibilities_;
    sumPrimalInfeasibilities_ = rhs.sumPrimalInfeasibilities_;
    sumOfRelaxedDualInfeasibilities_ = rhs.sumOfRelaxedDualInfeasibilities_;
    sumOfRelaxedPrimalInfeasibilities_ = rhs.sumOfRelaxedPrimalInfeasibilities_;
    numberDualInfeasibilities_ = rhs.numberDualInfeasibilities_;
    numberPrimalInfeasibilities_ = rhs.numberPrimalInfeasibilities_;
    firstGub_ = rhs.firstGub_;
    lastGub_ = rhs.lastGub_;
    gubType_ = rhs.gubType_;
  }
  return *this;
}
//-------------------------------------------------------------------
// Clone
//-------------------------------------------------------------------
ClpMatrixBase * ClpGubMatrix::clone() const
{
  return new ClpGubMatrix(*this);
}
/* Subset clone (without gaps).  Duplicates are allowed
   and order is as given */
ClpMatrixBase * 
ClpGubMatrix::subsetClone (int numberRows, const int * whichRows,
			   int numberColumns, 
			   const int * whichColumns) const 
{
  return new ClpGubMatrix(*this, numberRows, whichRows,
				   numberColumns, whichColumns);
}
/* Subset constructor (without gaps).  Duplicates are allowed
   and order is as given */
ClpGubMatrix::ClpGubMatrix (
			    const ClpGubMatrix & rhs,
			    int numberRows, const int * whichRows,
			    int numberColumns, const int * whichColumns)
  : ClpPackedMatrix(rhs, numberRows, whichRows, numberColumns, whichColumns)
{
  // Assuming no gub rows deleted
  // We also assume all sets in same order
  // Get array with backward pointers
  int numberColumnsOld = rhs.matrix_->getNumCols();
  int * array = new int [ numberColumnsOld];
  int i;
  for (i=0;i<numberColumnsOld;i++)
    array[i]=-1;
  for (int iSet=0;iSet<numberSets_;iSet++) {
    for (int j=start_[iSet];j<end_[iSet];j++)
      array[j]=iSet;
  }
  numberSets_=-1;
  int lastSet=-1;
  bool inSet=false;
  for (i=0;i<numberColumns;i++) {
    int iColumn = whichColumns[i];
    int iSet=array[iColumn];
    if (iSet<0) {
      inSet=false;
    } else {
      if (!inSet) {
	// start of new set but check okay
	if (iSet<=lastSet)
	  throw CoinError("overlapping or non-monotonic sets","subset constructor","ClpGubMatrix");
	lastSet = iSet;
	numberSets_++;
	start_[numberSets_]=i;
	end_[numberSets_]=i+1;
	lower_[numberSets_]=lower_[iSet];
	upper_[numberSets_]=upper_[iSet];
	inSet=true;
      } else {
	if (iSet<lastSet) {
	  throw CoinError("overlapping or non-monotonic sets","subset constructor","ClpGubMatrix");
	} else if (iSet==lastSet) {
	  end_[numberSets_]=i+1;
	} else {
	  // new set
	  lastSet = iSet;
	  numberSets_++;
	  start_[numberSets_]=i;
	  end_[numberSets_]=i+1;
	  lower_[numberSets_]=lower_[iSet];
	  upper_[numberSets_]=upper_[iSet];
	}
      }
    }
  }
  numberSets_++; // adjust
  // Find type of gub
  firstGub_=numberColumns+1;
  lastGub_=-1;
  for (i=0;i<numberColumns;i++) {
    if (backward_[i]>=0) {
      firstGub_ = min(firstGub_,i);
      lastGub_ = max(lastGub_,i);
    }
  }
  if (lastGub_>0)
    lastGub_++;
  gubType_=0;
  for (i=firstGub_;i<lastGub_;i++) {
    if (backward_[i]<0) {
      gubType_=1;
      break;
    }
  }

  // Make sure key is feasible if only key in set
}
ClpGubMatrix::ClpGubMatrix (
		       const CoinPackedMatrix & rhs,
		       int numberRows, const int * whichRows,
		       int numberColumns, const int * whichColumns)
  : ClpPackedMatrix(rhs, numberRows, whichRows, numberColumns, whichColumns),
    sumDualInfeasibilities_(0.0),
    sumPrimalInfeasibilities_(0.0),
    sumOfRelaxedDualInfeasibilities_(0.0),
    sumOfRelaxedPrimalInfeasibilities_(0.0),
    start_(NULL),
    end_(NULL),
    lower_(NULL),
    upper_(NULL),
    backward_(NULL),
    backToPivotRow_(NULL),
    changeCost_(NULL),
    keyVariable_(NULL),
    next_(NULL),
    toIndex_(NULL),
    fromIndex_(NULL),
    numberDualInfeasibilities_(0),
    numberPrimalInfeasibilities_(0),
    numberSets_(0),
    saveNumber_(0),
    possiblePivotKey_(0),
    gubSlackIn_(-1),
    firstGub_(0),
    lastGub_(0),
    gubType_(0)
{
  setType(11);
}
/* Return <code>x * A + y</code> in <code>z</code>. 
	Squashes small elements and knows about ClpSimplex */
void 
ClpGubMatrix::transposeTimes(const ClpSimplex * model, double scalar,
			      const CoinIndexedVector * rowArray,
			      CoinIndexedVector * y,
			      CoinIndexedVector * columnArray) const
{
  // Do packed part
  ClpPackedMatrix::transposeTimes(model, scalar, rowArray, y, columnArray);
  if (numberSets_) {
    abort();
  }
}
/* Return <code>x * A + y</code> in <code>z</code>. 
	Squashes small elements and knows about ClpSimplex */
void 
ClpGubMatrix::transposeTimesByRow(const ClpSimplex * model, double scalar,
			      const CoinIndexedVector * rowArray,
			      CoinIndexedVector * y,
			      CoinIndexedVector * columnArray) const
{
  // Do packed part
  ClpPackedMatrix::transposeTimesByRow(model, scalar, rowArray, y, columnArray);
  if (numberSets_) {
    abort();
  }
}
/* Return <code>x *A in <code>z</code> but
   just for indices in y.
   Squashes small elements and knows about ClpSimplex */
void 
ClpGubMatrix::subsetTransposeTimes(const ClpSimplex * model,
			      const CoinIndexedVector * rowArray,
			      const CoinIndexedVector * y,
			      CoinIndexedVector * columnArray) const
{
  // Do packed part
  ClpPackedMatrix::subsetTransposeTimes(model, rowArray, y, columnArray);
  if (numberSets_) {
    abort();
  }
}
/* If element NULL returns number of elements in column part of basis,
   If not NULL fills in as well */
CoinBigIndex 
ClpGubMatrix::fillBasis(ClpSimplex * model,
			   const int * whichColumn, 
			   int numberBasic,
			   int & numberColumnBasic,
			   int * indexRowU, int * indexColumnU,
			   double * elementU) 
{
  int i;
  int numberColumns = getNumCols();
  const int * columnLength = matrix_->getVectorLengths(); 
  int numberRows = getNumRows();
  int saveNumberBasic=numberBasic;
  assert (next_ ||!elementU) ;
  CoinBigIndex numberElements=0;
  int lastSet=-1;
  int key=-1;
  int keyLength=-1;
  double * work = new double[numberRows];
  CoinZeroN(work,numberRows);
  char * mark = new char[numberRows];
  CoinZeroN(mark,numberRows);
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * row = matrix_->getIndices();
  const double * elementByColumn = matrix_->getElements();
  const double * rowScale = model->rowScale();
  if (elementU!=NULL) {
    // fill
    if (!rowScale) {
      // no scaling
      for (i=0;i<numberColumnBasic;i++) {
	int iColumn = whichColumn[i];
	int iSet = backward_[iColumn];
	int length = columnLength[iColumn];
	CoinBigIndex j;
	if (iSet<0||keyVariable_[iSet]>=numberColumns) {
	  for (j=columnStart[iColumn];j<columnStart[iColumn]+columnLength[iColumn];j++) {
	    double value = elementByColumn[j];
	    if (fabs(value)>1.0e-20) {
	      int iRow = row[j];
	      indexRowU[numberElements]=iRow;
	      indexColumnU[numberElements]=numberBasic;
	      elementU[numberElements++]=value;
	    }
	  }
	  numberBasic++;
	} else {
	  // in gub set
	  if (iColumn!=keyVariable_[iSet]) {
	    // not key 
	    if (lastSet!=iSet) {
	      // erase work
	      if (key>=0) {
		for (j=columnStart[key];j<columnStart[key]+keyLength;j++) {
		  int iRow=row[j];
		  work[iRow]=0.0;
		  mark[iRow]=0;
		}
	      }
	      key=keyVariable_[iSet];
	      lastSet=iSet;
	      keyLength = columnLength[key];
	      for (j=columnStart[key];j<columnStart[key]+keyLength;j++) {
		int iRow=row[j];
		work[iRow]=elementByColumn[j];
		mark[iRow]=1;
	      }
	    }
	    for (j=columnStart[iColumn];j<columnStart[iColumn]+length;j++) {
	      int iRow = row[j];
	      double value=elementByColumn[j];
	      if (mark[iRow]) {
		mark[iRow]=0;
		double keyValue = work[iRow];
		value -= keyValue;
	      }
	      if (fabs(value)>1.0e-20) {
		indexRowU[numberElements]=iRow;
		indexColumnU[numberElements]=numberBasic;
		elementU[numberElements++]=value;
	      }
	    }
	    for (j=columnStart[key];j<columnStart[key]+keyLength;j++) {
	      int iRow = row[j];
	      if (mark[iRow]) {
		double value = -work[iRow];
		if (fabs(value)>1.0e-20) {
		  indexRowU[numberElements]=iRow;
		  indexColumnU[numberElements]=numberBasic;
		  elementU[numberElements++]=value;
		}
	      } else {
		// just put back mark
		mark[iRow]=1;
	      }
	    }
	    numberBasic++;
	  }
	}
      }
    } else {
      // scaling
      const double * columnScale = model->columnScale();
      for (i=0;i<numberColumnBasic;i++) {
	int iColumn = whichColumn[i];
	int iSet = backward_[iColumn];
	int length = columnLength[iColumn];
	CoinBigIndex j;
	if (iSet<0||keyVariable_[iSet]>=numberColumns) {
	  double scale = columnScale[iColumn];
	  for (j=columnStart[iColumn];j<columnStart[iColumn]+columnLength[iColumn];j++) {
	    int iRow = row[j];
	    double value = elementByColumn[j]*scale*rowScale[iRow];
	    if (fabs(value)>1.0e-20) {
	      indexRowU[numberElements]=iRow;
	      indexColumnU[numberElements]=numberBasic;
	      elementU[numberElements++]=value;
	    }
	  }
	  numberBasic++;
	} else {
	  // in gub set
	  if (iColumn!=keyVariable_[iSet]) {
	    double scale = columnScale[iColumn];
	    // not key 
	    if (lastSet<iSet) {
	      // erase work
	      if (key>=0) {
		for (j=columnStart[key];j<columnStart[key]+keyLength;j++) {
		  int iRow=row[j];
		  work[iRow]=0.0;
		  mark[iRow]=0;
		}
	      }
	      key=keyVariable_[iSet];
	      lastSet=iSet;
	      keyLength = columnLength[key];
	      double scale = columnScale[key];
	      for (j=columnStart[key];j<columnStart[key]+keyLength;j++) {
		int iRow=row[j];
		work[iRow]=elementByColumn[j]*scale*rowScale[iRow];
		mark[iRow]=1;
	      }
	    }
	    for (j=columnStart[iColumn];j<columnStart[iColumn]+length;j++) {
	      int iRow = row[j];
	      double value=elementByColumn[j]*scale*rowScale[iRow];
	      if (mark[iRow]) {
		mark[iRow]=0;
		double keyValue = work[iRow];
		value -= keyValue;
	      }
	      if (fabs(value)>1.0e-20) {
		indexRowU[numberElements]=iRow;
		indexColumnU[numberElements]=numberBasic;
		elementU[numberElements++]=value;
	      }
	    }
	    for (j=columnStart[key];j<columnStart[key]+keyLength;j++) {
	      int iRow = row[j];
	      if (mark[iRow]) {
		double value = -work[iRow];
		if (fabs(value)>1.0e-20) {
		  indexRowU[numberElements]=iRow;
		  indexColumnU[numberElements]=numberBasic;
		  elementU[numberElements++]=value;
		}
	      } else {
		// just put back mark
		mark[iRow]=1;
	      }
	    }
	    numberBasic++;
	  }
	}
      }
    }
  } else {
    // just count 
    if (!rowScale) {
      for (i=0;i<numberColumnBasic;i++) {
	int iColumn = whichColumn[i];
	int iSet = backward_[iColumn];
	int length = columnLength[iColumn];
	if (iSet<0||keyVariable_[iSet]>=numberColumns) {
	  numberElements += length;
	  numberBasic++;
	} else {
	  // in gub set
	  if (iColumn!=keyVariable_[iSet]) {
	    numberBasic++;
	    CoinBigIndex j;
	    // not key 
	    if (lastSet<iSet) {
	      // erase work
	      if (key>=0) {
		for (j=columnStart[key];j<columnStart[key]+keyLength;j++)
		  work[row[j]]=0.0;
	      }
	      key=keyVariable_[iSet];
	      lastSet=iSet;
	      keyLength = columnLength[key];
	      for (j=columnStart[key];j<columnStart[key]+keyLength;j++)
		work[row[j]]=elementByColumn[j];
	    }
	    int extra=keyLength;
	    for (j=columnStart[iColumn];j<columnStart[iColumn]+length;j++) {
	      int iRow = row[j];
	      double keyValue = work[iRow];
	      double value=elementByColumn[j];
	      if (!keyValue) {
		if (fabs(value)>1.0e-20)
		  extra++;
	      } else {
		value -= keyValue;
		if (fabs(value)<=1.0e-20)
		  extra--;
	      }
	    }
	    numberElements+=extra;
	  }
	}
      }
    } else {
      // scaled
      const double * columnScale = model->columnScale();
      for (i=0;i<numberColumnBasic;i++) {
	int iColumn = whichColumn[i];
	int iSet = backward_[iColumn];
	int length = columnLength[iColumn];
	if (iSet<0||keyVariable_[iSet]>=numberColumns) {
	  numberElements += length;
	  numberBasic++;
	} else {
	  // in gub set
	  if (iColumn!=keyVariable_[iSet]) {
	    numberBasic++;
	    CoinBigIndex j;
	    double scale = columnScale[iColumn];
	    // not key 
	    if (lastSet<iSet) {
	      // erase work
	      if (key>=0) {
		for (j=columnStart[key];j<columnStart[key]+keyLength;j++)
		  work[row[j]]=0.0;
	      }
	      key=keyVariable_[iSet];
	      lastSet=iSet;
	      keyLength = columnLength[key];
	      double scale = columnScale[key];
	      for (j=columnStart[key];j<columnStart[key]+keyLength;j++) {
		int iRow = row[j];
		work[iRow]=elementByColumn[j]*scale*rowScale[iRow];
	      }
	    }
	    int extra=keyLength;
	    for (j=columnStart[iColumn];j<columnStart[iColumn]+length;j++) {
	      int iRow = row[j];
	      double keyValue = work[iRow];
	      double value=elementByColumn[j]*scale*rowScale[iRow];
	      if (!keyValue) {
		if (fabs(value)>1.0e-20)
		  extra++;
	      } else {
		value -= keyValue;
		if (fabs(value)<=1.0e-20)
		  extra--;
	      }
	    }
	    numberElements+=extra;
	  }
	}
      }
    }
  }
  delete [] work;
  delete [] mark;
  // update number of column basic
  numberColumnBasic = numberBasic-saveNumberBasic;
  return numberElements;
}
/* Unpacks a column into an CoinIndexedvector
 */
void 
ClpGubMatrix::unpack(const ClpSimplex * model,CoinIndexedVector * rowArray,
		   int iColumn) const 
{
  assert (iColumn<model->numberColumns());
  // Do packed part
  ClpPackedMatrix::unpack(model,rowArray,iColumn);
  int iSet = backward_[iColumn];
  if (iSet>=0) {
    int iBasic = keyVariable_[iSet];
    if (iBasic <model->numberColumns()) {
      add(model,rowArray,iBasic,-1.0);
    }
  }
}
/* Unpacks a column into a CoinIndexedVector
** in packed format
Note that model is NOT const.  Bounds and objective could
be modified if doing column generation (just for this variable) */
void 
ClpGubMatrix::unpackPacked(ClpSimplex * model,
			    CoinIndexedVector * rowArray,
			    int iColumn) const
{
  int numberColumns = model->numberColumns();
  if (iColumn<numberColumns) {
    // Do packed part
    ClpPackedMatrix::unpackPacked(model,rowArray,iColumn);
    int iSet = backward_[iColumn];
    if (iSet>=0) {
      // columns are in order
      int iBasic = keyVariable_[iSet];
      if (iBasic <numberColumns) {
	int number = rowArray->getNumElements();
	const double * rowScale = model->rowScale();
	const int * row = matrix_->getIndices();
	const CoinBigIndex * columnStart = matrix_->getVectorStarts();
	const int * columnLength = matrix_->getVectorLengths(); 
	const double * elementByColumn = matrix_->getElements();
	double * array = rowArray->denseVector();
	int * index = rowArray->getIndices();
	CoinBigIndex i;
	assert(number);
	int numberOld=number;
	int lastIndex=0;
	int next=index[lastIndex];
	if (!rowScale) {
	  for (i=columnStart[iBasic];
	       i<columnStart[iBasic]+columnLength[iBasic];i++) {
	    int iRow = row[i];
	    while (iRow>next) {
	      lastIndex++;
	      if (lastIndex==numberOld)
		next=matrix_->getNumRows();
	      else
		next=index[lastIndex];
	    }
	    if (iRow<next) {
	      array[number]=-elementByColumn[i];
	      index[number++]=iRow;
	    } else {
	      assert (iRow==next);
	      array[lastIndex] -= elementByColumn[i];
	      if (!array[lastIndex])
		array[lastIndex]=1.0e-100;
	    }
	  }
	} else {
	  // apply scaling
	  double scale = model->columnScale()[iBasic];
	  for (i=columnStart[iBasic];
	       i<columnStart[iBasic]+columnLength[iBasic];i++) {
	    int iRow = row[i];
	    while (iRow>next) {
	      lastIndex++;
	      if (lastIndex==numberOld)
		next=matrix_->getNumRows();
	      else
		next=index[lastIndex];
	    }
	    if (iRow<next) {
	      array[number]=-elementByColumn[i]*scale*rowScale[iRow];
	      index[number++]=iRow;
	    } else {
	      assert (iRow==next);
	      array[lastIndex] -=elementByColumn[i]*scale*rowScale[iRow];
	      if (!array[lastIndex])
		array[lastIndex]=1.0e-100;
	    }
	  }
	}
	rowArray->setNumElements(number);
      }
    }
  } else {
    // key slack entering
    int iBasic = keyVariable_[gubSlackIn_];
    assert (iBasic <numberColumns);
    int number = 0;
    const double * rowScale = model->rowScale();
    const int * row = matrix_->getIndices();
    const CoinBigIndex * columnStart = matrix_->getVectorStarts();
    const int * columnLength = matrix_->getVectorLengths(); 
    const double * elementByColumn = matrix_->getElements();
    double * array = rowArray->denseVector();
    int * index = rowArray->getIndices();
    CoinBigIndex i;
    if (!rowScale) {
      for (i=columnStart[iBasic];
	   i<columnStart[iBasic]+columnLength[iBasic];i++) {
	int iRow = row[i];
	array[number]=elementByColumn[i];
	index[number++]=iRow;
      }
    } else {
      // apply scaling
      double scale = model->columnScale()[iBasic];
      for (i=columnStart[iBasic];
	   i<columnStart[iBasic]+columnLength[iBasic];i++) {
	int iRow = row[i];
	array[number]=elementByColumn[i]*scale*rowScale[iRow];
	index[number++]=iRow;
      }
    }
    rowArray->setNumElements(number);
    rowArray->setPacked();
  }
}
/* Adds multiple of a column into an CoinIndexedvector
      You can use quickAdd to add to vector */
void 
ClpGubMatrix::add(const ClpSimplex * model,CoinIndexedVector * rowArray,
		   int iColumn, double multiplier) const 
{
  assert (iColumn<model->numberColumns());
  // Do packed part
  ClpPackedMatrix::add(model,rowArray,iColumn,multiplier);
  int iSet = backward_[iColumn];
  if (iSet>=0&&iColumn!=keyVariable_[iSet]) {
    ClpPackedMatrix::add(model,rowArray,keyVariable_[iSet],-multiplier);
  }
}
/* Adds multiple of a column into an array */
void 
ClpGubMatrix::add(const ClpSimplex * model,double * array,
		   int iColumn, double multiplier) const 
{
  assert (iColumn<model->numberColumns());
  // Do packed part
  ClpPackedMatrix::add(model,array,iColumn,multiplier);
  if (iColumn<model->numberColumns()) {
    int iSet = backward_[iColumn];
    if (iSet>=0&&iColumn!=keyVariable_[iSet]&&keyVariable_[iSet]<model->numberColumns()) {
      ClpPackedMatrix::add(model,array,keyVariable_[iSet],-multiplier);
    }
  }
}
// Partial pricing 
void 
ClpGubMatrix::partialPricing(ClpSimplex * model, int start, int end,
			      int & bestSequence, int & numberWanted)
{
  if (numberSets_) {
    assert(!(gubType_&7));
    // Do packed part before gub
    ClpPackedMatrix::partialPricing(model,start,firstGub_,bestSequence,numberWanted);
    if (numberWanted) {
      // do gub
      const double * element =matrix_->getElements();
      const int * row = matrix_->getIndices();
      const CoinBigIndex * startColumn = matrix_->getVectorStarts();
      const int * length = matrix_->getVectorLengths();
      const double * rowScale = model->rowScale();
      const double * columnScale = model->columnScale();
      int iSequence;
      CoinBigIndex j;
      double tolerance=model->currentDualTolerance();
      double * reducedCost = model->djRegion();
      const double * duals = model->dualRowSolution();
      const double * cost = model->costRegion();
      double bestDj;
      int numberColumns = model->numberColumns();
      int numberRows = model->numberRows();
      if (bestSequence>=0)
	bestDj = fabs(reducedCost[bestSequence]);
      else
	bestDj=tolerance;
      int sequenceOut = model->sequenceOut();
      int saveSequence = bestSequence;
      int startG = max (start,firstGub_);
      int endG = min(lastGub_,end);
      int iSet = -1;
      double djMod=0.0;
      double infeasibilityCost = model->infeasibilityCost();
      if (rowScale) {
	double bestDjMod=0.0;
	// scaled
	for (iSequence=startG;iSequence<endG;iSequence++) {
	  if (backward_[iSequence]!=iSet) {
	    // get pi on gub row
	    iSet = backward_[iSequence];
	    int iBasic = keyVariable_[iSet];
	    if (iBasic>=numberColumns) {
	      djMod = - weight(iSet)*infeasibilityCost;
	    } else {
	      // get dj without 
	      assert (model->getStatus(iBasic)==ClpSimplex::basic);
	      djMod=0.0;
	      // scaled
	      for (j=startColumn[iBasic];
		   j<startColumn[iBasic]+length[iBasic];j++) {
		int jRow=row[j];
		djMod -= duals[jRow]*element[j]*rowScale[jRow];
	      }
	      // allow for scaling
	      djMod +=  cost[iBasic]/columnScale[iBasic];
	      // See if gub slack possible - dj is djMod
	      if (getStatus(iSet)==ClpSimplex::atLowerBound) {
		double value = -djMod;
		if (value>tolerance) {
		  numberWanted--;
		  if (value>bestDj) {
		    // check flagged variable and correct dj
		    if (!flagged(iSet)) {
		      bestDj=value;
		      bestSequence = numberRows+numberColumns+iSet;
		      bestDjMod = djMod;
		    } else {
		      // just to make sure we don't exit before got something
		      numberWanted++;
		      abort();
		    }
		  }
		}
	      } else if (getStatus(iSet)==ClpSimplex::atUpperBound) {
		double value = djMod;
		if (value>tolerance) {
		  numberWanted--;
		  if (value>bestDj) {
		    // check flagged variable and correct dj
		    if (!flagged(iSet)) {
		      bestDj=value;
		      bestSequence = numberRows+numberColumns+iSet;
		      bestDjMod = djMod;
		    } else {
		      // just to make sure we don't exit before got something
		      numberWanted++;
		      abort();
		    }
		  }
		}
	      }
	    }
	  }
	  if (iSequence!=sequenceOut) {
	    double value;
	    ClpSimplex::Status status = model->getStatus(iSequence);
	    
	    switch(status) {
	      
	    case ClpSimplex::basic:
	    case ClpSimplex::isFixed:
	      break;
	    case ClpSimplex::isFree:
	    case ClpSimplex::superBasic:
	      value=djMod;
	      // scaled
	      for (j=startColumn[iSequence];
		   j<startColumn[iSequence]+length[iSequence];j++) {
		int jRow=row[j];
		value -= duals[jRow]*element[j]*rowScale[jRow];
	      }
	      value = fabs(cost[iSequence] +value*columnScale[iSequence]);
	      if (value>FREE_ACCEPT*tolerance) {
		numberWanted--;
		// we are going to bias towards free (but only if reasonable)
		value *= FREE_BIAS;
		if (value>bestDj) {
		  // check flagged variable and correct dj
		  if (!model->flagged(iSequence)) {
		    bestDj=value;
		    bestSequence = iSequence;
		    bestDjMod = djMod;
		  } else {
		    // just to make sure we don't exit before got something
		    numberWanted++;
		  }
		}
	      }
	      break;
	    case ClpSimplex::atUpperBound:
	      value=djMod;
	      // scaled
	      for (j=startColumn[iSequence];
		   j<startColumn[iSequence]+length[iSequence];j++) {
		int jRow=row[j];
		value -= duals[jRow]*element[j]*rowScale[jRow];
	      }
	      value = cost[iSequence] +value*columnScale[iSequence];
	      if (value>tolerance) {
		numberWanted--;
		if (value>bestDj) {
		  // check flagged variable and correct dj
		  if (!model->flagged(iSequence)) {
		    bestDj=value;
		    bestSequence = iSequence;
		    bestDjMod = djMod;
		  } else {
		    // just to make sure we don't exit before got something
		    numberWanted++;
		  }
		}
	      }
	      break;
	    case ClpSimplex::atLowerBound:
	      value=djMod;
	      // scaled
	      for (j=startColumn[iSequence];
		   j<startColumn[iSequence]+length[iSequence];j++) {
		int jRow=row[j];
		value -= duals[jRow]*element[j]*rowScale[jRow];
	      }
	      value = -(cost[iSequence] +value*columnScale[iSequence]);
	      if (value>tolerance) {
		numberWanted--;
		if (value>bestDj) {
		  // check flagged variable and correct dj
		  if (!model->flagged(iSequence)) {
		    bestDj=value;
		    bestSequence = iSequence;
		    bestDjMod = djMod;
		  } else {
		    // just to make sure we don't exit before got something
		    numberWanted++;
		  }
		}
	      }
	      break;
	    }
	  }
	  if (!numberWanted)
	    break;
	}
	if (bestSequence!=saveSequence) {
	  if (bestSequence<numberRows+numberColumns) {
	    // recompute dj
	    double value=bestDjMod;
	    // scaled
	    for (j=startColumn[bestSequence];
		 j<startColumn[bestSequence]+length[bestSequence];j++) {
	      int jRow=row[j];
	      value -= duals[jRow]*element[j]*rowScale[jRow];
	    }
	    reducedCost[bestSequence] = cost[bestSequence] +value*columnScale[bestSequence];
	    gubSlackIn_=-1;
	  } else {
	    // slack - make last column
	    gubSlackIn_= bestSequence - numberRows - numberColumns;
	    bestSequence = numberColumns + 2*numberRows;
	    reducedCost[bestSequence] = bestDjMod;
	    model->setStatus(bestSequence,getStatus(gubSlackIn_));
	    if (getStatus(gubSlackIn_)==ClpSimplex::atUpperBound)
	      model->solutionRegion()[bestSequence] = upper_[gubSlackIn_];
	    else
	      model->solutionRegion()[bestSequence] = lower_[gubSlackIn_];
	    model->lowerRegion()[bestSequence] = lower_[gubSlackIn_];
	    model->upperRegion()[bestSequence] = upper_[gubSlackIn_];
	    model->costRegion()[bestSequence] = 0.0;
	  }
	}
      } else {
	double bestDjMod=0.0;
	for (iSequence=startG;iSequence<endG;iSequence++) {
	  if (backward_[iSequence]!=iSet) {
	    // get pi on gub row
	    iSet = backward_[iSequence];
	    int iBasic = keyVariable_[iSet];
	    if (iBasic>=numberColumns) {
	      djMod = - weight(iSet)*infeasibilityCost;
	    } else {
	      // get dj without 
	      assert (model->getStatus(iBasic)==ClpSimplex::basic);
	      djMod=0.0;
	      
	      for (j=startColumn[iBasic];
		   j<startColumn[iBasic]+length[iBasic];j++) {
		int jRow=row[j];
		djMod -= duals[jRow]*element[j];
	      }
	      djMod += cost[iBasic];
	      // See if gub slack possible - dj is djMod
	      if (getStatus(iSet)==ClpSimplex::atLowerBound) {
		double value = -djMod;
		if (value>tolerance) {
		  numberWanted--;
		  if (value>bestDj) {
		    // check flagged variable and correct dj
		    if (!flagged(iSet)) {
		      bestDj=value;
		      bestSequence = numberRows+numberColumns+iSet;
		      bestDjMod = djMod;
		    } else {
		      // just to make sure we don't exit before got something
		      numberWanted++;
		      abort();
		    }
		  }
		}
	      } else if (getStatus(iSet)==ClpSimplex::atUpperBound) {
		double value = djMod;
		if (value>tolerance) {
		  numberWanted--;
		  if (value>bestDj) {
		    // check flagged variable and correct dj
		    if (!flagged(iSet)) {
		      bestDj=value;
		      bestSequence = numberRows+numberColumns+iSet;
		      bestDjMod = djMod;
		    } else {
		      // just to make sure we don't exit before got something
		      numberWanted++;
		      abort();
		    }
		  }
		}
	      }
	    }
	  }
	  if (iSequence!=sequenceOut) {
	    double value;
	    ClpSimplex::Status status = model->getStatus(iSequence);
	    
	    switch(status) {
	      
	    case ClpSimplex::basic:
	    case ClpSimplex::isFixed:
	      break;
	    case ClpSimplex::isFree:
	    case ClpSimplex::superBasic:
	      value=cost[iSequence]-djMod;
	      for (j=startColumn[iSequence];
		   j<startColumn[iSequence]+length[iSequence];j++) {
		int jRow=row[j];
		value -= duals[jRow]*element[j];
	      }
	      value = fabs(value);
	      if (value>FREE_ACCEPT*tolerance) {
		numberWanted--;
		// we are going to bias towards free (but only if reasonable)
		value *= FREE_BIAS;
		if (value>bestDj) {
		  // check flagged variable and correct dj
		  if (!model->flagged(iSequence)) {
		    bestDj=value;
		    bestSequence = iSequence;
		    bestDjMod = djMod;
		  } else {
		    // just to make sure we don't exit before got something
		    numberWanted++;
		  }
		}
	      }
	      break;
	    case ClpSimplex::atUpperBound:
	      value=cost[iSequence]-djMod;
	      // scaled
	      for (j=startColumn[iSequence];
		   j<startColumn[iSequence]+length[iSequence];j++) {
		int jRow=row[j];
		value -= duals[jRow]*element[j];
	      }
	      if (value>tolerance) {
		numberWanted--;
		if (value>bestDj) {
		  // check flagged variable and correct dj
		  if (!model->flagged(iSequence)) {
		    bestDj=value;
		    bestSequence = iSequence;
		    bestDjMod = djMod;
		  } else {
		    // just to make sure we don't exit before got something
		    numberWanted++;
		  }
		}
	      }
	      break;
	    case ClpSimplex::atLowerBound:
	      value=cost[iSequence]-djMod;
	      for (j=startColumn[iSequence];
		   j<startColumn[iSequence]+length[iSequence];j++) {
		int jRow=row[j];
		value -= duals[jRow]*element[j];
	      }
	      value = -value;
	      if (value>tolerance) {
		numberWanted--;
		if (value>bestDj) {
		  // check flagged variable and correct dj
		  if (!model->flagged(iSequence)) {
		    bestDj=value;
		    bestSequence = iSequence;
		    bestDjMod = djMod;
		  } else {
		    // just to make sure we don't exit before got something
		    numberWanted++;
		  }
		}
	      }
	      break;
	    }
	  }
	  if (!numberWanted)
	    break;
	}
	if (bestSequence!=saveSequence) {
	  if (bestSequence<numberRows+numberColumns) {
	    // recompute dj
	    double value=cost[bestSequence]-bestDjMod;
	    for (j=startColumn[bestSequence];
		 j<startColumn[bestSequence]+length[bestSequence];j++) {
	      int jRow=row[j];
	      value -= duals[jRow]*element[j];
	    }
	    reducedCost[bestSequence] = value;
	    gubSlackIn_=-1;
	  } else {
	    // slack - make last column
	    gubSlackIn_= bestSequence - numberRows - numberColumns;
	    bestSequence = numberColumns + 2*numberRows;
	    reducedCost[bestSequence] = bestDjMod;
	    model->setStatus(bestSequence,getStatus(gubSlackIn_));
	    if (getStatus(gubSlackIn_)==ClpSimplex::atUpperBound)
	      model->solutionRegion()[bestSequence] = upper_[gubSlackIn_];
	    else
	      model->solutionRegion()[bestSequence] = lower_[gubSlackIn_];
	    model->lowerRegion()[bestSequence] = lower_[gubSlackIn_];
	    model->upperRegion()[bestSequence] = upper_[gubSlackIn_];
	    model->costRegion()[bestSequence] = 0.0;
	  }
	}
      }
    }
    if (numberWanted) {
      // Do packed part after gub
      ClpPackedMatrix::partialPricing(model,max(lastGub_,start),end,bestSequence,numberWanted);
    }
  } else {
    // no gub
    ClpPackedMatrix::partialPricing(model,start,end,bestSequence,numberWanted);
  }
}
/* expands an updated column to allow for extra rows which the main
   solver does not know about and returns number added. 
*/
int 
ClpGubMatrix::extendUpdated(ClpSimplex * model,CoinIndexedVector * update, int mode)
{
  // I think we only need to bother about sets with two in basis or incoming set
  int number = update->getNumElements();
  double * array = update->denseVector();
  int * index = update->getIndices();
  int i;
  assert (!number||update->packedMode());
  int * pivotVariable = model->pivotVariable();
  int numberRows = model->numberRows();
  int numberColumns = model->numberColumns();
  int numberTotal = numberRows+numberColumns;
  int iSequence = model->sequenceIn();
  int returnCode=0;
  int iSetX;
  if (iSequence<numberColumns)
    iSetX = backward_[iSequence];
  else if (iSequence<numberRows+numberColumns)
    iSetX = -1;
  else
    iSetX = gubSlackIn_;
  double * lower = model->lowerRegion();
  double * upper = model->upperRegion();
  double * cost = model->costRegion();
  double * solution = model->solutionRegion();
  int number2=number;
  if (!mode) {
    double primalTolerance = model->primalTolerance();
    double infeasibilityCost = model->infeasibilityCost();
    // extend
    saveNumber_ = number;
    for (i=0;i<number;i++) {
      int iRow = index[i];
      int iPivot = pivotVariable[iRow];
      if (iPivot<numberColumns) {
	int iSet = backward_[iPivot];
	if (iSet>=0) {
	  // two (or more) in set
	  int iIndex =toIndex_[iSet];
	  double otherValue=array[i];
	  double value;
	  if (iIndex<0) {
	    toIndex_[iSet]=number2;
	    int iNew = number2-number;
	    fromIndex_[number2-number]=iSet;
	    iIndex=number2;
	    index[number2]=numberRows+iNew;
	    // do key stuff
	    int iKey = keyVariable_[iSet];
	    if (iKey<numberColumns) {
	      if (iSet!=iSetX)
		value = 0.0;
	      else if (iSetX!=gubSlackIn_)
		value = 1.0;
	      else
		value =-1.0;
	      pivotVariable[numberRows+iNew]=iKey;
	      // Do I need to recompute?
	      double sol;
	      assert (getStatus(iSet)!=ClpSimplex::basic);
	      if (getStatus(iSet)==ClpSimplex::atLowerBound)
		sol = lower_[iSet];
	      else
		sol = upper_[iSet];
	      for (int j=start_[iSet];j<end_[iSet];j++) {
		if (j!=iKey)
		  sol -= solution[j];
	      }
	      solution[iKey]=sol;
	      if (model->algorithm()>0)
		model->nonLinearCost()->setOne(iKey,sol);
	      //assert (fabs(sol-solution[iKey])<1.0e-3);
	    } else {
	      // gub slack is basic
	      otherValue = - otherValue; //allow for - sign on slack
	      if (iSet!=iSetX)
		value = 0.0;
	      else
		value = -1.0;
	      pivotVariable[numberRows+iNew]=iNew+numberTotal;
	      double sol=0.0;
	      for (int j=start_[iSet];j<end_[iSet];j++) 
		sol += solution[j];
	      solution[iNew+numberTotal]=sol;
	      // and do cost in nonLinearCost
	      if (model->algorithm()>0)
		model->nonLinearCost()->setOne(iNew+numberTotal,sol,lower_[iSet],upper_[iSet]);
	      if (sol>upper_[iSet]+primalTolerance) {
		setAbove(iSet);
		lower[iNew+numberTotal]=upper_[iSet];
		upper[iNew+numberTotal]=COIN_DBL_MAX;
	      } else if (sol<lower_[iSet]-primalTolerance) {
		setBelow(iSet);
		lower[iNew+numberTotal]=-COIN_DBL_MAX;
		upper[iNew+numberTotal]=lower_[iSet];
	      } else {
		setFeasible(iSet);
		lower[iNew+numberTotal]=lower_[iSet];
		upper[iNew+numberTotal]=upper_[iSet];
	      }
	      cost[iNew+numberTotal]=weight(iSet)*infeasibilityCost;
	    }
	    number2++;
	  } else {
	    value = array[iIndex];
	  }
	  value -= otherValue;
	  array[iIndex]=value;
	}
      }
    }
    if (iSetX>=0&&toIndex_[iSetX]<0) {
      // Do incoming
      update->setPacked(); // just in case no elements
      toIndex_[iSetX]=number2;
      int iNew = number2-number;
      fromIndex_[number2-number]=iSetX;
      index[number2]=numberRows+iNew;
      // do key stuff
      int iKey = keyVariable_[iSetX];
      if (iKey<numberColumns) {
	if (gubSlackIn_<0)
	  array[number2]=1.0;
	else
	  array[number2]=-1.0;
	pivotVariable[numberRows+iNew]=iKey;
	// Do I need to recompute?
	double sol;
	assert (getStatus(iSetX)!=ClpSimplex::basic);
	if (getStatus(iSetX)==ClpSimplex::atLowerBound)
	  sol = lower_[iSetX];
	else
	  sol = upper_[iSetX];
	for (int j=start_[iSetX];j<end_[iSetX];j++) {
	  if (j!=iKey)
	    sol -= solution[j];
	}
	solution[iKey]=sol;
	//assert (fabs(sol-solution[iKey])<1.0e-3);
      } else {
	// gub slack is basic
	array[number2]=-1.0;
	pivotVariable[numberRows+iNew]=iNew+numberTotal;
	double sol=0.0;
	for (int j=start_[iSetX];j<end_[iSetX];j++) 
	  sol += solution[j];
	solution[iNew+numberTotal]=sol;
	// and do cost in nonLinearCost
	if (model->algorithm()>0)
	  model->nonLinearCost()->setOne(iNew+numberTotal,sol,lower_[iSetX],upper_[iSetX]);
	if (sol>upper_[iSetX]+primalTolerance) {
	  setAbove(iSetX);
	  lower[iNew+numberTotal]=upper_[iSetX];
	  upper[iNew+numberTotal]=COIN_DBL_MAX;
	} else if (sol<lower_[iSetX]-primalTolerance) {
	  setBelow(iSetX);
	  lower[iNew+numberTotal]=-COIN_DBL_MAX;
	  upper[iNew+numberTotal]=lower_[iSetX];
	} else {
	  setFeasible(iSetX);
	  lower[iNew+numberTotal]=lower_[iSetX];
	  upper[iNew+numberTotal]=upper_[iSetX];
	}
	cost[iNew+numberTotal]=weight(iSetX)*infeasibilityCost;
      }
      number2++;
    }
    // mark end
    fromIndex_[number2-number]=-1;
    returnCode = number2-number;
  } else {
    // take off?
    if (number>saveNumber_) {
      possiblePivotKey_=-1;
      if (model->pivotRow()>=numberRows) {
	const int * length = matrix_->getVectorLengths();
	int iExtra = model->pivotRow()-numberRows;
	assert (iExtra>=0);
	int sequenceOut = model->sequenceOut();
	int iSetOut = fromIndex_[iExtra];
	
	assert (model->sequenceOut()>=numberRows+numberColumns||
		model->sequenceOut()==keyVariable_[iSetOut]);
	// We need to find a possible pivot for incoming
	int shortest=numberRows+1;
	for (i=0;i<saveNumber_;i++) {
	  int iRow = index[i];
	  int iPivot = pivotVariable[iRow];
	  if (iPivot<numberColumns&&backward_[iPivot]==iSetOut) {
	    if (length[iPivot]<shortest) {
	      shortest = length[iPivot];
	      possiblePivotKey_=iRow;
	    }
	  }
	}
	assert (possiblePivotKey_>=0||iSetX==iSetOut);
	if (possiblePivotKey_>=0) {
	  returnCode=1; // need swap
	} else if (iSetX==iSetOut) {
	  returnCode = -1; // key swap - no need for update
	  // but check if any other basic
	  for (int j=start_[iSetX];j<end_[iSetX];j++) {
	    if (j!=sequenceOut&&j!=keyVariable_[iSetX]&&model->getStatus(j)==ClpSimplex::basic) {
	      returnCode=3; // need modifications
	      break;
	    }
	  }
	}
      } else if (iSequence>=numberRows+numberColumns) {
	returnCode=2; // need swap as gub slack in and must become key
      }
      // clear
      double theta = model->theta();
      for (i=saveNumber_;i<number;i++) {
	int iRow = index[i];
	int iColumn = pivotVariable[iRow];
	printf("Column %d (set %d) lower %g, upper %g - alpha %g - old value %g, new %g (theta %g)\n",
	       iColumn,fromIndex_[i-saveNumber_],lower[iColumn],upper[iColumn],array[i],
	       solution[iColumn],solution[iColumn]-theta*array[i],theta);
	array[i]=0.0;
	int iSet=fromIndex_[i-saveNumber_];
	toIndex_[iSet]=-1;
      }
    }
    for (i=0;i<numberSets_;i++)
      assert(toIndex_[i]==-1);
    number2= saveNumber_;
  }
  update->setNumElements(number2);
  return returnCode;
}
/*
     utility primal function for dealing with dynamic constraints
     mode=n see ClpGubMatrix.hpp for definition
     Remember to update here when settled down
*/
void 
ClpGubMatrix::primalExpanded(ClpSimplex * model,int mode)
{
  int numberColumns = model->numberColumns();
  switch (mode) {
    // If key variable then slot in gub rhs so will get correct contribution
  case 0:
    {
      int i;
      double * solution = model->solutionRegion();
      ClpSimplex::Status iStatus;
      for (i=0;i<numberSets_;i++) {
	int iColumn = keyVariable_[i];
	if (iColumn<numberColumns) {
	  // key is structural - where is slack
	  iStatus = getStatus(i);
	  assert (iStatus!=ClpSimplex::basic);
	  if (iStatus==ClpSimplex::atLowerBound)
	    solution[iColumn]=lower_[i];
	  else
	    solution[iColumn]=upper_[i];
	}
      }
    }
    break;
    // Compute values of key variables
  case 1:
    {
      int i;
      double * solution = model->solutionRegion();
      ClpSimplex::Status iStatus;
      //const int * columnLength = matrix_->getVectorLengths(); 
      //const CoinBigIndex * columnStart = matrix_->getVectorStarts();
      //const int * row = matrix_->getIndices();
      //const double * elementByColumn = matrix_->getElements();
      //int * pivotVariable = model->pivotVariable();
      sumPrimalInfeasibilities_=0.0;
      numberPrimalInfeasibilities_=0;
      double primalTolerance = model->primalTolerance();
      double relaxedTolerance=primalTolerance;
      // we can't really trust infeasibilities if there is primal error
      double error = min(1.0e-3,model->largestPrimalError());
      // allow tolerance at least slightly bigger than standard
      relaxedTolerance = relaxedTolerance +  error;
      // but we will be using difference
      relaxedTolerance -= primalTolerance;
      sumOfRelaxedPrimalInfeasibilities_ = 0.0;
      for (i=0;i<numberSets_;i++) { // Could just be over basics (esp if no bounds)
	int kColumn = keyVariable_[i];
	double value=0.0;
	if ((gubType_&8)!=0) {
	  int iColumn =next_[kColumn];
	  // sum all non-key variables
	  while(iColumn>=0) {
	    value+=solution[iColumn];
	    iColumn=next_[iColumn];
	  }
	} else {
	  // bounds exist - sum over all except key
	  for (int j=start_[i];j<end_[i];j++) {
	    if (j!=kColumn)
	      value += solution[j];
	  }
	}
	if (kColumn<numberColumns) {
	  // feasibility will be done later
	  assert (getStatus(i)!=ClpSimplex::basic);
	  if (getStatus(i)==ClpSimplex::atUpperBound)
	    solution[kColumn] = upper_[i]-value;
	  else
	    solution[kColumn] = lower_[i]-value;
	  printf("Value of key structural %d for set %d is %g\n",kColumn,i,solution[kColumn]);
	} else {
	  // slack is key
	  iStatus = getStatus(i);
	  assert (iStatus==ClpSimplex::basic);
	  double infeasibility=0.0;
	  if (value>upper_[i]+primalTolerance) {
	    infeasibility=value-upper_[i]-primalTolerance;
	    setAbove(i);
	  } else if (value<lower_[i]-primalTolerance) {
	    infeasibility=lower_[i]-value-primalTolerance ;
	    setBelow(i);
	  } else {
	    setFeasible(i);
	  }
	  printf("Value of key slack for set %d is %g\n",i,value);
	  if (infeasibility>0.0) {
	    sumPrimalInfeasibilities_ += infeasibility;
	    if (infeasibility>relaxedTolerance) 
	      sumOfRelaxedPrimalInfeasibilities_ += infeasibility;
	    numberPrimalInfeasibilities_ ++;
	  }
	}
      }
    }
    break;
    // Report on infeasibilities of key variables
  case 2:
    {
      model->setSumPrimalInfeasibilities(model->sumPrimalInfeasibilities()+
					 sumPrimalInfeasibilities_);
      model->setNumberPrimalInfeasibilities(model->numberPrimalInfeasibilities()+
					 numberPrimalInfeasibilities_);
      model->setSumOfRelaxedPrimalInfeasibilities(model->sumOfRelaxedPrimalInfeasibilities()+
					 sumOfRelaxedPrimalInfeasibilities_);
    }
    break;
  }
}
/*
     utility dual function for dealing with dynamic constraints
     mode=n see ClpGubMatrix.hpp for definition
     Remember to update here when settled down
*/
void 
ClpGubMatrix::dualExpanded(ClpSimplex * model,
			    CoinIndexedVector * array,
			    double * other,int mode)
{
  switch (mode) {
    // modify costs before transposeUpdate
  case 0:
    {
      int i;
      double * cost = model->costRegion();
      ClpSimplex::Status iStatus;
      // not dual values yet
      assert (!other);
      //double * work = array->denseVector();
      double infeasibilityCost = model->infeasibilityCost();
      int * pivotVariable = model->pivotVariable();
      int numberRows = model->numberRows();
      int numberColumns = model->numberColumns();
      // make sure fromIndex will not confuse pricing
      fromIndex_[0]=-1;
      for (i=0;i<numberRows;i++) {
	int iPivot = pivotVariable[i];
	if (iPivot<numberColumns) {
	  int iSet = backward_[iPivot];
	  if (iSet>=0) {
	    int kColumn = keyVariable_[iSet];
	    double costValue;
	    if (kColumn<numberColumns) {
	      // structural has cost
	      costValue = cost[kColumn];
	    } else {
	      // slack is key
	      iStatus = getStatus(iSet);
	      assert (iStatus==ClpSimplex::basic);
	      costValue=weight(iSet)*infeasibilityCost;
	    }
	    array->add(i,-costValue); // was work[i]-costValue
	  }
	}
      }
    }
    break;
    // create duals for key variables (without check on dual infeasible)
  case 1:
    {
      // If key slack then dual 0.0 (if feasible)
      // dj for key is zero so that defines dual on set
      int i;
      double * dj = model->djRegion();
      int numberColumns = model->numberColumns();
      double infeasibilityCost = model->infeasibilityCost();
      for (i=0;i<numberSets_;i++) {
	int kColumn = keyVariable_[i];
	if (kColumn<numberColumns) {
	  // dj without set
	  double value = dj[kColumn];
	  // Now subtract out from all 
	  dj[kColumn] =0.0;
	  int iColumn =next_[kColumn];
	  // modify all non-key variables
	  while(iColumn>=0) {
	    dj[iColumn]-=value;
	    iColumn=next_[iColumn];
	  }
	} else {
	  // slack key - may not be feasible
	  assert (getStatus(i)==ClpSimplex::basic);
	  double value=-weight(i)*infeasibilityCost;
	  if (value) {
	    int iColumn =next_[kColumn];
	    // modify all non-key variables basic
	    while(iColumn>=0) {
	      dj[iColumn]-=value;
	      iColumn=next_[iColumn];
	    }
	  }
	}
      }
    }
    break;
    // as 1 but check slacks and compute djs
  case 2:
    {
      // If key slack then dual 0.0
      // If not then slack could be dual infeasible
      // dj for key is zero so that defines dual on set
      int i;
      double * dj = model->djRegion();
      double * dual = model->dualRowSolution();
      double * cost = model->costRegion();
      ClpSimplex::Status iStatus;
      const int * columnLength = matrix_->getVectorLengths(); 
      const CoinBigIndex * columnStart = matrix_->getVectorStarts();
      const int * row = matrix_->getIndices();
      const double * elementByColumn = matrix_->getElements();
      int numberColumns = model->numberColumns();
      double infeasibilityCost = model->infeasibilityCost();
      sumDualInfeasibilities_=0.0;
      numberDualInfeasibilities_=0;
      double dualTolerance = model->dualTolerance();
      double relaxedTolerance=dualTolerance;
      // we can't really trust infeasibilities if there is dual error
      double error = min(1.0e-3,model->largestDualError());
      // allow tolerance at least slightly bigger than standard
      relaxedTolerance = relaxedTolerance +  error;
      // but we will be using difference
      relaxedTolerance -= dualTolerance;
      sumOfRelaxedDualInfeasibilities_ = 0.0;
      for (i=0;i<numberSets_;i++) {
	int kColumn = keyVariable_[i];
	if (kColumn<numberColumns) {
	  // dj without set
	  double value = cost[kColumn];
	  for (CoinBigIndex j=columnStart[kColumn];
	       j<columnStart[kColumn]+columnLength[kColumn];j++) {
	    int iRow = row[j];
	    value -= dual[iRow]*elementByColumn[j];
	  }
	  // Now subtract out from all 
	  for (int k=start_[i];k<end_[i];k++)
	    dj[k] -= value;
	  // check slack
	  iStatus = getStatus(i);
	  assert (iStatus!=ClpSimplex::basic);
	  double infeasibility=0.0;
	  // dj of slack is -(-1.0)value
	  if (iStatus==ClpSimplex::atLowerBound) {
	    if (value>-dualTolerance) 
	      infeasibility=-value-dualTolerance;
	  } else if (iStatus==ClpSimplex::atUpperBound) {
	    // at upper bound
	    if (value>dualTolerance) 
	      infeasibility=value-dualTolerance;
	  }
	  if (infeasibility>0.0) {
	    sumDualInfeasibilities_ += infeasibility;
	    if (infeasibility>relaxedTolerance) 
	      sumOfRelaxedDualInfeasibilities_ += infeasibility;
	    numberDualInfeasibilities_ ++;
	  }
	} else {
	  // slack key - may not be feasible
	  assert (getStatus(i)==ClpSimplex::basic);
	  double value=-weight(i)*infeasibilityCost;
	  if (value) {
	    // Now subtract out from all (if that's what we are doing)
	    for (int k=start_[i];k<end_[i];k++)
	      dj[k] -= value;
	  }
	}
      }
    }
    break;
    // Report on infeasibilities of key variables
  case 3:
    {
      model->setSumDualInfeasibilities(model->sumDualInfeasibilities()+
					 sumDualInfeasibilities_);
      model->setNumberDualInfeasibilities(model->numberDualInfeasibilities()+
					 numberDualInfeasibilities_);
      model->setSumOfRelaxedDualInfeasibilities(model->sumOfRelaxedDualInfeasibilities()+
					 sumOfRelaxedDualInfeasibilities_);
    }
    break;
    // modify costs before transposeUpdate for partial pricing
  case 4:
    {
      // First compute new costs etc for interesting gubs
      int iLook=0;
      int iSet=fromIndex_[0];
      double primalTolerance = model->primalTolerance();
      const double * cost = model->costRegion();
      double * solution = model->solutionRegion();
      double infeasibilityCost = model->infeasibilityCost();
      int numberColumns = model->numberColumns();
      int numberChanged=0;
      while (iSet>=0) {
	int key=keyVariable_[iSet];
	double value=0.0;
	// sum over all except key
	for (int j=start_[iSet];j<end_[iSet];j++) {
	  if (j!=key)
	    value += solution[j];
	}
	double costChange;
	if (key<numberColumns) {
	  assert (getStatus(iSet)!=ClpSimplex::basic);
	  double sol;
	  if (getStatus(iSet)==ClpSimplex::atUpperBound)
	    sol = upper_[iSet]-value;
	  else
	    sol = lower_[iSet]-value;
	  solution[key]=sol;
	  // fix up cost
	  double oldCost = cost[key];
	  model->nonLinearCost()->setOne(key,sol);
	  printf("yy Value of key structural %d for set %d is %g - cost %g\n",key,iSet,sol,
		 cost[key]);
	  costChange = cost[key]-oldCost;
	} else {
	  // slack is key
	  double oldCost = weight(iSet);
	  if (value>upper_[iSet]+primalTolerance) {
	    setAbove(iSet);
	  } else if (value<lower_[iSet]-primalTolerance) {
	    setBelow(iSet);
	  } else {
	    setFeasible(iSet);
	  }
	  costChange=(weight(iSet)-oldCost)*infeasibilityCost;
	  printf("yy Value of key slack for set %d is %g\n",iSet,value);
	}
	if (costChange) {
	  fromIndex_[numberChanged]=iSet;
	  toIndex_[iSet]=numberChanged;
	  changeCost_[numberChanged++]=costChange;
	}
	iSet = fromIndex_[++iLook];
      }
      if (numberChanged) {
	// first do those in list already
	int number = array->getNumElements();
	array->setPacked();
	int i;
	double * work = array->denseVector();
	int * which = array->getIndices();
	int * pivotVariable = model->pivotVariable();
	// temp
	int numberRows = model->numberRows();
	for (i=0;i<numberRows;i++) {
	  int iPivot = pivotVariable[i];
	  if (iPivot<numberColumns)
	    backToPivotRow_[iPivot]=i;
	}
	for (i=0;i<number;i++) {
	  int iRow = which[i];
	  int iPivot = pivotVariable[iRow];
	  if (iPivot<numberColumns) {
	    int iSet = backward_[iPivot];
	    if (iSet>=0&&toIndex_[iSet]>=0) {
	      double newValue = work[i]+changeCost_[toIndex_[iSet]];
	      if (!newValue)
		newValue =1.0e-100;
	      work[i]=newValue;
	      // mark as done
	      backward_[iPivot]=-1;
	    }
	  }
	}
	// now do rest and clean up
	for (i=0;i<numberChanged;i++) {
	  int iSet = fromIndex_[i];
	  int key=keyVariable_[iSet];
	  int iColumn = next_[key];
	  double change = changeCost_[i];
	  while (iColumn>=0) {
	    if (backward_[iColumn]>=0) {
	      int iRow = backToPivotRow_[iColumn];
	      assert (iRow>=0);
	      work[number] = change;
	      which[number++]=iRow;
	    } else {
	      // reset
	      backward_[iColumn]=iSet;
	    }
	    iColumn=next_[iColumn];
	  }
	  toIndex_[iSet]=-1;
	}
	fromIndex_[0]=-1;
	array->setNumElements(number);
      }
    }
    break;
  }
}
/*
     general utility function for dealing with dynamic constraints
     mode=n see ClpGubMatrix.hpp for definition
     Remember to update here when settled down
*/
int
ClpGubMatrix::generalExpanded(ClpSimplex * model,int mode,int &number)
{
  int returnCode=0;
  int numberColumns = model->numberColumns();
  switch (mode) {
    // Fill in pivotVariable but not for key variables
  case 0:
    {
      if (!next_ ) {
	// do ordering
	assert (!effectiveRhs_);
	// create and do gub crash
	useEffectiveRhs(model,false);
      }
      int i;
      int numberBasic=number;
      // Use different array so can build from true pivotVariable_
      //int * pivotVariable = model->pivotVariable();
      int * pivotVariable = model->rowArray(0)->getIndices();
      for (i=0;i<numberColumns;i++) {
	if (model->getColumnStatus(i) == ClpSimplex::basic) {
	  int iSet = backward_[i];
	  if (iSet<0||i!=keyVariable_[iSet])
	    pivotVariable[numberBasic++]=i;
	}
      }
      number = numberBasic;
    }
    break;
    // Make all key variables basic
  case 1:
    {
      int i;
      for (i=0;i<numberSets_;i++) {
	int iColumn = keyVariable_[i];
	if (iColumn<numberColumns)
	  model->setColumnStatus(iColumn,ClpSimplex::basic);
      }
    }
    break;
    // Do initial extra rows + maximum basic
  case 2:
    {
      returnCode= getNumRows()+1;
      number = model->numberRows()+numberSets_;
    }
    break;
    // Before normal replaceColumn
  case 3:
    {
      int sequenceIn = model->sequenceIn();
      int sequenceOut = model->sequenceOut();
      int numberColumns = model->numberColumns();
      int numberRows = model->numberRows();
      if (sequenceIn==sequenceOut) 
	return -1;
      int iSetIn=-1;
      int iSetOut=-1;
      if (sequenceOut<numberColumns) {
	iSetOut = backward_[sequenceOut];
      } else if (sequenceOut>=numberRows+numberColumns) {
	assert (model->pivotRow()>=numberRows);
	int iExtra = model->pivotRow()-numberRows;
	assert (iExtra>=0);
	if (iSetOut<0)
	  iSetOut = fromIndex_[iExtra];
	else
	  assert(iSetOut == fromIndex_[iExtra]);
      }
      if (sequenceIn<numberColumns) {
	iSetIn = backward_[sequenceIn];
      } else if (gubSlackIn_>=0) {
	iSetIn = gubSlackIn_;
      }
      
      if (number) {
	if (possiblePivotKey_>=0) {
	  returnCode=1; // need swap
	  // change pivot row and alpha
	} else if (iSetIn==iSetOut) {
	  returnCode = -1; // key swap - no need for update
	  // but check if any other basic
	  for (int j=start_[iSetIn];j<end_[iSetIn];j++) {
	    if (j!=sequenceOut&&j!=keyVariable_[iSetIn]&&model->getStatus(j)==ClpSimplex::basic) {
	      returnCode=3; // need modifications
	      break;
	    }
	  }
	}
      } else if (sequenceIn>=numberRows+numberColumns) {
	returnCode=2; // need swap as gub slack in and must become key
	// change pivot row and alpha
      }
      if (number)
	number=-1; // temporary
    }
    break;
    // After normal replaceColumn
  case 4:
    {
      // what to do on existing error
      assert(!number);
      //return 4; // temporary
      int sequenceIn = model->sequenceIn();
      int sequenceOut = model->sequenceOut();
      int numberColumns = model->numberColumns();
      int numberRows = model->numberRows();
      if (sequenceIn==sequenceOut) 
	return 0;
      int iSetIn=-1;
      int iSetOut=-1;
      if (sequenceOut<numberColumns) {
	iSetOut = backward_[sequenceOut];
      } else if (sequenceOut>=numberRows+numberColumns) {
	assert (model->pivotRow()>=numberRows);
	int iExtra = model->pivotRow()-numberRows;
	assert (iExtra>=0);
	if (iSetOut<0)
	  iSetOut = fromIndex_[iExtra];
	else
	  assert(iSetOut == fromIndex_[iExtra]);
      }
      if (sequenceIn<numberColumns) {
	iSetIn = backward_[sequenceIn];
      } else if (gubSlackIn_>=0) {
	iSetIn = gubSlackIn_;
      }
      
      if (possiblePivotKey_>=0) {
	returnCode=1; // need swap
	returnCode=4;
      } else if (iSetIn==iSetOut) {
	returnCode = 0; // key swap - no need for update
	// but check if any other basic
	for (int j=start_[iSetIn];j<end_[iSetIn];j++) {
	  if (j!=sequenceOut&&j!=keyVariable_[iSetIn]&&model->getStatus(j)==ClpSimplex::basic) {
	    returnCode=3; // need modifications
	    break;
	  }
	}
	//model->setPivotRow(-1);
      } else if (sequenceIn>=numberRows+numberColumns) {
	returnCode=2; // need swap as gub slack in and must become key
	returnCode=4;
      }
    }
    break;
  }
  return returnCode;
}
// Sets up an effective RHS and does gub crash if needed
void 
ClpGubMatrix::useEffectiveRhs(ClpSimplex * model, bool cheapest)
{
  // Do basis - cheapest or slack if feasible (unless cheapest set)
  int longestSet=0;
  int iSet;
  for (iSet=0;iSet<numberSets_;iSet++) 
    longestSet = max(longestSet,end_[iSet]-start_[iSet]);
    
  double * upper = new double[longestSet+1];
  double * cost = new double[longestSet+1];
  double * lower = new double[longestSet+1];
  double * solution = new double[longestSet+1];
  assert (!next_);
  int numberColumns = getNumCols();
  const int * columnLength = matrix_->getVectorLengths(); 
  const double * columnLower = model->lowerRegion();
  const double * columnUpper = model->upperRegion();
  double * columnSolution = model->solutionRegion();
  const double * objective = model->costRegion();
  int numberRows = getNumRows();
  next_ = new int[numberColumns+numberSets_];
  for (int iColumn=0;iColumn<numberColumns;iColumn++) 
    next_[iColumn]=INT_MAX;
  toIndex_ = new int[numberSets_];
  for (iSet=0;iSet<numberSets_;iSet++) 
    toIndex_[iSet]=-1;
  fromIndex_ = new int [getNumRows()+1];
  double tolerance = model->primalTolerance();
  bool noNormalBounds=true;
  gubType_ &= ~8;
  for (iSet=0;iSet<numberSets_;iSet++) {
    int j;
    int numberBasic=0;
    int iBasic=-1;
    int iStart = start_[iSet];
    int iEnd=end_[iSet];
    // find one with smallest length
    int smallest=numberRows+1;
    double value=0.0;
    for (j=iStart;j<iEnd;j++) {
      if (columnLower[j]&&columnLower[j]>-1.0e20)
	noNormalBounds=false;
      if (columnUpper[j]&&columnUpper[j]<1.0e20)
	noNormalBounds=false;
      if (model->getStatus(j)== ClpSimplex::basic) {
	if (columnLength[j]<smallest) {
	  smallest=columnLength[j];
	  iBasic=j;
	}
	numberBasic++;
      }
      value += columnSolution[j];
    }
    bool done=false;
    if (numberBasic>1||(numberBasic==1&&getStatus(iSet)==ClpSimplex::basic)) {
      if (getStatus(iSet)==ClpSimplex::basic) 
	iBasic = iSet+numberColumns;// slack key - use
      done=true;
    } else if (numberBasic==1) {
      // see if can be key
      double thisSolution = columnSolution[iBasic];
      if (thisSolution>columnUpper[iBasic]) {
	value -= thisSolution-columnUpper[iBasic];
	thisSolution = columnUpper[iBasic];
	columnSolution[iBasic]=thisSolution;
      }
      if (thisSolution<columnLower[iBasic]) {
	value -= thisSolution-columnLower[iBasic];
	thisSolution = columnLower[iBasic];
	columnSolution[iBasic]=thisSolution;
      }
      // try setting slack to a bound
      assert (upper_[iSet]<1.0e20||lower_[iSet]>-1.0e20);
      double cost1 = COIN_DBL_MAX;
      int whichBound=-1;
      if (upper_[iSet]<1.0e20) {
	// try slack at ub
	double newBasic = thisSolution +upper_[iSet]-value;
	if (newBasic>=columnLower[iBasic]-tolerance&&
	    newBasic<=columnUpper[iBasic]+tolerance) {
	  // can go
	  whichBound=1;
	  cost1 = newBasic*objective[iBasic];
	  // But if exact then may be good solution
	  if (fabs(upper_[iSet]-value)<tolerance)
	    cost1=-COIN_DBL_MAX;
	}
      }
      if (lower_[iSet]>-1.0e20) {
	// try slack at lb
	double newBasic = thisSolution +lower_[iSet]-value;
	if (newBasic>=columnLower[iBasic]-tolerance&&
	    newBasic<=columnUpper[iBasic]+tolerance) {
	  // can go but is it cheaper
	  double cost2 = newBasic*objective[iBasic];
	  // But if exact then may be good solution
	  if (fabs(lower_[iSet]-value)<tolerance)
	    cost2=-COIN_DBL_MAX;
	  if (cost2<cost1)
	    whichBound=0;
	}
      }
      if (whichBound!=-1) {
	// key
	done=true;
	if (whichBound) {
	  // slack to upper
	  columnSolution[iBasic]=thisSolution + upper_[iSet]-value;
	  setStatus(iSet,ClpSimplex::atUpperBound);
	} else {
	  // slack to lower
	  columnSolution[iBasic]=thisSolution + lower_[iSet]-value;
	  setStatus(iSet,ClpSimplex::atLowerBound);
	}
      }
    }
    if (!done) {
      if (!cheapest) {
	// see if slack can be key
	if (value>=lower_[iSet]-tolerance&&value<=upper_[iSet]+tolerance) {
	  done=true;
	  setStatus(iSet,ClpSimplex::basic);
	  iBasic=iSet+numberColumns;
	}
      }
      if (!done) {
	// set non basic if there was one
	if (iBasic>=0)
	  model->setStatus(iBasic,ClpSimplex::atLowerBound);
	// find cheapest
	int numberInSet = iEnd-iStart;
	CoinMemcpyN(columnLower+iStart,numberInSet,lower);
	CoinMemcpyN(columnUpper+iStart,numberInSet,upper);
	CoinMemcpyN(columnSolution+iStart,numberInSet,solution);
	// and slack
	iBasic=numberInSet;
	solution[iBasic]=-value;
	lower[iBasic]=-upper_[iSet];
	upper[iBasic]=-lower_[iSet];
	int kphase;
	if (value>=lower_[iSet]-tolerance&&value<=upper_[iSet]+tolerance) {
	  // feasible
	  kphase=1;
	  cost[iBasic]=0.0;
	  CoinMemcpyN(objective+iStart,numberInSet,cost);
	} else {
	  // infeasible
	  kphase=0;
	  // remember bounds are flipped so opposite to natural
	  if (value<lower_[iSet]-tolerance)
	    cost[iBasic]=1.0;
	  else
	    cost[iBasic]=-1.0;
	  CoinZeroN(cost,numberInSet);
	}
	double dualTolerance =model->dualTolerance();
	for (int iphase =kphase;iphase<2;iphase++) {
	  if (iphase) {
	    cost[iBasic]=0.0;
	    CoinMemcpyN(objective+iStart,numberInSet,cost);
	  }
	  // now do one row lp
	  bool improve=true;
	  while (improve) {
	    improve=false;
	    double dual = cost[iBasic];
	    int chosen =-1;
	    double best=dualTolerance;
	    int way=0;
	    for (int i=0;i<=numberInSet;i++) {
	      double dj = cost[i]-dual;
	      double improvement =0.0;
	      double distance=0.0;
	      if (iphase||i<numberInSet)
		assert (solution[i]>=lower[i]&&solution[i]<=upper[i]);
	      if (dj>dualTolerance)
		improvement = dj*(solution[i]-lower[i]);
	      else if (dj<-dualTolerance)
		improvement = dj*(solution[i]-upper[i]);
	      if (improvement>best) {
		best=improvement;
		chosen=i;
		if (dj<0.0) {
		  way = 1;
		  distance = upper[i]-solution[i];
		} else {
		  way = -1;
		  distance = solution[i]-lower[i];
		}
	      }
	    }
	    if (chosen>=0) {
	      // now see how far
	      if (way>0) {
		// incoming increasing so basic decreasing
		// if phase 0 then go to nearest bound
		double distance=upper[chosen]-solution[chosen];
		double basicDistance;
		if (!iphase) {
		  assert (iBasic==numberInSet);
		  assert (solution[iBasic]>upper[iBasic]);
		  basicDistance = solution[iBasic]-upper[iBasic];
		} else {
		  basicDistance = solution[iBasic]-lower[iBasic];
		}
		// need extra coding for unbounded
		assert (min(distance,basicDistance)<1.0e20);
		if (distance>basicDistance) {
		  // incoming becomes basic
		  solution[chosen] += basicDistance;
		  if (!iphase) 
		    solution[iBasic]=upper[iBasic];
		  else 
		    solution[iBasic]=lower[iBasic];
		  iBasic = chosen;
		} else {
		  // flip
		  solution[chosen]=upper[chosen];
		  solution[iBasic] -= distance;
		}
	      } else {
		// incoming decreasing so basic increasing
		// if phase 0 then go to nearest bound
		double distance=solution[chosen]-lower[chosen];
		double basicDistance;
		if (!iphase) {
		  assert (iBasic==numberInSet);
		  assert (solution[iBasic]<lower[iBasic]);
		  basicDistance = lower[iBasic]-solution[iBasic];
		} else {
		  basicDistance = upper[iBasic]-solution[iBasic];
		}
		// need extra coding for unbounded
		assert (min(distance,basicDistance)<1.0e20);
		if (distance>basicDistance) {
		  // incoming becomes basic
		  solution[chosen] -= basicDistance;
		  if (!iphase) 
		    solution[iBasic]=lower[iBasic];
		  else 
		    solution[iBasic]=upper[iBasic];
		  iBasic = chosen;
		} else {
		  // flip
		  solution[chosen]=lower[chosen];
		  solution[iBasic] += distance;
		}
	      }
	    }
	  }
	}
	// convert iBasic back and do bounds
	if (iBasic==numberInSet) {
	  // slack basic
	  setStatus(iSet,ClpSimplex::basic);
	  iBasic=iSet+numberColumns;
	} else {
	  iBasic += start_[iSet];
	  model->setStatus(iBasic,ClpSimplex::basic);
	  if (upper[numberInSet]==lower[numberInSet]) 
	    setStatus(iSet,ClpSimplex::isFixed);
	  else if (solution[numberInSet]==upper[numberInSet])
	    setStatus(iSet,ClpSimplex::atUpperBound);
	  else if (solution[numberInSet]==lower[numberInSet])
	    setStatus(iSet,ClpSimplex::atLowerBound);
	  else 
	    abort();
	}
	for (j=iStart;j<iEnd;j++) {
	  if (model->getStatus(j)!=ClpSimplex::basic) {
	    int inSet=j-iStart;
	    columnSolution[j]=solution[inSet];
	    if (upper[inSet]==lower[inSet]) 
	      model->setStatus(j,ClpSimplex::isFixed);
	    else if (solution[inSet]==upper[inSet])
	      model->setStatus(j,ClpSimplex::atUpperBound);
	    else if (solution[inSet]==lower[inSet])
	      model->setStatus(j,ClpSimplex::atLowerBound);
	  }
	}
      }
    } 
    keyVariable_[iSet]=iBasic;
    int lastMarker = -(iBasic+1);
    next_[iBasic]=lastMarker;
    int last = iBasic;
    for (j=iStart;j<iEnd;j++) {
      if (model->getStatus(j)==ClpSimplex::basic&&j!=iBasic) {
	next_[last]=j;
	next_[j]=lastMarker;
      }
    }
  }
  if (noNormalBounds)
    gubType_ |= 8;
  delete [] lower;
  delete [] solution;
  delete [] upper;
  delete [] cost;
  // make sure matrix is in good shape
  matrix_->orderMatrix();
  // create effective rhs
  delete [] effectiveRhs_;
  effectiveRhs_ = new double[numberRows];
  effectiveRhs(model,true);
  next_ = new int[numberColumns+numberSets_];
  char * mark = new char[numberColumns];
  memset(mark,0,numberColumns);
  for (int iColumn=0;iColumn<numberColumns;iColumn++) 
    next_[iColumn]=INT_MAX;
  int i;
  for (i=0;i<numberSets_;i++) {
    int j;
    for (j=start_[i];j<end_[i];j++) {
      if (model->getStatus(j)==ClpSimplex::basic) 
	mark[j]=1;
    }
    if (getStatus(i)!=ClpSimplex::basic) {
      // make sure fixed if it is
      if (upper_[i]==lower_[i])
	setStatus(i,ClpSimplex::isFixed);
      // slack not key - choose one with smallest length
      int smallest=numberRows+1;
      int key=-1;
      for (j=start_[i];j<end_[i];j++) {
	if (mark[j]&&columnLength[j]<smallest) {
	  key=j;
	  smallest=columnLength[j];
	}
      }
      if (key>=0) {
	keyVariable_[i]=key;
	int lastMarker = -(key+1);
	next_[key]=lastMarker;
	int last = key;
	int j;
	for (j=start_[i];j<end_[i];j++) {
	  if (mark[j]&&j!=key) {
	    next_[last]=j;
	    next_[j]=lastMarker;
	    last = j;
	  }
	}
      } else {
	// nothing basic - make slack key
	//((ClpGubMatrix *)this)->setStatus(i,ClpSimplex::basic);
	// fudge to avoid const problem
	status_[i]=1;
      }
    }
    if (getStatus(i)==ClpSimplex::basic) {
      // slack key
      keyVariable_[i]=numberColumns+i;
      int lastMarker = -(i+numberColumns+1);
      next_[numberColumns+i]=lastMarker;
      int last = numberColumns+i;
      int j;
      double sol=0.0;
      for (j=start_[i];j<end_[i];j++) {
	sol += columnSolution[j];
	if (mark[j]) {
	  next_[last]=j;
	  next_[j]=lastMarker;
	  last=j;
	}
      }
      if (sol>upper_[i]+tolerance) {
	setAbove(i);
      } else if (sol<lower_[i]-tolerance) {
	setBelow(i);
      } else {
	setFeasible(i);
      }
    }
  }
  delete [] mark;
}
/* Returns effective RHS if it is being used.  This is used for long problems
   or big gub or anywhere where going through full columns is
   expensive.  This may re-compute */
double * 
ClpGubMatrix::effectiveRhs(ClpSimplex * model,bool forceRefresh,bool check)
{
  if (effectiveRhs_) {
#ifdef CLP_DEBUG
    if (check) {
      // no need - but check anyway
      // zero out basic
      int numberRows = model->numberRows();
      int numberColumns = model->numberColumns();
      double * solution = new double [numberColumns];
      double * rhs = new double[numberRows];
      const double * solutionSlack = model->solutionRegion(0);
      CoinMemcpyN(model->solutionRegion(),numberColumns,solution);
      int iRow;
      for (iRow=0;iRow<numberRows;iRow++) {
	if (model->getRowStatus(iRow)!=ClpSimplex::basic)
	  rhs[iRow]=solutionSlack[iRow];
	else
	  rhs[iRow]=0.0;
      }
      for (int iColumn=0;iColumn<numberColumns;iColumn++) {
	if (model->getColumnStatus(iColumn)==ClpSimplex::basic)
	  solution[iColumn]=0.0;
      }
      times(-1.0,solution,rhs);
      delete [] solution;
      const double * columnSolution = model->solutionRegion();
      // and now subtract out non basic
      ClpSimplex::Status iStatus;
      for (int iSet=0;iSet<numberSets_;iSet++) {
	int iColumn = keyVariable_[iSet];
	if (iColumn<numberColumns) {
	  double b=0.0;
	  // key is structural - where is slack
	  iStatus = getStatus(iSet);
	  assert (iStatus!=ClpSimplex::basic);
	  if (iStatus==ClpSimplex::atLowerBound)
	    b=lower_[iSet];
	  else
	    b=upper_[iSet];
	  // subtract out others at bounds
	  int j;
	  for (j=start_[iSet];j<end_[iSet];j++) {
	    if (model->getColumnStatus(j)!=ClpSimplex::basic) 
	      b -= columnSolution[j];
	  }
	  // subtract out
	  ClpPackedMatrix::add(model,rhs,iColumn,-b);
	}
      }
      for (iRow=0;iRow<numberRows;iRow++) {
	if (fabs(rhs[iRow]-effectiveRhs_[iRow])>1.0e-3)
	  printf("** bad effective %d - true %g old %g\n",iRow,rhs[iRow],effectiveRhs_[iRow]);
      }
    }
#endif
    if (forceRefresh||(refreshFrequency_&&model->numberIterations()>=
		       lastRefresh_+refreshFrequency_)) {
      // zero out basic
      int numberRows = model->numberRows();
      int numberColumns = model->numberColumns();
      double * solution = new double [numberColumns];
      const double * solutionSlack = model->solutionRegion(0);
      CoinMemcpyN(model->solutionRegion(),numberColumns,solution);
      for (int iRow=0;iRow<numberRows;iRow++) {
	if (model->getRowStatus(iRow)!=ClpSimplex::basic)
	  effectiveRhs_[iRow]=solutionSlack[iRow];
	else
	  effectiveRhs_[iRow]=0.0;
      }
      for (int iColumn=0;iColumn<numberColumns;iColumn++) {
	if (model->getColumnStatus(iColumn)==ClpSimplex::basic)
	  solution[iColumn]=0.0;
      }
      times(-1.0,solution,effectiveRhs_);
      delete [] solution;
      lastRefresh_ = model->numberIterations();
      const double * columnSolution = model->solutionRegion();
      // and now subtract out non basic
      ClpSimplex::Status iStatus;
      for (int iSet=0;iSet<numberSets_;iSet++) {
	int iColumn = keyVariable_[iSet];
	if (iColumn<numberColumns) {
	  double b=0.0;
	  // key is structural - where is slack
	  iStatus = getStatus(iSet);
	  assert (iStatus!=ClpSimplex::basic);
	  if (iStatus==ClpSimplex::atLowerBound)
	    b=lower_[iSet];
	  else
	    b=upper_[iSet];
	  // subtract out others at bounds
	  int j;
	  for (j=start_[iSet];j<end_[iSet];j++) {
	    if (model->getColumnStatus(j)!=ClpSimplex::basic) 
	      b -= columnSolution[j];
	  }
	  // subtract out
	  ClpPackedMatrix::add(model,effectiveRhs_,iColumn,-b);
	}
      }
    }
  }
  return effectiveRhs_;
}
/* 
   update information for a pivot (and effective rhs)
*/
int 
ClpGubMatrix::updatePivot(ClpSimplex * model,double oldInValue, double oldOutValue)
{
  int sequenceIn = model->sequenceIn();
  int sequenceOut = model->sequenceOut();
  double * solution = model->solutionRegion();
  int numberColumns = model->numberColumns();
  int numberRows = model->numberRows();
  if (effectiveRhs_) {
    // update effective rhs
    if (sequenceIn==sequenceOut) {
      assert (sequenceIn<numberRows+numberColumns); // should be easy to deal with
      if (sequenceIn<numberColumns)
	add(model,effectiveRhs_,sequenceIn,oldInValue-solution[sequenceIn]);
      else
	effectiveRhs_[sequenceIn-numberColumns] -= oldInValue-solution[sequenceIn];
    } else {
      int iSetIn=-1;
      int iSetOut=-1;
      if (sequenceOut<numberColumns) {
	iSetOut = backward_[sequenceOut];
      } else if (sequenceOut>=numberRows+numberColumns) {
	assert (model->pivotRow()>=numberRows);
	int iExtra = model->pivotRow()-numberRows;
	assert (iExtra>=0);
	if (iSetOut<0)
	  iSetOut = fromIndex_[iExtra];
	else
	  assert(iSetOut == fromIndex_[iExtra]);
      }
      if (sequenceIn<numberColumns) {
	iSetIn = backward_[sequenceIn];
	// we need to test if WILL be key
	ClpPackedMatrix::add(model,effectiveRhs_,sequenceIn,oldInValue);
	if (iSetIn>=0)  {
	  // old contribution to effectiveRhs_
	  int key = keyVariable_[iSetIn];
	  if (key<numberColumns) {
	    double oldB=0.0;
	    ClpSimplex::Status iStatus = getStatus(iSetIn);
	    if (iStatus==ClpSimplex::atLowerBound)
	      oldB=lower_[iSetIn];
	    else
	      oldB=upper_[iSetIn];
	    // subtract out others at bounds
	    int j;
	    for (j=start_[iSetIn];j<end_[iSetIn];j++) {
	      if (j == sequenceIn) 
		oldB -= oldInValue;
	      else if (j!=key && j != sequenceOut &&
		  model->getColumnStatus(j)!=ClpSimplex::basic) 
		oldB -= solution[j];
	    }
	    if (oldB)
	      ClpPackedMatrix::add(model,effectiveRhs_,key,oldB);
	  }
	}
      } else if (sequenceIn<numberRows+numberColumns) {
	effectiveRhs_[sequenceIn-numberColumns] -= oldInValue;
      } else {
	printf("** in is key slack %d\n",sequenceIn);
	iSetIn=gubSlackIn_;
	// old contribution to effectiveRhs_
	int key = keyVariable_[iSetIn];
	if (key<numberColumns) {
	  double oldB=0.0;
	  ClpSimplex::Status iStatus = getStatus(iSetIn);
	  if (iStatus==ClpSimplex::atLowerBound)
	    oldB=lower_[iSetIn];
	  else
	    oldB=upper_[iSetIn];
	  // subtract out others at bounds
	  int j;
	  for (j=start_[iSetIn];j<end_[iSetIn];j++) {
	    if (j!=key && j != sequenceOut &&
		model->getColumnStatus(j)!=ClpSimplex::basic) 
	      oldB -= solution[j];
	  }
	  if (oldB)
	    ClpPackedMatrix::add(model,effectiveRhs_,key,oldB);
	}
      }
      if (sequenceOut<numberColumns) {
	iSetOut = backward_[sequenceOut];
	  ClpPackedMatrix::add(model,effectiveRhs_,sequenceOut,-solution[sequenceOut]);
	if (iSetOut>=0) {
	  // old contribution to effectiveRhs_
	  int key = keyVariable_[iSetOut];
	  if (key<numberColumns&&iSetIn!=iSetOut) {
	    double oldB=0.0;
	    ClpSimplex::Status iStatus = getStatus(iSetOut);
	    if (iStatus==ClpSimplex::atLowerBound)
	      oldB=lower_[iSetOut];
	    else
	      oldB=upper_[iSetOut];
	    // subtract out others at bounds
	    int j;
	    for (j=start_[iSetOut];j<end_[iSetOut];j++) {
	      if (j == sequenceIn) 
		oldB -= oldInValue;
	      else if (j!=key && j!=sequenceOut && 
		  model->getColumnStatus(j)!=ClpSimplex::basic) 
		oldB -= solution[j];
	    }
	    if (oldB)
	      ClpPackedMatrix::add(model,effectiveRhs_,key,oldB);
	  }
	}
      } else if (sequenceOut<numberRows+numberColumns) {
	effectiveRhs_[sequenceOut-numberColumns] -= -solution[sequenceOut];
      } else {
	printf("** out is key slack %d\n",sequenceOut);
	assert (model->pivotRow()>=numberRows);
	int iExtra = model->pivotRow()-numberRows;
	assert (iExtra>=0);
	if (iSetOut<0)
	  iSetOut = fromIndex_[iExtra];
	else
	  assert(iSetOut == fromIndex_[iExtra]);
	// old contribution to effectiveRhs_
	int key = keyVariable_[iSetOut];
	if (key<numberColumns&&iSetIn!=iSetOut) {
	  double oldB=0.0;
	  ClpSimplex::Status iStatus = getStatus(iSetOut);
	  if (iStatus==ClpSimplex::atLowerBound)
	    oldB=lower_[iSetOut];
	  else
	    oldB=upper_[iSetOut];
	  // subtract out others at bounds
	  int j;
	  for (j=start_[iSetOut];j<end_[iSetOut];j++) {
	    if (j == sequenceIn) 
	      oldB -= oldInValue;
	    else if (j!=key && j!=sequenceOut && 
		     model->getColumnStatus(j)!=ClpSimplex::basic) 
	      oldB -= solution[j];
	  }
	  if (oldB)
	    ClpPackedMatrix::add(model,effectiveRhs_,key,oldB);
	}
      }
    }
  }
  int * pivotVariable = model->pivotVariable();
  int iSetIn;
  if (sequenceIn<numberColumns)
    iSetIn = backward_[sequenceIn];
  else if (sequenceIn<numberColumns+numberRows)
    iSetIn = -1;
  else
    iSetIn = gubSlackIn_;
  int iSetOut=-1;
  if (sequenceOut<numberColumns) 
    iSetOut = backward_[sequenceOut];
  // may need to deal with key
  // Also need coding to mark/allow key slack entering
  if (model->pivotRow()>=numberRows) {
    int iExtra = model->pivotRow()-numberRows;
    assert (iExtra>=0);
    if (iSetOut<0)
      iSetOut = fromIndex_[iExtra];
    else
      assert(iSetOut == fromIndex_[iExtra]);
    assert (sequenceOut>=numberRows+numberColumns||sequenceOut==keyVariable_[iSetOut]);
    if (sequenceIn>=numberRows+numberColumns)
      printf("key slack %d in, set out %d\n",gubSlackIn_,iSetOut);
    printf("** danger - key out for set %d in %d (set %d)\n",iSetOut,sequenceIn,
	   iSetIn);
    // if slack out mark correctly
    if (sequenceOut>=numberRows+numberColumns) {
      double value=model->valueOut();
      if (value==upper_[iSetOut]) {
	setStatus(iSetOut,ClpSimplex::atUpperBound);
      } else if (value==lower_[iSetOut]) {
	setStatus(iSetOut,ClpSimplex::atLowerBound);
      } else {
	if (fabs(value-upper_[iSetOut])<
	    fabs(value-lower_[iSetOut])) {
	  setStatus(iSetOut,ClpSimplex::atUpperBound);
	  value = upper_[iSetOut];
	} else {
	  setStatus(iSetOut,ClpSimplex::atLowerBound);
	  value = lower_[iSetOut];
	}
      }
      if (upper_[iSetOut]==lower_[iSetOut])
	setStatus(iSetOut,ClpSimplex::isFixed);
      // subtract out bounds in set
      for (int j=start_[iSetOut];j<end_[iSetOut];j++) {
	if (model->getStatus(j)!=ClpSimplex::basic) {
	  value -= solution[j];
	}
      }
      setFeasible(iSetOut);
      // also update effective rhs
      if (effectiveRhs_) {
	int key;
	if (iSetOut==iSetIn) {
	  // key swap
	  if (next_[iSetOut+numberColumns]<0) {
	    // nothing else there
	    key = sequenceIn;
	    //add( model,effectiveRhs_,key,-value);
	  }
	} else {
	  key=pivotVariable[possiblePivotKey_];
	  //add( model,effectiveRhs_,key,-value);
	}
      }
    }
    if (iSetOut==iSetIn) {
      // key swap
      int key;
      if (sequenceIn>=numberRows+numberColumns) {
	key = numberColumns+iSetIn;
	setStatus(iSetIn,ClpSimplex::basic);
	printf("what should value be?\n");
      } else {
	key = sequenceIn;
	if (sequenceOut<numberRows+numberColumns) {
	  if (effectiveRhs_) {
	    if (next_[sequenceOut]<0) {
	      // nothing else there
	      //add( model,effectiveRhs_,key,-solution[key]);
	    }
	  }
	}
      }
      int j;
      keyVariable_[iSetIn]=key;
      int lastMarker = -(key+1);
      next_[key]=lastMarker;
      int last = key;
      for (j=start_[iSetOut];j<end_[iSetOut];j++) {
	if (j!=key&&model->getStatus(j)==ClpSimplex::basic) {
	  next_[last]=j;
	  next_[j]=lastMarker;
	  last = j;
	}
      }
    } else {
      // key was chosen
      assert (possiblePivotKey_>=0);
      int j;
      int key=pivotVariable[possiblePivotKey_];
      // also update effective rhs
      if (effectiveRhs_) {
	double value;
	assert (getStatus(iSetOut)!=ClpSimplex::basic);
	if (getStatus(iSetOut)==ClpSimplex::atUpperBound) {
	  value = upper_[iSetOut];
	} else {
	  value = lower_[iSetOut];
	}
	// don't need as this is currently key add( model,effectiveRhs_,keyVariable_[iSetOut],value);
	//add( model,effectiveRhs_,key,-value);
      }
      // and set incoming here
      pivotVariable[possiblePivotKey_]=sequenceIn;
      keyVariable_[iSetOut]=key;
      int lastMarker = -(key+1);
      next_[key]=lastMarker;
      int last = key;
      for (j=start_[iSetOut];j<end_[iSetOut];j++) {
	if (j!=key&&model->getStatus(j)==ClpSimplex::basic) {
	  next_[last]=j;
	  next_[j]=lastMarker;
	  last = j;
	}
      }
    }
  } else if (sequenceOut<numberColumns) {
    int iSetOut = backward_[sequenceOut];
    if (iSetOut>=0) {
      // just redo set
      int j;
      int key=keyVariable_[iSetOut];;
      int lastMarker = -(key+1);
      next_[key]=lastMarker;
      int last = key;
      for (j=start_[iSetOut];j<end_[iSetOut];j++) {
	if (j!=key&&model->getStatus(j)==ClpSimplex::basic) {
	  next_[last]=j;
	  next_[j]=lastMarker;
	  last = j;
	}
      }
    }
  }
  if (iSetIn>=0&&iSetIn!=iSetOut) {
    if (sequenceIn == numberColumns+2*numberRows) {
      // key slack in
      assert (model->pivotRow()<numberRows);
      // must swap with current key
      int key=keyVariable_[iSetIn];
      model->setStatus(key,ClpSimplex::basic);
      setStatus(iSetIn,ClpSimplex::basic);
      pivotVariable[model->pivotRow()]=key;
      keyVariable_[iSetIn] = iSetIn+numberColumns;
      // also update effective rhs
      if (effectiveRhs_) {
	double value;
	if (model->directionIn()<0) {
	  value = upper_[iSetIn];
	} else {
	  value = lower_[iSetIn];
	}
	//add( model,effectiveRhs_,key,value);
      }
    }
    // redo set to allow for new one
    int j;
    int key=keyVariable_[iSetIn];;
    int lastMarker = -(key+1);
    next_[key]=lastMarker;
    int last = key;
    for (j=start_[iSetIn];j<end_[iSetIn];j++) {
      if (j!=key&&model->getStatus(j)==ClpSimplex::basic) {
	next_[last]=j;
	next_[j]=lastMarker;
	last = j;
      }
    }
  }
  if (effectiveRhs_) {
    // update effective rhs
    if (sequenceIn!=sequenceOut) {
      if (sequenceIn<numberColumns) {
	if (iSetIn>=0) {
	  // new contribution to effectiveRhs_
	  int key = keyVariable_[iSetIn];
	  if (key<numberColumns) {
	    double newB=0.0;
	    ClpSimplex::Status iStatus = getStatus(iSetIn);
	    if (iStatus==ClpSimplex::atLowerBound)
	      newB=lower_[iSetIn];
	    else
	      newB=upper_[iSetIn];
	    // subtract out others at bounds
	    int j;
	    for (j=start_[iSetIn];j<end_[iSetIn];j++) {
	      if (j!=key && model->getColumnStatus(j)!=ClpSimplex::basic) 
		newB -= solution[j];
	    }
	    if (newB)
	      ClpPackedMatrix::add(model,effectiveRhs_,key,-newB);
	  }
	}
      }
      if (sequenceOut<numberColumns) {
	if (iSetOut>=0) {
	  // new contribution to effectiveRhs_
	  int key = keyVariable_[iSetOut];
	  if (key<numberColumns&&iSetIn!=iSetOut) {
	    double newB=0.0;
	    ClpSimplex::Status iStatus = getStatus(iSetOut);
	    if (iStatus==ClpSimplex::atLowerBound)
	      newB=lower_[iSetOut];
	    else
	      newB=upper_[iSetOut];
	    // subtract out others at bounds
	    int j;
	    for (j=start_[iSetOut];j<end_[iSetOut];j++) {
	      if (j!=key&&model->getColumnStatus(j)!=ClpSimplex::basic) 
		newB -= solution[j];
	    }
	    if (newB)
	      ClpPackedMatrix::add(model,effectiveRhs_,key,-newB);
	  }
	}
      }
    }
  }
#if 0
  // Now go over all sets which might have had changes
  {
    int iLook=0;
    int iSet=fromIndex_[0];
    double primalTolerance = model->primalTolerance();
    const double * cost = model->costRegion();
    while (iSet>=0) {
      int key=keyVariable_[iSet];
      double value=0.0;
      // sum over all except key
      for (int j=start_[iSet];j<end_[iSet];j++) {
	if (j!=key)
	  value += solution[j];
      }
      if (key<numberColumns) {
	assert (getStatus(iSet)!=ClpSimplex::basic);
	double sol;
	if (getStatus(iSet)==ClpSimplex::atUpperBound)
	  sol = upper_[iSet]-value;
	else
	  sol = lower_[iSet]-value;
	solution[key]=sol;
	// fix up cost
	model->nonLinearCost()->setOne(key,sol);
	printf("xx Value of key structural %d for set %d is %g - cost %g\n",key,iSet,sol,
	       cost[key]);
      } else {
	// slack is key
	if (value>upper_[iSet]+primalTolerance) {
	  setAbove(iSet);
	} else if (value<lower_[iSet]-primalTolerance) {
	  setBelow(iSet);
	} else {
	  setFeasible(iSet);
	}
	printf("xx Value of key slack for set %d is %g\n",iSet,value);
      }
      iSet = fromIndex_[++iLook];
    }
  }
#endif
#ifdef CLP_DEBUG
  // debug
  {
    int i;
    char xxxx[20000];
    memset(xxxx,0,numberColumns);
    for (i=0;i<numberRows;i++) {
      int iPivot = pivotVariable[i];
      assert (model->getStatus(iPivot)==ClpSimplex::basic);
      if (iPivot<numberColumns&&backward_[iPivot]>=0)
	xxxx[iPivot]=1;
    }
    double primalTolerance = model->primalTolerance();
    for (i=0;i<numberSets_;i++) {
      int key=keyVariable_[i];
      double value=0.0;
      // sum over all except key
      for (int j=start_[i];j<end_[i];j++) {
	if (j!=key)
	  value += solution[j];
      }
      int iColumn = next_[key];
      if (key<numberColumns) {
	// feasibility will be done later
	assert (getStatus(i)!=ClpSimplex::basic);
	double sol;
	if (getStatus(i)==ClpSimplex::atUpperBound)
	  sol = upper_[i]-value;
	else
	  sol = lower_[i]-value;
	//printf("xx Value of key structural %d for set %d is %g - cost %g\n",key,i,sol,
	//     cost[key]);
	//if (fabs(sol-solution[key])>1.0e-3)
	//printf("** stored value was %g\n",solution[key]);
      } else {
	// slack is key
	double infeasibility=0.0;
	if (value>upper_[i]+primalTolerance) {
	  infeasibility=value-upper_[i]-primalTolerance;
	  //setAbove(i);
	} else if (value<lower_[i]-primalTolerance) {
	  infeasibility=lower_[i]-value-primalTolerance ;
	  //setBelow(i);
	} else {
	  //setFeasible(i);
	}
	//printf("xx Value of key slack for set %d is %g\n",i,value);
      }
      while (iColumn>=0) {
	assert (xxxx[iColumn]);
	xxxx[iColumn]=0;
	iColumn=next_[iColumn];
      }
    }
    for (i=0;i<numberColumns;i++) {
      if (i<numberColumns&&backward_[i]>=0) {
	assert (!xxxx[i]||i==keyVariable_[backward_[i]]);
      }
    }
  }
#endif
  return 0;
}


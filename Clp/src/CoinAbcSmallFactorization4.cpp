/* $Id: CoinAbcSmallFactorization4.cpp 1373 2011-01-03 23:57:44Z lou $ */
// Copyright (C) 2002, International Business Machines
// Corporation and others, Copyright (C) 2012, FasterCoin.  All Rights Reserved.
// This code is licensed under the terms of the Eclipse Public License (EPL).

#if CLP_HAS_ABC
#include "CoinAbcCommonFactorization.hpp"
#ifndef ABC_JUST_ONE_FACTORIZATION
#define CoinAbcTypeFactorization CoinAbcSmallFactorization
#define ABC_SMALL 4
#include "CoinAbcBaseFactorization.hpp"
// If we are twiddling this one has data
#define FACTORS_HERE
#include "CoinAbcBaseFactorization4.cpp"
#endif
#endif
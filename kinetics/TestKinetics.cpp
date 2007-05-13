/**********************************************************************
** This program is part of 'MOOSE', the
** Messaging Object Oriented Simulation Environment,
**           copyright (C) 2003-2007 Upinder S. Bhalla. and NCBS
** It is made available under the terms of the
** GNU Lesser General Public License version 2.1
** See the file COPYING.LIB for the full notice.
**********************************************************************/
#ifdef DO_UNIT_TESTS

#include <fstream>
#include <math.h>
#include "header.h"
#include "moose.h"
#include "../element/Neutral.h"
#include "../builtins/Interpol.h"
#include "../builtins/Table.h"
#include "RateTerm.h"
#include "SparseMatrix.h"
#include "Stoich.h"

extern void testMolecule(); // Defined in Molecule.cpp
extern void testSparseMatrix(); // Defined in SparseMatrix.cpp
void testStoich();

void testKinetics()
{
	testMolecule();
	testSparseMatrix();
	testStoich();
}


//////////////////////////////////////////////////////////////////
// Here we set up a small reaction system for testing with the
// Stoich class.
//////////////////////////////////////////////////////////////////

void testStoich()
{
	cout << "\nTesting Stoich" << flush;
	const unsigned int NUM_COMPT = 10;

	Element* n = Neutral::create( "Neutral", "n", Element::root() );
	vector< Element* >m;
	vector< Element* >r;
	char name[10];
	bool ret;
	const Cinfo* molCinfo = Cinfo::find( "Molecule" );
	assert( molCinfo != 0 );
	const Finfo* rFinfo = molCinfo->findFinfo( "reac" );
	assert( rFinfo != 0 );

	const Cinfo* reacCinfo = Cinfo::find( "Reaction" );
	assert( reacCinfo != 0 );
	const Finfo* sFinfo = reacCinfo->findFinfo( "sub" );
	assert( sFinfo != 0 );
	const Finfo* pFinfo = reacCinfo->findFinfo( "prd" );
	assert( pFinfo != 0 );
	///////////////////////////////////////////////////////////
	// Create a linear sequence of molecules with reactions between.
	///////////////////////////////////////////////////////////
	for ( unsigned int i = 0; i < NUM_COMPT; i++ ) {
		sprintf( name, "m%d", i );
		Element* mtemp = Neutral::create( "Molecule", name, n );
		assert( mtemp != 0 );
		set< double >( mtemp, "nInit", 1.0 * i );
		set< double >( mtemp, "n", 1.0 * i );
		set< int >( mtemp, "mode", 0 );
		m.push_back( mtemp );

		if ( i > 0 ) {
			sprintf( name, "r%d", i );
			Element* rtemp = 
				Neutral::create( "Reaction", name, n );
			assert( rtemp != 0 );
			set< double >( rtemp, "kf", 0.1 );
			set< double >( rtemp, "kb", 0.1 );
			r.push_back( rtemp );
			ret = rFinfo->add( m[i - 1], rtemp, sFinfo );
			ASSERT( ret, "adding msg 0" );
			ret = rFinfo->add( mtemp, rtemp, pFinfo );
			ASSERT( ret, "adding msg 1" );
		}
	}

	///////////////////////////////////////////////////////////
	// Assign reaction system to a Stoich object
	///////////////////////////////////////////////////////////

	Element* stoich = Neutral::create( "Stoich", "s", Element::root() );

	ret = set< string >( stoich, "path", "/n/##" );
	ASSERT( ret, "Setting path" );

	Stoich* s = static_cast< Stoich* >( stoich->data() );
	ASSERT( s->N_.nRows() == NUM_COMPT, "num Species" ); 
	ASSERT( s->nMols_ == NUM_COMPT, "num Species" ); 
	ASSERT( s->N_.nColumns() == NUM_COMPT - 1, "num Reacns" ); 
	ASSERT( s->nReacs_ == NUM_COMPT - 1, "num Reacns" ); 
	ASSERT( s->reacMap_.size() == NUM_COMPT - 1, "num Reacns" ); 
	ASSERT( s->molMap_.size() == 10, "numSpecies" );
	// Note that the order of the species in the matrix is 
	// ill-defined because of the properties of the STL sort operation.
	// So here we need to look up the order based on the mol_map
	// Reac order has also been scrambled.
	
	// cout << s->N_;

	///////////////////////////////////////////////////////////
	// Check that stoich matrix is correct.
	///////////////////////////////////////////////////////////
	unsigned int molNum;
	int entry;
	map< const Element*, unsigned int >::iterator k;
	for ( unsigned int i = 0; i < NUM_COMPT; i++ ) {
		k = s->molMap_.find( m[i] );
		ASSERT( k != s->molMap_.end(), "look up molecule" );
		molNum = k->second;
		ASSERT( s->Sinit_[molNum] == 1.0 * i, "mol sequence" );
	}
	for ( unsigned int i = 0; i < NUM_COMPT; i++ ) {
		k = s->molMap_.find( m[i] );
		ASSERT( k != s->molMap_.end(), "look up molecule" );
		molNum = k->second;
		for ( unsigned int j = 0; j < NUM_COMPT - 1; j++ ) {
			k = s->reacMap_.find( r[j] );
			ASSERT( k != s->reacMap_.end(),
					"look up reac" );
			unsigned int reacNum = k->second;
			entry = s->N_.get( molNum, reacNum );
			if ( i == j )
				assert( entry == -1 );
			else if ( i == j + 1 )
				assert( entry == 1 );
			else
				assert( entry == 0 );
		}
	}

	///////////////////////////////////////////////////////////
	// Run an update step on this reaction system
	///////////////////////////////////////////////////////////
	cout << "\nTesting Stoich deriv calculation" << flush;
	for ( unsigned int i = 0; i < NUM_COMPT; i++ )
		s->S_[i] = s->Sinit_[i];

	const double EPSILON = 1e-10;

	vector< double > yprime( NUM_COMPT, 0.0 );
	s->updateRates( &yprime, 1.0 );
	for ( unsigned int i = 0; i < NUM_COMPT; i++ ) {
		k = s->molMap_.find( m[i] );
		molNum = k->second;
		if ( molNum == 0 ) {
			ASSERT( fabs( yprime[i] - 0.1 ) < EPSILON, "update");
		} else if ( molNum == 9 ) {
			ASSERT( fabs( yprime[i] + 0.1 ) < EPSILON, "update");
		} else {
			ASSERT( fabs( yprime[i] ) < EPSILON, "update" );
		}
	}

	///////////////////////////////////////////////////////////
	// Connect up stoich to KineticHub
	///////////////////////////////////////////////////////////
	
	cout << "\nTesting Stoich zombie data access" << flush;
	// Clean out the old stoich
	set( stoich, "destroy" );
	stoich = Neutral::create( "Stoich", "s", Element::root() );

	Element* hub = Neutral::create( "KineticHub", "hub", Element::root() );
	ret = stoich->findFinfo( "hub" )->
		add( stoich, hub, hub->findFinfo( "hub" ) );
	ASSERT( ret, "connecting stoich to hub" );

	// Rebuild the path now that the hub is connected.
	ret = set< string >( stoich, "path", "/n/##" );
	ASSERT( ret, "Setting path" );

	ret = set< double >( m[7], "n", 1234.5 );
	ASSERT( ret, "Setting value" );
	s = static_cast< Stoich* >( stoich->data() );
	k = s->molMap_.find( m[7] );
	molNum = k->second;
	ASSERT( s->S_[molNum] == 1234.5, "Setting value" );

	k = s->molMap_.find( m[3] );
	molNum = k->second;
	s->Sinit_[molNum] = 543210.0;
	double dret;
	ret = get< double >( m[3], "nInit", dret );
	ASSERT( ret, "Getting value" );
	ASSERT( dret = 543210.0, "Getting value" );

	/////////////////////////////////////////////////////////
	// Here we try reaction acces.
	/////////////////////////////////////////////////////////
	unsigned int index;
	ret = set< double >( r[5], "kf", 111.222 );
	ASSERT( ret, "Setting reac value" );
	k = s->reacMap_.find( r[5] );
	ASSERT( k != s->reacMap_.end(), "reac values" );
	index = k->second;
	ASSERT( s->rates_[index]->getR1() == 111.222, "Setting reac value" );
	ASSERT( s->rates_[index]->getR2() == 0.1, "Setting reac value" );

	k = s->reacMap_.find( r[2] );
	ASSERT( k != s->reacMap_.end(), "reac values" );
	index = k->second;
	s->rates_[index]->setR2( 999.888 );
	ret = get< double >( r[2], "kb", dret );
	ASSERT( ret, "Getting reac value" );
	ASSERT( dret == 999.888, "Getting reac value" );
	ret = get< double >( r[2], "kf", dret );
	ASSERT( ret, "Getting reac value" );
	ASSERT( dret == 0.1, "Getting reac value" );

	/////////////////////////////////////////////////////////
	// Now move on to handling pre-existing messages.
	/////////////////////////////////////////////////////////

	cout << "\nTesting Stoich external message redirection" << flush;
	// Clean out the old stuff
	set( hub, "destroy" );
	set( stoich, "destroy" );

	Element* table = Neutral::create( "Table", "table", Element::root() );
	ret = table->findFinfo( "outputSrc" )->add( table, m[5], 
			m[5]->findFinfo( "sumTotal" ) );
	ASSERT( ret, "Making test message" );

	stoich = Neutral::create( "Stoich", "s", Element::root() );

	hub = Neutral::create( "KineticHub", "hub", Element::root() );
	ret = stoich->findFinfo( "hub" )->
		add( stoich, hub, hub->findFinfo( "hub" ) );
	ASSERT( ret, "connecting stoich to hub" );

	// Rebuild the path now that the hub is connected.
	ret = set< string >( stoich, "path", "/n/##" );
	ASSERT( ret, "Setting path" );

	set< int >( table, "stepmode", 0 );
	set< int >( table, "xdivs", 1 );
	set< double >( table, "xmin", 0.0 );
	set< double >( table, "xmax", 10.0 );
	lookupSet< double, unsigned int >( table, "table", 33.0, 0 );
	lookupSet< double, unsigned int >( table, "table", 33.0, 1 );
	set< int >( table, "stepmode", 0 );
	Conn c( table, 0 );
	ProcInfoBase p;
	p.dt_ = 0.001;
	p.currTime_ = 0.0;
	Table::process( c, &p );
	// Check that the value has been added to the correct molecule
	ret = get< double >( m[5], "n", dret );
	ASSERT( ret, "Getting value" );
	ASSERT( dret = 33.0, "Message redirection" );
	k = s->molMap_.find( m[5] );
	molNum = k->second;
	ASSERT( s->S_[molNum] == 33.0, "Message redirection" );


	/////////////////////////////////////////////////////////
	// Get rid of all the compartments.
	/////////////////////////////////////////////////////////
	set( table, "destroy" );
	set( hub, "destroy" );
	set( stoich, "destroy" );
	set( n, "destroy" );
}

#endif

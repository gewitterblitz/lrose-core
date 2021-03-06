// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=* 
// ** Copyright UCAR (c) 1990 - 2016                                         
// ** University Corporation for Atmospheric Research (UCAR)                 
// ** National Center for Atmospheric Research (NCAR)                        
// ** Boulder, Colorado, USA                                                 
// ** BSD licence applies - redistribution and use in source and binary      
// ** forms, with or without modification, are permitted provided that       
// ** the following conditions are met:                                      
// ** 1) If the software is modified to produce derivative works,            
// ** such modified software should be clearly marked, so as not             
// ** to confuse it with the version available from UCAR.                    
// ** 2) Redistributions of source code must retain the above copyright      
// ** notice, this list of conditions and the following disclaimer.          
// ** 3) Redistributions in binary form must reproduce the above copyright   
// ** notice, this list of conditions and the following disclaimer in the    
// ** documentation and/or other materials provided with the distribution.   
// ** 4) Neither the name of UCAR nor the names of its contributors,         
// ** if any, may be used to endorse or promote products derived from        
// ** this software without specific prior written permission.               
// ** DISCLAIMER: THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS  
// ** OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED      
// ** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.    
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=* 
/////////////////////////////////////////////////////////////
// Props.cc
//
// Props class - storm properties computations
//
// Mike Dixon, RAP, NCAR, P.O.Box 3000, Boulder, CO, 80307-3000, USA
//
// November 1998
//
///////////////////////////////////////////////////////////////

#include "Params.hh"
#include "Props.hh"
#include "InputMdv.hh"
#include "StormFile.hh"
#include "Area.hh"
#include "GridClump.hh"
#include "Verify.hh"

#include <toolsa/umisc.h>
#include <toolsa/str.h>
#include <toolsa/pmu.h>
#include <rapmath/math_macros.h>
#include <rapmath/umath.h>
#include <physics/vil.h>
using namespace std;

#define MAX_EIG_DIM 3

//////////////
// constructor
//

Props::Props(char *prog_name, Params *params,
	     InputMdv *input_mdv, StormFile *storm_file,
	     Verify *verify)

{

  OK = TRUE;
  _progName = STRdup(prog_name);
  _params = params;
  _inputMdv = input_mdv;
  _stormFile = storm_file;
  _verify = verify;
  
  _area = new Area(_progName, _params, _inputMdv, _stormFile);

  // alloc arrays to initial sizes

  _nZAlloc = 1;
  _nHistAlloc = 1;
  
  _layer =
    (layer_stats_t *) umalloc (_nZAlloc * sizeof(layer_stats_t));
  _dbzHist =
    (dbz_hist_entry_t *) umalloc (_nHistAlloc * sizeof(dbz_hist_entry_t));
  _tiltData =
    (double **) umalloc2 (_nZAlloc, MAX_EIG_DIM, sizeof(double));
  _dbzGradientData =
    (double **) umalloc2 (_nZAlloc, MAX_EIG_DIM, sizeof(double));

  _means = (double *) umalloc (MAX_EIG_DIM * sizeof(double));
  _eigenvalues =
    (double *) umalloc (MAX_EIG_DIM * sizeof(double));
  _eigenvectors =
    (double **) umalloc2 (MAX_EIG_DIM, MAX_EIG_DIM, sizeof(double));

}

/////////////
// destructor
//

Props::~Props()

{

  STRfree(_progName);

  if (_area) {
    delete (_area);
  }

  if (_layer) {
    ufree(_layer);
  }

  if (_dbzHist) {
    ufree(_dbzHist);
  }

  if (_tiltData) {
    ufree2((void **) _tiltData);
  }

  if (_dbzGradientData) {
    ufree2((void **) _dbzGradientData);
  }

  if (_means) {
    ufree(_means);
  }

  if (_eigenvalues) {
    ufree(_eigenvalues);
  }

  if (_eigenvectors) {
    ufree2((void **) _eigenvectors);
  }

}

///////////////////////////////////////
// initialize for latest MDV input file
//

void Props::init()

{

  // min valid ht and number of valid planes
  
  minValidZ = _inputMdv->grid.minz +
    _inputMdv->minValidLayer * _inputMdv->grid.dz;
  _nzValid = _inputMdv->grid.nz - _inputMdv->minValidLayer;
  
  // compute test sampling range - if any points in a storm are at
  // longer range, set flag indicating it is range limited. The test
  // range is the max range less twice the diagonal of a grid cell.
  // For this test, it is only necessary to consider x and y, since the
  // elevation angle will be low.

  DsRadarParams_t *radar = &_inputMdv->handle.radarParams;
  
  if(radar->ngates > 0) {
    double max_range = ((double) radar->start_range +
		 (double) radar->gate_spacing *
		 radar->ngates) / 1000000.0;
    double range_margin =
      2.0 * pow(((double) _inputMdv->grid.dx * _inputMdv->grid.dx +
		 _inputMdv->grid.dy * _inputMdv->grid.dy), 0.5);
    _testRange = max_range - range_margin;
  } else {
    _testRange = 0.0;
  }
  
  // compute test elevation angle - if any points in a storm are at
  // higher elevation angle, set flag to indicate that the storm top
  // has not been sampled. The test angle is the highest elevation angle
  // in the volume scan less some margin.

  DsRadarElev_t *elevs = &_inputMdv->handle.radarElevs;

  if (elevs->n_elev > 0) {
    double elev_margin = radar->vert_beam_width;
    double max_elev = elevs->elev_array[elevs->n_elev - 1];
    _testElev = (max_elev - elev_margin) * DEG_TO_RAD;
  } else {
    _testElev = 0.0;
  }
  
  _dbzScale = _inputMdv->handle.fld_hdrs[_params->dbz_field].scale;
  _dbzBias = _inputMdv->handle.fld_hdrs[_params->dbz_field].bias;

  if (_params->vel_available) {
    _velScale = _inputMdv->handle.fld_hdrs[_params->vel_field].scale;
    _velBias = _inputMdv->handle.fld_hdrs[_params->vel_field].bias;
  }
  
  // inverse coefficients for the Z-R and Z-M relationships
  
  _ZPInverseCoeff = 1.0 / _params->ZR.coeff;
  _ZPInverseExpon = 1.0 / _params->ZR.expon;
  
  _ZMInverseCoeff = 1.0 / _params->ZM.coeff;
  _ZMInverseExpon = 1.0 / _params->ZM.expon;
  
  // vorticity factor to take into account the fact that
  // positive vorticity is anti-clockwise in the northern
  // hemisphere and clockwise in the southern hemisphere.
  // Northern - latitude positive. Southern - latitude negative.
  
  if (radar->latitude >= 0) {
    _vortHemisphereFactor = 1.0;
  } else {
    _vortHemisphereFactor = -1.0;
  }
    
  // min vorticity distance limits the calculations to a certain
  // distance from the cemtroid. to avoid very close points from
  // having a dis-proportional effect on the calculation
    
  _minVortDist = 2.0 * sqrt(_inputMdv->grid.dx * _inputMdv->grid.dx +
			    _inputMdv->grid.dy * _inputMdv->grid.dy);
    
  // initialize the area_comps module

  
  _area->init(_dbzScale, _dbzBias,
	      _ZPInverseCoeff, _ZPInverseExpon);
  
}

////////////////////////////////////////////////
// compute()
//
// Returns 0 if success (clump is a valid storm)
//         -1 if failure
//

int Props::compute(GridClump *grid_clump, int storm_num)

      
{

  _gridClump = grid_clump;
  storm_file_handle_t *s_handle = &_stormFile->handle;

  PMU_auto_register("Props::compute - computing properties");

  // allocate

  _alloc(_inputMdv->grid.nz, _stormFile->nDbzHistIntervals);

  // initialize
    
  _baseLayer = _nzValid;
  _topLayer = 0;
  _rangeLimited = FALSE;
  _topMissing = FALSE;
  _hailPresent = FALSE;
  _secondTrip = FALSE;
  _highDbzByte = _inputMdv->highDbzByte;
  
  memset (&_gprops, 0, sizeof(storm_file_global_props_t));
  memset (_layer, 0, _nzValid * sizeof(layer_stats_t));
  memset (_dbzHist, 0,
	  _stormFile->nDbzHistIntervals * sizeof(dbz_hist_entry_t));
  memset (&_sum, 0, sizeof(sum_stats_t));
    
  // first pass through the clumps, computing the relevant things
  // from which to compute the storm properties.
  // Also, count the number of data runs for this storm.
  
  if (_computeFirstPass()) {
    return (-1);
  }
  
  // perform the areal computations for precip and projected
  // areas, including dbz histogram for area

  _area->compute(_gridClump, &_gprops, _dbzHist);

  // compute other props during second pass through intervals
  
  _computeSecondPass();

  // tilt angle computations

  _tiltCompute();

  // compute dbz gradient with height

  _dbzGradientCompute();

  // decide whether this is a second trip echo
   
  if (_params->check_second_trip) {
    _secondTrip = _checkSecondTrip();
  }
      
  // write valid_storms verification file
  
  if (_params->create_verification_files) {
    _verify->updateValidStormsGrid(_gridClump);
  }

  // load up global storm properties

  storm_file_global_props_t *gprops =
    s_handle->gprops + storm_num;
  *gprops = _gprops;
  
  _loadGprops(gprops,
 	      storm_num, _nLayers, _baseLayer, _nDbzIntvls,
 	      _rangeLimited, _topMissing,
 	      _hailPresent, _secondTrip);
  
  // load layer props structure, for each layer
  
  for (int iz = _baseLayer; iz <= _topLayer; iz++) {
    _loadLprops(_layer + iz, s_handle->layer + iz - _baseLayer);
  }
  
  // load dbz histogram struct, for each histogram interval
    
  for (int dbz_intvl = 0; dbz_intvl < _nDbzIntvls; dbz_intvl++) {
    _loadDbzHist(_dbzHist + dbz_intvl,
		 s_handle->hist + dbz_intvl);
  }
  
  // store runs
  
  if (_params->store_storm_runs) {
    gprops->n_runs = _storeRuns();
  } else {
    gprops->n_runs = 0;
  }

  if (_params->store_proj_runs) {
    gprops->n_proj_runs = _area->storeProjRuns();
  } else {
    gprops->n_proj_runs = 0;
  }

  return (0);

}

////////////////////
// _alloc()
//

void Props::_alloc(int nz, int nhist)

{

  if (nz > _nZAlloc) {

    _layer = (layer_stats_t *) urealloc
      (_layer, nz * sizeof(layer_stats_t));

    ufree2((void **) _tiltData);
    _tiltData = (double **) umalloc2 (nz, MAX_EIG_DIM, sizeof(double));
    
    ufree2((void **) _dbzGradientData);
    _dbzGradientData = (double **) umalloc2 (nz, MAX_EIG_DIM, sizeof(double));
    
    _nZAlloc = nz;

  }

  if (nhist > _nHistAlloc) {

    _dbzHist = (dbz_hist_entry_t *) urealloc
      (_dbzHist, nhist * sizeof(dbz_hist_entry_t));
    
    _nHistAlloc = nhist;

  }
    
  RfAllocStormProps(&_stormFile->handle, _inputMdv->grid.nz,
		    _stormFile->nDbzHistIntervals,
		    0, 0, "Props::_alloc");


}

//////////////////////////////////////
// _computeFirstPass()
//
// First pass through data
//
// Returns 0 if successful, -1 if not
//

// FUDGE_X is the distance x is moved to avoid errors from
// computing arctan when the points are coincident
#define FUDGE_X (0.1)

int Props::_computeFirstPass()

{

  int max_dbz_intvl = 0;
  mdv_grid_t *grid = &_gridClump->grid;

  // initialize

  Interval *intvl = _gridClump->intervals;
  for (int intv = 0; intv < _gridClump->nIntervals; intv++, intvl++) {
    
    int iz = intvl->plane;
    int jz = iz + _inputMdv->minValidLayer;
    int iy = intvl->row_in_plane;
    _layer[iz].n += intvl->len;
	
    int index = ((iy + _gridClump->startIy) * grid->nx +
		 intvl->begin + _gridClump->startIx);
    
    ui08 *dbz_ptr = _inputMdv->dbzPlanes[jz] + index;
    ui08 *vel_ptr = NULL;
    if (_params->vel_available) {
      vel_ptr = _inputMdv->velPlanes[jz] + index;
    }
      
    for (int ix = intvl->begin; ix <= intvl->end;
	 ix++, dbz_ptr++, vel_ptr++) {
      
      // if dbz value exceeds max threshold, don't
      // process this clump
	
      if ((int) *dbz_ptr > _highDbzByte) {
	return (-1);
      }
      
      // get the dbz, vel and mass_factor
	
      double dbz = (double) *dbz_ptr * _dbzScale + _dbzBias;
      double r_dbz;

      if (dbz > _params->hail_dbz_threshold) {
	r_dbz = _params->hail_dbz_threshold;
	_hailPresent = TRUE;
      } else {
	r_dbz = dbz;
      }
	
      double refl = pow(10.0, r_dbz / 10.0);
      
      index = iy * grid->nx + ix;
	
      double vel;
      if (_params->vel_available) {
	vel = *vel_ptr * _velScale + _velBias;
      }
      
      double mass_factor = pow(refl, _ZMInverseExpon);
	
      // get the dbz histogram index number
      
      int dbz_intvl = _inputMdv->dbzInterval[*dbz_ptr];
      
      // increment quantities
      
      // number of grid points per layer
      
      _layer[iz].sum_x += ix;
      _layer[iz].sum_y += iy;
      
      // reflectivity
      
      _layer[iz].sum_refl_x += (double) ix * refl;
      _layer[iz].sum_refl_y += (double) iy * refl;
      _layer[iz].sum_refl += refl;
      
      if (dbz > _layer[iz].dbz_max) {
	_layer[iz].dbz_max = dbz;
      }
      
      // dbz histograms
      
      if (dbz_intvl >= 0) {
	_dbzHist[dbz_intvl].n_vol++;
      }
      
      // mass
      
      _layer[iz].sum_mass += mass_factor;
      
      // velocity
      
      if (_params->vel_available) {
	_layer[iz].sum_vel += vel;
	_layer[iz].sum_vel2 += vel * vel;
      }
      
    } // ix
    
  } // intv  - first pass
  
  // sum up variables over layers
  
  int dbz_max_layer = 0;

  for (int iz = 0; iz < _nzValid; iz++) {
    
    if (_layer[iz].n > 0) {
      
      _sum.n += _layer[iz].n;
      _sum.x += _layer[iz].sum_x;
      _sum.y += _layer[iz].sum_y;
      _sum.z += iz * _layer[iz].n;
      
      _sum.refl += _layer[iz].sum_refl;
      _sum.refl_x += _layer[iz].sum_refl_x;
      _sum.refl_y += _layer[iz].sum_refl_y;
      _sum.refl_z += (double) iz * _layer[iz].sum_refl;
	
      if (_layer[iz].dbz_max >= _gprops.dbz_max) {
	_gprops.dbz_max = _layer[iz].dbz_max;
	dbz_max_layer = iz;
      }
	
      _sum.mass += _layer[iz].sum_mass;
	
      if (iz > _topLayer)
	_topLayer = iz;
	
      if (iz < _baseLayer)
	_baseLayer = iz;
	
      if (_params->vel_available) {
	_sum.vel += _layer[iz].sum_vel;
	_sum.vel2 += _layer[iz].sum_vel2;
      }
      
    }
      
  } // iz
    
  _nLayers = _topLayer - _baseLayer + 1;
    
  // compute the properties
    
  double dn = (double) _sum.n;

  _gprops.vol_centroid_x =
    _gridClump->startX + ((double) _sum.x / dn) * grid->dx; // km
  _gprops.vol_centroid_y =
    _gridClump->startY + ((double) _sum.y / dn) * grid->dy; // km
  _gprops.vol_centroid_z =
    minValidZ + ((double) _sum.z / dn) * grid->dz; // km
  
  _gprops.refl_centroid_x =
    _gridClump->startX + (_sum.refl_x / _sum.refl) * grid->dx; // km
    
  _gprops.refl_centroid_y =
    _gridClump->startY + (_sum.refl_y / _sum.refl) * grid->dy; // km
  
  _gprops.refl_centroid_z =
    minValidZ + (_sum.refl_z / _sum.refl) * grid->dz; // km
  
  _gprops.volume = _gridClump->stormSize; // km3 or km2
  
  _gprops.area_mean =
    (dn / (double) _nLayers) * _gridClump->dAreaAtCentroid; // km2
  
  _gprops.top = minValidZ +
    ((double) _topLayer + 0.5) * grid->dz; // km
    
  _gprops.base = minValidZ +
    ((double) _baseLayer - 0.5) * grid->dz; // km
    
  _gprops.ht_of_dbz_max = minValidZ +
    (double) dbz_max_layer * grid->dz; // km
    
  _gprops.dbz_mean = log10(_sum.refl / dn) * 10.0; // dbz
  
  _gprops.rad_vel_mean = (_sum.vel / dn); // m/s
  
  _gprops.rad_vel_sd = usdev(_sum.vel, _sum.vel2, dn);

  _gprops.mass = (_sum.mass * _gridClump->dVolAtCentroid
		  * pow(_ZMInverseCoeff,
			_ZMInverseExpon)); // ktons
    
  // layer properties

  for (int iz = 0; iz < _nzValid; iz++) {
    
    _layer[iz].area =
      (double) _layer[iz].n * _gridClump->dAreaAtCentroid; // km2
    
    if (_layer[iz].n > 0) {
      
      double dnperlayer = (double) _layer[iz].n;
	
      _layer[iz].vol_centroid_x = _gridClump->startX +
	((double) _layer[iz].sum_x / dnperlayer) * grid->dx; // km
      _layer[iz].vol_centroid_y = _gridClump->startY +
	((double) _layer[iz].sum_y / dnperlayer) * grid->dy; // km
	
      _layer[iz].refl_centroid_x = _gridClump->startX +
	(_layer[iz].sum_refl_x / _layer[iz].sum_refl) * grid->dx; // km
      _layer[iz].refl_centroid_y = _gridClump->startY +
	(_layer[iz].sum_refl_y / _layer[iz].sum_refl) * grid->dy; // km
	
      _layer[iz].dbz_mean = log10(_layer[iz].sum_refl / dnperlayer) * 10.0;
      
      _layer[iz].mass =
	(_layer[iz].sum_mass * _gridClump->dVolAtCentroid
	 * pow(_ZMInverseCoeff, _ZMInverseExpon)); // ktons
      
      if (_params->vel_available) {
	_layer[iz].vel_mean = _layer[iz].sum_vel / dnperlayer;
	_layer[iz].vel_sd = usdev(_layer[iz].sum_vel,
				 _layer[iz].sum_vel2, dnperlayer);
      }

    } // if (_layer[iz].n .......
    
  } // iz

  // vil

  vil_init();
  for (int iz = 0; iz < _nzValid; iz++) {
    if (_layer[iz].n > 0) {
      vil_add(_layer[iz].dbz_max, grid->dz);
    } /* if (_layer[iz].n ....... */
  } /* iz */
  _gprops.vil_from_maxz = vil_compute();
  
  // dbz histograms
  
  for (int dbz_intvl = 0;
       dbz_intvl < _stormFile->nDbzHistIntervals; dbz_intvl++) {
    
    if (_dbzHist[dbz_intvl].n_vol > 0) {
      _dbzHist[dbz_intvl].percent_vol = 100.0 *
	(double) _dbzHist[dbz_intvl].n_vol / dn;
      max_dbz_intvl = dbz_intvl;
    } else {
      _dbzHist[dbz_intvl].percent_vol = 0.0;
    }
      
  } // dbz_intvl
    
  _nDbzIntvls = max_dbz_intvl + 1;
    
  // compute the azimuth and range from the radar to the layer centroids
    
  for (int iz = _baseLayer; iz <= _topLayer; iz++) {
      
    if (grid->sensor_x == _layer[iz].vol_centroid_x &&
	grid->sensor_y == _layer[iz].vol_centroid_y)
      _layer[iz].vol_centroid_x += FUDGE_X;
      
    double cent_x = _layer[iz].vol_centroid_x - grid->sensor_x;
    double cent_y = _layer[iz].vol_centroid_y - grid->sensor_y;
      
    double range = sqrt(cent_x * cent_x + cent_y * cent_y);
    _layer[iz].vol_centroid_range = range;
    if (range == 0.0)
      _layer[iz].vol_centroid_az = 0.0;
    else
      _layer[iz].vol_centroid_az = atan2(cent_y, cent_x);
      
  }

  return (0);

}

//////////////////////////////////////
// _computeSecondPass()
//
// Second pass through data, computing vorticity and checking
// range limits as required.
//

// vorticity model assumes the storm has a 
// circular structure
#define VORTICITY_MODEL_FACTOR 2.0

void Props::_computeSecondPass()

{

  mdv_grid_t *grid = &_inputMdv->grid;

  // initialize
  
  Interval *intvl = _gridClump->intervals;
  for (int intv = 0; intv < _gridClump->nIntervals; intv++, intvl++) {

    int iz = intvl->plane;
    int jz = iz + _inputMdv->minValidLayer;
    int iy = intvl->row_in_plane;
    
    double grid_x = (double) intvl->begin * grid->dx + _gridClump->startX;
    double grid_y = (double) iy * grid->dy + _gridClump->startY;
    double grid_z = (double) jz * grid->dz + grid->minz;
    double rel_y = grid_y - grid->sensor_y;
    double rel_z = grid_z - grid->sensor_z;

    int index = ((iy + _gridClump->startIy) * grid->nx +
		 intvl->begin + _gridClump->startIx);
    ui08 *vel_ptr = NULL;
    if (_params->vel_available) {
      vel_ptr = _inputMdv->velPlanes[jz] + index;
    }
   
    for (int ix = intvl->begin; ix <= intvl->end;
	 ix++, grid_x += grid->dx, vel_ptr++) {
      
      // compute grid x and y
      
      if (_params->check_range_limits) {
      
	// Test the range of the run end_points, and flag the storm if the
	// range exceeds the test range - only perform this if not
	// already flagged. Only (x, y) is considered relevant -
	// z will have negligible effect on range computations at
	// long range.

	// Also, test the elevation angle of the run end_points, and flag
	// the storm if the angle exceeds the test angle - only
	// perform this if not already flagged.
	// For these computations, the curvature of the earth and
	// beam may be ignored, since range is short, and elevation
	// angle high.
      
	if (ix == intvl->begin || ix == intvl->end) {
	
	  double rel_x = grid_x - grid->sensor_x;
	
	  double gnd_range = pow((rel_x * rel_x + rel_y * rel_y), 0.5);
	  
	  if (_testRange > 0.0 && !_rangeLimited) {
	    
	    if (gnd_range > _testRange)
	      _rangeLimited = TRUE;
	    else if (ix == 0)
	      _rangeLimited = TRUE;
	    else if (ix == grid->nx - 1)
	      _rangeLimited = TRUE;
	    else if (iy == 0)
	      _rangeLimited = TRUE;
	    else if (iy == grid->ny - 1)
	      _rangeLimited = TRUE;
	    
	    if (_params->debug >= Params::DEBUG_VERBOSE && _rangeLimited) {
	      fprintf(stderr,
		      "***** Range limited, x, y, range = "
		      "%g, %g, %g\n",
		      grid_x, grid_y, gnd_range);
	    }
	    
	  } // if (!_rangeLimited)

	  if (_testElev > 0.0 && !_topMissing) {
	    
	    double elev;

	    if (rel_z == 0.0 && gnd_range == 0.0)
	      elev = 0.0;
	    else
	      elev = atan2(rel_z, gnd_range);
	    
	    if (elev > _testElev) {
	      
	      _topMissing = TRUE;
	      
	      if (_params->debug >= Params::DEBUG_VERBOSE) {
		fprintf(stderr,
			"** Top missing,x,y,z, elev = "
			"%g,%g,%g,%g\n",
			grid_x, grid_y, grid_z,
			elev * RAD_TO_DEG);
	      }
	    
	    } // if (elev > _testElev)
	  
	  } // if (!_topMissing)
	
	} // if (ix == intvl->begin ...
	
      } // if (_params->check_range_limits)
	
      if (_params->vel_available) {

	// compute grid azimuth from radar,
	// and delta azimuth from vol_centroid azimuth
	
	double grid_az;
	if (grid_x == grid->sensor_x && grid_y == grid->sensor_y) {
	  grid_az = 0.0;
	} else {
	  grid_az = atan2(grid_y - grid->sensor_y, grid_x - grid->sensor_x);
	}
	
	double delta_az = _layer[iz].vol_centroid_az - grid_az;
	
	if (delta_az > M_PI) {
	  delta_az -= TWO_PI;
	}
	
	if (delta_az < M_PI) {
	  delta_az += TWO_PI;
	}
	
	double vorticity_dist = _layer[iz].vol_centroid_range * sin(delta_az);

	if (fabs(vorticity_dist) >= _minVortDist) {
	  double vel = *vel_ptr * _velScale + _velBias;
	  _layer[iz].n_vorticity++;
	  _layer[iz].sum_vorticity +=
	    (vel - _layer[iz].vel_mean) / (vorticity_dist * 1000.0);
	} // if (fabs ....
	
      } // if ((_params->vel_available)
      
    } // ix
    
  } // intv
  
  if (_params->vel_available) {
    
    // compute the vorticity at each layer and for the entire volume
    
    for (int iz = _baseLayer; iz <= _topLayer; iz++) {
      
      _sum.vorticity += _layer[iz].sum_vorticity;
      _sum.n_vorticity += _layer[iz].n_vorticity;
      
      if (_layer[iz].n_vorticity > 0) {
	_layer[iz].vorticity = ((_layer[iz].sum_vorticity /
				 (double) _layer[iz].n_vorticity) *
				VORTICITY_MODEL_FACTOR *
			       _vortHemisphereFactor);
      } else {
	_layer[iz].vorticity = 0.0;
      }
      
    }
    
    if (_sum.n_vorticity > 0) {
      _gprops.vorticity =
	((_sum.vorticity / (double) _sum.n_vorticity) *
	 VORTICITY_MODEL_FACTOR * _vortHemisphereFactor);
    } else {
      _gprops.vorticity = 0.0;
    }
    
  } else {
    
    for (int iz = _baseLayer; iz <= _topLayer; iz++)
      _layer[iz].vorticity = 0.0;
    
    _gprops.vorticity = 0.0;
    
  } // if (_params->vel_available)

}

////////////////////////////////////////////      
// _storeRuns()
//
// Store the storm runs
//
// Returns the number of runs in the clump.
//

int Props::_storeRuns()

{
  
  // make sure there is space for the runs

  RfAllocStormProps(&_stormFile->handle, _gridClump->grid.nz,
		    _stormFile->nDbzHistIntervals,
		    _gridClump->nIntervals, 0,
		    "Props::_storeRuns");
  
  storm_file_run_t *run = _stormFile->handle.runs;
  int start_ix = _gridClump->startIx;
  int start_iy = _gridClump->startIy;
  
  Interval *intvl = _gridClump->intervals;
  for (int intv = 0; intv < _gridClump->nIntervals; intv++, intvl++, run++) {

    run->ix = intvl->begin + start_ix;
    run->iy = intvl->row_in_plane + start_iy;
    run->iz = intvl->plane + _inputMdv->minValidLayer;
    run->n = intvl->len;
	
  } // intv

  return (_gridClump->nIntervals);

}

//////////////////
// _tiltCompute()
//
// Compute tilt from  principal component analysis
//
// Note: area is in sq km.
//       tilt_area is in sq grid_units.
//

void Props::_tiltCompute()
     
{

  // check for more than 1 layer
  
  if (_nLayers < 2) {
    return;
  }

  // tilt angle computations
      
  // load up tilt_data array with the centroid positions for
  // each layer. The z values are exaggerated by a factor of 10.0 to
  // force the vertical to be the direction of dominant variance
      
  for (int iz = _baseLayer; iz <= _topLayer; iz++) {
    _tiltData[iz][0] = _layer[iz].vol_centroid_x;
    _tiltData[iz][1] = _layer[iz].vol_centroid_y;
    _tiltData[iz][2] = minValidZ +
      (double) iz * _inputMdv->handle.grid.dz * 10.0;
  }
      
  // obtain the principal component transformation for this data - the
  // technique is applicable here because the first principal component
  // will lie along the axis of maximum variance, which is equivalent
  // to fitting a line through the data points, minimizing the sum of
  // the squared perpendicular distances from the data to the line.
  
  if (upct(3, _nLayers, _tiltData + _baseLayer,
	   _means, _eigenvectors, _eigenvalues) != 0) {
    
    fprintf(stderr, "WARNING - %s:Props::_tiltCompute\n", _progName);
    fprintf(stderr, "Computing pct for tilt angle and direction.\n");
    _gprops.tilt_dirn = MISSING_VAL;
    _gprops.tilt_angle = MISSING_VAL;
    
  } else {
    
    // compute the tilt angle and direction from the first eigenvector
    
    double tilt_x = _eigenvectors[0][0];
    double tilt_y = _eigenvectors[1][0];
    double tilt_z = _eigenvectors[2][0] / 10.0;
    double tilt_r = sqrt(tilt_x * tilt_x + tilt_y * tilt_y);
    
    if (tilt_x == 0 && tilt_y == 0) {
      _gprops.tilt_dirn = 0.0;
      _gprops.tilt_angle = 0.0;
    } else {
      _gprops.tilt_dirn = atan2(tilt_x, tilt_y) * RAD_TO_DEG;
      if (_gprops.tilt_dirn < 0) {
	_gprops.tilt_dirn += 360.0;
      }
      _gprops.tilt_angle = 90.0 - atan2(tilt_z, tilt_r) * RAD_TO_DEG;
    }
    
  } // if (upct(3, _nLayers, _tiltData + _baseLayer, ...

}

/////////////////////////////////////////////////////////
// _dbzGradientCompute()
//
// Compute dbz_gradient from  principal component analysis
//

void Props::_dbzGradientCompute()
     
{
  
  // check for more than 1 layer

  if (_nLayers < 2) {
    return;
  }

  // load up dbz_gradient_data array with dbz_max.
  // The z values are exaggerated by a factor of 1000.0 to
  // force the vertical to be the direction of dominant variance
      
  for (int iz = _baseLayer; iz <= _topLayer; iz++) {
    _dbzGradientData[iz][0] = _layer[iz].dbz_max;
    _dbzGradientData[iz][1] = minValidZ +
      (double) iz * _inputMdv->handle.grid.dz * 1000.0;
  }
      
  // obtain the principal component transformation for this data - the
  // technique is applicable here because the first principal component
  // will lie along the axis of maximum variance, which is equivalent
  // to fitting a line through the data points, minimizing the sum of
  // the sguared perpendicular distances from the data to the line.
  
  if (upct(2, _nLayers,
	   _dbzGradientData + _baseLayer,
	   _means, _eigenvectors, _eigenvalues) != 0) {
    fprintf(stderr, "WARNING - %s:Props::_dbzGradientCompute\n",
	    _progName);
    fprintf(stderr, "Computing pct for dbz_max_gradient.\n");
    _gprops.dbz_max_gradient = MISSING_VAL;
  } else {
    _gprops.dbz_max_gradient = _eigenvectors[0][0] * 1000.0;
  } // if (upct ........

  // Repeat for dbz_mean.
      
  for (int iz = _baseLayer; iz <= _topLayer; iz++) {
    _dbzGradientData[iz][0] = _layer[iz].dbz_mean;
    _dbzGradientData[iz][1] = minValidZ +
      (double) iz * _inputMdv->handle.grid.dz * 1000.0;
  }
      
  if (upct( 2, _nLayers,
	    _dbzGradientData + _baseLayer,
	    _means, _eigenvectors, _eigenvalues) != 0) {
    fprintf(stderr, "WARNING - %s:Props::_dbzGradientCompute\n",
	    _progName);
    fprintf(stderr, "Computing pct for dbz_mean_gradient.\n");
    _gprops.dbz_mean_gradient = MISSING_VAL;
  } else {
    _gprops.dbz_mean_gradient = _eigenvectors[0][0] * 1000.0;
  }

}

////////////////////////////////////////////////////////////////////
// _checkSecondTrip()
//
// Checks for second trip  - returns TRUE or FALSE
//
// Check is based on aspect ratio of the echo, and how closely the
// major axis of the fitted ellipse lies to the radial from the
// radar to the storm centroid.
//

int Props::_checkSecondTrip()

{

  double top = _gprops.top;
  double base = _gprops.base;
  double centroid_x = _gprops.proj_area_centroid_x;
  double centroid_y = _gprops.proj_area_centroid_y;
  double major_radius = _gprops.proj_area_major_radius;
  double minor_radius = _gprops.proj_area_minor_radius;
  double orientation =  _gprops.proj_area_orientation;

  double radial_azimuth;
  if (centroid_x == 0.0 && centroid_y == 0.0) {
    radial_azimuth = 0.0;
  } else {
    radial_azimuth = atan2(centroid_x, centroid_y) * RAD_TO_DEG;
  }
  if (radial_azimuth < 0.0) {
    radial_azimuth += 180.0;
  }

  double delta_orientation = fabs(radial_azimuth - orientation);
  
  int orientation_OK;
  if (delta_orientation < _params->sectrip_orientation_error ||
      180.0 - delta_orientation < _params->sectrip_orientation_error) {
    orientation_OK = TRUE;
  } else {
    orientation_OK = FALSE;
  }

  double vert_aspect = major_radius / (top - base);
  double horiz_aspect = major_radius / minor_radius;

  int vert_aspect_OK;
  if (vert_aspect > _params->sectrip_vert_aspect) {
    vert_aspect_OK = TRUE;
  } else {
    vert_aspect_OK = FALSE;
  }

  int horiz_aspect_OK;
  if (horiz_aspect > _params->sectrip_horiz_aspect) {
    horiz_aspect_OK = TRUE;
  } else {
    horiz_aspect_OK = FALSE;
  }

  if (_params->debug >= Params::DEBUG_VERBOSE &&
      orientation_OK && vert_aspect_OK && horiz_aspect_OK) {

    fprintf(stderr, "\n++++++++++ SECOND_TRIP ++++++++++++++\n");

    fprintf(stderr, "x,y : %g, %g\n", centroid_x, centroid_y);
    fprintf(stderr, "top, base : %g, %g\n", top, base);
    fprintf(stderr, "maj, min rad : %g, %g\n", major_radius,
	    minor_radius);
    fprintf(stderr, "orientation : %g\n", orientation);

    fprintf(stderr, "rad az : %g\n", radial_azimuth);
    fprintf(stderr, "delta orientation : %g\n", delta_orientation);
    fprintf(stderr, "orientation OK : %d\n", orientation_OK);
    fprintf(stderr, "vert, horiz aspect: %g, %g\n",
	    vert_aspect, horiz_aspect);
    fprintf(stderr, "vert_OK, horiz_OK : %d, %d\n\n",
	   vert_aspect_OK, horiz_aspect_OK);

  }

  if (orientation_OK && vert_aspect_OK && horiz_aspect_OK) {
    return (TRUE);
  } else {
    return (FALSE);
  }

}

/***************************************************************************
 * load_props.c
 *
 * Loads up the storm props
 *
 * Mike Dixon
 *
 * RAP, NCAR, Boulder, Colorado, USA
 *
 * April 1995
 *
 ****************************************************************************/

//////////////////////////////////
// _loadGprops()
//
// Load global storm properties
//

void Props::_loadGprops(storm_file_global_props_t *gprops,
			int storm_num,
			int n_layers,
			int base_layer,
			int n_dbz_intvls,
			int range_limited,
			int top_missing,
			int hail_present,
			int second_trip)

{

  gprops->storm_num = storm_num;
  gprops->n_layers = n_layers;
  gprops->base_layer = base_layer;
  gprops->n_dbz_intervals = n_dbz_intvls;
  
  gprops->range_limited = range_limited;
  gprops->top_missing = top_missing;
  gprops->hail_present = hail_present;
  gprops->second_trip = second_trip;
  
}


//////////////////////////////////
// _loadLprops()
//
// Load layer storm properties
//

void Props::_loadLprops(layer_stats_t *layer,
			 storm_file_layer_props_t *lprops)
     
{
  
  lprops->vol_centroid_x = layer->vol_centroid_x;
  lprops->vol_centroid_y = layer->vol_centroid_y;
  lprops->refl_centroid_x = layer->refl_centroid_x;
  lprops->refl_centroid_y = layer->refl_centroid_y;
  
  lprops->area = layer->area;
  lprops->dbz_max = layer->dbz_max;
  lprops->dbz_mean = layer->dbz_mean;
  lprops->mass = layer->mass;
  lprops->rad_vel_mean = layer->vel_mean;
  lprops->rad_vel_sd = layer->vel_sd;
  lprops->vorticity = layer->vorticity;
  
}

//////////////////////////////
// _loadDbzHist()
//
// Load dbz histogram props
//

void Props::_loadDbzHist(dbz_hist_entry_t *dbz_hist,
			 storm_file_dbz_hist_t *hist)

{

  hist->percent_volume = dbz_hist->percent_vol;
  hist->percent_area = dbz_hist->percent_area;

}
      
      

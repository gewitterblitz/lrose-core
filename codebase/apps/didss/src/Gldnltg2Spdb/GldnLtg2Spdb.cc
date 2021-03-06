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
/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

/**-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-**/
/*********************************************************************
 * GldnLtg2Spdb.cc: Program to convert GLDN lightning data into
 *                 SPDB format.
 *
 * RAP, NCAR, Boulder CO
 *
 * July 2004
 *
 * Gary Blackburn/Kay Levesque
 *
 *********************************************************************/

#include <cassert>
#include <signal.h>
#include <string>

#include <iostream>
#include <toolsa/ucopyright.h>
#include <toolsa/pmu.h>
#include <toolsa/str.h>
#include <toolsa/DateTime.hh>
#include <didss/RapDataDir.hh>
#include <Spdb/DsSpdb.hh>

#include <Spdb/LtgSpdbBuffer.hh>
#include "GldnLtg2Spdb.hh"
using namespace std;

// Global variables

GldnLtg2Spdb *GldnLtg2Spdb::_instance = (GldnLtg2Spdb *)NULL;

// Global constants

const int FOREVER = true;


/*********************************************************************
 * Constructor
 */

GldnLtg2Spdb::GldnLtg2Spdb(int argc, char **argv)
{
  const string method_name = "GldnLtg2Spdb::Constructor";
  
  // Make sure the singleton wasn't already created.

  assert(_instance == (GldnLtg2Spdb *)NULL);
  
  // Initialize the okay flag.

  okay = true;
  
  // Set the program name.
  _progName = "GldnLtg2Spdb";
  
  // Display ucopright message.
  ucopyright((char *) _progName.c_str());

  // Get the command line arguments.

  _args = new Args(argc, argv, _progName);
  
  if (!_args->okay)
  {
    cerr << "ERROR: "<< method_name << endl;
    cerr << "Problem with command line arguments." << endl;
    
    okay = false;
    
    return;
  }
  
  // Get TDRP parameters.

  _params = new Params();
  char *params_path = "unknown";
  
  if (_params->loadFromArgs(argc, argv, _args->override.list, &params_path))
  {
    cerr << "ERROR: " << method_name << endl;
    cerr << "Problem with TDRP parameters in file <" << params_path << ">" <<
      endl;
    
    okay = false;
    
    return;
  }

  // check args in ARCHIVE mode

  if (_params->mode == Params::ARCHIVE) {
    if (_args->inputFileList.size() == 0) {
      if ((_args->startTime == 0 || _args->endTime == 0)) {
        cerr << "ERROR: " << _progName << endl;
        cerr << "In ARCHIVE mode, you must specify a file list" << endl
             << "  or start and end times." << endl;
        okay = FALSE;
        return;
      }
    }
  }
  // init process mapper registration

  PMU_auto_init((char *) _progName.c_str(),
                _params->instance,
                PROCMAP_REGISTER_INTERVAL);

  // Create the input objects


  if (_params->mode == Params::ARCHIVE) {
    if (_args->inputFileList.size() > 0) {
      _input = new DsInputPath(_progName,
                               _params->debug,
                               _args->inputFileList);
      _input->setSearchExt("txt");
    } else if (_args->startTime != 0 && _args->endTime != 0) {
      string inDir;
      RapDataDir.fillPath(_params->input_dir, inDir);
      if (_params->debug) {
        cerr << "Input dir: " << inDir << endl;
      }
      _input = new DsInputPath(_progName,
                               _params->debug,
                               inDir,
                               _args->startTime,
                               _args->endTime);
      _input->setSearchExt("txt");
    }
  } else {
    string inDir;
    RapDataDir.fillPath(_params->input_dir, inDir);
    if (_params->debug) {
      cerr << "Input dir: " << inDir << endl;
    }
    _input = new DsInputPath(_progName,
                             _params->debug,
                             inDir,
                             _params->max_realtime_valid_age,
                             PMU_auto_register);
  }


  // Set the singleton instance pointer

  _instance = this;

  return;
}


/*********************************************************************
 * Destructor
 */

GldnLtg2Spdb::~GldnLtg2Spdb()
{
  // Free contained objects

  delete _params;
  delete _args;

  if (_input)
    delete _input;
  
}


/*********************************************************************
 * Inst() - Retrieve the singleton instance of this class.
 */

GldnLtg2Spdb *GldnLtg2Spdb::Inst(int argc, char **argv)
{
  if (_instance == (GldnLtg2Spdb *)NULL)
    new GldnLtg2Spdb(argc, argv);
  
  return(_instance);
}

GldnLtg2Spdb *GldnLtg2Spdb::Inst()
{
  assert(_instance != (GldnLtg2Spdb *)NULL);
  
  return(_instance);
}

/*********************************************************************
 * run()
 */

void GldnLtg2Spdb::run()
{
  const string method_name = "GldnLtg2Spdb::run()";
  
  // register with procmap
 
  PMU_auto_register("Run");

  int startup_flag = TRUE;
  
  // Create local objects

  GldnLtgFile ltg_file(_params->debug);
  LtgSpdbBuffer spdb_buffer(_params->debug);
  
  if (_params->mode == Params::ARCHIVE) {
    _input->reset();
  }

  char *inputFilePath;
  char procmap_string[BUFSIZ];

  while ((inputFilePath = _input->next()) != NULL) {
 
    sprintf(procmap_string, "Processing file <%s>", inputFilePath);

    if (_params->debug) {
      cerr << "Processing input file: " << inputFilePath << endl;
    }

    PMU_force_register(procmap_string);
      
    if (!ltg_file.loadFile(inputFilePath , _params->processing_delay))
    {
      cerr << "ERROR: " << method_name << endl;
      cerr << "Error loading GLDN data from file <" << inputFilePath <<
      ">" << endl;
      cerr << "   *** SKIPPING FILE ***" << endl;
      continue;
    }
      
    int strike_count = 0;
    LTG_strike_t *strike = ltg_file.getFirstStrike();

    time_t buffer_time = strike->time;

    while (strike != (LTG_strike_t *) NULL)
    {
	
       // See if this strike goes in the current SPDB buffer

       if (buffer_time != strike->time)
       {
         // Send to SPDB destinations

         spdb_buffer.writeToDatabase(_params->spdb_database,
				      _params->expire_secs,
				      startup_flag && _params->check_old_data);
	  
         spdb_buffer.clear();
	  
	 buffer_time = strike->time;
	  
       } /* endif - buffer_time != strike->time */
	
       // Register with the process mapper again just in case this
       // is a long file
	
       PMU_auto_register(procmap_string);

       // Convert the buffer to SPDB format and add it to the buffer.
       // First check to make sure that the strike is within the grid.
       // Note that GLDNLTG_to_spdb() returns a pointer to a static
       // data area so this pointer must NOT be freed.

       LTG_print_strike(stderr, strike);

       if (strike->latitude >= _params->min_strike_lat &&
	    strike->latitude <= _params->max_strike_lat &&
	    strike->longitude >= _params->min_strike_lon &&
	    strike->longitude <= _params->max_strike_lon)
       {
	  
         spdb_buffer.addStrike(strike);
       }
       else
       {
         if (_params->debug)
           cerr << "*** Ltg strike at " << strike->latitude << ", " <<
             strike->longitude << " outside of grid" << endl;
       }
	
       // Increment the strike count

       strike_count++;
	
       strike = ltg_file.getNextStrike();
    } /* endwhile - strike != NULL */
      
    // Send the last strikes in the file to the SPDB destinations

    spdb_buffer.writeToDatabase(_params->spdb_database,
				  _params->expire_secs,
				  startup_flag && _params->check_old_data);
      
    spdb_buffer.clear();
      
    if (_params->debug) {
	cerr << "Found " << strike_count << " strikes in input file <" <<
	  inputFilePath << ">" << endl;
    }
      
    // Close the input file

    ltg_file.close();
    
  }   // endwhile inputFilePath = _input->next()) != NULL
  return;

}
  


/**********************************************************************
 *              Private Member Functions                              *
 **********************************************************************/

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
////////////////////////////////////////////////////////////////////////
// CwbAws2Spdb.cc
//
// CwbAws2Spdb object
//
// Mike Dixon, RAP, NCAR, P.O.Box 3000, Boulder, CO, 80307-3000, USA
//
// May 2008
//
///////////////////////////////////////////////////////////////
//
// CwbAws2Spdb reads automated weather station surface
// observations, converts them to station_report_t format
// and writes them to an SPDB data base
//
///////////////////////////////////////////////////////////////////////

#include <cerrno>
#include <rapmath/math_macros.h>
#include <toolsa/umisc.h>
#include <toolsa/file_io.h>
#include <toolsa/pmu.h>
#include <toolsa/str.h>
#include <toolsa/Path.hh>
#include <toolsa/TaArray.hh>
#include <didss/DsInputPath.hh>
#include <physics/physics.h>
#include "CwbAws2Spdb.hh"
using namespace std;

// Constructor

CwbAws2Spdb::CwbAws2Spdb(int argc, char **argv)

{

  isOK = true;

  // set programe name

  _progName = "CwbAws2Spdb";
  ucopyright((char *) _progName.c_str());

  // get command line args

  if (_args.parse(argc, argv, _progName)) {
    cerr << "ERROR: " << _progName << endl;
    cerr << "Problem with command line args" << endl;
    isOK = FALSE;
    return;
  }

  // get TDRP params
  
  _paramsPath = (char *) "unknown";
  if (_params.loadFromArgs(argc, argv, _args.override.list,
			   &_paramsPath)) {
    cerr << "ERROR: " << _progName << endl;
    cerr << "Problem with TDRP parameters" << endl;
    isOK = FALSE;
    return;
  }

  // init process mapper registration

  PMU_auto_init((char *) _progName.c_str(),
		_params.instance,
		PROCMAP_REGISTER_INTERVAL);

  return;

}

// destructor

CwbAws2Spdb::~CwbAws2Spdb()

{

  // unregister process

  PMU_auto_unregister();

}

//////////////////////////////////////////////////
// Run

int CwbAws2Spdb::Run ()
{

  // register with procmap
  
  PMU_auto_register("Run");

  // file input object
  
  DsInputPath *input = NULL; // Set to NULL to get around compiler warnings.
  
  if (_params.mode == Params::FILELIST) {
    
    // FILELIST mode
    
    input = new DsInputPath(_progName,
			    _params.debug >= Params::DEBUG_VERBOSE,
			    _args.inputFileList);
    
  } else if (_params.mode == Params::ARCHIVE) {
    
    // archive mode - start time to end time
    
    input = new DsInputPath(_progName,
			    _params.debug >= Params::DEBUG_VERBOSE,
			    _params.input_dir,
			    _args.startTime, _args.endTime);
    
  } else if (_params.mode == Params::REALTIME) {
    
    // realtime mode - no latest_data_info file
    
    input = new DsInputPath(_progName,
			    _params.debug >= Params::DEBUG_VERBOSE,
			    _params.input_dir,
			    _params.max_realtime_valid_age,
			    PMU_auto_register,
			    _params.latest_data_info_avail,
			    true);
    
    if (_params.strict_subdir_check) {
      input->setStrictDirScan(true);
    }
    
    if (_params.file_name_check) {
      input->setSubString(_params.file_match_string);
    }
    
  }
  
  // loop through available files
  
  char *inputPath;
  while ((inputPath = input->next()) != NULL) {

    if (_params.station_type == Params::AWS_STATION) {
      if (_processAwsFile(inputPath)) {
	cerr << "WARNING - CwbAws2Spdb::Run" << endl;
	cerr << "  Errors in processing aws file: " << inputPath << endl;
      }
    } else if (_params.station_type == Params::ONE_MINUTE_AWS) {
      if (_processOneMinAwsFile(inputPath)) {
	cerr << "WARNING - CwbAws2Spdb::Run" << endl;
	cerr << "  Errors in processing one minute aws file: "
             << inputPath << endl;
      }
    } else {
      if (_processPrecipFile(inputPath)) {
	cerr << "WARNING - CwbAws2Spdb::Run" << endl;
	cerr << "  Errors in processing precip file: " << inputPath << endl;
      }
    }
    
  } // while
    
  return 0;

}

////////////////////
// process AWS file

int CwbAws2Spdb::_processAwsFile(const char *file_path)
  
{

  int iret = 0;

  if (_params.debug) {
    cerr << "Processing aws file: " << file_path << endl;
  }

  // registration

  char procmapString[BUFSIZ];
  Path path(file_path);
  sprintf(procmapString, "Processing file <%s>", path.getFile().c_str());
  PMU_force_register(procmapString);

  // compute the time from the file name

  Path ppath(file_path);
  string fileName = ppath.getFile();

  int year, month, day, hour, min;
  if (sscanf(fileName.c_str(), "%4d-%2d-%2d_%2d%2d",
	     &year, &month, &day, &hour, &min) != 5) {
    cerr << "ERROR - CwbAws2Spdb::_processAwsFile" << endl;
    cerr << "  Cannot compute time from file name" << endl;
    cerr << "  File path: " << file_path << endl;
    cerr << "  File name: " << fileName << endl;
    return -1;
  }
  DateTime validTime(year, month, day, hour, min, 0);

  // create spdb output object
  
  DsSpdb out;

  // read in file, loading up chunks in output object spdb

  FILE *in;
  if ((in = fopen(file_path, "r")) == NULL) {
    int errNum = errno;
    cerr << "ERROR - CwbAws2Spdb::_processAwsFile" << endl;
    cerr << "  Cannot open file: " << file_path << endl;
    cerr << "  " << strerror(errNum) << endl;
    return -1;
  }

  int nObs = 0;
  while (!feof(in)) {

    char line[1024];

    if (fgets(line, 1024, in) == NULL) {
      break;
    }

    char stid[128];
    double lat, lon, elev, wdir, wspd, temp, humd, pres;
    double sunhr, solarrad, preciphr, thourmax, thourmin;

    if (sscanf(line, "%5s%lg%lg%lg%lg%lg%lg%lg%lg%lg%lg%lg%lg%lg",
	       stid,
	       &lat, &lon, &elev, &wdir, &wspd, &temp, &humd, &pres,
	       &sunhr, &solarrad, &preciphr, &thourmax, &thourmin) != 14) {
      if (strstr(line, "STID") == NULL) {
	// not first line, print error message
	cerr << "ERROR - CwbAws2Spdb::_processAwsFile" << endl;
	cerr << "  Cannot read line: " << line;
      }
      continue;
    }

    if (_params.debug >= Params::DEBUG_VERBOSE) {
      cerr << "Read line: " << line;
      cerr << " stid: " << stid << endl;
      cerr << " lat: " << lat << endl;
      cerr << " lon: " << lon << endl;
      cerr << " elev: " << elev << endl;
      cerr << " wdir: " << wdir << endl;
      cerr << " wspd: " << wspd << endl;
      cerr << " temp: " << temp << endl;
      cerr << " humd: " << humd << endl;
      cerr << " pres: " << pres << endl;
      cerr << " sunhr: " << sunhr << endl;
      cerr << " solarrad: " << solarrad << endl;
      cerr << " preciphr: " << preciphr << endl;
      cerr << " thourmax: " << thourmax << endl;
      cerr << " thourmin: " << thourmin << endl;
    }
    
    // fill out station report
    
    station_report_t report;
    memset(&report, 0, sizeof(report));
    
    STRncopy(report.station_label, stid, ST_LABEL_SIZE);
    
    report.msg_id = SENSOR_REPORT;
    report.time = validTime.utime();
    
    report.lat = lat;
    report.lon = lon;
    report.alt = elev;
    report.temp = temp;
    double dew_point = PHYrhdp(temp, humd * 100.0);
    dew_point = ((int) floor (dew_point * 10.0 + 0.5)) / 10.0;
    report.dew_point = dew_point;
    report.relhum = humd * 100.0;
    report.windspd = wspd;
    report.winddir = wdir;
    report.windgust = STATION_NAN;
    report.pres = pres;
    report.liquid_accum = STATION_NAN;
    report.precip_rate = preciphr;
    report.visibility = STATION_NAN;
    report.ceiling = STATION_NAN;
    report.rvr = STATION_NAN;
    
    if (_params.debug >= Params::DEBUG_VERBOSE) {
      cerr << "====== FOUND REPORT ======" << endl;
      print_station_report(stderr, "", &report);
      cerr << "==========================" << endl;
    }
    
    // swap bytes
    
    station_report_to_be(&report);
    
    // add chunk
    
    int stationId = Spdb::hash4CharsToInt32(stid + 1);
    out.addPutChunk(stationId,
		    validTime.utime(),
		    validTime.utime() + _params.expire_seconds,
		    sizeof(report), &report);
    
    nObs++;
    
  } // while

  if (nObs < 0) {
    cerr << "ERROR - CwbAws2Spdb::_processAwsFile" << endl;
    cerr << "  No valid obs in file: " << file_path << endl;
    return -1;
  }

  // put the data
  
  if (out.put(_params.output_url,
	      SPDB_STATION_REPORT_ID,
	      SPDB_STATION_REPORT_LABEL)) {
    cerr << "ERROR - CwbAws2Spdb::_processAwsFile" << endl;
    cerr << "  Cannot put station data to: "
	 << _params.output_url << endl;
    cerr << "  " << out.getErrStr() << endl;
    iret = -1;
  }

  if (_params.debug >= Params::DEBUG_VERBOSE) {
    cerr << "Wrote " << nObs << " reports to url: "
	 << _params.output_url << endl;
  }

  if (_params.debug) {
    cerr << "  Done with file: " << file_path << endl;
  }

  return iret;
   
}

///////////////////////////////
// process one minute AWS file

int CwbAws2Spdb::_processOneMinAwsFile(const char *file_path)
  
{

  int iret = 0;

  if (_params.debug) {
    cerr << "Processing one minute aws file: " << file_path << endl;
  }

  // registration

  char procmapString[BUFSIZ];
  Path path(file_path);
  sprintf(procmapString, "Processing file <%s>", path.getFile().c_str());
  PMU_force_register(procmapString);

  // compute the time from the file name

  Path ppath(file_path);
  string fileName = ppath.getFile();

  int year, month, day, hour, min;
  if (sscanf(fileName.c_str(), "%4d-%2d-%2d_%2d%2d",
	     &year, &month, &day, &hour, &min) != 5) {
    cerr << "ERROR - CwbAws2Spdb::_processOneMinAwsFile" << endl;
    cerr << "  Cannot compute time from file name" << endl;
    cerr << "  File path: " << file_path << endl;
    cerr << "  File name: " << fileName << endl;
    return -1;
  }
  DateTime validTime(year, month, day, hour, min, 0);

  // create spdb output object
  
  DsSpdb out;

  // read in file, loading up chunks in output object spdb

  FILE *in;
  if ((in = fopen(file_path, "r")) == NULL) {
    int errNum = errno;
    cerr << "ERROR - CwbAws2Spdb::_processOneMinAwsFile" << endl;
    cerr << "  Cannot open file: " << file_path << endl;
    cerr << "  " << strerror(errNum) << endl;
    return -1;
  }

  int nObs = 0;
  while (!feof(in)) {

    char line[1024];

    if (fgets(line, 1024, in) == NULL) {
      break;
    }

    char localTime[128];
    char stid[128];
    double lat, lon, elev, wdir, wspd, temp, td, pres;
    double precip24hr, precip1min;

    if (sscanf(line, "%16s%5s%lg%lg%lg%lg%lg%lg%lg%lg%lg%lg",
	       localTime, stid,
	       &lat, &lon, &elev, &wdir, &wspd, &temp, &td, &pres,
	       &precip24hr, &precip1min) != 12) {
      if (strstr(line, "STID") == NULL) {
	// not first line, print error message
	cerr << "ERROR - CwbAws2Spdb::_processAwsFile" << endl;
	cerr << "  Cannot read line: " << line;
      }
      continue;
    }

    if (_params.debug >= Params::DEBUG_VERBOSE) {
      cerr << "Read line: " << line;
      cerr << " stid: " << stid << endl;
      cerr << " lat: " << lat << endl;
      cerr << " lon: " << lon << endl;
      cerr << " elev: " << elev << endl;
      cerr << " wdir: " << wdir << endl;
      cerr << " wspd: " << wspd << endl;
      cerr << " temp: " << temp << endl;
      cerr << " td: " << td << endl;
      cerr << " pres: " << pres << endl;
      cerr << " precip24hr: " << precip24hr << endl;
      cerr << " precip1min: " << precip1min << endl;
    }
    
    // fill out station report
    
    station_report_t report;
    memset(&report, 0, sizeof(report));
    
    STRncopy(report.station_label, stid, ST_LABEL_SIZE);
    
    report.msg_id = SENSOR_REPORT;
    report.time = validTime.utime();
    
    report.lat = lat;
    report.lon = lon;
    report.alt = elev;
    report.temp = temp;
    report.dew_point = td;
    report.relhum = PHYrelh(temp, td);
    report.windspd = wspd;
    report.winddir = wdir;
    report.windgust = STATION_NAN;
    report.pres = pres;
    report.liquid_accum = precip24hr;
    report.precip_rate = precip1min * 60.0;
    report.visibility = STATION_NAN;
    report.ceiling = STATION_NAN;
    report.rvr = STATION_NAN;
    
    if (_params.debug >= Params::DEBUG_VERBOSE) {
      cerr << "====== FOUND REPORT ======" << endl;
      print_station_report(stderr, "", &report);
      cerr << "==========================" << endl;
    }
    
    // swap bytes
    
    station_report_to_be(&report);
    
    // add chunk
    
    int stationId = Spdb::hash4CharsToInt32(stid + 1);
    out.addPutChunk(stationId,
		    validTime.utime(),
		    validTime.utime() + _params.expire_seconds,
		    sizeof(report), &report);
    
    nObs++;
    
  } // while

  if (nObs < 0) {
    cerr << "ERROR - CwbAws2Spdb::_processAwsFile" << endl;
    cerr << "  No valid obs in file: " << file_path << endl;
    return -1;
  }

  // put the data
  
  if (out.put(_params.output_url,
	      SPDB_STATION_REPORT_ID,
	      SPDB_STATION_REPORT_LABEL)) {
    cerr << "ERROR - CwbAws2Spdb::_processAwsFile" << endl;
    cerr << "  Cannot put station data to: "
	 << _params.output_url << endl;
    cerr << "  " << out.getErrStr() << endl;
    iret = -1;
  }

  if (_params.debug >= Params::DEBUG_VERBOSE) {
    cerr << "Wrote " << nObs << " reports to url: "
	 << _params.output_url << endl;
  }

  if (_params.debug) {
    cerr << "  Done with file: " << file_path << endl;
  }

  return iret;
   
}

///////////////////////
// process precip file

int CwbAws2Spdb::_processPrecipFile(const char *file_path)
  
{

  int iret = 0;

  if (_params.debug) {
    cerr << "Processing precip file: " << file_path << endl;
  }

  // registration

  char procmapString[BUFSIZ];
  Path path(file_path);
  sprintf(procmapString, "Processing file <%s>", path.getFile().c_str());
  PMU_force_register(procmapString);

  // compute the time from the file name

  Path ppath(file_path);
  string fileName = ppath.getFile();

  int year, month, day, hour, min;
  if (sscanf(fileName.c_str(), "%4d-%2d-%2d_%2d%2d",
	     &year, &month, &day, &hour, &min) != 5) {
    cerr << "ERROR - CwbAws2Spdb::_processPrecipFile" << endl;
    cerr << "  Cannot compute time from file name" << endl;
    cerr << "  File path: " << file_path << endl;
    cerr << "  File name: " << fileName << endl;
    return -1;
  }
  DateTime validTime(year, month, day, hour, min, 0);
  
  // create spdb output object
  
  DsSpdb out;

  // read in file, loading up chunks in output object spdb
  
  FILE *in;
  if ((in = fopen(file_path, "r")) == NULL) {
    int errNum = errno;
    cerr << "ERROR - CwbAws2Spdb::_processPrecipFile" << endl;
    cerr << "  Cannot open file: " << file_path << endl;
    cerr << "  " << strerror(errNum) << endl;
    return -1;
  }

  int nObs = 0;
  while (!feof(in)) {

    char line[1024];

    if (fgets(line, 1024, in) == NULL) {
      break;
    }

    char stid[128];
    double lat, lon, elev, precip;

    if (sscanf(line, "%5s%lg%lg%lg%lg",
	       stid, &precip, &lon, &lat, &elev) != 5) {
      if ((strstr(line, "FROM") == NULL) &&
	  (strstr(line, "stnno") == NULL)) {
	// not comments lines, print error message
	cerr << "ERROR - CwbAws2Spdb::_processPrecipFile" << endl;
	cerr << "  Cannot read line: " << line;
      }
      continue;
    }

    if (_params.debug >= Params::DEBUG_VERBOSE) {
      cerr << "Read line: " << line;
      cerr << " stid: " << stid << endl;
      cerr << " lat: " << lat << endl;
      cerr << " lon: " << lon << endl;
      cerr << " elev: " << elev << endl;
      cerr << " precip: " << precip << endl;
    }
    
    // fill out station report
    
    station_report_t report;
    memset(&report, 0, sizeof(report));
    
    STRncopy(report.station_label, stid, ST_LABEL_SIZE);
    
    report.msg_id = SENSOR_REPORT;
    report.time = validTime.utime();
    
    report.lat = lat;
    report.lon = lon;
    report.alt = elev;
    report.liquid_accum = precip;

    report.temp = STATION_NAN;
    report.dew_point = STATION_NAN;
    report.relhum = STATION_NAN;
    report.windspd = STATION_NAN;
    report.winddir = STATION_NAN;
    report.windgust = STATION_NAN;
    report.pres = STATION_NAN;
    report.precip_rate = STATION_NAN;
    report.visibility = STATION_NAN;
    report.ceiling = STATION_NAN;
    report.rvr = STATION_NAN;
    
    if (_params.debug >= Params::DEBUG_VERBOSE) {
      cerr << "====== FOUND REPORT ======" << endl;
      print_station_report(stderr, "", &report);
      cerr << "==========================" << endl;
    }
    
    // swap bytes
    
    station_report_to_be(&report);
    
    // add chunk
    
    int stationId = Spdb::hash4CharsToInt32(stid + 1);
    out.addPutChunk(stationId,
		    validTime.utime(),
		    validTime.utime() + _params.expire_seconds,
		    sizeof(report), &report);
    
    nObs++;
    
  } // while

  if (nObs < 0) {
    cerr << "ERROR - CwbAws2Spdb::_processPrecipFile" << endl;
    cerr << "  No valid obs in file: " << file_path << endl;
    return -1;
  }

  // put the data
  
  if (out.put(_params.output_url,
	      SPDB_STATION_REPORT_ID,
	      SPDB_STATION_REPORT_LABEL)) {
    cerr << "ERROR - CwbAws2Spdb::_processPrecipFile" << endl;
    cerr << "  Cannot put station data to: "
	 << _params.output_url << endl;
    cerr << "  " << out.getErrStr() << endl;
    iret = -1;
  }

  if (_params.debug >= Params::DEBUG_VERBOSE) {
    cerr << "Wrote " << nObs << " reports to url: "
	 << _params.output_url << endl;
  }

  if (_params.debug) {
    cerr << "  Done with file: " << file_path << endl;
  }

  return iret;
   
}


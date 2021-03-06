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
// NcfWrapper.hh
//
// NetCDF file wrapper
//
// Mike Dixon, RAP, NCAR
// P.O.Box 3000, Boulder, CO, 80307-3000, USA
//
// Feb 2013
//
///////////////////////////////////////////////////////////////

#ifndef NcfWrapper_HH
#define NcfWrapper_HH

#include <string>
#include <vector>
#include <Ncxx/Ncxx.hh>
#include <Ncxx/NcxxFile.hh>

using namespace std;

///////////////////////////////////////////////////////////////
/// CLASS FOR NETCDF IO OPERATIONS

class NcfWrapper

{
  
public:

  /// Constructor
  
  NcfWrapper();
  
  /// Destructor
  
  virtual ~NcfWrapper();

  //////////////////////////////////////////////////////////////
  /// \name File operations
  //@{

  /// open for reading
  /// Returns 0 on success, -1 on failure
  
  int openRead(const string &path);

  /// open netcdf file for writing
  /// create error object so we can handle errors
  /// set the netcdf format, before a write
  /// format options are:
  ///   Classic - classic format (i.e. version 1 format)
  ///   Offset64Bits - 64-bit offset format
  ///   Netcdf4 - netCDF-4 using HDF5 format
  ///   Netcdf4Classic - netCDF-4 using HDF5 but only netCDF-3 calls
  /// Returns 0 on success, -1 on failure
  
  int openWrite(const string &path, NcxxFile::FileFormat format);

  /// close previously-opened file

  void close();

  //@}

  //////////////////////////////////////////////////////////////
  /// \name Attributes
  //@{
  
  /// add string global attribute
  /// Returns 0 on success, -1 on failure

  int addGlobAttr(const string &name, const string &val);

  /// add int global attribute
  /// Returns 0 on success, -1 on failure
  
  int addGlobAttr(const string &name, int val);

  /// add float global attribute
  /// Returns 0 on success, -1 on failure
  
  int addGlobAttr(const string &name, float val);

  // read a global attribute
  // Returns 0 on success, -1 on failure
  
  int readGlobAttr(const string &name, string &val);
  int readGlobAttr(const string &name, int &val);
  int readGlobAttr(const string &name, float &val);
  int readGlobAttr(const string &name, double &val);

  /// add attribute of various types
  /// Returns 0 on success, -1 on failure

  int addAttr(NcxxVar &var, const string &name, const string &val);
  int addAttr(NcxxVar &var, const string &name, unsigned char val);
  int addAttr(NcxxVar &var, const string &name, short val);
  int addAttr(NcxxVar &var, const string &name, int val);
  int addAttr(NcxxVar &var, const string &name, int64_t val);
  int addAttr(NcxxVar &var, const string &name, float val);
  int addAttr(NcxxVar &var, const string &name, double val);

  //@}

  //////////////////////////////////////////////////////////////
  /// \name dimensions
  //@{
  
  int addDim(NcxxDim &dim, const string &name, int size);
  int readDim(const string &name, NcxxDim &dim);

  //@}

  //////////////////////////////////////////////////////////////
  /// \name variables
  //@{
  
  /// Add scalar var
  /// Returns 0 on success, -1 on failure

  int addVar(NcxxVar &var, const string &name, 
             const string &standardName,
             const string &longName,
             NcxxType ncType, 
             const string &units = "");

  /// Add 1-D array var
  /// Returns 0 on success, -1 on failure

  int addVar(NcxxVar &var, const string &name, 
             const string &standardName,
             const string &longName,
             NcxxType ncType,
             NcxxDim &dim,
             const string &units = "");
  
  /// Add 2-D array var
  /// Returns 0 on success, -1 on failure
  
  int addVar(NcxxVar &var, const string &name, 
             const string &standardName,
             const string &longName,
             NcxxType ncType,
             NcxxDim &dim0, NcxxDim &dim1, 
             const string &units = "");

  /// Add var in multiple-dimensions
  /// Returns 0 on success, -1 on failure
  /// Side effect: var is set

  int addVar(NcxxVar &var, 
             const string &name,
             const string &standardName,
             const string &longName,
             NcxxType ncType,
             vector<NcxxDim> &dims,
             const string &units = "");

  /// get the total number of values in a variable
  /// this is the product of the dimension sizes
  /// and is 1 for a scalar (i.e. no dimensions)

  int64_t numVals(NcxxVar &var);
  
  /////////////////////////////////////////////////////////////

  /// read int variable, set var and val
  /// Returns 0 on success, -1 on failure

  int readIntVar(NcxxVar &var, const string &name,
                 int &val, int missingVal, bool required = true);
  
  /// read int variable, set val
  /// Returns 0 on success, -1 on failure
  
  int readIntVal(const string &name, int &val, 
                 int missingVal, bool required = true);
  
  /// read float variable
  /// Returns 0 on success, -1 on failure
  
  int readFloatVar(NcxxVar &var, const string &name, float &val, 
                   float missingVal, bool required = true);
  
  /// read float value
  /// Returns 0 on success, -1 on failure
  
  int readFloatVal(const string &name, float &val,
                   float missingVal, bool required = true);

  /// read double variable
  /// Returns 0 on success, -1 on failure
  
  int readDoubleVar(NcxxVar &var, const string &name, double &val, 
                    double missingVal, bool required = true);
  
  /// read double value
  /// Returns 0 on success, -1 on failure
  
  int readDoubleVal(const string &name, double &val,
                    double missingVal, bool required = true);

  /// read a scalar string variable
  /// Returns 0 on success, -1 on failure

  int readStringVar(NcxxVar &var, const string &name, string &val);
  
  /// read int32 1D array, set val
  /// Note - unsigned 32-bit values will wrap
  /// 64-bit values are not handled.
  /// Returns 0 on success, -1 on failure
  
  int readInt32Array(const string &name, 
                     vector<int32_t> &array,
                     int32_t missingVal,
                     bool required);

  /// read int64 1D array, set val
  /// Note - unsigned 64-bit values will wrap
  /// Returns 0 on success, -1 on failure
  
  int readInt64Array(const string &name, 
                     vector<int64_t> &array,
                     int64_t missingVal,
                     bool required);

  /// read double 1D array, set val
  /// Returns 0 on success, -1 on failure
  
  int readDoubleArray(const string &name, 
                      vector<double> &array,
                      double missingVal,
                      bool required);
  
  /////////////////////////////////////////////////////////////

  /// write a scalar double variable
  /// Returns 0 on success, -1 on failure

  int writeVar(NcxxVar &var, double val);

  /// write a scalar float variable
  /// Returns 0 on success, -1 on failure
  
  int writeVar(NcxxVar &var, float val);

  /// write a scalar int variable
  /// Returns 0 on success, -1 on failure
  
  int writeVar(NcxxVar &var, int val);

  /// write a 1-D vector variable
  /// number of elements specified in dimension
  /// Returns 0 on success, -1 on failure

  int writeVar(NcxxVar &var, const NcxxDim &dim, const void *data);

  /// write a 1-D vector variable
  /// number of elements specified in arguments
  /// Returns 0 on success, -1 on failure
  
  int writeVar(NcxxVar &var, const NcxxDim &dim, size_t count, 
               const void *data);
  
  /// write a string variable
  /// Returns 0 on success, -1 on failure
  
  int writeStringVar(NcxxVar &var, const void *data);
  
  /// compress a variable

  int compressVar(NcxxVar &var, int compressionLevel);
  
  //@}

  ///////////////////////////////
  /// \name Strings from nc items
  //@{
  
  /// convert type enum to strings

  static string ncTypeToStr(nc_type nctype);

  /// convert var type string

  string varTypeToStr(const NcxxVar &var);
  
  /// get string representation of component

  static string asString(const NcxxAtt *att);
  
  //@}

  ////////////////////////
  /// \name Handles:
  //@{
  
  /// Get the path in use after read or write
  
  string getPathInUse() const { return _pathInUse; }
  
  /// Get the Nc format after a write
  
  NcxxFile::FileFormat getNcFileFormat() const { return _ncFormat; }
  
  /// Get the NcxxFile object
  
  NcxxFile *getNcFile() { return _ncFile; }

  /// Get the NcError object
  
  // NcError *getNcError() { return _err; }

  //@}

  ////////////////////////
  /// \name Error string:
  //@{
  
  /// Clear error string.
  
  void clearErrStr() { _errStr.clear(); }

  /// Get the Error String.
  ///
  /// The contents are only meaningful if an error has returned.
  
  string getErrStr() const { return _errStr; }
  
  //@}

  ////////////////////////
  /// \name Missing values
  //@{
  
  static const double missingDouble;
  static const float missingFloat;
  static const int missingInt;
  static const unsigned char missingUchar;

  //@}

protected:

private:

  void clear();

  // error string

  string _errStr; ///< Error string is set on read or write error
  
  // handles
  
  NcxxFile *_ncFile;
  string _pathInUse;
  NcxxFile::FileFormat _ncFormat;
  
  /// add integer value to error string, with label
  
  void _addErrInt(string label, int iarg,
                  bool cr = true);
  
  /// add double value to error string, with label
  
  void _addErrDbl(string label, double darg,
                  string format, bool cr = true);
  
  /// add string value to error string, with label
  
  void _addErrStr(string label, string strarg = "",
                  bool cr = true);
  
  // set fill value appropriately for the variable type
  
  void _setFillvalue(NcxxVar &var);

};

#endif

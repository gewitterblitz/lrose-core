/**
 * @file FIRFilter.hh
 * @brief  Perform FIR filter on data
 * @class FIRFilter
 * @brief  Perform FIR filter on data
 */

#ifndef FIR_FILTER_H
#define FIR_FILTER_H

#include <string>
#include <vector>

class RayLoopData;
class RadxRay;

class FIRFilter
{
public:
  /**
   * Constructor
   */
  FIRFilter();

  /**
   * Destructor
   */
  ~FIRFilter();
  
  /**
   * Peform FIR filter on inputs, outputs to last arg
   * @param[in] name Name of field to use
   * @param[in] firType  One of several edge handlers
   * @param[in] ray  The ray being processed
   * @param[in] data  Derived data
   * @param[in] output
   * @return true for success
   */
  bool filter(const std::string &name, const std::string &firType,
	      const RadxRay *ray, const std::vector<RayLoopData> &data,
	      RayLoopData *output);
    
private:

  /**
   * The FIR coefficients
   */
  std::vector<double> _coeff;

};

#endif

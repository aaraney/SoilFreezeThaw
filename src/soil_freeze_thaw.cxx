#ifndef SFT_CXX_INCLUDED
#define SFT_CXX_INCLUDED

#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include "../include/soil_freeze_thaw.hxx"


soilfreezethaw::SoilFreezeThaw::
SoilFreezeThaw()
{
  this->endtime    = 10.;
  this->time       = 0.;
  this->shape[0]   = 1;
  this->shape[1]   = 1;
  this->shape[2]   = 1;
  this->spacing[0] = 1.;
  this->spacing[1] = 1.;
  this->origin[0]  = 0.;
  this->origin[1]  = 0.;
  this->dt         = 3600;
  this->latent_heat_fusion      = 0.3336E06;
  this->option_bottom_boundary  = 2;
  this->option_top_boundary     = 2;
  this->ice_fraction_scheme     = " ";
  this->soil_depth              = 0.0;
  this->ice_fraction_schaake    = 0.0;
  this->ice_fraction_xinanjiang = 0.0;
  this->ground_temp             = 273.15;
  this->soil_ice_fraction       = 0.0;
  this->bottom_boundary_temp_const = 275.15;
}

soilfreezethaw::SoilFreezeThaw::
SoilFreezeThaw(std::string config_file)
{
  this->latent_heat_fusion = 0.3336E06;
  
  //this->option_bottom_boundary = 2; // 1: constant temp, 2: zero thermal flux
  //this->option_top_boundary = 2;    // 1: constant temp, 2: from a file

  this->InitFromConfigFile(config_file);
  
  this->shape[0]   = this->ncells;
  this->shape[1]   = 1;
  this->shape[2]   = 1;
  this->spacing[0] = 1.;
  this->spacing[1] = 1.;
  this->origin[0]  = 0.0;
  this->origin[1]  = 0.0;

  this->InitializeArrays();
  SoilCellsThickness(); // get soil cells thickness
  this->ice_fraction_schaake    = 0.0;
  this->ice_fraction_xinanjiang = 0.0;
  this->ground_temp             = 273.15;
  this->time                    = 0.0;
  this->soil_ice_fraction       = 0.0;
  this->energy_balance          = 0.0;
}


void soilfreezethaw::SoilFreezeThaw::
InitializeArrays(void)
{
  this->thermal_conductivity = new double[ncells];
  this->heat_capacity = new double[ncells];
  this->soil_dz = new double[ncells];
  this->soil_ice_content = new double[ncells];
  this->soil_temperature_prev = new double[ncells];
  
  for (int i=0;i<ncells;i++) {
    this->soil_ice_content[i] = this->soil_moisture_content[i] - this->soil_liquid_content[i];

    // at t = 0, current and previous soil temperature states are the same
    this->soil_temperature_prev[i] = this->soil_temperature[i];

    // initialize heat capacity to zero, will be update in the advanced before updating soil T
    this->heat_capacity[i] = 0.0;
  }
}

void soilfreezethaw::SoilFreezeThaw::
InitFromConfigFile(std::string config_file)
{ 
  std::ifstream fp;
  fp.open(config_file);

  if (!fp) {
    std::cerr<<"File \""<<config_file<<"\"does not exist."<<"\n";
    abort();
  }
  int n_st, n_mct, n_mcl;

  this->is_soil_moisture_bmi_set = false;
  bool is_endtime_set = false;
  bool is_dt_set = false;
  bool is_soil_z_set = false;
  bool is_smcmax_set = false;
  bool is_b_set = false;
  bool is_quartz_set = false;
  bool is_satpsi_set = false;
  bool is_soil_temperature_set = false;
  bool is_soil_moisture_content_set = false; // total moisture content
  bool is_soil_liquid_content_set = false;   // liquid moisture content
  bool is_ice_fraction_scheme_set = false;   // ice fraction scheme
  bool is_bottom_boundary_temp_set = false;  // bottom boundary temperature
  bool is_top_boundary_temp_set = false;     // bottom boundary temperature
    
  while (fp) {

    std::string line;
    std::string param_key, param_value, param_unit;

    std::getline(fp, line);

    int loc_eq = line.find("=") + 1;
    int loc_u = line.find("[");
    param_key = line.substr(0,line.find("="));

    bool is_unit = line.find("[") != std::string::npos;

    if (is_unit)
      param_unit = line.substr(loc_u,line.find("]")+1);
    else
      param_unit = "";

    param_value = line.substr(loc_eq,loc_u - loc_eq);
    
    if (param_key == "soil_moisture_bmi") {
      this->is_soil_moisture_bmi_set = true;
      continue;
    }
    else if (param_key == "end_time") {
      this->endtime = std::stod(param_value);

      if (param_unit == "[d]" || param_unit == "[day]") 
	this->endtime *= 86400;
      else if (param_unit == "[s]" || param_unit == "[sec]")
	this->endtime *= 1.0;
      else if (param_unit == "[h]" || param_unit == "[hr]" || param_unit == "") // defalut time unit is hour
	this->endtime *= 3600.0;

      is_endtime_set = true;
      continue;
    }
    else if (param_key == "dt") {
      this->dt = std::stod(param_value);
      if (param_unit == "[d]" || param_unit == "[day]")
	this->dt *= 86400;
      else if (param_unit == "[s]" || param_unit == "[sec]")
	this->dt *= 1.0;
      else if (param_unit == "[h]" || param_unit == "[hr]" || param_unit == "") // defalut time unit is hour
	this->dt *= 3600.0;
      
      is_dt_set = true;
      continue;
    }
    else if (param_key == "soil_z") {
      std::vector<double> vec = ReadVectorData(param_value);
      
      this->soil_z = new double[vec.size()];
      for (unsigned int i=0; i < vec.size(); i++)
	this->soil_z[i] = vec[i];
      this->ncells = vec.size();
      this->soil_depth = this->soil_z[this->ncells-1];
      is_soil_z_set = true;
      continue;
    }
    else if (param_key == "soil_params.smcmax") {
      this->smcmax = std::stod(param_value);
      is_smcmax_set = true;
      continue;
    }
    else if (param_key == "soil_params.b") {
      this->b = std::stod(param_value);
      std::string b_unit = line.substr(loc_u+1,line.length());
      assert (this->b > 0);
      is_b_set = true;
      continue;
    }
    else if (param_key == "soil_params.quartz") {
      this->quartz = std::stod(param_value);
      assert (this->quartz > 0);
      is_quartz_set = true;
      continue;
    }
    else if (param_key == "soil_params.satpsi") {  //Soil saturated matrix potential
      this->satpsi = std::stod(param_value);
      is_satpsi_set = true;
      continue;
    }
    else if (param_key == "soil_temperature") {
      std::vector<double> vec = ReadVectorData(param_value);
      this->soil_temperature = new double[vec.size()];
      for (unsigned int i=0; i < vec.size(); i++)
	this->soil_temperature[i] = vec[i];
      n_st = vec.size();
      
      is_soil_temperature_set = true;
      continue;

    }
    else if (param_key == "soil_moisture_content") {
      std::vector<double> vec = ReadVectorData(param_value);
      this->soil_moisture_content = new double[vec.size()];
      for (unsigned int i=0; i < vec.size(); i++)
	this->soil_moisture_content[i] = vec[i];
      n_mct = vec.size();
      is_soil_moisture_content_set = true;
      continue;
    }
    else if (param_key == "soil_liquid_content") {
      std::vector<double> vec = ReadVectorData(param_value);
      this->soil_liquid_content = new double[vec.size()];
      for (unsigned int i=0; i < vec.size(); i++) {
	//	assert (this->soil_moisture_content[i] >= vec[i]);
	this->soil_liquid_content[i] = vec[i];
      }
      n_mcl = vec.size();
      is_soil_liquid_content_set = true;
      continue;
    }
    else if (param_key == "ice_fraction_scheme") {
      this->ice_fraction_scheme = param_value;
      is_ice_fraction_scheme_set = true;
      continue;
    }
    else if (param_key == "bottom_boundary_temp") {
      this->bottom_boundary_temp_const = stod(param_value);
      is_bottom_boundary_temp_set = true;
      continue;
    }
    else if (param_key == "top_boundary_temp") {
      this->top_boundary_temp_const = stod(param_value);
      is_top_boundary_temp_set = true;
      continue;
    }
    else if (param_key == "verbosity") {
      if (param_value == "high" || param_value == "low")
	this->verbosity = param_value;
      else
	this->verbosity = "none";
      continue;
    }
  }
  
  fp.close();
  
  // simply allocate space for soil_liquid_content and soil_moisture_content arrays, as they will be set through CFE_BMI
  if (this->is_soil_moisture_bmi_set && is_soil_z_set) {
    this->soil_moisture_content = new double[this->ncells]();
    this->soil_liquid_content = new double[this->ncells]();
    n_mct = this->ncells;
    n_mcl = this->ncells;
    is_soil_moisture_content_set = true;

  }

  if (!is_endtime_set) {
    std::cout<<"Config file: "<<this->config_file<<"\n";
    throw std::runtime_error("End time not set in the config file!");
  }

  if (!is_dt_set) {
    std::cout<<"Config file: "<<this->config_file<<"\n";
    throw std::runtime_error("Time step (dt) not set in the config file!");
  }
  if (!is_soil_z_set) {
    std::cout<<"Config file: "<<this->config_file<<"\n";
    throw std::runtime_error("soil_z not set in the config file!");
  }
  if (!is_smcmax_set) {
    std::cout<<"Config file: "<<this->config_file<<"\n";
    throw std::runtime_error("smcmax not set in the config file!");
  }
  if (!is_b_set) {
    std::cout<<"Config file: "<<this->config_file<<"\n";
    throw std::runtime_error("b (Clapp-Hornberger's parameter) not set in the config file!");
  }
  if (!is_quartz_set) {
    std::cout<<"Config file: "<<this->config_file<<"\n";
    throw std::runtime_error("quartz (soil parameter) not set in the config file!");
  }
  if (!is_satpsi_set) {
    std::cout<<"Config file: "<<this->config_file<<"\n";
    throw std::runtime_error("satpsi not set in the config file!");
  }
  if (!is_soil_temperature_set) {
    std::cout<<"Config file: "<<this->config_file<<"\n";
    throw std::runtime_error("Soil temperature not set in the config file!");
  }
  if (!is_soil_moisture_content_set && !this->is_soil_moisture_bmi_set) {
    std::cout<<"Config file: "<<this->config_file<<"\n";
    throw std::runtime_error("Total soil moisture content not set in the config file!");
  }
  if (!is_soil_liquid_content_set && !this->is_soil_moisture_bmi_set) {
    std::cout<<"Config file: "<<this->config_file<<"\n";
    throw std::runtime_error("Liquid soil moisture content not set in the config file!");
  }
  if (!is_ice_fraction_scheme_set) {
    std::cout<<"Config file: "<<this->config_file<<"\n";
    throw std::runtime_error("Ice fraction scheme not set in the config file!");
  }

  this->option_bottom_boundary = is_bottom_boundary_temp_set == true ? 1 : 2; // if false zero geothermal flux is the BC

  this->option_top_boundary = is_top_boundary_temp_set == true ? 1 : 2; // 1: constant temp, 2: from a file

  // check if the size of the input data is consistent
  assert (n_st == this->ncells);
  assert (n_mct == this->ncells);
  assert (n_mcl == this->ncells);
}

/*
  Reads soil discretization, soil moisture, soil temperature from the config file
  Note: soil moisture are not read from the config file when the model is coupled to SoilMoistureProfiles modules
*/
std::vector<double> soilfreezethaw::SoilFreezeThaw::
ReadVectorData(std::string key)
{
  int pos =0;
  std::string delimiter = ",";
  std::vector<double> value(0.0);
  std::string z1 = key;
  
  if (z1.find(delimiter) == std::string::npos) {
    double v = stod(z1);
    if (v == 0.0) {
      std::stringstream errMsg;
      errMsg << "soil_z (depth of soil reservior) should be greater than zero. It it set to "<< v << " in the config file "<< "\n";
      throw std::runtime_error(errMsg.str());
    }
    
    value.push_back(v);
    
  }
  else {
    while (z1.find(delimiter) != std::string::npos) {
      pos = z1.find(delimiter);
      std::string z_v = z1.substr(0, pos);

      value.push_back(stod(z_v.c_str()));
      
      z1.erase(0, pos + delimiter.length());
      if (z1.find(delimiter) == std::string::npos)
	value.push_back(stod(z1));
    }
  }
  
  return value;
}

/*
  Computes surface runoff scheme based ice fraction
  These scheme are consistent with the schemes in the NOAH-MP (used in the current NWM)
  - Schaake scheme computes volume of frozen water in meters
  - Xinanjiang uses exponential based ice fraction taking only ice from the top cell
  - Note: ice fraction is not used by the freeze-thaw model, it is a bmi output to rainfall-runoff models
*/
void soilfreezethaw::SoilFreezeThaw::
ComputeIceFraction()
{
  
  this->ice_fraction_schaake    = 0.0; // set it to zero
  this->ice_fraction_xinanjiang = 0.0;
  this->soil_ice_fraction       = 0.0;
  
  if (this->ice_fraction_scheme == "Schaake") {
    this->ice_fraction_scheme_bmi = 1;
  }
  else if (this->ice_fraction_scheme == "Xinanjiang") {
    this->ice_fraction_scheme_bmi = 2;
  }
  
  if (this->ice_fraction_scheme_bmi == SurfaceRunoffScheme::Schaake) {
    double val = 0.0;
    for (int i =0; i < ncells; i++) {
      val += this->soil_ice_content[i] * this->soil_dz[i];
    }
    assert (this->ice_fraction_schaake <= this->soil_depth);
    this->ice_fraction_schaake = val;
  }
  else if (this->ice_fraction_scheme_bmi == SurfaceRunoffScheme::Xinanjiang) {
    double fice = std::min(1.0, this->soil_ice_content[0]/this->smcmax);
    double A = 4.0; // taken from NWM SOILWATER subroutine
    double fcr = std::max(0.0, std::exp(-A*(1.0-fice)) - std::exp(-A)) / (1.0 - std::exp(-A));
    this->ice_fraction_xinanjiang = fcr;
  }
  else {
    throw std::runtime_error("Ice Fraction Scheme not specified either in the config file nor set by CFE BMI. Options: Schaake or Xinanjiang!");
  }
  
  // compute soil ice fraction (the fraction of soil moisture that is ice)
  double ice_v = 0.0;
  double moisture_v = 0.0;
  
  for (int i=0; i < ncells; i++) {
    moisture_v += this->soil_moisture_content[i] * this->soil_dz[i]; 
    ice_v += this->soil_ice_content[i] * this->soil_dz[i];
  }

  //moisture_v = moisture_v > 0 ? moisture_v : 1E-6;
  if (moisture_v > 0 && ice_v > 1E-6)
    this->soil_ice_fraction = ice_v/moisture_v;
}
  
double soilfreezethaw::SoilFreezeThaw::
GetDt()
{
  return this->dt;
}


/*
  Advance the timestep of the soil freeze thaw model called by BMI Update
  
*/
void soilfreezethaw::SoilFreezeThaw::
Advance()
{
  // before advancing the time, store the current state 
  for (int i=0; i<this->ncells;i++) {
      this->soil_temperature_prev[i] = this->soil_temperature[i];
  }
  
  /* BMI sets (total) soil moisture content only, so we update the liquid content based
     on the previous ice content; initially ice_content is zero; assuming we are starting
     somewhere in the summer/fall
  */
  
  if (this->is_soil_moisture_bmi_set) {
    for (int i=0; i<this->ncells;i++) {
      this->soil_liquid_content[i] = std::max(this->soil_moisture_content[i] - this->soil_ice_content[i], 0.0);
      //this->soil_ice_content[i] = std::max(this->soil_ice_content[i], 0.0); // make sure ice_content is non-negative
    }
  }
  
  /* Update Thermal conductivities due to update in the soil moisture */
  ThermalConductivity(); // initialize thermal conductivities

  /* Update volumetric heat capacity */
  SoilHeatCapacity();

  /* Solve the diffusion equation to get updated soil temperatures */
  SolveDiffusionEquation();

  /* Now time to update ice content based on the new soil moisture and and
     soil temperature profiles.
     Call Phase Change module to partition soil moisture into water and ice.
  */
  PhaseChange();

  this->time += this->dt;

  ComputeIceFraction();

  if (verbosity.compare("high") == 0) {
    for (int i=0;i<ncells;i++)
      std::cerr<<"Soil Temp (previous, current) = "<<this->soil_temperature_prev[i]<<", "<<this->soil_temperature[i]<<"\n";

    for (int i=0;i<ncells;i++)
      std::cerr<<"Soil moisture (total, water, ice) = "<<this->soil_moisture_content[i]<<", "<<this->soil_liquid_content[i]<<", "<<this->soil_ice_content[i]<<"\n";
  }

  EnergyBalanceCheck();

  /* getting temperature below 200 would mean the space resolution is too
     fine and time resolution is too coarse */
  //assert (this->soil_temperature[0] > 200.0); 
}

/*
  Module returns updated ground heat flux used in surface boundary condition in
  the diffusion equation
  Option 1 : prescribed (user-defined) constant surface/ground temperature
  Option 2 : dynamic surface/ground temperature (user-provided or provided by a coupled model)
*/
double soilfreezethaw::SoilFreezeThaw::
GroundHeatFlux(double soil_temp)
{
  double surface_temp = 0.0; // ground surface temnperature
  
  if (option_top_boundary == 1) {
    surface_temp = this->top_boundary_temp_const; // temperature specified as constant
  }
  else if (option_top_boundary == 2) {
    surface_temp = this->ground_temp;       // temperature from a file/coupling
  }
  else {
    throw std::runtime_error("Ground heat flux: option for top boundary should be 1 (constant temperature) or 2 (temperature from file/coupling)!");
    return 0;
  }

  assert (this->soil_z[0] >0);
  double ground_heat_flux_loc = - thermal_conductivity[0] * (soil_temp  - surface_temp) / (0.5*soil_z[0]); // half of top cell thickness
  
  return ground_heat_flux_loc;
}

/*
  See README.md for a detailed description of the model
  Solves a 1D diffusion equation with variable thermal conductivity
  Discretizad through an implicit Crank-Nicolson scheme
  A, B, C are the coefficients on the left handside
  X is the solution of the system at the current timestep
*/
void soilfreezethaw::SoilFreezeThaw::
SolveDiffusionEquation()
{
    // local 1D vectors
    std::vector<double> thermal_flux(ncells);
    std::vector<double> AI(ncells);
    std::vector<double> BI(ncells);
    std::vector<double> CI(ncells);
    std::vector<double> RHS(ncells);
    std::vector<double> lambda(ncells);
    std::vector<double> denominator(ncells);
    std::vector<double> X(ncells);
    std::vector<double> dsoilT_dz(ncells);
    double bottomflux = 0.0;
    double h1 = 0.0, h2 = 0.0;
    
    // compute matrix coefficient using Crank-Nicolson discretization scheme
    // first compute thermal fluxes and later multiplied by lambda [=dt/(heat_capacity * (h_i - h_i-1))]
    
    for (int i=0;i<ncells; i++) {
      if (i == 0) {
	h1 = soil_z[i];
	h2 = soil_z[i+1];
	
	lambda[i] = dt / (h1 * heat_capacity[i]);
	denominator[i] = 2.0/h2;
	
	this->ground_heat_flux = this->GroundHeatFlux(soil_temperature[i]);
	dsoilT_dz[i] = 2.0 * (soil_temperature[i+1] - soil_temperature[i])/ h2;;
	
	thermal_flux[i] = thermal_conductivity[i] * dsoilT_dz[i] + this->ground_heat_flux;
      }
      else if (i < ncells-1) {
	h1 = soil_z[i] - soil_z[i-1];
        h2 = soil_z[i+1] - soil_z[i-1];

	lambda[i] = dt/(h1 * heat_capacity[i]);
	denominator[i] = 2.0/h2;

	dsoilT_dz[i] = 2.0 * (soil_temperature[i+1] - soil_temperature[i])/ h2;

	thermal_flux[i] = thermal_conductivity[i] * dsoilT_dz[i] - thermal_conductivity[i-1] * dsoilT_dz[i-1];
      }
      else if (i == ncells-1) {
	h1 = soil_z[i] - soil_z[i-1];
	
	lambda[i] = dt/(h1 * heat_capacity[i]);
	
	if (this->option_bottom_boundary == 1) {
	  double dzdt = 2 * (soil_temperature[i] - bottom_boundary_temp_const) / h1;
	  /* dT_dz = (T_bottom - T_i)/ (dz/2), note the next term uses `-dtdz1`
	     just to be consistent with the definition of geothermnal flux */
	  
	  bottomflux = - thermal_conductivity[i] * dzdt;
	}
	else if (this->option_bottom_boundary == 2) {
	  bottomflux = 0.;
	}
	
	thermal_flux[i] = bottomflux - thermal_conductivity[i-1] * dsoilT_dz[i-1];

	this->bottom_heat_flux = bottomflux;
      }
      
    }

    // put coefficients in the corresponding vectors A,B,C, and RHS
    for (int i=0; i<ncells;i++) {
      if (i == 0) {
	AI[i] = 0;
	CI[i] = -lambda[i] * thermal_conductivity[i] * denominator[i];
	BI[i] = 1 - CI[i];
      }
      else if (i < ncells-1) {
	AI[i] = -lambda[i] * thermal_conductivity[i-1] * denominator[i-1];
	CI[i] = -lambda[i] * thermal_conductivity[i] * denominator[i];
	BI[i] = 1 - AI[i] - CI[i];
      }
      else if (i == ncells-1) { 
	AI[i] = -lambda[i] * thermal_conductivity[i-1] * denominator[i-1];
	CI[i] = 0;
	BI[i] = 1 - AI[i];
      }
      RHS[i] = lambda[i] * thermal_flux[i];
    }

    SolverTDMA(AI, BI, CI, RHS, X);

    // Update soil temperature
    for (int i=0;i<ncells;i++)
      this->soil_temperature[i] += X[i];

}

//*****************************************************************************
// Solve the tri-diagonal system using the Thomas Algorithm (TDMA)            *
//     a_i X_i-1 + b_i X_i + c_i X_i+1 = d_i,     i = 0, n - 1                *
//                                                                            *
// Effectively, this is an n x n matrix equation.                             *
// a[i], b[i], c[i] are non-zero diagonals of the matrix and d[i] is the rhs. *
// a[0] and c[n-1] aren't used.                                               *
// X is the solution of the n x n system                                      *
//*****************************************************************************
bool soilfreezethaw::SoilFreezeThaw::
SolverTDMA(const vector<double> &a, const vector<double> &b, const vector<double> &c, const vector<double> &d, vector<double> &X ) {
   int n = d.size();
   vector<double> P( n, 0 );
   vector<double> Q( n, 0 );
   X = P;
   
   // Forward pass
   double denominator = b[0];

   P[0] = -c[0]/denominator;
   Q[0] =  d[0]/denominator;

   for (int i = 1; i < n; i++) {
     denominator = b[i] + a[i] * P[i-1];

     if ( std::abs(denominator) < 1e-20 ) return false;
     
     P[i] =  -c[i]/denominator;
     Q[i] = (d[i] - a[i] * Q[i-1])/denominator;
   }
   
   // Backward substiution
   X[n-1] = Q[n-1];
   for (int i = n - 2; i >= 0; i--)
     X[i] = P[i] * X[i+1] + Q[i];
   
   return true;
}

/*
  Computes bulk soil thermal conductivity
  thermal conductivity model follows the parameterization of Peters-Lidars 
*/
void soilfreezethaw::SoilFreezeThaw::
ThermalConductivity() {
  Properties prop;
  const int nz = this->shape[0];

  double tcmineral = this->quartz > 0.2 ? 2.0 : 3.0; //thermal_conductivity of other mineral
  double tcquartz = 7.7;   // thermal_conductivity of Quartz [W/(mK)] 
  double tcwater  = 0.57;  // thermal_conductivity of water  [W/(mK)] 
  double tcice    = 2.2;   // thermal conductiviyt of ice    [W/(mK)] 
  
  for (int i=0; i<nz;i++) {
    
    double sat_ratio = soil_moisture_content[i]/ this->smcmax;

    //thermal_conductivity of solids Eq. (10) Peters-Lidard
    double tc_solid = pow(tcquartz,this->quartz) * pow(tcmineral, (1. - this->quartz));

    /******** SATURATED THERMAL CONDUCTIVITY *********/
    
    //UNFROZEN VOLUME FOR SATURATION (POROSITY*XUNFROZ)
    double x_unfrozen= 1.0; //prevents zero division
    if (this->soil_moisture_content[i] > 0)
      x_unfrozen = this->soil_liquid_content[i] / this->soil_moisture_content[i]; // (phi * Sliq) / (phi * sliq + phi * sice) = sliq/(sliq+sice) 
    
    double xu = x_unfrozen * this->smcmax; // unfrozen volume fraction
    double tc_sat = pow(tc_solid,(1. - this->smcmax)) * pow(tcice, (this->smcmax - xu)) * pow(tcwater,xu);
    
    /******** DRY THERMAL CONDUCTIVITY ************/
    
    double gammd = (1. - this->smcmax)*2700.; // dry density
    double tc_dry = (0.135* gammd+ 64.7)/ (2700. - 0.947* gammd);
    
    // Kersten Number
    
    double KN;
    if ( (soil_liquid_content[i] + 0.0005) < soil_moisture_content[i])
      KN = sat_ratio; // for frozen soil
    else {
      if (sat_ratio > 0.1)
	KN = log10(sat_ratio) + 1.;
      else if (sat_ratio > 0.05)
	KN = 0.7 * log10(sat_ratio) + 1.;
      else
	KN = 0.0;
    }
    
    // Thermal conductivity
    thermal_conductivity[i] = KN * (tc_sat - tc_dry) + tc_dry;
    
  }
}

/*
  The effective volumetric heat capacity is calculated based on the respective fraction of each component (water, ice, air, and rock):
*/
void soilfreezethaw::SoilFreezeThaw::
SoilHeatCapacity() {
  Properties prop;
  const int nz = this->shape[0];
  
  for (int i=0; i<nz;i++) {
    double sice = soil_moisture_content[i] - soil_liquid_content[i];
    heat_capacity[i] = soil_liquid_content[i]*prop.hcwater_ + sice*prop.hcice_ + (1.0-this->smcmax)*prop.hcsoil_ + (this->smcmax-soil_moisture_content[i])*prop.hcair_;
  }

}

void soilfreezethaw::SoilFreezeThaw::
SoilCellsThickness() {
  const int nz = this->shape[0];

  soil_dz[0] = soil_z[0];
  for (int i=0; i<nz-1;i++) {
    soil_dz[i+1] = soil_z[i+1] - soil_z[i];
  }
}


/*
  See README.md for a detailed description of the model
  The phase change module partition soil moisture into water and ice based on freezing-point depression formulation
  The freezing-point depression equation gives the maximum amount of liquid water (unfrozen soil moisture content) that can exist below the subfreezing temperature
  Here we have used Clap-Hornberger soil moisture function to compute the unfrozen soil moisture content
*/
void soilfreezethaw::SoilFreezeThaw::
PhaseChange() {
  
  Properties prop;
  const int nz = this->shape[0];
  double *Supercool = new double[nz];    // supercooled water in soil [kg/m2]
  double *MassIce_L = new double[nz];    // soil ice mass [kg/m2]
  double *MassLiq_L = new double[nz];    // snow/soil liquid mass [kg/m2]
  double *HeatEnergy_L = new double[nz]();      // energy residual [w/m2] HM = HeatEnergy_L
  double *MassPhaseChange_L = new double[nz];        // melting or freezing water [kg/m2] XM_L = mass of phase change

  // arrays keep local copies of the data at the previous timestep
  double *soil_moisture_content_c = new double[nz];
  double *MassLiq_c = new double[nz]; 
  double *MassIce_c = new double[nz];

  int *IndexMelt = new int[nz]; // tracking melting/freezing index of layers

  this->energy_consumed = 0.0;
  //compute mass of liquid/ice in soil layers in mm
  for (int i=0; i<nz;i++) {
      MassIce_L[i] = (soil_moisture_content[i] - soil_liquid_content[i]) * soil_dz[i] * prop.wdensity_; // [kg/m2]
      MassLiq_L[i] = soil_liquid_content[i] * soil_dz[i] * prop.wdensity_;
  }
  //set local variables
  
  //create copies of the current Mice and MLiq
  memcpy(MassLiq_c, MassLiq_L, sizeof (double) * nz);
  memcpy(MassIce_c, MassIce_L, sizeof (double) * nz);

  //Phase change between ice and liquid water
  for (int i=0; i<nz;i++) {
    IndexMelt[i] = 0;
    soil_moisture_content_c[i] = MassIce_L[i] + MassLiq_L[i];
  }

  /*------------------------------------------------------------------- */
  //Soil water potential
  // SUPERCOOL is the maximum liquid water that can exist below (T - TFRZ) freezing point
  double lam = -1./(this->b);
  for (int i=0; i<nz;i++) {
    if (soil_temperature[i] < prop.tfrez_) {
      double smp = latent_heat_fusion /(prop.grav_*soil_temperature[i]) * (prop.tfrez_ - soil_temperature[i]);     // [m] Soil Matrix potential
      Supercool[i] = this->smcmax* pow((smp/this->satpsi), lam); // SMCMAX = porsity
      Supercool[i] = Supercool[i]*soil_dz[i]* prop.wdensity_;    // [kg/m2]
    }
  }


  /*------------------------------------------------------------------- */
  // ****** get layer freezing/melting index ************
  for (int i=0; i<nz;i++) {
    if (MassIce_L[i] > 0 && soil_temperature[i] > prop.tfrez_) //Melting condition
      IndexMelt[i] = 1;
    else if (MassLiq_L[i] > Supercool[i] && soil_temperature[i] <= prop.tfrez_)// freezing condition in NoahMP
      IndexMelt[i] = 2;
  }
  
  /*------------------------------------------------------------------- */
  // ****** get excess or deficit of energy during phase change (use Hm) ********
  //  HC = volumetic heat capacity [J/m3/K]
  // Heat Energy = (T- Tref) * HC * DZ /Dt = K * J/(m3 * K) * m * 1/s = (J/s)*m/m3 = W/m2
  //if HeatEnergy < 0 --> freezing energy otherwise melting energy
  
  for (int i=0; i<nz;i++) {
    
    if (IndexMelt[i] > 0) {
      HeatEnergy_L[i] = (soil_temperature[i] - prop.tfrez_) * (heat_capacity[i] * soil_dz[i]) / dt; // q = m * c * delta_T
      soil_temperature[i] = prop.tfrez_; // Note the temperature does not go below 0 until there is mixture of water and ice

      this->energy_consumed += HeatEnergy_L[i]; // track total energy used/lost during the phase change (for energy balance check)
    }
    
    if (IndexMelt[i] == 1 && HeatEnergy_L[i] <0) {
      HeatEnergy_L[i] = 0;
      IndexMelt[i] = 0;
    }
    
    if (IndexMelt[i] == 2 && HeatEnergy_L[i] > 0) {
      HeatEnergy_L[i] = 0;
      IndexMelt[i] = 0;
    }
    
    /* compute the amount of melting or freezing water [kg/m2]. That is, how much water needs to be melted
       or freezed for the given energy change: MPC = MassPhaseChange */
    MassPhaseChange_L[i] = HeatEnergy_L[i] * dt / latent_heat_fusion;
  }

  
  /*------------------------------------------------------------------- */
  // The rate of melting and freezing for snow and soil
  // mass partition between ice and water and the corresponding adjustment for the next timestep
  for (int i=0; i<nz;i++) {
    if (IndexMelt[i] >0 && std::abs(HeatEnergy_L[i]) >0) {
      if (MassPhaseChange_L[i] >0)      //melting
	MassIce_L[i] = std::max(0., MassIce_c[i]-MassPhaseChange_L[i]);
      else if (MassPhaseChange_L[i] <0) { //freezing
	if (soil_moisture_content_c[i] < Supercool[i])
	  MassIce_L[i] = 0;
	else {
	  MassIce_L[i] = std::min(soil_moisture_content_c[i] - Supercool[i], MassIce_c[i] - MassPhaseChange_L[i]);
	  MassIce_L[i] = std::max(MassIce_L[i],0.0);
	}
      }
    
      // compute heat residual
      // total energy available - energy consumed by phase change (ice_old - ice_new). The residual becomes sensible heat
      double HEATR = HeatEnergy_L[i] - latent_heat_fusion * (MassIce_c[i]-MassIce_L[i]) / dt; // [W/m2] Energy Residual, last part is the energy due to change in ice mass
      MassLiq_L[i] = std::max(0.,soil_moisture_content_c[i] - MassIce_L[i]);

      // Temperature correction
      this->energy_consumed -= HEATR;
      if (std::abs(HEATR)>0) {
	double f = dt/(heat_capacity[i] * soil_dz[i]);       // [m2 K/W]
	soil_temperature[i] = soil_temperature[i] + f * HEATR; /* [K] , this is computed from HeatMass = (T_n+1-T_n) * Heat_capacity * DZ/ DT
								  convert sensible heat to temperature and add to the soil temp. */
      }

	       
    }
  }
  
  for (int i=0; i<nz;i++) {
    soil_liquid_content[i] =  MassLiq_L[i] / (prop.wdensity_ * soil_dz[i]);                   // [-]
    soil_moisture_content[i]  = (MassLiq_L[i] + MassIce_L[i]) / (prop.wdensity_ * soil_dz[i]); // [-]
    soil_ice_content[i] = std::max(soil_moisture_content[i] - soil_liquid_content[i],0.);
  }
}


/*
  Module computes the energy balance (locally and globally)
  will throw an error if energy balance is not satisfied with in
  the error tolerance
  @param ground_heat_flux [W/m2] is the energy at the top of the ground surface
  @param bottom_heat_flux [W/m2] is the energy (leaving the soil) through the bottom of the domain
*/

void soilfreezethaw::SoilFreezeThaw::
EnergyBalanceCheck()
{
  double net_flux = this->ground_heat_flux + this->bottom_heat_flux;
  
  double energy_current  = 0.0;
  double energy_previous = 0.0;
  double energy_residual = 0.0;
  double tolerance       = 1.0E-4;
  double Tref            = 273.15; // reference temperature [K]
    
  for (int i=0;i<ncells; i++) {
    //energy_temp += heat_capacity[i] * (soil_temperature[i] - soil_temperature_prev[i]) * soil_dz[i] / dt; // W/m^2
    energy_previous += heat_capacity[i] * (soil_temperature_prev[i] - Tref) * soil_dz[i] / dt; // W/m^2
    energy_current  += heat_capacity[i] * (soil_temperature[i] - Tref) * soil_dz[i] / dt;       // W/m^2
  }
  
  energy_residual = energy_current - energy_previous;

  double energy_balance_timestep = (energy_residual + this->energy_consumed) - net_flux;

  this->energy_balance += energy_balance_timestep;
  
  if (verbosity.compare("high") == 0 || fabs(energy_balance) >  tolerance) {
    
    printf("Energy (previous timestep)     [W/m^2] = %6.6f \n", energy_previous);
    printf("Energy (current timestep)      [W/m^2] = %6.6f \n", energy_current);
    printf("Energy gain (+) or loss (-)    [W/m^2] = %6.6f \n", (energy_current - energy_previous));
    printf("Surface flux (in (+), out (-)) [W/m^2] = %6.6f \n", this->ground_heat_flux);
    printf("Bottom flux  (in (+), out (-)) [W/m^2] = %6.6f \n", this->bottom_heat_flux);
    printf("Netflux (in (+) or out (-))    [W/m^2] = %6.6f \n", net_flux);
    printf("Energy (phase change)          [W/m^2] = %6.6f \n", this->energy_consumed);
    printf("Energy balance error (local)   [W/m^2] = %6.4e \n", energy_balance_timestep);
    printf("Energy lalance error (global)  [W/m^2] = %6.4e \n", energy_balance);

    if (fabs(energy_balance) > tolerance)
      throw std::runtime_error("Soil energy balance error...");
  }
  
  
}


/*
  class containing some of the static variables used by several modules
*/
soilfreezethaw::Properties::
Properties() :
  hcwater_ (4.188E06),
  hcice_   (2.094E06), 
  hcair_   (1004.64),
  hcsoil_  (2.00E+6),
  grav_    (9.86),
  tfrez_     (273.15),
  wdensity_ (1000.)
{}

soilfreezethaw::SoilFreezeThaw::
~SoilFreezeThaw()
{}

#endif

#include "Copyright.h"
#ifndef GPU



#include "GAMER.h"

#ifdef UNSPLIT_GRAVITY
#include "CUPOT.h"
extern double ExtPot_AuxArray[EXT_POT_NAUX_MAX];
extern double ExtAcc_AuxArray[EXT_ACC_NAUX_MAX];
#endif

#if   ( MODEL == HYDRO )
#if   ( FLU_SCHEME == RTVD )
void CPU_FluidSolver_RTVD( real Flu_Array_In [][5][ FLU_NXT*FLU_NXT*FLU_NXT ], 
                           real Flu_Array_Out[][5][ PS2*PS2*PS2 ], 
                           real Flux_Array[][9][5][ PS2*PS2 ], 
                           const double Corner_Array[][3],
                           const real Pot_Array_USG[][USG_NXT_F][USG_NXT_F][USG_NXT_F],
                           const int NPatchGroup, const real dt, const real dh, const real Gamma,
                           const bool StoreFlux, const bool XYZ );
#elif ( FLU_SCHEME == WAF )
void CPU_FluidSolver_WAF( real Flu_Array_In [][5][ FLU_NXT*FLU_NXT*FLU_NXT ], 
                          real Flu_Array_Out[][5][ PS2*PS2*PS2 ], 
                          real Flux_Array[][9][5][ PS2*PS2 ], 
                          const double Corner_Array[][3],
                          const real Pot_Array_USG[][USG_NXT_F][USG_NXT_F][USG_NXT_F],
                          const int NPatchGroup, const real dt, const real dh, const real Gamma, 
                          const bool StoreFlux, const bool XYZ, const WAF_Limiter_t WAF_Limiter );
#elif ( FLU_SCHEME == MHM  ||  FLU_SCHEME == MHM_RP )
void CPU_FluidSolver_MHM( const real Flu_Array_In[][5][ FLU_NXT*FLU_NXT*FLU_NXT ], 
                          real Flu_Array_Out     [][5][ PS2*PS2*PS2 ], 
                          real Flux_Array     [][9][5][ PS2*PS2 ], 
                          const double Corner_Array[][3],
                          const real Pot_Array_USG[][USG_NXT_F][USG_NXT_F][USG_NXT_F],
                          const int NPatchGroup, const real dt, const real dh, const real Gamma, 
                          const bool StoreFlux, const LR_Limiter_t LR_Limiter, const real MinMod_Coeff, 
                          const real EP_Coeff, const double Time, const OptGravityType_t GravityType,
                          const double ExtAcc_AuxArray[] );
#elif ( FLU_SCHEME == CTU )
void CPU_FluidSolver_CTU( const real Flu_Array_In[][5][ FLU_NXT*FLU_NXT*FLU_NXT ], 
                          real Flu_Array_Out     [][5][ PS2*PS2*PS2 ], 
                          real Flux_Array     [][9][5][ PS2*PS2 ], 
                          const double Corner_Array[][3],
                          const real Pot_Array_USG[][USG_NXT_F][USG_NXT_F][USG_NXT_F],
                          const int NPatchGroup, const real dt, const real dh, const real Gamma, 
                          const bool StoreFlux, const LR_Limiter_t LR_Limiter, const real MinMod_Coeff, 
                          const real EP_Coeff, const double Time, const OptGravityType_t GravityType,
                          const double ExtAcc_AuxArray[] );
#endif // FLU_SCHEME

#elif ( MODEL == MHD )
#warning : WAIT MHD !!!

#elif ( MODEL == ELBDM )
void CPU_ELBDMSolver( real Flu_Array_In [][FLU_NIN ][ FLU_NXT*FLU_NXT*FLU_NXT ], 
                      real Flu_Array_Out[][FLU_NOUT][ PS2*PS2*PS2 ], 
                      real Flux_Array[][9][NFLUX][ PS2*PS2 ], 
                      const int NPatchGroup, const real dt, const real dh, const real Eta, const bool StoreFlux,
                      const real Taylor3_Coeff, const bool XYZ );

#else 
#error : ERROR : unsupported MODEL !!
#endif // MODEL




//-------------------------------------------------------------------------------------------------------
// Function    :  CPU_FluidSolver
// Description :  1. MODEL == HYDRO : use CPU to solve the Euler equations by different schemes
//                2. MODEL == ELBDM : use CPU to solve the kinematic operator in the Schrodinger's equation
//
// Note        :  Currently five hydro schemes are supported in HYDRO : 
//                   1. Relaxing TVD scheme                            (RTVD  ) -->   split
//                   2. Weighted-Average-Flux scheme                   (WAF   ) -->   split
//                   3. MUSCL-Hancock scheme                           (MHM   ) --> unsplit
//                   4. MUSCL-Hancock scheme with Riemann prediction   (MHM_RP) --> unsplit
//                   5. Corner-Transport-Upwind scheme                 (CTU   ) --> unsplit
//
//
// Parameter   :  h_Flu_Array_In       : Host array storing the input fluid variables
//                h_Flu_Array_Out      : Host array to store the output variables
//                h_Flux_Array         : Host array to store the output fluxes (useful only if StoreFlux == true)
//                h_Corner_Array       : Host array storing the physical corner coordinates of each patch group
//                h_MinDtInfo_Array    : Host array to store the minimum time-step information in each patch group
//                                       --> useful only if "GetMinDtInfo == true"
//                                       --> NOT supported yet
//                h_Pot_Array_USG      : Host array storing the input potential for UNSPLIT_GRAVITY                       
//                NPatchGroup          : Number of patch groups to be evaluated
//                dt                   : Time interval to advance solution
//                dh                   : Grid size
//                Gamma                : Ratio of specific heats
//                StoreFlux            : true --> store the coarse-fine fluxes
//                XYZ                  : true   : x->y->z ( forward sweep)
//                                       false1 : z->y->x (backward sweep)
//                                       --> only useful for the RTVD and WAF schemes
//                LR_Limiter           : Slope limiter for the data reconstruction in the MHM/MHM_RP/CTU schemes
//                                       (0/1/2/3/4) = (vanLeer/generalized MinMod/vanAlbada/
//                                                      vanLeer + generalized MinMod/extrema-preserving) limiter
//                MinMod_Coeff         : Coefficient of the generalized MinMod limiter
//                EP_Coeff             : Coefficient of the extrema-preserving limiter
//                WAF_Limiter          : Flux limiter for the WAF scheme
//                                       (0/1/2/3) = (SuperBee/vanLeer/vanAlbada/MinBee)
//                ELBDM_Eta            : Particle mass / Planck constant
//                ELBDM_Taylor3_Coeff  : Coefficient in front of the third term in the Taylor expansion for ELBDM
//                ELBDM_Taylor3_Auto   : true --> Determine ELBDM_Taylor3_Coeff automatically by invoking the 
//                                                function "ELBDM_SetTaylor3Coeff"
//                GetMinDtInfo         : true --> Gather the minimum time-step information (the CFL condition in 
//                                                HYDRO) in each patch group
//                                            --> NOT supported yet
//                Time                 : Current physical time                                     (for UNSPLIT_GRAVITY only)
//                GravityType          : Types of gravity --> self-gravity, external gravity, both (for UNSPLIT_GRAVITY only)
//
// Useless parameters in HYDRO : ELBDM_Eta, ELBDM_Taylor3_Coeff, ELBDM_Taylor3_Auto
// Useless parameters in ELBDM : Gamma, LR_Limiter, MinMod_Coeff, EP_Coeff, WAF_Limiter
//-------------------------------------------------------------------------------------------------------
void CPU_FluidSolver( real h_Flu_Array_In [][FLU_NIN ][ FLU_NXT*FLU_NXT*FLU_NXT ], 
                      real h_Flu_Array_Out[][FLU_NOUT][ PS2*PS2*PS2 ], 
                      real h_Flux_Array[][9][NFLUX   ][ PS2*PS2 ], 
                      const double h_Corner_Array[][3],
                      real h_MinDtInfo_Array[],
                      const real h_Pot_Array_USG[][USG_NXT_F][USG_NXT_F][USG_NXT_F],
                      const int NPatchGroup, const real dt, const real dh, const real Gamma, const bool StoreFlux,
                      const bool XYZ, const LR_Limiter_t LR_Limiter, const real MinMod_Coeff, const real EP_Coeff,
                      const WAF_Limiter_t WAF_Limiter, const real ELBDM_Eta, real ELBDM_Taylor3_Coeff, 
                      const bool ELBDM_Taylor3_Auto, const bool GetMinDtInfo,
                      const double Time, const OptGravityType_t GravityType )
{

// check
#  ifdef GAMER_DEBUG
   if ( StoreFlux  &&  h_Flux_Array == NULL )   Aux_Error( ERROR_INFO, "Flux_Array is not allocated !!\n" );
   
#  ifdef UNSPLIT_GRAVITY
   if (  ( GravityType == GRAVITY_SELF || GravityType == GRAVITY_BOTH )  &&  h_Pot_Array_USG == NULL  )
   Aux_Error( ERROR_INFO, "h_Pot_Array_USG == NULL !!\n" );

   if (  ( GravityType == GRAVITY_EXTERNAL || GravityType == GRAVITY_BOTH )  &&  h_Corner_Array == NULL  )
   Aux_Error( ERROR_INFO, "h_Corner_Array == NULL !!\n" );
#  endif
#  endif

#  ifndef UNSPLIT_GRAVITY
   double ExtPot_AuxArray[1];
   double ExtAcc_AuxArray[1];
#  endif


#  if   ( MODEL == HYDRO )

#     if   ( FLU_SCHEME == RTVD )

      CPU_FluidSolver_RTVD( h_Flu_Array_In, h_Flu_Array_Out, h_Flux_Array, h_Corner_Array, h_Pot_Array_USG,
                            NPatchGroup, dt, dh, Gamma, StoreFlux, XYZ );

#     elif ( FLU_SCHEME == WAF )

      CPU_FluidSolver_WAF ( h_Flu_Array_In, h_Flu_Array_Out, h_Flux_Array, h_Corner_Array, h_Pot_Array_USG,
                            NPatchGroup, dt, dh, Gamma, StoreFlux, XYZ, WAF_Limiter );

#     elif ( FLU_SCHEME == MHM  ||  FLU_SCHEME == MHM_RP )

      CPU_FluidSolver_MHM ( h_Flu_Array_In, h_Flu_Array_Out, h_Flux_Array, h_Corner_Array, h_Pot_Array_USG,
                            NPatchGroup, dt, dh, Gamma, StoreFlux, LR_Limiter, MinMod_Coeff, EP_Coeff, Time,
                            GravityType, ExtAcc_AuxArray );

#     elif ( FLU_SCHEME == CTU )

      CPU_FluidSolver_CTU ( h_Flu_Array_In, h_Flu_Array_Out, h_Flux_Array, h_Corner_Array, h_Pot_Array_USG,
                            NPatchGroup, dt, dh, Gamma, StoreFlux, LR_Limiter, MinMod_Coeff, EP_Coeff, Time,
                            GravityType, ExtAcc_AuxArray );

#     else

#     error : unsupported CPU hydro scheme

#     endif


#  elif ( MODEL == MHD )
#     warning : WAIT MHD !!!


#  elif ( MODEL == ELBDM )
//    evaluate the optimized Taylor expansion coefficient
      if ( ELBDM_Taylor3_Auto )  ELBDM_Taylor3_Coeff = ELBDM_SetTaylor3Coeff( dt, dh, ELBDM_Eta );

      CPU_ELBDMSolver( h_Flu_Array_In, h_Flu_Array_Out, h_Flux_Array, NPatchGroup, dt, dh, ELBDM_Eta, StoreFlux,
                       ELBDM_Taylor3_Coeff, XYZ );

#  else 
#     error : ERROR : unsupported MODEL !!
#  endif // MODEL

} // FUNCTION : CPU_FluidSolver



#endif // #ifndef GPU
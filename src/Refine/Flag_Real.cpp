#include "GAMER.h"

void Flag_Grandson( const int lv, const int PID, const int LocalID );
void Prepare_for_Lohner( const OptLohnerForm_t Form, const real *Var1D, real *Ave1D, real *Slope1D, const int NVar );




//-------------------------------------------------------------------------------------------------------
// Function    :  Flag_Real
// Description :  Flag the real patches at level "lv" according to the given refinement criteria
//
// Note        :  1. Flag operation of the buffer patches is performed by the function "Flag_Buffer"
//                2. In this function, the buffer patches may still be flagged due to non-zero
//                   (FLAG_BUFFER_SIZE, FLAG_BUFFER_SIZE_MAXM1_LV, FLAG_BUFFER_SIZE_MAXM2_LV) and the grandson check
//                3. To add new refinement criteria, please edit the function "Flag_Check"
//                4. Definition of the function "Prepare_for_Lohner" is put in the file "Flag_Lohner"
//
// Parameter   :  lv          : Target refinement level to be flagged
//                UseLBFunc   : Use the load-balance alternative functions for the grandson check and exchanging
//                              the buffer flags (useless if LOAD_BALANCE is off)
//                              --> USELB_YES : use the load-balance alternative functions
//                                  USELB_NO  : do not use the load-balance alternative functions
//-------------------------------------------------------------------------------------------------------
void Flag_Real( const int lv, const UseLBFunc_t UseLBFunc )
{

// check
   if ( lv == NLEVEL-1 )
      Aux_Error( ERROR_INFO, "function <%s> should NOT be applied to the finest level\" !!\n", __FUNCTION__ );


// initialize all flags as false
#  pragma omp parallel for schedule( static )
   for (int PID=0; PID<amr->num[lv]; PID++)  amr->patch[0][lv][PID]->flag = false;


   const int SibID_Array[3][3][3]     = {  { {18, 10, 19}, {14,   4, 16}, {20, 11, 21} },
                                           { { 6,  2,  7}, { 0, 999,  1}, { 8,  3,  9} },
                                           { {22, 12, 23}, {15,   5, 17}, {24, 13, 25} }  };    // sibling indices
   const int  FlagBuf                 = ( lv == MAX_LEVEL-1 ) ? FLAG_BUFFER_SIZE_MAXM1_LV :
                                        ( lv == MAX_LEVEL-2 ) ? FLAG_BUFFER_SIZE_MAXM2_LV :
                                                                FLAG_BUFFER_SIZE;
   const real dv                      = CUBE( amr->dh[lv] );
   const bool IntPhase_No             = false;                 // for invoking "Prepare_PatchData"
   const bool DE_Consistency_No       = false;                 // for invoking "Prepare_PatchData"
   const int  NPG                     = 1;                     // for invoking "Prepare_PatchData"
   const int  Lohner_NGhost           = 2;                     // number of ghost cells for the Lohner error estimator
   const int  Lohner_NCell            = PS1 + 2*Lohner_NGhost; // size of the variable array for Lohner
   const int  Lohner_NAve             = Lohner_NCell - 2;      // size of the average array for Lohner
   const int  Lohner_NSlope           = Lohner_NAve;           // size of the slope array for Lohner
   const IntScheme_t Lohner_IntScheme = INT_MINMOD1D;          // interpolation scheme for Lohner
#  if (  ( MODEL == HYDRO || MODEL == MHD )  &&  defined GRAVITY  )
   const real JeansCoeff              = M_PI*GAMMA/( SQR(FlagTable_Jeans[lv])*NEWTON_G ); // flag if dh^2 > JeansCoeff*Pres/Dens^2
#  else
   const real JeansCoeff              = NULL_REAL;
#  endif
#  ifndef GRAVITY
   const OptPotBC_t OPT__BC_POT       = BC_POT_NONE;
#  endif
#  ifdef PARTICLE
   const bool PredictPos_No           = false;                 // used by Par_MassAssignment
   const bool InitZero_Yes            = true;
   const bool Periodic_No             = false;
   const bool UnitDens_Yes            = true;
   const bool UnitDens_No             = false;
   const bool CheckFarAway_No         = false;
   const bool SibBufPatch_No          = false;
   const bool FaSibBufPatch_No        = false;
   const bool JustCountNPar_Yes       = true;
   const bool JustCountNPar_No        = false;
   const bool TimingSendPar_No        = false;
#  endif

//###NOTE: no refinement is allowed near the simulation boundary if the isolated BC for self-gravity is selected
#  ifdef GRAVITY
   const bool NoRefineNearBoundary    =  (  ( OPT__GRAVITY_TYPE == GRAVITY_SELF || OPT__GRAVITY_TYPE == GRAVITY_BOTH )  &&
                                            OPT__BC_POT == BC_POT_ISOLATED  ) ? true : false;
#  else
   const bool NoRefineNearBoundary    = false;
#  endif
   const int  NoRefineBoundaryRegion  = ( NoRefineNearBoundary ) ? PS1*( 1<<(NLEVEL-lv) )*( (1<<lv)-1 ) : NULL_INT;


// set the variables for the Lohner's error estimator
   int  Lohner_NVar=0, Lohner_TVar=0, Lohner_Stride;
   real MinDens=-1.0, MinPres=-1.0;    // default is to turn off minimum density/pressure checks

#  if   ( MODEL == HYDRO  ||  MODEL == MHD )
   if ( OPT__FLAG_LOHNER_DENS )  {  Lohner_NVar++;   Lohner_TVar |= _DENS;   MinDens = MIN_DENS;  }
   if ( OPT__FLAG_LOHNER_ENGY )  {  Lohner_NVar++;   Lohner_TVar |= _ENGY;                        }
   if ( OPT__FLAG_LOHNER_PRES )  {  Lohner_NVar++;   Lohner_TVar |= _PRES;   MinPres = MIN_PRES;  }
   if ( OPT__FLAG_LOHNER_TEMP )  {  Lohner_NVar++;   Lohner_TVar |= _TEMP;   MinPres = MIN_PRES;  }

#  elif ( MODEL == MHD )
#  warning : WAIT MHD !!!

#  elif ( MODEL == ELBDM )
   if ( OPT__FLAG_LOHNER_DENS )
   {
      Lohner_NVar = 2;
      Lohner_TVar = _REAL | _IMAG;
   }

#  else
#  error : unsupported MODEL !!
#  endif // MODEL

   Lohner_Stride = Lohner_NVar*Lohner_NCell*Lohner_NCell*Lohner_NCell;  // stride of array for one patch


// collect particles to **real** patches at lv
#  ifdef PARTICLE
   if ( OPT__FLAG_NPAR_CELL  ||  OPT__FLAG_PAR_MASS_CELL )
      Par_CollectParticle2OneLevel( lv, PredictPos_No, NULL_REAL, SibBufPatch_No, FaSibBufPatch_No, JustCountNPar_No,
                                    TimingSendPar_No );

// Par_CollectParticle2OneLevel with JustCountNPar_No will set NPar_Copy for each patch as well
// --> so call Par_CollectParticle2OneLevel with JustCountNPar_Yes only when OPT__FLAG_NPAR_CELL == false
   else if ( OPT__FLAG_NPAR_PATCH != 0 )
      Par_CollectParticle2OneLevel( lv, PredictPos_No, NULL_REAL, SibBufPatch_No, FaSibBufPatch_No, JustCountNPar_Yes,
                                    TimingSendPar_No );
#  endif


//###ISSUE: use atomic ??
#  pragma omp parallel
   {
      const real (*Fluid)[PS1][PS1][PS1] = NULL;
      real (*Pres)[PS1][PS1]             = NULL;
      real (*Pot )[PS1][PS1]             = NULL;
      real (*Lohner_Var)                 = NULL;   // array storing the variables for Lohner
      real (*Lohner_Ave)                 = NULL;   // array storing the averages of Lohner_Var for Lohner
      real (*Lohner_Slope)               = NULL;   // array storing the slopes of Lohner_Var for Lohner
      real (*ParCount)[PS1][PS1]         = NULL;   // declare as **real** to be consistent with Par_MassAssignment
      real (*ParDens )[PS1][PS1]         = NULL;

      int  i_start, i_end, j_start, j_end, k_start, k_end, SibID, SibPID, PID;
      bool ProperNesting, NextPatch;

#     if   ( MODEL == HYDRO )
      if ( OPT__FLAG_PRES_GRADIENT )   Pres = new real [PS1][PS1][PS1];
#     elif ( MODEL == MHD )
#     warning : WAIT MHD !!!
#     endif // MODEL

#     ifdef PARTICLE
      if ( OPT__FLAG_NPAR_CELL )       ParCount = new real [PS1][PS1][PS1];
      if ( OPT__FLAG_PAR_MASS_CELL )   ParDens  = new real [PS1][PS1][PS1];
#     endif

      if ( Lohner_NVar > 0 )
      {
         Lohner_Var   = new real [ 8*Lohner_NVar*Lohner_NCell *Lohner_NCell *Lohner_NCell  ]; // 8: number of local patches
         Lohner_Ave   = new real [ 3*Lohner_NVar*Lohner_NAve  *Lohner_NAve  *Lohner_NAve   ]; // 3: X/Y/Z of 1 patch
         Lohner_Slope = new real [ 3*Lohner_NVar*Lohner_NSlope*Lohner_NSlope*Lohner_NSlope ]; // 3: X/Y/Z of 1 patch
      }


//    loop over all REAL patches (the buffer patches will be flagged only due to the FlagBuf
//    extension or the grandson check )
#     pragma omp for schedule( runtime )
      for (int PID0=0; PID0<amr->NPatchComma[lv][1]; PID0+=8)
      {
//       prepare the ghost-zone data for Lohner
         if ( Lohner_NVar > 0 )
            Prepare_PatchData( lv, Time[lv], Lohner_Var, Lohner_NGhost, NPG, &PID0, Lohner_TVar,
                               Lohner_IntScheme, UNIT_PATCH, NSIDE_26, IntPhase_No, OPT__BC_FLU, OPT__BC_POT,
                               MinDens, MinPres, DE_Consistency_No );


//       loop over all local patches within the same patch group
         for (int LocalID=0; LocalID<8; LocalID++)
         {
            PID = PID0 + LocalID;

//          check the proper-nesting condition
            ProperNesting = true;

            for (int sib=0; sib<26; sib++)
            {
//             for the non-periodic BC we have sibling index = SIB_OFFSET_NONPERIODIC - sibling = -1000 - sibling < 0
               if (  (  NoRefineNearBoundary && amr->patch[0][lv][PID]->sibling[sib] < 0   )  ||
                     ( !NoRefineNearBoundary && amr->patch[0][lv][PID]->sibling[sib] == -1 )     )
               {
                  ProperNesting = false;
                  break;
               }
            }

//          check further --> necessary for OPT__UM_START_DOWNGRADE to avoid refinement near the boundary
            if ( ProperNesting  &&  NoRefineNearBoundary )
            {
               for (int d=0; d<3; d++)
               {
                  int CornerL = amr->patch[0][lv][PID]->corner[d];
                  int CornerR = CornerL + Mis_Cell2Scale( PS1, lv );

                  if ( CornerL <= 0                + NoRefineBoundaryRegion  ||
                       CornerR >= amr->BoxScale[d] - NoRefineBoundaryRegion     )
                  {
                     ProperNesting = false;
                     break;
                  }
               }
            }


//          do flag check only if 26 siblings all exist (proper-nesting constraint)
            if ( ProperNesting )
            {
               NextPatch = false;
               Fluid     = amr->patch[ amr->FluSg[lv] ][lv][PID]->fluid;
#              ifdef GRAVITY
               Pot       = amr->patch[ amr->PotSg[lv] ][lv][PID]->pot;
#              endif


//             evaluate pressure
#              if   ( MODEL == HYDRO )
               if ( OPT__FLAG_PRES_GRADIENT )
               {
                  const bool CheckMinPres_Yes = true;
                  const real Gamma_m1         = GAMMA - (real)1.0;

                  real Ek;

                  for (int k=0; k<PS1; k++)
                  for (int j=0; j<PS1; j++)
                  for (int i=0; i<PS1; i++)
                  {
#                    ifdef DUAL_ENERGY
#                    if   ( DUAL_ENERGY == DE_ENPY )
                     Pres[k][j][i] = CPU_DensEntropy2Pres( Fluid[DENS][k][j][i], Fluid[ENPY][k][j][i],
                                                           Gamma_m1, CheckMinPres_Yes, MIN_PRES );
#                    elif ( DUAL_ENERGY == DE_EINT )
#                    error : DE_EINT is NOT supported yet !!
#                    endif

#                    else
                     Pres[k][j][i] = CPU_GetPressure( Fluid[DENS][k][j][i], Fluid[MOMX][k][j][i], Fluid[MOMY][k][j][i],
                                                      Fluid[MOMZ][k][j][i], Fluid[ENGY][k][j][i],
                                                      Gamma_m1, CheckMinPres_Yes, MIN_PRES );
#                    endif // #ifdef DUAL_ENERGY ... else ...
                  } // k,j,i
               } // if ( OPT__FLAG_PRES_GRADIENT )

#              elif ( MODEL == MHD )
#              warning : WAIT MHD !!!
#              endif // MODEL


//             evaluate the averages and slopes along x/y/z for Lohner
               if ( Lohner_NVar > 0 )
                  Prepare_for_Lohner( OPT__FLAG_LOHNER_FORM, Lohner_Var+LocalID*Lohner_Stride, Lohner_Ave, Lohner_Slope,
                                      Lohner_NVar );


//             count the number of particles and/or particle mass density on each cell
#              ifdef PARTICLE
               if ( OPT__FLAG_NPAR_CELL  ||  OPT__FLAG_PAR_MASS_CELL )
               {
                  long  *ParList = NULL;
                  int    NParThisPatch;
                  bool   UseInputMassPos;
                  real **InputMassPos = NULL;

//                determine the number of particles and the particle list
                  if ( amr->patch[0][lv][PID]->son == -1 )
                  {
                     NParThisPatch   = amr->patch[0][lv][PID]->NPar;
                     ParList         = amr->patch[0][lv][PID]->ParList;
                     UseInputMassPos = false;
                     InputMassPos    = NULL;

#                    ifdef DEBUG_PARTICLE
                     if ( amr->patch[0][lv][PID]->NPar_Copy != -1 )
                        Aux_Error( ERROR_INFO, "lv %d, PID %d, NPar_Copy = %d != -1 !!\n",
                                   lv, PID, amr->patch[0][lv][PID]->NPar_Copy );
#                    endif
                  }

                  else
                  {
                     NParThisPatch   = amr->patch[0][lv][PID]->NPar_Copy;
#                    ifdef LOAD_BALANCE
                     ParList         = NULL;
                     UseInputMassPos = true;
                     InputMassPos    = amr->patch[0][lv][PID]->ParMassPos_Copy;
#                    else
                     ParList         = amr->patch[0][lv][PID]->ParList_Copy;
                     UseInputMassPos = false;
                     InputMassPos    = NULL;
#                    endif

#                    ifdef DEBUG_PARTICLE
                     if ( amr->patch[0][lv][PID]->NPar != 0 )
                        Aux_Error( ERROR_INFO, "lv %d, PID %d, NPar = %d != 0 !!\n",
                                   lv, PID, amr->patch[0][lv][PID]->NPar );
#                    endif
                  }

#                 ifdef DEBUG_PARTICLE
                  if ( NParThisPatch < 0 )
                     Aux_Error( ERROR_INFO, "NPar (%d) has not been calculated (lv %d, PID %d) !!\n",
                                NParThisPatch, lv, PID );

                  if ( NParThisPatch > 0 )
                  {
                     if ( UseInputMassPos )
                     {
                        for (int v=0; v<4; v++)
                           if ( InputMassPos[v] == NULL )
                              Aux_Error( ERROR_INFO, "InputMassPos[%d] == NULL for NPar (%d) > 0 (lv %d, PID %d) !!\n",
                                         v, NParThisPatch, lv, PID );
                     }

                     else if ( !UseInputMassPos  &&  ParList == NULL )
                     Aux_Error( ERROR_INFO, "ParList == NULL for NPar (%d) > 0 (lv %d, PID %d) !!\n",
                                NParThisPatch, lv, PID );
                  }
#                 endif

//                deposit particle mass onto grids
//                --> for OPT__FLAG_NPAR_CELL, set UnitDens_Yes
//                --> for OPT__FLAG_PAR_MASS_CELL, set UnitDens_No and note that Par_MassAssignment() returns
//                    **density** instead of mass
//                    --> must multiply with the cell volume before checking the particle **mass** refinement criterion
                  if ( OPT__FLAG_NPAR_CELL )
                  Par_MassAssignment( ParList, NParThisPatch, PAR_INTERP_NGP, ParCount[0][0], PS1,
                                      amr->patch[0][lv][PID]->EdgeL, amr->dh[lv], PredictPos_No, NULL_REAL,
                                      InitZero_Yes, Periodic_No, NULL, UnitDens_Yes, CheckFarAway_No,
                                      UseInputMassPos, InputMassPos );

                  if ( OPT__FLAG_PAR_MASS_CELL )
                  Par_MassAssignment( ParList, NParThisPatch, PAR_INTERP_NGP, ParDens [0][0], PS1,
                                      amr->patch[0][lv][PID]->EdgeL, amr->dh[lv], PredictPos_No, NULL_REAL,
                                      InitZero_Yes, Periodic_No, NULL, UnitDens_No,  CheckFarAway_No,
                                      UseInputMassPos, InputMassPos );
               } // if ( OPT__FLAG_NPAR_CELL  ||  OPT__FLAG_PAR_MASS_CELL )
#              endif // #ifdef PARTICLE


//             loop over all cells within the target patch
               for (int k=0; k<PS1; k++)  {  if ( NextPatch )  break;
                                             k_start = ( k - FlagBuf < 0    ) ? 0 : 1;
                                             k_end   = ( k + FlagBuf >= PS1 ) ? 2 : 1;

               for (int j=0; j<PS1; j++)  {  if ( NextPatch )  break;
                                             j_start = ( j - FlagBuf < 0    ) ? 0 : 1;
                                             j_end   = ( j + FlagBuf >= PS1 ) ? 2 : 1;

               for (int i=0; i<PS1; i++)  {  if ( NextPatch )  break;
                                             i_start = ( i - FlagBuf < 0    ) ? 0 : 1;
                                             i_end   = ( i + FlagBuf >= PS1 ) ? 2 : 1;

//                check if the target cell satisfies the refinement criteria (useless pointers are always == NULL)
                  if (  lv < MAX_LEVEL  &&  Flag_Check( lv, PID, i, j, k, dv, Fluid, Pot, Pres, Lohner_Var+LocalID*Lohner_Stride,
                                                        Lohner_Ave, Lohner_Slope, Lohner_NVar, ParCount, ParDens, JeansCoeff )  )
                  {
//                   flag itself
                     amr->patch[0][lv][PID]->flag = true;

//                   flag sibling patches according to the size of FlagBuf
                     for (int kk=k_start; kk<=k_end; kk++)
                     for (int jj=j_start; jj<=j_end; jj++)
                     for (int ii=i_start; ii<=i_end; ii++)
                     {
                        SibID = SibID_Array[kk][jj][ii];

                        if ( SibID != 999 )
                        {
                           SibPID = amr->patch[0][lv][PID]->sibling[SibID];

#                          ifdef GAMER_DEBUG
                           if ( SibPID == -1 )
                              Aux_Error( ERROR_INFO, "SibPID == -1 --> proper-nesting check failed !!\n" );

                           if ( SibPID <= SIB_OFFSET_NONPERIODIC  &&  NoRefineNearBoundary )
                              Aux_Error( ERROR_INFO, "SibPID == %d when NoRefineNearBoundary is on !!\n", SibPID );
#                          endif

                           if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
                        }
                     }

//                   for FlagBuf == PATCH_SIZE, once a cell is flagged, all 26 siblings will be flagged
                     if ( FlagBuf == PS1 )   NextPatch = true;

                  } // check flag
               }}} // k, j, i


//             flag based on the number particles per patch (which doesn't need to go through all cells one-by-one)
#              ifdef PARTICLE
               if ( lv < MAX_LEVEL  &&  OPT__FLAG_NPAR_PATCH != 0 )
               {
                  const int NParFlag = FlagTable_NParPatch[lv];
                  int NParThisPatch;

                  if ( amr->patch[0][lv][PID]->son == -1 )  NParThisPatch = amr->patch[0][lv][PID]->NPar;
                  else                                      NParThisPatch = amr->patch[0][lv][PID]->NPar_Copy;

#                 ifdef DEBUG_PARTICLE
                  if ( NParThisPatch < 0 )
                     Aux_Error( ERROR_INFO, "NPar (%d) has not been calculated (lv %d, PID %d) !!\n",
                                NParThisPatch, lv, PID );
#                 endif

                  if ( NParThisPatch > NParFlag )
                  {
//                   flag itself
                     amr->patch[0][lv][PID]->flag = true;

//                   flag all siblings for OPT__FLAG_NPAR_PATCH == 2
                     if ( OPT__FLAG_NPAR_PATCH == 2 )
                     {
                        for (int s=0; s<26; s++)
                        {
                           SibPID = amr->patch[0][lv][PID]->sibling[s];

#                          ifdef DEBUG_PARTICLE
                           if ( SibPID == -1 )
                              Aux_Error( ERROR_INFO, "SibPID == -1 --> proper-nesting check failed !!\n" );

                           if ( SibPID <= SIB_OFFSET_NONPERIODIC  &&  NoRefineNearBoundary )
                              Aux_Error( ERROR_INFO, "SibPID == %d when NoRefineNearBoundary is on !!\n", SibPID );
#                          endif

                           if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
                        }
                     }
                  } // if ( NParThisPatch > NParFlag  )
               } // if ( OPT__FLAG_NPAR_PATCH != 0 )
#              endif // #ifdef PARTICLE

            } // if ( ProperNesting )
         } // for (int LocalID=0; LocalID<8; LocalID++)
      } // for (int PID0=0; PID0<amr->NPatchComma[lv][1]; PID0+=8)


      if ( Pres     != NULL )    delete [] Pres;
      if ( ParCount != NULL )    delete [] ParCount;
      if ( ParDens  != NULL )    delete [] ParDens;

      if ( Lohner_NVar > 0 )
      {
         delete [] Lohner_Var;
         delete [] Lohner_Ave;
         delete [] Lohner_Slope;
      }

   } // OpenMP parallel region


// free memory allocated by Par_CollectParticle2OneLevel
#  ifdef PARTICLE
   if ( OPT__FLAG_NPAR_CELL  ||  OPT__FLAG_PAR_MASS_CELL  ||  OPT__FLAG_NPAR_PATCH != 0 )
      Par_CollectParticle2OneLevel_FreeMemory( lv, SibBufPatch_No, FaSibBufPatch_No );
#  endif


// apply the proper-nesting constraint again (should also apply to the buffer patches)
#  pragma omp parallel for schedule( runtime )
   for (int PID=0; PID<amr->num[lv]; PID++)
   {
      for (int sib=0; sib<26; sib++)
      {
         if (  (  NoRefineNearBoundary && amr->patch[0][lv][PID]->sibling[sib] < 0   )  ||
               ( !NoRefineNearBoundary && amr->patch[0][lv][PID]->sibling[sib] == -1 )     )
         {
            amr->patch[0][lv][PID]->flag = false;
            break;
         }
      }

//    check further --> necessary for OPT__UM_START_DOWNGRADE to avoid refinement near the boundary
      if ( amr->patch[0][lv][PID]->flag  &&  NoRefineNearBoundary )
      {
        for (int d=0; d<3; d++)
        {
           int CornerL = amr->patch[0][lv][PID]->corner[d];
           int CornerR = CornerL + Mis_Cell2Scale( PS1, lv );

           if ( CornerL <= 0                + NoRefineBoundaryRegion  ||
                CornerR >= amr->BoxScale[d] - NoRefineBoundaryRegion     )
            {
               amr->patch[0][lv][PID]->flag = false;
               break;
            }
         }
      }
   } // for (int PID=0; PID<amr->num[lv]; PID++)


// invoke the load-balance functions
#  ifdef LOAD_BALANCE
   if ( UseLBFunc == USELB_YES )
   {
//    grandson check
      if ( lv < NLEVEL-2 )    LB_GrandsonCheck( lv );

//    exchange the flagged buffer patches
      LB_ExchangeFlaggedBuffer( lv );

      return;
   }
#  endif


   int SonPID;

// grandson check
   if ( lv < NLEVEL-2 )
   {
//###ISSUE: use atomic ??
//#     pragma omp parallel for private( SonPID )
      for (int PID=0; PID<amr->NPatchComma[lv][1]; PID++)
      {
         SonPID = amr->patch[0][lv][PID]->son;

         if ( SonPID != -1 )
         {
            for (int LocalID=0; LocalID<8; LocalID++)
            {
               if ( amr->patch[0][lv+1][SonPID+LocalID]->son != -1 )    // if grandson exists
               {
//                flag the corresponding siblings of patch PID
                  Flag_Grandson( lv, PID, LocalID );

//                flag the patch PID
                  amr->patch[0][lv][PID]->flag = true;
               }
            }
         }
      }
   }  // if ( lv < NLEVEL-2 )


// set up the BounFlag_NList and BounFlag_PosList
   Buf_RecordBoundaryFlag( lv );

} // FUNCTION : Flag_Real



//-------------------------------------------------------------------------------------------------------
// Function    :  Flag_Grandson
// Description :  Properly flag the siblings of patch "PID" at level "lv" to ensure that the proper-nesting
//                condition is satisfied at level "lv+2"
//
// Note        :  Properly take care of the non-periodic BC where there are no buffer patches outside the simulation box
//                and hence the proper-nesting constraint cannot be applied to the real patches adajacent to the simulation
//                boundary (sibling index <= SIB_OFFSET_NONPERIODIC)
//
// Parameter   :  lv      : Target level to be flagged
//                PID     : Target patch ID at level "lv"
//                LocalID : Index of son (0~7) which has its own son (grandson of patch "PID" at level "lv")
//-------------------------------------------------------------------------------------------------------
void Flag_Grandson( const int lv, const int PID, const int LocalID )
{

   int SibPID;

   switch ( LocalID )
   {
      case 0:
         SibPID = amr->patch[0][lv][PID]->sibling[ 0];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 2];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 4];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 6];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[14];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[18];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[10];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;

#        ifdef GAMER_DEBUG
         SibPID = amr->patch[0][lv][PID]->sibling[ 0];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 2];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 4];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 6];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[14];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[18];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[10];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
#        endif
         break;


      case 1:
         SibPID = amr->patch[0][lv][PID]->sibling[ 1];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 7];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[19];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[16];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 2];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 4];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[10];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;

#        ifdef GAMER_DEBUG
         SibPID = amr->patch[0][lv][PID]->sibling[ 1];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 7];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[19];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[16];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 2];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 4];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[10];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
#        endif
         break;


      case 2:
         SibPID = amr->patch[0][lv][PID]->sibling[ 3];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 8];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[11];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[20];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 0];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[14];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 4];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;

#        ifdef GAMER_DEBUG
         SibPID = amr->patch[0][lv][PID]->sibling[ 3];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 8];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[11];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[20];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 0];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[14];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 4];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
#        endif
         break;


      case 3:
         SibPID = amr->patch[0][lv][PID]->sibling[ 5];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[15];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[12];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[22];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 0];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 6];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 2];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;

#        ifdef GAMER_DEBUG
         SibPID = amr->patch[0][lv][PID]->sibling[ 5];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[15];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[12];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[22];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 0];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 6];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 2];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
#        endif
         break;


      case 4:
         SibPID = amr->patch[0][lv][PID]->sibling[ 1];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[21];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 9];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[16];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[11];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 3];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 4];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;

#        ifdef GAMER_DEBUG
         SibPID = amr->patch[0][lv][PID]->sibling[ 1];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[21];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 9];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[16];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[11];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 3];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 4];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
#        endif
         break;


      case 5:
         SibPID = amr->patch[0][lv][PID]->sibling[ 0];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[24];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 8];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[15];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 3];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[13];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 5];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;

#        ifdef GAMER_DEBUG
         SibPID = amr->patch[0][lv][PID]->sibling[ 0];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[24];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 8];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[15];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 3];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[13];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 5];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
#        endif
         break;


      case 6:
         SibPID = amr->patch[0][lv][PID]->sibling[ 1];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[23];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 7];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[17];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 2];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[12];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 5];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;

#        ifdef GAMER_DEBUG
         SibPID = amr->patch[0][lv][PID]->sibling[ 1];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[23];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 7];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[17];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 2];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[12];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 5];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
#        endif
         break;


      case 7:
         SibPID = amr->patch[0][lv][PID]->sibling[ 1];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 3];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 5];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[25];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[ 9];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[17];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;
         SibPID = amr->patch[0][lv][PID]->sibling[13];   if ( SibPID >= 0 )   amr->patch[0][lv][SibPID]->flag = true;

#        ifdef GAMER_DEBUG
         SibPID = amr->patch[0][lv][PID]->sibling[ 1];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 3];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 5];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[25];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[ 9];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[17];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
         SibPID = amr->patch[0][lv][PID]->sibling[13];   if ( SibPID == -1 )  Aux_Error( ERROR_INFO, "SibPID == -1 !!\n" );
#        endif
         break;


      default:
         Aux_Error( ERROR_INFO, "incorrect parameter %s = %d !!\n", "LocalID", LocalID );

   } // switch ( LocalID )

} // FUNCTION : Flag_Grandson

#include "GAMER.h"

static void Init_StartOver_AssignData( const int lv );




//-------------------------------------------------------------------------------------------------------
// Function    :  Init_StartOver
// Description :  Set up the initial condition by invoking the function "Init_StartOver_AssignData"
//-------------------------------------------------------------------------------------------------------
void Init_StartOver()
{

   if ( MPI_Rank == 0 )    Aux_Message( stdout, "Init_StartOver ... \n" );


// construct levels 0 ~ NLEVEL-1
   for (int lv=0; lv<NLEVEL; lv++)
   {
      if ( MPI_Rank == 0 )    Aux_Message( stdout, "   Constructing level %d ... ", lv );

      if ( lv == 0 )
      {
         Init_BaseLevel();

#        ifdef PARTICLE
         Par_FindHomePatch_Base( BaseP );
#        endif
      }

      Init_StartOver_AssignData( lv );

      Buf_GetBufferData( lv, amr->FluSg[lv], NULL_INT, DATA_GENERAL, _TOTAL, Flu_ParaBuf, USELB_NO );

      if ( lv != TOP_LEVEL )
      {
         Flag_Real( lv, USELB_NO );

         MPI_ExchangeBoundaryFlag( lv );

         Flag_Buffer( lv );

         Init_Refine( lv );
      }

      if ( MPI_Rank == 0 )    Aux_Message( stdout, "done\n" );

   } // for (int lv=0; lv<NLEVEL; lv++)


// restrict all variables to be consistent with the finite volume scheme
   if ( OPT__INIT_RESTRICT )
   {
      for (int lv=TOP_LEVEL-1; lv>=0; lv--)
      {
         Flu_Restrict( lv, amr->FluSg[lv+1], amr->FluSg[lv], NULL_INT, NULL_INT, _TOTAL );

         Buf_GetBufferData( lv, amr->FluSg[lv], NULL_INT, DATA_GENERAL, _TOTAL, Flu_ParaBuf, USELB_NO );
      } // for (int lv=NLEVEL-2; lv>=0; lv--)
   } // if ( OPT__INIT_RESTRICT )


   if ( MPI_Rank == 0 )    Aux_Message( stdout, "Init_StartOver ... done\n" );

} // FUNCTION : Init_StartOver



//-------------------------------------------------------------------------------------------------------
// Function    :  Init_StartOver_AssignData
// Description :  Construct the initial condition in different models
//
// Note        :  Work for the option "OPT__INIT == INIT_STARTOVER"
//
// Parameter   :  lv : Target refinement level
//-------------------------------------------------------------------------------------------------------
void Init_StartOver_AssignData( const int lv )
{

#  if   ( MODEL == HYDRO )
   Hydro_Init_StartOver_AssignData( lv );

#  elif ( MODEL == MHD )
#  warning : WAIT MHD !!!

#  elif ( MODEL == ELBDM )
   ELBDM_Init_StartOver_AssignData( lv );

#  else
#  error : unsupported MODEL !!
#  endif // MODEL

} // FUNCTION : Init_StartOver_AssignData

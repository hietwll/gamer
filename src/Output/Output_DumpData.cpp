#include "Copyright.h"
#include "GAMER.h"

static void Write_DumpRecord();




//-------------------------------------------------------------------------------------------------------
// Function    :  Output_DumpData
// Description :  Trigger the output functions "Output_DumpData_Total, Output_DumpData_Part, Output_TestProbErr, 
//                Output_BasePowerSpectrum, Par_Output_Particle"
//
// Parameter   :  Stage :  0 : start
//                         1 : middle
//                         2 : end
//-------------------------------------------------------------------------------------------------------
void Output_DumpData( const int Stage )
{

// check
   if ( Stage < 0  ||  Stage > 2 )
      Aux_Error( ERROR_INFO, "incorrect parameter %s = %d !!\n", "Stage", Stage );


// nothing to do if all output options are off
#  ifdef PARTICLE
   if ( !OPT__OUTPUT_TOTAL && !OPT__OUTPUT_PART && !OPT__OUTPUT_TEST_ERROR && !OPT__OUTPUT_BASEPS && !OPT__OUTPUT_PARTICLE )
#  else
   if ( !OPT__OUTPUT_TOTAL && !OPT__OUTPUT_PART && !OPT__OUTPUT_TEST_ERROR && !OPT__OUTPUT_BASEPS )
#  endif
      return;


// set the first dump time 
   static int DumpTableID;

   if ( Stage == 0 )
   {
      switch ( OPT__OUTPUT_MODE )
      {
         case OUTPUT_CONST_DT :
         {
            if ( OPT__INIT != INIT_RESTART )    DumpTime = Time[0];
            else                                DumpTime = Time[0] + OUTPUT_DT;
         }
         break;

         case OUTPUT_USE_TABLE :
         {
            if ( OPT__INIT != INIT_RESTART )
            {
               for (DumpTableID=0; DumpTableID<DumpTable_NDump; DumpTableID++)
               {
                  DumpTime = DumpTable[DumpTableID];

                  if (   (  DumpTime >= Time[0]  )                                            ||  
                         (  Time[0] != 0.0 && fabs( (Time[0]-DumpTime)/Time[0] ) < 1.0e-8  )  ||   
                         (  Time[0] == 0.0 && fabs(  Time[0]-DumpTime          ) < 1.0e-12 )      )   break;     
               }
            }

            else
            {
               for (DumpTableID=0; DumpTableID<DumpTable_NDump; DumpTableID++)
               {
                  DumpTime = DumpTable[DumpTableID];

                  if (   (  DumpTime >= Time[0]  )                                            &&  
                        !(  Time[0] != 0.0 && fabs( (Time[0]-DumpTime)/Time[0] ) < 1.0e-8  )  &&   
                        !(  Time[0] == 0.0 && fabs(  Time[0]-DumpTime          ) < 1.0e-12 )      )   break;
               }
            } 

            if ( DumpTableID >= DumpTable_NDump )
               Aux_Error( ERROR_INFO, "no proper data dump time is found in the dump table !!\n" );
         }
         break;

      } // switch ( OPT__OUTPUT_MODE )
   } // if ( Stage == 0 )


// do not output the initial data for the restart run
   if ( OPT__INIT == INIT_RESTART  &&  Stage == 0 )     return;


// set the file names for all output functions
   char FileName_Total[50], FileName_Part[50], FileName_Temp[50], FileName_PS[50];
#  ifdef PARTICLE
   char FileName_Particle[50];
#  endif
   int ID[6];

   ID[0] = DumpID/100000;
   ID[1] = DumpID%100000/10000;
   ID[2] = DumpID%10000/1000;
   ID[3] = DumpID%1000/100;
   ID[4] = DumpID%100/10;
   ID[5] = DumpID%10;

   if ( OPT__OUTPUT_TOTAL )
      sprintf( FileName_Total, "Data_%d%d%d%d%d%d", ID[0], ID[1], ID[2], ID[3], ID[4], ID[5] );
    
   if ( OPT__OUTPUT_PART )
   {
      switch ( OPT__OUTPUT_PART )
      {
         case OUTPUT_XY :    sprintf( FileName_Temp, "XYslice_z%.3f_%d%d%d%d%d%d", 
                             OUTPUT_PART_Z, ID[0], ID[1], ID[2], ID[3], ID[4], ID[5] );
                             break;

         case OUTPUT_YZ :    sprintf( FileName_Temp, "YZslice_x%.3f_%d%d%d%d%d%d", 
                             OUTPUT_PART_X, ID[0], ID[1], ID[2], ID[3], ID[4], ID[5] );
                             break;

         case OUTPUT_XZ :    sprintf( FileName_Temp, "XZslice_y%.3f_%d%d%d%d%d%d", 
                             OUTPUT_PART_Y, ID[0], ID[1], ID[2], ID[3], ID[4], ID[5] );
                             break;

         case OUTPUT_X  :    sprintf( FileName_Temp, "Xline_y%.3f_z%.3f_%d%d%d%d%d%d", 
                             OUTPUT_PART_Y, OUTPUT_PART_Z, ID[0], ID[1], ID[2], ID[3], ID[4], ID[5] );
                             break;

         case OUTPUT_Y  :    sprintf( FileName_Temp, "Yline_x%.3f_z%.3f_%d%d%d%d%d%d", 
                             OUTPUT_PART_X, OUTPUT_PART_Z, ID[0], ID[1], ID[2], ID[3], ID[4], ID[5] );
                             break;

         case OUTPUT_Z  :    sprintf( FileName_Temp, "Zline_x%.3f_y%.3f_%d%d%d%d%d%d", 
                             OUTPUT_PART_X, OUTPUT_PART_Y, ID[0], ID[1], ID[2], ID[3], ID[4], ID[5] );
                             break;

         case OUTPUT_DIAG :  sprintf( FileName_Temp, "Diag_%d%d%d%d%d%d", 
                                      ID[0], ID[1], ID[2], ID[3], ID[4], ID[5] );
                             break;

         default :
                             Aux_Error( ERROR_INFO, "incorrect parameter %s = %d !!\n", 
                                        "OPT__OUTPUT_PART", OPT__OUTPUT_PART );

      } // switch ( OPT__OUTPUT_PART )

      if ( OPT__OUTPUT_BASE )    
      {
         sprintf( FileName_Part, "%s", "Base" );
         strcat( FileName_Part, FileName_Temp );
      }
      else
         strcpy( FileName_Part, FileName_Temp );

   } // if ( OPT__OUTPUT_PART )

   if ( OPT__OUTPUT_BASEPS )
      sprintf( FileName_PS, "PowerSpec_%d%d%d%d%d%d", ID[0], ID[1], ID[2], ID[3], ID[4], ID[5] );

#  ifdef PARTICLE
   if ( OPT__OUTPUT_PARTICLE )
      sprintf( FileName_Particle, "Particle_%d%d%d%d%d%d", ID[0], ID[1], ID[2], ID[3], ID[4], ID[5] );
#  endif


// determine whether or not to output data during the simulation
   static int PreviousDumpStep   = -999;
   bool OutputData               = false;

   switch ( OPT__OUTPUT_MODE )
   {
      case OUTPUT_CONST_STEP :   if ( Step%OUTPUT_STEP == 0 )     OutputData = true; 
                                 break;

      case OUTPUT_CONST_DT : 
      case OUTPUT_USE_TABLE :    if (   ( Time[0] != 0.0 && fabs( (Time[0]-DumpTime)/Time[0] ) < 1.0e-8  )   
                                     || ( Time[0] == 0.0 && fabs(  Time[0]-DumpTime          ) < 1.0e-12 )   )   
                                    OutputData = true;
                                 break; 

      default :
         Aux_Error( ERROR_INFO, "incorrect parameter %s = %d !!\n", "OPT__OUTPUT_MODE", OPT__OUTPUT_MODE );
   } // switch ( OPT__OUTPUT_MODE )


// always output data in the end of the simulation (unless it has already been output)
   if ( Stage == 2 )
   {
      if ( Step == PreviousDumpStep )     OutputData = false;
      else                                OutputData = true;
   }


// dump data if any process has detected the file named "DUMP_GAMER_DUMP"
   int OutputData_RunTime = false;

// enable this functionality only if OPT__MANUAL_CONTROL is on
   if ( OPT__MANUAL_CONTROL )    Output_DumpManually( OutputData_RunTime );


// output data
   if ( OutputData || OutputData_RunTime )
   {
//    synchronize all particles
#     ifdef PARTICLE
      int ParSyncStatus = -1;
      if ( amr->Par->SyncDump )  ParSyncStatus = Par_Synchronize( Time[0], PAR_SYNC_TEMP );
#     endif

      if ( OPT__OUTPUT_TOTAL )      Output_DumpData_Total( FileName_Total );
      if ( OPT__OUTPUT_PART  )      Output_DumpData_Part( OPT__OUTPUT_PART, OPT__OUTPUT_BASE, OUTPUT_PART_X, 
                                                          OUTPUT_PART_Y, OUTPUT_PART_Z, FileName_Part );
      if ( OPT__OUTPUT_TEST_ERROR ) Output_TestProbErr( OPT__OUTPUT_BASE );
#     ifdef GRAVITY
      if ( OPT__OUTPUT_BASEPS )     Output_BasePowerSpectrum( FileName_PS );
#     endif

#     ifdef PARTICLE
      if ( OPT__OUTPUT_PARTICLE )   Par_Output_Particle( FileName_Particle );
#     endif

      Write_DumpRecord();

      DumpID ++;

      if ( OutputData )
      {
         if ( OPT__OUTPUT_MODE == OUTPUT_CONST_DT  )  DumpTime = Time[0] + OUTPUT_DT;
         if ( OPT__OUTPUT_MODE == OUTPUT_USE_TABLE )  DumpTime = DumpTable[ ++DumpTableID ];
      }

      PreviousDumpStep = Step;

//    restore particle attributes to the values before synchronization
#     ifdef PARTICLE
      if ( ParSyncStatus == 0 )  Par_Synchronize_Restore( Time[0] );
#     endif
   } // if ( OutputData || OutputData_RunTime )

} // FUNCTION : Output_DumpData



//-------------------------------------------------------------------------------------------------------
// Function    :  Write_DumpRecord
// Description :  Record the information of each data dump in the file "Record__Dump"
//-------------------------------------------------------------------------------------------------------
void Write_DumpRecord()
{

   const char FileName[] = "Record__Dump";


// create the "Record__Dump" file at the first dump
   static bool FirstTime = true;

   if ( MPI_Rank == 0  &&  FirstTime )
   {
      if ( Aux_CheckFileExist(FileName) )
         Aux_Message( stderr, "WARNING : file \"%s\" already exists !!\n", FileName );

      else
      {
         FILE *File = fopen( FileName, "w" );
         fprintf( File, "%6s\t\t%20s\t\t%9s\n", "DumpID", "Time", "Step" );
         fclose( File );
      }

      FirstTime = false;
   }


// record the information of data dump in the file "Record__Dump"
   if ( MPI_Rank == 0 )
   {
      FILE *File = fopen( FileName, "a" );
      fprintf( File, "%6d\t\t%20.14e\t\t%9ld\n", DumpID, Time[0], Step );
      fclose( File );
   }

} // FUNCTION : Write_DumpRecord
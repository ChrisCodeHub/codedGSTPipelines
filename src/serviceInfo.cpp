#include <gst/gst.h>
#include <stdlib.h>     /* atoi */
#include <string.h>
#include <stdbool.h>
#include <iostream>
#include "serviceInfo.h"
#include "localTypes.h"



streamServicesInfo::streamServicesInfo(void): haveSeenPAT(false),
                                              haveSeenPMT(false),
                                              haveSeenSDT(false),
                                              numberServicesInfoStoredFor(0),
                                              MAX_numberServicesInfoStoredFor(40)
{

    ServiceComponents = (PerServiceInfo**)(malloc(MAX_numberServicesInfoStoredFor * (sizeof(PerServiceInfo*))));
    for (int i = 0; i < MAX_numberServicesInfoStoredFor; ++i)
    {
        ServiceComponents[i] = NULL;
    }
}

//##############################################################
//
//
//
streamServicesInfo::~streamServicesInfo()
{
    for (int i = 0; i < MAX_numberServicesInfoStoredFor; ++i)
    {
        if (ServiceComponents[i] != NULL)
        {
            if(ServiceComponents[i]->ServiceName != NULL)
            {
                free (ServiceComponents[i]->ServiceName);
            }
            free(ServiceComponents[i]);
        }
    }
    if(ServiceComponents != NULL)
    {
        free(ServiceComponents);
    }     
}






//##############################################################
//
void streamServicesInfo::PAT_messageHandler( GstMpegtsSection *pTSSectionBeingStudied)
{
    std::cout<<"PAT Section"<<std::endl;
    
    haveSeenPAT = true;
    GPtrArray *pPAT_Section = gst_mpegts_section_get_pat (pTSSectionBeingStudied);
    if (NULL != pPAT_Section)
    {
        // how many structures are there
        guint16 lengthOfStructures = pPAT_Section->len;
        guint16 nextProgramNumber;
        guint16 programsPMT_PID;
        // NOTE that the programe_number 0x0000 is the NIT,  not a PMT
        gpointer Element = g_ptr_array_index(pPAT_Section, 0);
        nextProgramNumber = ((GstMpegtsPatProgram*)Element)->program_number;
        programsPMT_PID   = ((GstMpegtsPatProgram*)Element)->network_or_program_map_PID;
        g_print("PAT length  %d  %d  %04x \n", lengthOfStructures, nextProgramNumber, programsPMT_PID);
        g_ptr_array_unref(pPAT_Section);
    }

    return;
}
//##############################################################
//
void streamServicesInfo::PMT_messageHandler( GstMpegtsSection *pTSSectionBeingStudied)
{
    static int PMT_HasBeenParsed = FALSE;
    // for reference
    /* struct GstMpegtsPMT {  guint16    pcr_pid;         //
                                guint16    program_number;  //
                                GPtrArray *descriptors;     // array of GstMpegtsDescriptor
                                GPtrArray *streams;         // Array of GstMpegtsPMTStream.
                            };
    */
    const GstMpegtsPMT *pPMT_Section;
    pPMT_Section = gst_mpegts_section_get_pmt(pTSSectionBeingStudied);
    guint16  thisProgramPCR_PID = pPMT_Section->pcr_pid;
    guint16  thisProgramProgramNumber = pPMT_Section->program_number;
    haveSeenPMT = true;

    guint16 nextSIStoreSlot = numberServicesInfoStoredFor;
    guint16 maxSIStoreSlot  = MAX_numberServicesInfoStoredFor;

    GPtrArray *pPMTstreams     = pPMT_Section->streams;
    guint16 lengthOfStructures = pPMTstreams->len;
    gpointer Element;
    guint16 componentPID, i;
    guint16 componentType;

    guint16 videoPID = 0;
    guint16 audioPID = 0;
    GPtrArray *streamDescriptors;

    for (i = 0; i < lengthOfStructures; i++)
    {
        Element       = g_ptr_array_index(pPMTstreams, i);
        componentPID  = ((GstMpegtsPMTStream*)Element)->pid;
        componentType = ((GstMpegtsPMTStream*)Element)->stream_type;

        switch (componentType)
        {
            case GST_MPEGTS_STREAM_TYPE_VIDEO_MPEG2:
            case GST_MPEGTS_STREAM_TYPE_VIDEO_H264:
            case GST_MPEGTS_STREAM_TYPE_VIDEO_HEVC:
            {
                videoPID = componentPID;
                #if 0
                streamDescriptors = ((GstMpegtsPMTStream*)Element)->descriptors;
                guint16 i, streamDescriptorsLength = streamDescriptors->len;
                //g_print("video descriptors len %d : ", streamDescriptorsLength);
                for (i = 0; i < streamDescriptorsLength; i++)
                {
                    gpointer nextDescriptor  = g_ptr_array_index(streamDescriptors, i);
                    guint8 nextTag = ((GstMpegtsDescriptor*)nextDescriptor)->tag;
                    // TAG 0x02 is the video descriptor tag that contains the
                    // chroma format, frame rate profile and level indications
                    g_print(" 0x%x", nextTag);
                }
                g_print("\n");
                #endif
            }
            break;
            case GST_MPEGTS_STREAM_TYPE_PRIVATE_PES_PACKETS:
                audioPID = componentPID;
        }
    }
    //g_ptr_array_unref(pPMTstreams);
    // check to see if the program_number has already been stored in the table, or if this is new
    //information that requires a new entry to be created

    int programAlreadyRecorded = FALSE;
    for(guint16 entry = 0; entry < nextSIStoreSlot; entry++)
    {
        if( (ServiceComponents[entry]->programNumber == pPMT_Section->program_number))
        {
            programAlreadyRecorded = TRUE;
            break;  // jump out of for loop if we've found the entry
        }
    }
    if(programAlreadyRecorded == FALSE)
    {
        if (nextSIStoreSlot < maxSIStoreSlot)
        {
        ServiceComponents[nextSIStoreSlot] = (PerServiceInfo*)malloc(sizeof(PerServiceInfo));
        numberServicesInfoStoredFor++;

        ServiceComponents[nextSIStoreSlot]->PCR_RID = pPMT_Section->pcr_pid;
        ServiceComponents[nextSIStoreSlot]->VideoPID = videoPID;
        ServiceComponents[nextSIStoreSlot]->AudioPID = audioPID;
        ServiceComponents[nextSIStoreSlot]->programNumber = pPMT_Section->program_number;
        ServiceComponents[nextSIStoreSlot]->ServiceName = NULL;     // set these two ponters SAFE
        ServiceComponents[nextSIStoreSlot]->ServiceProvider = NULL;

        }
        else
        {
            g_print("RAN OUT OF SI STORE SLOTS!!!!!! \n");
        }
    }
    return;

}

//##############################################################
//
void streamServicesInfo::SDT_messageHandler( GstMpegtsSection *pTSSectionBeingStudied)
{
    const GstMpegtsSDT *pSDT_Section;
    haveSeenSDT = true;

    g_print("SDT  Section \n");
    pSDT_Section = gst_mpegts_section_get_sdt(pTSSectionBeingStudied);
    GPtrArray *pSDTservices = pSDT_Section->services;
    if (NULL != pSDTservices)
    {
        guint16 lengthOfStructures = pSDTservices->len;
        guint16 ProgramToParse     = 0;
        for (ProgramToParse = 0; ProgramToParse < lengthOfStructures; ProgramToParse++)
        {
            gpointer Element          = g_ptr_array_index(pSDTservices, ProgramToParse);
            guint8 servceID            = ((GstMpegtsSDTService*)Element)->service_id;
            GPtrArray *pSDTDecsriptors = ((GstMpegtsSDTService*)Element)->descriptors;
            if(pSDTDecsriptors != NULL)
            {
                const gpointer Descriptor = g_ptr_array_index(pSDTDecsriptors , 0);
                guint8 tag = ((GstMpegtsDescriptor*)Descriptor)->tag;
                gboolean didIgetAServiceName;
                GstMpegtsDVBServiceType *service_type;
                gchar *ServiceName[100];
                gchar *ProvideName[100];
                gchar **pServiceName = &ServiceName[0];
                gchar **pProvideName  = &ProvideName[0];
                gst_mpegts_descriptor_parse_dvb_service( (const GstMpegtsDescriptor*)Descriptor,
                                                        NULL,
                                                        pServiceName,
                                                        pProvideName);
                g_ptr_array_unref(pSDTDecsriptors);

                // store findings
                guint16 SIStoreSlotsUsedSoFar = numberServicesInfoStoredFor;
                guint16 i;
                for (i = 0 ; i < SIStoreSlotsUsedSoFar; i++)
                {
                if (ServiceComponents[i]->programNumber == servceID)
                {
                    ServiceComponents[i]->ServiceName = (char*)malloc(100);
                    strcpy(ServiceComponents[i]->ServiceName, ServiceName[0]);
                }
                }
                // end store
                g_ptr_array_unref(pSDTDecsriptors);
            }
        }
        g_print("\n");
        g_ptr_array_unref(pSDTservices);
    }
return;
}

//##############################################################
//
//
//
void streamServicesInfo::ParseInfoFromTSFrontEnds( GstMessage *msg)
{
  #if 1
  // Try to get at the info inside the message
  if ( GST_MESSAGE_ELEMENT == GST_MESSAGE_TYPE (msg))
  {
    GstMpegtsSection *pTSSectionBeingStudied;
     pTSSectionBeingStudied = gst_message_parse_mpegts_section(msg);
     if (pTSSectionBeingStudied != NULL)
     {
          switch (GST_MPEGTS_SECTION_TYPE (pTSSectionBeingStudied))
          {
            case GST_MPEGTS_SECTION_PAT:
              {
                  PAT_messageHandler(pTSSectionBeingStudied);
              }
              break;
            case GST_MPEGTS_SECTION_PMT:
              {
                PMT_messageHandler(pTSSectionBeingStudied);
              }
              break;
              
            case GST_MPEGTS_SECTION_TDT:
                {
                    //g_print("TDT  Section \n");
                }
                break;
            case GST_MPEGTS_SECTION_SDT:
            {
                SDT_messageHandler(pTSSectionBeingStudied);
            }
            break;
            default:
                  {
                      unsigned int MessageType = GST_MPEGTS_SECTION_TYPE (pTSSectionBeingStudied);
                      g_print(" UNKNOWN MESSGAE %d ", MessageType);
                      break;
                  }


          }
      }
  }
  #endif
}


//##############################################################
//
//  show what we found
//

void streamServicesInfo::ShowStreamSummaries(void)
{
    for (int i = 0; i < MAX_numberServicesInfoStoredFor; ++i)
    {
        if (ServiceComponents[i] != NULL)
        {
            g_print("PMT [%02d]: PCR 0x%04x  V:0x%04x  A:0x%04x Service Name:%s \n",ServiceComponents[i]->programNumber,
                                                                                    ServiceComponents[i]->PCR_RID,
                                                                                    ServiceComponents[i]->VideoPID,
                                                                                    ServiceComponents[i]->AudioPID,
                                                                                    ServiceComponents[i]->ServiceName);
        }
    }
}
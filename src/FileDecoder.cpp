#include <gst/gst.h>
#include <stdlib.h>     /* atoi */
#include <string.h>
#include <stdbool.h>
#include <iostream>

// below line is required as the mpegts stuff is unstable
#define GST_USE_UNSTABLE_API
#include <gst/mpegts/mpegts.h>



// defiitions and enums that describe teh services
typedef enum{
        MPEG2 = 0,
        H264 = 1
}eVideoCodec;

typedef enum{
      yuv420p = 0,
      yuv422p,
      yuv420p10,
      yuv422p10
}eChromaFormat;


typedef struct{
  guint16       programNumber;
  guint16       VideoPID;
  guint16       AudioPID;
  guint16       PCR_RID;
  eVideoCodec   VideoCodec;
  char*         ServiceProvider;
  char*         ServiceName;
  guint16       VideoWidth;
  guint16       VideoHeight;
  eChromaFormat ChromaFormat;
}PerServiceMetaData;

typedef struct{
    bool  haveSeenPAT;
    bool  haveSeenPMT;
    bool  haveSeenSDT;
    guint16 numberServicesInfoStoredFor;
    guint16 MAX_numberServicesInfoStoredFor;
    PerServiceMetaData  **ServiceComponents;
}ServiceMetaData;


typedef struct{
  GMainLoop       *mainLoop;
  ServiceMetaData *pServiceMetaData;
}mainAppMetaData;


// below is used in the periodic timer based "Janitor function"
typedef struct
{
  int timerCallsSoFar;
  GstElement *pipelineToControl;
  GMainLoop  *ApplicationMainloop;
  GstElement *parserInLine;
  GstElement *videoDecoder;
  GstElement *theVideoAdapter;
  GstElement *theVideoFakeSink;
}infoAboutMe;

// useful URLS
// https://www.dvb.org/resources/public/standards/a38_dvb-si_specification.pdf
// Link to how to tell what the structures are
// https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-bad-libs/html/mpegts.html
// /usr/include/gstreamer-1.0/gst
//https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-bad-libs/html/gst-plugins-bad-libs-Base-MPEG-TS-sections.html#GstMpegtsSection-struct


//how to use GstCaps *gst_pad_get_caps (GstPad *pad);
//GstCaps *gst_pad_get_caps

// stuff to read capabilties to see what is flowingthrough the system
void read_video_props (GstCaps *caps)
{
  gint width, height;
  const GstStructure *str;

  g_return_if_fail (gst_caps_is_fixed (caps));

  str = gst_caps_get_structure (caps, 0);
  if (!gst_structure_get_int (str, "width", &width) ||
      !gst_structure_get_int (str, "height", &height)) {
    g_print ("No width/height available\n");
    return;
  }

  g_print ("The video size of this set of capabilities is %dx%d\n",
       width, height);
}



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// This program is aimed at testing that the below CLI
// #gst-launch-1.0 -v filesrc location=/stream.ts
//                  ! decodebin !
//                    videoconvert !
//                    autovideosink
// can be made into a C program and still work


static gboolean timedCall (gpointer data)
{
  infoAboutMe* psystemMetaInfo = (infoAboutMe*)data;
  g_print("Timed  %d\n", psystemMetaInfo->timerCallsSoFar);
  psystemMetaInfo->timerCallsSoFar++;

  // really brittle!
  if(1)
  {
    if(psystemMetaInfo->timerCallsSoFar == 3)
    {
        GstPad* VideoAdapterOutputPin;
        GstCaps* capsOfVideoOutputPin;
        g_print(" Checking pin caps  \n");
        VideoAdapterOutputPin = gst_element_get_static_pad (psystemMetaInfo->theVideoAdapter, "sink");
        if (VideoAdapterOutputPin == NULL)
        {
          g_print(" PIN IS NULL  \n");
        }
        else
        {
          gboolean isActive = gst_pad_is_active (VideoAdapterOutputPin);
          (isActive == TRUE) ? g_print(" PIN IS ACTIVE  \n"): g_print(" PIN IS ASLEEP  \n");
        }
        capsOfVideoOutputPin = gst_pad_get_current_caps(VideoAdapterOutputPin);
        if (capsOfVideoOutputPin == NULL)
        {
          g_print(" CAPS IS NULL  \n");
        }
        else
        {
            gint width, height;
            const GstStructure *str;

            //g_return_if_fail (gst_caps_is_fixed (capsOfVideoOutputPin));

            str = gst_caps_get_structure (capsOfVideoOutputPin, 0);
            if (!gst_structure_get_int (str, "width", &width) ||
                !gst_structure_get_int (str, "height", &height))
            {
              g_print ("No width/height available\n");
            }
            else
            {
              g_print (" video is %dx%d\n", width, height);
            }
        }
        //read_video_props(capsOfVideoOutputPin);
        //gst_pad_get_caps();
    }
  }
  if(psystemMetaInfo->timerCallsSoFar == 5)
  {
      g_print(" Setting pipeline state to NULL \n");
      gst_element_set_state (psystemMetaInfo->pipelineToControl, GST_STATE_NULL);
  }
  if(psystemMetaInfo->timerCallsSoFar == 8)
  {
      g_print(" quitting the main loop \n");
      g_main_loop_quit(psystemMetaInfo->ApplicationMainloop);
  }
  return(1);
}


static void ParseInfoFromTSFrontEnds( GstMessage *msg,
                                      ServiceMetaData *pServiceMeta )
{
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
                g_print(" PAT  Section \n");
                pServiceMeta->haveSeenPAT = true;
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
              }
              break;
            case GST_MPEGTS_SECTION_PMT:
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
                pServiceMeta->haveSeenPMT = true;

                guint16 nextSIStoreSlot = pServiceMeta->numberServicesInfoStoredFor;
                guint16 maxSIStoreSlot  = pServiceMeta->MAX_numberServicesInfoStoredFor;

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
                  if( (pServiceMeta->ServiceComponents[entry]->programNumber == pPMT_Section->program_number))
                  {
                      programAlreadyRecorded = TRUE;
                      break;  // jump out of for loop if we've found the entry
                  }
                }
                if(programAlreadyRecorded == FALSE)
                {
                  if (nextSIStoreSlot < maxSIStoreSlot)
                  {
                    pServiceMeta->ServiceComponents[nextSIStoreSlot] = (PerServiceMetaData*)malloc(sizeof(PerServiceMetaData));
                    pServiceMeta->numberServicesInfoStoredFor++;

                    pServiceMeta->ServiceComponents[nextSIStoreSlot]->PCR_RID = pPMT_Section->pcr_pid;
                    pServiceMeta->ServiceComponents[nextSIStoreSlot]->VideoPID = videoPID;
                    pServiceMeta->ServiceComponents[nextSIStoreSlot]->AudioPID = audioPID;


                    pServiceMeta->ServiceComponents[nextSIStoreSlot]->programNumber = pPMT_Section->program_number;
                    pServiceMeta->ServiceComponents[nextSIStoreSlot]->ServiceName = NULL;     // set these two ponters SAFE
                    pServiceMeta->ServiceComponents[nextSIStoreSlot]->ServiceProvider = NULL;

                  }
                  else
                  {
                      g_print("RAN OUT OF SI STORE SLOTS!!!!!! \n");
                  }
                }
                break;
              }
            case GST_MPEGTS_SECTION_TDT:
                {
                //g_print("TDT  Section \n");
                }
                break;
            case GST_MPEGTS_SECTION_SDT:
            {
                /*struct _GstMpegtsSDT
                  struct _GstMpegtsSDTService
                  struct GstMpegtsDescriptor
                  */

                    const GstMpegtsSDT *pSDT_Section;
                    pServiceMeta->haveSeenSDT = true;

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
                              guint16 SIStoreSlotsUsedSoFar = pServiceMeta->numberServicesInfoStoredFor;
                              guint16 i;
                              for (i = 0 ; i < SIStoreSlotsUsedSoFar; i++)
                              {
                                if (pServiceMeta->ServiceComponents[i]->programNumber == servceID)
                                {
                                  pServiceMeta->ServiceComponents[i]->ServiceName = (char*)malloc(100);
                                  strcpy(pServiceMeta->ServiceComponents[i]->ServiceName, ServiceName[0]);
                                }
                              }
                              // end store
                              g_ptr_array_unref(pSDTDecsriptors);
                          }
                        }
                        g_print("\n");
                        g_ptr_array_unref(pSDTservices);
                    }
            }
            break;
            default:
                  {
                      unsigned int MessageType = GST_MPEGTS_SECTION_TYPE (pTSSectionBeingStudied);
                      g_print(" UNKNOWN MESSGAE %d ", MessageType);
                      break;
                  }


          }
          // end of "switch (GST_MPEGTS_SECTION_TYPE (pTSSectionBeingStudied))""

      }
  }
}



static gboolean busCall ( GstBus     *bus,
                          GstMessage *msg,
                          gpointer    data)
{
  mainAppMetaData *pMainAppMeta = (mainAppMetaData*)data;
  GMainLoop *loop = pMainAppMeta->mainLoop;
  ServiceMetaData *pServiceMeta = pMainAppMeta->pServiceMetaData;

  if (0 == strcmp(GST_MESSAGE_SRC_NAME(msg),"TSParser"))
  {
    ParseInfoFromTSFrontEnds(msg, pMainAppMeta->pServiceMetaData);
  }

  return TRUE;
}


static void onPadAdded (GstElement *element,
                        GstPad     *pad,
                        gpointer    data)
{
  GstPad *sinkpad;
  GstElement *downStreamElement = (GstElement *) data;

  /* We can now link this pad with the vorbis-decoder sink pad */
  g_print ("Dynamic pad created \n");
  sinkpad = gst_element_get_static_pad (downStreamElement, "sink");
  gst_pad_link (pad, sinkpad);
  gst_object_unref (sinkpad);
}


///////////////////////////////////////////////////////////////////////////////////
//
//

int main(int argc, char *argv[])
{
  GMainLoop *loop;
  GstElement *pipeline, *VideoSink, *testSource, *tsdemuxer, *tsparser;
  GstElement *genericDecoder, *videoAdapter, *fileSource, *h264Videoparser;
  GstElement *chromaBlatter;
  GstBus *bus;
  guint bus_watch_id;
  int numBuffers, i;
  infoAboutMe systemMetaInfo;
  ServiceMetaData ServiceInfo_MasterStore;
  ServiceMetaData *pServiceInfoTable = &ServiceInfo_MasterStore;
  mainAppMetaData MainAppMeta;
  std::string inputTSFile("NULL");


 if (argc == 2)
  {
    inputTSFile = argv[1];
  }
  else
  {
    std::cout<< "Requires a source TS file to work on"<<std::endl;
    exit(0);
  }

  numBuffers = 500;



  //   **ServiceComponents;


  // Initialize GStreamer
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);
  MainAppMeta.mainLoop = loop;
  MainAppMeta.pServiceMetaData = pServiceInfoTable;

  ServiceInfo_MasterStore.haveSeenPAT = false;
  ServiceInfo_MasterStore.haveSeenPMT = false;
  ServiceInfo_MasterStore.haveSeenSDT = false;
  ServiceInfo_MasterStore.numberServicesInfoStoredFor = 0;
  ServiceInfo_MasterStore.MAX_numberServicesInfoStoredFor = 40;

  // create an array of 40 pointers to "PerServiceMetaData" which will hold teh info on each service as its discovered
  // Need to create space for the array of pointers to each SI set (there is 1 PerServiceMetaData per program in the PMT)
  // then set each pointer in that array to NULL so that its initialised and can be deleted at the taxi's home stage
  // THEN in teh PMT etc parser, when we get a service malloc a PerServiceMetaData spave and store in
  // ServiceInfo_MasterStore.ServiceComponents[i].
  int maxSIEntries = ServiceInfo_MasterStore.MAX_numberServicesInfoStoredFor;
  ServiceInfo_MasterStore.ServiceComponents = (PerServiceMetaData**)(malloc(maxSIEntries * (sizeof(PerServiceMetaData*))));
  for (i = 0; i < maxSIEntries; ++i)
  {
    ServiceInfo_MasterStore.ServiceComponents[i] = NULL;
  }


  pipeline       = gst_pipeline_new ("step1");
  testSource     = gst_element_factory_make("videotestsrc", "testPattern");
  fileSource     = gst_element_factory_make("filesrc", "sourceFile");
  tsparser       = gst_element_factory_make("tsparse", "TSParser");
  h264Videoparser    = gst_element_factory_make("h264parse", "videoParser");

  tsdemuxer      = gst_element_factory_make("tsdemux", "TSDemultiplexer");
  genericDecoder = gst_element_factory_make("decodebin", "videoDecoder");
  videoAdapter   = gst_element_factory_make("videoconvert", "videoMagicBox");
  chromaBlatter  = gst_element_factory_make("chrisFilter",  "chromaBlatter");
  //VideoSink           = gst_element_factory_make("autovideosink", "display");
  VideoSink      = gst_element_factory_make("fakesink", "blackHole");



 

  if (!pipeline || !testSource || !VideoSink)
  {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }


  // set parameter in named element
  g_object_set (G_OBJECT (testSource), "num-buffers", numBuffers, NULL);
  g_object_set (G_OBJECT (fileSource), "location", inputTSFile.c_str(), NULL);
  g_object_set (G_OBJECT (tsdemuxer),  "program-number", 2, NULL);


  //  add a message handler
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, busCall, &MainAppMeta);
  gst_object_unref (bus);

  // add ALL elements into the pipeline in 1 go, whilst cannot link in 1 go can chuck into the bucket
  // file-source | decoder  | converter | screen-output
  gst_bin_add_many (GST_BIN (pipeline), fileSource, tsparser, tsdemuxer, h264Videoparser, genericDecoder,
                    videoAdapter, chromaBlatter, VideoSink,  NULL);

  // Link the elements together, the decoder has sometime pad's which are created only after the
  // decoder starts.  SInce it does not yet exist, need to bind what we can, then add a callback to
  // join the bits together once the decoder makes the final pad available.

  //gst_element_link (fileSource, genericDecoder);
  gst_element_link_many (fileSource, tsparser, tsdemuxer, NULL);
  //gst_element_link_many (fileSource, tsdemuxer, NULL);
  // tsdemxer to genericDecoder is dynamic as the pads are only created as the signal flows through
  gst_element_link_many (videoAdapter, chromaBlatter, VideoSink, NULL);

  g_signal_connect (tsdemuxer, "pad-added", G_CALLBACK (onPadAdded), genericDecoder);
  g_signal_connect (genericDecoder, "pad-added", G_CALLBACK (onPadAdded), videoAdapter);

  guint timedFuncInterval = 1000;
  systemMetaInfo.timerCallsSoFar      = 0;
  systemMetaInfo.pipelineToControl    = pipeline;
  systemMetaInfo.ApplicationMainloop  = loop;
  systemMetaInfo.parserInLine         = h264Videoparser;
  systemMetaInfo.videoDecoder         = genericDecoder;
  systemMetaInfo.theVideoAdapter      = videoAdapter;
  systemMetaInfo.theVideoFakeSink     = VideoSink;

  // create a periodic timer based function that is called to do the housework
  // and any required maintenance tasksm scheduing etc
  g_timeout_add (timedFuncInterval, timedCall , &systemMetaInfo);

  // Set the pipeline to the "playing" state
  g_print ("Now starting the pipeline :\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  // send the main loop off into space to leave the Gstreamer thread running
  // note that "g_main_loop_quit" comes from the "janitor" task
  g_print ("Running...\n");
  g_main_loop_run (loop);


  // now we are out of the main loop, clean up nicely
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

//  show what we found
#if 1
  for (i = 0; i < maxSIEntries; ++i)
  {
    if (ServiceInfo_MasterStore.ServiceComponents[i] != NULL)
    {
        g_print("PMT [%02d]: PCR 0x%04x  V:0x%04x  A:0x%04x Service Name:%s \n", ServiceInfo_MasterStore.ServiceComponents[i]->programNumber,
                                                                                  ServiceInfo_MasterStore.ServiceComponents[i]->PCR_RID,
                                                                                  ServiceInfo_MasterStore.ServiceComponents[i]->VideoPID,
                                                                                  ServiceInfo_MasterStore.ServiceComponents[i]->AudioPID,
                                                                                  ServiceInfo_MasterStore.ServiceComponents[i]->ServiceName);
    }
  }
#endif



  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);

  for (i = 0; i < maxSIEntries; ++i)
  {
    if (ServiceInfo_MasterStore.ServiceComponents[i] != NULL)
    {
      if(ServiceInfo_MasterStore.ServiceComponents[i]->ServiceName != NULL)
      {
          free (ServiceInfo_MasterStore.ServiceComponents[i]->ServiceName);
      }
      free(ServiceInfo_MasterStore.ServiceComponents[i]);
    }
  }
  if(ServiceInfo_MasterStore.ServiceComponents != NULL)
  {
    free(ServiceInfo_MasterStore.ServiceComponents);
  }

  // taxi's home
return 0;
}

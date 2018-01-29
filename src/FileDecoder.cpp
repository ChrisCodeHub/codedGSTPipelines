#include <gst/gst.h>
#include <stdlib.h>     /* atoi */
#include <string.h>
#include <stdbool.h>
#include <iostream>
#include "serviceInfo.h"
#include "localTypes.h"
#include "watchDog.h"

// useful URLS
// https://www.dvb.org/resources/public/standards/a38_dvb-si_specification.pdf
// Link to how to tell what the structures are
// https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-bad-libs/html/mpegts.html
// /usr/include/gstreamer-1.0/gst
//https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-bad-libs/html/gst-plugins-bad-libs-Base-MPEG-TS-sections.html#GstMpegtsSection-struct


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// This program is aimed at testing that the below CLI
// #gst-launch-1.0 -v filesrc location=/stream.ts
//                  ! decodebin !
//                    videoconvert !
//                    autovideosink
// can be made into a C program and still work

static gboolean timedCall2 (gpointer data)
{
  WatchDog *theWatchDog = (WatchDog*)data;
  theWatchDog->wd_JanitorCall();
  return(1);
}



static gboolean busCall ( GstBus     *bus,
                          GstMessage *msg,
                          gpointer    data)
{
  
  if (0 == strcmp(GST_MESSAGE_SRC_NAME(msg),"TSParser"))
  {
    streamServicesInfo *pstreamServicesInfo = (streamServicesInfo*)data;
    pstreamServicesInfo->ParseInfoFromTSFrontEnds(msg);
    //ParseInfoFromTSFrontEnds(msg, (ServiceMetaData *)data);
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
  int numBuffers(500);
  infoAboutMe systemMetaInfo;
  streamServicesInfo *pServiceInfo_MasterStore = new streamServicesInfo;
  std::string inputTSFile("NULL");

  WatchDog *theWatchDog = new WatchDog(1000, &systemMetaInfo);
  

  if (argc == 2){
    inputTSFile = argv[1];
  }
  else{
    std::cout<< "Requires a source TS file to work on"<<std::flush;
    exit(0);
  }

  // Initialize GStreamer
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);
  
  pipeline       = gst_pipeline_new ("step1");
  testSource     = gst_element_factory_make("videotestsrc", "testPattern");
  fileSource     = gst_element_factory_make("filesrc", "sourceFile");
  tsparser       = gst_element_factory_make("tsparse", "TSParser");
  h264Videoparser = gst_element_factory_make("h264parse", "videoParser");

  tsdemuxer      = gst_element_factory_make("tsdemux", "TSDemultiplexer");
  genericDecoder = gst_element_factory_make("decodebin", "videoDecoder");
  videoAdapter   = gst_element_factory_make("videoconvert", "videoMagicBox");
  chromaBlatter  = gst_element_factory_make("chrisFilter",  "chromaBlatter");
  //VideoSink           = gst_element_factory_make("autovideosink", "display");
  VideoSink      = gst_element_factory_make("fakesink", "blackHole");

  if (!pipeline || !testSource || !VideoSink){
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  // set parameter in named element
  g_object_set (G_OBJECT (testSource), "num-buffers", numBuffers, NULL);
  g_object_set (G_OBJECT (fileSource), "location", inputTSFile.c_str(), NULL);
  g_object_set (G_OBJECT (tsdemuxer),  "program-number", 2, NULL);


  //  add a message handler
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, busCall, pServiceInfo_MasterStore);
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

 
  // create a periodic timer based function that is called to do the housework
  // and any required maintenance tasksm scheduing etc
  systemMetaInfo.pipelineToControl    = pipeline;
  systemMetaInfo.ApplicationMainloop  = loop;
  systemMetaInfo.parserInLine         = h264Videoparser;
  systemMetaInfo.videoDecoder         = genericDecoder;
  systemMetaInfo.theVideoAdapter      = videoAdapter;
  systemMetaInfo.theVideoFakeSink     = VideoSink;
  guint idOfTimer2 = g_timeout_add(1000, timedCall2, theWatchDog);

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
  pServiceInfo_MasterStore->ShowStreamSummaries();

  //given teh timers are using "stuff" - kill them off before we start
  // tearing down the eco-system to stop inadvertantly accesing things that
  // have just been deleted
  // g_source_remove(idOfTimer1);
  g_source_remove(idOfTimer2);

  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);

  if(pServiceInfo_MasterStore != NULL)
    delete pServiceInfo_MasterStore;

  if (theWatchDog != NULL)
    delete theWatchDog;
  
  // taxi's home
return 0;
}

#ifndef __LOCAL_TYPES__
#define __LOCAL_TYPES__
#include <gst/gst.h>



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


// below is used in the periodic timer based "Janitor function"
struct infoAboutMe
{
  int timerCallsSoFar;
  GstElement *pipelineToControl;
  GMainLoop  *ApplicationMainloop;
  GstElement *parserInLine;
  GstElement *videoDecoder;
  GstElement *theVideoAdapter;
  GstElement *theVideoFakeSink;
  infoAboutMe(){
    timerCallsSoFar = 0;
    pipelineToControl = NULL;
    ApplicationMainloop = NULL;
    parserInLine = NULL;
    videoDecoder = NULL;
    theVideoAdapter = NULL;
    theVideoFakeSink = NULL;
  }
};

#endif
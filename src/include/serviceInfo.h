#ifndef __SERVICE_INFO__
#define __SERVICE_INFO__
#include <gst/gst.h>
#include "localTypes.h"


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


#endif
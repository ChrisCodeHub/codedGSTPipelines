#ifndef __SERVICE_INFO__
#define __SERVICE_INFO__
#include <gst/gst.h>
#include "localTypes.h"
#include <iostream>



struct PerServiceInfo{
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
};


// create an array of 40 pointers to "PerServiceMetaData" which will hold teh info on each service as its discovered
// Need to create space for the array of pointers to each SI set (there is 1 PerServiceMetaData per program in the PMT)
// then set each pointer in that array to NULL so that its initialised and can be deleted at the taxi's home stage
// THEN in teh PMT etc parser, when we get a service malloc a PerServiceMetaData spave and store in
// ServiceInfo_MasterStore.ServiceComponents[i].

class streamServicesInfo
{
  public:

    streamServicesInfo(void);
    ~streamServicesInfo();
    void ParseInfoFromTSFrontEnds( GstMessage *msg);
    void ShowStreamSummaries(void);


  private:
    bool  haveSeenPAT;
    bool  haveSeenPMT;
    bool  haveSeenSDT;
    guint16 numberServicesInfoStoredFor;
    guint16 MAX_numberServicesInfoStoredFor;
    PerServiceInfo  **ServiceComponents;
};

#endif
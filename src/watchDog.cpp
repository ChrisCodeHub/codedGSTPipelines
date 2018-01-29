#include <gst/gst.h>
#include <stdlib.h>     /* atoi */
#include <string.h>
#include <stdbool.h>
#include <iostream>
#include "localTypes.h"
#include "watchDog.h"


WatchDog::WatchDog( unsigned int CallingInterval, 
                    infoAboutMe *ecoSystemInfo){

    callsSoFar = 0;
    localEcoSystemInfo = ecoSystemInfo;
    std::cout<< "Made a watchDog, hopefully it barks as needed" << std::endl;
 }

 WatchDog::~WatchDog(){

     std::cout<<"WatchDog has been killed - do you need to worry ???????"<<std::endl;

 }

int WatchDog::wd_JanitorCall(void){

    std::cout<<"WatchDog barks"<<std::endl; 
    callsSoFar++;

    std::cout<<" EcoSystem Info calls "<< callsSoFar << std::endl;
    if(callsSoFar == 5)
    {
        RunInterogation();
    }

    if(callsSoFar == 5)
    {
        std::cout<<" Setting pipeline state to NULL"<<std::endl;
        gst_element_set_state (localEcoSystemInfo->pipelineToControl, GST_STATE_NULL);
    }
    if(callsSoFar == 8)
    {
        std::cout<<" quitting the pipeline main loop"<<std::endl;
        g_main_loop_quit(localEcoSystemInfo->ApplicationMainloop);
    }
}



void WatchDog::RunInterogation(void)
{
    GstPad* VideoAdapterOutputPin;
    GstCaps* capsOfVideoOutputPin;
    std::cout<<" Checking pin caps "<<std::endl;;
    VideoAdapterOutputPin = gst_element_get_static_pad (localEcoSystemInfo->theVideoAdapter, "sink");
    if (VideoAdapterOutputPin == NULL)
    {
        std::cout<<" PIN IS NULL "<<std::endl;
    }
    else
    {
        gboolean isActive = gst_pad_is_active (VideoAdapterOutputPin);
        (isActive == TRUE)  ? std::cout<< " PIN IS ACTIVE "<<std::endl
                            : std::cout<< " PIN IS ASLEEP  "<<std::endl;
    }
    capsOfVideoOutputPin = gst_pad_get_current_caps(VideoAdapterOutputPin);
    if (capsOfVideoOutputPin == NULL)
    {
        std::cout<<" CAPS IS NULL"<<std::endl;
    }
    else
    {
        gint width, height;
        const GstStructure *str;
        str = gst_caps_get_structure (capsOfVideoOutputPin, 0);
        if (!gst_structure_get_int (str, "width", &width) ||
            !gst_structure_get_int (str, "height", &height))
        {
            std::cout<<"No width/height available"<<std::endl;
        }
        else
        {
            std::cout<<"video is "<<width<<"x"<< height<<std::endl;
        }
    }
}

// stuff to read capabilties to see what is flowingthrough the system
void WatchDog::readInfoFromPinCaps (GstCaps *caps)
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
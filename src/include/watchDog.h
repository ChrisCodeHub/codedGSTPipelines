#ifndef __WATCHDOG_H_
#define __WATCHDOG_H_

#include <gst/gst.h>
#include <stdlib.h>     /* atoi */
#include <string.h>
#include <stdbool.h>
#include <iostream>
#include "localTypes.h"

class WatchDog {


public:
    WatchDog(   unsigned int CallingInterval, 
                infoAboutMe *ecoSystemInfo);
    ~WatchDog();

    int wd_JanitorCall(void);

private:
    void readInfoFromPinCaps (GstCaps *caps);
    void RunInterogation(void);

    infoAboutMe *localEcoSystemInfo;
    unsigned int callsSoFar;

};


#endif

#ifndef MYAPPLICATION_NEXTPROXYMONITOR_HH
#define MYAPPLICATION_NEXTPROXYMONITOR_HH

#ifndef USAGEENVIRONMENT_HH
#define USAGEENVIRONMENT_HH
#include <UsageEnvironment.hh>
#endif//USAGEENVIRONMENT_HH

#ifndef MEDIASESSION_HH
#define MEDIASESSION_HH
#include <MediaSession.hh>
#include <RTSPServer.hh>

#endif//MEDIASESSION_HH

class NextProxyMonitor {
public:
    static NextProxyMonitor* createNew(RTSPServer* rtspServer, MediaSession* clientMediaSession);
    void checkInterPacketGaps(void* clientData);


protected:
    NextProxyMonitor(RTSPServer* rtspServer, MediaSession* clientMediaSession);
    ~NextProxyMonitor();

private:
    RTSPServer* rtspServer;
    MediaSession* clientMediaSession;
    unsigned interPacketGapMaxTime = 5;//in seceonds
    unsigned totNumPacketsReceived = ~0; // used if checking inter-packet gaps
    TaskToken proxyClientDescribeCompletenessCheckTask;
    TaskToken interPacketGapCheckTimerTask = NULL;
    void scheduleNext();

};

#endif //MYAPPLICATION_NEXTPROXYMONITOR_HH

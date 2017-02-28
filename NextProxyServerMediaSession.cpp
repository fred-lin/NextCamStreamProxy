#include <H264VideoFileSink.hh>
#include "NextProxyServerMediaSession.hh"

//NextProxyServerMediaSession implementation
NextProxyServerMediaSession* NextProxyServerMediaSession::createNew(UsageEnvironment &env,
                                                                    GenericMediaServer *ourMediaServer,
                                                                    char const *inputStreamURL,
                                                                    portNumBits tunnelOverHTTPPortNum,
                                                                    const char *streamName,
                                                                    const char *username,
                                                                    const char *password,
                                                                    int verbosityLevel) {
    return new NextProxyServerMediaSession(env, ourMediaServer, inputStreamURL, streamName, username, password, tunnelOverHTTPPortNum, verbosityLevel);
}

NextProxyServerMediaSession::NextProxyServerMediaSession(UsageEnvironment &env,
                                                         GenericMediaServer *ourMediaServer,
                                                         char const *inputStreamURL,
                                                         const char *streamName, const char *username,
                                                         char const *password,
                                                         portNumBits tunnelOverHTTPPortNum,
                                                         int verbosityLevel)
        : ProxyServerMediaSession(env, ourMediaServer, inputStreamURL,
                                  streamName, username, password, tunnelOverHTTPPortNum,
                                  verbosityLevel, -1, NULL, createNewOurProxyRTSPClientFunc, 6970, False) {
    /*fProxyRTSPClient = createNewOurProxyRTSPClientFunc(*this, inputStreamURL, username, password,
                                                       tunnelOverHTTPPortNum,
                                                       verbosityLevel > 0 ? verbosityLevel-1 : verbosityLevel,
                                                       6970);*/
}

RTCPInstance* NextProxyServerMediaSession::createRTCP(Groupsock *RTCPgs, unsigned totSessionBW,
                                                      unsigned char const *cname, RTPSink *sink) {

    proxyServerRTCPInstance = ProxyServerMediaSession::createRTCP(RTCPgs, totSessionBW, cname, sink);
    return proxyServerRTCPInstance;
}

MediaSession* NextProxyServerMediaSession::getClientMediaSession() {
    return fClientMediaSession;
}

RTSPClient* NextProxyServerMediaSession::getRTSPClient() {
    return fProxyRTSPClient;
}

RTCPInstance* NextProxyServerMediaSession::getRTCPInstance() {
    return proxyServerRTCPInstance;
}

//NextProxyRTSPClient implementation

NextProxyRTSPClient::NextProxyRTSPClient(class ProxyServerMediaSession &ourProxyServerMediaSession,
                                         char const *rtspURL, char const *username,
                                         char const *password, portNumBits tunnelOverHTTPPortNum,
                                         int verbosityLevel, int socketNumToServer)
        : ProxyRTSPClient(ourProxyServerMediaSession, rtspURL, username, password,
                          tunnelOverHTTPPortNum, verbosityLevel, socketNumToServer){ }


ProxyRTSPClient* createNewOurProxyRTSPClientFunc(ProxyServerMediaSession& ourServerMediaSession,
                                                 char const* rtspURL,
                                                 char const* username, char const* password,
                                                 portNumBits tunnelOverHTTPPortNum, int verbosityLevel,
                                                 int socketNumToServer) {
    return new NextProxyRTSPClient(ourServerMediaSession, rtspURL, username, password,
                                   tunnelOverHTTPPortNum, verbosityLevel, socketNumToServer);
}

void NextProxyServerMediaSession::createLocalFile(MediaSubsession subsession, char const* fileName) {
    FileSink* fileSink = NULL;
    fileSink = H264VideoFileSink::createNew(fOurMediaServer->envir(), subsession.fmtp_spropparametersets(), fileName);
}

/*void NextProxyServerMediaSession::checkInterPacketGaps(void*) {
    if (interPacketGapMaxTime == 0) return; // we're not checking

    // Check each subsession, counting up how many packets have been received:
    unsigned newTotNumPacketsReceived = 0;

    MediaSubsessionIterator iter((MediaSession&)fClientMediaSession);
    MediaSubsession* subsession;
    while ((subsession = iter.next()) != NULL) {
        RTPSource* src = subsession->rtpSource();
        if (src == NULL) continue;
        newTotNumPacketsReceived += src->receptionStatsDB().totNumPacketsReceived();
    }

    if (newTotNumPacketsReceived == totNumPacketsReceived) {
        // No additional packets have been received since the last time we
        // checked, so end this stream:

        interPacketGapCheckTimerTask = NULL;
        //notifyBackendDead();

    } else {
        totNumPacketsReceived = newTotNumPacketsReceived;
        // Check again, after the specified delay:
        interPacketGapCheckTimerTask
                = ((RTSPServer*)fOurMediaServer)->envir().taskScheduler().scheduleDelayedTask(interPacketGapMaxTime*1000000,
                                                                          (TaskFunc*) checkInterPacketGaps, NULL);
    }
}

void NextProxyServerMediaSession::checkProxyClientDescribeCompleteness(void*) {

    if(describeCompletedSuccessfully()) {
        proxyClientDescribeCompletenessCheckTask = fOurMediaServer->envir().taskScheduler().scheduleDelayedTask(2, (TaskFunc*) checkProxyClientDescribeCompleteness, NULL);
    } else {
        //notifyProxyStatusChanged();
    }

}*/
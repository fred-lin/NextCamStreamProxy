//
// Created by cheng-lun on 17年1月17日.
//

#ifndef MYAPPLICATION_NEXTPROXYSERVERMEDIASESSION_HH
#define MYAPPLICATION_NEXTPROXYSERVERMEDIASESSION_HH

#ifndef PROXYSERVERMEDIASESSION_HH
#define PROXYSERVERMEDIASESSION_HH
#include "ProxyServerMediaSession.hh"
#endif //PROXYSERVERMEDIASESSION_HH

class NextProxyRTSPClient: public ProxyRTSPClient {
public:
    NextProxyRTSPClient(class ProxyServerMediaSession& ourProxyServerMediaSession, char const* rtspURL,
                       char const* username, char const* password,
                       portNumBits tunnelOverHTTPPortNum, int verbosityLevel, int socketNumToServer);
    //void continueAfterDESCRIBE(char const* sdpDescription);
    //void continueAfterSETUP(int resultCode);
private:
    friend class NextProxyServerMediaSession;
    //static void sendDESCRIBE(void* clientData);
};

ProxyRTSPClient* createNewOurProxyRTSPClientFunc(ProxyServerMediaSession& ourServerMediaSession,
                                                 char const* rtspURL,
                                                 char const* username, char const* password,
                                                 portNumBits tunnelOverHTTPPortNum, int verbosityLevel,
                                                 int socketNumToServer);

class NextProxyServerMediaSession: public ProxyServerMediaSession {
public:
    static NextProxyServerMediaSession* createNew(UsageEnvironment& env,
                                                 GenericMediaServer* ourMediaServer, // Note: We can be used by just one server
                                                 char const* inputStreamURL, // the "rtsp://" URL of the stream we'll be proxying
                                                 char const* streamName = NULL,
                                                 char const* username = NULL, char const* password = NULL,
                                                 portNumBits tunnelOverHTTPPortNum = 0,
            // for streaming the *proxied* (i.e., back-end) stream
                                                 int verbosityLevel = 0);

protected:
    NextProxyServerMediaSession(UsageEnvironment& env,
                               GenericMediaServer* ourMediaServer, // Note: We can be used by just one server
                               char const* inputStreamURL, // the "rtsp://" URL of the stream we'll be proxying
                               char const* streamName = NULL,
                               char const* username = NULL, char const* password = NULL,
                               portNumBits tunnelOverHTTPPortNum = 0,
            // for streaming the *proxied* (i.e., back-end) stream
                               int verbosityLevel = 0);

    RTCPInstance* createRTCP(Groupsock* RTCPgs, unsigned totSessionBW, /* in kbps */
                             unsigned char const* cname, RTPSink* sink);

private:
    RTCPInstance* proxyServerRTCPInstance;
public:
    MediaSession* getClientMediaSession();
    RTSPClient* getRTSPClient();
    RTCPInstance* getRTCPInstance();

};

#endif //MYAPPLICATION_NEXTPROXYSERVERMEDIASESSION_HH
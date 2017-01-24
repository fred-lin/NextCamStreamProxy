#include "NextProxyServerMediaSession.hh"

//NextProxyServerMediaSession implementation
NextProxyServerMediaSession* NextProxyServerMediaSession::createNew(UsageEnvironment &env,
                                                                    GenericMediaServer *ourMediaServer,
                                                                    char const *inputStreamURL,
                                                                    const char *streamName,
                                                                    const char *username,
                                                                    const char *password,
                                                                    portNumBits tunnelOverHTTPPortNum,
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
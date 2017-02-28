#include <NextProxyMonitor.hh>
#include <MediaSession.hh>

NextProxyMonitor* NextProxyMonitor::createNew(RTSPServer* rtspServer, MediaSession *clientMediaSession) {
    return new NextProxyMonitor(rtspServer, clientMediaSession);
}

NextProxyMonitor::NextProxyMonitor(RTSPServer* rtspServer, MediaSession *clientMediaSession):
rtspServer(rtspServer){

}

NextProxyMonitor::~NextProxyMonitor() {
    rtspServer->envir().taskScheduler().unscheduleDelayedTask(interPacketGapCheckTimerTask);
}

void NextProxyMonitor::checkInterPacketGaps(void *clientData) {
    if (interPacketGapMaxTime == 0) return; // we're not checking

    // Check each subsession, counting up how many packets have been received:
    unsigned newTotNumPacketsReceived = 0;

    MediaSubsessionIterator iter(*clientMediaSession);
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
        //notifyProxyStatusChanged();

    } else {
        totNumPacketsReceived = newTotNumPacketsReceived;
        // Check again, after the specified delay:
        scheduleNext();
        /*interPacketGapCheckTimerTask
                = rtspServer->envir().taskScheduler().scheduleDelayedTask(interPacketGapMaxTime*1000000,
                                                                          (TaskFunc*)checkInterPacketGaps, NULL);*/
    }

}


void NextProxyMonitor::scheduleNext() {

}
#include <pthread.h>

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "NextProxyServerMediaSession.hh"
#include "QosMeasurementRecord.hh"

RTSPServer* rtspServer = NULL;

char eventLoopWatchVariable = 0;
void printQOSData();
void addToStreamClientState();
void beginQOSMeasurement();
TaskToken receiverReportTimerTask = NULL;
TaskToken interPacketGapCheckTimerTask = NULL;
unsigned interPacketGapMaxTime = 5;//in seceonds
unsigned totNumPacketsReceived = ~0; // used if checking inter-packet gaps
unsigned qosMeasurementIntervalMS = 1000;
static qosMeasurementRecord* qosRecordHead = NULL;
/*static*/ void periodicQOSMeasurement(void* clientData); // forward
/*static*/ unsigned nextQOSMeasurementUSecs;
void proxyStatusChanged(int messageCode);

typedef struct stream_client_state {
    MediaSession* mediaSession;
    MediaSession* clientMediaSession;
} StreamClientState;
StreamClientState streamState;

typedef struct proxy_context {
    pthread_mutex_t lock;
} ProxyContext;
ProxyContext proxyContext;

static  RTSPServer* createRTSPServer(UsageEnvironment* usageEnv) {
    UserAuthenticationDatabase* authDB = NULL;
    portNumBits rtspServerPortNum = 20001;
    //portNumBits rtspServerPortNum = (portNumBits) rand() % 100 + 20000;
    return RTSPServer::createNew(*usageEnv, (Port) rtspServerPortNum, authDB);
}

void addToStreamClientState() {
    MediaSession* mediaSession = ((NextProxyServerMediaSession*) streamState.mediaSession)->getClientMediaSession();
    streamState.clientMediaSession = mediaSession;
}

void *doEventLoopFunc(void* context) {

    int message = PROXY_INITIALIZING;
    proxyStatusChanged(message);

    rtspServer->envir().taskScheduler().doEventLoop(&((NextProxyServerMediaSession*) streamState.mediaSession)->describeCompletedFlag);
    addToStreamClientState();

    message = PROXY_READY;
    proxyStatusChanged(message);

    rtspServer->envir().taskScheduler().doEventLoop(&eventLoopWatchVariable);

    return context;
}

void shutdownServer(RTSPServer *rtspServer) {
    if(rtspServer != NULL) {
        eventLoopWatchVariable = 1;

        rtspServer->envir().taskScheduler().unscheduleDelayedTask(receiverReportTimerTask);
        rtspServer->envir().taskScheduler().unscheduleDelayedTask(interPacketGapCheckTimerTask);

        printQOSData();

        Medium::close(rtspServer);

        streamState.clientMediaSession = NULL;
        streamState.mediaSession = NULL;
        qosRecordHead = NULL;
        rtspServer->envir().reclaim(); //usageEnv = NULL;
        eventLoopWatchVariable = 0;
    }
}

void scheduleNextQOSMeasurement() {
    nextQOSMeasurementUSecs += qosMeasurementIntervalMS*1000;
    struct timeval timeNow;
    gettimeofday(&timeNow, NULL);
    unsigned timeNowUSecs = timeNow.tv_sec*1000000 + timeNow.tv_usec;
    int usecsToDelay = nextQOSMeasurementUSecs - timeNowUSecs;

    receiverReportTimerTask = rtspServer->envir().taskScheduler().scheduleDelayedTask(
            usecsToDelay, (TaskFunc*)periodicQOSMeasurement, (void*)NULL);
}

void periodicQOSMeasurement(void* /*clientData*/) {
    struct timeval timeNow;
    gettimeofday(&timeNow, NULL);

    for (qosMeasurementRecord* qosRecord = qosRecordHead;
         qosRecord != NULL; qosRecord = qosRecord->fNext) {
        qosRecord->periodicQOSMeasurement(timeNow);
    }

    // Do this again later:
    scheduleNextQOSMeasurement();
}

void beginQOSMeasurement() {
    // Set up a measurement record for each active subsession:
    struct timeval startTime;
    gettimeofday(&startTime, NULL);
    nextQOSMeasurementUSecs = startTime.tv_sec*1000000 + startTime.tv_usec;
    qosMeasurementRecord* qosRecordTail = NULL;
    MediaSubsessionIterator iter(*streamState.clientMediaSession);
    MediaSubsession* subsession;
    while ((subsession = iter.next()) != NULL) {
        RTPSource* src = subsession->rtpSource();
        if (src == NULL) continue;

        qosMeasurementRecord* qosRecord
                = new qosMeasurementRecord(startTime, src);
        if (qosRecordHead == NULL) qosRecordHead = qosRecord;
        if (qosRecordTail != NULL) qosRecordTail->fNext = qosRecord;
        qosRecordTail  = qosRecord;
    }

    // Then schedule the first of the periodic measurements:
    scheduleNextQOSMeasurement();
}

void printQOSData() {

    // Print out stats for each active subsession:
    qosMeasurementRecord* curQOSRecord = qosRecordHead;
    if (streamState.clientMediaSession != NULL) {
        MediaSubsessionIterator iter(*streamState.clientMediaSession);
        MediaSubsession* subsession;
        while ((subsession = iter.next()) != NULL) {
            RTPSource* src = subsession->rtpSource();
            if (src == NULL) continue;

            //*env << "subsession\t" << subsession->mediumName()
            //<< "/" << subsession->codecName() << "\n";

            unsigned numPacketsReceived = 0, numPacketsExpected = 0;

            if (curQOSRecord != NULL) {
                numPacketsReceived = curQOSRecord->totNumPacketsReceived;
                numPacketsExpected = curQOSRecord->totNumPacketsExpected;
            }

            //Log these statistics for debugging
            //"num_packets_received %d", numPacketsReceived
            //"num_packets_lost %d", int(numPacketsExpected - numPacketsReceived)

            if (curQOSRecord != NULL) {
                unsigned secsDiff = curQOSRecord->measurementEndTime.tv_sec
                                    - curQOSRecord->measurementStartTime.tv_sec;
                int usecsDiff = curQOSRecord->measurementEndTime.tv_usec
                                - curQOSRecord->measurementStartTime.tv_usec;
                double measurementTime = secsDiff + usecsDiff/1000000.0;

                //Log these statistics for debugging
                //"elapsed_measurement_time %f", measurementTime
                //"kBytes_received_total %f", curQOSRecord->kBytesTotal

                if (curQOSRecord->kbits_per_second_max == 0) {
                    // no record
                    // Do nothing
                } else {
                    //Log these statistics for debugging
                    //"kbits_per_second_min %f", curQOSRecord->kbits_per_second_min
                    //"kbits_per_second_ave %f", (measurementTime == 0.0 ? 0.0 : 8*curQOSRecord->kBytesTotal/measurementTime)
                    //"kbits_per_second_max %f", curQOSRecord->kbits_per_second_max
                }

                //Log these statistics for debugging
                //"packet_loss_percentage_min %f", 100*curQOSRecord->packet_loss_fraction_min
                double packetLossFraction = numPacketsExpected == 0 ? 1.0
                                                                    : 1.0 - numPacketsReceived/(double)numPacketsExpected;
                if (packetLossFraction < 0.0) packetLossFraction = 0.0;
                //Log these statistics for debugging
                //"packet_loss_percentage_ave %f", 100*packetLossFraction
                //"packet_loss_percentage_max %f", (packetLossFraction == 1.0 ? 100.0 : 100*curQOSRecord->packet_loss_fraction_max)

                RTPReceptionStatsDB::Iterator statsIter(src->receptionStatsDB());
                // Assume that there's only one SSRC source (usually the case):
                RTPReceptionStats* stats = statsIter.next(True);
                if (stats != NULL) {
                    //Log these statistics for debugging
                    //"inter_packet_gap_ms_min %f", stats->minInterPacketGapUS()/1000.0
                    struct timeval totalGaps = stats->totalInterPacketGaps();
                    double totalGapsMS = totalGaps.tv_sec*1000.0 + totalGaps.tv_usec/1000.0;
                    unsigned totNumPacketsReceived = stats->totNumPacketsReceived();
                    //Log these statistics for debugging
                    //"inter_packet_gap_ms_ave %f", (totNumPacketsReceived == 0 ? 0.0 : totalGapsMS/totNumPacketsReceived)
                    //"inter_packet_gap_ms_max %f", stats->maxInterPacketGapUS()/1000.0
                }

                curQOSRecord = curQOSRecord->fNext;
            }
        }
    }

    delete qosRecordHead;
}

void checkInterPacketGaps() {
    if (interPacketGapMaxTime == 0) return; // we're not checking

    // Check each subsession, counting up how many packets have been received:
    unsigned newTotNumPacketsReceived = 0;

    MediaSubsessionIterator iter(*streamState.clientMediaSession);
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

        int message = PROXY_BACK_END_NO_RESPONSE;
        proxyStatusChanged(message);

    } else {
        totNumPacketsReceived = newTotNumPacketsReceived;
        // Check again, after the specified delay:
        interPacketGapCheckTimerTask
                = rtspServer->envir().taskScheduler().scheduleDelayedTask(interPacketGapMaxTime*1000000,
                                                                          (TaskFunc*)checkInterPacketGaps, NULL);
    }
}

void proxyStatusChanged(int messageCode) {

    //
    // call front-end video player to do relative action
    //

    switch(messageCode) {

        case PROXY_BACK_END_NO_RESPONSE:

            break;

        case PROXY_INITIALIZING:

            break;

        case PROXY_READY:

            break;

        default:

            break;

    }

}

char startRtspProxy(char* streamUrl) {
    OutPacketBuffer::maxSize = 100000;

    UsageEnvironment* usageEnv;
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    usageEnv = BasicUsageEnvironment::createNew(*scheduler);

    portNumBits tunnelOverHTTPPortNum = 0;

    rtspServer = createRTSPServer(usageEnv);

    char const* proxiedStreamUrl = streamUrl;

    // for TCP back-end connection
    tunnelOverHTTPPortNum = (portNumBits)(~0);

    NextProxyServerMediaSession* sms = NextProxyServerMediaSession::createNew(*usageEnv, rtspServer, proxiedStreamUrl, tunnelOverHTTPPortNum);

    streamState.mediaSession = (MediaSession*) sms;

    rtspServer->addServerMediaSession(sms);

    pthread_attr_t threadAttr_;
    pthread_attr_init(&threadAttr_);
    pthread_attr_setdetachstate(&threadAttr_, PTHREAD_CREATE_DETACHED);
    pthread_mutex_init(&proxyContext.lock, NULL);
    pthread_t looperThread;
    pthread_create(&looperThread, &threadAttr_, &doEventLoopFunc, &proxyContext);

    return *rtspServer->rtspURL((ServerMediaSession*) streamState.mediaSession);
}

void startStreamingMeasurement() {
    checkInterPacketGaps();
    //beginQOSMeasurement();
}

void shutdownStream() {
    shutdownServer(rtspServer);
}

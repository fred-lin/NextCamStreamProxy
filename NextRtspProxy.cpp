#include <pthread.h>

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "NextProxyServerMediaSession.hh"
#include "QosMeasurementRecord.hh"

UsageEnvironment* usageEnv;

Boolean proxyREGISTERRequests = False;
UserAuthenticationDatabase* authDB = NULL;
UserAuthenticationDatabase* authDBForREGISTER = NULL;
RTSPServer* rtspServer;

int verbosityLevel = 0;
Boolean streamRTPOverTCP = False;
portNumBits tunnelOverHTTPPortNum = 0;
portNumBits rtspServerPortNum = 20001;
char* username = NULL;
char* password = NULL;
char eventLoopWatchVariable = 0;
char stop = 1;
TaskToken receiverReportTimerTask;
void printQOSData(int exitCode);
void addToStreamClientState();
void beginQOSMeasurement();

#define LOG_TAG "LIVE555"

#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, \
                                             __VA_ARGS__))

#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, \
                                             __VA_ARGS__))

typedef struct stream_client_state {
    MediaSession* mediaSession;
    MediaSession* clientMediaSession;
} StreamClientState;
StreamClientState streamState;

typedef struct proxy_context {
    pthread_mutex_t lock;
} ProxyContext;
ProxyContext proxyContext;

static  RTSPServer* createRTSPServer(Port port) {
    return RTSPServer::createNew(*usageEnv, port, authDB);
}


void *doEventLoopFunc(void* context) {

    usageEnv->taskScheduler().doEventLoop(&((NextProxyServerMediaSession*) streamState.mediaSession)->describeCompletedFlag);

    //
    // Send message to player to start playing here!
    //

    usageEnv->taskScheduler().doEventLoop(&eventLoopWatchVariable);

    return context;
}

void shutdownServer(RTSPServer *rtspServer) {
    if(usageEnv != NULL) {
        usageEnv->taskScheduler().unscheduleDelayedTask(receiverReportTimerTask);
    }
    printQOSData();

    ((NextProxyServerMediaSession*) streamState.mediaSession)->getfPresentationTimeSessionNormalizer()->getPresentationTimeSubsessionNormalizer()->detachInputSource();

    if(streamState.clientMediaSession != NULL) {
        MediaSubsessionIterator iterator(*streamState.clientMediaSession);
        MediaSubsession* subsession;
        while((subsession = iterator.next()) != NULL) {
            //subsession->rtpSource()->stopGettingFrames();
            Medium::close(subsession->sink);
            subsession->sink = NULL;
        }
    }

    Medium::close(rtspServer);

    pthread_mutex_destroy(&proxyContext.lock);

    usageEnv->reclaim(); usageEnv = NULL;
    //delete scheduler; scheduler = NULL;
}

void addToStreamClientState() {
    MediaSession* mediaSession = ((NextProxyServerMediaSession*) streamState.mediaSession)->getClientMediaSession();
    streamState.clientMediaSession = mediaSession;
}

void sendAppPacket() {
    //streamState.mediaSubsessionIterator->next()->rtcpInstance()->sendAppPacket()
}

char startRtspProxy(char* streamUrl) {
    OutPacketBuffer::maxSize = 100000;

    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    usageEnv = BasicUsageEnvironment::createNew(*scheduler);

    rtspServer = createRTSPServer(rtspServerPortNum);

    char const* proxiedStreamUrl = streamUrl;

    // for TCP back-end connection
    streamRTPOverTCP = True;
    //tunnelOverHTTPPortNum = (portNumBits)(~0);

    char streamName[30];
    sprintf(streamName, "%s", "proxyStream");

    NextProxyServerMediaSession* sms = NextProxyServerMediaSession::createNew(*usageEnv, rtspServer, proxiedStreamUrl, streamName, username, password, tunnelOverHTTPPortNum, verbosityLevel);

    streamState.mediaSession = (MediaSession*) sms;

    rtspServer->addServerMediaSession(sms);

    //char* proxyStreamUrl = rtspServer->rtspURL(sms);

    pthread_attr_t threadAttr_;

    pthread_attr_init(&threadAttr_);
    pthread_attr_setdetachstate(&threadAttr_, PTHREAD_CREATE_DETACHED);

    pthread_mutex_init(&proxyContext.lock, NULL);

    pthread_t looperThread;
    pthread_create(&looperThread, &threadAttr_, &doEventLoopFunc, &proxyContext);

    return *rtspServer->rtspURL(sms);
}

void shutdownStream() {
    shutdownServer(rtspServer);
}

unsigned qosMeasurementIntervalMS = 1000;

qosMeasurementRecord* qosRecordHead = NULL;

void periodicQOSMeasurement(void* clientData); // forward

unsigned nextQOSMeasurementUSecs;

void scheduleNextQOSMeasurement() {
    nextQOSMeasurementUSecs += qosMeasurementIntervalMS*1000;
    struct timeval timeNow;
    gettimeofday(&timeNow, NULL);
    unsigned timeNowUSecs = timeNow.tv_sec*1000000 + timeNow.tv_usec;
    int usecsToDelay = nextQOSMeasurementUSecs - timeNowUSecs;

    receiverReportTimerTask = usageEnv->taskScheduler().scheduleDelayedTask(
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

void startStreamingMeasurement() {
    addToStreamClientState();
    beginQOSMeasurement();
}

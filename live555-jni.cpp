#include <jni.h>
#include <android/log.h>
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
void checkProxyClientDescribeCompleteness();
TaskToken receiverReportTimerTask = NULL;
TaskToken interPacketGapCheckTimerTask = NULL;
TaskToken proxyClientDescribeCompletenessCheckTask = NULL;
void checkInterPacketGaps();
unsigned interPacketGapMaxTime = 5;//in seceonds
unsigned totNumPacketsReceived = ~0; // used if checking inter-packet gaps
unsigned qosMeasurementIntervalMS = 1000;
static qosMeasurementRecord* qosRecordHead = NULL;
/*static*/ void periodicQOSMeasurement(void* clientData); // forward
/*static*/ unsigned nextQOSMeasurementUSecs;

#define LOG_TAG "LIVE555"

#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, \
                                             __VA_ARGS__))

#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, \
                                             __VA_ARGS__))

#define FUNC(RETURN_TYPE, NAME, ...) \
  extern "C" { \
  JNIEXPORT RETURN_TYPE \
    Java_io_nextdrive_proxyserver_MainActivity_ ## NAME \
      (JNIEnv* env, jobject thiz, ##__VA_ARGS__);\
  } \
  JNIEXPORT RETURN_TYPE \
    Java_io_nextdrive_proxyserver_MainActivity_ ## NAME \
      (JNIEnv* env, jobject thiz, ##__VA_ARGS__)\

typedef struct stream_client_state {
    MediaSession* mediaSession;
    MediaSession* clientMediaSession;
} StreamClientState;
StreamClientState streamState;

typedef struct proxy_context {
    JavaVM *javaVM;
    jclass mainActivityClazz;
    jobject  mainActivityObject;
    pthread_mutex_t lock;
} ProxyContext;
ProxyContext proxyContext;

/*static*/ RTSPServer* createRTSPServer(UsageEnvironment* usageEnv) {
    UserAuthenticationDatabase* authDB = NULL;
    portNumBits rtspServerPortNum = 20001;
    //portNumBits rtspServerPortNum = (portNumBits) rand() % 100 + 20000;
    return RTSPServer::createNew(*usageEnv, (Port) rtspServerPortNum, authDB);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    memset(&proxyContext, 0, sizeof(proxyContext));

    proxyContext.javaVM = vm;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR; // JNI version not supported.
    }

    jclass  clz = env->FindClass("io/nextdrive/proxyserver/MainActivity");
    proxyContext.mainActivityClazz = reinterpret_cast<jclass>(env->NewGlobalRef(clz));

    jmethodID  jniHelperCtor = env->GetMethodID(proxyContext.mainActivityClazz, "<init>", "()V");
    jobject    handler = env->NewObject(proxyContext.mainActivityClazz,
                                        jniHelperCtor);
    proxyContext.mainActivityObject = env->NewGlobalRef(handler);

    return  JNI_VERSION_1_6;
}

void sendJavaMsg(JNIEnv *env, jobject instance, jmethodID func, const char* msg) {
    jstring javaMsg = env->NewStringUTF(msg);
    env->CallVoidMethod(instance, func, javaMsg);
    env->DeleteLocalRef(javaMsg);
}

void notifyBackendDead(int msg) {
    int messageCode = msg;

    JavaVM *javaVM = proxyContext.javaVM;
    JNIEnv *env;
    jint res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (res != JNI_OK) {
        res = javaVM->AttachCurrentThread(&env, NULL);
        if (JNI_OK != res) {
            LOGE("Failed to AttachCurrentThread, ErrorCode = %d", res);
            return;
        }
    }

    jmethodID methodId = env->GetMethodID(proxyContext.mainActivityClazz, "callbackReceived", "(I)V");
    env->CallVoidMethod(proxyContext.mainActivityObject, methodId, messageCode);

    javaVM->DetachCurrentThread();
}

void *doEventLoopFunc(void* context) {

    int message = PROXY_INITIALIZING;
    notifyBackendDead(message);

    /*ProxyContext *pContext = (ProxyContext*) context;
    JavaVM *javaVM = pContext->javaVM;
    JNIEnv *env;
    jint res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (res != JNI_OK) {
        res = javaVM->AttachCurrentThread(&env, NULL);
        if (JNI_OK != res) {
            LOGE("Failed to AttachCurrentThread, ErrorCode = %d", res);
            return NULL;
        }
    }

    jmethodID statusId = env->GetMethodID(pContext->mainActivityClazz,
                                          "serverStarted",
                                          "(Ljava/lang/String;)V");*/

    rtspServer->envir().taskScheduler().doEventLoop(&((NextProxyServerMediaSession*) streamState.mediaSession)->describeCompletedFlag);

    message = PROXY_READY;
    notifyBackendDead(message);
    /*sendJavaMsg(env, pContext->mainActivityObject, statusId, "Good!");

    javaVM->DetachCurrentThread();*/

    addToStreamClientState();

    LOGD("PLAY");
    rtspServer->envir().taskScheduler().doEventLoop(&eventLoopWatchVariable);

    return context;
}

void shutdownServer(RTSPServer *rtspServer) {
    if(rtspServer != NULL) {
        eventLoopWatchVariable = 1;

        rtspServer->envir().taskScheduler().unscheduleDelayedTask(receiverReportTimerTask);
        rtspServer->envir().taskScheduler().unscheduleDelayedTask(interPacketGapCheckTimerTask);
        rtspServer->envir().taskScheduler().unscheduleDelayedTask(proxyClientDescribeCompletenessCheckTask);

        printQOSData();

        /*if(streamState.clientMediaSession != NULL) {
            MediaSubsessionIterator iterator(*streamState.clientMediaSession);
            MediaSubsession* subsession;
            while((subsession = iterator.next()) != NULL) {
                //LOGD("session ID: %s", subsession->sessionId());
                //subsession->rtpSource()->stopGettingFrames();
                Medium::close(subsession->sink);
                subsession->sink = NULL;
            }
        }*/

        Medium::close(rtspServer);

        streamState.clientMediaSession = NULL;
        streamState.mediaSession = NULL;
        qosRecordHead = NULL;
        rtspServer->envir().reclaim(); //usageEnv = NULL;
        eventLoopWatchVariable = 0;
        //rtspServer = NULL;
    }


}

void addToStreamClientState() {
    MediaSession* mediaSession = ((NextProxyServerMediaSession*) streamState.mediaSession)->getClientMediaSession();
    streamState.clientMediaSession = mediaSession;
}

/*static*/ void scheduleNextQOSMeasurement() {
    nextQOSMeasurementUSecs += qosMeasurementIntervalMS*1000;
    struct timeval timeNow;
    gettimeofday(&timeNow, NULL);
    unsigned timeNowUSecs = timeNow.tv_sec*1000000 + timeNow.tv_usec;
    int usecsToDelay = nextQOSMeasurementUSecs - timeNowUSecs;

    receiverReportTimerTask = rtspServer->envir().taskScheduler().scheduleDelayedTask(
            usecsToDelay, (TaskFunc*)periodicQOSMeasurement, (void*)NULL);
}

/*static*/ void periodicQOSMeasurement(void* /*clientData*/) {
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
    LOGD("begin_QOS_statistics");

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
            LOGD("num_packets_received %d", numPacketsReceived);
            LOGD("num_packets_lost %d", int(numPacketsExpected - numPacketsReceived));

            if (curQOSRecord != NULL) {
                unsigned secsDiff = curQOSRecord->measurementEndTime.tv_sec
                                    - curQOSRecord->measurementStartTime.tv_sec;
                int usecsDiff = curQOSRecord->measurementEndTime.tv_usec
                                - curQOSRecord->measurementStartTime.tv_usec;
                double measurementTime = secsDiff + usecsDiff/1000000.0;
                LOGD("elapsed_measurement_time %f", measurementTime);
                //*env << "kBytes_received_total\t" << curQOSRecord->kBytesTotal << "\n";
                LOGD("kBytes_received_total %f", curQOSRecord->kBytesTotal);
                //*env << "measurement_sampling_interval_ms\t" << qosMeasurementIntervalMS << "\n";

                if (curQOSRecord->kbits_per_second_max == 0) {
                    // special case: we didn't receive any data:
                    //*env <<
                    //"kbits_per_second_min\tunavailable\n"
                    //        "kbits_per_second_ave\tunavailable\n"
                    //        "kbits_per_second_max\tunavailable\n";
                } else {
                    //*env << "kbits_per_second_min\t" << curQOSRecord->kbits_per_second_min << "\n";
                    LOGD("kbits_per_second_min %f", curQOSRecord->kbits_per_second_min);
                    //*env << "kbits_per_second_ave\t"
                    //<< (measurementTime == 0.0 ? 0.0 : 8*curQOSRecord->kBytesTotal/measurementTime) << "\n";
                    //*env << "kbits_per_second_max\t" << curQOSRecord->kbits_per_second_max << "\n";
                }

                //*env << "packet_loss_percentage_min\t" << 100*curQOSRecord->packet_loss_fraction_min << "\n";
                double packetLossFraction = numPacketsExpected == 0 ? 1.0
                                                                    : 1.0 - numPacketsReceived/(double)numPacketsExpected;
                if (packetLossFraction < 0.0) packetLossFraction = 0.0;
                //*env << "packet_loss_percentage_ave\t" << 100*packetLossFraction << "\n";
                //*env << "packet_loss_percentage_max\t"
                //<< (packetLossFraction == 1.0 ? 100.0 : 100*curQOSRecord->packet_loss_fraction_max) << "\n";

                RTPReceptionStatsDB::Iterator statsIter(src->receptionStatsDB());
                // Assume that there's only one SSRC source (usually the case):
                RTPReceptionStats* stats = statsIter.next(True);
                if (stats != NULL) {
                    LOGD("inter_packet_gap_ms_min %f", stats->minInterPacketGapUS()/1000.0);
                    struct timeval totalGaps = stats->totalInterPacketGaps();
                    double totalGapsMS = totalGaps.tv_sec*1000.0 + totalGaps.tv_usec/1000.0;
                    unsigned totNumPacketsReceived = stats->totNumPacketsReceived();
                    //*env << "inter_packet_gap_ms_ave\t"
                    //<< (totNumPacketsReceived == 0 ? 0.0 : totalGapsMS/totNumPacketsReceived) << "\n";
                    //*env << "inter_packet_gap_ms_max\t" << stats->maxInterPacketGapUS()/1000.0 << "\n";
                    LOGD("inter_packet_gap_ms_ave %f", (totNumPacketsReceived == 0 ? 0.0 : totalGapsMS/totNumPacketsReceived));
                    LOGD("inter_packet_gap_ms_max %f", stats->maxInterPacketGapUS()/1000.0);
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
        RTPReceptionStatsDB::Iterator iterator(src->receptionStatsDB());
        RTPReceptionStats* receptionStats = iterator.next(False);
        if(receptionStats != NULL) {
            unsigned srTime = receptionStats->lastReceivedSR_time().tv_sec;
            LOGD("SR_time: %d", srTime);
            unsigned srNtplsw = receptionStats->lastReceivedSR_NTPlsw();
            LOGD("SR_NTPlsw: %d", srNtplsw);
            unsigned srNtpmsw = receptionStats->lastReceivedSR_NTPmsw();
            LOGD("SR_NTPmsw: %d", srNtpmsw);
        }
    }


    if (newTotNumPacketsReceived == totNumPacketsReceived) {
        // No additional packets have been received since the last time we
        // checked, so end this stream:

        interPacketGapCheckTimerTask = NULL;
        int message = PROXY_BACK_END_NO_RESPONSE;
        notifyBackendDead(message);

    } else {
        totNumPacketsReceived = newTotNumPacketsReceived;
        // Check again, after the specified delay:
        interPacketGapCheckTimerTask
                = rtspServer->envir().taskScheduler().scheduleDelayedTask(interPacketGapMaxTime*1000000,
                                                           (TaskFunc*)checkInterPacketGaps, NULL);
    }
}

void checkProxyClientDescribeCompleteness() {

    if(((NextProxyServerMediaSession*) streamState.mediaSession)->describeCompletedSuccessfully()) {
        proxyClientDescribeCompletenessCheckTask = rtspServer->envir().taskScheduler().scheduleDelayedTask(2, (TaskFunc*) checkProxyClientDescribeCompleteness, NULL);
    } else {
        int message = PROXY_BACK_END_NO_RESPONSE;
        notifyBackendDead(message);
    }

}

FUNC(jstring, initRtspProxy, jstring streamUrl) {

    OutPacketBuffer::maxSize = 100000;

    UsageEnvironment* usageEnv;
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    usageEnv = BasicUsageEnvironment::createNew(*scheduler);

    portNumBits tunnelOverHTTPPortNum = 0;

    rtspServer = createRTSPServer(usageEnv);

    const jsize len = env->GetStringUTFLength(streamUrl);
    char const* proxiedStreamUrl = env->GetStringUTFChars(streamUrl, 0);

    // for TCP back-end connection
    //streamRTPOverTCP = True;
    tunnelOverHTTPPortNum = (portNumBits)(~0);

    char streamName[30];
    sprintf(streamName, "%s", "proxyStream");

    NextProxyServerMediaSession* sms = NextProxyServerMediaSession::createNew(*usageEnv, rtspServer, proxiedStreamUrl, tunnelOverHTTPPortNum, streamName);

    streamState.mediaSession = (MediaSession*) sms;

    rtspServer->addServerMediaSession(sms);

    //jmethodID methodId = env->GetMethodID(proxyContext.mainActivityClazz, "callbackReceived", "(J)V");
    //env->CallVoidMethod(proxyContext.mainActivityObject, methodId, reinterpret_cast<intptr_t>(rtspServer));

    LOGD("Stream is proxied!");

    jclass clazz = env->GetObjectClass(thiz);
    proxyContext.mainActivityClazz = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
    proxyContext.mainActivityObject= env->NewGlobalRef(thiz);

    pthread_attr_t threadAttr_;
    pthread_attr_init(&threadAttr_);
    pthread_attr_setdetachstate(&threadAttr_, PTHREAD_CREATE_DETACHED);
    pthread_mutex_init(&proxyContext.lock, NULL);
    pthread_t looperThread;
    pthread_create(&looperThread, &threadAttr_, &doEventLoopFunc, &proxyContext);

    return env->NewStringUTF(rtspServer->rtspURL((ServerMediaSession*) streamState.mediaSession));
}

FUNC(void, startStreamingMeasurement) {
    //addToStreamClientState();
    checkInterPacketGaps();
    //((NextProxyServerMediaSession*) streamState.mediaSession)->checkInterPacketGaps(NULL);
    beginQOSMeasurement();
    //checkProxyClientDescribeCompleteness();
}

FUNC(void, shutdownStream) {
    //RTSPServer *server = reinterpret_cast<RTSPServer*>(jrtspServer);
    shutdownServer(rtspServer);
}

/*
FUNC(void, startProxy) {
    eventLoopWatchVariable = 0;
    pthread_t looperThread;
    pthread_attr_t threadAttr_;

    pthread_attr_init(&threadAttr_);
    pthread_attr_setdetachstate(&threadAttr_, PTHREAD_CREATE_DETACHED);

    pthread_mutex_init(&proxyContext.lock, NULL);

    jclass clazz = env->GetObjectClass(thiz);
    proxyContext.mainActivityClazz = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
    proxyContext.mainActivityObject= env->NewGlobalRef(thiz);

    pthread_create(&looperThread, &threadAttr_, &doEventLoopFunc, &proxyContext);
}

FUNC(void, stopProxy) {
    if(&eventLoopWatchVariable == 0) {
        rtspServer->envir().taskScheduler().unscheduleDelayedTask(receiverReportTimerTask);

        pthread_mutex_lock(&proxyContext.lock);
        eventLoopWatchVariable = 1;
        pthread_mutex_unlock(&proxyContext.lock);

        env->DeleteGlobalRef(proxyContext.mainActivityClazz);
        env->DeleteGlobalRef(proxyContext.mainActivityObject);
        proxyContext.mainActivityClazz = NULL;
        proxyContext.mainActivityObject = NULL;
    }
}*/

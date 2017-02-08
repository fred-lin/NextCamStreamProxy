#include <MediaSession.hh>
#include "QosMeasurementRecord.hh"

void qosMeasurementRecord
::periodicQOSMeasurement(struct timeval const& timeNow) {
    unsigned secsDiff = timeNow.tv_sec - measurementEndTime.tv_sec;
    int usecsDiff = timeNow.tv_usec - measurementEndTime.tv_usec;
    double timeDiff = secsDiff + usecsDiff/1000000.0;
    measurementEndTime = timeNow;

    RTPReceptionStatsDB::Iterator statsIter(fSource->receptionStatsDB());
    // Assume that there's only one SSRC source (usually the case):
    RTPReceptionStats* stats = statsIter.next(True);
    if (stats != NULL) {
        double kBytesTotalNow = stats->totNumKBytesReceived();
        double kBytesDeltaNow = kBytesTotalNow - kBytesTotal;
        kBytesTotal = kBytesTotalNow;

        double kbpsNow = timeDiff == 0.0 ? 0.0 : 8*kBytesDeltaNow/timeDiff;
        if (kbpsNow < 0.0) kbpsNow = 0.0; // in case of roundoff error
        if (kbpsNow < kbits_per_second_min) kbits_per_second_min = kbpsNow;
        if (kbpsNow > kbits_per_second_max) kbits_per_second_max = kbpsNow;

        unsigned totReceivedNow = stats->totNumPacketsReceived();
        unsigned totExpectedNow = stats->totNumPacketsExpected();
        unsigned deltaReceivedNow = totReceivedNow - totNumPacketsReceived;
        unsigned deltaExpectedNow = totExpectedNow - totNumPacketsExpected;
        totNumPacketsReceived = totReceivedNow;
        totNumPacketsExpected = totExpectedNow;

        double lossFractionNow = deltaExpectedNow == 0 ? 0.0
                                                       : 1.0 - deltaReceivedNow/(double)deltaExpectedNow;
        //if (lossFractionNow < 0.0) lossFractionNow = 0.0; //reordering can cause
        if (lossFractionNow < packet_loss_fraction_min) {
            packet_loss_fraction_min = lossFractionNow;
        }
        if (lossFractionNow > packet_loss_fraction_max) {
            packet_loss_fraction_max = lossFractionNow;
        }
    }
}

void qosMeasurementRecord::scheduleNextQOSMeasurement() {
    nextQOSMeasurementUSecs += qosMeasurementIntervalMS*1000;
    struct timeval timeNow;
    gettimeofday(&timeNow, NULL);
    unsigned timeNowUSecs = timeNow.tv_sec*1000000 + timeNow.tv_usec;
    int usecsToDelay = nextQOSMeasurementUSecs - timeNowUSecs;

    //receiverReportTimerTask = rtspServer->envir().taskScheduler().scheduleDelayedTask(
    //        usecsToDelay, (TaskFunc*)periodicQOSMeasurement, (void*)NULL);
}

void qosMeasurementRecord::periodicQOSMeasurement(void* /*clientData*/) {
    struct timeval timeNow;
    gettimeofday(&timeNow, NULL);

    for (qosMeasurementRecord* qosRecord = qosRecordHead;
         qosRecord != NULL; qosRecord = qosRecord->fNext) {
        qosRecord->periodicQOSMeasurement(timeNow);
    }

    // Do this again later:
    scheduleNextQOSMeasurement();
}

void qosMeasurementRecord::checkInterPacketGaps(void* /*clientData*/) {
    if (interPacketGapMaxTime == 0) return; // we're not checking

    // Check each subsession, counting up how many packets have been received:
    unsigned newTotNumPacketsReceived = 0;

    /*MediaSubsessionIterator iter(*streamState.clientMediaSession);
    MediaSubsession* subsession;
    while ((subsession = iter.next()) != NULL) {
        RTPSource* src = subsession->rtpSource();
        if (src == NULL) continue;
        newTotNumPacketsReceived += src->receptionStatsDB().totNumPacketsReceived();
    }

    if (newTotNumPacketsReceived == totNumPacketsReceived) {
        // No additional packets have been received since the last time we
        // checked, so end this stream:
        //*env << "Closing session, because we stopped receiving packets.\n";
        interPacketGapCheckTimerTask = NULL;
        //sessionAfterPlaying();
    } else {
        totNumPacketsReceived = newTotNumPacketsReceived;
        // Check again, after the specified delay:
        interPacketGapCheckTimerTask
                = env->taskScheduler().scheduleDelayedTask(interPacketGapMaxTime*1000000,
                                                           (TaskFunc*)checkInterPacketGaps, NULL);
    }*/
}
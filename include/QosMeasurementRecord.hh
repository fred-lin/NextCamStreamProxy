
#ifndef MYAPPLICATION_QOSMEASUREMENTRECORD_HH
#define MYAPPLICATION_QOSMEASUREMENTRECORD_HH

#ifndef RTPSOURCE_HH
#define RTPSOURCE_HH
#include "RTPSource.hh"
#endif //RTPSOURCE_HH

class qosMeasurementRecord {
public:
    qosMeasurementRecord(struct timeval const& startTime, RTPSource* src)
            : fSource(src), fNext(NULL),
              kbits_per_second_min(1e20), kbits_per_second_max(0),
              kBytesTotal(0.0),
              packet_loss_fraction_min(1.0), packet_loss_fraction_max(0.0),
              totNumPacketsReceived(0), totNumPacketsExpected(0) {
        measurementEndTime = measurementStartTime = startTime;

        RTPReceptionStatsDB::Iterator statsIter(src->receptionStatsDB());
        // Assume that there's only one SSRC source (usually the case):
        RTPReceptionStats* stats = statsIter.next(True);
        if (stats != NULL) {
            kBytesTotal = stats->totNumKBytesReceived();
            totNumPacketsReceived = stats->totNumPacketsReceived();
            totNumPacketsExpected = stats->totNumPacketsExpected();
        }
    }
    virtual ~qosMeasurementRecord() { delete fNext; }

    void periodicQOSMeasurement(struct timeval const& timeNow);

public:
    RTPSource* fSource;
    qosMeasurementRecord* fNext;
    TaskToken interPacketGapCheckTimerTask = NULL;

public:
    struct timeval measurementStartTime, measurementEndTime;
    double kbits_per_second_min, kbits_per_second_max;
    double kBytesTotal;
    double packet_loss_fraction_min, packet_loss_fraction_max;
    unsigned totNumPacketsReceived = ~0, totNumPacketsExpected;

private:
    void scheduleNextQOSMeasurement();
    void periodicQOSMeasurement(void* /*clientData*/);
    void checkInterPacketGaps(void* /*clientData*/);
    unsigned qosMeasurementIntervalMS = 1000;
    unsigned nextQOSMeasurementUSecs;
    qosMeasurementRecord* qosRecordHead = NULL;
    unsigned interPacketGapMaxTime = 5;//in seceonds
};

#endif //MYAPPLICATION_QOSMEASUREMENTRECORD_HH

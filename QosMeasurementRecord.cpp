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

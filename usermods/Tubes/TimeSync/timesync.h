/** \file
    \brief TimeSync: Time Synchronization
    \copyright Copyright (c) 2017-2018 Christopher A. Taylor.  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of TimeSync nor the names of its contributors may be
      used to endorse or promote products derived from this software without
      specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "counter.h"

#include <atomic>

/**
    Time Synchronization Protocol

    -- Motivation:

    Time synchronization is an important core component of an rUDP library,
    enabling multiple advantages over reliable UDP libraries without:

    (1) This specific (new) time synchronization works better over cellular
    networks than PTP/NTP.

    (2) The API provides time synchronization as a feature for applications,
    enabling millisecond-accurate dead reckoning for video games, and
    16-microsecond-accurate timing for scientific applications with 2-3 bytes.

    (3) Peer2Peer NAT hole-punch can be optimized because it can use time
    synchronization to initiate probes simultaneously on both peers.

    (4) Delay-based Congestion Control systems should use One Way Delay (OWD)
    on each packet as a signal, which allows it to e.g. avoid causing latency
    in realtime games while delivering a file transfer in the background.
    All existing Delay-based CC algorithms use differential OWD rather than
    proper time synchronization.  By adding time synchronization, CC becomes
    robust to changes in the base OWD as the end-points remain synced.

    -- Background:

    Network time synchronization can be done two ways:
    (a) Broadcast - Infeasible on the Internet and so not used.
    (b) Assuming that the link is symmetric, and trusting Min(RTT/2) = OWD.
    Meaning that existing network time synchronization protocols like NTP and
    PTP work by sending multiple probes, and then taking the probe with the
    smallest round trip time to be the best data to use in the set of probes.

    Time synchronization at higher resolutions needs to be performed constantly
    because clocks drift at a rate of about 1 millisecond per 10 seconds.
    So, a common choice for reliable UDP game protocols is to probe for time
    synchronization purposes (running NTP all the time) at a fixed interval of
    e.g. 5 to 10 seconds.

    The disadvantage of all of these existing approaches is that they only use
    a finite number of probes, and all probes may be slightly skewed,
    especially when jitter is present, or cross-traffic, or self-congestion
    from a file transfer.

    For cellular networks, existing time synchronization approaches provide
    degraded results due to the 4-10 millisecond jitter on every packet.
    "An End-to-End Measurement Study of Modern Cellular Data Networks" (2014)
    https://www.comp.nus.edu.sg/~bleong/publications/pam14-ispcheck.pdf

    when a link is asymmetrical there is no known practical method for
    performing time synchronization between two peers that meet on the Internet,
    so in that case a best effort is done, and at least it will be consistent.

    -- Algorithm:

    This TimeSync protocol overcomes this jitter problem by using a massive
    number of probes (every packet is a probe), greatly increasing the odds
    of a minimal RTT probe.

    How it works is that every packet has a 3 byte microsecond timestamp on it,
    large enough to prevent roll-over.  Both sides record the receive time of
    each packet as early as possible, and then throw the packet onto another
    thread to process, so that the network delay is more accurate and each
    client of a server can run in parallel.

    While each packet is being processed the difference in send and receive
    times are compared with prior such differences.  And a windowed minimum
    of these differences is updated.  Periodically this minimum difference
    is reported to the remote peer so that both sides have both the minimum
    (outgoing - incoming) difference and the minimum (incoming - outgoing)
    difference.

    These minimum differences correspond to the shortest trips each way.
    Effectively, it turns every single packet into a time synchronization
    probe, guaranteeing that it gets the best result possible.

    -- The (Simple) Math for TimeSync:

    We measure (Smallest C2S Delta) and (Smallest S2C Delta)
    through the per-packet timestamps.

        C2S = (Smallest C2S Delta)
            = (Server Time @ Client Send Time) + (C2S Propagation Delay) - (Client Send Time)
            = (C2S Propagation Delay) + (Clock Delta)

        S2C = (Smallest S2C Delta)
            = (Client Time @ Server Send Time) + (S2C Propagation Delay) - (Server Send Time)
            = (S2C Propagation Delay) - (Clock Delta)

        Such that (Propagation Delay) for each direction is minimized
        independently as described previously.

    We want to solve for (Clock Delta) but there is a problem:

    Note that in the definition of C2S and S2C there are three unknowns and
    only two measurements.  To resolve this problem we make the assumption
    that the min. propagation delays are almost the same in each direction.

    And so:

        (S2C Propagation Delay) approx equals (C2S Propagation Delay).

    Thus we can simply write:

        (Clock Delta) = (C2S - S2C) / 2

    This gives us 23 bits of the delta between clocks, since the division
    by 2 (right shift) pulls in an unknown high bit.  The effect of any
    link asymmetry is halved as a side effect, helping to minimize it.

    -- The (Simple) Math for Network Trip Time:

    It also provides a robust estimate of the "speed of light" between two
    network hosts (minimal one-way delay).  This information is then used to
    calculate the network trip time for every packet that arrives:

    Let DD = Distance from the current packet timestamp difference and minimal.
    DD = (Packet Receive - Packet Send) - Min(Packet Receive - Packet Send)
    Packet trip time = (Minimal one-way delay) + DD.
*/


//------------------------------------------------------------------------------
// Constants

/// Default One Way Delay (OWD) to return before time sync completes
static const uint32_t kDefaultOWDUsec = 200 * 1000; ///< 200 ms

/// Number of bits removed from the low end of the microsecond timestamp
static const unsigned kTime23LostBits = 3;

/// Largest 23-bit counter difference value considered positive
static const unsigned kTime23Bias = 0x200000;

/// Error bound for 23-bit timestamps <= 8*2-1 = 15 microseconds
static const unsigned kTime23ErrorBound = (1 << kTime23LostBits) * 2 - 1;

/// Number of bits removed from the low end of the microsecond timestamp
static const unsigned kTime16LostBits = 9;

/// Largest 16-bit counter difference value considered positive
static const unsigned kTime16Bias = 0x4000;

/// Error bound for 16-bit timestamps <= 512*2-1 = 1.023 milliseconds
static const unsigned kTime16ErrorBound = (1 << kTime16LostBits) * 2 - 1;

/// Window size for WindowedMinTS24Deltas.
/// Since clocks drift over time, eventually old measurements must be ignored.
/// This is also the longest that a timing measurement will affect time synch.
/// Assumes that clocks drift 1 millisecond every 10 seconds
static const uint64_t kDriftWindowUsec = 10 * 1000 * 1000; ///< 10 seconds


//------------------------------------------------------------------------------
// Types

/// Use Counter23::Decompress to expand back to 64-bit counters
typedef Counter<uint32_t, 23> Counter23;


//------------------------------------------------------------------------------
// WindowedMinTS24

/// Windowed minimum in TS24 units
class WindowedMinTS24
{
public:
    WindowedMinTS24() {}

    struct Sample
    {
        /// Sample value
        Counter24 Value;

        /// Timestamp of data collection
        uint64_t Timestamp;


        /// Default values and initializing constructor
        explicit Sample(Counter24 value = 0, uint64_t timestamp = 0)
            : Value(value)
            , Timestamp(timestamp)
        {
        }

        /// Check if a timeout expired
        inline bool TimeoutExpired(uint64_t now, uint64_t timeout)
        {
            return (uint64_t)(now - Timestamp) > timeout;
        }
    };


    /// Number of samples collected
    static const unsigned kSampleCount = 3;

    /// Sorted samples from smallest to largest
    Sample Samples[kSampleCount];


    /// Are there any samples?
    inline bool IsValid() const
    {
        return Samples[0].Value != 0; ///< ish
    }

    /// Get smallest sample
    inline Counter24 GetBest() const
    {
        return Samples[0].Value;
    }

    /// Reset samples
    inline void Reset(const Sample sample = Sample())
    {
        Samples[0] = Samples[1] = Samples[2] = sample;
    }

    /// Update minimum with new value
    void Update(
        Counter24 value,
        uint64_t timestamp,
        const uint64_t windowLengthTime);
};


//------------------------------------------------------------------------------
// TimeSynchronizer

class TimeSynchronizer
{
public:
    /**
        OnPeerMinDeltaTS24()

        Call this when the peer provides its latest 24-bit MinDeltaTS24 value.
        The peer should do this periodically, ideally faster in the first minute
        and then settling down to once every 2 seconds or so.  This can be used
        as a keep-alive for example.

        minDeltaTS24: Provide the low 24 bits of the peer's delta value.
    */
    void OnPeerMinDeltaTS24(Counter24 minDeltaTS24);

    /// Convert local time in microseconds to a 24-bit datagram timestamp
    static inline uint32_t LocalTimeToDatagramTS24(uint64_t localUsec)
    {
        return (uint32_t)(localUsec >> kTime23LostBits) & 0x00ffffff;
    }

    /**
        OnAuthenticatedDatagramTimestamp()

        Call this when a datagram arrives with an attached 24-bit timestamp.
        Ideally every UDP/IP datagram we receive will have a timestamp.
        It is recommended to check if the datagram is from the peer before
        accepting the timestamp on the datagram.

        remoteSendTS24: The 24-bit timestamp attached to an incoming datagram.
        localRecvUsec: A recent timestamp in microsecond units.

        Returns estimated one way delay (OWD) in microseconds for this datagram.
        Returns 0 if OWD is unavailable.
    */
    unsigned OnAuthenticatedDatagramTimestamp(
        Counter24 remoteSendTS24,
        uint64_t localRecvUsec);


    /// Get the minimum TS24 (receipt - send) delta seen in the past interval
    inline Counter24 GetMinDeltaTS24() const
    {
        return WindowedMinTS24Deltas.GetBest();
    }

    /// Is time synchronized?
    inline bool IsSynchronized() const
    {
        return Synchronized;
    }

    /// Get the minimum one-way delay seen so far.
    /// This is equivalent to the shortest RTT/2 seen so far by any pair of packets,
    /// meaning that it is the average of the upstream and downstream OWD.
    inline uint32_t GetMinimumOneWayDelayUsec() const
    {
        return MinimumOneWayDelayUsec;
    }

    /// Returns 16-bit remote time field to send in a packet
    inline uint16_t ToRemoteTime16(uint64_t localUsec)
    {
        if (!Synchronized) {
            return 0;
        }

        const uint16_t localTS16 = (uint16_t)(localUsec >> kTime16LostBits);
        const uint16_t deltaTS16 = (uint16_t)(RemoteTimeDeltaUsec >> kTime16LostBits);

        return localTS16 + deltaTS16;
    }

    /// Returns local time given local time from packet
    inline uint64_t FromLocalTime16(
        uint64_t localUsec,
        Counter16 timestamp16)
    {
        return Counter64::ExpandFromTruncatedWithBias(
            localUsec >> kTime16LostBits,
            timestamp16,
            kTime16Bias).ToUnsigned() << kTime16LostBits;
    }

    /// Returns 23-bit remote time field to send in a packet
    inline uint32_t ToRemoteTime23(uint64_t localUsec)
    {
        if (!Synchronized) {
            return 0;
        }

        const Counter23 localTS23 = (uint32_t)(localUsec >> kTime23LostBits);
        const Counter23 deltaTS23 = RemoteTimeDeltaUsec >> kTime23LostBits;

        return (localTS23 + deltaTS23).ToUnsigned();
    }

    /// Returns local time given remote time from packet
    inline uint64_t FromLocalTime23(
        uint64_t localUsec,
        Counter23 timestamp23)
    {
        return Counter64::ExpandFromTruncatedWithBias(
            localUsec >> kTime23LostBits,
            timestamp23,
            kTime23Bias).ToUnsigned() << kTime23LostBits;
    }

protected:
    /// Synchronized?
    std::atomic<bool> Synchronized = ATOMIC_VAR_INIT(false);

    /// Calculated delta = (Remote time - Local time)
    std::atomic<uint32_t> RemoteTimeDeltaUsec = ATOMIC_VAR_INIT(0); ///< usec

    /// Calculated minimum OWD
    std::atomic<uint32_t> MinimumOneWayDelayUsec = ATOMIC_VAR_INIT(kDefaultOWDUsec); ///< in usec

    /// Windowed minimum value for received packet timestamp deltas
    /// Keep track of the smallest (receipt - send) time delta seen so far
    WindowedMinTS24 WindowedMinTS24Deltas; ///< in Timestamp24 units

    /// Keep a copy of the last MinDeltaUsec from the flow control data from peer
    Counter24 LastFC_MinDeltaTS24 = 0;

    /// Is peer update received yet?
    bool GotPeerUpdate = false;


    /// Recalculate MinimumOneWayDelayUsec and RemoteTimeDeltaUsec
    void Recalculate();
};
#include "metrics.hpp"

#include <algorithm>
#include <chrono>
#include <iphlpapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <vector>
#include <windows.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "pdh.lib")

namespace {
unsigned long long fileTimeToULL(const FILETIME& ft) {
    ULARGE_INTEGER ui;
    ui.LowPart  = ft.dwLowDateTime;
    ui.HighPart = ft.dwHighDateTime;
    return ui.QuadPart;
}
} // namespace

MetricsCollector::MetricsCollector() {}
MetricsCollector::~MetricsCollector() {
    if (pdhQuery_) {
        PdhCloseQuery(reinterpret_cast<PDH_HQUERY>(pdhQuery_));
    }
}

bool MetricsCollector::initialize() {
    lastSampleTick_ = GetTickCount64();

    // Initialize disk PDH counters (best-effort)
    PDH_HQUERY q = nullptr;
    if (PdhOpenQuery(nullptr, 0, &q) == ERROR_SUCCESS) {
        PDH_HCOUNTER cRead = nullptr, cWrite = nullptr;
        // Use explicit wide-char PDH functions to avoid ANSI mismatch.
        if (PdhAddCounterW(q, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &cRead) == ERROR_SUCCESS &&
            PdhAddCounterW(q, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &cWrite) == ERROR_SUCCESS) {
            if (PdhCollectQueryData(q) == ERROR_SUCCESS) {
                pdhQuery_        = q;
                pdhReadCounter_  = cRead;
                pdhWriteCounter_ = cWrite;
                diskInitialized_ = true;
            }
        }
        if (!diskInitialized_) {
            if (q)
                PdhCloseQuery(q);
        }
    }
    return true;
}

CpuSample MetricsCollector::sampleCpu() {
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user))
        return {};
    auto i = fileTimeToULL(idle);
    auto k = fileTimeToULL(kernel);
    auto u = fileTimeToULL(user);
    if (prevIdle_ == 0) {
        prevIdle_   = i;
        prevKernel_ = k;
        prevUser_   = u;
        return {};
    }
    unsigned long long idleDelta   = i - prevIdle_;
    unsigned long long kernelDelta = k - prevKernel_;
    unsigned long long userDelta   = u - prevUser_;
    unsigned long long total       = kernelDelta + userDelta;
    prevIdle_                      = i;
    prevKernel_                    = k;
    prevUser_                      = u;
    CpuSample cs;
    if (total) {
        float usage = 1.0f - (float) idleDelta / (float) total;
        if (usage < 0)
            usage = 0;
        if (usage > 1)
            usage = 1;
        cs.usage = usage;
    }
    return cs;
}

MemorySample MetricsCollector::sampleMemory() {
    MEMORYSTATUSEX ms{sizeof(ms)};
    MemorySample   m;
    if (GlobalMemoryStatusEx(&ms)) {
        m.usage = (float) (ms.ullTotalPhys - ms.ullAvailPhys) / (float) ms.ullTotalPhys;
    }
    return m;
}

std::optional<NetSample> MetricsCollector::sampleNet() {
    ULONG size = 0;
    if (GetIfTable(nullptr, &size, FALSE) != ERROR_INSUFFICIENT_BUFFER) {
        return std::nullopt;
    }
    std::vector<unsigned char> buf(size);
    PMIB_IFTABLE               table = (PMIB_IFTABLE) buf.data();
    if (GetIfTable(table, &size, FALSE) != NO_ERROR) {
        return std::nullopt;
    }

    PMIB_IFROW selected = nullptr;

    if (selectedNetInterface_ == -1) {
        // Auto-select: Pick interface with highest speed that is up and not loopback
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            auto& row = table->table[i];
            if (row.dwOperStatus != IF_OPER_STATUS_OPERATIONAL)
                continue;
            if (row.dwType == IF_TYPE_SOFTWARE_LOOPBACK)
                continue;
            if (!selected || row.dwSpeed > selected->dwSpeed)
                selected = &row;
        }
    } else {
        // Use specific interface by index
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            auto& row = table->table[i];
            if (row.dwOperStatus != IF_OPER_STATUS_OPERATIONAL)
                continue;
            if (row.dwType == IF_TYPE_SOFTWARE_LOOPBACK)
                continue;
            if ((int) row.dwIndex == selectedNetInterface_) {
                selected = &row;
                break;
            }
        }
    }

    if (!selected)
        return std::nullopt;

    unsigned long long recv = selected->dwInOctets;
    unsigned long long sent = selected->dwOutOctets;

    double bytesRecvPerSec = 0.0;
    double bytesSentPerSec = 0.0;

    if (!netInitialized_) {
        prevRecv_       = recv;
        prevSent_       = sent;
        netInitialized_ = true;
    } else {
        unsigned long long recvDiff = recv - prevRecv_;
        unsigned long long sentDiff = sent - prevSent_;
        bytesRecvPerSec             = (double) recvDiff;
        bytesSentPerSec             = (double) sentDiff;
        prevRecv_                   = recv;
        prevSent_                   = sent;
    }

    NetSample ns;
    ns.bytesRecvPerSec     = bytesRecvPerSec;
    ns.bytesSentPerSec     = bytesSentPerSec;
    ns.linkSpeedBitsPerSec = selected->dwSpeed;
    return ns;
}

std::optional<DiskSample> MetricsCollector::sampleDisk() {
    if (!diskInitialized_)
        return std::nullopt;
    auto nowTick = GetTickCount64();
    if (PdhCollectQueryData(reinterpret_cast<PDH_HQUERY>(pdhQuery_)) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    PDH_FMT_COUNTERVALUE vRead{};
    DWORD                typeRead = 0;
    PDH_FMT_COUNTERVALUE vWrite{};
    DWORD                typeWrite = 0;
    if (PdhGetFormattedCounterValue(reinterpret_cast<PDH_HCOUNTER>(pdhReadCounter_), PDH_FMT_LARGE, &typeRead, &vRead) != ERROR_SUCCESS)
        return std::nullopt;
    if (PdhGetFormattedCounterValue(reinterpret_cast<PDH_HCOUNTER>(pdhWriteCounter_), PDH_FMT_LARGE, &typeWrite, &vWrite) != ERROR_SUCCESS)
        return std::nullopt;

    double readB  = (double) vRead.largeValue;
    double writeB = (double) vWrite.largeValue;

    return DiskSample{readB, writeB};
}

MetricsSnapshot MetricsCollector::sample() {
    MetricsSnapshot snap;
    snap.cpu    = sampleCpu();
    snap.memory = sampleMemory();
    snap.net    = sampleNet();
    snap.disk   = sampleDisk();
    return snap;
}

void MetricsCollector::setSelectedNetworkInterface(int interfaceIndex) {
    if (selectedNetInterface_ != interfaceIndex) {
        selectedNetInterface_ = interfaceIndex;
        netInitialized_       = false; // Reset to reinitialize counters
    }
}

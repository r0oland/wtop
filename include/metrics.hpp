#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <optional>

struct CpuSample {
    float usage = 0.0f; // 0..1
};

struct MemorySample {
    float usage = 0.0f; // 0..1
};

struct NetSample {
    double bytesRecvPerSec = 0.0; // per second
    double bytesSentPerSec = 0.0;
    unsigned long linkSpeedBitsPerSec = 0; // interface nominal speed
};

struct DiskSample {
    double readBytesPerSec = 0.0;
    double writeBytesPerSec = 0.0;
};

struct MetricsSnapshot {
    CpuSample cpu;
    MemorySample memory;
    std::optional<NetSample> net; // may be unavailable
    std::optional<DiskSample> disk; // may be unavailable for MVP
};

class MetricsCollector {
public:
    MetricsCollector();
    ~MetricsCollector();

    bool initialize();
    MetricsSnapshot sample();
    void setSelectedNetworkInterface(int interfaceIndex); // -1 for auto-select

private:
    // CPU times
    unsigned long long prevIdle_ = 0;
    unsigned long long prevKernel_ = 0;
    unsigned long long prevUser_ = 0;

    // Network snapshot
    unsigned long long prevRecv_ = 0; // aggregate interface selection
    unsigned long long prevSent_ = 0;
    bool netInitialized_ = false;
    int selectedNetInterface_ = -1; // -1 = auto-select, else specific interface index

    // Disk PDH
    void* pdhQuery_ = nullptr; // PDH_HQUERY
    void* pdhReadCounter_ = nullptr; // PDH_HCOUNTER
    void* pdhWriteCounter_ = nullptr; // PDH_HCOUNTER
    bool diskInitialized_ = false;
    unsigned long long lastSampleTick_ = 0;

    CpuSample sampleCpu();
    MemorySample sampleMemory();
    std::optional<NetSample> sampleNet();
    std::optional<DiskSample> sampleDisk();
};

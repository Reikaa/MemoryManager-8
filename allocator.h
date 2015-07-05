#ifndef ALLOCATOR_H_INCLUDED
#define ALLOCATOR_H_INCLUDED

#include <vector>
#include <memory>
#include <cstddef>

class Allocator
{
public:
    void* Allocate(uint32_t size);
    void Free();

    Allocator();

private:
    struct Deleter {
        void operator()(void* memory) {
            free(memory);
        }
    };

    struct ChunkHeader {
        uint32_t size;
        uint32_t prevSize;
        uint32_t nextSize;
    };

    struct PoolHeader {
        PoolHeader* next;
        PoolHeader* prev;
        uint32_t poolSize;
    };

    void* init(uint32_t size = Allocator::defaultSize);
    void setUnused(ChunkHeader* header);
    void setUsed(ChunkHeader* header);
    bool isUsed(const ChunkHeader* header) const;

    void setSize(ChunkHeader* header, uint32_t size);
    uint32_t getSize(const ChunkHeader* header) const;

    void createHeader(void* addr, uint32_t size, uint32_t prevSize, uint32_t nextSize);
    void updateNextHeader(ChunkHeader* nextHeader, uint32_t prevSize);
    void updatePreviousHeader(ChunkHeader* prevHeader, uint32_t nextSize);

    size_t addressRemainder(void* address) const;
    void* getFirstAlignedAddress(void* address) const;
    void* getRightBlock(uint32_t size);
    void occupy(ChunkHeader* header, uint32_t freeSize, uint32_t size);

    void* allocateNewChunk(uint32_t size);
    uint32_t speciallyAllocatedChunkSize(uint32_t) const;

    static const uint32_t defaultSize = 1 << 16; /// 64KB;
    static const int leftMostBit = sizeof(uint32_t) * 8;
    static const int bitUsedShift = leftMostBit - 1;
    static const uint32_t maxBlockSize = ~(1 << bitUsedShift);
    static const size_t alignment = 8;
    static const uint32_t maxBlockToGive = maxBlockSize - sizeof(PoolHeader) - sizeof(ChunkHeader) - alignment;

    bool initialiazed;
    std::unique_ptr<void, Deleter> memoryPool;
};


#endif // ALLOCATOR_H_INCLUDED

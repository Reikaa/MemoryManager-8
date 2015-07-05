#include "allocator.h"

Allocator::Allocator()
{
    initialiazed = false;
}

void Allocator::setUsed(ChunkHeader* header)
{
    header->size |= (1 << Allocator::bitUsedShift);
}

void Allocator::setUnused(ChunkHeader* header)
{
    header->size &= ~(1 << Allocator::bitUsedShift);
}

bool Allocator::isUsed(const ChunkHeader* header) const
{
    return header->size & (1 << Allocator::bitUsedShift);
}

void Allocator::setSize(ChunkHeader* header, uint32_t size)
{
    bool used = isUsed(header);
    header->size = size;
    if(used) {
        setUsed(header);
    }
}

uint32_t Allocator::getSize(const ChunkHeader* header) const
{
    return header->size & ~(1 << bitUsedShift);
}

size_t Allocator::addressRemainder(void* address) const
{
    return (size_t)address % Allocator::alignment;
}

void* Allocator::getFirstAlignedAddress(void* address) const
{
    return (void*)((char*)address + addressRemainder(address));
}

void Allocator::createHeader(void* addr, uint32_t size, uint32_t prevSize, uint32_t nextSize)
{
    ChunkHeader* newHeader = (ChunkHeader*)addr;
    newHeader->size = size;
    newHeader->prevSize = prevSize;
    newHeader->nextSize = nextSize;
    setUnused(newHeader);

    if(nextSize > 0) {
        ChunkHeader* next = (ChunkHeader*)((char*)addr + size);
        updateNextHeader(next, size);
    }

    if(prevSize > 0) {
        ChunkHeader* prev = (ChunkHeader*)((char*)addr - prevSize - sizeof(ChunkHeader));
        updatePreviousHeader(prev, size);
    }
}

void Allocator::updateNextHeader(ChunkHeader* nextHeader, uint32_t prevSize)
{
    nextHeader->prevSize = prevSize;
}

void Allocator::updatePreviousHeader(ChunkHeader* prevHeader, uint32_t nextSize)
{
    prevHeader->nextSize = nextSize;
}

uint32_t Allocator::speciallyAllocatedChunkSize(uint32_t size) const
{
    return size + sizeof(PoolHeader) + sizeof(ChunkHeader) + Allocator::alignment;
}

void* Allocator::allocateNewChunk(uint32_t size)
{
    uint32_t newPoolSize = speciallyAllocatedChunkSize(size);
    return malloc(newPoolSize);
}

void Allocator::occupy(ChunkHeader* header, uint32_t freeSize, uint32_t size)
{
    void* userBlockStart = (void*)((char*)header + sizeof(ChunkHeader));
    uint32_t alignedAddressRemainder = header->size - freeSize;
    void* alignedAddress = (void*)((char*)userBlockStart + alignedAddressRemainder);

    uint32_t chunkSize = getSize(header);
    uint32_t leftSize = freeSize - size;
    uint32_t finalChunkSize = leftSize <= sizeof(ChunkHeader) ? chunkSize : size + alignedAddressRemainder;
    setUsed(header);
    setSize(header, finalChunkSize);

    if(finalChunkSize < chunkSize) { /// enough memory left
        void* newHeaderAddr = (void*)((char*)alignedAddress + size);
        uint32_t newChunkSize = leftSize - sizeof(ChunkHeader);
        uint32_t prevChunkSize = finalChunkSize;
        uint32_t nextChunkSize = header->nextSize;
        createHeader(newHeaderAddr, newChunkSize, prevChunkSize, nextChunkSize);
    }
}

void* Allocator::getRightBlock(uint32_t size)
{
    char* memoryStart = (char*)memoryPool.get();
    PoolHeader* poolStart = (PoolHeader*)memoryStart;
    uint32_t poolSize = poolStart->poolSize;
    void* poolEnd = memoryStart + poolSize;

    ChunkHeader* currentChunk = (ChunkHeader*)(memoryStart + sizeof(PoolHeader));
    void* allocatedChunk = NULL; ///result
    while(currentChunk) {
        uint32_t currentChunkSize = getSize(currentChunk);
        ChunkHeader* nextChunk = (ChunkHeader*)((char*)currentChunk + currentChunkSize);
        if(nextChunk >= poolEnd) {
            nextChunk = NULL;
        }
        if(isUsed(currentChunk) || size > currentChunkSize) {
            currentChunk = nextChunk;
            continue;
        }
        void* blockAddress = (void*)((char*)currentChunk + sizeof(ChunkHeader));
        void* lastAddress = (void*)((char*)blockAddress + currentChunkSize);
        size_t addrRemainder = addressRemainder(blockAddress);
        void* firstAligned = (void*)((char*)blockAddress + addrRemainder);
        if(firstAligned >= lastAddress) {
            currentChunk = nextChunk;
            continue;
        }
        uint32_t blockSizeLeft = currentChunkSize - addrRemainder;
        if(size > blockSizeLeft) {
            currentChunk = nextChunk;
            continue;
        }
        occupy(currentChunk, blockSizeLeft, size);
        return firstAligned;
    }
    ///allocate new chunk of memory
    void* newChunk = allocateNewChunk(size);
    if(!newChunk) {
        return NULL;
    }
    ///set headers
    PoolHeader* newPoolHeader = (PoolHeader*)newChunk;
    newPoolHeader->next = NULL;
    newPoolHeader->prev = poolStart;
    newPoolHeader->poolSize = speciallyAllocatedChunkSize(size);

    poolStart->next = newPoolHeader;

    void* chunkHeaderStart = (void*)((char*)newPoolHeader + sizeof(PoolHeader));
    uint32_t chunkHeaderSize = newPoolHeader->poolSize - sizeof(PoolHeader) - sizeof(ChunkHeader);
    createHeader(chunkHeaderStart, chunkHeaderSize, 0, 0);
    setUsed((ChunkHeader*)chunkHeaderStart);

    return getFirstAlignedAddress((void*)((char*)chunkHeaderSize + sizeof(ChunkHeader)));
}

void* Allocator::init(uint32_t size)
{
    void* memory = malloc(size);
    if(!memory) {
        return NULL;
    }
    PoolHeader* poolHeader = (PoolHeader*)memory;
    poolHeader->next = NULL;
    poolHeader->poolSize = size;

    char* poolStart = (char*)memory;
    ChunkHeader* chunkHeader = (ChunkHeader*)(poolStart + sizeof(PoolHeader));
    chunkHeader->prevSize = 0;
    chunkHeader->nextSize = 0;
    chunkHeader->size = size - sizeof(PoolHeader) - sizeof(ChunkHeader);
    setUnused(chunkHeader);

    initialiazed = true;
    return memory;
}

void* Allocator::Allocate(uint32_t size)
{
    if(size > Allocator::maxBlockToGive) {
        return NULL;
    }
    if(!initialiazed) {
        uint32_t neededSize;
        if(Allocator::defaultSize <= size) {
            neededSize = size + sizeof(PoolHeader) + sizeof(ChunkHeader);
        } else {
            neededSize = Allocator::defaultSize;
        }

        void* pool = init(neededSize);
        if(!pool) {
            return NULL;
        }
        memoryPool.reset(init(neededSize));
    }

    return getRightBlock(size);
}

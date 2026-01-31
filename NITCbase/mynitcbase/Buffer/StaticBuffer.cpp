#include "StaticBuffer.h"
#include <cstdlib>
#include <cstring>

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer()
{
    for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        metainfo[bufferIndex].free = true;
        metainfo[bufferIndex].dirty = false;
        metainfo[bufferIndex].blockNum = -1;
        metainfo[bufferIndex].timeStamp = -1;
    }
}

StaticBuffer::~StaticBuffer() 
{
    for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        if(metainfo[bufferIndex].free == false && metainfo[bufferIndex].dirty == true)
        {
            Disk::writeBlock(StaticBuffer::blocks[bufferIndex], metainfo[bufferIndex].blockNum);
        }
    }
}

int StaticBuffer::getFreeBuffer(int blockNum)
{
    if(blockNum < 0 || blockNum > DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }

    for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        if(metainfo[bufferIndex].free == false)
        {
            metainfo[bufferIndex].timeStamp++;
        }
    }

    int bufferNum = -1;
    
    for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        if(metainfo[bufferIndex].free)
        {
            bufferNum = bufferIndex;
            break;
        }
    }

    if(bufferNum == -1)
    {
        int maxTimeStamp = -1;
        int maxTimeStampIndex = 0;
    
        for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
        {
            if(metainfo[bufferIndex].timeStamp > maxTimeStamp)
            {
                maxTimeStamp = metainfo[bufferIndex].timeStamp;
                maxTimeStampIndex = bufferIndex;
            }
        }
    
        if(metainfo[maxTimeStampIndex].dirty)
        {
            Disk::writeBlock(StaticBuffer::blocks[maxTimeStampIndex], metainfo[maxTimeStampIndex].blockNum);
        }
    
        bufferNum = maxTimeStampIndex;
    }

    metainfo[bufferNum].free = false;
    metainfo[bufferNum].dirty = false;
    metainfo[bufferNum].blockNum = blockNum;
    metainfo[bufferNum].timeStamp = 0;

    return bufferNum;
}

int StaticBuffer::getBufferNum(int blockNum)
{
    if(blockNum < 0 || blockNum > DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }

    for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        if(metainfo[bufferIndex].blockNum == blockNum)
        {
            return bufferIndex;
        }
    }

    return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::setDirtyBit(int blockNum)
{
    int bufferIndex = StaticBuffer::getBufferNum(blockNum);

    if(bufferIndex == E_BLOCKNOTINBUFFER)
    {
        return E_BLOCKNOTINBUFFER;
    }

    if(bufferIndex == E_OUTOFBOUND)
    {
        return E_OUTOFBOUND;
    }

    metainfo[bufferIndex].dirty = true;

    return SUCCESS;
}

#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum) 
{
    this->blockNum = blockNum;
}

BlockBuffer::BlockBuffer(char blockType) 
{
    int blockTypeInt;
    
    if(blockType == 'R')
    {
        blockTypeInt = REC;
    }
    else if(blockType == 'I')
    {
        blockTypeInt = IND_INTERNAL;
    }
    else if(blockType == 'L')
    {
        blockTypeInt = IND_LEAF;
    }
    else
    {
        blockTypeInt = UNUSED_BLK;
    }

    int blockNum = BlockBuffer::getFreeBlock(blockTypeInt);
    this->blockNum = blockNum;

    if(blockNum < 0 || blockNum >= DISK_BLOCKS)
    {
        return;
    }
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}
RecBuffer::RecBuffer() : BlockBuffer('R') {}

IndBuffer::IndBuffer(char blockType) : BlockBuffer(blockType){}
IndBuffer::IndBuffer(int blockNum) : BlockBuffer(blockNum){}

IndInternal::IndInternal() : IndBuffer('I'){}
IndInternal::IndInternal(int blockNum) : IndBuffer(blockNum){}

IndLeaf::IndLeaf() : IndBuffer('L'){}
IndLeaf::IndLeaf(int blockNum) : IndBuffer(blockNum){}

int BlockBuffer::getBlockNum()
{
    return this->blockNum;
}

int BlockBuffer::getFreeBlock(int blockType)
{
    int freeBlock = -1;

    for(int i = 0; i < DISK_BLOCKS; i++)
    {
        if(StaticBuffer::blockAllocMap[i] == UNUSED_BLK)
        {
            freeBlock = i;
            break;
        }
    }

    if(freeBlock == -1)
    {
        return E_DISKFULL;
    }

    this->blockNum = freeBlock;

    int bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    struct HeadInfo head;

    head.pblock = -1;
    head.lblock = -1;
    head.rblock = -1;
    head.numEntries = 0;
    head.numSlots = 0;
    head.numAttrs = 0;

    int ret = BlockBuffer::setHeader(&head);
    if(ret != SUCCESS)
    {
        return ret;
    }

    ret = BlockBuffer::setBlockType(blockType);
    if(ret != SUCCESS)
    {
        return ret;
    }

    return this->blockNum;
}

int BlockBuffer::getHeader(struct HeadInfo *head) {
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) 
    {
        return ret;
    }

    memcpy(&head->reserved, bufferPtr + 28, 4);
    memcpy(&head->numSlots, bufferPtr + 24, 4);
    memcpy(&head->numAttrs, bufferPtr + 20, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->rblock, bufferPtr + 12, 4);
    memcpy(&head->lblock, bufferPtr + 8, 4);
    memcpy(&head->pblock, bufferPtr + 4, 4);
    memcpy(&head->blockType, bufferPtr, 4);
    
    return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head)
{
    unsigned char* bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS)
    {
        return ret;
    }

    struct HeadInfo *bufferHeader = (struct HeadInfo*)bufferPtr;

    bufferHeader->blockType = head->blockType;
    bufferHeader->lblock = head->lblock;
    bufferHeader->numAttrs = head->numAttrs;
    bufferHeader->numEntries = head->numEntries;
    bufferHeader->numSlots = head->numSlots;
    bufferHeader->pblock = head->pblock;
    bufferHeader->rblock = head->rblock;

    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if(ret != SUCCESS)
    {
        return ret;
    }

    return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char * slotMap) 
{
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS)
    {
        return ret;
    }

    struct HeadInfo head;
    this->getHeader(&head);

    int slotCount = head.numSlots;
    unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

    memcpy(slotMap, slotMapInBuffer, slotCount);

    return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char* slotMap)
{
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS)
    {
        return ret;
    }

    struct HeadInfo head;
    this->getHeader(&head);

    int numSlots = head.numSlots;

    memcpy(bufferPtr + HEADER_SIZE, slotMap, numSlots);

    return StaticBuffer::setDirtyBit(this->blockNum);
}

int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
    struct HeadInfo head;

    this->getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS)
    {
        return ret;
    }

    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) 
{
    unsigned char* bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS)
    {
        return ret;
    }

    struct HeadInfo head;

    this->getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    if(slotNum < 0 || slotNum >= slotCount)
    {
        return E_OUTOFBOUND;
    }

    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

    memcpy(slotPointer, rec, recordSize);

    StaticBuffer::setDirtyBit(this->blockNum);

    return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **bufferPtr)
{
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    if(bufferNum != E_BLOCKNOTINBUFFER) 
    {
        StaticBuffer::metainfo[bufferNum].timeStamp = 0;

        for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
        {
            if(StaticBuffer::metainfo[bufferIndex].free == false)
            {
                StaticBuffer::metainfo[bufferIndex].timeStamp++;
            }
        }
    }
    else
    {
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        if(bufferNum == E_OUTOFBOUND)
        {
            return E_OUTOFBOUND;
        }
        
        Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
    }

    *bufferPtr = StaticBuffer::blocks[bufferNum];

    return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType)
{
    unsigned char* bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS)
    {
        return ret;
    }

    *((int32_t*)bufferPtr) = blockType;

    StaticBuffer::blockAllocMap[this->blockNum] = blockType;

    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if(ret != SUCCESS)
    {
        return ret;
    }

    return SUCCESS;
}

void BlockBuffer::releaseBlock()
{
    if(this->blockNum < 0 || this->blockNum >= DISK_BLOCKS)
    {
        return;
    }
    else
    {
        int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
        if(bufferNum == E_BLOCKNOTINBUFFER)
        {
            return;
        }
        
        StaticBuffer::metainfo[bufferNum].free = true;
        StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;
        this->blockNum = INVALID_BLOCKNUM;
        
    }
}

int compareAttrs(Attribute attr1, Attribute attr2, int attrType)
{
    double diff;

    if(attrType == STRING)
    {
        diff = strcmp(attr1.sVal, attr2.sVal);
    }
    else
    {
        diff = attr1.nVal - attr2.nVal;
    }

    if(diff > 0) return 1;
    if(diff < 0) return -1;
    if(diff == 0) return 0;
}

int IndInternal::getEntry(void *ptr, int indexNum)
{
    if(indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL)
    {
        return E_OUTOFBOUND;
    }

    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS)
    {
        return ret;
    }

    struct InternalEntry *internalEntry = (struct InternalEntry*) ptr;

    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

    memcpy(&(internalEntry->lChild), entryPtr, sizeof(int32_t));
    memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
    memcpy(&(internalEntry->rChild), entryPtr + 20, 4);

    return SUCCESS;
}

int IndLeaf::getEntry(void *ptr, int indexNum)
{
    if(indexNum < 0 || indexNum >= MAX_KEYS_LEAF)
    {
        return E_OUTOFBOUND;
    }

    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS)
    {
        return ret;
    }

    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
    memcpy((struct Index*)ptr, entryPtr, LEAF_ENTRY_SIZE);

    return SUCCESS;
}

int IndInternal::setEntry(void *ptr, int indexNum)
{
    return 0;
}

int IndLeaf::setEntry(void *ptr, int indexNum)
{
    return 0;
}
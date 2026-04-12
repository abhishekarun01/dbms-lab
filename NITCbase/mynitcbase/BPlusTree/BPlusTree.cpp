#include "BPlusTree.h"

#include <cstring>

int BPlusTree::numTreeComparisons;

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op)
{
    IndexId searchIndex;
    AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);

    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    int block, index;

    if(searchIndex.block == -1 && searchIndex.index == -1)
    {
        block = attrCatEntry.rootBlock;
        index = 0;

        if(block == -1)
        {
            return RecId{-1, -1};
        }
    }
    else
    {
        block = searchIndex.block;
        index = searchIndex.index + 1;

        IndLeaf leaf(block);

        HeadInfo leafHead;

        leaf.getHeader(&leafHead);

        if(index >= leafHead.numEntries)
        {
            block = leafHead.rblock;
            index = 0;

            if(block == -1)
            {
                return RecId{-1, -1};
            }
        }
    }

    while(StaticBuffer::getStaticBlockType(block) == 1)
    {
        IndInternal internalBlk(block);

        HeadInfo intHead;
        internalBlk.getHeader(&intHead);

        InternalEntry intEntry;

        if(op == NE || op == LT || op == LE)
        {
            internalBlk.getEntry(&intEntry, 0);
            block = intEntry.lChild;
        }
        else
        {
            int i = 0;
            while(i < intHead.numEntries)
            {
                internalBlk.getEntry(&intEntry, i);
                int val = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
                BPlusTree::numTreeComparisons++;

                if(((op == EQ || op == GE) && val >= 0) || (op == GT && val > 0))
                {
                    break;
                }
                
                i++;
            }

            if(i < intHead.numEntries)
            {
                block = intEntry.lChild;
            }
            else
            {
                block = intEntry.rChild;
            }
        }
    }

    while(block != -1)
    {
        IndLeaf leafBlk(block);
        HeadInfo leafHead;
        leafBlk.getHeader(&leafHead);

        Index leafEntry;

        while(index < leafHead.numEntries)
        {
            leafBlk.getEntry(&leafEntry, index);

            int cmpval = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);
            BPlusTree::numTreeComparisons++;

            if
            (
                (op == EQ && cmpval == 0) ||
                (op == LE && cmpval <= 0) ||
                (op == LT && cmpval < 0)  ||
                (op == GT && cmpval > 0)  ||
                (op == GE && cmpval >= 0) ||
                (op == NE && cmpval != 0)
            )
            {
                IndexId searchIndex = {block, index};
                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
                return RecId{leafEntry.block, leafEntry.slot};
            }
            else if((op == EQ || op == LE || op == LT) && cmpval > 0)
            {
                return RecId{-1, -1};
            }
            ++index;
        }

        if(op != NE)
        {
            break;
        }
        
        block = leafHead.rblock;
        index = 0;
    }

    return RecId{-1, -1};
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE])
{
    if(relId == RELCAT_RELID || relId == ATTRCAT_RELID)
    {
        return E_NOTPERMITTED;
    }

    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    if(ret != SUCCESS)
    {
        return ret;
    }

    if(attrCatEntry.rootBlock != -1)
    {
        return SUCCESS;
    }

    IndLeaf rootBlockBuf;

    int rootBlock = rootBlockBuf.getBlockNum();

    if(rootBlock == E_DISKFULL)
    {
        return E_DISKFULL;
    }

    attrCatEntry.rootBlock = rootBlock;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    int block = relCatEntry.firstBlk;

    while(block != -1)
    {
        RecBuffer recBuffer(block);
        unsigned char slotMap[relCatEntry.numSlotsPerBlk];
        recBuffer.getSlotMap(slotMap);
        int i;

        for(i = 0; i < relCatEntry.numSlotsPerBlk; i++)
        {
            if(slotMap[i] == SLOT_OCCUPIED)
            {
                Attribute record[relCatEntry.numAttrs];
                recBuffer.getRecord(record, i);
    
                RecId recId;
                recId.block = block;
                recId.slot = i;
    
                int retVal = bPlusInsert(relId, attrName, record[attrCatEntry.offset], recId);
    
                if(retVal == E_DISKFULL)
                {
                    return E_DISKFULL;
                }
            }
        }

        HeadInfo head;
        recBuffer.getHeader(&head);

        block = head.rblock;
    }

    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum)
{
    if(rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }

    int type = StaticBuffer::getStaticBlockType(rootBlockNum);

    if(type == IND_LEAF)
    {
        IndLeaf rootBlock(rootBlockNum);
        rootBlock.releaseBlock();

        return SUCCESS;
    }
    else if(type == IND_INTERNAL)
    {
        IndInternal rootBlock(rootBlockNum);

        HeadInfo head;
        rootBlock.getHeader(&head);

        InternalEntry internalEntry;
        rootBlock.getEntry(&internalEntry, 0);
        BPlusTree::bPlusDestroy(internalEntry.lChild);

        for(int i = 0; i < head.numEntries; i++)
        {
            rootBlock.getEntry(&internalEntry, i);
            BPlusTree::bPlusDestroy(internalEntry.rChild);
        }

        rootBlock.releaseBlock();

        return SUCCESS;
    }
    else
    {
        return E_INVALIDBLOCK;
    }    
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId)
{
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if(ret != SUCCESS)
    {
        return ret;
    }

    int blockNum = attrCatEntry.rootBlock;

    if(blockNum == -1)
    {
        return E_NOINDEX;
    }

    int leafBlkNum = findLeafToInsert(blockNum, attrVal, attrCatEntry.attrType);

    struct Index entry;
    entry.attrVal = attrVal;
    entry.block = recId.block;
    entry.slot = recId.slot;
    ret = insertIntoLeaf(relId, attrName, leafBlkNum, entry);

    if(ret == E_DISKFULL)
    {
        BPlusTree::bPlusDestroy(blockNum);
        attrCatEntry.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

        return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType)
{
    int blockNum = rootBlock;

    while(StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF)
    {
        IndInternal internalBlk(blockNum);

        HeadInfo head;
        internalBlk.getHeader(&head);

        int i;
        
        for(i = 0; i < head.numEntries; i++)
        {
            InternalEntry entry;
            internalBlk.getEntry(&entry, i);

            int retVal = compareAttrs(attrVal, entry.attrVal, attrType);

            if(retVal <= 0)
            {
                break;
            }
        }

        if(i == head.numEntries)
        {
            InternalEntry entry;
            internalBlk.getEntry(&entry, head.numEntries - 1);
            blockNum = entry.rChild;
        }
        else
        {
            InternalEntry entry;
            internalBlk.getEntry(&entry, i);
            blockNum = entry.lChild;
        }
    }

    return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry)
{
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if(ret != SUCCESS)
    {
        return ret;
    }

    IndLeaf leafBlk(blockNum);

    HeadInfo head;
    leafBlk.getHeader(&head);

    Index indices[head.numEntries + 1];

    Index leafEntry;
    int index = 0;
    int i;

    for(i = 0; i < head.numEntries; i++)
    {
        leafBlk.getEntry(&leafEntry, i);
        if(compareAttrs(leafEntry.attrVal, indexEntry.attrVal, attrCatEntry.attrType) >= 0)
        {
            break;
        }

        indices[index] = leafEntry;
        index++;
    }

    indices[index] = indexEntry;
    index++;

    for(; i < head.numEntries; i++)
    {
        leafBlk.getEntry(&leafEntry, i);
        indices[index] = leafEntry;
        index++;
    }

    if(head.numEntries != MAX_KEYS_LEAF)
    {
        head.numEntries++;
        leafBlk.setHeader(&head);

        for(int i = 0; i < head.numEntries; i++)
        {
            leafEntry = indices[i];
            leafBlk.setEntry(&leafEntry, i);

        }
        
        return SUCCESS;
    }

    int newRightBlk = splitLeaf(blockNum, indices);

    if(newRightBlk == E_DISKFULL)
    {
        return E_DISKFULL;
    }

    if(head.pblock != -1)
    {
        InternalEntry internalEntry;
        internalEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
        internalEntry.lChild = blockNum;
        internalEntry.rChild = newRightBlk;

        int retVal = insertIntoInternal(relId, attrName, head.pblock, internalEntry);
        if(retVal == E_DISKFULL)
        {
            return E_DISKFULL;
        }
    }
    else
    {
        int retVal = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
        if(retVal == E_DISKFULL)
        {
            return E_DISKFULL;
        }
    }
    
    return SUCCESS;
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[])
{
    IndLeaf rightBlk;
    IndLeaf leftBlk(leafBlockNum);

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = leftBlk.getBlockNum();

    if(rightBlkNum == E_DISKFULL || leftBlkNum == E_DISKFULL)
    {
        return E_DISKFULL;
    }

    HeadInfo leftHeader, rightHeader;
    rightBlk.getHeader(&rightHeader);
    leftBlk.getHeader(&leftHeader);

    rightHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    rightHeader.pblock = leftHeader.pblock;
    rightHeader.lblock = leftBlkNum;
    rightHeader.rblock = leftHeader.rblock;

    rightBlk.setHeader(&rightHeader);

    leftHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    leftHeader.rblock = rightBlkNum;

    leftBlk.setHeader(&leftHeader);

    for (int i = 0; i <= MIDDLE_INDEX_LEAF; i++)
    {
        leftBlk.setEntry(&indices[i], i);
        rightBlk.setEntry(&indices[i + 32], i);
    }

    return rightBlkNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry)
{
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if(ret != SUCCESS)
    {
        return ret;
    }

    IndInternal intBlk(intBlockNum);

    HeadInfo head;
    intBlk.getHeader(&head);
    
    InternalEntry internalEntries[head.numEntries + 1];

    InternalEntry internalEntry;
    int index = 0;
    int i = 0;

    for(i = 0; i < head.numEntries; i++)
    {
        intBlk.getEntry(&internalEntry, i);
        if(compareAttrs(internalEntry.attrVal, intEntry.attrVal, attrCatEntry.attrType) >= 0)
        {
            if(index > 0)
            {
                internalEntries[index-1].rChild = intEntry.lChild;
            }
            break;
        }

        internalEntries[index++] = internalEntry;
    }

    internalEntries[index++] = intEntry;

    if(i < head.numEntries)
    {
        intBlk.getEntry(&internalEntry, i);
        internalEntry.lChild = intEntry.rChild;
        internalEntries[index] = internalEntry;
        index++;
        i++;
    }

    for(; i < head.numEntries; i++)
    {
        intBlk.getEntry(&internalEntry, i);
        internalEntries[index] = internalEntry;
        index++;
    }

    if(head.numEntries != MAX_KEYS_INTERNAL)
    {
        head.numEntries++;
        intBlk.setHeader(&head);

        for(int i = 0; i < head.numEntries; i++)
        {
            intBlk.setEntry(&internalEntries[i], i);
        }

        return SUCCESS;
    }

    int newRightBlk = splitInternal(intBlockNum, internalEntries);

    if(newRightBlk == E_DISKFULL)
    {
        BPlusTree::bPlusDestroy(intEntry.rChild);
        return E_DISKFULL;
    }

    if(head.pblock != -1)
    {
        InternalEntry entry;
        entry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;
        entry.lChild = intBlockNum;
        entry.rChild = newRightBlk;

        int retVal = insertIntoInternal(relId, attrName, head.pblock, entry);
        if(retVal == E_DISKFULL)
        {
            return E_DISKFULL;
        }
    }
    else
    {
        int retVal = createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk);
        if(retVal == E_DISKFULL)
        {
            return E_DISKFULL;
        }
    }

    return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[])
{
    IndInternal rightBlk;
    IndInternal leftBlk(intBlockNum);

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = leftBlk.getBlockNum();

    if(rightBlkNum == E_DISKFULL || leftBlkNum == E_DISKFULL)
    {
        return E_DISKFULL;
    }

    HeadInfo leftHead, rightHead;
    rightBlk.getHeader(&rightHead);
    leftBlk.getHeader(&leftHead);

    rightHead.numEntries = MAX_KEYS_INTERNAL / 2;
    rightHead.pblock = leftHead.pblock;
    rightBlk.setHeader(&rightHead);

    leftHead.numEntries = MAX_KEYS_INTERNAL / 2;
    leftBlk.setHeader(&leftHead);

    for (int i = 0; i < 50; i++)       
    {
        leftBlk.setEntry(&internalEntries[i], i);
        rightBlk.setEntry(&internalEntries[i+51], i);
    }

    int type = StaticBuffer::getStaticBlockType(internalEntries[0].lChild);
    BlockBuffer blockBuffer(internalEntries[MIDDLE_INDEX_INTERNAL + 1].lChild);

    HeadInfo blockHeader;
    blockBuffer.getHeader(&blockHeader);
    blockHeader.pblock = rightBlkNum;
    blockBuffer.setHeader(&blockHeader);

    for(int i = 0; i < MIDDLE_INDEX_INTERNAL; i++)
    {
        BlockBuffer blockBuffer(internalEntries[i + 51].rChild);
        blockBuffer.getHeader(&blockHeader);
        blockHeader.pblock = rightBlkNum;
        blockBuffer.setHeader(&blockHeader);
    }

    return rightBlkNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild)
{
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if(ret != SUCCESS)
    {
        return ret;
    }

    IndInternal newRootBlk;

    int newRootBlkNum = newRootBlk.getBlockNum();

    if(newRootBlkNum == E_DISKFULL)
    {
        BPlusTree::bPlusDestroy(rChild);
        return E_DISKFULL;
    }

    HeadInfo head;
    newRootBlk.getHeader(&head);
    head.numEntries = 1;
    newRootBlk.setHeader(&head);

    InternalEntry entry;
    entry.lChild = lChild;
    entry.attrVal = attrVal;
    entry.rChild = rChild;
    newRootBlk.setEntry(&entry, 0);

    BlockBuffer leftChild(lChild);
    BlockBuffer rightChild(rChild);

    HeadInfo leftHeader, rightHeader;

    leftChild.getHeader(&leftHeader);
    rightChild.getHeader(&rightHeader);
    
    leftHeader.pblock = newRootBlkNum;
    rightHeader.pblock = newRootBlkNum;
    
    leftChild.setHeader(&leftHeader);
    rightChild.setHeader(&rightHeader);

    attrCatEntry.rootBlock = newRootBlkNum;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

    return SUCCESS;
}
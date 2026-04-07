#include "BlockAccess.h"

#include <cstring>
int BlockAccess::numLinearComparisons;

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op)
{
    RecId searchIndex;
    RelCacheTable::getSearchIndex(relId, &searchIndex);

    int block;
    int slot;

    if(searchIndex.block == -1 && searchIndex.slot == -1)
    {
        RelCatEntry relCatBuf;
        RelCacheTable::getRelCatEntry(relId, &relCatBuf);

        block = relCatBuf.firstBlk;
        slot = 0;
    }
    else
    {
        block = searchIndex.block;
        slot = searchIndex.slot + 1;
    }

    while(block != -1)
    {
        RecBuffer bufferBlock(block);

        HeadInfo head;
        bufferBlock.getHeader(&head);

        Attribute record[head.numAttrs];
        bufferBlock.getRecord(record, slot);

        unsigned char slotMap[head.numSlots];
        bufferBlock.getSlotMap(slotMap);

        if(slot >= head.numSlots)
        {
            block = head.rblock;
            slot = 0;
            continue;
        }

        if(slotMap[slot] == SLOT_UNOCCUPIED)
        {
            slot++;
            continue;
        }

        AttrCatEntry attrCatBuf;
        AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);

        Attribute attr = record[attrCatBuf.offset];
        int compareVal = compareAttrs(attr, attrVal, attrCatBuf.attrType);
        BlockAccess::numLinearComparisons++;

        if
        (
            (op == NE && compareVal != 0) ||
            (op == LT && compareVal < 0)  ||
            (op == LE && compareVal <= 0) ||
            (op == EQ && compareVal == 0) ||
            (op == GT && compareVal > 0)  ||
            (op == GE && compareVal >= 0) 
        )
        {
            searchIndex.block = block;
            searchIndex.slot = slot;
            RelCacheTable::setSearchIndex(relId, &searchIndex);
            return RecId{block, slot};
        }

        slot++;

    }

    return RecId{-1 , -1};
}

int BlockAccess::renameRelation(char* oldName, char* newName)
{
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelName;
    strcpy(newRelName.sVal, newName);

    RecId index = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, newRelName, EQ);

    if(!(index.block == -1 && index.slot == -1))
    {
        return E_RELEXIST;
    }

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute oldRelName;
    strcpy(oldRelName.sVal, oldName);

    index = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, oldRelName, EQ);

    if(index.block == -1 && index.slot == -1)
    {
        return E_RELNOTEXIST;
    }

    RecBuffer relCatBuffer(RELCAT_BLOCK);
    Attribute record[RELCAT_NO_ATTRS];

    relCatBuffer.getRecord(record, index.slot);
    strcpy(record[RELCAT_REL_NAME_INDEX].sVal, newName);
    relCatBuffer.setRecord(record, index.slot);
    int numAttrs = record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    for(int i = 0; i < numAttrs; i++)
    {   
        index = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, oldRelName, EQ);
        
        RecBuffer attrCatBuffer(index.block);
        Attribute attrRecord[ATTRCAT_NO_ATTRS];

        attrCatBuffer.getRecord(attrRecord, index.slot);
        strcpy(attrRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);
        attrCatBuffer.setRecord(attrRecord, index.slot);
    }

    return SUCCESS;
}

int BlockAccess::renameAttribute(char *relName, char *oldName, char *newName)
{
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    RecId index = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttr, EQ);

    if(index.block == -1 && index.slot == -1)
    {
        return E_RELNOTEXIST;
    }

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    RecId attrToRenameRecId = {-1 , -1};
    Attribute attrRecord[ATTRCAT_NO_ATTRS];

    while(true)
    {
        index = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

        if(index.block == -1 && index.slot == -1)
        {
            break;
        }

        RecBuffer attrCatBuffer(index.block);
        attrCatBuffer.getRecord(attrRecord, index.slot);

        if(strcmp(attrRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0)
        {
            attrToRenameRecId.block = index.block;
            attrToRenameRecId.slot = index.slot;
        }

        if(strcmp(attrRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0)
        {
            return E_ATTREXIST;
        }
    }

    if(attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1)
    {
        return E_ATTRNOTEXIST;
    }

    RecBuffer attrCatBuffer(attrToRenameRecId.block);
    attrCatBuffer.getRecord(attrRecord, attrToRenameRecId.slot);
    strcpy(attrRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);
    attrCatBuffer.setRecord(attrRecord, attrToRenameRecId.slot);

    return SUCCESS;
}

int BlockAccess::insert(int relId, Attribute *record)
{
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    int blockNum = relCatEntry.firstBlk;

    RecId recId = {-1, -1};

    int numSlots = relCatEntry.numSlotsPerBlk;
    int numAttrs = relCatEntry.numAttrs;

    int prevBlockNum = -1;

    while(blockNum != -1)
    {
        RecBuffer blockBuffer(blockNum);
        
        struct HeadInfo head;
        blockBuffer.getHeader(&head);

        unsigned char slotMap[numSlots];
        blockBuffer.getSlotMap(slotMap);

        int freeSlot = -1;
        for(int i = 0; i < numSlots; i++)
        {
            if(slotMap[i] == SLOT_UNOCCUPIED)
            {
                freeSlot = i;
                break;
            }
        }

        
        if(freeSlot != -1)
        {
            recId = {blockNum, freeSlot};
            break;
        }
        else
        {
            prevBlockNum = blockNum;
            blockNum = head.rblock;
        }
    }

    if(recId.block == -1 && recId.slot == -1)
    {
        if(relId == RELCAT_RELID)
        {
            return E_MAXRELATIONS;
        }
        else
        {
            RecBuffer newBuffer;
            int newBlock = newBuffer.getBlockNum();
            
            if(newBlock == E_DISKFULL)
            {
                return E_DISKFULL;
            }

            recId = {newBlock, 0};

            struct HeadInfo head;
            head.blockType = REC;
            head.lblock = prevBlockNum;
            head.rblock = -1;
            head.pblock = -1;
            head.numAttrs = numAttrs;
            head.numSlots = numSlots;
            head.numEntries = 0;
            newBuffer.setHeader(&head);

            unsigned char newSlotMap[numSlots];

            for(int i = 0; i < numSlots; i++)
            {
                newSlotMap[i] = SLOT_UNOCCUPIED;
            }
            
            newBuffer.setSlotMap(newSlotMap);

            if(prevBlockNum != -1)
            {
                RecBuffer prevBuffer(prevBlockNum);
                
                struct HeadInfo prevHeader;
                prevBuffer.getHeader(&prevHeader);
                prevHeader.rblock = recId.block;
                prevBuffer.setHeader(&prevHeader);
            }
            else
            {
                relCatEntry.firstBlk = recId.block;
                RelCacheTable::setRelCatEntry(relId, &relCatEntry);
            }

            relCatEntry.lastBlk = recId.block;
            RelCacheTable::setRelCatEntry(relId, &relCatEntry);
        }
    }

    RecBuffer insertIntoBlock(recId.block);
    insertIntoBlock.setRecord(record, recId.slot);

    unsigned char updatedSlotMap[numSlots];
    insertIntoBlock.getSlotMap(updatedSlotMap);
    updatedSlotMap[recId.slot] = SLOT_OCCUPIED;
    insertIntoBlock.setSlotMap(updatedSlotMap);

    struct HeadInfo updatedHeader;
    insertIntoBlock.getHeader(&updatedHeader);
    updatedHeader.numEntries++;
    insertIntoBlock.setHeader(&updatedHeader);

    relCatEntry.numRecs++;
    RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    
    return SUCCESS;
}

int BlockAccess::search(int relId, Attribute* record, char attrName[ATTR_SIZE], Attribute attrVal, int op)
{
    RecId recId;

    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if(ret != SUCCESS)
    {
        return ret;
    }

    int rootBlock = attrCatEntry.rootBlock;

    if(rootBlock == -1)
    {
        recId = BlockAccess::linearSearch(relId, attrName, attrVal, op);
    }
    else
    {
        recId = BPlusTree::bPlusSearch(relId, attrName, attrVal, op);
    }

    if(recId.block == -1 && recId.slot == -1)
    {
        return E_NOTFOUND;
    }

    RecBuffer bufferBlock(recId.block);
    bufferBlock.getRecord(record, recId.slot);

    return SUCCESS;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE])
{
    if(strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
    {
        return E_NOTPERMITTED;
    }

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    RecId recId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, relNameAttr, EQ);

    if(recId.block == -1 && recId.slot == -1)
    {
        return E_RELNOTEXIST;
    }

    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];

    RecBuffer recBuffer(recId.block);
    recBuffer.getRecord(relCatEntryRecord, recId.slot);

    int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    int numAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    while(firstBlock != -1)
    {
        RecBuffer bufferBlock(firstBlock);
        
        struct HeadInfo head;
        bufferBlock.getHeader(&head);

        firstBlock = head.rblock;

        bufferBlock.releaseBlock();
    }

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    int numAttrsDeleted = 0;

    while(true)
    {
        RecId attrCatRecId;

        attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_RELNAME, relNameAttr, EQ);

        if(attrCatRecId.block == -1 && attrCatRecId.slot == -1)
        {
            break;
        }

        numAttrsDeleted++;

        RecBuffer bufferBlock(attrCatRecId.block);
        
        struct HeadInfo head;
        bufferBlock.getHeader(&head);

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        bufferBlock.getRecord(attrCatRecord, attrCatRecId.slot);

        int rootBlock = attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

        unsigned char slotMap[head.numSlots];
        bufferBlock.getSlotMap(slotMap);
        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        bufferBlock.setSlotMap(slotMap);

        head.numEntries--;
        bufferBlock.setHeader(&head);

        if(head.numEntries == 0)
        {
            RecBuffer leftBuffer(head.lblock);
            struct HeadInfo leftBlockHeader;
            leftBuffer.getHeader(&leftBlockHeader);
            leftBlockHeader.rblock = head.rblock;
            leftBuffer.setHeader(&leftBlockHeader);

            if(head.rblock != -1)
            {
                RecBuffer rightBuffer(head.rblock);
                struct HeadInfo rightBlockHeader;
                leftBuffer.getHeader(&rightBlockHeader);
                rightBlockHeader.lblock = head.lblock;
                leftBuffer.setHeader(&rightBlockHeader);
            }
            else
            {
                RelCatEntry relCatEntry;
                RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);
                relCatEntry.lastBlk = head.lblock;
                RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntry);
            }

            bufferBlock.releaseBlock();
        }

        if(rootBlock != -1)
        {

        }
    }

    RecBuffer relCatBuffer(RELCAT_BLOCK);
    struct HeadInfo relCatHeader;
    relCatBuffer.getHeader(&relCatHeader);
    relCatHeader.numEntries--;
    relCatBuffer.setHeader(&relCatHeader);

    unsigned char relCatSlotMap[relCatHeader.numSlots];
    relCatBuffer.getSlotMap(relCatSlotMap);
    relCatSlotMap[recId.slot] = SLOT_UNOCCUPIED;
    relCatBuffer.setSlotMap(relCatSlotMap);

    RelCatEntry relationCatEntry;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relationCatEntry);
    relationCatEntry.numRecs--;
    RelCacheTable::setRelCatEntry(RELCAT_RELID, &relationCatEntry);

    RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relationCatEntry);
    relationCatEntry.numRecs -= numAttrsDeleted;
    RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relationCatEntry);

    return SUCCESS;
}

int BlockAccess::project(int relId, Attribute* record)
{
    RecId searchIndex;

    RelCacheTable::getSearchIndex(relId, &searchIndex);

    int block, slot;

    if(searchIndex.block == -1 && searchIndex.slot == -1)
    {
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);

        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else
    {
        block = searchIndex.block;
        slot = searchIndex.slot + 1;
    }

    while(block != -1)
    {
        RecBuffer recBuffer(block);

        struct HeadInfo head;
        recBuffer.getHeader(&head);

        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);

        if(slot >= head.numSlots)
        {
            block = head.rblock;
            slot = 0;
        }
        else if(slotMap[slot] == SLOT_UNOCCUPIED)
        {
            slot++;
        }
        else
        {
            break;
        }
    }

    if(block == -1)
    {
        return E_NOTFOUND;
    }

    RecId nextRecord = {block, slot};
    RelCacheTable::setSearchIndex(relId, &nextRecord);

    RecBuffer buffer(nextRecord.block);
    buffer.getRecord(record, nextRecord.slot);

    return SUCCESS;
}

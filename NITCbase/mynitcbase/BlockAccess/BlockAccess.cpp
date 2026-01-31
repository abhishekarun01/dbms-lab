#include "BlockAccess.h"

#include <cstring>

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

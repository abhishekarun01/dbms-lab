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

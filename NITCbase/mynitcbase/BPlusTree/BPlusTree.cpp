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
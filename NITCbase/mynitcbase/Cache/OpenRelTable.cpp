#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

OpenRelTable::OpenRelTable() 
{
    for(int i = 0; i < MAX_OPEN; i++)
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }

    //-----------------------RELATION CACHE TABLE-------------------------------

    // Setting up the Relation Catalog relation in the Relation Cache Table
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

    struct RelCacheEntry relCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

    RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;
    // ----------------------------------------------------------------------------

    // Setting up the Attribute Catalog relation in the Relation Cache Table
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

    RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry;
    // ----------------------------------------------------------------------------

    // Setting up the Student relation in the Relation Cache Table
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT + 1);
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT + 1;

    RelCacheTable::relCache[ATTRCAT_RELID + 1] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[ATTRCAT_RELID + 1]) = relCacheEntry;
    // ----------------------------------------------------------------------------


    //-----------------------ATTRIBUTE CACHE TABLE-------------------------------

    //setting up the Relation Catalog relation in the Attribute Cache Table
    RecBuffer attrCatBlock(ATTRCAT_BLOCK);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    AttrCacheEntry* attrCacheHead = nullptr;
    AttrCacheEntry* prev = nullptr;


    for(int i = 0; i < RELCAT_NO_ATTRS; i++)
    {
        AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        attrCatBlock.getRecord(attrCatRecord, i);
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
        attrCacheEntry->recId.block = ATTRCAT_BLOCK;
        attrCacheEntry->recId.slot = i;
        attrCacheEntry->next = nullptr;
        
        if(attrCacheHead == nullptr)
        {
            attrCacheHead = attrCacheEntry;
        }
        else
        {
            prev->next = attrCacheEntry;
        }

        prev = attrCacheEntry;
    }

    AttrCacheTable::attrCache[RELCAT_RELID] = attrCacheHead;
    // ----------------------------------------------------------------------------

    //setting up the Attribute Catalog relation in the Attribute Cache Table
    attrCacheHead = nullptr;
    prev = nullptr;

    for(int i = RELCAT_NO_ATTRS; i < RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS; i++)
    {
        AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        attrCatBlock.getRecord(attrCatRecord, i);
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
        attrCacheEntry->recId.block = ATTRCAT_BLOCK;
        attrCacheEntry->recId.slot = i;
        attrCacheEntry->next = nullptr;

        if(attrCacheHead == nullptr)
        {
            attrCacheHead = attrCacheEntry;
        }
        else
        {
            prev->next = attrCacheEntry;
        }

        prev = attrCacheEntry;
    }

    AttrCacheTable::attrCache[ATTRCAT_RELID] = attrCacheHead;
    // ----------------------------------------------------------------------------


    //setting up the Attribute Catalog relation in the Attribute Cache Table
    attrCacheHead = nullptr;
    prev = nullptr;

    for(int i = RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS; i < RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS + 4; i++)
    {
        AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        attrCatBlock.getRecord(attrCatRecord, i);
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
        attrCacheEntry->recId.block = ATTRCAT_BLOCK;
        attrCacheEntry->recId.slot = i;
        attrCacheEntry->next = nullptr;

        if(attrCacheHead == nullptr)
        {
            attrCacheHead = attrCacheEntry;
        }
        else
        {
            prev->next = attrCacheEntry;
        }

        prev = attrCacheEntry;
    }

    AttrCacheTable::attrCache[ATTRCAT_RELID+1] = attrCacheHead;
}

OpenRelTable::~OpenRelTable() 
{
    for(int i = 0; i < MAX_OPEN; i++)
    {
        free(RelCacheTable::relCache[i]);
        for(AttrCacheEntry* entry = AttrCacheTable::attrCache[i]; entry != nullptr;)
        {
            AttrCacheEntry* nextEntry = entry->next;
            free(entry);
            entry = nextEntry;
        }
    }
};
#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

int OpenRelTable::getRelId(char relName[ATTR_SIZE])
{
    if(!strcmp(relName, RELCAT_RELNAME))
    {
        return RELCAT_RELID;
    }

    if(!strcmp(relName, ATTRCAT_RELNAME))
    {
        return ATTRCAT_RELID;
    }

    if(!strcmp(relName, "Students"))
    {
        return 2;
    }

    return E_RELNOTOPEN;
}

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
    struct RelCacheEntry relCacheEntry;
    
    for(int slot = 0; slot <= 2; slot++)
    {
        relCatBlock.getRecord(relCatRecord, slot);
        RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
        relCacheEntry.recId.block = RELCAT_BLOCK;
        relCacheEntry.recId.slot = slot;
    
        RelCacheTable::relCache[slot] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
        *(RelCacheTable::relCache[slot]) = relCacheEntry;
    }
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


    //setting up the Students relation in the Attribute Cache Table
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

    AttrCacheTable::attrCache[ATTRCAT_RELID + 1] = attrCacheHead;
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
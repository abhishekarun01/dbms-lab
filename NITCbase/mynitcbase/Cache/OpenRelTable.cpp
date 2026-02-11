#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

int OpenRelTable::getRelId(char relName[ATTR_SIZE])
{
    for(int i = 0; i < MAX_OPEN; i++)
    {
        if(strcmp(OpenRelTable::tableMetaInfo[i].relName, relName) == 0 && OpenRelTable::tableMetaInfo[i].free == false)
        {
            return i;
        }
    }

    return E_RELNOTOPEN;
}

AttrCacheEntry* createList(int length) {
    AttrCacheEntry* head = (AttrCacheEntry*) malloc(sizeof(AttrCacheEntry));
    AttrCacheEntry* tail = head;
    for (int i = 1; i < length; i++) {
        tail->next = (AttrCacheEntry*) malloc(sizeof(AttrCacheEntry));
        tail = tail->next;
    }
    tail->next = nullptr;
    return head;
}

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable::OpenRelTable() 
{
    for(int i = 0; i < MAX_OPEN; i++)
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
        OpenRelTable::tableMetaInfo[i].free = true;
    }

    //-----------------------RELATION CACHE TABLE-------------------------------

    // Setting up the Relation Catalog relation in the Relation Cache Table
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    struct RelCacheEntry relCacheEntry;
    
    for(int slot = 0; slot <= ATTRCAT_RELID; slot++)
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

    //setting up the tableMetaInfo entries of Relation Catalog
    OpenRelTable::tableMetaInfo[RELCAT_RELID].free = false;
    strcpy(OpenRelTable::tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME); 
    // -------------------------------------------------------

    //setting up the tableMetaInfo entries of Attribute Catalog
    OpenRelTable::tableMetaInfo[ATTRCAT_RELID].free = false;
    strcpy(OpenRelTable::tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);
    // -------------------------------------------------------
}

OpenRelTable::~OpenRelTable() 
{
    for(int i = 0; i < 2; i++)
    {
        free(RelCacheTable::relCache[i]);
        for(AttrCacheEntry* entry = AttrCacheTable::attrCache[i]; entry != nullptr;)
        {
            AttrCacheEntry* nextEntry = entry->next;
            free(entry);
            entry = nextEntry;
        }

        for(int i = 2; i < MAX_OPEN; i++)
        {
            if(!OpenRelTable::tableMetaInfo[i].free)
            {
                OpenRelTable::closeRel(i);
            }
        }
    }
};

int OpenRelTable::getFreeOpenRelTableEntry()
{
    for(int i = 0; i < MAX_OPEN; i++)
    {
        if(OpenRelTable::tableMetaInfo[i].free)
        {
            return i;
        }
    }

    return E_CACHEFULL;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE])
{
    int ret = OpenRelTable::getRelId(relName);
    
    if(ret >= 0 && ret < MAX_OPEN)
    {
        return ret;
    }

    int relId = OpenRelTable::getFreeOpenRelTableEntry();

    if(relId == E_CACHEFULL)
    {
        return E_CACHEFULL;
    }

    /****** Setting up Relation Cache entry for the relation ******/

    Attribute relationName;
    memcpy(relationName.sVal, relName, ATTR_SIZE);
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char*) RELCAT_ATTR_RELNAME, relationName, EQ);

    if(relCatRecId.block == -1 && relCatRecId.slot == -1)
    {
        return E_RELNOTEXIST;
    }

    RecBuffer relCatBuffer(relCatRecId.block);
    Attribute record[RELCAT_NO_ATTRS];
    relCatBuffer.getRecord(record, relCatRecId.slot);

    RelCatEntry relCatEntry;
    RelCacheTable::recordToRelCatEntry(record, &relCatEntry);

    RelCacheTable::relCache[relId] = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    RelCacheTable::relCache[relId]->recId = relCatRecId;
    RelCacheTable::relCache[relId]->relCatEntry = relCatEntry;

    /*------------------------------------------------------------*/

    /****** Setting up Attribute Cache entry for the relation ******/

    int numAttrs = relCatEntry.numAttrs;
    AttrCacheEntry* listHead = createList(numAttrs);
    AttrCacheEntry* node = listHead;

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    for(int i = 0; i < numAttrs; i++)
    {
        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relationName, EQ);

        if(attrCatRecId.block == -1 && attrCatRecId.slot == -1)
        {
            break;
        }

        RecBuffer attrCatBuffer(attrCatRecId.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrCatRecord, attrCatRecId.slot);

        AttrCatEntry attrCatEntry;
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCatEntry);

        node->recId = attrCatRecId;
        node->attrCatEntry = attrCatEntry;
        node = node->next;
    }

    AttrCacheTable::attrCache[relId] = listHead;

    OpenRelTable::tableMetaInfo[relId].free = false;
    memcpy(OpenRelTable::tableMetaInfo[relId].relName, relName, ATTR_SIZE);

    return relId;
}

int OpenRelTable::closeRel(int relId)
{
    if(relId == RELCAT_RELID || relId == ATTRCAT_RELID)
    {
        return E_NOTPERMITTED;
    }

    if(relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if(OpenRelTable::tableMetaInfo[relId].free)
    {
        return E_RELNOTOPEN;
    }

    if(RelCacheTable::relCache[relId]->dirty)
    {
        Attribute record[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[relId]->relCatEntry), record);

        RecId recId = RelCacheTable::relCache[relId]->recId;

        RecBuffer relCatBlock(recId.block);
        relCatBlock.setRecord(record, recId.slot);
    }

    free(RelCacheTable::relCache[relId]);
    for(AttrCacheEntry* entry = AttrCacheTable::attrCache[relId]; entry != nullptr;)
    {
        AttrCacheEntry* nextEntry = entry->next;
        free(entry);
        entry = nextEntry;
    }
    
    OpenRelTable::tableMetaInfo[relId].free = true;
    RelCacheTable::relCache[relId] = nullptr;
    AttrCacheTable::attrCache[relId] = nullptr;

    return SUCCESS;
}
#include "Algebra.h"
#include <iostream>
#include <cstring>
#include <cstdlib>

bool isNumber(char *str);

int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE])
{
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if(srcRelId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }

    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
    
    if(ret == E_ATTRNOTEXIST)
    {
        return ret;
    }

    int type = attrCatEntry.attrType;
    Attribute attrVal;
    if(type == NUMBER)
    {
        if(isNumber(strVal))
        {
            attrVal.nVal = atof(strVal);
        }
        else
        {
            return E_ATTRTYPEMISMATCH;
        }
    }
    else if(type == STRING)
    {
        strcpy(attrVal.sVal, strVal);
    }

    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    int src_nAttrs = relCatEntry.numAttrs;

    char attr_names[src_nAttrs][ATTR_SIZE];
    int attr_types[src_nAttrs];

    for(int i = 0; i < src_nAttrs; i++)
    {
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
        strcpy(attr_names[i], attrCatEntry.attrName);
        attr_types[i] = attrCatEntry.attrType;
    }

    ret = Schema::createRel(targetRel, src_nAttrs, attr_names, attr_types);
    if(ret != SUCCESS)
    {
        return ret;
    }

    int targetRelId = OpenRelTable::openRel(targetRel);

    if(targetRelId < 0 || targetRelId >= MAX_OPEN)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    RelCacheTable::resetSearchIndex(targetRelId);
    Attribute record[src_nAttrs];

    RelCacheTable::resetSearchIndex(srcRelId);
    AttrCacheTable::resetSearchIndex(srcRelId, attr);

    BPlusTree::numTreeComparisons = 0;
    // BlockAccess::numLinearComparisons = 0;
    while(BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS)
    {
        ret = BlockAccess::insert(targetRelId, record);
        if(ret != SUCCESS)
        {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    // printf("Number of Comparisons using Linear Search: %d\n", BlockAccess::numLinearComparisons);
    // printf("Number of Comparisons using BPlus Search: %d\n", BPlusTree::numTreeComparisons);
    Schema::closeRel(targetRel);

    return SUCCESS;
}

bool isNumber(char *str)
{
    int len;
    float ignore;

    int ret = sscanf(str, "%f %n", &ignore, &len);
    return ret == 1 && len == strlen(str);
}

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE])
{
    if(strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
    {
        return E_NOTPERMITTED;
    }

    int relId = OpenRelTable::getRelId(relName);

    if(relId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }

    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    if(relCatEntry.numAttrs != nAttrs)
    {
        return E_NATTRMISMATCH;
    }

    union Attribute recordValues[nAttrs];

    for(int i = 0; i < nAttrs; i++)
    {
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);

        int type = attrCatEntry.attrType;

        if(type == NUMBER)
        {
            if(isNumber(record[i]))
            {
                recordValues[i].nVal = atof(record[i]);
            }
            else
            {
                return E_ATTRTYPEMISMATCH;
            }
        }
        else if(type == STRING)
        {
            strcpy(recordValues[i].sVal, record[i]);
        }

    }
    
    int retVal = BlockAccess::insert(relId, recordValues);

    return retVal;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE])
{
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if(srcRelId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }

    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    int numAttrs = relCatEntry.numAttrs;

    char attrNames[numAttrs][ATTR_SIZE];
    int attrTypes[numAttrs];

    for(int i = 0; i < numAttrs; i++)
    {
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);

        strcpy(attrNames[i], attrCatEntry.attrName);
        attrTypes[i] = attrCatEntry.attrType;
    }

    int ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
    if(ret != SUCCESS)
    {
        return ret;
    }

    int targetRelId = OpenRelTable::openRel(targetRel);
    if(targetRelId < 0 || targetRelId >= MAX_OPEN)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    RelCacheTable::resetSearchIndex(srcRelId);

    Attribute record[numAttrs];

    while(BlockAccess::project(srcRelId, record) == SUCCESS)
    {
        ret = BlockAccess::insert(targetRelId, record);
        if(ret != SUCCESS)
        {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    Schema::closeRel(targetRel);

    return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE])
{
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if(srcRelId < 0 || srcRelId >= MAX_OPEN)
    {
        return srcRelId;
    }

    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    int numAttrs = relCatEntry.numAttrs;

    int attr_offset[tar_nAttrs];
    int attr_types[tar_nAttrs];

    for(int i = 0; i < tar_nAttrs; i++)
    {
        AttrCatEntry attrCatEntry;
        int ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);
        if(ret != SUCCESS)
        {
            return E_ATTRNOTEXIST;
        }

        attr_offset[i] = attrCatEntry.offset;
        attr_types[i] = attrCatEntry.attrType;
    }

    int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attr_types);
    if(ret != SUCCESS)
    {
        return ret;
    }

    int targetRelId = OpenRelTable::openRel(targetRel);
    if(targetRelId < 0 || targetRelId >= MAX_OPEN)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    RelCacheTable::resetSearchIndex(srcRelId);
    Attribute record[numAttrs];

    while(BlockAccess::project(srcRelId, record) == SUCCESS)
    {
        Attribute proj_record[tar_nAttrs];

        for(int i = 0; i < tar_nAttrs; i++)
        {
            proj_record[i] = record[attr_offset[i]];

        }

        ret = BlockAccess::insert(targetRelId, proj_record);
        if(ret != SUCCESS)
        {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    Schema::closeRel(targetRel);
    return SUCCESS;
}

int Algebra::join(char srcRelation1[ATTR_SIZE], char srcRelation2[ATTR_SIZE], char targetRelation[ATTR_SIZE], char attribute1[ATTR_SIZE], char attribute2[ATTR_SIZE]) {
    int srcId1 = OpenRelTable::getRelId(srcRelation1);
    int srcId2 = OpenRelTable::getRelId(srcRelation2);

    if (srcId1 == E_RELNOTOPEN || srcId2 == E_RELNOTOPEN)
        return E_RELNOTOPEN;

    AttrCatEntry attrCatEntry1, attrCatEntry2;
    int ret1 = AttrCacheTable::getAttrCatEntry(srcId1, attribute1, &attrCatEntry1);
    int ret2 = AttrCacheTable::getAttrCatEntry(srcId2, attribute2, &attrCatEntry2);
    
    if (ret1 == E_ATTRNOTEXIST || ret2 == E_ATTRNOTEXIST)
        return E_ATTRNOTEXIST;

    // printf ("E_ATTRNOTEXIST check done\n");

    if (attrCatEntry1.attrType != attrCatEntry2.attrType)
        return E_ATTRTYPEMISMATCH;

    // printf ("E_ATTRTYPEMISMATCH check done\n");

    RelCatEntry relCatEntry1, relCatEntry2;
    RelCacheTable::getRelCatEntry(srcId1, &relCatEntry1);
    RelCacheTable::getRelCatEntry(srcId2, &relCatEntry2);

    AttrCatEntry temp1, temp2;
    int numAttrs1 = relCatEntry1.numAttrs;
    int numAttrs2 = relCatEntry2.numAttrs;

    for (int j = 0; j < numAttrs2; j++)
    {
        if (j == attrCatEntry2.offset) continue;
        AttrCacheTable::getAttrCatEntry(srcId2, j, &temp2);

        for (int i = 0; i < numAttrs1; i++)
        {
            AttrCacheTable::getAttrCatEntry(srcId1, i, &temp1);
            if (strcmp(temp1.attrName, temp2.attrName) == 0)
                return E_DUPLICATEATTR;
        }
    }
    // printf ("E_DUPLICATEATTR check done\n");

    if (attrCatEntry2.rootBlock == -1)
    {
        int ret = BPlusTree::bPlusCreate(srcId2, attrCatEntry2.attrName);
        // printf ("B+ tree created for second relation\n");
        if (ret != SUCCESS)     // Should only be E_DISKFULL
            return ret;
    }

    int numAttrsTarget = numAttrs1 + numAttrs2 - 1;

    // Arrays to store the details of the target relation
    char targetRelAttrNames[numAttrsTarget][ATTR_SIZE];
    int targetRelAttrTypes[numAttrsTarget];
    int i = 0;

    for (i = 0; i < numAttrs1; i++)
    {
        AttrCacheTable::getAttrCatEntry(srcId1, i, &temp1);
        strcpy(targetRelAttrNames[i], temp1.attrName);
        targetRelAttrTypes[i] = temp1.attrType;
    }

    // Copying till attribute2 in srcRelation2
    for (i = 0; i < attrCatEntry2.offset; i++)
    {
        AttrCacheTable::getAttrCatEntry(srcId2, i, &temp2);
        strcpy(targetRelAttrNames[numAttrs1 + i], temp2.attrName);
        targetRelAttrTypes[numAttrs1 + i] = temp2.attrType;
    }

    // Copying after attribute2 in srcRelation2
    for (i = attrCatEntry2.offset+1; i < numAttrs2; i++)
    {
        AttrCacheTable::getAttrCatEntry(srcId2, i, &temp2);
        strcpy(targetRelAttrNames[numAttrs1 + i - 1], temp2.attrName);
        targetRelAttrTypes[numAttrs1 + i - 1] = temp2.attrType;
    }

    // printf ("Copying of details for target relation done\n");

    ret1 = Schema::createRel(targetRelation, numAttrsTarget, targetRelAttrNames, targetRelAttrTypes);
    if (ret1 != SUCCESS)
    {
        printf ("Creating relation failed\n");
        return ret1;
    }

    int targetId = OpenRelTable::openRel(targetRelation);
    if (targetId < 0)
    {
        printf ("Opening relation failed\n");
        Schema::deleteRel(targetRelation);
        return targetId;
    }

    Attribute record1[numAttrs1];
    Attribute record2[numAttrs2];
    Attribute targetRecord[numAttrsTarget];
    RelCacheTable::resetSearchIndex(srcId1);

    // Loop to get every record of srcRelation1 one by one
    while (BlockAccess::project(srcId1, record1) == SUCCESS) 
    {
        RelCacheTable::resetSearchIndex(srcId2);
        AttrCacheTable::resetSearchIndex(srcId2, attribute2);

        // Loop to get every record of srcRelation2 which satisfies record1.attribute1 = record2.attribute2
        while (BlockAccess::search(srcId2, record2, attribute2, record1[attrCatEntry1.offset], EQ) == SUCCESS ) 
        {
            int i = 0;

            // Copying rcRelation1's and srcRelation2's attribute values (except for attribute2 in rel2) to targetRecord
            for (i = 0; i < numAttrs1; i++)
                targetRecord[i] = record1[i];

            // Copying till attribute2 in srcRelation2
            for (i = 0; i < attrCatEntry2.offset; i++)
                targetRecord[numAttrs1 + i] = record2[i];
            
            // Copying after attribute2 in srcRelation2
            for (i = attrCatEntry2.offset+1; i < numAttrs2; i++)
                targetRecord[numAttrs1 + i - 1] = record2[i];

            // printf ("Copying into record done\n");
            ret1 = BlockAccess::insert(targetId, targetRecord);

            if (ret1 == E_DISKFULL) 
            {
                OpenRelTable::closeRel(targetId);
                Schema::deleteRel(targetRelation);
                return E_DISKFULL;
            }
        }
    }

    OpenRelTable::closeRel(targetId);
    return SUCCESS;
}
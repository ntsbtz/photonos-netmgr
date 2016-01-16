/*
 * Copyright © 2016 VMware, Inc.  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the “License”); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS, without
 * warranties or conditions of any kind, EITHER EXPRESS OR IMPLIED.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "includes.h"

static
uint32_t
ini_cfg_parse_section_name(
    const char* pszBuffer,
    char**      ppszName
    );

static
uint32_t
ini_cfg_parse_key_value(
    const char* pszBuffer,
    char**      ppszKey,
    char**      ppszValue
    );

static
void
ini_cfg_free_section(
    PSECTION_INI pSection
    );

static
void
ini_cfg_free_keyvalue(
    PKEYVALUE_INI pKeyValue
    );

uint32_t
ini_cfg_read(
    const char*  pszPath,
    PCONFIG_INI* ppConfig 
    )
{
    uint32_t err = 0;
    FILE* fp = NULL;
    PCONFIG_INI pConfig = NULL;
    PSECTION_INI pSection = NULL;
    char* pszName = NULL;
    char* pszKey = NULL;
    char* pszValue = NULL;

    if (!pszPath || !*pszPath || !ppConfig)
    {
        err = EINVAL;
        goto error;
    }

    fp = fopen(pszPath, "r");
    if (!fp)
    {
        err = errno;
        goto error;
    }

    err = ini_cfg_alloc(sizeof(CONFIG_INI), (void*)&pConfig);
    if (err)
    {
        goto error;
    }

    err = ini_cfg_alloc_string(pszPath, &pConfig->pszPath);
    if (err)
    {
        goto error;
    }

    while(!feof(fp))
    {
        char buffer[1024];
        const char* pszCursor = NULL;

        if (!fgets(buffer, sizeof(buffer), fp))
        {
            err = errno;
            goto error;
        }

        pszCursor = &buffer[0];

        // skip leading whitespace
        while (pszCursor && *pszCursor && isspace((int)*pszCursor))
        {
            pszCursor++;
        }
        // skip empty lines and comments
        if (!pszCursor || !*pszCursor || *pszCursor == '#')
        {
            continue;
        }
        else if (*pszCursor == '[') // section
        {
            if (pszName)
            {
                ini_cfg_free(pszName);
                pszName = NULL;
            }

            err = ini_cfg_parse_section_name(pszCursor, &pszName);
            if (err)
            {
                goto error;
            }

            err = ini_cfg_add_section(pConfig, pszName, &pSection);
            if (err)
            {
                goto error;
            }
        }
        else // key value pair
        {
            if (!pSection)
            {
                err = EBADMSG;
                goto error;
            }

            if (pszKey)
            {
                ini_cfg_free(pszKey);
                pszKey = NULL;
            }
            if (pszValue)
            {
                ini_cfg_free(pszValue);
                pszValue = NULL;
            }

            err = ini_cfg_parse_key_value(pszCursor, &pszKey, &pszValue);
            if (err)
            {
                goto error;
            }

            err = ini_cfg_add_key(pSection, pszKey, pszValue);
            if (err)
            {
                goto error;
            }
        }
    }

    *ppConfig = pConfig;

cleanup:

    if (fp)
    {
        fclose(fp);
    }
    if (pszName)
    {
        ini_cfg_free(pszName);
    }
    if (pszKey)
    {
        ini_cfg_free(pszKey);
    }
    if (pszValue)
    {
        ini_cfg_free(pszValue);
    }

    return err;

error:

    if (ppConfig)
    {
        *ppConfig = NULL;
    }
    if (pConfig)
    {
        ini_cfg_free_config(pConfig);
    }

    goto cleanup;
}

uint32_t
ini_cfg_add_section(
    PCONFIG_INI pConfig,
    const char* pszName,
    PSECTION_INI* ppSection
    )
{
    uint32_t err = 0;
    PSECTION_INI pSection = NULL;
    PSECTION_INI pCursor = NULL;

    if (!pConfig || !pszName || !*pszName || !ppSection)
    {
        err = EINVAL;
        goto error;
    }

    err = ini_cfg_alloc(sizeof(SECTION_INI), (void*)&pSection);
    if (err)
    {
        goto error;
    }

    err = ini_cfg_alloc_string(pszName, &pSection->pszName);
    if (err)
    {
        goto error;
    }

    pCursor = pConfig->pSection;
    while (pCursor && pCursor->pNext != NULL)
    {
        pCursor = pCursor->pNext;
    }

    if (!pCursor)
    {
        pConfig->pSection = pSection;
    }
    else
    {
        pCursor->pNext = pSection;
    }

    *ppSection = pSection;

cleanup:

    return err;

error:

    if (ppSection)
    {
        *ppSection = NULL;
    }
    if (pSection)
    {
        ini_cfg_free_section(pSection);
    }

    goto cleanup;
}

uint32_t
ini_cfg_find_sections(
    PCONFIG_INI    pConfig,
    const char*    pszName,
    PSECTION_INI** pppSections,
    uint32_t*      pdwNumSections
    )
{
    uint32_t err = 0;
    uint32_t nSections = 0;
    PSECTION_INI* ppSections = NULL;
    PSECTION_INI  pCursor = NULL;
    size_t iSection = 0;

    if (!pConfig || !pszName || !*pszName || !pppSections || !pdwNumSections)
    {
        err = EINVAL;
        goto error;
    }

    for (pCursor = pConfig->pSection; pCursor; pCursor = pCursor->pNext)
    {
        if (!strcmp(pCursor->pszName, pszName))
        {
            nSections++;
        }
    }

    if (!nSections)
    {
        goto cleanup;
    }

    err = ini_cfg_alloc(sizeof(PSECTION_INI) * nSections, (void*)&ppSections);
    if (err)
    {
        goto error; 
    }

    for (pCursor = pConfig->pSection; pCursor; pCursor = pCursor->pNext)
    {
        if (!strcmp(pCursor->pszName, pszName))
        {
            ppSections[iSection++] = pCursor; 
        }
    }

    *pppSections = ppSections;
    *pdwNumSections = nSections;

cleanup:

    return err;

error:

    if (pppSections)
    {
        *pppSections = NULL;
    }
    if (pdwNumSections)
    {
        *pdwNumSections = 0;
    }
    if (ppSections)
    {
        ini_cfg_free(ppSections);
    }

    goto cleanup;
}

void
ini_cfg_free_sections(
    PSECTION_INI* ppSections,
    uint32_t      dwNumSections
    )
{
    if (ppSections)
    {
        ini_cfg_free(ppSections);
    }
}

uint32_t
ini_cfg_delete_sections(
    PCONFIG_INI   pConfig,
    const char*   pszName
    )
{
    uint32_t err = 0;
    PSECTION_INI pCursor = NULL;

    if (!pConfig || !pszName || !*pszName)
    {
        err = EINVAL;
        goto error;
    }

    pCursor = pConfig->pSection; 
    while (pCursor)
    {
        PSECTION_INI pCandidate = NULL;

        if (!strcmp(pCursor->pszName, pszName))
        {
            pCandidate = pCursor;

            if (pCursor == pConfig->pSection)
            {
                pConfig->pSection = pCursor->pNext;
            }
        }

        pCursor = pCursor->pNext;

        if (pCandidate)
        {
            pCandidate->pNext = NULL;

            ini_cfg_free_section(pCandidate);
        }
    }

error:

    return err;
}

PKEYVALUE_INI
ini_cfg_find_key(
    PSECTION_INI  pSection,
    const char*   pszKey
    )
{
    PKEYVALUE_INI pKeyValue = NULL;
    PKEYVALUE_INI pCursor = pSection->pKeyValue;

    if (!pszKey || !*pszKey)
    {
        goto cleanup;
    }

    for (; !pKeyValue && pCursor; pCursor = pCursor->pNext)
    {
        if (!strcmp(pCursor->pszKey, pszKey))
        {
            pKeyValue = pCursor;
        }
    }

cleanup:

    return pKeyValue;
}

uint32_t
ini_cfg_add_key(
    PSECTION_INI  pSection,
    const char*   pszKey,
    const char*   pszValue
    )
{
    uint32_t err = 0;
    PKEYVALUE_INI pKeyValue = NULL;
    PKEYVALUE_INI pCursor = NULL;

    if (!pSection || !pszKey || !*pszKey || !pszValue || !*pszValue)
    {
        err = EINVAL;
        goto error;
    }

    if (ini_cfg_find_key(pSection, pszKey))
    {
        err = EEXIST;
        goto error;
    }

    err = ini_cfg_alloc(sizeof(KEYVALUE_INI), (void*)&pKeyValue);
    if (err)
    {
        goto error;
    }
    err = ini_cfg_alloc_string(pszKey, &pKeyValue->pszKey);
    if (err)
    {
        goto error;
    }
    err = ini_cfg_alloc_string(pszValue, &pKeyValue->pszValue);
    if (err)
    {
        goto error;
    }

    pCursor = pSection->pKeyValue;
    while (pCursor && pCursor->pNext != NULL)
    {
        pCursor = pCursor->pNext;
    }

    if (!pCursor)
    {
        pSection->pKeyValue = pKeyValue;
    }
    else
    {
        pCursor->pNext = pKeyValue;
    }

cleanup:

    return err;

error:

    if (pKeyValue)
    {
        ini_cfg_free_keyvalue(pKeyValue);
    }

    goto cleanup;
}

uint32_t
ini_cfg_set_value(
    PSECTION_INI  pSection,
    const char*   pszKey,
    const char*   pszValue
    )
{
    uint32_t err = 0;
    char* pszNewValue = NULL;
    PKEYVALUE_INI pCandidate = NULL;

    if (!pSection || !pszKey || !*pszKey || !pszValue || !*pszValue)
    {
        err = EINVAL;
        goto error;
    }

    pCandidate = ini_cfg_find_key(pSection, pszKey);
    if (!pCandidate)
    {
        err = ENOENT;
        goto error;
    }

    err = ini_cfg_alloc_string(pszValue, &pszNewValue);
    if (err)
    {
        goto error;
    }

    if (pCandidate->pszValue)
    {
        ini_cfg_free(pCandidate->pszValue);
    }

    pCandidate->pszValue = pszNewValue;

error:

    return err;
}

uint32_t
ini_cfg_delete_key(
    PSECTION_INI  pSection,
    const char*   pszKey
    )
{
    uint32_t err = 0;
    PKEYVALUE_INI pCursor = NULL;

    if (!pSection || !pszKey || !*pszKey)
    {
        err = EINVAL;
        goto error;
    }

    pCursor = pSection->pKeyValue;
    while (pCursor)
    {
        PKEYVALUE_INI pCandidate = NULL;

        if (!strcmp(pCursor->pszKey, pszKey))
        {
            pCandidate = pCursor;

            if (pCursor == pSection->pKeyValue)
            {
                pSection->pKeyValue = pCursor->pNext;
            }
        }

        pCursor = pCursor->pNext;

        if (pCandidate)
        {
            pCandidate->pNext = NULL;

            ini_cfg_free_keyvalue(pCandidate);
        }
    }

error:

    return err;
}
     
uint32_t
ini_cfg_save(
    const char*   pszPath,
    PCONFIG_INI   pConfig
    )
{
    uint32_t err = 0;
    char* pszTmpPath = NULL;
    const char* pszSuffix = ".new";
    FILE* fp = NULL;
    PSECTION_INI pSection = NULL;

    if (!pszPath || !*pszPath || !pConfig) 
    {
        err = EINVAL;
        goto error;
    }

    err = ini_cfg_alloc(
            strlen(pszPath)+strlen(pszSuffix)+1,
            (void*)&pszTmpPath);
    if (err)
    {
        goto error;
    }

    sprintf(pszTmpPath, "%s%s", pszPath, pszSuffix);

    fp = fopen(pszTmpPath, "w");
    if (!fp)
    {
        err = errno;
        goto error;
    }

    for (pSection = pConfig->pSection; pSection; pSection = pSection->pNext)
    {
        PKEYVALUE_INI pKeyValue = pSection->pKeyValue;

        fprintf(fp, "\n[%s]\n", pSection->pszName);

        for (; pKeyValue; pKeyValue = pKeyValue->pNext)
        {
            fprintf(fp, "%s=%s\n", pKeyValue->pszKey, pKeyValue->pszValue);
        }
    }

    fclose(fp);
    fp = NULL;

    if (rename(pszTmpPath, pszPath) < 0)
    {
        err = errno;
        goto error;
    }
        
cleanup:

    if (pszTmpPath)
    {
        ini_cfg_free(pszTmpPath);
    }
    if (fp)
    {
        fclose(fp);
    }

    return err;

error:

    goto cleanup;
}

void
ini_cfg_free_config(
    PCONFIG_INI pConfig
    )
{
    if (pConfig)
    {
        if (pConfig->pszPath)
        {
            ini_cfg_free(pConfig->pszPath);
        }
        while (pConfig->pSection)
        {
            PSECTION_INI pSection = pConfig->pSection;

            pConfig->pSection = pSection->pNext;

            ini_cfg_free_section(pSection);
        }
        ini_cfg_free(pConfig);
    }
}

static
uint32_t
ini_cfg_parse_section_name(
    const char* pszBuffer,
    char**      ppszName
    )
{
    uint32_t err = 0;
    const char* pszCursor = pszBuffer;
    const char* pszNameMarker = NULL;
    size_t len = 0;
    char* pszName = NULL;

    // skip leading whitespace
    while (pszCursor && *pszCursor && isspace((int)*pszCursor))
    {
        pszCursor++;
    }
    // check prefix
    if (!pszCursor || !*pszCursor || *pszCursor != '[')
    {
        err = EBADMSG;
        goto error;
    }
    // skip prefix
    pszCursor++;
    // skip whitespace
    while (pszCursor && *pszCursor && isspace((int)*pszCursor))
    {
        pszCursor++;
    }
    pszNameMarker = pszCursor;
    if (!pszNameMarker || !*pszNameMarker)
    {
        err = EBADMSG;
        goto error;
    }
    // allow only (('a'-'z') || ('A'-'Z'))+
    while (pszCursor && *pszCursor && isalpha((int)*pszCursor))
    {
        pszCursor++;
        len++;
    }
    // skip whitespace
    while (pszCursor && *pszCursor && isspace((int)*pszCursor))
    {
        pszCursor++;
    }
    // check suffix
    if (!pszCursor || !*pszCursor || *pszCursor != ']')
    {
        err = EBADMSG;
        goto error;
    }
    // skip suffix
    pszCursor++;
    // skip whitespace
    while (pszCursor && *pszCursor && isspace((int)*pszCursor))
    {
        pszCursor++;
    }
    // Expect end of line and a non-empty name
    if ((pszCursor && *pszCursor) || !len)
    {
        err = EBADMSG;
        goto error;
    }

    err = ini_cfg_alloc_string_len(pszNameMarker, len, &pszName);
    if (err)
    {
        goto error;
    }

    *ppszName = pszName;

cleanup:

    return err;

error:

    if (ppszName)
    {
        *ppszName = NULL;
    }
    if (pszName)
    {
        ini_cfg_free(pszName);
    }

    goto cleanup;
}

static
uint32_t
ini_cfg_parse_key_value(
    const char* pszBuffer,
    char**      ppszKey,
    char**      ppszValue
    )
{
    uint32_t err = 0;
    const char* pszCursor = pszBuffer;
    const char* pszKeyMarker = NULL;
    const char* pszValueMarker = NULL;
    size_t len_key = 0;
    size_t len_value = 0;
    char* pszKey = NULL;
    char* pszValue = NULL;

    // skip leading whitespace
    while (pszCursor && *pszCursor && isspace((int)*pszCursor))
    {
        pszCursor++;
    }
    pszKeyMarker = pszCursor;
    if (!pszKeyMarker || !*pszKeyMarker)
    {
        err = EBADMSG;
        goto error;
    }
    // allow only (('a'-'z') || ('A'-'Z') || ('0'-'9'))+
    while (pszCursor && *pszCursor && (isalpha((int)*pszCursor) || isdigit((int)*pszCursor)))
    {
        pszCursor++;
        len_key++;
    }
    // skip whitespace
    while (pszCursor && *pszCursor && isspace((int)*pszCursor))
    {
        pszCursor++;
    }
    // check operator
    if (!pszCursor || !*pszCursor || *pszCursor != '=')
    {
        err = EBADMSG;
        goto error;
    }
    // skip operator
    pszCursor++;
    // skip whitespace
    while (pszCursor && *pszCursor && isspace((int)*pszCursor))
    {
        pszCursor++;
    }
    pszValueMarker = pszCursor;
    if (!pszValueMarker || !*pszValueMarker)
    {
        err = EBADMSG;
        goto error;
    }
    while (pszCursor && *pszCursor && !isspace((int)*pszCursor))
    {
        pszCursor++;
        len_value++;
    }
    // skip whitespace
    while (pszCursor && *pszCursor && isspace((int)*pszCursor))
    {
        pszCursor++;
    }
    if ((pszCursor && *pszCursor) || !len_key || !len_value)
    {
        err = EBADMSG;
        goto error;
    }

    err = ini_cfg_alloc_string_len(pszKeyMarker, len_key, &pszKey);
    if (err)
    {
        goto error;
    }
    err = ini_cfg_alloc_string_len(pszValueMarker, len_value, &pszValue);
    if (err)
    {
        goto error;
    }

    *ppszKey = pszKey;
    *ppszValue = pszValue;

cleanup:

    return err;

error:

    if (ppszKey)
    {
        *ppszKey = NULL;
    }
    if (ppszValue)
    {
        *ppszValue = NULL;
    }
    if (pszKey)
    {
        ini_cfg_free(pszKey);
    }
    if (pszValue)
    {
        ini_cfg_free(pszValue);
    }

    goto cleanup;
}

static
void
ini_cfg_free_section(
    PSECTION_INI pSection
    )
{
    if (pSection)
    {
        if (pSection->pszName)
        {
            ini_cfg_free(pSection->pszName);
        }
        while (pSection->pKeyValue)
        {
            PKEYVALUE_INI pKeyValue = pSection->pKeyValue;

            pSection->pKeyValue = pKeyValue->pNext;

            ini_cfg_free_keyvalue(pKeyValue);
        }
        ini_cfg_free(pSection);
    }
}

static
void
ini_cfg_free_keyvalue(
    PKEYVALUE_INI pKeyValue
    )
{
    if (pKeyValue)
    {
        if (pKeyValue->pszKey)
        {
            ini_cfg_free(pKeyValue->pszKey);
        }
        if (pKeyValue->pszValue)
        {
            ini_cfg_free(pKeyValue->pszValue);
        }
        ini_cfg_free(pKeyValue);
    }
}


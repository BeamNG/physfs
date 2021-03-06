#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"


/*
 * From rfc3629, the UTF-8 spec:
 *  https://www.ietf.org/rfc/rfc3629.txt
 *
 *   Char. number range  |        UTF-8 octet sequence
 *      (hexadecimal)    |              (binary)
 *   --------------------+---------------------------------------------
 *   0000 0000-0000 007F | 0xxxxxxx
 *   0000 0080-0000 07FF | 110xxxxx 10xxxxxx
 *   0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
 *   0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 */


/*
 * This may not be the best value, but it's one that isn't represented
 *  in Unicode (0x10FFFF is the largest codepoint value). We return this
 *  value from utf8codepoint() if there's bogus bits in the
 *  stream. utf8codepoint() will turn this value into something
 *  reasonable (like a question mark), for text that wants to try to recover,
 *  whereas utf8valid() will use the value to determine if a string has bad
 *  bits.
 */
#define UNICODE_BOGUS_CHAR_VALUE 0xFFFFFFFF

/*
 * This is the codepoint we currently return when there was bogus bits in a
 *  UTF-8 string. May not fly in Asian locales?
 */
#define UNICODE_BOGUS_CHAR_CODEPOINT '?'

static PHYSFS_uint32 utf8codepoint(const char **_str)
{
    const char *str = *_str;
    PHYSFS_uint32 retval = 0;
    PHYSFS_uint32 octet = (PHYSFS_uint32) ((PHYSFS_uint8) *str);
    PHYSFS_uint32 octet2, octet3, octet4;

    if (octet == 0)  /* null terminator, end of string. */
        return 0;

    else if (octet < 128)  /* one octet char: 0 to 127 */
    {
        (*_str)++;  /* skip to next possible start of codepoint. */
        return octet;
    } /* else if */

    else if ((octet > 127) && (octet < 192))  /* bad (starts with 10xxxxxx). */
    {
        /*
         * Apparently each of these is supposed to be flagged as a bogus
         *  char, instead of just resyncing to the next valid codepoint.
         */
        (*_str)++;  /* skip to next possible start of codepoint. */
        return UNICODE_BOGUS_CHAR_VALUE;
    } /* else if */

    else if (octet < 224)  /* two octets */
    {
        (*_str)++;  /* advance at least one byte in case of an error */
        octet -= (128+64);
        octet2 = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet2 & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        *_str += 1;  /* skip to next possible start of codepoint. */
        retval = ((octet << 6) | (octet2 - 128));
        if ((retval >= 0x80) && (retval <= 0x7FF))
            return retval;
    } /* else if */

    else if (octet < 240)  /* three octets */
    {
        (*_str)++;  /* advance at least one byte in case of an error */
        octet -= (128+64+32);
        octet2 = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet2 & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        octet3 = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet3 & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        *_str += 2;  /* skip to next possible start of codepoint. */
        retval = ( ((octet << 12)) | ((octet2-128) << 6) | ((octet3-128)) );

        /* There are seven "UTF-16 surrogates" that are illegal in UTF-8. */
        switch (retval)
        {
            case 0xD800:
            case 0xDB7F:
            case 0xDB80:
            case 0xDBFF:
            case 0xDC00:
            case 0xDF80:
            case 0xDFFF:
                return UNICODE_BOGUS_CHAR_VALUE;
        } /* switch */

        /* 0xFFFE and 0xFFFF are illegal, too, so we check them at the edge. */
        if ((retval >= 0x800) && (retval <= 0xFFFD))
            return retval;
    } /* else if */

    else if (octet < 248)  /* four octets */
    {
        (*_str)++;  /* advance at least one byte in case of an error */
        octet -= (128+64+32+16);
        octet2 = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet2 & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        octet3 = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet3 & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        octet4 = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet4 & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        *_str += 3;  /* skip to next possible start of codepoint. */
        retval = ( ((octet << 18)) | ((octet2 - 128) << 12) |
                   ((octet3 - 128) << 6) | ((octet4 - 128)) );
        if ((retval >= 0x10000) && (retval <= 0x10FFFF))
            return retval;
    } /* else if */

    /*
     * Five and six octet sequences became illegal in rfc3629.
     *  We throw the codepoint away, but parse them to make sure we move
     *  ahead the right number of bytes and don't overflow the buffer.
     */

    else if (octet < 252)  /* five octets */
    {
        (*_str)++;  /* advance at least one byte in case of an error */
        octet = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        octet = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        octet = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        octet = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        *_str += 4;  /* skip to next possible start of codepoint. */
        return UNICODE_BOGUS_CHAR_VALUE;
    } /* else if */

    else  /* six octets */
    {
        (*_str)++;  /* advance at least one byte in case of an error */
        octet = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        octet = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        octet = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        octet = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        octet = (PHYSFS_uint32) ((PHYSFS_uint8) *(++str));
        if ((octet & (128+64)) != 128)  /* Format isn't 10xxxxxx? */
            return UNICODE_BOGUS_CHAR_VALUE;

        *_str += 6;  /* skip to next possible start of codepoint. */
        return UNICODE_BOGUS_CHAR_VALUE;
    } /* else if */

    return UNICODE_BOGUS_CHAR_VALUE;
} /* utf8codepoint */


void PHYSFS_utf8ToUcs4(const char *src, PHYSFS_uint32 *dst, PHYSFS_uint64 len)
{
    len -= sizeof (PHYSFS_uint32);   /* save room for null char. */
    while (len >= sizeof (PHYSFS_uint32))
    {
        PHYSFS_uint32 cp = utf8codepoint(&src);
        if (cp == 0)
            break;
        else if (cp == UNICODE_BOGUS_CHAR_VALUE)
            cp = UNICODE_BOGUS_CHAR_CODEPOINT;
        *(dst++) = cp;
        len -= sizeof (PHYSFS_uint32);
    } /* while */

    *dst = 0;
} /* PHYSFS_utf8ToUcs4 */


void PHYSFS_utf8ToUcs2(const char *src, PHYSFS_uint16 *dst, PHYSFS_uint64 len)
{
    len -= sizeof (PHYSFS_uint16);   /* save room for null char. */
    while (len >= sizeof (PHYSFS_uint16))
    {
        PHYSFS_uint32 cp = utf8codepoint(&src);
        if (cp == 0)
            break;
        else if (cp == UNICODE_BOGUS_CHAR_VALUE)
            cp = UNICODE_BOGUS_CHAR_CODEPOINT;

        if (cp > 0xFFFF)  /* UTF-16 surrogates (bogus chars in UCS-2) */
            cp = UNICODE_BOGUS_CHAR_CODEPOINT;

        *(dst++) = cp;
        len -= sizeof (PHYSFS_uint16);
    } /* while */

    *dst = 0;
} /* PHYSFS_utf8ToUcs2 */


void PHYSFS_utf8ToUtf16(const char *src, PHYSFS_uint16 *dst, PHYSFS_uint64 len)
{
    len -= sizeof (PHYSFS_uint16);   /* save room for null char. */
    while (len >= sizeof (PHYSFS_uint16))
    {
        PHYSFS_uint32 cp = utf8codepoint(&src);
        if (cp == 0)
            break;
        else if (cp == UNICODE_BOGUS_CHAR_VALUE)
            cp = UNICODE_BOGUS_CHAR_CODEPOINT;

        if (cp > 0xFFFF)  /* encode as surrogate pair */
        {
            if (len < (sizeof (PHYSFS_uint16) * 2))
                break;  /* not enough room for the pair, stop now. */

            cp -= 0x10000;  /* Make this a 20-bit value */

            *(dst++) = 0xD800 + ((cp >> 10) & 0x3FF);
            len -= sizeof (PHYSFS_uint16);

            cp = 0xDC00 + (cp & 0x3FF);
        } /* if */

        *(dst++) = cp;
        len -= sizeof (PHYSFS_uint16);
    } /* while */

    *dst = 0;
} /* PHYSFS_utf8ToUtf16 */

static void utf8fromcodepoint(PHYSFS_uint32 cp, char **_dst, PHYSFS_uint64 *_len)
{
    char *dst = *_dst;
    PHYSFS_uint64 len = *_len;

    if (len == 0)
        return;

    if (cp > 0x10FFFF)
        cp = UNICODE_BOGUS_CHAR_CODEPOINT;
    else if ((cp == 0xFFFE) || (cp == 0xFFFF))  /* illegal values. */
        cp = UNICODE_BOGUS_CHAR_CODEPOINT;
    else
    {
        /* There are seven "UTF-16 surrogates" that are illegal in UTF-8. */
        switch (cp)
        {
            case 0xD800:
            case 0xDB7F:
            case 0xDB80:
            case 0xDBFF:
            case 0xDC00:
            case 0xDF80:
            case 0xDFFF:
                cp = UNICODE_BOGUS_CHAR_CODEPOINT;
        } /* switch */
    } /* else */

    /* Do the encoding... */
    if (cp < 0x80)
    {
        *(dst++) = (char) cp;
        len--;
    } /* if */

    else if (cp < 0x800)
    {
        if (len < 2)
            len = 0;
        else
        {
            *(dst++) = (char) ((cp >> 6) | 128 | 64);
            *(dst++) = (char) (cp & 0x3F) | 128;
            len -= 2;
        } /* else */
    } /* else if */

    else if (cp < 0x10000)
    {
        if (len < 3)
            len = 0;
        else
        {
            *(dst++) = (char) ((cp >> 12) | 128 | 64 | 32);
            *(dst++) = (char) ((cp >> 6) & 0x3F) | 128;
            *(dst++) = (char) (cp & 0x3F) | 128;
            len -= 3;
        } /* else */
    } /* else if */

    else
    {
        if (len < 4)
            len = 0;
        else
        {
            *(dst++) = (char) ((cp >> 18) | 128 | 64 | 32 | 16);
            *(dst++) = (char) ((cp >> 12) & 0x3F) | 128;
            *(dst++) = (char) ((cp >> 6) & 0x3F) | 128;
            *(dst++) = (char) (cp & 0x3F) | 128;
            len -= 4;
        } /* else if */
    } /* else */

    *_dst = dst;
    *_len = len;
} /* utf8fromcodepoint */

#define UTF8FROMTYPE(typ, src, dst, len) \
    if (len == 0) return; \
    len--;  \
    while (len) \
    { \
        const PHYSFS_uint32 cp = (PHYSFS_uint32) ((typ) (*(src++))); \
        if (cp == 0) break; \
        utf8fromcodepoint(cp, &dst, &len); \
    } \
    *dst = '\0'; \

void PHYSFS_utf8FromUcs4(const PHYSFS_uint32 *src, char *dst, PHYSFS_uint64 len)
{
    UTF8FROMTYPE(PHYSFS_uint32, src, dst, len);
} /* PHYSFS_utf8FromUcs4 */

void PHYSFS_utf8FromUcs2(const PHYSFS_uint16 *src, char *dst, PHYSFS_uint64 len)
{
    UTF8FROMTYPE(PHYSFS_uint64, src, dst, len);
} /* PHYSFS_utf8FromUcs2 */

/* latin1 maps to unicode codepoints directly, we just utf-8 encode it. */
void PHYSFS_utf8FromLatin1(const char *src, char *dst, PHYSFS_uint64 len)
{
    UTF8FROMTYPE(PHYSFS_uint8, src, dst, len);
} /* PHYSFS_utf8FromLatin1 */

#undef UTF8FROMTYPE


void PHYSFS_utf8FromUtf16(const PHYSFS_uint16 *src, char *dst, PHYSFS_uint64 len)
{
    if (len == 0)
        return;

    len--;
    while (len)
    {
        PHYSFS_uint32 cp = (PHYSFS_uint32) *(src++);
        if (cp == 0)
            break;

        /* Orphaned second half of surrogate pair? */
        if ((cp >= 0xDC00) && (cp <= 0xDFFF))
            cp = UNICODE_BOGUS_CHAR_CODEPOINT;
        else if ((cp >= 0xD800) && (cp <= 0xDBFF))  /* start surrogate pair! */
        {
            const PHYSFS_uint32 pair = (PHYSFS_uint32) *src;
            if ((pair < 0xDC00) || (pair > 0xDFFF))
                cp = UNICODE_BOGUS_CHAR_CODEPOINT;
            else
            {
                src++;  /* eat the other surrogate. */
                cp = (((cp - 0xD800) << 10) | (pair - 0xDC00));
            } /* else */
        } /* else if */

        utf8fromcodepoint(cp, &dst, &len);
    } /* while */

    *dst = '\0';
} /* PHYSFS_utf8FromUtf16 */


typedef struct CaseFoldMapping
{
    PHYSFS_uint32 from;
    PHYSFS_uint32 to0;
    PHYSFS_uint32 to1;
    PHYSFS_uint32 to2;
} CaseFoldMapping;

typedef struct CaseFoldHashBucket
{
    const PHYSFS_uint8 count;
    const CaseFoldMapping *list;
} CaseFoldHashBucket;

#include "physfs_casefolding.h"

static void locate_case_fold_mapping(const PHYSFS_uint32 from,
                                     PHYSFS_uint32 *to)
{
    PHYSFS_uint32 i;
    const PHYSFS_uint8 hashed = ((from ^ (from >> 8)) & 0xFF);
    const CaseFoldHashBucket *bucket = &case_fold_hash[hashed];
    const CaseFoldMapping *mapping = bucket->list;

    for (i = 0; i < bucket->count; i++, mapping++)
    {
        if (mapping->from == from)
        {
            to[0] = mapping->to0;
            to[1] = mapping->to1;
            to[2] = mapping->to2;
            return;
        } /* if */
    } /* for */

    /* Not found...there's no remapping for this codepoint. */
    to[0] = from;
    to[1] = 0;
    to[2] = 0;
} /* locate_case_fold_mapping */


static int utf8codepointcmp(const PHYSFS_uint32 cp1, const PHYSFS_uint32 cp2)
{
    PHYSFS_uint32 folded1[3], folded2[3];

    if (cp1 == cp2)
        return 0;  /* obviously matches. */

    locate_case_fold_mapping(cp1, folded1);
    locate_case_fold_mapping(cp2, folded2);

    if (folded1[0] < folded2[0])
        return -1;
    else if (folded1[0] > folded2[0])
        return 1;
    else if (folded1[1] < folded2[1])
        return -1;
    else if (folded1[1] > folded2[1])
        return 1;
    else if (folded1[2] < folded2[2])
        return -1;
    else if (folded1[2] > folded2[2])
        return 1;

    return 0;  /* complete match. */
} /* utf8codepointcmp */


int __PHYSFS_utf8stricmp(const char *str1, const char *str2)
{
    while (1)
    {
        const PHYSFS_uint32 cp1 = utf8codepoint(&str1);
        const PHYSFS_uint32 cp2 = utf8codepoint(&str2);
        const int rc = utf8codepointcmp(cp1, cp2);
        if (rc != 0)
            return rc;
        else if (cp1 == 0)
            break;  /* complete match. */
    } /* while */

    return 0;
} /* __PHYSFS_utf8stricmp */


int __PHYSFS_utf8strnicmp(const char *str1, const char *str2, PHYSFS_uint32 n)
{
    while (n > 0)
    {
        const PHYSFS_uint32 cp1 = utf8codepoint(&str1);
        const PHYSFS_uint32 cp2 = utf8codepoint(&str2);
        const int rc = utf8codepointcmp(cp1, cp2);
        if (rc != 0)
            return rc;
        else if (cp1 == 0)
            return 0;
        n--;
    } /* while */

    return 0;  /* matched to n chars. */
} /* __PHYSFS_utf8strnicmp */


int __PHYSFS_stricmpASCII(const char *str1, const char *str2)
{
    while (1)
    {
        const char ch1 = *(str1++);
        const char ch2 = *(str2++);
        const char cp1 = ((ch1 >= 'A') && (ch1 <= 'Z')) ? (ch1+32) : ch1;
        const char cp2 = ((ch2 >= 'A') && (ch2 <= 'Z')) ? (ch2+32) : ch2;
        if (cp1 < cp2)
            return -1;
        else if (cp1 > cp2)
            return 1;
        else if (cp1 == 0)  /* they're both null chars? */
            break;
    } /* while */

    return 0;
} /* __PHYSFS_stricmpASCII */


int __PHYSFS_strnicmpASCII(const char *str1, const char *str2, PHYSFS_uint32 n)
{
    while (n-- > 0)
    {
        const char ch1 = *(str1++);
        const char ch2 = *(str2++);
        const char cp1 = ((ch1 >= 'A') && (ch1 <= 'Z')) ? (ch1+32) : ch1;
        const char cp2 = ((ch2 >= 'A') && (ch2 <= 'Z')) ? (ch2+32) : ch2;
        if (cp1 < cp2)
            return -1;
        else if (cp1 > cp2)
            return 1;
        else if (cp1 == 0)  /* they're both null chars? */
            return 0;
    } /* while */

    return 0;
} /* __PHYSFS_strnicmpASCII */


/* end of physfs_unicode.c ... */


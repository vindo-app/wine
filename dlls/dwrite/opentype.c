/*
 *    Methods for dealing with opentype font tables
 *
 * Copyright 2014 Aric Stewart for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include "dwrite_private.h"
#include "winternl.h"

WINE_DEFAULT_DEBUG_CHANNEL(dwrite);

#define MS_HEAD_TAG DWRITE_MAKE_OPENTYPE_TAG('h','e','a','d')
#define MS_HHEA_TAG DWRITE_MAKE_OPENTYPE_TAG('h','h','e','a')
#define MS_OTTO_TAG DWRITE_MAKE_OPENTYPE_TAG('O','T','T','O')
#define MS_OS2_TAG  DWRITE_MAKE_OPENTYPE_TAG('O','S','/','2')
#define MS_POST_TAG DWRITE_MAKE_OPENTYPE_TAG('p','o','s','t')
#define MS_TTCF_TAG DWRITE_MAKE_OPENTYPE_TAG('t','t','c','f')
#define MS_GPOS_TAG DWRITE_MAKE_OPENTYPE_TAG('G','P','O','S')
#define MS_GSUB_TAG DWRITE_MAKE_OPENTYPE_TAG('G','S','U','B')
#define MS_NAME_TAG DWRITE_MAKE_OPENTYPE_TAG('n','a','m','e')

#ifdef WORDS_BIGENDIAN
#define GET_BE_WORD(x) (x)
#define GET_BE_DWORD(x) (x)
#else
#define GET_BE_WORD(x)  RtlUshortByteSwap(x)
#define GET_BE_DWORD(x) RtlUlongByteSwap(x)
#endif

typedef struct {
    CHAR TTCTag[4];
    DWORD Version;
    DWORD numFonts;
    DWORD OffsetTable[1];
} TTC_Header_V1;

typedef struct {
    DWORD version;
    WORD numTables;
    WORD searchRange;
    WORD entrySelector;
    WORD rangeShift;
} TTC_SFNT_V1;

typedef struct {
    CHAR tag[4];
    DWORD checkSum;
    DWORD offset;
    DWORD length;
} TT_TableRecord;

typedef struct {
    WORD platformID;
    WORD encodingID;
    DWORD offset;
} CMAP_EncodingRecord;

typedef struct {
    WORD version;
    WORD numTables;
    CMAP_EncodingRecord tables[1];
} CMAP_Header;

typedef struct {
    DWORD startCharCode;
    DWORD endCharCode;
    DWORD startGlyphID;
} CMAP_SegmentedCoverage_group;

typedef struct {
    WORD format;
    WORD reserved;
    DWORD length;
    DWORD language;
    DWORD nGroups;
    CMAP_SegmentedCoverage_group groups[1];
} CMAP_SegmentedCoverage;

typedef struct {
    WORD format;
    WORD length;
    WORD language;
    WORD segCountX2;
    WORD searchRange;
    WORD entrySelector;
    WORD rangeShift;
    WORD endCode[1];
} CMAP_SegmentMapping_0;

enum OPENTYPE_CMAP_TABLE_FORMAT
{
    OPENTYPE_CMAP_TABLE_SEGMENT_MAPPING = 4,
    OPENTYPE_CMAP_TABLE_SEGMENTED_COVERAGE = 12
};

/* PANOSE is 10 bytes in size, need to pack the structure properly */
#include "pshpack2.h"
typedef struct
{
    ULONG version;
    ULONG revision;
    ULONG checksumadj;
    ULONG magic;
    USHORT flags;
    USHORT unitsPerEm;
    ULONGLONG created;
    ULONGLONG modified;
    SHORT xMin;
    SHORT yMin;
    SHORT xMax;
    SHORT yMax;
    USHORT macStyle;
    USHORT lowestRecPPEM;
    SHORT direction_hint;
    SHORT index_format;
    SHORT glyphdata_format;
} TT_HEAD;

enum TT_HEAD_MACSTYLE
{
    TT_HEAD_MACSTYLE_BOLD      = 1 << 0,
    TT_HEAD_MACSTYLE_ITALIC    = 1 << 1,
    TT_HEAD_MACSTYLE_UNDERLINE = 1 << 2,
    TT_HEAD_MACSTYLE_OUTLINE   = 1 << 3,
    TT_HEAD_MACSTYLE_SHADOW    = 1 << 4,
    TT_HEAD_MACSTYLE_CONDENSED = 1 << 5,
    TT_HEAD_MACSTYLE_EXTENDED  = 1 << 6,
};

typedef struct
{
    ULONG Version;
    ULONG italicAngle;
    SHORT underlinePosition;
    SHORT underlineThickness;
    ULONG fixed_pitch;
    ULONG minmemType42;
    ULONG maxmemType42;
    ULONG minmemType1;
    ULONG maxmemType1;
} TT_POST;

typedef struct
{
    USHORT version;
    SHORT xAvgCharWidth;
    USHORT usWeightClass;
    USHORT usWidthClass;
    SHORT fsType;
    SHORT ySubscriptXSize;
    SHORT ySubscriptYSize;
    SHORT ySubscriptXOffset;
    SHORT ySubscriptYOffset;
    SHORT ySuperscriptXSize;
    SHORT ySuperscriptYSize;
    SHORT ySuperscriptXOffset;
    SHORT ySuperscriptYOffset;
    SHORT yStrikeoutSize;
    SHORT yStrikeoutPosition;
    SHORT sFamilyClass;
    PANOSE panose;
    ULONG ulUnicodeRange1;
    ULONG ulUnicodeRange2;
    ULONG ulUnicodeRange3;
    ULONG ulUnicodeRange4;
    CHAR achVendID[4];
    USHORT fsSelection;
    USHORT usFirstCharIndex;
    USHORT usLastCharIndex;
    /* According to the Apple spec, original version didn't have the below fields,
     * version numbers were taken from the OpenType spec.
     */
    /* version 0 (TrueType 1.5) */
    USHORT sTypoAscender;
    USHORT sTypoDescender;
    USHORT sTypoLineGap;
    USHORT usWinAscent;
    USHORT usWinDescent;
    /* version 1 (TrueType 1.66) */
    ULONG ulCodePageRange1;
    ULONG ulCodePageRange2;
    /* version 2 (OpenType 1.2) */
    SHORT sxHeight;
    SHORT sCapHeight;
    USHORT usDefaultChar;
    USHORT usBreakChar;
    USHORT usMaxContext;
} TT_OS2_V2;

typedef struct {
    ULONG  version;
    SHORT  ascender;
    SHORT  descender;
    SHORT  linegap;
    USHORT advanceWidthMax;
    SHORT  minLeftSideBearing;
    SHORT  minRightSideBearing;
    SHORT  xMaxExtent;
    SHORT  caretSlopeRise;
    SHORT  caretSlopeRun;
    SHORT  caretOffset;
    SHORT  reserved[4];
    SHORT  metricDataFormat;
    USHORT numberOfHMetrics;
} TT_HHEA;

#include "poppack.h"

enum OS2_FSSELECTION {
    OS2_FSSELECTION_ITALIC           = 1 << 0,
    OS2_FSSELECTION_UNDERSCORE       = 1 << 1,
    OS2_FSSELECTION_NEGATIVE         = 1 << 2,
    OS2_FSSELECTION_OUTLINED         = 1 << 3,
    OS2_FSSELECTION_STRIKEOUT        = 1 << 4,
    OS2_FSSELECTION_BOLD             = 1 << 5,
    OS2_FSSELECTION_REGULAR          = 1 << 6,
    OS2_FSSELECTION_USE_TYPO_METRICS = 1 << 7,
    OS2_FSSELECTION_WWS              = 1 << 8,
    OS2_FSSELECTION_OBLIQUE          = 1 << 9
};

typedef struct {
    WORD platformID;
    WORD encodingID;
    WORD languageID;
    WORD nameID;
    WORD length;
    WORD offset;
} TT_NameRecord;

typedef struct {
    WORD format;
    WORD count;
    WORD stringOffset;
    TT_NameRecord nameRecord[1];
} TT_NAME_V0;

struct VDMX_Header
{
    WORD version;
    WORD numRecs;
    WORD numRatios;
};

struct VDMX_Ratio
{
    BYTE bCharSet;
    BYTE xRatio;
    BYTE yStartRatio;
    BYTE yEndRatio;
};

struct VDMX_group
{
    WORD recs;
    BYTE startsz;
    BYTE endsz;
};

struct VDMX_vTable
{
    WORD yPelHeight;
    SHORT yMax;
    SHORT yMin;
};

typedef struct {
    CHAR FeatureTag[4];
    WORD Feature;
} OT_FeatureRecord;

typedef struct {
    WORD FeatureCount;
    OT_FeatureRecord FeatureRecord[1];
} OT_FeatureList;

typedef struct {
    WORD LookupOrder; /* Reserved */
    WORD ReqFeatureIndex;
    WORD FeatureCount;
    WORD FeatureIndex[1];
} OT_LangSys;

typedef struct {
    CHAR LangSysTag[4];
    WORD LangSys;
} OT_LangSysRecord;

typedef struct {
    WORD DefaultLangSys;
    WORD LangSysCount;
    OT_LangSysRecord LangSysRecord[1];
} OT_Script;

typedef struct {
    CHAR ScriptTag[4];
    WORD Script;
} OT_ScriptRecord;

typedef struct {
    WORD ScriptCount;
    OT_ScriptRecord ScriptRecord[1];
} OT_ScriptList;

typedef struct {
    DWORD version;
    WORD ScriptList;
    WORD FeatureList;
    WORD LookupList;
} GPOS_GSUB_Header;

enum OPENTYPE_PLATFORM_ID
{
    OPENTYPE_PLATFORM_UNICODE = 0,
    OPENTYPE_PLATFORM_MAC,
    OPENTYPE_PLATFORM_ISO,
    OPENTYPE_PLATFORM_WIN,
    OPENTYPE_PLATFORM_CUSTOM
};

enum TT_NAME_WINDOWS_ENCODING_ID
{
    TT_NAME_WINDOWS_ENCODING_SYMBOL = 0,
    TT_NAME_WINDOWS_ENCODING_UCS2,
    TT_NAME_WINDOWS_ENCODING_SJIS,
    TT_NAME_WINDOWS_ENCODING_PRC,
    TT_NAME_WINDOWS_ENCODING_BIG5,
    TT_NAME_WINDOWS_ENCODING_WANSUNG,
    TT_NAME_WINDOWS_ENCODING_JOHAB,
    TT_NAME_WINDOWS_ENCODING_RESERVED1,
    TT_NAME_WINDOWS_ENCODING_RESERVED2,
    TT_NAME_WINDOWS_ENCODING_RESERVED3,
    TT_NAME_WINDOWS_ENCODING_UCS4
};

enum TT_NAME_MAC_ENCODING_ID
{
    TT_NAME_MAC_ENCODING_ROMAN = 0,
    TT_NAME_MAC_ENCODING_JAPANESE,
    TT_NAME_MAC_ENCODING_TRAD_CHINESE,
    TT_NAME_MAC_ENCODING_KOREAN,
    TT_NAME_MAC_ENCODING_ARABIC,
    TT_NAME_MAC_ENCODING_HEBREW,
    TT_NAME_MAC_ENCODING_GREEK,
    TT_NAME_MAC_ENCODING_RUSSIAN,
    TT_NAME_MAC_ENCODING_RSYMBOL,
    TT_NAME_MAC_ENCODING_DEVANAGARI,
    TT_NAME_MAC_ENCODING_GURMUKHI,
    TT_NAME_MAC_ENCODING_GUJARATI,
    TT_NAME_MAC_ENCODING_ORIYA,
    TT_NAME_MAC_ENCODING_BENGALI,
    TT_NAME_MAC_ENCODING_TAMIL,
    TT_NAME_MAC_ENCODING_TELUGU,
    TT_NAME_MAC_ENCODING_KANNADA,
    TT_NAME_MAC_ENCODING_MALAYALAM,
    TT_NAME_MAC_ENCODING_SINHALESE,
    TT_NAME_MAC_ENCODING_BURMESE,
    TT_NAME_MAC_ENCODING_KHMER,
    TT_NAME_MAC_ENCODING_THAI,
    TT_NAME_MAC_ENCODING_LAOTIAN,
    TT_NAME_MAC_ENCODING_GEORGIAN,
    TT_NAME_MAC_ENCODING_ARMENIAN,
    TT_NAME_MAC_ENCODING_SIMPL_CHINESE,
    TT_NAME_MAC_ENCODING_TIBETAN,
    TT_NAME_MAC_ENCODING_MONGOLIAN,
    TT_NAME_MAC_ENCODING_GEEZ,
    TT_NAME_MAC_ENCODING_SLAVIC,
    TT_NAME_MAC_ENCODING_VIETNAMESE,
    TT_NAME_MAC_ENCODING_SINDHI,
    TT_NAME_MAC_ENCODING_UNINTERPRETED
};

enum TT_NAME_MAC_LANGUAGE_ID
{
    TT_NAME_MAC_LANGID_ENGLISH = 0,
    TT_NAME_MAC_LANGID_FRENCH,
    TT_NAME_MAC_LANGID_GERMAN,
    TT_NAME_MAC_LANGID_ITALIAN,
    TT_NAME_MAC_LANGID_DUTCH,
    TT_NAME_MAC_LANGID_SWEDISH,
    TT_NAME_MAC_LANGID_SPANISH,
    TT_NAME_MAC_LANGID_DANISH,
    TT_NAME_MAC_LANGID_PORTUGUESE,
    TT_NAME_MAC_LANGID_NORWEGIAN,
    TT_NAME_MAC_LANGID_HEBREW,
    TT_NAME_MAC_LANGID_JAPANESE,
    TT_NAME_MAC_LANGID_ARABIC,
    TT_NAME_MAC_LANGID_FINNISH,
    TT_NAME_MAC_LANGID_GREEK,
    TT_NAME_MAC_LANGID_ICELANDIC,
    TT_NAME_MAC_LANGID_MALTESE,
    TT_NAME_MAC_LANGID_TURKISH,
    TT_NAME_MAC_LANGID_CROATIAN,
    TT_NAME_MAC_LANGID_TRAD_CHINESE,
    TT_NAME_MAC_LANGID_URDU,
    TT_NAME_MAC_LANGID_HINDI,
    TT_NAME_MAC_LANGID_THAI,
    TT_NAME_MAC_LANGID_KOREAN,
    TT_NAME_MAC_LANGID_LITHUANIAN,
    TT_NAME_MAC_LANGID_POLISH,
    TT_NAME_MAC_LANGID_HUNGARIAN,
    TT_NAME_MAC_LANGID_ESTONIAN,
    TT_NAME_MAC_LANGID_LATVIAN,
    TT_NAME_MAC_LANGID_SAMI,
    TT_NAME_MAC_LANGID_FAROESE,
    TT_NAME_MAC_LANGID_FARSI,
    TT_NAME_MAC_LANGID_RUSSIAN,
    TT_NAME_MAC_LANGID_SIMPL_CHINESE,
    TT_NAME_MAC_LANGID_FLEMISH,
    TT_NAME_MAC_LANGID_GAELIC,
    TT_NAME_MAC_LANGID_ALBANIAN,
    TT_NAME_MAC_LANGID_ROMANIAN,
    TT_NAME_MAC_LANGID_CZECH,
    TT_NAME_MAC_LANGID_SLOVAK,
    TT_NAME_MAC_LANGID_SLOVENIAN,
    TT_NAME_MAC_LANGID_YIDDISH,
    TT_NAME_MAC_LANGID_SERBIAN,
    TT_NAME_MAC_LANGID_MACEDONIAN,
    TT_NAME_MAC_LANGID_BULGARIAN,
    TT_NAME_MAC_LANGID_UKRAINIAN,
    TT_NAME_MAC_LANGID_BYELORUSSIAN,
    TT_NAME_MAC_LANGID_UZBEK,
    TT_NAME_MAC_LANGID_KAZAKH,
    TT_NAME_MAC_LANGID_AZERB_CYR,
    TT_NAME_MAC_LANGID_AZERB_ARABIC,
    TT_NAME_MAC_LANGID_ARMENIAN,
    TT_NAME_MAC_LANGID_GEORGIAN,
    TT_NAME_MAC_LANGID_MOLDAVIAN,
    TT_NAME_MAC_LANGID_KIRGHIZ,
    TT_NAME_MAC_LANGID_TAJIKI,
    TT_NAME_MAC_LANGID_TURKMEN,
    TT_NAME_MAC_LANGID_MONGOLIAN,
    TT_NAME_MAC_LANGID_MONGOLIAN_CYR,
    TT_NAME_MAC_LANGID_PASHTO,
    TT_NAME_MAC_LANGID_KURDISH,
    TT_NAME_MAC_LANGID_KASHMIRI,
    TT_NAME_MAC_LANGID_SINDHI,
    TT_NAME_MAC_LANGID_TIBETAN,
    TT_NAME_MAC_LANGID_NEPALI,
    TT_NAME_MAC_LANGID_SANSKRIT,
    TT_NAME_MAC_LANGID_MARATHI,
    TT_NAME_MAC_LANGID_BENGALI,
    TT_NAME_MAC_LANGID_ASSAMESE,
    TT_NAME_MAC_LANGID_GUJARATI,
    TT_NAME_MAC_LANGID_PUNJABI,
    TT_NAME_MAC_LANGID_ORIYA,
    TT_NAME_MAC_LANGID_MALAYALAM,
    TT_NAME_MAC_LANGID_KANNADA,
    TT_NAME_MAC_LANGID_TAMIL,
    TT_NAME_MAC_LANGID_TELUGU,
    TT_NAME_MAC_LANGID_SINHALESE,
    TT_NAME_MAC_LANGID_BURMESE,
    TT_NAME_MAC_LANGID_KHMER,
    TT_NAME_MAC_LANGID_LAO,
    TT_NAME_MAC_LANGID_VIETNAMESE,
    TT_NAME_MAC_LANGID_INDONESIAN,
    TT_NAME_MAC_LANGID_TAGALONG,
    TT_NAME_MAC_LANGID_MALAY_ROMAN,
    TT_NAME_MAC_LANGID_MALAY_ARABIC,
    TT_NAME_MAC_LANGID_AMHARIC,
    TT_NAME_MAC_LANGID_TIGRINYA,
    TT_NAME_MAC_LANGID_GALLA,
    TT_NAME_MAC_LANGID_SOMALI,
    TT_NAME_MAC_LANGID_SWAHILI,
    TT_NAME_MAC_LANGID_KINYARWANDA,
    TT_NAME_MAC_LANGID_RUNDI,
    TT_NAME_MAC_LANGID_NYANJA,
    TT_NAME_MAC_LANGID_MALAGASY,
    TT_NAME_MAC_LANGID_ESPERANTO,
    TT_NAME_MAC_LANGID_WELSH,
    TT_NAME_MAC_LANGID_BASQUE,
    TT_NAME_MAC_LANGID_CATALAN,
    TT_NAME_MAC_LANGID_LATIN,
    TT_NAME_MAC_LANGID_QUENCHUA,
    TT_NAME_MAC_LANGID_GUARANI,
    TT_NAME_MAC_LANGID_AYMARA,
    TT_NAME_MAC_LANGID_TATAR,
    TT_NAME_MAC_LANGID_UIGHUR,
    TT_NAME_MAC_LANGID_DZONGKHA,
    TT_NAME_MAC_LANGID_JAVANESE,
    TT_NAME_MAC_LANGID_SUNDANESE,
    TT_NAME_MAC_LANGID_GALICIAN,
    TT_NAME_MAC_LANGID_AFRIKAANS,
    TT_NAME_MAC_LANGID_BRETON,
    TT_NAME_MAC_LANGID_INUKTITUT,
    TT_NAME_MAC_LANGID_SCOTTISH_GAELIC,
    TT_NAME_MAC_LANGID_MANX_GAELIC,
    TT_NAME_MAC_LANGID_IRISH_GAELIC,
    TT_NAME_MAC_LANGID_TONGAN,
    TT_NAME_MAC_LANGID_GREEK_POLYTONIC,
    TT_NAME_MAC_LANGID_GREENLANDIC,
    TT_NAME_MAC_LANGID_AZER_ROMAN
};

/* Names are indexed with TT_NAME_MAC_LANGUAGE_ID values */
static const char name_mac_langid_to_locale[][10] = {
    "en-US",
    "fr-FR",
    "de-DE",
    "it-IT",
    "nl-NL",
    "sv-SE",
    "es-ES",
    "da-DA",
    "pt-PT",
    "no-NO",
    "he-IL",
    "ja-JP",
    "ar-AR",
    "fi-FI",
    "el-GR",
    "is-IS",
    "mt-MT",
    "tr-TR",
    "hr-HR",
    "zh-HK",
    "ur-PK",
    "hi-IN",
    "th-TH",
    "ko-KR",
    "lt-LT",
    "pl-PL",
    "hu-HU",
    "et-EE",
    "lv-LV",
    "se-NO",
    "fo-FO",
    "fa-IR",
    "ru-RU",
    "zh-CN",
    "nl-BE",
    "gd-GB",
    "sq-AL",
    "ro-RO",
    "cs-CZ",
    "sk-SK",
    "sl-SI",
    "",
    "sr-Latn",
    "mk-MK",
    "bg-BG",
    "uk-UA",
    "be-BY",
    "uz-Latn",
    "kk-KZ",
    "az-Cyrl-AZ",
    "az-AZ",
    "hy-AM",
    "ka-GE",
    "",
    "",
    "tg-TJ",
    "tk-TM",
    "mn-Mong",
    "mn-MN",
    "ps-AF",
    "ku-Arab",
    "",
    "sd-Arab",
    "bo-CN",
    "ne-NP",
    "sa-IN",
    "mr-IN",
    "bn-IN",
    "as-IN",
    "gu-IN",
    "pa-Arab",
    "or-IN",
    "ml-IN",
    "kn-IN",
    "ta-LK",
    "te-IN",
    "si-LK",
    "",
    "km-KH",
    "lo-LA",
    "vi-VN",
    "id-ID",
    "",
    "ms-MY",
    "ms-Arab",
    "am-ET",
    "ti-ET",
    "",
    "",
    "sw-KE",
    "rw-RW",
    "",
    "",
    "",
    "",
    "cy-GB",
    "eu-ES",
    "ca-ES",
    "",
    "",
    "",
    "",
    "tt-RU",
    "ug-CN",
    "",
    "",
    "",
    "gl-ES",
    "af-ZA",
    "br-FR",
    "iu-Latn-CA",
    "gd-GB",
    "",
    "ga-IE",
    "",
    "",
    "kl-GL",
    "az-Latn"
};

enum OPENTYPE_STRING_ID
{
    OPENTYPE_STRING_COPYRIGHT_NOTICE = 0,
    OPENTYPE_STRING_FAMILY_NAME,
    OPENTYPE_STRING_SUBFAMILY_NAME,
    OPENTYPE_STRING_UNIQUE_IDENTIFIER,
    OPENTYPE_STRING_FULL_FONTNAME,
    OPENTYPE_STRING_VERSION_STRING,
    OPENTYPE_STRING_POSTSCRIPT_FONTNAME,
    OPENTYPE_STRING_TRADEMARK,
    OPENTYPE_STRING_MANUFACTURER,
    OPENTYPE_STRING_DESIGNER,
    OPENTYPE_STRING_DESCRIPTION,
    OPENTYPE_STRING_VENDOR_URL,
    OPENTYPE_STRING_DESIGNER_URL,
    OPENTYPE_STRING_LICENSE_DESCRIPTION,
    OPENTYPE_STRING_LICENSE_INFO_URL,
    OPENTYPE_STRING_RESERVED_ID15,
    OPENTYPE_STRING_PREFERRED_FAMILY_NAME,
    OPENTYPE_STRING_PREFERRED_SUBFAMILY_NAME,
    OPENTYPE_STRING_COMPATIBLE_FULLNAME,
    OPENTYPE_STRING_SAMPLE_TEXT,
    OPENTYPE_STRING_POSTSCRIPT_CID_NAME,
    OPENTYPE_STRING_WWS_FAMILY_NAME,
    OPENTYPE_STRING_WWS_SUBFAMILY_NAME
};

static const UINT16 dwriteid_to_opentypeid[DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_CID_NAME+1] =
{
    (UINT16)-1, /* DWRITE_INFORMATIONAL_STRING_NONE is not used */
    OPENTYPE_STRING_COPYRIGHT_NOTICE,
    OPENTYPE_STRING_VERSION_STRING,
    OPENTYPE_STRING_TRADEMARK,
    OPENTYPE_STRING_MANUFACTURER,
    OPENTYPE_STRING_DESIGNER,
    OPENTYPE_STRING_DESIGNER_URL,
    OPENTYPE_STRING_DESCRIPTION,
    OPENTYPE_STRING_VENDOR_URL,
    OPENTYPE_STRING_LICENSE_DESCRIPTION,
    OPENTYPE_STRING_LICENSE_INFO_URL,
    OPENTYPE_STRING_FAMILY_NAME,
    OPENTYPE_STRING_SUBFAMILY_NAME,
    OPENTYPE_STRING_PREFERRED_FAMILY_NAME,
    OPENTYPE_STRING_PREFERRED_SUBFAMILY_NAME,
    OPENTYPE_STRING_SAMPLE_TEXT,
    OPENTYPE_STRING_FULL_FONTNAME,
    OPENTYPE_STRING_POSTSCRIPT_FONTNAME,
    OPENTYPE_STRING_POSTSCRIPT_CID_NAME
};

/* CPAL table */
struct CPAL_Header_0
{
    USHORT version;
    USHORT numPaletteEntries;
    USHORT numPalette;
    USHORT numColorRecords;
    ULONG  offsetFirstColorRecord;
    USHORT colorRecordIndices[1];
};

/* for version == 1, this comes after full CPAL_Header_0 */
struct CPAL_SubHeader_1
{
    ULONG  offsetPaletteTypeArray;
    ULONG  offsetPaletteLabelArray;
    ULONG  offsetPaletteEntryLabelArray;
};

struct CPAL_ColorRecord
{
    BYTE blue;
    BYTE green;
    BYTE red;
    BYTE alpha;
};

BOOL is_face_type_supported(DWRITE_FONT_FACE_TYPE type)
{
    return (type == DWRITE_FONT_FACE_TYPE_CFF) ||
           (type == DWRITE_FONT_FACE_TYPE_TRUETYPE) ||
           (type == DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION) ||
           (type == DWRITE_FONT_FACE_TYPE_RAW_CFF);
}

HRESULT opentype_analyze_font(IDWriteFontFileStream *stream, UINT32* font_count, DWRITE_FONT_FILE_TYPE *file_type, DWRITE_FONT_FACE_TYPE *face_type, BOOL *supported)
{
    /* TODO: Do font validation */
    DWRITE_FONT_FACE_TYPE face;
    const void *font_data;
    const char* tag;
    void *context;
    HRESULT hr;

    hr = IDWriteFontFileStream_ReadFileFragment(stream, &font_data, 0, sizeof(TTC_Header_V1), &context);
    if (FAILED(hr))
        return hr;

    tag = font_data;
    *file_type = DWRITE_FONT_FILE_TYPE_UNKNOWN;
    face = DWRITE_FONT_FACE_TYPE_UNKNOWN;
    *font_count = 0;

    if (DWRITE_MAKE_OPENTYPE_TAG(tag[0], tag[1], tag[2], tag[3]) == MS_TTCF_TAG)
    {
        const TTC_Header_V1 *header = font_data;
        *font_count = GET_BE_DWORD(header->numFonts);
        *file_type = DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION;
        face = DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION;
    }
    else if (GET_BE_DWORD(*(DWORD*)font_data) == 0x10000)
    {
        *font_count = 1;
        *file_type = DWRITE_FONT_FILE_TYPE_TRUETYPE;
        face = DWRITE_FONT_FACE_TYPE_TRUETYPE;
    }
    else if (DWRITE_MAKE_OPENTYPE_TAG(tag[0], tag[1], tag[2], tag[3]) == MS_OTTO_TAG)
    {
        *file_type = DWRITE_FONT_FILE_TYPE_CFF;
        face = DWRITE_FONT_FACE_TYPE_CFF;
    }

    if (face_type)
        *face_type = face;

    *supported = is_face_type_supported(face);

    IDWriteFontFileStream_ReleaseFileFragment(stream, context);
    return S_OK;
}

HRESULT opentype_get_font_table(IDWriteFontFileStream *stream, DWRITE_FONT_FACE_TYPE type, UINT32 font_index, UINT32 tag,
    const void **table_data, void **table_context, UINT32 *table_size, BOOL *found)
{
    HRESULT hr;
    TTC_SFNT_V1 *font_header = NULL;
    void *sfnt_context;
    TT_TableRecord *table_record = NULL;
    void *table_record_context;
    int table_count, table_offset = 0;
    int i;

    if (found) *found = FALSE;
    if (table_size) *table_size = 0;

    if (type == DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION) {
        const TTC_Header_V1 *ttc_header;
        void * ttc_context;
        hr = IDWriteFontFileStream_ReadFileFragment(stream, (const void**)&ttc_header, 0, sizeof(*ttc_header), &ttc_context);
        if (SUCCEEDED(hr)) {
            if (font_index >= GET_BE_DWORD(ttc_header->numFonts))
                hr = E_INVALIDARG;
            else {
                table_offset = GET_BE_DWORD(ttc_header->OffsetTable[font_index]);
                hr = IDWriteFontFileStream_ReadFileFragment(stream, (const void**)&font_header, table_offset, sizeof(*font_header), &sfnt_context);
            }
            IDWriteFontFileStream_ReleaseFileFragment(stream, ttc_context);
        }
    }
    else
        hr = IDWriteFontFileStream_ReadFileFragment(stream, (const void**)&font_header, 0, sizeof(*font_header), &sfnt_context);

    if (FAILED(hr))
        return hr;

    table_count = GET_BE_WORD(font_header->numTables);
    table_offset += sizeof(*font_header);
    for (i = 0; i < table_count; i++)
    {
        hr = IDWriteFontFileStream_ReadFileFragment(stream, (const void**)&table_record, table_offset, sizeof(*table_record), &table_record_context);
        if (FAILED(hr))
            break;
        if (DWRITE_MAKE_OPENTYPE_TAG(table_record->tag[0], table_record->tag[1], table_record->tag[2], table_record->tag[3]) == tag)
            break;
        IDWriteFontFileStream_ReleaseFileFragment(stream, table_record_context);
        table_offset += sizeof(*table_record);
    }

    IDWriteFontFileStream_ReleaseFileFragment(stream, sfnt_context);
    if (SUCCEEDED(hr) && i < table_count)
    {
        int offset = GET_BE_DWORD(table_record->offset);
        int length = GET_BE_DWORD(table_record->length);
        IDWriteFontFileStream_ReleaseFileFragment(stream, table_record_context);

        if (found) *found = TRUE;
        if (table_size) *table_size = length;
        hr = IDWriteFontFileStream_ReadFileFragment(stream, table_data, offset, length, table_context);
    }

    return hr;
}

/**********
 * CMAP
 **********/

static UINT32 opentype_cmap_get_unicode_ranges_count(const CMAP_Header *CMAP_Table)
{
    UINT32 count = 0;
    int i;

    for (i = 0; i < GET_BE_WORD(CMAP_Table->numTables); i++) {
        WORD type;
        WORD *table;

        if (GET_BE_WORD(CMAP_Table->tables[i].platformID) != 3)
            continue;

        table = (WORD*)(((BYTE*)CMAP_Table) + GET_BE_DWORD(CMAP_Table->tables[i].offset));
        type = GET_BE_WORD(*table);
        TRACE("table type %i\n", type);

        switch (type)
        {
            case OPENTYPE_CMAP_TABLE_SEGMENT_MAPPING:
            {
                CMAP_SegmentMapping_0 *format = (CMAP_SegmentMapping_0*)table;
                count += GET_BE_WORD(format->segCountX2)/2;
                break;
            }
            case OPENTYPE_CMAP_TABLE_SEGMENTED_COVERAGE:
            {
                CMAP_SegmentedCoverage *format = (CMAP_SegmentedCoverage*)table;
                count += GET_BE_DWORD(format->nGroups);
                break;
            }
            default:
                FIXME("table type %i unhandled.\n", type);
        }
    }

    return count;
}

HRESULT opentype_cmap_get_unicode_ranges(void *data, UINT32 max_count, DWRITE_UNICODE_RANGE *ranges, UINT32 *count)
{
    CMAP_Header *CMAP_Table = data;
    int i, k = 0;

    if (!CMAP_Table)
        return E_FAIL;

    *count = opentype_cmap_get_unicode_ranges_count(CMAP_Table);

    for (i = 0; i < GET_BE_WORD(CMAP_Table->numTables) && k < max_count; i++)
    {
        WORD type;
        WORD *table;
        int j;

        if (GET_BE_WORD(CMAP_Table->tables[i].platformID) != 3)
            continue;

        table = (WORD*)(((BYTE*)CMAP_Table) + GET_BE_DWORD(CMAP_Table->tables[i].offset));
        type = GET_BE_WORD(*table);
        TRACE("table type %i\n", type);

        switch (type)
        {
            case OPENTYPE_CMAP_TABLE_SEGMENT_MAPPING:
            {
                CMAP_SegmentMapping_0 *format = (CMAP_SegmentMapping_0*)table;
                UINT16 segment_count = GET_BE_WORD(format->segCountX2)/2;
                UINT16 *startCode = (WORD*)((BYTE*)format + sizeof(CMAP_SegmentMapping_0) + (sizeof(WORD) * segment_count));

                for (j = 0; j < segment_count && GET_BE_WORD(format->endCode[j]) < 0xffff && k < max_count; j++, k++) {
                    ranges[k].first = GET_BE_WORD(startCode[j]);
                    ranges[k].last  = GET_BE_WORD(format->endCode[j]);
                }
                break;
            }
            case OPENTYPE_CMAP_TABLE_SEGMENTED_COVERAGE:
            {
                CMAP_SegmentedCoverage *format = (CMAP_SegmentedCoverage*)table;
                for (j = 0; j < GET_BE_DWORD(format->nGroups) && k < max_count; j++, k++) {
                    ranges[k].first = GET_BE_DWORD(format->groups[j].startCharCode);
                    ranges[k].last  = GET_BE_DWORD(format->groups[j].endCharCode);
                }
                break;
            }
            default:
                FIXME("table type %i unhandled.\n", type);
        }
    }

    return *count > max_count ? E_NOT_SUFFICIENT_BUFFER : S_OK;
}

void opentype_get_font_metrics(IDWriteFontFileStream *stream, DWRITE_FONT_FACE_TYPE face_type, UINT32 face_index,
    DWRITE_FONT_METRICS1 *metrics, DWRITE_CARET_METRICS *caret)
{
    void *os2_context, *head_context, *post_context, *hhea_context;
    const TT_OS2_V2 *tt_os2;
    const TT_HEAD *tt_head;
    const TT_POST *tt_post;
    const TT_HHEA *tt_hhea;

    memset(metrics, 0, sizeof(*metrics));

    opentype_get_font_table(stream, face_type, face_index, MS_OS2_TAG,  (const void**)&tt_os2, &os2_context, NULL, NULL);
    opentype_get_font_table(stream, face_type, face_index, MS_HEAD_TAG, (const void**)&tt_head, &head_context, NULL, NULL);
    opentype_get_font_table(stream, face_type, face_index, MS_POST_TAG, (const void**)&tt_post, &post_context, NULL, NULL);
    opentype_get_font_table(stream, face_type, face_index, MS_HHEA_TAG, (const void**)&tt_hhea, &hhea_context, NULL, NULL);

    if (tt_head) {
        metrics->designUnitsPerEm = GET_BE_WORD(tt_head->unitsPerEm);
        metrics->glyphBoxLeft = GET_BE_WORD(tt_head->xMin);
        metrics->glyphBoxTop = GET_BE_WORD(tt_head->yMax);
        metrics->glyphBoxRight = GET_BE_WORD(tt_head->xMax);
        metrics->glyphBoxBottom = GET_BE_WORD(tt_head->yMin);
    }

    if (caret) {
        if (tt_hhea) {
            caret->slopeRise = GET_BE_WORD(tt_hhea->caretSlopeRise);
            caret->slopeRun = GET_BE_WORD(tt_hhea->caretSlopeRun);
            caret->offset = GET_BE_WORD(tt_hhea->caretOffset);
        }
        else {
            caret->slopeRise = 0;
            caret->slopeRun = 0;
            caret->offset = 0;
        }
    }

    if (tt_os2) {
        USHORT version = GET_BE_WORD(tt_os2->version);

        metrics->ascent  = GET_BE_WORD(tt_os2->usWinAscent);
        /* Some fonts have usWinDescent value stored as signed short, which could be wrongly
           interpreted as large unsigned value. */
        metrics->descent = abs((SHORT)GET_BE_WORD(tt_os2->usWinDescent));

        /* line gap is estimated using two sets of ascender/descender values and 'hhea' line gap */
        if (tt_hhea) {
            SHORT descender = (SHORT)GET_BE_WORD(tt_hhea->descender);
            INT32 linegap;

            linegap = GET_BE_WORD(tt_hhea->ascender) + abs(descender) + GET_BE_WORD(tt_hhea->linegap) -
                metrics->ascent - metrics->descent;
            metrics->lineGap = linegap > 0 ? linegap : 0;
        }

        metrics->strikethroughPosition  = GET_BE_WORD(tt_os2->yStrikeoutPosition);
        metrics->strikethroughThickness = GET_BE_WORD(tt_os2->yStrikeoutSize);
        metrics->subscriptPositionX = GET_BE_WORD(tt_os2->ySubscriptXOffset);
        /* Y offset is stored as positive offset below baseline */
        metrics->subscriptPositionY = -GET_BE_WORD(tt_os2->ySubscriptYOffset);
        metrics->subscriptSizeX = GET_BE_WORD(tt_os2->ySubscriptXSize);
        metrics->subscriptSizeY = GET_BE_WORD(tt_os2->ySubscriptYSize);
        metrics->superscriptPositionX = GET_BE_WORD(tt_os2->ySuperscriptXOffset);
        metrics->superscriptPositionY = GET_BE_WORD(tt_os2->ySuperscriptYOffset);
        metrics->superscriptSizeX = GET_BE_WORD(tt_os2->ySuperscriptXSize);
        metrics->superscriptSizeY = GET_BE_WORD(tt_os2->ySuperscriptYSize);

        /* version 2 fields */
        if (version >= 2) {
            metrics->capHeight = GET_BE_WORD(tt_os2->sCapHeight);
            metrics->xHeight   = GET_BE_WORD(tt_os2->sxHeight);
        }

        /* version 4 fields */
        if (version >= 4) {
            if (GET_BE_WORD(tt_os2->fsSelection) & OS2_FSSELECTION_USE_TYPO_METRICS) {
                SHORT descent = GET_BE_WORD(tt_os2->sTypoDescender);
                metrics->ascent = GET_BE_WORD(tt_os2->sTypoAscender);
                metrics->descent = descent < 0 ? -descent : 0;
                metrics->lineGap = GET_BE_WORD(tt_os2->sTypoLineGap);
                metrics->hasTypographicMetrics = TRUE;
            }
        }
    }

    if (tt_post) {
        metrics->underlinePosition = GET_BE_WORD(tt_post->underlinePosition);
        metrics->underlineThickness = GET_BE_WORD(tt_post->underlineThickness);
    }

    /* estimate missing metrics */
    if (metrics->xHeight == 0)
        metrics->xHeight = metrics->designUnitsPerEm / 2;
    if (metrics->capHeight == 0)
        metrics->capHeight = metrics->designUnitsPerEm * 7 / 10;

    if (tt_os2)
        IDWriteFontFileStream_ReleaseFileFragment(stream, os2_context);
    if (tt_head)
        IDWriteFontFileStream_ReleaseFileFragment(stream, head_context);
    if (tt_post)
        IDWriteFontFileStream_ReleaseFileFragment(stream, post_context);
    if (tt_hhea)
        IDWriteFontFileStream_ReleaseFileFragment(stream, hhea_context);
}

void opentype_get_font_properties(IDWriteFontFileStream *stream, DWRITE_FONT_FACE_TYPE type, UINT32 index,
    struct dwrite_font_props *props)
{
    void *os2_context, *head_context;
    const TT_OS2_V2 *tt_os2;
    const TT_HEAD *tt_head;

    opentype_get_font_table(stream, type, index, MS_OS2_TAG,  (const void**)&tt_os2, &os2_context, NULL, NULL);
    opentype_get_font_table(stream, type, index, MS_HEAD_TAG, (const void**)&tt_head, &head_context, NULL, NULL);

    /* default stretch, weight and style to normal */
    props->stretch = DWRITE_FONT_STRETCH_NORMAL;
    props->weight = DWRITE_FONT_WEIGHT_NORMAL;
    props->style = DWRITE_FONT_STYLE_NORMAL;
    memset(&props->panose, 0, sizeof(props->panose));

    /* DWRITE_FONT_STRETCH enumeration values directly match font data values */
    if (tt_os2) {
        USHORT version = GET_BE_WORD(tt_os2->version);
        USHORT fsSelection = GET_BE_WORD(tt_os2->fsSelection);
        USHORT usWeightClass = GET_BE_WORD(tt_os2->usWeightClass);

        if (GET_BE_WORD(tt_os2->usWidthClass) <= DWRITE_FONT_STRETCH_ULTRA_EXPANDED)
            props->stretch = GET_BE_WORD(tt_os2->usWidthClass);

        if (usWeightClass >= 1 && usWeightClass <= 9)
            usWeightClass *= 100;

        if (usWeightClass > DWRITE_FONT_WEIGHT_ULTRA_BLACK)
            props->weight = DWRITE_FONT_WEIGHT_ULTRA_BLACK;
        else
            props->weight = usWeightClass;

        if (version >= 4 && (fsSelection & OS2_FSSELECTION_OBLIQUE))
            props->style = DWRITE_FONT_STYLE_OBLIQUE;
        else if (fsSelection & OS2_FSSELECTION_ITALIC)
            props->style = DWRITE_FONT_STYLE_ITALIC;
        memcpy(&props->panose, &tt_os2->panose, sizeof(props->panose));
    }
    else if (tt_head) {
        USHORT macStyle = GET_BE_WORD(tt_head->macStyle);

        if (macStyle & TT_HEAD_MACSTYLE_CONDENSED)
            props->stretch = DWRITE_FONT_STRETCH_CONDENSED;
        else if (macStyle & TT_HEAD_MACSTYLE_EXTENDED)
            props->stretch = DWRITE_FONT_STRETCH_EXPANDED;

        if (macStyle & TT_HEAD_MACSTYLE_BOLD)
            props->weight = DWRITE_FONT_WEIGHT_BOLD;

        if (macStyle & TT_HEAD_MACSTYLE_ITALIC)
            props->style = DWRITE_FONT_STYLE_ITALIC;
    }

    TRACE("stretch=%d, weight=%d, style %d\n", props->stretch, props->weight, props->style);

    if (tt_os2)
        IDWriteFontFileStream_ReleaseFileFragment(stream, os2_context);
    if (tt_head)
        IDWriteFontFileStream_ReleaseFileFragment(stream, head_context);
}

static UINT get_name_record_codepage(enum OPENTYPE_PLATFORM_ID platform, USHORT encoding)
{
    UINT codepage = 0;

    switch (platform) {
    case OPENTYPE_PLATFORM_UNICODE:
        break;
    case OPENTYPE_PLATFORM_MAC:
        switch (encoding)
        {
            case TT_NAME_MAC_ENCODING_ROMAN:
                codepage = 10000;
                break;
            case TT_NAME_MAC_ENCODING_JAPANESE:
                codepage = 10001;
                break;
            case TT_NAME_MAC_ENCODING_TRAD_CHINESE:
                codepage = 10002;
                break;
            case TT_NAME_MAC_ENCODING_KOREAN:
                codepage = 10003;
                break;
            case TT_NAME_MAC_ENCODING_ARABIC:
                codepage = 10004;
                break;
            case TT_NAME_MAC_ENCODING_HEBREW:
                codepage = 10005;
                break;
            case TT_NAME_MAC_ENCODING_GREEK:
                codepage = 10006;
                break;
            case TT_NAME_MAC_ENCODING_RUSSIAN:
                codepage = 10007;
                break;
            case TT_NAME_MAC_ENCODING_SIMPL_CHINESE:
                codepage = 10008;
                break;
            case TT_NAME_MAC_ENCODING_THAI:
                codepage = 10021;
                break;
            default:
                FIXME("encoding %u not handled, platform %d.\n", encoding, platform);
                break;
        }
        break;
    case OPENTYPE_PLATFORM_WIN:
        switch (encoding)
        {
            case TT_NAME_WINDOWS_ENCODING_SYMBOL:
            case TT_NAME_WINDOWS_ENCODING_UCS2:
                break;
            case TT_NAME_WINDOWS_ENCODING_SJIS:
                codepage = 932;
                break;
            case TT_NAME_WINDOWS_ENCODING_PRC:
                codepage = 936;
                break;
            case TT_NAME_WINDOWS_ENCODING_BIG5:
                codepage = 950;
                break;
            case TT_NAME_WINDOWS_ENCODING_WANSUNG:
                codepage = 20949;
                break;
            case TT_NAME_WINDOWS_ENCODING_JOHAB:
                codepage = 1361;
                break;
            default:
                FIXME("encoding %u not handled, platform %d.\n", encoding, platform);
                break;
        }
        break;
    default:
        FIXME("unknown platform %d\n", platform);
    }

    return codepage;
}

static void get_name_record_locale(enum OPENTYPE_PLATFORM_ID platform, USHORT lang_id, WCHAR *locale, USHORT locale_len)
{
    static const WCHAR enusW[] = {'e','n','-','U','S',0};

    switch (platform) {
    case OPENTYPE_PLATFORM_MAC:
    {
        const char *locale_name = NULL;

        if (lang_id > TT_NAME_MAC_LANGID_AZER_ROMAN)
            ERR("invalid mac lang id %d\n", lang_id);
        else if (!name_mac_langid_to_locale[lang_id][0])
            FIXME("failed to map mac lang id %d to locale name\n", lang_id);
        else
            locale_name = name_mac_langid_to_locale[lang_id];

        if (locale_name)
            MultiByteToWideChar(CP_ACP, 0, name_mac_langid_to_locale[lang_id], -1, locale, locale_len);
        else
            strcpyW(locale, enusW);
        break;
    }
    case OPENTYPE_PLATFORM_WIN:
        if (!LCIDToLocaleName(MAKELCID(lang_id, SORT_DEFAULT), locale, locale_len, 0)) {
            FIXME("failed to get locale name for lcid=0x%08x\n", MAKELCID(lang_id, SORT_DEFAULT));
            strcpyW(locale, enusW);
        }
        break;
    default:
        FIXME("unknown platform %d\n", platform);
    }
}

static HRESULT opentype_get_font_strings_from_id(const void *table_data, enum OPENTYPE_STRING_ID id, IDWriteLocalizedStrings **strings)
{
    const TT_NAME_V0 *header;
    BYTE *storage_area = 0;
    USHORT count = 0;
    WORD format;
    BOOL exists;
    HRESULT hr;
    int i;

    if (!table_data)
        return E_FAIL;

    hr = create_localizedstrings(strings);
    if (FAILED(hr)) return hr;

    header = table_data;
    format = GET_BE_WORD(header->format);

    switch (format) {
    case 0:
    case 1:
        break;
    default:
        FIXME("unsupported NAME format %d\n", format);
    }


    storage_area = (LPBYTE)table_data + GET_BE_WORD(header->stringOffset);
    count = GET_BE_WORD(header->count);

    exists = FALSE;
    for (i = 0; i < count; i++) {
        const TT_NameRecord *record = &header->nameRecord[i];
        USHORT lang_id, length, offset, encoding, platform;

        if (GET_BE_WORD(record->nameID) != id)
            continue;

        exists = TRUE;

        /* Right now only accept unicode and windows encoded fonts */
        platform = GET_BE_WORD(record->platformID);
        if (platform != OPENTYPE_PLATFORM_UNICODE &&
            platform != OPENTYPE_PLATFORM_MAC &&
            platform != OPENTYPE_PLATFORM_WIN)
        {
            FIXME("platform %i not supported\n", platform);
            continue;
        }

        /* Skip such entries for now, as it's not clear which locale is implied when
           unicode platform is used. Also fonts tend to duplicate those strings as
           WIN platform entries. */
        if (platform == OPENTYPE_PLATFORM_UNICODE)
            continue;

        lang_id = GET_BE_WORD(record->languageID);
        length = GET_BE_WORD(record->length);
        offset = GET_BE_WORD(record->offset);
        encoding = GET_BE_WORD(record->encodingID);

        if (lang_id < 0x8000) {
            WCHAR locale[LOCALE_NAME_MAX_LENGTH];
            WCHAR *name_string;
            UINT codepage;

            codepage = get_name_record_codepage(platform, encoding);
            get_name_record_locale(platform, lang_id, locale, sizeof(locale)/sizeof(WCHAR));

            if (codepage) {
                DWORD len = MultiByteToWideChar(codepage, 0, (LPSTR)(storage_area + offset), length, NULL, 0);
                name_string = heap_alloc(sizeof(WCHAR) * (len+1));
                MultiByteToWideChar(codepage, 0, (LPSTR)(storage_area + offset), length, name_string, len);
                name_string[len] = 0;
            }
            else {
                int i;

                length /= sizeof(WCHAR);
                name_string = heap_strdupnW((LPWSTR)(storage_area + offset), length);
                for (i = 0; i < length; i++)
                    name_string[i] = GET_BE_WORD(name_string[i]);
            }

            TRACE("string %s for locale %s found\n", debugstr_w(name_string), debugstr_w(locale));
            add_localizedstring(*strings, locale, name_string);
            heap_free(name_string);
        }
        else {
            FIXME("handle NAME format 1\n");
            continue;
        }
    }

    if (!exists) {
        IDWriteLocalizedStrings_Release(*strings);
        *strings = NULL;
    }

    return exists ? S_OK : E_FAIL;
}

/* Provides a conversion from DWRITE to OpenType name ids, input id be valid, it's not checked. */
HRESULT opentype_get_font_info_strings(const void *table_data, DWRITE_INFORMATIONAL_STRING_ID id, IDWriteLocalizedStrings **strings)
{
    return opentype_get_font_strings_from_id(table_data, dwriteid_to_opentypeid[id], strings);
}

/* FamilyName locating order is WWS Family Name -> Preferred Family Name -> Family Name. If font claims to
   have 'Preferred Family Name' in WWS format, then WWS name is not used. */
HRESULT opentype_get_font_familyname(IDWriteFontFileStream *stream, DWRITE_FONT_FACE_TYPE facetype, UINT32 index,
    IDWriteLocalizedStrings **names)
{
    const TT_OS2_V2 *tt_os2;
    void *os2_context, *name_context;
    const void *name_table;
    HRESULT hr;

    opentype_get_font_table(stream, facetype, index, MS_OS2_TAG,  (const void**)&tt_os2, &os2_context, NULL, NULL);
    opentype_get_font_table(stream, facetype, index, MS_NAME_TAG, &name_table, &name_context, NULL, NULL);

    *names = NULL;

    /* if Preferred Family doesn't conform to WWS model try WWS name */
    if (tt_os2 && !(GET_BE_WORD(tt_os2->fsSelection) & OS2_FSSELECTION_WWS))
        hr = opentype_get_font_strings_from_id(name_table, OPENTYPE_STRING_WWS_FAMILY_NAME, names);
    else
        hr = E_FAIL;

    if (FAILED(hr))
        hr = opentype_get_font_strings_from_id(name_table, OPENTYPE_STRING_PREFERRED_FAMILY_NAME, names);
    if (FAILED(hr))
        hr = opentype_get_font_strings_from_id(name_table, OPENTYPE_STRING_FAMILY_NAME, names);

    if (tt_os2)
        IDWriteFontFileStream_ReleaseFileFragment(stream, os2_context);
    if (name_context)
        IDWriteFontFileStream_ReleaseFileFragment(stream, name_context);

    return hr;
}

/* FaceName locating order is WWS Face Name -> Preferred Face Name -> Face Name. If font claims to
   have 'Preferred Face Name' in WWS format, then WWS name is not used. */
HRESULT opentype_get_font_facename(IDWriteFontFileStream *stream, DWRITE_FONT_FACE_TYPE facetype, UINT32 index,
    IDWriteLocalizedStrings **names)
{
    const TT_OS2_V2 *tt_os2;
    void *os2_context, *name_context;
    const void *name_table;
    HRESULT hr;

    opentype_get_font_table(stream, facetype, index, MS_OS2_TAG,  (const void**)&tt_os2, &os2_context, NULL, NULL);
    opentype_get_font_table(stream, facetype, index, MS_NAME_TAG, &name_table, &name_context, NULL, NULL);

    *names = NULL;

    /* if Preferred Family doesn't conform to WWS model try WWS name */
    if (tt_os2 && !(GET_BE_WORD(tt_os2->fsSelection) & OS2_FSSELECTION_WWS))
        hr = opentype_get_font_strings_from_id(name_table, OPENTYPE_STRING_WWS_SUBFAMILY_NAME, names);
    else
        hr = E_FAIL;

    if (FAILED(hr))
        hr = opentype_get_font_strings_from_id(name_table, OPENTYPE_STRING_PREFERRED_SUBFAMILY_NAME, names);
    if (FAILED(hr))
        hr = opentype_get_font_strings_from_id(name_table, OPENTYPE_STRING_SUBFAMILY_NAME, names);

    if (tt_os2)
        IDWriteFontFileStream_ReleaseFileFragment(stream, os2_context);
    if (name_context)
        IDWriteFontFileStream_ReleaseFileFragment(stream, name_context);

    return hr;
}

static inline const OT_Script *opentype_get_script(const OT_ScriptList *scriptlist, UINT32 scripttag)
{
    UINT16 j;

    for (j = 0; j < GET_BE_WORD(scriptlist->ScriptCount); j++) {
        const char *tag = scriptlist->ScriptRecord[j].ScriptTag;
        if (scripttag == DWRITE_MAKE_OPENTYPE_TAG(tag[0], tag[1], tag[2], tag[3]))
            return (OT_Script*)((BYTE*)scriptlist + GET_BE_WORD(scriptlist->ScriptRecord[j].Script));
    }

    return NULL;
}

static inline const OT_LangSys *opentype_get_langsys(const OT_Script *script, UINT32 languagetag)
{
    UINT16 j;

    for (j = 0; j < GET_BE_WORD(script->LangSysCount); j++) {
        const char *tag = script->LangSysRecord[j].LangSysTag;
        if (languagetag == DWRITE_MAKE_OPENTYPE_TAG(tag[0], tag[1], tag[2], tag[3]))
            return (OT_LangSys*)((BYTE*)script + GET_BE_WORD(script->LangSysRecord[j].LangSys));
    }

    return NULL;
}

static void opentype_add_font_features(const GPOS_GSUB_Header *header, const OT_LangSys *langsys,
    UINT32 max_tagcount, UINT32 *count, DWRITE_FONT_FEATURE_TAG *tags)
{
    const OT_FeatureList *features = (const OT_FeatureList*)((const BYTE*)header + GET_BE_WORD(header->FeatureList));
    UINT16 j;

    for (j = 0; j < GET_BE_WORD(langsys->FeatureCount); j++) {
        const OT_FeatureRecord *feature = &features->FeatureRecord[langsys->FeatureIndex[j]];
        const char *tag = feature->FeatureTag;

        if (*count < max_tagcount)
            tags[*count] = DWRITE_MAKE_OPENTYPE_TAG(tag[0], tag[1], tag[2], tag[3]);

        (*count)++;
    }
}

HRESULT opentype_get_typographic_features(IDWriteFontFace *fontface, UINT32 scripttag, UINT32 languagetag, UINT32 max_tagcount,
    UINT32 *count, DWRITE_FONT_FEATURE_TAG *tags)
{
    UINT32 tables[2] = { MS_GSUB_TAG, MS_GPOS_TAG };
    HRESULT hr;
    UINT8 i;

    *count = 0;
    for (i = 0; i < sizeof(tables)/sizeof(tables[0]); i++) {
        const OT_ScriptList *scriptlist;
        const GPOS_GSUB_Header *header;
        const OT_Script *script;
        const void *ptr;
        void *context;
        UINT32 size;
        BOOL exists;

        exists = FALSE;
        hr = IDWriteFontFace_TryGetFontTable(fontface, tables[i], &ptr, &size, &context, &exists);
        if (FAILED(hr))
            return hr;

        if (!exists)
            continue;

        header = (const GPOS_GSUB_Header*)ptr;
        scriptlist = (const OT_ScriptList*)((const BYTE*)header + GET_BE_WORD(header->ScriptList));

        script = opentype_get_script(scriptlist, scripttag);
        if (script) {
            const OT_LangSys *langsys = opentype_get_langsys(script, languagetag);
            if (langsys)
                opentype_add_font_features(header, langsys, max_tagcount, count, tags);
        }

        IDWriteFontFace_ReleaseFontTable(fontface, context);
    }

    return *count > max_tagcount ? E_NOT_SUFFICIENT_BUFFER : S_OK;
}

static const struct VDMX_group *find_vdmx_group(const struct VDMX_Header *hdr)
{
    WORD num_ratios, i, group_offset = 0;
    struct VDMX_Ratio *ratios = (struct VDMX_Ratio*)(hdr + 1);
    BYTE dev_x_ratio = 1, dev_y_ratio = 1;

    num_ratios = GET_BE_WORD(hdr->numRatios);

    for (i = 0; i < num_ratios; i++) {

        if (!ratios[i].bCharSet) continue;

        if ((ratios[i].xRatio == 0 && ratios[i].yStartRatio == 0 &&
             ratios[i].yEndRatio == 0) ||
	   (ratios[i].xRatio == dev_x_ratio && ratios[i].yStartRatio <= dev_y_ratio &&
            ratios[i].yEndRatio >= dev_y_ratio))
        {
            group_offset = GET_BE_WORD(*((WORD *)(ratios + num_ratios) + i));
            break;
        }
    }
    if (group_offset)
        return (const struct VDMX_group *)((BYTE *)hdr + group_offset);
    return NULL;
}

BOOL opentype_get_vdmx_size(const void *data, INT emsize, UINT16 *ascent, UINT16 *descent)
{
    const struct VDMX_Header *hdr = (const struct VDMX_Header*)data;
    const struct VDMX_group *group;
    const struct VDMX_vTable *tables;
    WORD recs, i;

    if (!data)
        return FALSE;

    group = find_vdmx_group(hdr);
    if (!group)
        return FALSE;

    recs = GET_BE_WORD(group->recs);
    if (emsize < group->startsz || emsize >= group->endsz) return FALSE;

    tables = (const struct VDMX_vTable *)(group + 1);
    for (i = 0; i < recs; i++) {
        WORD ppem = GET_BE_WORD(tables[i].yPelHeight);
        if (ppem > emsize) {
            FIXME("interpolate %d\n", emsize);
            return FALSE;
        }

        if (ppem == emsize) {
            *ascent = (SHORT)GET_BE_WORD(tables[i].yMax);
            *descent = -(SHORT)GET_BE_WORD(tables[i].yMin);
            return TRUE;
        }
    }
    return FALSE;
}

WORD opentype_get_gasp_flags(const WORD *ptr, UINT32 size, INT emsize)
{
    WORD num_recs, version;
    WORD flags = 0;

    if (!ptr)
        return 0;

    version  = GET_BE_WORD( *ptr++ );
    num_recs = GET_BE_WORD( *ptr++ );
    if (version > 1 || size < (num_recs * 2 + 2) * sizeof(WORD)) {
        ERR("unsupported gasp table: ver %d size %d recs %d\n", version, size, num_recs);
        goto done;
    }

    while (num_recs--) {
        flags = GET_BE_WORD( *(ptr + 1) );
        if (emsize <= GET_BE_WORD( *ptr )) break;
        ptr += 2;
    }

done:
    return flags;
}

UINT32 opentype_get_cpal_palettecount(const void *cpal)
{
    const struct CPAL_Header_0 *header = (const struct CPAL_Header_0*)cpal;
    return header ? GET_BE_WORD(header->numPalette) : 0;
}

UINT32 opentype_get_cpal_paletteentrycount(const void *cpal)
{
    const struct CPAL_Header_0 *header = (const struct CPAL_Header_0*)cpal;
    return header ? GET_BE_WORD(header->numPaletteEntries) : 0;
}

HRESULT opentype_get_cpal_entries(const void *cpal, UINT32 palette, UINT32 first_entry_index, UINT32 entry_count,
    DWRITE_COLOR_F *entries)
{
    const struct CPAL_Header_0 *header = (const struct CPAL_Header_0*)cpal;
    const struct CPAL_ColorRecord *records;
    UINT32 palettecount, entrycount, i;

    if (!header) return DWRITE_E_NOCOLOR;

    palettecount = GET_BE_WORD(header->numPalette);
    if (palette >= palettecount)
        return DWRITE_E_NOCOLOR;

    entrycount = GET_BE_WORD(header->numPaletteEntries);
    if (first_entry_index + entry_count > entrycount)
        return E_INVALIDARG;

    records = (const struct CPAL_ColorRecord*)((BYTE*)cpal + GET_BE_DWORD(header->offsetFirstColorRecord));
    first_entry_index += GET_BE_WORD(header->colorRecordIndices[palette]);

    for (i = 0; i < entry_count; i++) {
        entries[i].r = records[first_entry_index + i].red   / 255.0f;
        entries[i].g = records[first_entry_index + i].green / 255.0f;
        entries[i].b = records[first_entry_index + i].blue  / 255.0f;
        entries[i].a = records[first_entry_index + i].alpha / 255.0f;
    }

    return S_OK;
}

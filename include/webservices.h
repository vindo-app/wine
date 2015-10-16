/*
 * Copyright 2015 Nikolay Sivov for CodeWeavers
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

#ifndef __WINE_WEBSERVICES_H
#define __WINE_WEBSERVICES_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct _WS_ERROR WS_ERROR;
typedef struct _WS_HEAP WS_HEAP;
typedef struct _WS_XML_BUFFER WS_XML_BUFFER;
typedef struct _WS_XML_READER WS_XML_READER;

struct _WS_STRUCT_DESCRIPTION;
struct _WS_XML_STRING;

typedef enum {
    WS_ERROR_PROPERTY_STRING_COUNT,
    WS_ERROR_PROPERTY_ORIGINAL_ERROR_CODE,
    WS_ERROR_PROPERTY_LANGID
} WS_ERROR_PROPERTY_ID;

typedef struct _WS_ERROR_PROPERTY {
    WS_ERROR_PROPERTY_ID id;
    void                *value;
    ULONG                valueSize;
} WS_ERROR_PROPERTY;

typedef enum {
    WS_HEAP_PROPERTY_MAX_SIZE,
    WS_HEAP_PROPERTY_TRIM_SIZE,
    WS_HEAP_PROPERTY_REQUESTED_SIZE,
    WS_HEAP_PROPERTY_ACTUAL_SIZE
} WS_HEAP_PROPERTY_ID;

typedef struct _WS_HEAP_PROPERTY {
    WS_HEAP_PROPERTY_ID id;
    void               *value;
    ULONG               valueSize;
} WS_HEAP_PROPERTY;

typedef enum {
    WS_XML_READER_PROPERTY_MAX_DEPTH,
    WS_XML_READER_PROPERTY_ALLOW_FRAGMENT,
    WS_XML_READER_PROPERTY_MAX_ATTRIBUTES,
    WS_XML_READER_PROPERTY_READ_DECLARATION,
    WS_XML_READER_PROPERTY_CHARSET,
    WS_XML_READER_PROPERTY_ROW,
    WS_XML_READER_PROPERTY_COLUMN,
    WS_XML_READER_PROPERTY_UTF8_TRIM_SIZE,
    WS_XML_READER_PROPERTY_STREAM_BUFFER_SIZE,
    WS_XML_READER_PROPERTY_IN_ATTRIBUTE,
    WS_XML_READER_PROPERTY_STREAM_MAX_ROOT_MIME_PART_SIZE,
    WS_XML_READER_PROPERTY_STREAM_MAX_MIME_HEADERS_SIZE,
    WS_XML_READER_PROPERTY_MAX_MIME_PARTS,
    WS_XML_READER_PROPERTY_ALLOW_INVALID_CHARACTER_REFERENCES,
    WS_XML_READER_PROPERTY_MAX_NAMESPACES
} WS_XML_READER_PROPERTY_ID;

typedef struct _WS_XML_READER_PROPERTY {
    WS_XML_READER_PROPERTY_ID id;
    void                     *value;
    ULONG                     valueSize;
} WS_XML_READER_PROPERTY;

typedef enum {
    WS_XML_READER_ENCODING_TYPE_TEXT   = 1,
    WS_XML_READER_ENCODING_TYPE_BINARY = 2,
    WS_XML_READER_ENCODING_TYPE_MTOM   = 3,
    WS_XML_READER_ENCODING_TYPE_RAW    = 4
} WS_XML_READER_ENCODING_TYPE;

typedef struct _WS_XML_READER_ENCODING {
    WS_XML_READER_ENCODING_TYPE encodingType;
} WS_XML_READER_ENCODING;

typedef enum {
    WS_CHARSET_AUTO,
    WS_CHARSET_UTF8,
    WS_CHARSET_UTF16LE,
    WS_CHARSET_UTF16BE
} WS_CHARSET;

typedef struct _WS_XML_READER_TEXT_ENCODING {
    WS_XML_READER_ENCODING encoding;
    WS_CHARSET charSet;
} WS_XML_READER_TEXT_ENCODING;

typedef enum {
    WS_XML_READER_INPUT_TYPE_BUFFER = 1,
    WS_XML_READER_INPUT_TYPE_STREAM = 2
} WS_XML_READER_INPUT_TYPE;

typedef struct _WS_XML_READER_INPUT {
    WS_XML_READER_INPUT_TYPE inputType;
} WS_XML_READER_INPUT;

typedef struct _WS_XML_READER_BUFFER_INPUT {
    WS_XML_READER_INPUT input;
    void *encodedData;
    ULONG encodedDataSize;
} WS_XML_READER_BUFFER_INPUT;

typedef enum {
    WS_SHORT_CALLBACK,
    WS_LONG_CALLBACK
} WS_CALLBACK_MODEL;

typedef void (CALLBACK *WS_ASYNC_CALLBACK)
    (HRESULT, WS_CALLBACK_MODEL, void *);

typedef struct _WS_ASYNC_CONTEXT {
    WS_ASYNC_CALLBACK callback;
    void             *callbackState;
} WS_ASYNC_CONTEXT;

typedef HRESULT (CALLBACK *WS_READ_CALLBACK)
    (void*, void*, ULONG, ULONG*, const WS_ASYNC_CONTEXT*, WS_ERROR*);

typedef struct _WS_XML_READER_STREAM_INPUT {
    WS_XML_READER_INPUT input;
    WS_READ_CALLBACK readCallback;
    void *readCallbackState;
} WS_XML_READER_STREAM_INPUT;

typedef struct _WS_XML_DICTIONARY {
    GUID                   guid;
    struct _WS_XML_STRING *strings;
    ULONG                  stringCount;
    BOOL                   isConst;
} WS_XML_DICTIONARY;

typedef struct _WS_XML_STRING {
    ULONG              length;
    BYTE              *bytes;
    WS_XML_DICTIONARY *dictionary;
    ULONG              id;
} WS_XML_STRING;

typedef enum {
    WS_ELEMENT_TYPE_MAPPING         = 1,
    WS_ATTRIBUTE_TYPE_MAPPING       = 2,
    WS_ELEMENT_CONTENT_TYPE_MAPPING = 3,
    WS_ANY_ELEMENT_TYPE_MAPPING     = 4
} WS_TYPE_MAPPING;

typedef enum {
    WS_BOOL_TYPE,
    WS_INT8_TYPE,
    WS_INT16_TYPE,
    WS_INT32_TYPE,
    WS_INT64_TYPE,
    WS_UINT8_TYPE,
    WS_UINT16_TYPE,
    WS_UINT32_TYPE,
    WS_UINT64_TYPE,
    WS_FLOAT_TYPE,
    WS_DOUBLE_TYPE,
    WS_DECIMAL_TYPE,
    WS_DATETIME_TYPE,
    WS_TIMESPAN_TYPE,
    WS_GUID_TYPE,
    WS_UNIQUE_ID_TYPE,
    WS_STRING_TYPE,
    WS_WSZ_TYPE,
    WS_BYTES_TYPE,
    WS_XML_STRING_TYPE,
    WS_XML_QNAME_TYPE,
    WS_XML_BUFFER_TYPE,
    WS_CHAR_ARRAY_TYPE,
    WS_UTF8_ARRAY_TYPE,
    WS_BYTE_ARRAY_TYPE,
    WS_DESCRIPTION_TYPE,
    WS_STRUCT_TYPE,
    WS_CUSTOM_TYPE,
    WS_ENDPOINT_ADDRESS_TYPE,
    WS_FAULT_TYPE,
    WS_VOID_TYPE,
    WS_ENUM_TYPE,
    WS_DURATION_TYPE,
    WS_UNION_TYPE,
    WS_ANY_ATTRIBUTES_TYPE
} WS_TYPE;

typedef enum {
    WS_READ_REQUIRED_VALUE   = 1,
    WS_READ_REQUIRED_POINTER = 2,
    WS_READ_OPTIONAL_POINTER = 3,
    WS_READ_NILLABLE_POINTER = 4,
    WS_READ_NILLABLE_VALUE   = 5
} WS_READ_OPTION;

typedef enum {
    WS_TYPE_ATTRIBUTE_FIELD_MAPPING,
    WS_ATTRIBUTE_FIELD_MAPPING,
    WS_ELEMENT_FIELD_MAPPING,
    WS_REPEATING_ELEMENT_FIELD_MAPPING,
    WS_TEXT_FIELD_MAPPING,
    WS_NO_FIELD_MAPPING,
    WS_XML_ATTRIBUTE_FIELD_MAPPING,
    WS_ELEMENT_CHOICE_FIELD_MAPPING,
    WS_REPEATING_ELEMENT_CHOICE_FIELD_MAPPING,
    WS_ANY_ELEMENT_FIELD_MAPPING,
    WS_REPEATING_ANY_ELEMENT_FIELD_MAPPING,
    WS_ANY_CONTENT_FIELD_MAPPING,
    WS_ANY_ATTRIBUTES_FIELD_MAPPING
} WS_FIELD_MAPPING;

typedef struct _WS_DEFAULT_VALUE {
    void *value;
    ULONG valueSize;
} WS_DEFAULT_VALUE;

typedef struct _WS_ITEM_RANGE {
    ULONG minItemCount;
    ULONG maxItemCount;
} WS_ITEM_RANGE;

typedef struct _WS_FIELD_DESCRIPTION {
    WS_FIELD_MAPPING mapping;
    WS_XML_STRING *localName;
    WS_XML_STRING *ns;
    WS_TYPE type;
    void *typeDescription;
    ULONG offset;
    ULONG options;
    WS_DEFAULT_VALUE *defaultValue;
    ULONG countOffset;
    WS_XML_STRING *itemLocalName;
    WS_XML_STRING *itemNs;
    WS_ITEM_RANGE *itemRange;
} WS_FIELD_DESCRIPTION;

typedef struct _WS_STRUCT_DESCRIPTION {
    ULONG size;
    ULONG alignment;
    WS_FIELD_DESCRIPTION **fields;
    ULONG fieldCount;
    WS_XML_STRING *typeLocalName;
    WS_XML_STRING *typeNs;
    struct _WS_STRUCT_DESCRIPTION *parentType;
    struct _WS_STRUCT_DESCRIPTION **subTypes;
    ULONG subTypeCount;
    ULONG structOptions;
} WS_STRUCT_DESCRIPTION;

typedef struct _WS_ATTRIBUTE_DESCRIPTION {
    WS_XML_STRING *attributeLocalName;
    WS_XML_STRING *attributeNs;
    WS_TYPE type;
    void *typeDescription;
} WS_ATTRIBUTE_DESCRIPTION;

typedef struct _WS_STRING {
    ULONG length;
    WCHAR *chars;
} WS_STRING;

typedef enum {
    WS_XML_NODE_TYPE_ELEMENT     = 1,
    WS_XML_NODE_TYPE_TEXT        = 2,
    WS_XML_NODE_TYPE_END_ELEMENT = 3,
    WS_XML_NODE_TYPE_COMMENT     = 4,
    WS_XML_NODE_TYPE_CDATA       = 6,
    WS_XML_NODE_TYPE_END_CDATA   = 7,
    WS_XML_NODE_TYPE_EOF         = 8,
    WS_XML_NODE_TYPE_BOF         = 9
} WS_XML_NODE_TYPE;

typedef struct _WS_XML_NODE {
    WS_XML_NODE_TYPE nodeType;
} WS_XML_NODE;

typedef enum {
    WS_XML_TEXT_TYPE_UTF8      = 1,
    WS_XML_TEXT_TYPE_UTF16     = 2,
    WS_XML_TEXT_TYPE_BASE64    = 3,
    WS_XML_TEXT_TYPE_BOOL      = 4,
    WS_XML_TEXT_TYPE_INT32     = 5,
    WS_XML_TEXT_TYPE_INT64     = 6,
    WS_XML_TEXT_TYPE_UINT64    = 7,
    WS_XML_TEXT_TYPE_FLOAT     = 8,
    WS_XML_TEXT_TYPE_DOUBLE    = 9,
    WS_XML_TEXT_TYPE_DECIMAL   = 10,
    WS_XML_TEXT_TYPE_GUID      = 11,
    WS_XML_TEXT_TYPE_UNIQUE_ID = 12,
    WS_XML_TEXT_TYPE_DATETIME  = 13,
    WS_XML_TEXT_TYPE_TIMESPAN  = 14,
    WS_XML_TEXT_TYPE_QNAME     = 15,
    WS_XML_TEXT_TYPE_LIST      = 16
} WS_XML_TEXT_TYPE;

typedef struct _WS_XML_TEXT {
    WS_XML_TEXT_TYPE textType;
} WS_XML_TEXT;

typedef struct _WS_XML_UTF8_TEXT {
    WS_XML_TEXT text;
    WS_XML_STRING value;
} WS_XML_UTF8_TEXT;

typedef struct _WS_XML_ATTRIBUTE {
    BYTE singleQuote;
    BYTE isXmlNs;
    WS_XML_STRING *prefix;
    WS_XML_STRING *localName;
    WS_XML_STRING *ns;
    WS_XML_TEXT *value;
} WS_XML_ATTRIBUTE;

typedef struct _WS_XML_ELEMENT_NODE {
    WS_XML_NODE node;
    WS_XML_STRING *prefix;
    WS_XML_STRING *localName;
    WS_XML_STRING *ns;
    ULONG attributeCount;
    WS_XML_ATTRIBUTE **attributes;
    BOOL isEmpty;
} WS_XML_ELEMENT_NODE;

typedef struct _WS_XML_TEXT_NODE {
    WS_XML_NODE node;
    WS_XML_TEXT *text;
} WS_XML_TEXT_NODE;

typedef struct _WS_XML_COMMENT_NODE {
    WS_XML_NODE node;
    WS_XML_STRING value;
} WS_XML_COMMENT_NODE;

typedef struct _WS_XML_NODE_POSITION {
    WS_XML_BUFFER *buffer;
    void *node;
} WS_XML_NODE_POSITION;

HRESULT WINAPI WsCreateError(const WS_ERROR_PROPERTY*, ULONG, WS_ERROR**);
HRESULT WINAPI WsCreateHeap(SIZE_T, SIZE_T, const WS_HEAP_PROPERTY*, ULONG, WS_HEAP**, WS_ERROR*);
HRESULT WINAPI WsCreateReader(const WS_XML_READER_PROPERTY*, ULONG, WS_XML_READER**, WS_ERROR*);
HRESULT WINAPI WsFillReader(WS_XML_READER*, ULONG, const WS_ASYNC_CONTEXT*, WS_ERROR*);
HRESULT WINAPI WsFindAttribute(WS_XML_READER*, const WS_XML_STRING*, const WS_XML_STRING*, BOOL,
                               ULONG*, WS_ERROR*);
void WINAPI WsFreeError(WS_ERROR*);
void WINAPI WsFreeHeap(WS_HEAP*);
void WINAPI WsFreeReader(WS_XML_READER*);
HRESULT WINAPI WsGetErrorProperty(WS_ERROR*, WS_ERROR_PROPERTY_ID, void*, ULONG);
HRESULT WINAPI WsGetErrorString(WS_ERROR*, ULONG, WS_STRING**);
HRESULT WINAPI WsGetHeapProperty(WS_HEAP*, WS_HEAP_PROPERTY_ID, void*, ULONG, WS_ERROR*);
HRESULT WINAPI WsGetReaderNode(WS_XML_READER*, const WS_XML_NODE**, WS_ERROR*);
HRESULT WINAPI WsGetReaderPosition(WS_XML_READER*, WS_XML_NODE_POSITION*, WS_ERROR*);
HRESULT WINAPI WsGetReaderProperty(WS_XML_READER*, WS_XML_READER_PROPERTY_ID, void*, ULONG, WS_ERROR*);
HRESULT WINAPI WsGetXmlAttribute(WS_XML_READER*, const WS_XML_STRING*, WS_HEAP*, WCHAR**,
                                 ULONG*, WS_ERROR*);
HRESULT WINAPI WsReadAttribute(WS_XML_READER*, const WS_ATTRIBUTE_DESCRIPTION*, WS_READ_OPTION,
                               WS_HEAP*, void*, ULONG, WS_ERROR*);
HRESULT WINAPI WsReadEndElement(WS_XML_READER*, WS_ERROR*);
HRESULT WINAPI WsReadNode(WS_XML_READER*, WS_ERROR*);
HRESULT WINAPI WsReadStartAttribute(WS_XML_READER*, ULONG, WS_ERROR*);
HRESULT WINAPI WsReadStartElement(WS_XML_READER*, WS_ERROR*);
HRESULT WINAPI WsReadToStartElement(WS_XML_READER*, const WS_XML_STRING*, const WS_XML_STRING*,
                                    BOOL*, WS_ERROR*);
HRESULT WINAPI WsReadType(WS_XML_READER*, WS_TYPE_MAPPING, WS_TYPE, const void*, WS_READ_OPTION,
                          WS_HEAP*, void*, ULONG, WS_ERROR*);
HRESULT WINAPI WsSetErrorProperty(WS_ERROR*, WS_ERROR_PROPERTY_ID, const void*, ULONG);
HRESULT WINAPI WsSetInput(WS_XML_READER*, const WS_XML_READER_ENCODING*, const WS_XML_READER_INPUT*,
                          const WS_XML_READER_PROPERTY*, ULONG, WS_ERROR*);


#define WS_S_ASYNC                          0x003d0000
#define WS_S_END                            0x003d0001
#define WS_E_INVALID_FORMAT                 0x803d0000
#define WS_E_OBJECT_FAULTED                 0x803d0001
#define WS_E_NUMERIC_OVERFLOW               0x803d0002
#define WS_E_INVALID_OPERATION              0x803d0003
#define WS_E_OPERATION_ABORTED              0x803d0004
#define WS_E_ENDPOINT_ACCESS_DENIED         0x803d0005
#define WS_E_OPERATION_TIMED_OUT            0x803d0006
#define WS_E_OPERATION_ABANDONED            0x803d0007
#define WS_E_QUOTA_EXCEEDED                 0x803d0008
#define WS_E_NO_TRANSLATION_AVAILABLE       0x803d0009
#define WS_E_SECURITY_VERIFICATION_FAILURE  0x803d000a
#define WS_E_ADDRESS_IN_USE                 0x803d000b
#define WS_E_ADDRESS_NOT_AVAILABLE          0x803d000c
#define WS_E_ENDPOINT_NOT_FOUND             0x803d000d
#define WS_E_ENDPOINT_NOT_AVAILABLE         0x803d000e
#define WS_E_ENDPOINT_FAILURE               0x803d000f
#define WS_E_ENDPOINT_UNREACHABLE           0x803d0010
#define WS_E_ENDPOINT_ACTION_NOT_SUPPORTED  0x803d0011
#define WS_E_ENDPOINT_TOO_BUSY              0x803d0012
#define WS_E_ENDPOINT_FAULT_RECEIVED        0x803d0013
#define WS_E_ENDPOINT_DISCONNECTED          0x803d0014
#define WS_E_PROXY_FAILURE                  0x803d0015
#define WS_E_PROXY_ACCESS_DENIED            0x803d0016
#define WS_E_NOT_SUPPORTED                  0x803d0017
#define WS_E_PROXY_REQUIRES_BASIC_AUTH      0x803d0018
#define WS_E_PROXY_REQUIRES_DIGEST_AUTH     0x803d0019
#define WS_E_PROXY_REQUIRES_NTLM_AUTH       0x803d001a
#define WS_E_PROXY_REQUIRES_NEGOTIATE_AUTH  0x803d001b
#define WS_E_SERVER_REQUIRES_BASIC_AUTH     0x803d001c
#define WS_E_SERVER_REQUIRES_DIGEST_AUTH    0x803d001d
#define WS_E_SERVER_REQUIRES_NTLM_AUTH      0x803d001e
#define WS_E_SERVER_REQUIRES_NEGOTIATE_AUTH 0x803d001f
#define WS_E_INVALID_ENDPOINT_URL           0x803d0020
#define WS_E_OTHER                          0x803d0021
#define WS_E_SECURITY_TOKEN_EXPIRED         0x803d0022
#define WS_E_SECURITY_SYSTEM_FAILURE        0x803d0023

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* __WINE_WEBSERVICES_H */

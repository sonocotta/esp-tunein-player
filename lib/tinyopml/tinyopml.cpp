/*
Original code by Lee Thomason (www.grinninglizard.com)

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

#include "tinyopml.h"

#include <new>		// yes, this one new style header, is in the Android SDK.
#if defined(ANDROID_NDK) || defined(__BORLANDC__) || defined(__QNXNTO__)
#   include <stddef.h>
#   include <stdarg.h>
#else
#   include <cstddef>
#   include <cstdarg>
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1400 ) && (!defined WINCE)
	// Microsoft Visual Studio, version 2005 and higher. Not WinCE.
	/*int _snprintf_s(
	   char *buffer,
	   size_t sizeOfBuffer,
	   size_t count,
	   const char *format [,
		  argument] ...
	);*/
	static inline int TIOPML_SNPRINTF( char* buffer, size_t size, const char* format, ... )
	{
		va_list va;
		va_start( va, format );
		const int result = vsnprintf_s( buffer, size, _TRUNCATE, format, va );
		va_end( va );
		return result;
	}

	static inline int TIOPML_VSNPRINTF( char* buffer, size_t size, const char* format, va_list va )
	{
		const int result = vsnprintf_s( buffer, size, _TRUNCATE, format, va );
		return result;
	}

	#define TIOPML_VSCPRINTF	_vscprintf
	#define TIOPML_SSCANF	sscanf_s
#elif defined _MSC_VER
	// Microsoft Visual Studio 2003 and earlier or WinCE
	#define TIOPML_SNPRINTF	_snprintf
	#define TIOPML_VSNPRINTF _vsnprintf
	#define TIOPML_SSCANF	sscanf
	#if (_MSC_VER < 1400 ) && (!defined WINCE)
		// Microsoft Visual Studio 2003 and not WinCE.
		#define TIOPML_VSCPRINTF   _vscprintf // VS2003's C runtime has this, but VC6 C runtime or WinCE SDK doesn't have.
	#else
		// Microsoft Visual Studio 2003 and earlier or WinCE.
		static inline int TIOPML_VSCPRINTF( const char* format, va_list va )
		{
			int len = 512;
			for (;;) {
				len = len*2;
				char* str = new char[len]();
				const int required = _vsnprintf(str, len, format, va);
				delete[] str;
				if ( required != -1 ) {
					TIOPMLASSERT( required >= 0 );
					len = required;
					break;
				}
			}
			TIOPMLASSERT( len >= 0 );
			return len;
		}
	#endif
#else
	// GCC version 3 and higher
	//#warning( "Using sn* functions." )
	#define TIOPML_SNPRINTF	snprintf
	#define TIOPML_VSNPRINTF	vsnprintf
	static inline int TIOPML_VSCPRINTF( const char* format, va_list va )
	{
		int len = vsnprintf( 0, 0, format, va );
		TIOPMLASSERT( len >= 0 );
		return len;
	}
	#define TIOPML_SSCANF   sscanf
#endif

#if defined(_WIN64)
	#define TIOPML_FSEEK _fseeki64
	#define TIOPML_FTELL _ftelli64
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) \
	|| defined(__NetBSD__) || defined(__DragonFly__) || defined(__ANDROID__)
	#define TIOPML_FSEEK fseeko
	#define TIOPML_FTELL ftello
#elif defined(__unix__) && defined(__x86_64__)
	#define TIOPML_FSEEK fseeko64
	#define TIOPML_FTELL ftello64
#else
	#define TIOPML_FSEEK fseek
	#define TIOPML_FTELL ftell
#endif


static const char LINE_FEED				= static_cast<char>(0x0a);			// all line endings are normalized to LF
static const char LF = LINE_FEED;
static const char CARRIAGE_RETURN		= static_cast<char>(0x0d);			// CR gets filtered out
static const char CR = CARRIAGE_RETURN;
static const char SINGLE_QUOTE			= '\'';
static const char DOUBLE_QUOTE			= '\"';

// Bunch of unicode info at:
//		http://www.unicode.org/faq/utf_bom.html
//	ef bb bf (Microsoft "lead bytes") - designates UTF-8

static const unsigned char TIOPML_UTF_LEAD_0 = 0xefU;
static const unsigned char TIOPML_UTF_LEAD_1 = 0xbbU;
static const unsigned char TIOPML_UTF_LEAD_2 = 0xbfU;

namespace tinyopml
{

struct Entity {
    const char* pattern;
    int length;
    char value;
};

static const int NUM_ENTITIES = 5;
static const Entity entities[NUM_ENTITIES] = {
    { "quot", 4,	DOUBLE_QUOTE },
    { "amp", 3,		'&'  },
    { "apos", 4,	SINGLE_QUOTE },
    { "lt",	2, 		'<'	 },
    { "gt",	2,		'>'	 }
};


StrPair::~StrPair()
{
    Reset();
}


void StrPair::TransferTo( StrPair* other )
{
    if ( this == other ) {
        return;
    }
    // This in effect implements the assignment operator by "moving"
    // ownership (as in auto_ptr).

    TIOPMLASSERT( other != 0 );
    TIOPMLASSERT( other->_flags == 0 );
    TIOPMLASSERT( other->_start == 0 );
    TIOPMLASSERT( other->_end == 0 );

    other->Reset();

    other->_flags = _flags;
    other->_start = _start;
    other->_end = _end;

    _flags = 0;
    _start = 0;
    _end = 0;
}


void StrPair::Reset()
{
    if ( _flags & NEEDS_DELETE ) {
        delete [] _start;
    }
    _flags = 0;
    _start = 0;
    _end = 0;
}


void StrPair::SetStr( const char* str, int flags )
{
    TIOPMLASSERT( str );
    Reset();
    size_t len = strlen( str );
    TIOPMLASSERT( _start == 0 );
    _start = new char[ len+1 ];
    memcpy( _start, str, len+1 );
    _end = _start + len;
    _flags = flags | NEEDS_DELETE;
}


char* StrPair::ParseText( char* p, const char* endTag, int strFlags, int* curLineNumPtr )
{
    TIOPMLASSERT( p );
    TIOPMLASSERT( endTag && *endTag );
	TIOPMLASSERT(curLineNumPtr);

    char* start = p;
    const char  endChar = *endTag;
    size_t length = strlen( endTag );

    // Inner loop of text parsing.
    while ( *p ) {
        if ( *p == endChar && strncmp( p, endTag, length ) == 0 ) {
            Set( start, p, strFlags );
            return p + length;
        } else if (*p == '\n') {
            ++(*curLineNumPtr);
        }
        ++p;
        TIOPMLASSERT( p );
    }
    return 0;
}


char* StrPair::ParseName( char* p )
{
    if ( !p || !(*p) ) {
        return 0;
    }
    if ( !OPMLUtil::IsNameStartChar( (unsigned char) *p ) ) {
        return 0;
    }

    char* const start = p;
    ++p;
    while ( *p && OPMLUtil::IsNameChar( (unsigned char) *p ) ) {
        ++p;
    }

    Set( start, p, 0 );
    return p;
}


void StrPair::CollapseWhitespace()
{
    // Adjusting _start would cause undefined behavior on delete[]
    TIOPMLASSERT( ( _flags & NEEDS_DELETE ) == 0 );
    // Trim leading space.
    _start = OPMLUtil::SkipWhiteSpace( _start, 0 );

    if ( *_start ) {
        const char* p = _start;	// the read pointer
        char* q = _start;	// the write pointer

        while( *p ) {
            if ( OPMLUtil::IsWhiteSpace( *p )) {
                p = OPMLUtil::SkipWhiteSpace( p, 0 );
                if ( *p == 0 ) {
                    break;    // don't write to q; this trims the trailing space.
                }
                *q = ' ';
                ++q;
            }
            *q = *p;
            ++q;
            ++p;
        }
        *q = 0;
    }
}


const char* StrPair::GetStr()
{
    TIOPMLASSERT( _start );
    TIOPMLASSERT( _end );
    if ( _flags & NEEDS_FLUSH ) {
        *_end = 0;
        _flags ^= NEEDS_FLUSH;

        if ( _flags ) {
            const char* p = _start;	// the read pointer
            char* q = _start;	// the write pointer

            while( p < _end ) {
                if ( (_flags & NEEDS_NEWLINE_NORMALIZATION) && *p == CR ) {
                    // CR-LF pair becomes LF
                    // CR alone becomes LF
                    // LF-CR becomes LF
                    if ( *(p+1) == LF ) {
                        p += 2;
                    }
                    else {
                        ++p;
                    }
                    *q = LF;
                    ++q;
                }
                else if ( (_flags & NEEDS_NEWLINE_NORMALIZATION) && *p == LF ) {
                    if ( *(p+1) == CR ) {
                        p += 2;
                    }
                    else {
                        ++p;
                    }
                    *q = LF;
                    ++q;
                }
                else if ( (_flags & NEEDS_ENTITY_PROCESSING) && *p == '&' ) {
                    // Entities handled by tinyOPML2:
                    // - special entities in the entity table [in/out]
                    // - numeric character reference [in]
                    //   &#20013; or &#x4e2d;

                    if ( *(p+1) == '#' ) {
                        const int buflen = 10;
                        char buf[buflen] = { 0 };
                        int len = 0;
                        const char* adjusted = const_cast<char*>( OPMLUtil::GetCharacterRef( p, buf, &len ) );
                        if ( adjusted == 0 ) {
                            *q = *p;
                            ++p;
                            ++q;
                        }
                        else {
                            TIOPMLASSERT( 0 <= len && len <= buflen );
                            TIOPMLASSERT( q + len <= adjusted );
                            p = adjusted;
                            memcpy( q, buf, len );
                            q += len;
                        }
                    }
                    else {
                        bool entityFound = false;
                        for( int i = 0; i < NUM_ENTITIES; ++i ) {
                            const Entity& entity = entities[i];
                            if ( strncmp( p + 1, entity.pattern, entity.length ) == 0
                                    && *( p + entity.length + 1 ) == ';' ) {
                                // Found an entity - convert.
                                *q = entity.value;
                                ++q;
                                p += entity.length + 2;
                                entityFound = true;
                                break;
                            }
                        }
                        if ( !entityFound ) {
                            // fixme: treat as error?
                            ++p;
                            ++q;
                        }
                    }
                }
                else {
                    *q = *p;
                    ++p;
                    ++q;
                }
            }
            *q = 0;
        }
        // The loop below has plenty going on, and this
        // is a less useful mode. Break it out.
        if ( _flags & NEEDS_WHITESPACE_COLLAPSING ) {
            CollapseWhitespace();
        }
        _flags = (_flags & NEEDS_DELETE);
    }
    TIOPMLASSERT( _start );
    return _start;
}




// --------- OPMLUtil ----------- //

const char* OPMLUtil::writeBoolTrue  = "true";
const char* OPMLUtil::writeBoolFalse = "false";

void OPMLUtil::SetBoolSerialization(const char* writeTrue, const char* writeFalse)
{
	static const char* defTrue  = "true";
	static const char* defFalse = "false";

	writeBoolTrue = (writeTrue) ? writeTrue : defTrue;
	writeBoolFalse = (writeFalse) ? writeFalse : defFalse;
}


const char* OPMLUtil::ReadBOM( const char* p, bool* bom )
{
    TIOPMLASSERT( p );
    TIOPMLASSERT( bom );
    *bom = false;
    const unsigned char* pu = reinterpret_cast<const unsigned char*>(p);
    // Check for BOM:
    if (    *(pu+0) == TIOPML_UTF_LEAD_0
            && *(pu+1) == TIOPML_UTF_LEAD_1
            && *(pu+2) == TIOPML_UTF_LEAD_2 ) {
        *bom = true;
        p += 3;
    }
    TIOPMLASSERT( p );
    return p;
}


void OPMLUtil::ConvertUTF32ToUTF8( unsigned long input, char* output, int* length )
{
    const unsigned long BYTE_MASK = 0xBF;
    const unsigned long BYTE_MARK = 0x80;
    const unsigned long FIRST_BYTE_MARK[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

    if (input < 0x80) {
        *length = 1;
    }
    else if ( input < 0x800 ) {
        *length = 2;
    }
    else if ( input < 0x10000 ) {
        *length = 3;
    }
    else if ( input < 0x200000 ) {
        *length = 4;
    }
    else {
        *length = 0;    // This code won't convert this correctly anyway.
        return;
    }

    output += *length;

    // Scary scary fall throughs are annotated with carefully designed comments
    // to suppress compiler warnings such as -Wimplicit-fallthrough in gcc
    switch (*length) {
        case 4:
            --output;
            *output = static_cast<char>((input | BYTE_MARK) & BYTE_MASK);
            input >>= 6;
            //fall through
        case 3:
            --output;
            *output = static_cast<char>((input | BYTE_MARK) & BYTE_MASK);
            input >>= 6;
            //fall through
        case 2:
            --output;
            *output = static_cast<char>((input | BYTE_MARK) & BYTE_MASK);
            input >>= 6;
            //fall through
        case 1:
            --output;
            *output = static_cast<char>(input | FIRST_BYTE_MARK[*length]);
            break;
        default:
            TIOPMLASSERT( false );
    }
}


const char* OPMLUtil::GetCharacterRef( const char* p, char* value, int* length )
{
    // Presume an entity, and pull it out.
    *length = 0;

    if ( *(p+1) == '#' && *(p+2) ) {
        unsigned long ucs = 0;
        TIOPMLASSERT( sizeof( ucs ) >= 4 );
        ptrdiff_t delta = 0;
        unsigned mult = 1;
        static const char SEMICOLON = ';';

        if ( *(p+2) == 'x' ) {
            // Hexadecimal.
            const char* q = p+3;
            if ( !(*q) ) {
                return 0;
            }

            q = strchr( q, SEMICOLON );

            if ( !q ) {
                return 0;
            }
            TIOPMLASSERT( *q == SEMICOLON );

            delta = q-p;
            --q;

            while ( *q != 'x' ) {
                unsigned int digit = 0;

                if ( *q >= '0' && *q <= '9' ) {
                    digit = *q - '0';
                }
                else if ( *q >= 'a' && *q <= 'f' ) {
                    digit = *q - 'a' + 10;
                }
                else if ( *q >= 'A' && *q <= 'F' ) {
                    digit = *q - 'A' + 10;
                }
                else {
                    return 0;
                }
                TIOPMLASSERT( digit < 16 );
                TIOPMLASSERT( digit == 0 || mult <= UINT_MAX / digit );
                const unsigned int digitScaled = mult * digit;
                TIOPMLASSERT( ucs <= ULONG_MAX - digitScaled );
                ucs += digitScaled;
                TIOPMLASSERT( mult <= UINT_MAX / 16 );
                mult *= 16;
                --q;
            }
        }
        else {
            // Decimal.
            const char* q = p+2;
            if ( !(*q) ) {
                return 0;
            }

            q = strchr( q, SEMICOLON );

            if ( !q ) {
                return 0;
            }
            TIOPMLASSERT( *q == SEMICOLON );

            delta = q-p;
            --q;

            while ( *q != '#' ) {
                if ( *q >= '0' && *q <= '9' ) {
                    const unsigned int digit = *q - '0';
                    TIOPMLASSERT( digit < 10 );
                    TIOPMLASSERT( digit == 0 || mult <= UINT_MAX / digit );
                    const unsigned int digitScaled = mult * digit;
                    TIOPMLASSERT( ucs <= ULONG_MAX - digitScaled );
                    ucs += digitScaled;
                }
                else {
                    return 0;
                }
                TIOPMLASSERT( mult <= UINT_MAX / 10 );
                mult *= 10;
                --q;
            }
        }
        // convert the UCS to UTF-8
        ConvertUTF32ToUTF8( ucs, value, length );
        return p + delta + 1;
    }
    return p+1;
}


void OPMLUtil::ToStr( int v, char* buffer, int bufferSize )
{
    TIOPML_SNPRINTF( buffer, bufferSize, "%d", v );
}


void OPMLUtil::ToStr( unsigned v, char* buffer, int bufferSize )
{
    TIOPML_SNPRINTF( buffer, bufferSize, "%u", v );
}


void OPMLUtil::ToStr( bool v, char* buffer, int bufferSize )
{
    TIOPML_SNPRINTF( buffer, bufferSize, "%s", v ? writeBoolTrue : writeBoolFalse);
}

/*
	ToStr() of a number is a very tricky topic.
	https://github.com/leethomason/tinyopml2/issues/106
*/
void OPMLUtil::ToStr( float v, char* buffer, int bufferSize )
{
    TIOPML_SNPRINTF( buffer, bufferSize, "%.8g", v );
}


void OPMLUtil::ToStr( double v, char* buffer, int bufferSize )
{
    TIOPML_SNPRINTF( buffer, bufferSize, "%.17g", v );
}


void OPMLUtil::ToStr( int64_t v, char* buffer, int bufferSize )
{
	// horrible syntax trick to make the compiler happy about %lld
	TIOPML_SNPRINTF(buffer, bufferSize, "%lld", static_cast<long long>(v));
}

void OPMLUtil::ToStr( uint64_t v, char* buffer, int bufferSize )
{
    // horrible syntax trick to make the compiler happy about %llu
    TIOPML_SNPRINTF(buffer, bufferSize, "%llu", (long long)v);
}

bool OPMLUtil::ToInt(const char* str, int* value)
{
    if (IsPrefixHex(str)) {
        unsigned v;
        if (TIOPML_SSCANF(str, "%x", &v) == 1) {
            *value = static_cast<int>(v);
            return true;
        }
    }
    else {
        if (TIOPML_SSCANF(str, "%d", value) == 1) {
            return true;
        }
    }
    return false;
}

bool OPMLUtil::ToUnsigned(const char* str, unsigned* value)
{
    if (TIOPML_SSCANF(str, IsPrefixHex(str) ? "%x" : "%u", value) == 1) {
        return true;
    }
    return false;
}

bool OPMLUtil::ToBool( const char* str, bool* value )
{
    int ival = 0;
    if ( ToInt( str, &ival )) {
        *value = (ival==0) ? false : true;
        return true;
    }
    static const char* TRUE_VALS[] = { "true", "True", "TRUE", 0 };
    static const char* FALSE_VALS[] = { "false", "False", "FALSE", 0 };

    for (int i = 0; TRUE_VALS[i]; ++i) {
        if (StringEqual(str, TRUE_VALS[i])) {
            *value = true;
            return true;
        }
    }
    for (int i = 0; FALSE_VALS[i]; ++i) {
        if (StringEqual(str, FALSE_VALS[i])) {
            *value = false;
            return true;
        }
    }
    return false;
}


bool OPMLUtil::ToFloat( const char* str, float* value )
{
    if ( TIOPML_SSCANF( str, "%f", value ) == 1 ) {
        return true;
    }
    return false;
}


bool OPMLUtil::ToDouble( const char* str, double* value )
{
    if ( TIOPML_SSCANF( str, "%lf", value ) == 1 ) {
        return true;
    }
    return false;
}


bool OPMLUtil::ToInt64(const char* str, int64_t* value)
{
    if (IsPrefixHex(str)) {
        unsigned long long v = 0;	// horrible syntax trick to make the compiler happy about %llx
        if (TIOPML_SSCANF(str, "%llx", &v) == 1) {
            *value = static_cast<int64_t>(v);
            return true;
        }
    }
    else {
        long long v = 0;	// horrible syntax trick to make the compiler happy about %lld
        if (TIOPML_SSCANF(str, "%lld", &v) == 1) {
            *value = static_cast<int64_t>(v);
            return true;
        }
    }
	return false;
}


bool OPMLUtil::ToUnsigned64(const char* str, uint64_t* value) {
    unsigned long long v = 0;	// horrible syntax trick to make the compiler happy about %llu
    if(TIOPML_SSCANF(str, IsPrefixHex(str) ? "%llx" : "%llu", &v) == 1) {
        *value = (uint64_t)v;
        return true;
    }
    return false;
}


char* OPMLDocument::Identify( char* p, OPMLNode** node )
{
    TIOPMLASSERT( node );
    TIOPMLASSERT( p );
    char* const start = p;
    int const startLine = _parseCurLineNum;
    p = OPMLUtil::SkipWhiteSpace( p, &_parseCurLineNum );
    if( !*p ) {
        *node = 0;
        TIOPMLASSERT( p );
        return p;
    }

    // These strings define the matching patterns:
    static const char* opmlHeader		= { "<?" };
    static const char* commentHeader	= { "<!--" };
    static const char* cdataHeader		= { "<![CDATA[" };
    static const char* dtdHeader		= { "<!" };
    static const char* elementHeader	= { "<" };	// and a header for everything else; check last.

    static const int opmlHeaderLen		= 2;
    static const int commentHeaderLen	= 4;
    static const int cdataHeaderLen		= 9;
    static const int dtdHeaderLen		= 2;
    static const int elementHeaderLen	= 1;

    TIOPMLASSERT( sizeof( OPMLComment ) == sizeof( OPMLUnknown ) );		// use same memory pool
    TIOPMLASSERT( sizeof( OPMLComment ) == sizeof( OPMLDeclaration ) );	// use same memory pool
    OPMLNode* returnNode = 0;
    if ( OPMLUtil::StringEqual( p, opmlHeader, opmlHeaderLen ) ) {
        returnNode = CreateUnlinkedNode<OPMLDeclaration>( _commentPool );
        returnNode->_parseLineNum = _parseCurLineNum;
        p += opmlHeaderLen;
    }
    else if ( OPMLUtil::StringEqual( p, commentHeader, commentHeaderLen ) ) {
        returnNode = CreateUnlinkedNode<OPMLComment>( _commentPool );
        returnNode->_parseLineNum = _parseCurLineNum;
        p += commentHeaderLen;
    }
    else if ( OPMLUtil::StringEqual( p, cdataHeader, cdataHeaderLen ) ) {
        OPMLText* text = CreateUnlinkedNode<OPMLText>( _textPool );
        returnNode = text;
        returnNode->_parseLineNum = _parseCurLineNum;
        p += cdataHeaderLen;
        text->SetCData( true );
    }
    else if ( OPMLUtil::StringEqual( p, dtdHeader, dtdHeaderLen ) ) {
        returnNode = CreateUnlinkedNode<OPMLUnknown>( _commentPool );
        returnNode->_parseLineNum = _parseCurLineNum;
        p += dtdHeaderLen;
    }
    else if ( OPMLUtil::StringEqual( p, elementHeader, elementHeaderLen ) ) {
        returnNode =  CreateUnlinkedNode<OPMLElement>( _elementPool );
        returnNode->_parseLineNum = _parseCurLineNum;
        p += elementHeaderLen;
    }
    else {
        returnNode = CreateUnlinkedNode<OPMLText>( _textPool );
        returnNode->_parseLineNum = _parseCurLineNum; // Report line of first non-whitespace character
        p = start;	// Back it up, all the text counts.
        _parseCurLineNum = startLine;
    }

    TIOPMLASSERT( returnNode );
    TIOPMLASSERT( p );
    *node = returnNode;
    return p;
}


bool OPMLDocument::Accept( OPMLVisitor* visitor ) const
{
    TIOPMLASSERT( visitor );
    if ( visitor->VisitEnter( *this ) ) {
        for ( const OPMLNode* node=FirstChild(); node; node=node->NextSibling() ) {
            if ( !node->Accept( visitor ) ) {
                break;
            }
        }
    }
    return visitor->VisitExit( *this );
}


// --------- OPMLNode ----------- //

OPMLNode::OPMLNode( OPMLDocument* doc ) :
    _document( doc ),
    _parent( 0 ),
    _value(),
    _parseLineNum( 0 ),
    _firstChild( 0 ), _lastChild( 0 ),
    _prev( 0 ), _next( 0 ),
	_userData( 0 ),
    _memPool( 0 )
{
}


OPMLNode::~OPMLNode()
{
    DeleteChildren();
    if ( _parent ) {
        _parent->Unlink( this );
    }
}

const char* OPMLNode::Value() const
{
    // Edge case: OPMLDocuments don't have a Value. Return null.
    if ( this->ToDocument() )
        return 0;
    return _value.GetStr();
}

void OPMLNode::SetValue( const char* str, bool staticMem )
{
    if ( staticMem ) {
        _value.SetInternedStr( str );
    }
    else {
        _value.SetStr( str );
    }
}

OPMLNode* OPMLNode::DeepClone(OPMLDocument* target) const
{
	OPMLNode* clone = this->ShallowClone(target);
	if (!clone) return 0;

	for (const OPMLNode* child = this->FirstChild(); child; child = child->NextSibling()) {
		OPMLNode* childClone = child->DeepClone(target);
		TIOPMLASSERT(childClone);
		clone->InsertEndChild(childClone);
	}
	return clone;
}

void OPMLNode::DeleteChildren()
{
    while( _firstChild ) {
        TIOPMLASSERT( _lastChild );
        DeleteChild( _firstChild );
    }
    _firstChild = _lastChild = 0;
}


void OPMLNode::Unlink( OPMLNode* child )
{
    TIOPMLASSERT( child );
    TIOPMLASSERT( child->_document == _document );
    TIOPMLASSERT( child->_parent == this );
    if ( child == _firstChild ) {
        _firstChild = _firstChild->_next;
    }
    if ( child == _lastChild ) {
        _lastChild = _lastChild->_prev;
    }

    if ( child->_prev ) {
        child->_prev->_next = child->_next;
    }
    if ( child->_next ) {
        child->_next->_prev = child->_prev;
    }
	child->_next = 0;
	child->_prev = 0;
	child->_parent = 0;
}


void OPMLNode::DeleteChild( OPMLNode* node )
{
    TIOPMLASSERT( node );
    TIOPMLASSERT( node->_document == _document );
    TIOPMLASSERT( node->_parent == this );
    Unlink( node );
	TIOPMLASSERT(node->_prev == 0);
	TIOPMLASSERT(node->_next == 0);
	TIOPMLASSERT(node->_parent == 0);
    DeleteNode( node );
}


OPMLNode* OPMLNode::InsertEndChild( OPMLNode* addThis )
{
    TIOPMLASSERT( addThis );
    if ( addThis->_document != _document ) {
        TIOPMLASSERT( false );
        return 0;
    }
    InsertChildPreamble( addThis );

    if ( _lastChild ) {
        TIOPMLASSERT( _firstChild );
        TIOPMLASSERT( _lastChild->_next == 0 );
        _lastChild->_next = addThis;
        addThis->_prev = _lastChild;
        _lastChild = addThis;

        addThis->_next = 0;
    }
    else {
        TIOPMLASSERT( _firstChild == 0 );
        _firstChild = _lastChild = addThis;

        addThis->_prev = 0;
        addThis->_next = 0;
    }
    addThis->_parent = this;
    return addThis;
}


OPMLNode* OPMLNode::InsertFirstChild( OPMLNode* addThis )
{
    TIOPMLASSERT( addThis );
    if ( addThis->_document != _document ) {
        TIOPMLASSERT( false );
        return 0;
    }
    InsertChildPreamble( addThis );

    if ( _firstChild ) {
        TIOPMLASSERT( _lastChild );
        TIOPMLASSERT( _firstChild->_prev == 0 );

        _firstChild->_prev = addThis;
        addThis->_next = _firstChild;
        _firstChild = addThis;

        addThis->_prev = 0;
    }
    else {
        TIOPMLASSERT( _lastChild == 0 );
        _firstChild = _lastChild = addThis;

        addThis->_prev = 0;
        addThis->_next = 0;
    }
    addThis->_parent = this;
    return addThis;
}


OPMLNode* OPMLNode::InsertAfterChild( OPMLNode* afterThis, OPMLNode* addThis )
{
    TIOPMLASSERT( addThis );
    if ( addThis->_document != _document ) {
        TIOPMLASSERT( false );
        return 0;
    }

    TIOPMLASSERT( afterThis );

    if ( afterThis->_parent != this ) {
        TIOPMLASSERT( false );
        return 0;
    }
    if ( afterThis == addThis ) {
        // Current state: BeforeThis -> AddThis -> OneAfterAddThis
        // Now AddThis must disappear from it's location and then
        // reappear between BeforeThis and OneAfterAddThis.
        // So just leave it where it is.
        return addThis;
    }

    if ( afterThis->_next == 0 ) {
        // The last node or the only node.
        return InsertEndChild( addThis );
    }
    InsertChildPreamble( addThis );
    addThis->_prev = afterThis;
    addThis->_next = afterThis->_next;
    afterThis->_next->_prev = addThis;
    afterThis->_next = addThis;
    addThis->_parent = this;
    return addThis;
}




const OPMLElement* OPMLNode::FirstChildElement( const char* name ) const
{
    for( const OPMLNode* node = _firstChild; node; node = node->_next ) {
        const OPMLElement* element = node->ToElementWithName( name );
        if ( element ) {
            return element;
        }
    }
    return 0;
}


const OPMLElement* OPMLNode::LastChildElement( const char* name ) const
{
    for( const OPMLNode* node = _lastChild; node; node = node->_prev ) {
        const OPMLElement* element = node->ToElementWithName( name );
        if ( element ) {
            return element;
        }
    }
    return 0;
}


const OPMLElement* OPMLNode::NextSiblingElement( const char* name ) const
{
    for( const OPMLNode* node = _next; node; node = node->_next ) {
        const OPMLElement* element = node->ToElementWithName( name );
        if ( element ) {
            return element;
        }
    }
    return 0;
}


const OPMLElement* OPMLNode::PreviousSiblingElement( const char* name ) const
{
    for( const OPMLNode* node = _prev; node; node = node->_prev ) {
        const OPMLElement* element = node->ToElementWithName( name );
        if ( element ) {
            return element;
        }
    }
    return 0;
}


char* OPMLNode::ParseDeep( char* p, StrPair* parentEndTag, int* curLineNumPtr )
{
    // This is a recursive method, but thinking about it "at the current level"
    // it is a pretty simple flat list:
    //		<foo/>
    //		<!-- comment -->
    //
    // With a special case:
    //		<foo>
    //		</foo>
    //		<!-- comment -->
    //
    // Where the closing element (/foo) *must* be the next thing after the opening
    // element, and the names must match. BUT the tricky bit is that the closing
    // element will be read by the child.
    //
    // 'endTag' is the end tag for this node, it is returned by a call to a child.
    // 'parentEnd' is the end tag for the parent, which is filled in and returned.

	OPMLDocument::DepthTracker tracker(_document);
	if (_document->Error())
		return 0;

	while( p && *p ) {
        OPMLNode* node = 0;

        p = _document->Identify( p, &node );
        TIOPMLASSERT( p );
        if ( node == 0 ) {
            break;
        }

       const int initialLineNum = node->_parseLineNum;

        StrPair endTag;
        p = node->ParseDeep( p, &endTag, curLineNumPtr );
        if ( !p ) {
            DeleteNode( node );
            if ( !_document->Error() ) {
                _document->SetError( OPML_ERROR_PARSING, initialLineNum, 0);
            }
            break;
        }

        const OPMLDeclaration* const decl = node->ToDeclaration();
        if ( decl ) {
            // Declarations are only allowed at document level
            //
            // Multiple declarations are allowed but all declarations
            // must occur before anything else. 
            //
            // Optimized due to a security test case. If the first node is 
            // a declaration, and the last node is a declaration, then only 
            // declarations have so far been added.
            bool wellLocated = false;

            if (ToDocument()) {
                if (FirstChild()) {
                    wellLocated =
                        FirstChild() &&
                        FirstChild()->ToDeclaration() &&
                        LastChild() &&
                        LastChild()->ToDeclaration();
                }
                else {
                    wellLocated = true;
                }
            }
            if ( !wellLocated ) {
                _document->SetError( OPML_ERROR_PARSING_DECLARATION, initialLineNum, "OPMLDeclaration value=%s", decl->Value());
                DeleteNode( node );
                break;
            }
        }

        OPMLElement* ele = node->ToElement();
        if ( ele ) {
            // We read the end tag. Return it to the parent.
            if ( ele->ClosingType() == OPMLElement::CLOSING ) {
                if ( parentEndTag ) {
                    ele->_value.TransferTo( parentEndTag );
                }
                node->_memPool->SetTracked();   // created and then immediately deleted.
                DeleteNode( node );
                return p;
            }

            // Handle an end tag returned to this level.
            // And handle a bunch of annoying errors.
            bool mismatch = false;
            if ( endTag.Empty() ) {
                if ( ele->ClosingType() == OPMLElement::OPEN ) {
                    mismatch = true;
                }
            }
            else {
                if ( ele->ClosingType() != OPMLElement::OPEN ) {
                    mismatch = true;
                }
                else if ( !OPMLUtil::StringEqual( endTag.GetStr(), ele->Name() ) ) {
                    mismatch = true;
                }
            }
            if ( mismatch ) {
                _document->SetError( OPML_ERROR_MISMATCHED_ELEMENT, initialLineNum, "OPMLElement name=%s", ele->Name());
                DeleteNode( node );
                break;
            }
        }
        InsertEndChild( node );
    }
    return 0;
}

/*static*/ void OPMLNode::DeleteNode( OPMLNode* node )
{
    if ( node == 0 ) {
        return;
    }
	TIOPMLASSERT(node->_document);
	if (!node->ToDocument()) {
		node->_document->MarkInUse(node);
	}

    MemPool* pool = node->_memPool;
    node->~OPMLNode();
    pool->Free( node );
}

void OPMLNode::InsertChildPreamble( OPMLNode* insertThis ) const
{
    TIOPMLASSERT( insertThis );
    TIOPMLASSERT( insertThis->_document == _document );

	if (insertThis->_parent) {
        insertThis->_parent->Unlink( insertThis );
	}
	else {
		insertThis->_document->MarkInUse(insertThis);
        insertThis->_memPool->SetTracked();
	}
}

const OPMLElement* OPMLNode::ToElementWithName( const char* name ) const
{
    const OPMLElement* element = this->ToElement();
    if ( element == 0 ) {
        return 0;
    }
    if ( name == 0 ) {
        return element;
    }
    if ( OPMLUtil::StringEqual( element->Name(), name ) ) {
       return element;
    }
    return 0;
}

// --------- OPMLText ---------- //
char* OPMLText::ParseDeep( char* p, StrPair*, int* curLineNumPtr )
{
    if ( this->CData() ) {
        p = _value.ParseText( p, "]]>", StrPair::NEEDS_NEWLINE_NORMALIZATION, curLineNumPtr );
        if ( !p ) {
            _document->SetError( OPML_ERROR_PARSING_CDATA, _parseLineNum, 0 );
        }
        return p;
    }
    else {
        int flags = _document->ProcessEntities() ? StrPair::TEXT_ELEMENT : StrPair::TEXT_ELEMENT_LEAVE_ENTITIES;
        if ( _document->WhitespaceMode() == COLLAPSE_WHITESPACE ) {
            flags |= StrPair::NEEDS_WHITESPACE_COLLAPSING;
        }

        p = _value.ParseText( p, "<", flags, curLineNumPtr );
        if ( p && *p ) {
            return p-1;
        }
        if ( !p ) {
            _document->SetError( OPML_ERROR_PARSING_TEXT, _parseLineNum, 0 );
        }
    }
    return 0;
}


OPMLNode* OPMLText::ShallowClone( OPMLDocument* doc ) const
{
    if ( !doc ) {
        doc = _document;
    }
    OPMLText* text = doc->NewText( Value() );	// fixme: this will always allocate memory. Intern?
    text->SetCData( this->CData() );
    return text;
}


bool OPMLText::ShallowEqual( const OPMLNode* compare ) const
{
    TIOPMLASSERT( compare );
    const OPMLText* text = compare->ToText();
    return ( text && OPMLUtil::StringEqual( text->Value(), Value() ) );
}


bool OPMLText::Accept( OPMLVisitor* visitor ) const
{
    TIOPMLASSERT( visitor );
    return visitor->Visit( *this );
}


// --------- OPMLComment ---------- //

OPMLComment::OPMLComment( OPMLDocument* doc ) : OPMLNode( doc )
{
}


OPMLComment::~OPMLComment()
{
}


char* OPMLComment::ParseDeep( char* p, StrPair*, int* curLineNumPtr )
{
    // Comment parses as text.
    p = _value.ParseText( p, "-->", StrPair::COMMENT, curLineNumPtr );
    if ( p == 0 ) {
        _document->SetError( OPML_ERROR_PARSING_COMMENT, _parseLineNum, 0 );
    }
    return p;
}


OPMLNode* OPMLComment::ShallowClone( OPMLDocument* doc ) const
{
    if ( !doc ) {
        doc = _document;
    }
    OPMLComment* comment = doc->NewComment( Value() );	// fixme: this will always allocate memory. Intern?
    return comment;
}


bool OPMLComment::ShallowEqual( const OPMLNode* compare ) const
{
    TIOPMLASSERT( compare );
    const OPMLComment* comment = compare->ToComment();
    return ( comment && OPMLUtil::StringEqual( comment->Value(), Value() ));
}


bool OPMLComment::Accept( OPMLVisitor* visitor ) const
{
    TIOPMLASSERT( visitor );
    return visitor->Visit( *this );
}


// --------- OPMLDeclaration ---------- //

OPMLDeclaration::OPMLDeclaration( OPMLDocument* doc ) : OPMLNode( doc )
{
}


OPMLDeclaration::~OPMLDeclaration()
{
    //printf( "~OPMLDeclaration\n" );
}


char* OPMLDeclaration::ParseDeep( char* p, StrPair*, int* curLineNumPtr )
{
    // Declaration parses as text.
    p = _value.ParseText( p, "?>", StrPair::NEEDS_NEWLINE_NORMALIZATION, curLineNumPtr );
    if ( p == 0 ) {
        _document->SetError( OPML_ERROR_PARSING_DECLARATION, _parseLineNum, 0 );
    }
    return p;
}


OPMLNode* OPMLDeclaration::ShallowClone( OPMLDocument* doc ) const
{
    if ( !doc ) {
        doc = _document;
    }
    OPMLDeclaration* dec = doc->NewDeclaration( Value() );	// fixme: this will always allocate memory. Intern?
    return dec;
}


bool OPMLDeclaration::ShallowEqual( const OPMLNode* compare ) const
{
    TIOPMLASSERT( compare );
    const OPMLDeclaration* declaration = compare->ToDeclaration();
    return ( declaration && OPMLUtil::StringEqual( declaration->Value(), Value() ));
}



bool OPMLDeclaration::Accept( OPMLVisitor* visitor ) const
{
    TIOPMLASSERT( visitor );
    return visitor->Visit( *this );
}

// --------- OPMLUnknown ---------- //

OPMLUnknown::OPMLUnknown( OPMLDocument* doc ) : OPMLNode( doc )
{
}


OPMLUnknown::~OPMLUnknown()
{
}


char* OPMLUnknown::ParseDeep( char* p, StrPair*, int* curLineNumPtr )
{
    // Unknown parses as text.
    p = _value.ParseText( p, ">", StrPair::NEEDS_NEWLINE_NORMALIZATION, curLineNumPtr );
    if ( !p ) {
        _document->SetError( OPML_ERROR_PARSING_UNKNOWN, _parseLineNum, 0 );
    }
    return p;
}


OPMLNode* OPMLUnknown::ShallowClone( OPMLDocument* doc ) const
{
    if ( !doc ) {
        doc = _document;
    }
    OPMLUnknown* text = doc->NewUnknown( Value() );	// fixme: this will always allocate memory. Intern?
    return text;
}


bool OPMLUnknown::ShallowEqual( const OPMLNode* compare ) const
{
    TIOPMLASSERT( compare );
    const OPMLUnknown* unknown = compare->ToUnknown();
    return ( unknown && OPMLUtil::StringEqual( unknown->Value(), Value() ));
}


bool OPMLUnknown::Accept( OPMLVisitor* visitor ) const
{
    TIOPMLASSERT( visitor );
    return visitor->Visit( *this );
}

// --------- OPMLAttribute ---------- //

const char* OPMLAttribute::Name() const
{
    return _name.GetStr();
}

const char* OPMLAttribute::Value() const
{
    return _value.GetStr();
}

char* OPMLAttribute::ParseDeep( char* p, bool processEntities, int* curLineNumPtr )
{
    // Parse using the name rules: bug fix, was using ParseText before
    p = _name.ParseName( p );
    if ( !p || !*p ) {
        return 0;
    }

    // Skip white space before =
    p = OPMLUtil::SkipWhiteSpace( p, curLineNumPtr );
    if ( *p != '=' ) {
        return 0;
    }

    ++p;	// move up to opening quote
    p = OPMLUtil::SkipWhiteSpace( p, curLineNumPtr );
    if ( *p != '\"' && *p != '\'' ) {
        return 0;
    }

    const char endTag[2] = { *p, 0 };
    ++p;	// move past opening quote

    p = _value.ParseText( p, endTag, processEntities ? StrPair::ATTRIBUTE_VALUE : StrPair::ATTRIBUTE_VALUE_LEAVE_ENTITIES, curLineNumPtr );
    return p;
}


void OPMLAttribute::SetName( const char* n )
{
    _name.SetStr( n );
}


OPMLError OPMLAttribute::QueryIntValue( int* value ) const
{
    if ( OPMLUtil::ToInt( Value(), value )) {
        return OPML_SUCCESS;
    }
    return OPML_WRONG_ATTRIBUTE_TYPE;
}


OPMLError OPMLAttribute::QueryUnsignedValue( unsigned int* value ) const
{
    if ( OPMLUtil::ToUnsigned( Value(), value )) {
        return OPML_SUCCESS;
    }
    return OPML_WRONG_ATTRIBUTE_TYPE;
}


OPMLError OPMLAttribute::QueryInt64Value(int64_t* value) const
{
	if (OPMLUtil::ToInt64(Value(), value)) {
		return OPML_SUCCESS;
	}
	return OPML_WRONG_ATTRIBUTE_TYPE;
}


OPMLError OPMLAttribute::QueryUnsigned64Value(uint64_t* value) const
{
    if(OPMLUtil::ToUnsigned64(Value(), value)) {
        return OPML_SUCCESS;
    }
    return OPML_WRONG_ATTRIBUTE_TYPE;
}


OPMLError OPMLAttribute::QueryBoolValue( bool* value ) const
{
    if ( OPMLUtil::ToBool( Value(), value )) {
        return OPML_SUCCESS;
    }
    return OPML_WRONG_ATTRIBUTE_TYPE;
}


OPMLError OPMLAttribute::QueryFloatValue( float* value ) const
{
    if ( OPMLUtil::ToFloat( Value(), value )) {
        return OPML_SUCCESS;
    }
    return OPML_WRONG_ATTRIBUTE_TYPE;
}


OPMLError OPMLAttribute::QueryDoubleValue( double* value ) const
{
    if ( OPMLUtil::ToDouble( Value(), value )) {
        return OPML_SUCCESS;
    }
    return OPML_WRONG_ATTRIBUTE_TYPE;
}


void OPMLAttribute::SetAttribute( const char* v )
{
    _value.SetStr( v );
}


void OPMLAttribute::SetAttribute( int v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    _value.SetStr( buf );
}


void OPMLAttribute::SetAttribute( unsigned v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    _value.SetStr( buf );
}


void OPMLAttribute::SetAttribute(int64_t v)
{
	char buf[BUF_SIZE];
	OPMLUtil::ToStr(v, buf, BUF_SIZE);
	_value.SetStr(buf);
}

void OPMLAttribute::SetAttribute(uint64_t v)
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr(v, buf, BUF_SIZE);
    _value.SetStr(buf);
}


void OPMLAttribute::SetAttribute( bool v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    _value.SetStr( buf );
}

void OPMLAttribute::SetAttribute( double v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    _value.SetStr( buf );
}

void OPMLAttribute::SetAttribute( float v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    _value.SetStr( buf );
}


// --------- OPMLElement ---------- //
OPMLElement::OPMLElement( OPMLDocument* doc ) : OPMLNode( doc ),
    _closingType( OPEN ),
    _rootAttribute( 0 )
{
}


OPMLElement::~OPMLElement()
{
    while( _rootAttribute ) {
        OPMLAttribute* next = _rootAttribute->_next;
        DeleteAttribute( _rootAttribute );
        _rootAttribute = next;
    }
}


const OPMLAttribute* OPMLElement::FindAttribute( const char* name ) const
{
    for( OPMLAttribute* a = _rootAttribute; a; a = a->_next ) {
        if ( OPMLUtil::StringEqual( a->Name(), name ) ) {
            return a;
        }
    }
    return 0;
}


const char* OPMLElement::Attribute( const char* name, const char* value ) const
{
    const OPMLAttribute* a = FindAttribute( name );
    if ( !a ) {
        return 0;
    }
    if ( !value || OPMLUtil::StringEqual( a->Value(), value )) {
        return a->Value();
    }
    return 0;
}

int OPMLElement::IntAttribute(const char* name, int defaultValue) const
{
	int i = defaultValue;
	QueryIntAttribute(name, &i);
	return i;
}

unsigned OPMLElement::UnsignedAttribute(const char* name, unsigned defaultValue) const
{
	unsigned i = defaultValue;
	QueryUnsignedAttribute(name, &i);
	return i;
}

int64_t OPMLElement::Int64Attribute(const char* name, int64_t defaultValue) const
{
	int64_t i = defaultValue;
	QueryInt64Attribute(name, &i);
	return i;
}

uint64_t OPMLElement::Unsigned64Attribute(const char* name, uint64_t defaultValue) const
{
	uint64_t i = defaultValue;
	QueryUnsigned64Attribute(name, &i);
	return i;
}

bool OPMLElement::BoolAttribute(const char* name, bool defaultValue) const
{
	bool b = defaultValue;
	QueryBoolAttribute(name, &b);
	return b;
}

double OPMLElement::DoubleAttribute(const char* name, double defaultValue) const
{
	double d = defaultValue;
	QueryDoubleAttribute(name, &d);
	return d;
}

float OPMLElement::FloatAttribute(const char* name, float defaultValue) const
{
	float f = defaultValue;
	QueryFloatAttribute(name, &f);
	return f;
}

const char* OPMLElement::GetText() const
{
    /* skip comment node */
    const OPMLNode* node = FirstChild();
    while (node) {
        if (node->ToComment()) {
            node = node->NextSibling();
            continue;
        }
        break;
    }

    if ( node && node->ToText() ) {
        return node->Value();
    }
    return 0;
}


void	OPMLElement::SetText( const char* inText )
{
	if ( FirstChild() && FirstChild()->ToText() )
		FirstChild()->SetValue( inText );
	else {
		OPMLText*	theText = GetDocument()->NewText( inText );
		InsertFirstChild( theText );
	}
}


void OPMLElement::SetText( int v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    SetText( buf );
}


void OPMLElement::SetText( unsigned v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    SetText( buf );
}


void OPMLElement::SetText(int64_t v)
{
	char buf[BUF_SIZE];
	OPMLUtil::ToStr(v, buf, BUF_SIZE);
	SetText(buf);
}

void OPMLElement::SetText(uint64_t v) {
    char buf[BUF_SIZE];
    OPMLUtil::ToStr(v, buf, BUF_SIZE);
    SetText(buf);
}


void OPMLElement::SetText( bool v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    SetText( buf );
}


void OPMLElement::SetText( float v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    SetText( buf );
}


void OPMLElement::SetText( double v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    SetText( buf );
}


OPMLError OPMLElement::QueryIntText( int* ival ) const
{
    if ( FirstChild() && FirstChild()->ToText() ) {
        const char* t = FirstChild()->Value();
        if ( OPMLUtil::ToInt( t, ival ) ) {
            return OPML_SUCCESS;
        }
        return OPML_CAN_NOT_CONVERT_TEXT;
    }
    return OPML_NO_TEXT_NODE;
}


OPMLError OPMLElement::QueryUnsignedText( unsigned* uval ) const
{
    if ( FirstChild() && FirstChild()->ToText() ) {
        const char* t = FirstChild()->Value();
        if ( OPMLUtil::ToUnsigned( t, uval ) ) {
            return OPML_SUCCESS;
        }
        return OPML_CAN_NOT_CONVERT_TEXT;
    }
    return OPML_NO_TEXT_NODE;
}


OPMLError OPMLElement::QueryInt64Text(int64_t* ival) const
{
	if (FirstChild() && FirstChild()->ToText()) {
		const char* t = FirstChild()->Value();
		if (OPMLUtil::ToInt64(t, ival)) {
			return OPML_SUCCESS;
		}
		return OPML_CAN_NOT_CONVERT_TEXT;
	}
	return OPML_NO_TEXT_NODE;
}


OPMLError OPMLElement::QueryUnsigned64Text(uint64_t* ival) const
{
    if(FirstChild() && FirstChild()->ToText()) {
        const char* t = FirstChild()->Value();
        if(OPMLUtil::ToUnsigned64(t, ival)) {
            return OPML_SUCCESS;
        }
        return OPML_CAN_NOT_CONVERT_TEXT;
    }
    return OPML_NO_TEXT_NODE;
}


OPMLError OPMLElement::QueryBoolText( bool* bval ) const
{
    if ( FirstChild() && FirstChild()->ToText() ) {
        const char* t = FirstChild()->Value();
        if ( OPMLUtil::ToBool( t, bval ) ) {
            return OPML_SUCCESS;
        }
        return OPML_CAN_NOT_CONVERT_TEXT;
    }
    return OPML_NO_TEXT_NODE;
}


OPMLError OPMLElement::QueryDoubleText( double* dval ) const
{
    if ( FirstChild() && FirstChild()->ToText() ) {
        const char* t = FirstChild()->Value();
        if ( OPMLUtil::ToDouble( t, dval ) ) {
            return OPML_SUCCESS;
        }
        return OPML_CAN_NOT_CONVERT_TEXT;
    }
    return OPML_NO_TEXT_NODE;
}


OPMLError OPMLElement::QueryFloatText( float* fval ) const
{
    if ( FirstChild() && FirstChild()->ToText() ) {
        const char* t = FirstChild()->Value();
        if ( OPMLUtil::ToFloat( t, fval ) ) {
            return OPML_SUCCESS;
        }
        return OPML_CAN_NOT_CONVERT_TEXT;
    }
    return OPML_NO_TEXT_NODE;
}

int OPMLElement::IntText(int defaultValue) const
{
	int i = defaultValue;
	QueryIntText(&i);
	return i;
}

unsigned OPMLElement::UnsignedText(unsigned defaultValue) const
{
	unsigned i = defaultValue;
	QueryUnsignedText(&i);
	return i;
}

int64_t OPMLElement::Int64Text(int64_t defaultValue) const
{
	int64_t i = defaultValue;
	QueryInt64Text(&i);
	return i;
}

uint64_t OPMLElement::Unsigned64Text(uint64_t defaultValue) const
{
	uint64_t i = defaultValue;
	QueryUnsigned64Text(&i);
	return i;
}

bool OPMLElement::BoolText(bool defaultValue) const
{
	bool b = defaultValue;
	QueryBoolText(&b);
	return b;
}

double OPMLElement::DoubleText(double defaultValue) const
{
	double d = defaultValue;
	QueryDoubleText(&d);
	return d;
}

float OPMLElement::FloatText(float defaultValue) const
{
	float f = defaultValue;
	QueryFloatText(&f);
	return f;
}


OPMLAttribute* OPMLElement::FindOrCreateAttribute( const char* name )
{
    OPMLAttribute* last = 0;
    OPMLAttribute* attrib = 0;
    for( attrib = _rootAttribute;
            attrib;
            last = attrib, attrib = attrib->_next ) {
        if ( OPMLUtil::StringEqual( attrib->Name(), name ) ) {
            break;
        }
    }
    if ( !attrib ) {
        attrib = CreateAttribute();
        TIOPMLASSERT( attrib );
        if ( last ) {
            TIOPMLASSERT( last->_next == 0 );
            last->_next = attrib;
        }
        else {
            TIOPMLASSERT( _rootAttribute == 0 );
            _rootAttribute = attrib;
        }
        attrib->SetName( name );
    }
    return attrib;
}


void OPMLElement::DeleteAttribute( const char* name )
{
    OPMLAttribute* prev = 0;
    for( OPMLAttribute* a=_rootAttribute; a; a=a->_next ) {
        if ( OPMLUtil::StringEqual( name, a->Name() ) ) {
            if ( prev ) {
                prev->_next = a->_next;
            }
            else {
                _rootAttribute = a->_next;
            }
            DeleteAttribute( a );
            break;
        }
        prev = a;
    }
}


char* OPMLElement::ParseAttributes( char* p, int* curLineNumPtr )
{
    OPMLAttribute* prevAttribute = 0;

    // Read the attributes.
    while( p ) {
        p = OPMLUtil::SkipWhiteSpace( p, curLineNumPtr );
        if ( !(*p) ) {
            _document->SetError( OPML_ERROR_PARSING_ELEMENT, _parseLineNum, "OPMLElement name=%s", Name() );
            return 0;
        }

        // attribute.
        if (OPMLUtil::IsNameStartChar( (unsigned char) *p ) ) {
            OPMLAttribute* attrib = CreateAttribute();
            TIOPMLASSERT( attrib );
            attrib->_parseLineNum = _document->_parseCurLineNum;

            const int attrLineNum = attrib->_parseLineNum;

            p = attrib->ParseDeep( p, _document->ProcessEntities(), curLineNumPtr );
            if ( !p || Attribute( attrib->Name() ) ) {
                DeleteAttribute( attrib );
                _document->SetError( OPML_ERROR_PARSING_ATTRIBUTE, attrLineNum, "OPMLElement name=%s", Name() );
                return 0;
            }
            // There is a minor bug here: if the attribute in the source opml
            // document is duplicated, it will not be detected and the
            // attribute will be doubly added. However, tracking the 'prevAttribute'
            // avoids re-scanning the attribute list. Preferring performance for
            // now, may reconsider in the future.
            if ( prevAttribute ) {
                TIOPMLASSERT( prevAttribute->_next == 0 );
                prevAttribute->_next = attrib;
            }
            else {
                TIOPMLASSERT( _rootAttribute == 0 );
                _rootAttribute = attrib;
            }
            prevAttribute = attrib;
        }
        // end of the tag
        else if ( *p == '>' ) {
            ++p;
            break;
        }
        // end of the tag
        else if ( *p == '/' && *(p+1) == '>' ) {
            _closingType = CLOSED;
            return p+2;	// done; sealed element.
        }
        else {
            _document->SetError( OPML_ERROR_PARSING_ELEMENT, _parseLineNum, 0 );
            return 0;
        }
    }
    return p;
}

void OPMLElement::DeleteAttribute( OPMLAttribute* attribute )
{
    if ( attribute == 0 ) {
        return;
    }
    MemPool* pool = attribute->_memPool;
    attribute->~OPMLAttribute();
    pool->Free( attribute );
}

OPMLAttribute* OPMLElement::CreateAttribute()
{
    TIOPMLASSERT( sizeof( OPMLAttribute ) == _document->_attributePool.ItemSize() );
    OPMLAttribute* attrib = new (_document->_attributePool.Alloc() ) OPMLAttribute();
    TIOPMLASSERT( attrib );
    attrib->_memPool = &_document->_attributePool;
    attrib->_memPool->SetTracked();
    return attrib;
}


OPMLElement* OPMLElement::InsertNewChildElement(const char* name)
{
    OPMLElement* node = _document->NewElement(name);
    return InsertEndChild(node) ? node : 0;
}

OPMLComment* OPMLElement::InsertNewComment(const char* comment)
{
    OPMLComment* node = _document->NewComment(comment);
    return InsertEndChild(node) ? node : 0;
}

OPMLText* OPMLElement::InsertNewText(const char* text)
{
    OPMLText* node = _document->NewText(text);
    return InsertEndChild(node) ? node : 0;
}

OPMLDeclaration* OPMLElement::InsertNewDeclaration(const char* text)
{
    OPMLDeclaration* node = _document->NewDeclaration(text);
    return InsertEndChild(node) ? node : 0;
}

OPMLUnknown* OPMLElement::InsertNewUnknown(const char* text)
{
    OPMLUnknown* node = _document->NewUnknown(text);
    return InsertEndChild(node) ? node : 0;
}



//
//	<ele></ele>
//	<ele>foo<b>bar</b></ele>
//
char* OPMLElement::ParseDeep( char* p, StrPair* parentEndTag, int* curLineNumPtr )
{
    // Read the element name.
    p = OPMLUtil::SkipWhiteSpace( p, curLineNumPtr );

    // The closing element is the </element> form. It is
    // parsed just like a regular element then deleted from
    // the DOM.
    if ( *p == '/' ) {
        _closingType = CLOSING;
        ++p;
    }

    p = _value.ParseName( p );
    if ( _value.Empty() ) {
        return 0;
    }

    p = ParseAttributes( p, curLineNumPtr );
    if ( !p || !*p || _closingType != OPEN ) {
        return p;
    }

    p = OPMLNode::ParseDeep( p, parentEndTag, curLineNumPtr );
    return p;
}



OPMLNode* OPMLElement::ShallowClone( OPMLDocument* doc ) const
{
    if ( !doc ) {
        doc = _document;
    }
    OPMLElement* element = doc->NewElement( Value() );					// fixme: this will always allocate memory. Intern?
    for( const OPMLAttribute* a=FirstAttribute(); a; a=a->Next() ) {
        element->SetAttribute( a->Name(), a->Value() );					// fixme: this will always allocate memory. Intern?
    }
    return element;
}


bool OPMLElement::ShallowEqual( const OPMLNode* compare ) const
{
    TIOPMLASSERT( compare );
    const OPMLElement* other = compare->ToElement();
    if ( other && OPMLUtil::StringEqual( other->Name(), Name() )) {

        const OPMLAttribute* a=FirstAttribute();
        const OPMLAttribute* b=other->FirstAttribute();

        while ( a && b ) {
            if ( !OPMLUtil::StringEqual( a->Value(), b->Value() ) ) {
                return false;
            }
            a = a->Next();
            b = b->Next();
        }
        if ( a || b ) {
            // different count
            return false;
        }
        return true;
    }
    return false;
}


bool OPMLElement::Accept( OPMLVisitor* visitor ) const
{
    TIOPMLASSERT( visitor );
    if ( visitor->VisitEnter( *this, _rootAttribute ) ) {
        for ( const OPMLNode* node=FirstChild(); node; node=node->NextSibling() ) {
            if ( !node->Accept( visitor ) ) {
                break;
            }
        }
    }
    return visitor->VisitExit( *this );
}


// --------- OPMLDocument ----------- //

// Warning: List must match 'enum OPMLError'
const char* OPMLDocument::_errorNames[OPML_ERROR_COUNT] = {
    "OPML_SUCCESS",
    "OPML_NO_ATTRIBUTE",
    "OPML_WRONG_ATTRIBUTE_TYPE",
    "OPML_ERROR_FILE_NOT_FOUND",
    "OPML_ERROR_FILE_COULD_NOT_BE_OPENED",
    "OPML_ERROR_FILE_READ_ERROR",
    "OPML_ERROR_PARSING_ELEMENT",
    "OPML_ERROR_PARSING_ATTRIBUTE",
    "OPML_ERROR_PARSING_TEXT",
    "OPML_ERROR_PARSING_CDATA",
    "OPML_ERROR_PARSING_COMMENT",
    "OPML_ERROR_PARSING_DECLARATION",
    "OPML_ERROR_PARSING_UNKNOWN",
    "OPML_ERROR_EMPTY_DOCUMENT",
    "OPML_ERROR_MISMATCHED_ELEMENT",
    "OPML_ERROR_PARSING",
    "OPML_CAN_NOT_CONVERT_TEXT",
    "OPML_NO_TEXT_NODE",
	"OPML_ELEMENT_DEPTH_EXCEEDED"
};


OPMLDocument::OPMLDocument( bool processEntities, Whitespace whitespaceMode ) :
    OPMLNode( 0 ),
    _writeBOM( false ),
    _processEntities( processEntities ),
    _errorID(OPML_SUCCESS),
    _whitespaceMode( whitespaceMode ),
    _errorStr(),
    _errorLineNum( 0 ),
    _charBuffer( 0 ),
    _parseCurLineNum( 0 ),
	_parsingDepth(0),
    _unlinked(),
    _elementPool(),
    _attributePool(),
    _textPool(),
    _commentPool()
{
    // avoid VC++ C4355 warning about 'this' in initializer list (C4355 is off by default in VS2012+)
    _document = this;
}


OPMLDocument::~OPMLDocument()
{
    Clear();
}


void OPMLDocument::MarkInUse(const OPMLNode* const node)
{
	TIOPMLASSERT(node);
	TIOPMLASSERT(node->_parent == 0);

	for (int i = 0; i < _unlinked.Size(); ++i) {
		if (node == _unlinked[i]) {
			_unlinked.SwapRemove(i);
			break;
		}
	}
}

void OPMLDocument::Clear()
{
    DeleteChildren();
	while( _unlinked.Size()) {
		DeleteNode(_unlinked[0]);	// Will remove from _unlinked as part of delete.
	}

#ifdef TINYOPML2_DEBUG
    const bool hadError = Error();
#endif
    ClearError();

    delete [] _charBuffer;
    _charBuffer = 0;
	_parsingDepth = 0;

#if 0
    _textPool.Trace( "text" );
    _elementPool.Trace( "element" );
    _commentPool.Trace( "comment" );
    _attributePool.Trace( "attribute" );
#endif

#ifdef TINYOPML2_DEBUG
    if ( !hadError ) {
        TIOPMLASSERT( _elementPool.CurrentAllocs()   == _elementPool.Untracked() );
        TIOPMLASSERT( _attributePool.CurrentAllocs() == _attributePool.Untracked() );
        TIOPMLASSERT( _textPool.CurrentAllocs()      == _textPool.Untracked() );
        TIOPMLASSERT( _commentPool.CurrentAllocs()   == _commentPool.Untracked() );
    }
#endif
}


void OPMLDocument::DeepCopy(OPMLDocument* target) const
{
	TIOPMLASSERT(target);
    if (target == this) {
        return; // technically success - a no-op.
    }

	target->Clear();
	for (const OPMLNode* node = this->FirstChild(); node; node = node->NextSibling()) {
		target->InsertEndChild(node->DeepClone(target));
	}
}

OPMLElement* OPMLDocument::NewElement( const char* name )
{
    OPMLElement* ele = CreateUnlinkedNode<OPMLElement>( _elementPool );
    ele->SetName( name );
    return ele;
}


OPMLComment* OPMLDocument::NewComment( const char* str )
{
    OPMLComment* comment = CreateUnlinkedNode<OPMLComment>( _commentPool );
    comment->SetValue( str );
    return comment;
}


OPMLText* OPMLDocument::NewText( const char* str )
{
    OPMLText* text = CreateUnlinkedNode<OPMLText>( _textPool );
    text->SetValue( str );
    return text;
}


OPMLDeclaration* OPMLDocument::NewDeclaration( const char* str )
{
    OPMLDeclaration* dec = CreateUnlinkedNode<OPMLDeclaration>( _commentPool );
    dec->SetValue( str ? str : "opml version=\"1.0\" encoding=\"UTF-8\"" );
    return dec;
}


OPMLUnknown* OPMLDocument::NewUnknown( const char* str )
{
    OPMLUnknown* unk = CreateUnlinkedNode<OPMLUnknown>( _commentPool );
    unk->SetValue( str );
    return unk;
}

static FILE* callfopen( const char* filepath, const char* mode )
{
    TIOPMLASSERT( filepath );
    TIOPMLASSERT( mode );
#if defined(_MSC_VER) && (_MSC_VER >= 1400 ) && (!defined WINCE)
    FILE* fp = 0;
    const errno_t err = fopen_s( &fp, filepath, mode );
    if ( err ) {
        return 0;
    }
#else
    FILE* fp = fopen( filepath, mode );
#endif
    return fp;
}

void OPMLDocument::DeleteNode( OPMLNode* node )	{
    TIOPMLASSERT( node );
    TIOPMLASSERT(node->_document == this );
    if (node->_parent) {
        node->_parent->DeleteChild( node );
    }
    else {
        // Isn't in the tree.
        // Use the parent delete.
        // Also, we need to mark it tracked: we 'know'
        // it was never used.
        node->_memPool->SetTracked();
        // Call the static OPMLNode version:
        OPMLNode::DeleteNode(node);
    }
}


OPMLError OPMLDocument::LoadFile( const char* filename )
{
    if ( !filename ) {
        TIOPMLASSERT( false );
        SetError( OPML_ERROR_FILE_COULD_NOT_BE_OPENED, 0, "filename=<null>" );
        return _errorID;
    }

    Clear();
    FILE* fp = callfopen( filename, "rb" );
    if ( !fp ) {
        SetError( OPML_ERROR_FILE_NOT_FOUND, 0, "filename=%s", filename );
        return _errorID;
    }
    LoadFile( fp );
    fclose( fp );
    return _errorID;
}

OPMLError OPMLDocument::LoadFile( FILE* fp )
{
    Clear();

    TIOPML_FSEEK( fp, 0, SEEK_SET );
    if ( fgetc( fp ) == EOF && ferror( fp ) != 0 ) {
        SetError( OPML_ERROR_FILE_READ_ERROR, 0, 0 );
        return _errorID;
    }

    TIOPML_FSEEK( fp, 0, SEEK_END );

    unsigned long long filelength;
    {
        const long long fileLengthSigned = TIOPML_FTELL( fp );
        TIOPML_FSEEK( fp, 0, SEEK_SET );
        if ( fileLengthSigned == -1L ) {
            SetError( OPML_ERROR_FILE_READ_ERROR, 0, 0 );
            return _errorID;
        }
        TIOPMLASSERT( fileLengthSigned >= 0 );
        filelength = static_cast<unsigned long long>(fileLengthSigned);
    }

    const size_t maxSizeT = static_cast<size_t>(-1);
    // We'll do the comparison as an unsigned long long, because that's guaranteed to be at
    // least 8 bytes, even on a 32-bit platform.
    if ( filelength >= static_cast<unsigned long long>(maxSizeT) ) {
        // Cannot handle files which won't fit in buffer together with null terminator
        SetError( OPML_ERROR_FILE_READ_ERROR, 0, 0 );
        return _errorID;
    }

    if ( filelength == 0 ) {
        SetError( OPML_ERROR_EMPTY_DOCUMENT, 0, 0 );
        return _errorID;
    }

    const size_t size = static_cast<size_t>(filelength);
    TIOPMLASSERT( _charBuffer == 0 );
    _charBuffer = new char[size+1];
    const size_t read = fread( _charBuffer, 1, size, fp );
    if ( read != size ) {
        SetError( OPML_ERROR_FILE_READ_ERROR, 0, 0 );
        return _errorID;
    }

    _charBuffer[size] = 0;

    Parse();
    return _errorID;
}


OPMLError OPMLDocument::SaveFile( const char* filename, bool compact )
{
    if ( !filename ) {
        TIOPMLASSERT( false );
        SetError( OPML_ERROR_FILE_COULD_NOT_BE_OPENED, 0, "filename=<null>" );
        return _errorID;
    }

    FILE* fp = callfopen( filename, "w" );
    if ( !fp ) {
        SetError( OPML_ERROR_FILE_COULD_NOT_BE_OPENED, 0, "filename=%s", filename );
        return _errorID;
    }
    SaveFile(fp, compact);
    fclose( fp );
    return _errorID;
}


OPMLError OPMLDocument::SaveFile( FILE* fp, bool compact )
{
    // Clear any error from the last save, otherwise it will get reported
    // for *this* call.
    ClearError();
    OPMLPrinter stream( fp, compact );
    Print( &stream );
    return _errorID;
}


OPMLError OPMLDocument::Parse( const char* p, size_t len )
{
    Clear();

    if ( len == 0 || !p || !*p ) {
        SetError( OPML_ERROR_EMPTY_DOCUMENT, 0, 0 );
        return _errorID;
    }
    if ( len == static_cast<size_t>(-1) ) {
        len = strlen( p );
    }
    TIOPMLASSERT( _charBuffer == 0 );
    _charBuffer = new char[ len+1 ];
    memcpy( _charBuffer, p, len );
    _charBuffer[len] = 0;

    Parse();
    if ( Error() ) {
        // clean up now essentially dangling memory.
        // and the parse fail can put objects in the
        // pools that are dead and inaccessible.
        DeleteChildren();
        _elementPool.Clear();
        _attributePool.Clear();
        _textPool.Clear();
        _commentPool.Clear();
    }
    return _errorID;
}


void OPMLDocument::Print( OPMLPrinter* streamer ) const
{
    if ( streamer ) {
        Accept( streamer );
    }
    else {
        OPMLPrinter stdoutStreamer( stdout );
        Accept( &stdoutStreamer );
    }
}


void OPMLDocument::ClearError() {
    _errorID = OPML_SUCCESS;
    _errorLineNum = 0;
    _errorStr.Reset();
}


void OPMLDocument::SetError( OPMLError error, int lineNum, const char* format, ... )
{
    TIOPMLASSERT( error >= 0 && error < OPML_ERROR_COUNT );
    _errorID = error;
    _errorLineNum = lineNum;
	_errorStr.Reset();

    const size_t BUFFER_SIZE = 1000;
    char* buffer = new char[BUFFER_SIZE];

    TIOPMLASSERT(sizeof(error) <= sizeof(int));
    TIOPML_SNPRINTF(buffer, BUFFER_SIZE, "Error=%s ErrorID=%d (0x%x) Line number=%d", ErrorIDToName(error), int(error), int(error), lineNum);

	if (format) {
		size_t len = strlen(buffer);
		TIOPML_SNPRINTF(buffer + len, BUFFER_SIZE - len, ": ");
		len = strlen(buffer);

		va_list va;
		va_start(va, format);
		TIOPML_VSNPRINTF(buffer + len, BUFFER_SIZE - len, format, va);
		va_end(va);
	}
	_errorStr.SetStr(buffer);
	delete[] buffer;
}


/*static*/ const char* OPMLDocument::ErrorIDToName(OPMLError errorID)
{
	TIOPMLASSERT( errorID >= 0 && errorID < OPML_ERROR_COUNT );
    const char* errorName = _errorNames[errorID];
    TIOPMLASSERT( errorName && errorName[0] );
    return errorName;
}

const char* OPMLDocument::ErrorStr() const
{
	return _errorStr.Empty() ? "" : _errorStr.GetStr();
}


void OPMLDocument::PrintError() const
{
    printf("%s\n", ErrorStr());
}

const char* OPMLDocument::ErrorName() const
{
    return ErrorIDToName(_errorID);
}

void OPMLDocument::Parse()
{
    TIOPMLASSERT( NoChildren() ); // Clear() must have been called previously
    TIOPMLASSERT( _charBuffer );
    _parseCurLineNum = 1;
    _parseLineNum = 1;
    char* p = _charBuffer;
    p = OPMLUtil::SkipWhiteSpace( p, &_parseCurLineNum );
    p = const_cast<char*>( OPMLUtil::ReadBOM( p, &_writeBOM ) );
    if ( !*p ) {
        SetError( OPML_ERROR_EMPTY_DOCUMENT, 0, 0 );
        return;
    }
    ParseDeep(p, 0, &_parseCurLineNum );
}

void OPMLDocument::PushDepth()
{
	_parsingDepth++;
	if (_parsingDepth == TINYOPML2_MAX_ELEMENT_DEPTH) {
		SetError(OPML_ELEMENT_DEPTH_EXCEEDED, _parseCurLineNum, "Element nesting is too deep." );
	}
}

void OPMLDocument::PopDepth()
{
	TIOPMLASSERT(_parsingDepth > 0);
	--_parsingDepth;
}

OPMLPrinter::OPMLPrinter( FILE* file, bool compact, int depth ) :
    _elementJustOpened( false ),
    _stack(),
    _firstElement( true ),
    _fp( file ),
    _depth( depth ),
    _textDepth( -1 ),
    _processEntities( true ),
    _compactMode( compact ),
    _buffer()
{
    for( int i=0; i<ENTITY_RANGE; ++i ) {
        _entityFlag[i] = false;
        _restrictedEntityFlag[i] = false;
    }
    for( int i=0; i<NUM_ENTITIES; ++i ) {
        const char entityValue = entities[i].value;
        const unsigned char flagIndex = static_cast<unsigned char>(entityValue);
        TIOPMLASSERT( flagIndex < ENTITY_RANGE );
        _entityFlag[flagIndex] = true;
    }
    _restrictedEntityFlag[static_cast<unsigned char>('&')] = true;
    _restrictedEntityFlag[static_cast<unsigned char>('<')] = true;
    _restrictedEntityFlag[static_cast<unsigned char>('>')] = true;	// not required, but consistency is nice
    _buffer.Push( 0 );
}


void OPMLPrinter::Print( const char* format, ... )
{
    va_list     va;
    va_start( va, format );

    if ( _fp ) {
        vfprintf( _fp, format, va );
    }
    else {
        const int len = TIOPML_VSCPRINTF( format, va );
        // Close out and re-start the va-args
        va_end( va );
        TIOPMLASSERT( len >= 0 );
        va_start( va, format );
        TIOPMLASSERT( _buffer.Size() > 0 && _buffer[_buffer.Size() - 1] == 0 );
        char* p = _buffer.PushArr( len ) - 1;	// back up over the null terminator.
		TIOPML_VSNPRINTF( p, len+1, format, va );
    }
    va_end( va );
}


void OPMLPrinter::Write( const char* data, size_t size )
{
    if ( _fp ) {
        fwrite ( data , sizeof(char), size, _fp);
    }
    else {
        char* p = _buffer.PushArr( static_cast<int>(size) ) - 1;   // back up over the null terminator.
        memcpy( p, data, size );
        p[size] = 0;
    }
}


void OPMLPrinter::Putc( char ch )
{
    if ( _fp ) {
        fputc ( ch, _fp);
    }
    else {
        char* p = _buffer.PushArr( sizeof(char) ) - 1;   // back up over the null terminator.
        p[0] = ch;
        p[1] = 0;
    }
}


void OPMLPrinter::PrintSpace( int depth )
{
    for( int i=0; i<depth; ++i ) {
        Write( "    " );
    }
}


void OPMLPrinter::PrintString( const char* p, bool restricted )
{
    // Look for runs of bytes between entities to print.
    const char* q = p;

    if ( _processEntities ) {
        const bool* flag = restricted ? _restrictedEntityFlag : _entityFlag;
        while ( *q ) {
            TIOPMLASSERT( p <= q );
            // Remember, char is sometimes signed. (How many times has that bitten me?)
            if ( *q > 0 && *q < ENTITY_RANGE ) {
                // Check for entities. If one is found, flush
                // the stream up until the entity, write the
                // entity, and keep looking.
                if ( flag[static_cast<unsigned char>(*q)] ) {
                    while ( p < q ) {
                        const size_t delta = q - p;
                        const int toPrint = ( INT_MAX < delta ) ? INT_MAX : static_cast<int>(delta);
                        Write( p, toPrint );
                        p += toPrint;
                    }
                    bool entityPatternPrinted = false;
                    for( int i=0; i<NUM_ENTITIES; ++i ) {
                        if ( entities[i].value == *q ) {
                            Putc( '&' );
                            Write( entities[i].pattern, entities[i].length );
                            Putc( ';' );
                            entityPatternPrinted = true;
                            break;
                        }
                    }
                    if ( !entityPatternPrinted ) {
                        // TIOPMLASSERT( entityPatternPrinted ) causes gcc -Wunused-but-set-variable in release
                        TIOPMLASSERT( false );
                    }
                    ++p;
                }
            }
            ++q;
            TIOPMLASSERT( p <= q );
        }
        // Flush the remaining string. This will be the entire
        // string if an entity wasn't found.
        if ( p < q ) {
            const size_t delta = q - p;
            const int toPrint = ( INT_MAX < delta ) ? INT_MAX : static_cast<int>(delta);
            Write( p, toPrint );
        }
    }
    else {
        Write( p );
    }
}


void OPMLPrinter::PushHeader( bool writeBOM, bool writeDec )
{
    if ( writeBOM ) {
        static const unsigned char bom[] = { TIOPML_UTF_LEAD_0, TIOPML_UTF_LEAD_1, TIOPML_UTF_LEAD_2, 0 };
        Write( reinterpret_cast< const char* >( bom ) );
    }
    if ( writeDec ) {
        PushDeclaration( "opml version=\"1.0\"" );
    }
}

void OPMLPrinter::PrepareForNewNode( bool compactMode )
{
    SealElementIfJustOpened();

    if ( compactMode ) {
        return;
    }

    if ( _firstElement ) {
        PrintSpace (_depth);
    } else if ( _textDepth < 0) {
        Putc( '\n' );
        PrintSpace( _depth );
    }

    _firstElement = false;
}

void OPMLPrinter::OpenElement( const char* name, bool compactMode )
{
    PrepareForNewNode( compactMode );
    _stack.Push( name );

    Write ( "<" );
    Write ( name );

    _elementJustOpened = true;
    ++_depth;
}


void OPMLPrinter::PushAttribute( const char* name, const char* value )
{
    TIOPMLASSERT( _elementJustOpened );
    Putc ( ' ' );
    Write( name );
    Write( "=\"" );
    PrintString( value, false );
    Putc ( '\"' );
}


void OPMLPrinter::PushAttribute( const char* name, int v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    PushAttribute( name, buf );
}


void OPMLPrinter::PushAttribute( const char* name, unsigned v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    PushAttribute( name, buf );
}


void OPMLPrinter::PushAttribute(const char* name, int64_t v)
{
	char buf[BUF_SIZE];
	OPMLUtil::ToStr(v, buf, BUF_SIZE);
	PushAttribute(name, buf);
}


void OPMLPrinter::PushAttribute(const char* name, uint64_t v)
{
	char buf[BUF_SIZE];
	OPMLUtil::ToStr(v, buf, BUF_SIZE);
	PushAttribute(name, buf);
}


void OPMLPrinter::PushAttribute( const char* name, bool v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    PushAttribute( name, buf );
}


void OPMLPrinter::PushAttribute( const char* name, double v )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( v, buf, BUF_SIZE );
    PushAttribute( name, buf );
}


void OPMLPrinter::CloseElement( bool compactMode )
{
    --_depth;
    const char* name = _stack.Pop();

    if ( _elementJustOpened ) {
        Write( "/>" );
    }
    else {
        if ( _textDepth < 0 && !compactMode) {
            Putc( '\n' );
            PrintSpace( _depth );
        }
        Write ( "</" );
        Write ( name );
        Write ( ">" );
    }

    if ( _textDepth == _depth ) {
        _textDepth = -1;
    }
    if ( _depth == 0 && !compactMode) {
        Putc( '\n' );
    }
    _elementJustOpened = false;
}


void OPMLPrinter::SealElementIfJustOpened()
{
    if ( !_elementJustOpened ) {
        return;
    }
    _elementJustOpened = false;
    Putc( '>' );
}


void OPMLPrinter::PushText( const char* text, bool cdata )
{
    _textDepth = _depth-1;

    SealElementIfJustOpened();
    if ( cdata ) {
        Write( "<![CDATA[" );
        Write( text );
        Write( "]]>" );
    }
    else {
        PrintString( text, true );
    }
}


void OPMLPrinter::PushText( int64_t value )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}


void OPMLPrinter::PushText( uint64_t value )
{
	char buf[BUF_SIZE];
	OPMLUtil::ToStr(value, buf, BUF_SIZE);
	PushText(buf, false);
}


void OPMLPrinter::PushText( int value )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}


void OPMLPrinter::PushText( unsigned value )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}


void OPMLPrinter::PushText( bool value )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}


void OPMLPrinter::PushText( float value )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}


void OPMLPrinter::PushText( double value )
{
    char buf[BUF_SIZE];
    OPMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}


void OPMLPrinter::PushComment( const char* comment )
{
    PrepareForNewNode( _compactMode );

    Write( "<!--" );
    Write( comment );
    Write( "-->" );
}


void OPMLPrinter::PushDeclaration( const char* value )
{
    PrepareForNewNode( _compactMode );

    Write( "<?" );
    Write( value );
    Write( "?>" );
}


void OPMLPrinter::PushUnknown( const char* value )
{
    PrepareForNewNode( _compactMode );

    Write( "<!" );
    Write( value );
    Putc( '>' );
}


bool OPMLPrinter::VisitEnter( const OPMLDocument& doc )
{
    _processEntities = doc.ProcessEntities();
    if ( doc.HasBOM() ) {
        PushHeader( true, false );
    }
    return true;
}


bool OPMLPrinter::VisitEnter( const OPMLElement& element, const OPMLAttribute* attribute )
{
    const OPMLElement* parentElem = 0;
    if ( element.Parent() ) {
        parentElem = element.Parent()->ToElement();
    }
    const bool compactMode = parentElem ? CompactMode( *parentElem ) : _compactMode;
    OpenElement( element.Name(), compactMode );
    while ( attribute ) {
        PushAttribute( attribute->Name(), attribute->Value() );
        attribute = attribute->Next();
    }
    return true;
}


bool OPMLPrinter::VisitExit( const OPMLElement& element )
{
    CloseElement( CompactMode(element) );
    return true;
}


bool OPMLPrinter::Visit( const OPMLText& text )
{
    PushText( text.Value(), text.CData() );
    return true;
}


bool OPMLPrinter::Visit( const OPMLComment& comment )
{
    PushComment( comment.Value() );
    return true;
}

bool OPMLPrinter::Visit( const OPMLDeclaration& declaration )
{
    PushDeclaration( declaration.Value() );
    return true;
}


bool OPMLPrinter::Visit( const OPMLUnknown& unknown )
{
    PushUnknown( unknown.Value() );
    return true;
}

}   // namespace tinyopml

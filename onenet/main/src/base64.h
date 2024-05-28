#ifndef _BASE64_H_
#define _BASE64_H_

enum Escaped {
        WC_STD_ENC = 0,       /* normal \n line ending encoding */
        WC_ESC_NL_ENC,        /* use escape sequence encoding   */
        WC_NO_NL_ENC          /* no encoding at all             */
    }; /* Encoding types */


#ifndef byte
    typedef unsigned char  byte;
#endif
    typedef unsigned short word16;
    typedef unsigned int   word32;
    typedef byte           word24[3]; 	
	
int Base64_Decode(const byte* in, word32 inLen, byte* out,word32* outLen);
int Base64_Encode(const byte* in, word32 inLen, byte* out,word32* outLen);
int Base64_EncodeEsc(const byte* in, word32 inLen, byte* out,word32* outLen);
int Base64_Encode_NoNl(const byte* in, word32 inLen, byte* out,word32* outLen);

#endif

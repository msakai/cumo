typedef u_int16_t dtype;
typedef u_int16_t rtype;
#define cT  cumo_cUInt16
#define cRT cT

#define m_num_to_data(x) ((dtype)NUM2UINT(x))
#define m_data_to_num(x) UINT2NUM((unsigned int)(x))
#define m_extract(x)     UINT2NUM((unsigned int)*(dtype*)(x))
#define m_sprintf(s,x)   sprintf(s,"%u",(unsigned int)(x))

#ifndef UINT16_MIN
#define UINT16_MIN (0)
#endif

#ifndef UINT16_MAX
#define UINT16_MAX (65535)
#endif

#define DATA_MIN UINT16_MIN
#define DATA_MAX UINT16_MAX

#define M_MIN  INT2FIX(0)
#define M_MAX  m_data_to_num(UINT16_MAX)

#include "uint_macro.h"

#define NULL 0
#include <ctype.h>
#include <string.h>
//#include <sys/types.h>
//#include <stdarg.h>
//#include <stdlib.h>

#include "../common/evm.h"

#define LDOUBLE float
typedef evm_val32_t *va_list;


#define VA_COPY(dest, src) ((dest) = (src))
#define va_arg(list, section) ((list++)->section)


#define DP_S_DEFAULT 0
#define DP_S_FLAGS   1
#define DP_S_MIN     2
#define DP_S_DOT     3
#define DP_S_MAX     4
#define DP_S_MOD     5
#define DP_S_CONV    6
#define DP_S_DONE    7

/* format flags - Bits */
#define DP_F_MINUS 	(1 << 0)
#define DP_F_PLUS  	(1 << 1)
#define DP_F_SPACE 	(1 << 2)
#define DP_F_NUM   	(1 << 3)
#define DP_F_ZERO  	(1 << 4)
#define DP_F_UP    	(1 << 5)
#define DP_F_UNSIGNED 	(1 << 6)

/* Conversion Flags */
#define DP_C_SHORT   1
#define DP_C_LONG    2
#define DP_C_LDOUBLE 3
#define DP_C_long   4

#define char_to_int(p) ((p)- '0')
#ifndef MAX
#define MAX(p,q) (((p) >= (q)) ? (p) : (q))
#endif



static size_t dopr(evm_thread_t *thread, char *buffer, size_t maxlen, const char *format, 
		   evm_result_t *result, evm_u32 legalFormatLength, va_list args_in, va_list maxarg);
static int fmtstr(char *buffer, evm_u32 legalLength, size_t *currlen, size_t maxlen,
		    char *fvalue, int flags, int min, int max);
static void fmtint(char *buffer, size_t *currlen, size_t maxlen,
		    long fvalue, int base, int min, int max, int flags);
static void fmtfp(char *buffer, size_t *currlen, size_t maxlen,
		   LDOUBLE fvalue, int min, int max, int flags);
static void dopr_outch(char *buffer, size_t *currlen, size_t maxlen, char c);


#define SAFEREADCHAR(dst) if(fmtOffset == legalFormatLength)\
{\
	*result = EVM_RTERR_ACCESS_VIOLATION;\
	return currlen;\
}\
dst = format[fmtOffset++]

static size_t dopr(evm_thread_t *thread, char *buffer, size_t maxlen, const char *format, evm_result_t *result, evm_u32 legalFormatLength, va_list args_in, va_list maxarg)
{
	char ch;
	int fvalue;
	char *strfvalue;
	int min;
	int max;
	int state;
	int flags;
	int cflags;
	evm_u32 fmtOffset;
	size_t currlen;
	va_list args;

	evm_pointer_t ptr;

	VA_COPY(args, args_in);

	*result = EVM_OK;

	state = DP_S_DEFAULT;
	currlen = flags = cflags = min = 0;
	max = -1;

	fmtOffset = 0;

	if(!legalFormatLength)
	{
		*result = EVM_RTERR_ACCESS_VIOLATION;
		return 0;
	}

	ch = format[fmtOffset++];
	
	while (state != DP_S_DONE) {
		if (ch == '\0') 
			state = DP_S_DONE;

		switch(state) {
		case DP_S_DEFAULT:
			if (ch == '%') 
				state = DP_S_FLAGS;
			else 
				dopr_outch (buffer, &currlen, maxlen, ch);
			SAFEREADCHAR(ch);
			break;
		case DP_S_FLAGS:
			switch (ch) {
			case '-':
				flags |= DP_F_MINUS;
				SAFEREADCHAR(ch);
				break;
			case '+':
				flags |= DP_F_PLUS;
				SAFEREADCHAR(ch);
				break;
			case ' ':
				flags |= DP_F_SPACE;
				SAFEREADCHAR(ch);
				break;
			case '#':
				flags |= DP_F_NUM;
				SAFEREADCHAR(ch);
				break;
			case '0':
				flags |= DP_F_ZERO;
				SAFEREADCHAR(ch);
				break;
			default:
				state = DP_S_MIN;
				break;
			}
			break;
		case DP_S_MIN:
			if (isdigit((unsigned char)ch)) {
				min = 10*min + char_to_int (ch);
				SAFEREADCHAR(ch);
			} else if (ch == '*') {
				if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
				min = va_arg (args, l);
				SAFEREADCHAR(ch);
				state = DP_S_DOT;
			} else {
				state = DP_S_DOT;
			}
			break;
		case DP_S_DOT:
			if (ch == '.') {
				state = DP_S_MAX;
				SAFEREADCHAR(ch);
			} else { 
				state = DP_S_MOD;
			}
			break;
		case DP_S_MAX:
			if (isdigit((unsigned char)ch)) {
				if (max < 0)
					max = 0;
				max = 10*max + char_to_int (ch);
				SAFEREADCHAR(ch);
			} else if (ch == '*') {
				if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
				max = va_arg (args, l);
				SAFEREADCHAR(ch);
				state = DP_S_MOD;
			} else {
				state = DP_S_MOD;
			}
			break;
		case DP_S_MOD:
			switch (ch) {
			case 'h':
				cflags = DP_C_SHORT;
				SAFEREADCHAR(ch);
				break;
			case 'l':
				cflags = DP_C_LONG;
				ch = *format++;
				if (ch == 'l') {	/* It's a long long */
					cflags = DP_C_long;
					SAFEREADCHAR(ch);
				}
				break;
			case 'L':
				cflags = DP_C_LDOUBLE;
				SAFEREADCHAR(ch);
				break;
			default:
				break;
			}
			state = DP_S_CONV;
			break;
		case DP_S_CONV:
			switch (ch) {
			case 'd':
			case 'i':
				if (cflags == DP_C_SHORT) 
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, l);
				}
				else if (cflags == DP_C_LONG)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, l);
				}
				else if (cflags == DP_C_long)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, l);
				}
				else
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, l);
				}
				fmtint (buffer, &currlen, maxlen, fvalue, 10, min, max, flags);
				break;
			case 'o':
				flags |= DP_F_UNSIGNED;
				if (cflags == DP_C_SHORT)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, ul);
				}
				else if (cflags == DP_C_LONG)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = (long)va_arg (args, ul);
				}
				else if (cflags == DP_C_long)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = (long)va_arg (args, ul);
				}
				else
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = (long)va_arg (args, ul);
				}
				fmtint (buffer, &currlen, maxlen, fvalue, 8, min, max, flags);
				break;
			case 'u':
				flags |= DP_F_UNSIGNED;
				if (cflags == DP_C_SHORT)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, ul);
				}
				else if (cflags == DP_C_LONG)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = (long)va_arg (args, ul);
				}
				else if (cflags == DP_C_long)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = (long)va_arg (args, ul);
				}
				else
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = (long)va_arg (args, ul);
				}
				fmtint (buffer, &currlen, maxlen, fvalue, 10, min, max, flags);
				break;
			case 'X':
				flags |= DP_F_UP;
			case 'x':
				flags |= DP_F_UNSIGNED;
				if (cflags == DP_C_SHORT)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, ul);
				}
				else if (cflags == DP_C_LONG)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = (long)va_arg (args, ul);
				}
				else if (cflags == DP_C_long)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = (long)va_arg (args, ul);
				}
				else
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = (long)va_arg (args, ul);
				}
				fmtint (buffer, &currlen, maxlen, fvalue, 16, min, max, flags);
				break;
			case 'f':
				if (cflags == DP_C_LDOUBLE)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, f);
				}
				else
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, f);
				}
				/* um, floating point? */
				fmtfp (buffer, &currlen, maxlen, fvalue, min, max, flags);
				break;
			case 'E':
				flags |= DP_F_UP;
			case 'e':
				if (cflags == DP_C_LDOUBLE)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, f);
				}
				else
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, f);
				}
				fmtfp (buffer, &currlen, maxlen, fvalue, min, max, flags);
				break;
			case 'G':
				flags |= DP_F_UP;
			case 'g':
				if (cflags == DP_C_LDOUBLE)
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, f);
				}
				else
				{
					if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
					fvalue = va_arg (args, f);
				}
				fmtfp (buffer, &currlen, maxlen, fvalue, min, max, flags);
				break;
			case 'c':
				dopr_outch (buffer, &currlen, maxlen, va_arg (args, l));
				break;
			case 's':
				if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
				ptr = va_arg (args, ptr);

				if(!ptr)
					strfvalue = "<null>";
				else
					strfvalue = thread->evm_vmGetPointer(thread, ptr);
				if(!strfvalue)
				{
					*result = EVM_RTERR_ACCESS_VIOLATION;
					return currlen;
				}

				if (max == -1) {
					max = strlen(strfvalue);
				}
				if (min > 0 && max >= 0 && min > max) max = min;
				if(!fmtstr (buffer, thread->evm_vmGetPointerCapacity(thread, ptr), &currlen, maxlen, strfvalue, flags, min, max))
				{
					*result = EVM_RTERR_ACCESS_VIOLATION;
					return currlen;
				}
				break;
			case 'p':
				if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }
				fmtint (buffer, &currlen, maxlen, va_arg(args, l), 16, min, max, flags);
				break;
			case 'n':

#define COPY_CHARS_WRITTEN(type, mask, retrieval) \
	type *num;\
	if(args == maxarg) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }\
	ptr = va_arg (args, ptr);\
	if(!ptr) { *result = EVM_RTERR_NULL_ACCESS; return currlen; }\
	if(ptr & mask) { *result = EVM_RTERR_ALIGNMENT_ERROR; return currlen; }\
	num = thread->evm_vmGetPointer(thread, ptr);\
	if(!num) { *result = EVM_RTERR_ACCESS_VIOLATION; return currlen; }\
	*num = currlen

				if (cflags == DP_C_SHORT) {
					COPY_CHARS_WRITTEN(short int, 1, l);
				} else if (cflags == DP_C_LONG) {
					COPY_CHARS_WRITTEN(long int, 1, l);
				} else if (cflags == DP_C_long) {
					COPY_CHARS_WRITTEN(long, 1, l);
				} else {
					COPY_CHARS_WRITTEN(int, 1, l);
				}
				break;
			case '%':
				dopr_outch (buffer, &currlen, maxlen, ch);
				break;
			case 'w':
				SAFEREADCHAR(ch);
				break;
			default:
				break;
			}
			SAFEREADCHAR(ch);
			state = DP_S_DEFAULT;
			flags = cflags = min = 0;
			max = -1;
			break;
		case DP_S_DONE:
			break;
		default:
			/* hmm? */
			break; /* some picky compilers need this */
		}
	}
	if (maxlen != 0) {
		if (currlen < maxlen - 1) 
			buffer[currlen] = '\0';
		else if (maxlen > 0) 
			buffer[maxlen - 1] = '\0';
	}
	
	return currlen;
}

static int fmtstr(char *buffer, evm_u32 legalLength, size_t *currlen, size_t maxlen,
		    char *fvalue, int flags, int min, int max)
{
	int padlen, strln;     /* amount to pad */
	int cnt = 0;

#ifdef DEBUG_SNPRINTF
	printf("fmtstr min=%d max=%d s=[%s]\n", min, max, fvalue);
#endif
	if (fvalue == 0) {
		fvalue = "<NULL>";
		strln = 6;
	}
	else
	{
		for(strln=0;;)
		{
			if(strln >= legalLength)
				return 0;
			if(fvalue[strln])
				strln++;
			else
				break;
		}
	}

	padlen = min - strln;
	if (padlen < 0) 
		padlen = 0;
	if (flags & DP_F_MINUS) 
		padlen = -padlen; /* Left Justify */
	
	while ((padlen > 0) && (cnt < max)) {
		dopr_outch (buffer, currlen, maxlen, ' ');
		--padlen;
		++cnt;
	}
	while (*fvalue && (cnt < max)) {
		dopr_outch (buffer, currlen, maxlen, *fvalue++);
		++cnt;
	}
	while ((padlen < 0) && (cnt < max)) {
		dopr_outch (buffer, currlen, maxlen, ' ');
		++padlen;
		++cnt;
	}

	return 1;
}

/* Have to handle DP_F_NUM (ie 0x and 0 alternates) */

static void fmtint(char *buffer, size_t *currlen, size_t maxlen,
		    long fvalue, int base, int min, int max, int flags)
{
	int signfvalue = 0;
	unsigned long ufvalue;
	char convert[20];
	int place = 0;
	int spadlen = 0; /* amount to space pad */
	int zpadlen = 0; /* amount to zero pad */
	int caps = 0;
	
	if (max < 0)
		max = 0;
	
	ufvalue = fvalue;
	
	if(!(flags & DP_F_UNSIGNED)) {
		if( fvalue < 0 ) {
			signfvalue = '-';
			ufvalue = -fvalue;
		} else {
			if (flags & DP_F_PLUS)  /* Do a sign (+/i) */
				signfvalue = '+';
			else if (flags & DP_F_SPACE)
				signfvalue = ' ';
		}
	}
  
	if (flags & DP_F_UP) caps = 1; /* Should characters be upper case? */

	do {
		convert[place++] =
			(caps? "0123456789ABCDEF":"0123456789abcdef")
			[ufvalue % (unsigned)base  ];
		ufvalue = (ufvalue / (unsigned)base );
	} while(ufvalue && (place < 20));
	if (place == 20) place--;
	convert[place] = 0;

	zpadlen = max - place;
	spadlen = min - MAX (max, place) - (signfvalue ? 1 : 0);
	if (zpadlen < 0) zpadlen = 0;
	if (spadlen < 0) spadlen = 0;
	if (flags & DP_F_ZERO) {
		zpadlen = MAX(zpadlen, spadlen);
		spadlen = 0;
	}
	if (flags & DP_F_MINUS) 
		spadlen = -spadlen; /* Left Justifty */

#ifdef DEBUG_SNPRINTF
	printf("zpad: %d, spad: %d, min: %d, max: %d, place: %d\n",
	       zpadlen, spadlen, min, max, place);
#endif

	/* Spaces */
	while (spadlen > 0) {
		dopr_outch (buffer, currlen, maxlen, ' ');
		--spadlen;
	}

	/* Sign */
	if (signfvalue) 
		dopr_outch (buffer, currlen, maxlen, signfvalue);

	/* Zeros */
	if (zpadlen > 0) {
		while (zpadlen > 0) {
			dopr_outch (buffer, currlen, maxlen, '0');
			--zpadlen;
		}
	}

	/* Digits */
	while (place > 0) 
		dopr_outch (buffer, currlen, maxlen, convert[--place]);
  
	/* Left Justified spaces */
	while (spadlen < 0) {
		dopr_outch (buffer, currlen, maxlen, ' ');
		++spadlen;
	}
}

static LDOUBLE abs_val(LDOUBLE fvalue)
{
	LDOUBLE result = fvalue;

	if (fvalue < 0)
		result = -fvalue;
	
	return result;
}

static LDOUBLE POW10(int exp)
{
	LDOUBLE result = 1;
	
	while (exp) {
		result *= 10;
		exp--;
	}
  
	return result;
}

static long ROUND(LDOUBLE fvalue)
{
	long intpart;

	intpart = (long)fvalue;
	fvalue = fvalue - intpart;
	if (fvalue >= 0.5) intpart++;
	
	return intpart;
}

/* a replacement for modf that doesn't need the math library. Should
   be portable, but slow */
static double my_modf(double x0, double *iptr)
{
	int i;
	long l;
	double x = x0;
	double f = 1.0;

	for (i=0;i<100;i++) {
		l = (long)x;
		if (l <= (x+1) && l >= (x-1)) break;
		x *= 0.1;
		f *= 10.0;
	}

	if (i == 100) {
		/* yikes! the number is beyond what we can handle. What do we do? */
		(*iptr) = 0;
		return 0;
	}

	if (i != 0) {
		double i2;
		double ret;

		ret = my_modf(x0-l*f, &i2);
		(*iptr) = l*f + i2;
		return ret;
	} 

	(*iptr) = l;
	return x - (*iptr);
}


static void fmtfp (char *buffer, size_t *currlen, size_t maxlen,
		   LDOUBLE fvalue, int min, int max, int flags)
{
	int signfvalue = 0;
	double ufvalue;
	char iconvert[311];
	char fconvert[311];
	int iplace = 0;
	int fplace = 0;
	int padlen = 0; /* amount to pad */
	int zpadlen = 0; 
	int caps = 0;
	int idx;
	double intpart;
	double fracpart;
	double temp;
  
	/* 
	 * AIX manpage says the default is 0, but Solaris says the default
	 * is 6, and sprintf on AIX defaults to 6
	 */
	if (max < 0)
		max = 6;

	ufvalue = abs_val (fvalue);

	if (fvalue < 0) {
		signfvalue = '-';
	} else {
		if (flags & DP_F_PLUS) { /* Do a sign (+/i) */
			signfvalue = '+';
		} else {
			if (flags & DP_F_SPACE)
				signfvalue = ' ';
		}
	}

#if 0
	if (flags & DP_F_UP) caps = 1; /* Should characters be upper case? */
#endif

#if 0
	 if (max == 0) ufvalue += 0.5; /* if max = 0 we must round */
#endif

	/* 
	 * Sorry, we only support 16 digits past the decimal because of our 
	 * conversion method
	 */
	if (max > 16)
		max = 16;

	/* We "cheat" by converting the fractional part to integer by
	 * multiplying by a factor of 10
	 */

	temp = ufvalue;
	my_modf(temp, &intpart);

	fracpart = ROUND((POW10(max)) * (ufvalue - intpart));
	
	if (fracpart >= POW10(max)) {
		intpart++;
		fracpart -= POW10(max);
	}


	/* Convert integer part */
	do {
		temp = intpart*0.1;
		my_modf(temp, &intpart);
		idx = (int) ((temp -intpart +0.05)* 10.0);
		/* idx = (int) (((double)(temp*0.1) -intpart +0.05) *10.0); */
		/* printf ("%llf, %f, %x\n", temp, intpart, idx); */
		iconvert[iplace++] =
			(caps? "0123456789ABCDEF":"0123456789abcdef")[idx];
	} while (intpart && (iplace < 311));
	if (iplace == 311) iplace--;
	iconvert[iplace] = 0;

	/* Convert fractional part */
	if (fracpart)
	{
		do {
			temp = fracpart*0.1;
			my_modf(temp, &fracpart);
			idx = (int) ((temp -fracpart +0.05)* 10.0);
			/* idx = (int) ((((temp/10) -fracpart) +0.05) *10); */
			/* printf ("%lf, %lf, %ld\n", temp, fracpart, idx ); */
			fconvert[fplace++] =
			(caps? "0123456789ABCDEF":"0123456789abcdef")[idx];
		} while(fracpart && (fplace < 311));
		if (fplace == 311) fplace--;
	}
	fconvert[fplace] = 0;
  
	/* -1 for decimal point, another -1 if we are printing a sign */
	padlen = min - iplace - max - 1 - ((signfvalue) ? 1 : 0); 
	zpadlen = max - fplace;
	if (zpadlen < 0) zpadlen = 0;
	if (padlen < 0) 
		padlen = 0;
	if (flags & DP_F_MINUS) 
		padlen = -padlen; /* Left Justifty */
	
	if ((flags & DP_F_ZERO) && (padlen > 0)) {
		if (signfvalue) {
			dopr_outch (buffer, currlen, maxlen, signfvalue);
			--padlen;
			signfvalue = 0;
		}
		while (padlen > 0) {
			dopr_outch (buffer, currlen, maxlen, '0');
			--padlen;
		}
	}
	while (padlen > 0) {
		dopr_outch (buffer, currlen, maxlen, ' ');
		--padlen;
	}
	if (signfvalue) 
		dopr_outch (buffer, currlen, maxlen, signfvalue);
	
	while (iplace > 0) 
		dopr_outch (buffer, currlen, maxlen, iconvert[--iplace]);

#ifdef DEBUG_SNPRINTF
	printf("fmtfp: fplace=%d zpadlen=%d\n", fplace, zpadlen);
#endif

	/*
	 * Decimal point.  This should probably use locale to find the correct
	 * char to print out.
	 */
	if (max > 0) {
		dopr_outch (buffer, currlen, maxlen, '.');
		
		while (zpadlen > 0) {
			dopr_outch (buffer, currlen, maxlen, '0');
			--zpadlen;
		}

		while (fplace > 0) 
			dopr_outch (buffer, currlen, maxlen, fconvert[--fplace]);
	}

	while (padlen < 0) {
		dopr_outch (buffer, currlen, maxlen, ' ');
		++padlen;
	}
}

static void dopr_outch(char *buffer, size_t *currlen, size_t maxlen, char c)
{
	if (*currlen < maxlen) {
		buffer[(*currlen)] = c;
	}
	(*currlen)++;
}

static evm_result_t generic_vsnprintf(evm_thread_t *t, evm_val32_t *returnfvalue, evm_val32_t *args, evm_u32 numArgs, evm_val32_t *vargs, evm_u32 numVargs)
{
	char *buf;
	char *fmt;
	evm_u32 maxlen;
	evm_result_t result;

	if(!args[0].ptr)
		return EVM_RTERR_NULL_ACCESS;

	buf = t->evm_vmGetPointer(t, args[0].ptr);
	if(!buf)
		return EVM_RTERR_ACCESS_VIOLATION;

	fmt = t->evm_vmGetPointer(t, args[2].ptr);
	if(!fmt)
		return EVM_RTERR_ACCESS_VIOLATION;

	maxlen = t->evm_vmGetPointerCapacity(t, args[0].ptr);
	if(maxlen > args[1].ul)
		maxlen = args[1].ul;

	returnfvalue->l = dopr(t, buf, maxlen, fmt, &result, t->evm_vmGetPointerCapacity(t, args[2].ptr), vargs, vargs + numVargs);

	return result;
}

evm_result_t libc_snprintf(evm_thread_t *t, evm_val32_t *returnfvalue, evm_val32_t *args, evm_u32 numArgs)
{
	if(numArgs < 3)
		return EVM_RTERR_ACCESS_VIOLATION;
	return generic_vsnprintf(t, returnfvalue, args, numArgs, args+3, numArgs-3);
}

evm_result_t libc_vsnprintf(evm_thread_t *t, evm_val32_t *returnfvalue, evm_val32_t *args, evm_u32 numArgs)
{
	char *buf;
	char *fmt;
	evm_result_t result;
	evm_val32_t *vargs;

	// Params: str, size, format, args list
	if(numArgs < 4)
		return EVM_RTERR_ACCESS_VIOLATION;

	if(!args[3].ptr)
		return EVM_RTERR_NULL_ACCESS;

	vargs = t->evm_vmGetPointer(t, args[3].ptr);
	if(!vargs)
		return EVM_RTERR_NULL_ACCESS;

	return generic_vsnprintf(t, returnfvalue, args, numArgs, vargs, t->evm_vmGetPointerCapacity(t, args[3].ptr) / 4);
}


#if 0
int main (void)
{
	char buf1[1024];
	char buf2[1024];
	char *fp_fmt[] = {
		"%1.1f",
		"%-1.5f",
		"%1.5f",
		"%123.9f",
		"%10.5f",
		"% 10.5f",
		"%+22.9f",
		"%+4.9f",
		"%01.3f",
		"%4f",
		"%3.1f",
		"%3.2f",
		"%.0f",
		"%f",
		"-16.16f",
		NULL
	};
	double fp_nums[] = { 6442452944.1234, -1.5, 134.21, 91340.2, 341.1234, 203.9, 0.96, 0.996, 
			     0.9996, 1.996, 4.136, 5.030201, 0.00205,
			     /* END LIST */ 0};
	char *int_fmt[] = {
		"%-1.5d",
		"%1.5d",
		"%123.9d",
		"%5.5d",
		"%10.5d",
		"% 10.5d",
		"%+22.33d",
		"%01.3d",
		"%4d",
		"%d",
		NULL
	};
	long int_nums[] = { -1, 134, 91340, 341, 0203, 0};
	char *str_fmt[] = {
		"10.5s",
		"5.10s",
		"10.1s",
		"0.10s",
		"10.0s",
		"1.10s",
		"%s",
		"%.1s",
		"%.10s",
		"%10s",
		NULL
	};
	char *str_vals[] = {"hello", "a", "", "a longer string", NULL};
	int x, y;
	int fail = 0;
	int num = 0;

	printf ("Testing snprintf format codes against system sprintf...\n");

	for (x = 0; fp_fmt[x] ; x++) {
		for (y = 0; fp_nums[y] != 0 ; y++) {
			int l1 = snprintf(NULL, 0, fp_fmt[x], fp_nums[y]);
			int l2 = snprintf(buf1, sizeof(buf1), fp_fmt[x], fp_nums[y]);
			sprintf (buf2, fp_fmt[x], fp_nums[y]);
			if (strcmp (buf1, buf2)) {
				printf("snprintf doesn't match Format: %s\n\tsnprintf = [%s]\n\t sprintf = [%s]\n", 
				       fp_fmt[x], buf1, buf2);
				fail++;
			}
			if (l1 != l2) {
				printf("snprintf l1 != l2 (%d %d) %s\n", l1, l2, fp_fmt[x]);
				fail++;
			}
			num++;
		}
	}

	for (x = 0; int_fmt[x] ; x++) {
		for (y = 0; int_nums[y] != 0 ; y++) {
			int l1 = snprintf(NULL, 0, int_fmt[x], int_nums[y]);
			int l2 = snprintf(buf1, sizeof(buf1), int_fmt[x], int_nums[y]);
			sprintf (buf2, int_fmt[x], int_nums[y]);
			if (strcmp (buf1, buf2)) {
				printf("snprintf doesn't match Format: %s\n\tsnprintf = [%s]\n\t sprintf = [%s]\n", 
				       int_fmt[x], buf1, buf2);
				fail++;
			}
			if (l1 != l2) {
				printf("snprintf l1 != l2 (%d %d) %s\n", l1, l2, int_fmt[x]);
				fail++;
			}
			num++;
		}
	}

	for (x = 0; str_fmt[x] ; x++) {
		for (y = 0; str_vals[y] != 0 ; y++) {
			int l1 = snprintf(NULL, 0, str_fmt[x], str_vals[y]);
			int l2 = snprintf(buf1, sizeof(buf1), str_fmt[x], str_vals[y]);
			sprintf (buf2, str_fmt[x], str_vals[y]);
			if (strcmp (buf1, buf2)) {
				printf("snprintf doesn't match Format: %s\n\tsnprintf = [%s]\n\t sprintf = [%s]\n", 
				       str_fmt[x], buf1, buf2);
				fail++;
			}
			if (l1 != l2) {
				printf("snprintf l1 != l2 (%d %d) %s\n", l1, l2, str_fmt[x]);
				fail++;
			}
			num++;
		}
	}

	printf ("%d tests failed out of %d.\n", fail, num);

	printf("seeing how many digits we support\n");
	{
		double v0 = 0.12345678901234567890123456789012345678901;
		for (x=0; x<100; x++) {
			double p = pow(10, x); 
			double r = v0*p;
			snprintf(buf1, sizeof(buf1), "%1.1f", r);
			sprintf(buf2,                "%1.1f", r);
			if (strcmp(buf1, buf2)) {
				printf("we seem to support %d digits\n", x-1);
				break;
			}
		}
	}

	return 0;
}
#endif /* TEST_SNPRINTF */
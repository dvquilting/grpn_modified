#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "real.h"
#include "mode.h"
#include "constant.h"
#include "lcd.h"

#define RND MPFR_RNDN

static void set_status(Real *a){
   if(mpfr_nan_p(a->num)){ a->ok=REAL_NAN; mpfr_set_zero(a->num,0); }
   else if(mpfr_inf_p(a->num)){ a->ok=REAL_INF; mpfr_set_zero(a->num,0); }
   else a->ok=REAL_OK;
}
static void copy_status(Real *to,const Real *from){
   to->ok=from->ok;
   if(from->ok==REAL_OK) mpfr_set(to->num,from->num,RND); else mpfr_set_zero(to->num,0);
}
static void invalid(Real *a,char status){ a->ok=status; mpfr_set_zero(a->num,0); }

Real *newReal(void){
   Real *p=malloc(sizeof(*p)); if(!p){perror("malloc");exit(EXIT_FAILURE);}
   mpfr_init2(p->num,REAL_PRECISION_BITS); mpfr_set_zero(p->num,0); p->ok=REAL_OK; return p;
}
void freeReal(Real *a){ if(a){mpfr_clear(a->num);free(a);} }
double setDoubleReal(Real *a){
   if(!a||a->ok!=REAL_OK){fprintf(stderr,"setDoubleReal: value is not finite\n");return 0.0;}
   return mpfr_get_d(a->num,RND);
}
Real *setRealDouble(Real *a,double d){ a->ok=REAL_OK;mpfr_set_d(a->num,d,RND);set_status(a);return a; }
Real *setRealString(Real *a,const char *s,int base,char **end){
   char *stop=NULL; a->ok=REAL_OK; mpfr_strtofr(a->num,s,&stop,base,RND);
   if(end) *end=stop;
   if(stop==s) invalid(a,REAL_NAN);
   else set_status(a);
   return a;
}
Real *setRealReal(Real *a,Real *b){if(a&&b)copy_status(a,b);return a;}

Real *toRadixReal(Real *a){return getRadixMode()==DEGREES?mulReal(a,real180Pi):setRealReal(newReal(),a);}
Real *fromRadixReal(Real *a){return getRadixMode()==DEGREES?divReal(a,real180Pi):setRealReal(newReal(),a);}

static char *print_integer_base(Real *a,int base,const char *prefix){
   mpz_t z; char *digits,*out,*magnitude; size_t size; int negative;
   mpz_init(z); mpfr_get_z(z,a->num,MPFR_RNDZ); negative=mpz_sgn(z)<0;
   size=mpz_sizeinbase(z,base)+2; digits=malloc(size); if(!digits){perror("malloc");exit(EXIT_FAILURE);}
   mpz_get_str(digits,base,z); magnitude=negative?digits+1:digits;
   size=(negative?1:0)+strlen(prefix)+strlen(magnitude)+1; out=malloc(size);
   if(!out){perror("malloc");exit(EXIT_FAILURE);}
   snprintf(out,size,"%s%s%s",negative?"-":"",prefix,magnitude);
   free(digits);mpz_clear(z);return out;
}
static char *print_fraction_base(Real *a,int base,const char *prefix){
   mpfr_exp_t exponent; char *digits=mpfr_get_str(NULL,&exponent,base,0,a->num,RND);
   char *magnitude=digits[0]=='-'?digits+1:digits, *out; int negative=digits[0]=='-'; size_t size;
   size=(negative?1:0)+strlen(prefix)+strlen(magnitude)+32;
   out=malloc(size);if(!out){perror("malloc");exit(EXIT_FAILURE);}
   if(strlen(magnitude)>1)
      snprintf(out,size,"%s%s%c.%s@%ld",negative?"-":"",prefix,magnitude[0],magnitude+1,(long)exponent-1);
   else
      snprintf(out,size,"%s%s%s@%ld",negative?"-":"",prefix,magnitude,(long)exponent-1);
   mpfr_free_str(digits);return out;
}
static char *print_base(Real *a,int base,const char *prefix){
   return mpfr_integer_p(a->num)?print_integer_base(a,base,prefix):print_fraction_base(a,base,prefix);
}
static char *group_decimal(const char *number){
   const char *start=number+(*number=='-'||*number=='+' ? 1:0), *end=start;
   char *grouped, *dst; size_t integer_digits, separators, prefix;
   while(*end && *end!='.' && *end!='e' && *end!='E') end++;
   integer_digits=(size_t)(end-start);
   if(integer_digits<=3) return strdup(number);
   separators=(integer_digits-1)/3; prefix=(size_t)(start-number);
   grouped=malloc(strlen(number)+separators+1); if(!grouped){perror("malloc");exit(EXIT_FAILURE);}
   dst=grouped; if(prefix)*dst++=*number;
   while(start<end){
      size_t remaining=(size_t)(end-start);
      *dst++=*start++;
      if(remaining>1 && (remaining-1)%3==0) *dst++=',';
   }
   strcpy(dst,end); return grouped;
}
/* Round a to n significant decimal digits, halves away from zero (as HP
   calculators do).  Returns exactly n digits; the value is
   0.DDD x 10^(*exp10+1), i.e. *exp10 is the power of ten of the first digit.
   The value is first converted to a few digits fewer than the binary
   precision holds, so representation error (9.995 is stored as
   9.99499999...) does not hide a decimal tie; that string is then rounded
   half-up in decimal. */
static char *round_decimal(Real *a,int n,long *exp10,int *negative){
   mpfr_exp_t e; char *full,*d,*out; size_t m; int i;
   long guard=(long)(mpfr_get_prec(a->num)*0.30102999566398) - 5;   /* bits -> digits, minus guard */
   if(guard<n+1) guard=n+1;
   full=mpfr_get_str(NULL,&e,10,(size_t)guard,a->num,RND);
   *negative=full[0]=='-'; d=full+*negative; m=strlen(d);
   out=malloc((size_t)n+1); if(!out){perror("malloc");exit(EXIT_FAILURE);}
   for(i=0;i<n;i++) out[i]=(size_t)i<m?d[i]:'0';
   out[n]='\0'; *exp10=(long)e-1;
   if((size_t)n<m && d[n]>='5'){
      for(i=n-1;i>=0&&out[i]=='9';i--) out[i]='0';
      if(i>=0) out[i]++;
      else { out[0]='1'; (*exp10)++; }   /* 9.99 -> 10.0 */
   }
   mpfr_free_str(full); return out;
}

/* like printf %g: trailing zeros removed, exponent form when the
   exponent is < -4 or >= the number of digits */
static char *print_std(Real *a,int digits){
   char *d,*out,*p; long x; int negative,i,len;
   if(mpfr_zero_p(a->num)) return strdup(mpfr_signbit(a->num)?"-0":"0");
   d=round_decimal(a,digits,&x,&negative);
   len=digits+(int)(x<0?-x:x)+40; out=malloc(len); if(!out){perror("malloc");exit(EXIT_FAILURE);}
   p=out; if(negative)*p++='-';
   if(x<-4 || x>=digits){
      *p++=d[0]; *p++='.'; for(i=1;i<digits;i++) *p++=d[i];
   } else if(x<0){
      *p++='0'; *p++='.'; for(i=0;i<-x-1;i++) *p++='0'; for(i=0;i<digits;i++) *p++=d[i];
   } else {
      for(i=0;i<=x;i++) *p++=d[i];
      *p++='.'; for(;i<digits;i++) *p++=d[i];
   }
   while(p[-1]=='0') p--;                 /* strip trailing zeros ... */
   if(p[-1]=='.') p--;                    /* ... and a bare decimal point */
   if(x<-4 || x>=digits) snprintf(p,len-(p-out),"e%c%02ld",x<0?'-':'+',x<0?-x:x);
   else *p='\0';
   free(d); return out;
}

/* Engineering notation in the style of the HP 48 ENG mode: `digits`
   significant digits (trailing zeros kept), an exponent that is a multiple
   of 3, and 1 to 3 digits before the decimal point, e.g. 12.3456789012E3 */
static char *print_eng(Real *a,int digits){
   char *d,*out,*p; long exp10,engExp; int negative,intDigits,len,i;
   if(mpfr_zero_p(a->num)){
      d=malloc((size_t)digits+1); if(!d){perror("malloc");exit(EXIT_FAILURE);}
      memset(d,'0',(size_t)digits); d[digits]='\0'; exp10=0; negative=0;
   } else d=round_decimal(a,digits,&exp10,&negative);
   engExp=exp10>=0 ? exp10/3*3 : -((-exp10+2)/3)*3;
   intDigits=(int)(exp10-engExp)+1;                 /* 1, 2 or 3 */
   len=digits+intDigits+32; out=malloc(len); if(!out){perror("malloc");exit(EXIT_FAILURE);}
   p=out; if(negative)*p++='-';
   for(i=0;i<intDigits;i++) *p++= i<digits?d[i]:'0';
   if(digits>intDigits){ *p++='.'; for(i=intDigits;i<digits;i++) *p++=d[i]; }
   snprintf(p,len-(p-out),"E%ld",engExp);
   free(d); return out;
}

char *printReal(Real *a){
   char *out=NULL, *result;
   if(a->ok==REAL_NAN) return strdup("NaN");
   if(a->ok==REAL_INF) return strdup("Infinity");
   switch(getBaseMode()){
      case BINARY:return print_base(a,2,"");
      case OCTAL:return print_base(a,8,"0");
      case HEXIDECIMAL:return print_base(a,16,"0x");
      case DECIMAL:
      default:
         if(getEngNotation()) return print_eng(a,getDisplayDigits());
         out=print_std(a,getDisplayDigits());
         if(!getThousandsSeparators()) return out;
         result=group_decimal(out); free(out); return result;
   }
}

int cmpReal(Real *a,Real *b){if(a->ok==REAL_OK&&b->ok==REAL_OK)return mpfr_cmp(a->num,b->num);if(a->ok==b->ok)return 0;return 1;}
int isIntReal(Real *a){return a->ok==REAL_OK&&mpfr_integer_p(a->num);}

Real *absReal(Real *a){Real*p=newReal();p->ok=a->ok;if(a->ok==REAL_OK)mpfr_abs(p->num,a->num,RND);return p;}
Real *negReal(Real *a){Real*p=newReal();p->ok=a->ok;if(a->ok==REAL_OK)mpfr_neg(p->num,a->num,RND);return p;}
Real *negEqReal(Real *a){if(a->ok==REAL_OK)mpfr_neg(a->num,a->num,RND);return a;}
Real *invReal(Real *a){Real*p=setRealReal(newReal(),a);return invEqReal(p);}
Real *invEqReal(Real *a){if(a->ok==REAL_OK){if(mpfr_zero_p(a->num))invalid(a,REAL_NAN);else{mpfr_ui_div(a->num,1,a->num,RND);set_status(a);}}return a;}

Real *powReal(Real*a,Real*b){Real*p=setRealReal(newReal(),a);return powEqReal(p,b);}
Real *powEqReal(Real*a,Real*b){if(a->ok==REAL_OK&&b->ok==REAL_OK){mpfr_pow(a->num,a->num,b->num,RND);set_status(a);}else invalid(a,a->ok==REAL_NAN||b->ok==REAL_NAN?REAL_NAN:REAL_INF);return a;}
Real *powRealInt(Real*a,int b){Real*p=newReal();p->ok=a->ok;if(a->ok==REAL_OK){mpfr_pow_si(p->num,a->num,b,RND);set_status(p);}return p;}

static Real *unary_new(Real*a,int(*fn)(mpfr_ptr,mpfr_srcptr,mpfr_rnd_t)){Real*p=setRealReal(newReal(),a);if(p->ok==REAL_OK){fn(p->num,p->num,RND);set_status(p);}return p;}
static Real *unary_eq(Real*a,int(*fn)(mpfr_ptr,mpfr_srcptr,mpfr_rnd_t)){if(a->ok==REAL_OK){fn(a->num,a->num,RND);set_status(a);}return a;}
Real *lnReal(Real*a){return unary_new(a,mpfr_log);} Real *lnEqReal(Real*a){return unary_eq(a,mpfr_log);}
Real *logReal(Real*a){return unary_new(a,mpfr_log10);} Real *logEqReal(Real*a){return unary_eq(a,mpfr_log10);}
Real *expReal(Real*a){return unary_new(a,mpfr_exp);} Real *expEqReal(Real*a){return unary_eq(a,mpfr_exp);}
Real *asinReal(Real*a){return unary_new(a,mpfr_asin);} Real *asinEqReal(Real*a){return unary_eq(a,mpfr_asin);}
Real *acosReal(Real*a){return unary_new(a,mpfr_acos);} Real *acosEqReal(Real*a){return unary_eq(a,mpfr_acos);}
Real *atanReal(Real*a){return unary_new(a,mpfr_atan);} Real *atanEqReal(Real*a){return unary_eq(a,mpfr_atan);}
Real *sinReal(Real*a){return unary_new(a,mpfr_sin);} Real *sinEqReal(Real*a){return unary_eq(a,mpfr_sin);}
Real *cosReal(Real*a){return unary_new(a,mpfr_cos);} Real *cosEqReal(Real*a){return unary_eq(a,mpfr_cos);}
Real *tanReal(Real*a){return unary_new(a,mpfr_tan);} Real *tanEqReal(Real*a){return unary_eq(a,mpfr_tan);}
/* x! = Gamma(x+1); undefined for negative integers */
Real *factReal(Real*a){
   Real*p=setRealReal(newReal(),a);
   if(p->ok!=REAL_OK) return p;
   if(mpfr_integer_p(p->num)&&mpfr_sgn(p->num)<0){ invalid(p,REAL_NAN); return p; }
   mpfr_add_ui(p->num,p->num,1,RND); mpfr_gamma(p->num,p->num,RND); set_status(p); return p;
}
Real *atan2Real(Real*a,Real*b){Real*p=newReal();if(a->ok==REAL_OK&&b->ok==REAL_OK){mpfr_atan2(p->num,a->num,b->num,RND);set_status(p);}else invalid(p,a->ok==REAL_NAN||b->ok==REAL_NAN?REAL_NAN:REAL_INF);return p;}

static Real *binary_new(Real*a,Real*b,int(*fn)(mpfr_ptr,mpfr_srcptr,mpfr_srcptr,mpfr_rnd_t)){
   Real*p=newReal();if(a->ok==REAL_OK&&b->ok==REAL_OK){fn(p->num,a->num,b->num,RND);set_status(p);}else invalid(p,a->ok==REAL_NAN||b->ok==REAL_NAN?REAL_NAN:REAL_INF);return p;
}
static Real *binary_eq(Real*a,Real*b,int(*fn)(mpfr_ptr,mpfr_srcptr,mpfr_srcptr,mpfr_rnd_t)){
   if(a->ok==REAL_OK&&b->ok==REAL_OK){fn(a->num,a->num,b->num,RND);set_status(a);}else invalid(a,a->ok==REAL_NAN||b->ok==REAL_NAN?REAL_NAN:REAL_INF);return a;
}
Real *mulReal(Real*a,Real*b){return binary_new(a,b,mpfr_mul);} Real *mulEqReal(Real*a,Real*b){return binary_eq(a,b,mpfr_mul);}
Real *addReal(Real*a,Real*b){return binary_new(a,b,mpfr_add);} Real *addEqReal(Real*a,Real*b){return binary_eq(a,b,mpfr_add);}
Real *subReal(Real*a,Real*b){return binary_new(a,b,mpfr_sub);} Real *subEqReal(Real*a,Real*b){return binary_eq(a,b,mpfr_sub);}
Real *divReal(Real*a,Real*b){Real*p=newReal();if(a->ok==REAL_OK&&b->ok==REAL_OK&&!mpfr_zero_p(b->num)){mpfr_div(p->num,a->num,b->num,RND);set_status(p);}else invalid(p,REAL_NAN);return p;}
Real *divEqReal(Real*a,Real*b){if(a->ok==REAL_OK&&b->ok==REAL_OK&&!mpfr_zero_p(b->num)){mpfr_div(a->num,a->num,b->num,RND);set_status(a);}else invalid(a,REAL_NAN);return a;}

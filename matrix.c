/*

Copyright (C) 2002  Paul Wilkins

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/* matrix.c  by Paul Wilkins */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include "matrix.h"
#include "number.h"
#include "error.h"
#include "constant.h"

/* get the space for the data */
Number ** mallocData(Number **d, int rows, int cols){
   int size;
   Number **p;

   if(d){
      size = rows*cols*sizeof(Number *);
      if(NULL == (p=(Number **)realloc((char *)d, size))){
         perror("realloc"); exit(0);
      }
   } else {
      if(NULL == (p = (Number **)malloc(rows*cols*sizeof(Number *)))){
         perror("Malloc"); exit(0);
      }
   }
   return p;
}


/* create a new matrix */
Matrix * newMatrix(){
   Matrix *p;
   if(NULL == (p = (Matrix *)malloc(sizeof(Matrix)))){
      perror("Malloc");
      exit(0);
   }
   p->rows = 0;
   p->cols = 0;
   p->data = NULL;

   return p;
}

void freeMatrix(Matrix *a){
   int i, j;
   Number **ptr;
   if(a->data){
      for(i=0; i<a->rows; i++)
         for(j=0; j<a->cols; j++){
            ptr = a->data + (a->cols * i) + j;
            freeNumber(*ptr);
         }
      free((char *)a->data);
   }
   free((char *)a);
}

void setMatrix(Matrix *a, Number *b, int row, int col){
   int oldrows, oldcols, i, j;
   Number **ptr;
   row++; col++;
   if(a->data == NULL){
      if(row > a->rows) a->rows = row;
      if(col > a->cols) a->cols = col;
      a->data = mallocData(NULL, a->rows, a->cols);
      memset(a->data, 0, a->rows*a->cols*sizeof(Number *));
   } 
   else if(row > a->rows || col > a->cols){
      Number **olddata = a->data;
      oldrows = a->rows; oldcols = a->cols;
      if(row > a->rows) a->rows = row;
      if(col > a->cols) a->cols = col;
      a->data = calloc((size_t)a->rows*a->cols, sizeof(Number *));
      if(!a->data){ perror("calloc"); exit(EXIT_FAILURE); }
      for(i=0; i<oldrows; i++)
         for(j=0; j<oldcols; j++)
            a->data[i*a->cols+j] = olddata[i*oldcols+j];
      free(olddata);
   }

   row--; col--;
   ptr = a->data + (a->cols * row) + col;
   if(*ptr) freeNumber(*ptr);
   *ptr = setNumberNumber(newNumber(), b);
}

Matrix * setMatrixMatrix(Matrix *a, Matrix *b){
   int i, j;
   Number **ptr1, **ptr2;

   if(a == NULL || b == NULL) 
      { fprintf(stderr, "setMatrixMatrix(NULL)\n"); exit(0); }

   if(a == b) return a;
   if(a->data){
      for(i=0; i<a->rows*a->cols; i++) freeNumber(a->data[i]);
      free(a->data);
   }
   /* initalize the stuff in p */
   a->data = mallocData(NULL, b->rows, b->cols);
   a->rows = b->rows;
   a->cols = b->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = b->data + (a->cols * i) + j;
         ptr2 = a->data + (a->cols * i) + j;
         *ptr2 = setNumberNumber(newNumber(), *ptr1);
      }

   return a;
}


#define MATRIX_PRINT_SIZE 50

char * printMatrixShort(Matrix *a){
   char *c;
   if(NULL == (c=(char *)malloc(MATRIX_PRINT_SIZE)))
      { perror("Malloc"); exit(0); }

   sprintf(c, "[%d x %d Matrix]", a->rows, a->cols);
   return c;
}

char * printMatrix(Matrix *a){
   char *c;
   char *p1;
   int i, j;
   size_t used=0, capacity=32;

   c=malloc(capacity); if(!c){perror("malloc");exit(EXIT_FAILURE);} c[0]='\0';
#define APPEND_TEXT(text) do { \
   const char *append_text_=(text); size_t append_len_=strlen(append_text_); \
   if(used+append_len_+1>capacity){while(used+append_len_+1>capacity)capacity*=2; c=realloc(c,capacity);if(!c){perror("realloc");exit(EXIT_FAILURE);}} \
   memcpy(c+used,append_text_,append_len_+1);used+=append_len_; \
} while(0)
   if(a->data){
      APPEND_TEXT("[ ");
      for(i=0; i<a->rows; i++){
         for(j=0; j<a->cols; j++){
            Number *entry=*(a->data+(a->cols*i)+j);
            p1=entry?printNumberShort(entry):strdup("0");
            APPEND_TEXT(p1);
            APPEND_TEXT(" ");
            free(p1);
         }
         if(i < a->rows-1) APPEND_TEXT(" \n");
      }
      APPEND_TEXT("]");
   }
#undef APPEND_TEXT

   return c;
}

Matrix * negMatrix(Matrix *a){
   int i, j;
   Number **ptr1, **ptr2;
   Matrix *p = newMatrix();

   if(a == NULL) { fprintf(stderr, "negMatrix(NULL)\n"); exit(0); }

   /* initalize the stuff in p */
   p->data = mallocData(NULL, a->rows, a->cols);
   p->rows = a->rows;
   p->cols = a->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = a->data + (a->cols * i) + j;
         ptr2 = p->data + (a->cols * i) + j;
         *ptr2 = negNumber(*ptr1);
      }

   return p;
}

/* is this (real or complex) entry exactly zero?  NULL entries count as 0 */
static int isZeroEntry(Number *n){
   Real *re, *im;
   if(n == NULL) return 1;
   switch(n->type){
      case REAL:
         re = n->data;
         return re->ok == REAL_OK && mpfr_zero_p(re->num);
      case COMPLEX:
         re = ((Cmplx *)n->data)->re;
         im = ((Cmplx *)n->data)->im;
         return re->ok == REAL_OK && mpfr_zero_p(re->num) &&
                im->ok == REAL_OK && mpfr_zero_p(im->num);
   }
   return 0;
}

/* is this real or complex entry finite (not NaN or Infinity)? */
static int isFiniteEntry(Number *n){
   if(n == NULL) return 1;
   if(n->type == REAL) return ((Real *)n->data)->ok == REAL_OK;
   return ((Cmplx *)n->data)->re->ok == REAL_OK &&
          ((Cmplx *)n->data)->im->ok == REAL_OK;
}

/* |n| of a real or complex entry, for choosing pivots (0 for zero) */
static Real *entryMagnitude(Number *n){
   if(isZeroEntry(n)) return setRealReal(newReal(), realZero);
   return n->type == REAL ? absReal(n->data) : absCmplx(n->data);
}

/* A pivot no bigger than this fraction of the largest entry is treated as
 * zero: values carry about 77 digits, so a singular matrix leaves pivots
 * of rounding residue around 1e-77 of the entries' size.  Matrices whose
 * condition number exceeds about 1e70 are thus also reported singular. */
#define INV_PIVOT_TOLERANCE "1e-70"

/* Inverse by Gauss-Jordan elimination with partial pivoting on the
 * augmented matrix [a | I].  Entries may be real or complex. */
Matrix * invMatrix(Matrix *a){
   int n, w, i, j, r, col, pivot;
   Number **m, **row, *p, *f, *t, *x;
   Real *best, *mag, *tol;
   Matrix *inv;

   if(a == NULL) { fprintf(stderr, "invMatrix(NULL)\n"); exit(0); }

   n = a->rows;
   if(n == 0 || n != a->cols || a->data == NULL){
      sprintf(getStringError(), "Inv Error: Invalid dimension.");
      return NULL;
   }
   for(i=0; i<n*n; i++){
      if(a->data[i] && a->data[i]->type == MATRIX){
         invalidTyprError("Inv");
         return NULL;
      }
      if(!isFiniteEntry(a->data[i])){
         sprintf(getStringError(), "Inv Error: Non-finite entry.");
         return NULL;
      }
   }

   /* tol = largest |entry| * INV_PIVOT_TOLERANCE */
   tol = setRealReal(newReal(), realZero);
   for(i=0; i<n*n; i++){
      mag = entryMagnitude(a->data[i]);
      if(cmpReal(mag, tol) > 0){ freeReal(tol); tol = mag; }
      else freeReal(mag);
   }
   mag = setRealString(newReal(), INV_PIVOT_TOLERANCE, 10, NULL);
   mulEqReal(tol, mag);
   freeReal(mag);

   /* m = [a | I], an n x 2n array of numbers we own */
   w = 2*n;
   m = mallocData(NULL, n, w);
   for(i=0; i<n; i++){
      for(j=0; j<n; j++){
         x = a->data[n*i + j];
         m[w*i + j] = x ? setNumberNumber(newNumber(), x)
                        : setNumberReal(newNumber(), realZero);
         m[w*i + n + j] = setNumberReal(newNumber(), i == j ? realOne : realZero);
      }
   }

   for(col=0; col<n; col++){
      /* pick the largest entry in this column as the pivot; if even
       * that is within rounding residue of zero, the matrix is singular */
      pivot = -1;
      best = NULL;
      for(r=col; r<n; r++){
         mag = entryMagnitude(m[w*r + col]);
         if(mag->ok == REAL_OK && cmpReal(mag, tol) > 0 &&
            (best == NULL || cmpReal(mag, best) > 0)){
            if(best) freeReal(best);
            best = mag;
            pivot = r;
         } else {
            freeReal(mag);
         }
      }
      if(best) freeReal(best);
      if(pivot < 0){
         sprintf(getStringError(), "Inv Error: Singular matrix.");
         for(i=0; i<n*w; i++) freeNumber(m[i]);
         free(m);
         freeReal(tol);
         return NULL;
      }

      /* swap the pivot row into place */
      if(pivot != col){
         for(j=0; j<w; j++){
            t = m[w*col + j];
            m[w*col + j] = m[w*pivot + j];
            m[w*pivot + j] = t;
         }
      }

      /* scale the pivot row so the pivot is 1 */
      row = m + w*col;
      p = setNumberNumber(newNumber(), row[col]);
      for(j=0; j<w; j++){
         x = divNumber(row[j], p);
         freeNumber(row[j]);
         row[j] = x;
      }
      freeNumber(p);

      /* clear this column from every other row */
      for(r=0; r<n; r++){
         if(r == col || isZeroEntry(m[w*r + col])) continue;
         f = setNumberNumber(newNumber(), m[w*r + col]);
         for(j=0; j<w; j++){
            t = mulNumber(f, row[j]);
            x = subNumber(m[w*r + j], t);
            freeNumber(t);
            freeNumber(m[w*r + j]);
            m[w*r + j] = x;
         }
         freeNumber(f);
      }
   }

   freeReal(tol);

   /* the right half is the inverse */
   inv = newMatrix();
   inv->rows = inv->cols = n;
   inv->data = mallocData(NULL, n, n);
   for(i=0; i<n; i++){
      for(j=0; j<n; j++){
         inv->data[n*i + j] = m[w*i + n + j];
         freeNumber(m[w*i + j]);
      }
   }
   free(m);
   return inv;
}

/***************** MULTIPLY *******************/

/* multiply 2 Matrixes  */
Matrix * mulMatrix(Matrix *a, Matrix *b){
   int i, j, k;
   Matrix *p;
   Number *n1, *n2, *nsum;
   Number **ptr1, **ptr2, **ptr3;

   if(a == NULL || b == NULL) 
      { fprintf(stderr, "mulMatrix(NULL)\n"); exit(0); }

   if(a->cols != b->rows){
      sprintf(getStringError(), "* Error: Invalid dimension.");
      return NULL;
   }

   /* initalize the stuff in p */
   p = newMatrix();
   p->data = mallocData(NULL, a->rows, b->cols);
   p->rows = a->rows;
   p->cols = b->cols;

   /* p[i][j] = sum t=1 to k  of a[i][t]*b[t][j] */
   for(i=0; i<a->rows; i++){
      for(j=0; j<b->cols; j++){
         ptr3 = p->data + (p->cols * i) + j;
         nsum = setNumberReal(newNumber(), realZero);
         for(k=0; k<a->cols; k++){
            ptr1 = a->data + (a->cols * i) + k;
            ptr2 = b->data + (b->cols * k) + j;
            n1 = mulNumber(*ptr1, *ptr2);
            n2 = nsum;
            nsum = addNumber(n2, n1);
            freeNumber(n1);
            freeNumber(n2);
            
         }
         *ptr3 = nsum;
      }
   }

   return p;
}

   
/* multiply a Matrix and a Cmplx */
Matrix * mulMatrixCmplx(Matrix *a, Cmplx *b){
   int i, j;
   Number *n1;
   Number **ptr1, **ptr2;
   Matrix *p = newMatrix();

   if(a == NULL) { fprintf(stderr, "mulMatrixCmplx(NULL)\n"); exit(0); }

   /* initalize the stuff in p */
   p->data = mallocData(NULL, a->rows, a->cols);
   p->rows = a->rows;
   p->cols = a->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = a->data + (a->cols * i) + j;
         ptr2 = p->data + (a->cols * i) + j;
         *ptr2 = mulNumber(*ptr1, (n1=setNumberCmplx(newNumber(), b)));
         freeNumber(n1);
      }

   return p;
}

/* multiply a Matrix and a Real */
Matrix * mulMatrixReal(Matrix *a, Real *b){
   int i, j;
   Number *n1;
   Number **ptr1, **ptr2;
   Matrix *p = newMatrix();

   if(a == NULL) { fprintf(stderr, "mulMatrixReal(NULL)\n"); exit(0); }

   /* initalize the stuff in p */
   p->data = mallocData(NULL, a->rows, a->cols);
   p->rows = a->rows;
   p->cols = a->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = a->data + (a->cols * i) + j;
         ptr2 = p->data + (a->cols * i) + j;
         *ptr2 = mulNumber(*ptr1, (n1=setNumberReal(newNumber(), b)));
         freeNumber(n1);
      }

   return p;
}


/***************** DIVIDE *******************/

/* divide 2 Matrixes  */
Matrix * divMatrix(Matrix *a, Matrix *b){
   (void)a; (void)b;
   invalidTyprError("/");
   return NULL;
}

/* divide a Cmplx by a Matrix */
Matrix * divCmplxMatrix(Cmplx *a, Matrix *b){
   (void)a; (void)b;
   invalidTyprError("/");
   return NULL;
}

/* divide a Cmplx by a Matrix */
Matrix * divRealMatrix(Real *a, Matrix *b){
   (void)a; (void)b;
   invalidTyprError("/");
   return NULL;
}

/* divide a Matrix by a Cmplx */
Matrix * divMatrixCmplx(Matrix *a, Cmplx *b){
   int i, j;
   Number *n1;
   Number **ptr1, **ptr2;
   Matrix *p = newMatrix();

   if(a == NULL) { fprintf(stderr, "divMatrixCmplx(NULL)\n"); exit(0); }

   /* initalize the stuff in p */
   p->data = mallocData(NULL, a->rows, a->cols);
   p->rows = a->rows;
   p->cols = a->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = a->data + (a->cols * i) + j;
         ptr2 = p->data + (a->cols * i) + j;
         *ptr2 = divNumber(*ptr1, (n1=setNumberCmplx(newNumber(), b)));
         freeNumber(n1);
      }

   return p;
}

/* divide a Matrix by a Real */
Matrix * divMatrixReal(Matrix *a, Real *b){
   int i, j;
   Number *n1;
   Number **ptr1, **ptr2;
   Matrix *p = newMatrix();

   if(a == NULL) { fprintf(stderr, "divMatrixReal(NULL)\n"); exit(0); }

   /* initalize the stuff in p */
   p->data = mallocData(NULL, a->rows, a->cols);
   p->rows = a->rows;
   p->cols = a->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = a->data + (a->cols * i) + j;
         ptr2 = p->data + (a->cols * i) + j;
         *ptr2 = divNumber(*ptr1, (n1=setNumberReal(newNumber(), b)));
         freeNumber(n1);
      }

   return p;
}



/***************** ADD *******************/

/* add 2 Matrixes  */
Matrix * addMatrix(Matrix *a, Matrix *b){
   int i, j;
   Number **ptr1, **ptr2, **ptr3;
   Matrix *p;

   if(a == NULL || b == NULL) 
      { fprintf(stderr, "addMatrix(NULL)\n"); exit(0); }

   if(a->rows != b->rows || a->cols != b->cols){
      sprintf(getStringError(), "+ Error: Invalid dimension.");
      return NULL;
   }

   /* initalize the stuff in p */
   p = newMatrix();
   p->data = mallocData(NULL, a->rows, a->cols);
   p->rows = a->rows;
   p->cols = a->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = a->data + (a->cols * i) + j;
         ptr2 = b->data + (a->cols * i) + j;
         ptr3 = p->data + (a->cols * i) + j;
         *ptr3 = addNumber(*ptr1, *ptr2);
      }

   return p;
}

/* add a Matrix and a Cmplx */
Matrix * addMatrixCmplx(Matrix *a, Cmplx *b){
   int i, j;
   Number *n1;
   Number **ptr1, **ptr2;
   Matrix *p = newMatrix();

   if(a == NULL) { fprintf(stderr, "addMatrixCmplx(NULL)\n"); exit(0); }

   /* initalize the stuff in p */
   p->data = mallocData(NULL, a->rows, a->cols);
   p->rows = a->rows;
   p->cols = a->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = a->data + (a->cols * i) + j;
         ptr2 = p->data + (a->cols * i) + j;
         *ptr2 = addNumber(*ptr1, (n1=setNumberCmplx(newNumber(), b)));
         freeNumber(n1);
      }

   return p;
}

/* add a Matrix and a Real */
Matrix * addMatrixReal(Matrix *a, Real *b){
   int i, j;
   Number *n1;
   Number **ptr1, **ptr2;
   Matrix *p = newMatrix();

   if(a == NULL) { fprintf(stderr, "addMatrixReal(NULL)\n"); exit(0); }

   /* initalize the stuff in p */
   p->data = mallocData(NULL, a->rows, a->cols);
   p->rows = a->rows;
   p->cols = a->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = a->data + (a->cols * i) + j;
         ptr2 = p->data + (a->cols * i) + j;
         *ptr2 = addNumber(*ptr1, (n1=setNumberReal(newNumber(), b)));
         freeNumber(n1);
      }

   return p;
}



/***************** SUBTRACT *******************/

/* subtract 2 Matrixes  */
Matrix * subMatrix(Matrix *a, Matrix *b){
   int i, j;
   Number **ptr1, **ptr2, **ptr3;
   Matrix *p;

   if(a == NULL || b == NULL) 
      { fprintf(stderr, "subMatrix(NULL)\n"); exit(0); }

   if(a->rows != b->rows || a->cols != b->cols){
      sprintf(getStringError(), "- Error: Invalid dimension.");
      return NULL;
   }

   /* initalize the stuff in p */
   p = newMatrix();
   p->data = mallocData(NULL, a->rows, a->cols);
   p->rows = a->rows;
   p->cols = a->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = a->data + (a->cols * i) + j;
         ptr2 = b->data + (a->cols * i) + j;
         ptr3 = p->data + (a->cols * i) + j;
         *ptr3 = subNumber(*ptr1, *ptr2);
      }

   return p;
}

/* subtract a Cmplx from a Matrix */
Matrix * subCmplxMatrix(Cmplx *a, Matrix *b){
   (void)a; (void)b;
   invalidTyprError("-");
   return NULL;
}

/* subtract a Real from a Matrix */
Matrix * subRealMatrix(Real *a, Matrix *b){
   (void)a; (void)b;
   invalidTyprError("-");
   return NULL;
}

/* subtract a Cmplx from a Matrix */
Matrix * subMatrixCmplx(Matrix *a, Cmplx *b){
   int i, j;
   Number *n1;
   Number **ptr1, **ptr2;
   Matrix *p = newMatrix();

   if(a == NULL) { fprintf(stderr, "subMatrixCmplx(NULL)\n"); exit(0); }

   /* initalize the stuff in p */
   p->data = mallocData(NULL, a->rows, a->cols);
   p->rows = a->rows;
   p->cols = a->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = a->data + (a->cols * i) + j;
         ptr2 = p->data + (a->cols * i) + j;
         *ptr2 = subNumber(*ptr1, (n1=setNumberCmplx(newNumber(), b)));
         freeNumber(n1);
      }

   return p;
}

/* subtract a Cmplx from a Matrix */
Matrix * subMatrixReal(Matrix *a, Real *b){
   int i, j;
   Number *n1;
   Number **ptr1, **ptr2;
   Matrix *p = newMatrix();

   if(a == NULL) { fprintf(stderr, "subMatrixReal(NULL)\n"); exit(0); }

   /* initalize the stuff in p */
   p->data = mallocData(NULL, a->rows, a->cols);
   p->rows = a->rows;
   p->cols = a->cols;

   /* p[i][j] = b + a[i][j] */
   for(i=0; i<a->rows; i++)
      for(j=0; j<a->cols; j++){
         ptr1 = a->data + (a->cols * i) + j;
         ptr2 = p->data + (a->cols * i) + j;
         *ptr2 = subNumber(*ptr1, (n1=setNumberReal(newNumber(), b)));
         freeNumber(n1);
      }

   return p;
}

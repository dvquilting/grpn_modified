/* lcd.c: the stack display's text and settings.  The Qt widget that
   draws it is in lcd_qt.cpp. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lcd.h"
#include "mode.h"
#include "number.h"
#include "stack.h"
#include "editor.h"
#include "error.h"
#include "undo.h"
#include "gui.h"

char **lcdText=NULL;
int lcdWidth=0,lcdHeight=0;
static int thousandsSeparators=1;
static int displayDigits=DEFAULT_DISPLAY_DIGITS, engNotation=0;
static int fontSize=12;
static int lcd_dirty=1;

void calcStackLCD(void);

void resizeLCD(int cols,int rows){
   int i; char *buf;
   if(cols<1) cols=1;
   if(rows<1) rows=1;
   if(cols==lcdWidth&&rows==lcdHeight&&lcdText) return;
   if(lcdText){free(lcdText[0]);free(lcdText);} lcdWidth=cols;lcdHeight=rows;
   lcdText=malloc((lcdHeight+1)*sizeof(*lcdText));buf=calloc((size_t)lcdHeight*lcdWidth+1,1);if(!lcdText||!buf){perror("malloc");exit(EXIT_FAILURE);} lcdText[0]=buf;
   for(i=0;i<lcdHeight;i++)lcdText[i]=buf+i*lcdWidth;
   lcd_dirty=1;
}
void updateLCD(void){ if(lcd_dirty&&lcdText){ calcStackLCD(); lcd_dirty=0; } }
void clearLCD(void){ clearStack();resetError();cancelEditor();initUndoSystem();redrawLCD(); }
void redrawLCD(void){ lcd_dirty=1;lcdQueueDraw(); }
void setThousandsSeparators(int enabled){ thousandsSeparators=enabled!=0; }
int getThousandsSeparators(void){return thousandsSeparators;}
void setDisplayDigits(int n){ if(n<MIN_DISPLAY_DIGITS)n=MIN_DISPLAY_DIGITS; else if(n>MAX_DISPLAY_DIGITS)n=MAX_DISPLAY_DIGITS; displayDigits=n; }
int getDisplayDigits(void){return displayDigits;}
void setEngNotation(int enabled){ engNotation=enabled!=0; }
int getEngNotation(void){return engNotation;}
void setLCDFontSize(int size){
   if(size<8) size=8; else if(size>36) size=36;
   if(size==fontSize) return;
   fontSize=size; lcd_dirty=1; lcdFontChanged();
}
int getLCDFontSize(void){return fontSize;}

void calcStackLCD(void){
   int i,j,strLen,pLen,curPos,height,indx,row,txtPos,lines,top,bottom,labelLen; char *c,*p,*txt,label[16]; Number *num;
   if(!lcdText) return;
   height=isEditingEditor()?lcdHeight-1:lcdHeight;
   if(height<=0) return;
   memset(lcdText[0],' ',(size_t)lcdHeight*lcdWidth);
   if(isError()){top=1;c=getStringError();strLen=strlen(c);if(strLen>lcdWidth)strLen=lcdWidth;memcpy(lcdText[0],c,strLen);}else top=0; bottom=height;
   for(indx=0;indx<stackLen();indx++){
      num=getStackEle(indx);if(!num)break;txt=printNumber(num);if(!txt)break;
      lines=1;for(p=txt;*p;p++)if(*p=='\n'){lines++;*p='\0';} row=bottom-lines;if(row<top)row=top;
      snprintf(label,sizeof(label),"%d: ",indx+1);labelLen=strlen(label);if(labelLen>lcdWidth)labelLen=lcdWidth;memcpy(lcdText[row],label,labelLen);
      p=txt;for(j=0,i=row;i<bottom&&j<lines;i++,j++){int copyLen;pLen=strlen(p);txtPos=lcdWidth-pLen;if(txtPos<labelLen)txtPos=labelLen;if(txtPos>lcdWidth)break;copyLen=pLen;if(copyLen>lcdWidth-txtPos)copyLen=lcdWidth-txtPos;memcpy(&lcdText[i][txtPos],p,copyLen);p+=pLen+1;} free(txt);bottom-=j;if(bottom==top)break;
   }
   for(i=indx,j=bottom;j>top;i++,j--){snprintf(label,sizeof(label),"%d: ",i+1);labelLen=strlen(label);if(labelLen>lcdWidth)labelLen=lcdWidth;memcpy(lcdText[j-1],label,labelLen);}
   if(isEditingEditor()){txt=getLineEditor();curPos=cursorPosEditor();if(curPos>lcdWidth)txt+=curPos-lcdWidth;strLen=strlen(txt);if(strLen>lcdWidth)strLen=lcdWidth;memcpy(lcdText[lcdHeight-1],txt,strLen);}
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "lcd.h"
#include "mode.h"
#include "number.h"
#include "stack.h"
#include "process_input.h"
#include "editor.h"
#include "error.h"
#include "undo.h"

#define BORDER 4
#define DEFAULT_LCD_FONT "DejaVu Sans Mono 12"
#define START_SELECTING 1
#define NONE_SELECTED 2
#define DATA_SELECTED 4

GtkWidget *lcdDA=NULL;
char **lcdText=NULL;
int lcdWidth=0,lcdHeight=0;
static int thousandsSeparators=1;
static int displayDigits=DEFAULT_DISPLAY_DIGITS, engNotation=0;
static int fontW=9,fontH=18,fontD=4;
static int fontSize=12;
static int dataSelected=NONE_SELECTED,hiX0,hiX1,hiX2,hiY0,hiY1,hiY2;
static char *font_name=NULL;
static gboolean lcd_dirty=TRUE;

static void resize_buffer(int width,int height);
void calcStackLCD(void);

static void queue_draw(void){ if(lcdDA) gtk_widget_queue_draw(lcdDA); }
void drawStackLCD(void){ queue_draw(); }
void clearLCDwindow(void){ queue_draw(); }

static void measure_font(GtkWidget *widget){
   PangoLayout *layout=gtk_widget_create_pango_layout(widget,"M");
   PangoFontDescription *desc=pango_font_description_from_string(font_name?font_name:DEFAULT_LCD_FONT);
   int w,h; pango_font_description_set_size(desc,fontSize*PANGO_SCALE);
   pango_layout_set_font_description(layout,desc); pango_layout_get_pixel_size(layout,&w,&h);
   fontW=w>0?w:9; fontH=h>0?h:18; fontD=fontH/4;
   pango_font_description_free(desc); g_object_unref(layout);
}

static void render(GtkDrawingArea *area,cairo_t *cr,int width,int height,gpointer data){
   PangoLayout *layout; PangoFontDescription *desc; int i,start,stop,cur;
   (void)area; (void)data;
   resize_buffer(width,height);
   if(lcd_dirty){ calcStackLCD(); lcd_dirty=FALSE; }
   cairo_set_source_rgb(cr,1,1,1); cairo_paint(cr);
   layout=gtk_widget_create_pango_layout(lcdDA,NULL);
   desc=pango_font_description_from_string(font_name?font_name:DEFAULT_LCD_FONT);
   pango_font_description_set_size(desc,fontSize*PANGO_SCALE); pango_layout_set_font_description(layout,desc);
   for(i=0;i<lcdHeight;i++){
      start=0; stop=lcdWidth;
      if(dataSelected==DATA_SELECTED && i>=hiY1 && i<=hiY2){
         cairo_set_source_rgb(cr,0.15,0.25,0.45); cairo_rectangle(cr,BORDER+hiX1*fontW,BORDER+i*fontH,(hiX2-hiX1+1)*fontW,fontH); cairo_fill(cr);
      }
      if(stop>start){
         pango_layout_set_text(layout,lcdText[i],lcdWidth);
         cairo_set_source_rgb(cr,0,0,0); cairo_move_to(cr,BORDER,BORDER+i*fontH); pango_cairo_show_layout(cr,layout);
      }
      if(dataSelected==DATA_SELECTED && i>=hiY1 && i<=hiY2){
         pango_layout_set_text(layout,&lcdText[i][hiX1],hiX2-hiX1+1);
         cairo_set_source_rgb(cr,1,1,1); cairo_move_to(cr,BORDER+hiX1*fontW,BORDER+i*fontH); pango_cairo_show_layout(cr,layout);
      }
   }
   if(isEditingEditor() && lcdHeight>0){ cur=cursorPosEditor(); if(cur>lcdWidth) cur=lcdWidth; cairo_set_source_rgb(cr,0,0,0);
      cairo_move_to(cr,BORDER+cur*fontW-1,BORDER+(lcdHeight-1)*fontH); cairo_line_to(cr,BORDER+cur*fontW-1,BORDER+lcdHeight*fontH); cairo_stroke(cr); }
   pango_font_description_free(desc); g_object_unref(layout);
}

static gboolean key_pressed(GtkEventControllerKey *c,guint keyval,guint keycode,GdkModifierType state,gpointer data){
   gunichar u=gdk_keyval_to_unicode(keyval); char ch=(u>0 && u<128)?(char)u:'\0'; (void)c;(void)keycode;(void)data;
   processInput(keyval,(state&GDK_CONTROL_MASK)!=0,ch,ch); return TRUE;
}
static void paste_ready(GObject *source,GAsyncResult *result,gpointer data){
   char *text=gdk_clipboard_read_text_finish(GDK_CLIPBOARD(source),result,NULL); const char *p; (void)data;
   if(!text) return;
   for(p=text;*p;p++) processInput(0,0,*p,*p);
   g_free(text);
}
static void copy_selection(void){
   GString *s; int y,w;
   if(dataSelected!=DATA_SELECTED) return;
   s=g_string_new(NULL); w=hiX2-hiX1+1;
   for(y=hiY1;y<=hiY2;y++){ g_string_append_len(s,&lcdText[y][hiX1],w); if(y<hiY2)g_string_append_c(s,'\n'); }
   gdk_clipboard_set_text(gdk_display_get_primary_clipboard(gtk_widget_get_display(lcdDA)),s->str); g_string_free(s,TRUE);
}
static void point_to_cell(double x,double y,int *xx,int *yy){
   *xx=((int)x-BORDER)/fontW; *yy=((int)y-BORDER)/fontH;
   if(*xx<0) *xx=0;
   if(*xx>=lcdWidth) *xx=lcdWidth-1;
   if(*yy<0) *yy=0;
   if(*yy>=lcdHeight) *yy=lcdHeight-1;
}
static void drag_begin(GtkGestureDrag *g,double x,double y,gpointer d){ (void)g;(void)d; gtk_widget_grab_focus(lcdDA); point_to_cell(x,y,&hiX0,&hiY0); hiX1=hiX2=hiX0; hiY1=hiY2=hiY0; dataSelected=START_SELECTING; queue_draw(); }
static void drag_update(GtkGestureDrag *g,double dx,double dy,gpointer d){ double x,y; (void)d; gtk_gesture_drag_get_start_point(g,&x,&y); point_to_cell(x+dx,y+dy,&hiX2,&hiY2); hiX1=hiX0; hiY1=hiY0; if(hiX1>hiX2){int t=hiX1;hiX1=hiX2;hiX2=t;} if(hiY1>hiY2){int t=hiY1;hiY1=hiY2;hiY2=t;} dataSelected=DATA_SELECTED; queue_draw(); }
static void drag_end(GtkGestureDrag *g,double dx,double dy,gpointer d){ (void)g;(void)dx;(void)dy;(void)d; if(dataSelected==DATA_SELECTED)copy_selection(); else dataSelected=NONE_SELECTED; queue_draw(); }
static void click(GtkGestureClick *g,int n,double x,double y,gpointer d){ guint b=gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(g)); (void)n;(void)x;(void)y;(void)d; if(b==2)gdk_clipboard_read_text_async(gdk_display_get_primary_clipboard(gtk_widget_get_display(lcdDA)),NULL,paste_ready,NULL); }

GtkWidget *setupLCD(GtkWidget *parent,int rows,int cols,char *font){
   GtkEventController *keys; GtkGesture *drag,*clicker;
   lcdWidth=cols; lcdHeight=rows; font_name=g_strdup(font?font:DEFAULT_LCD_FONT);
   lcdDA=gtk_drawing_area_new(); measure_font(lcdDA);
   gtk_widget_set_size_request(lcdDA,2*BORDER+cols*fontW,2*BORDER+rows*fontH); gtk_widget_set_vexpand(lcdDA,TRUE);
   gtk_widget_set_focusable(lcdDA,TRUE); gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(lcdDA),render,NULL,NULL);
   keys=gtk_event_controller_key_new(); g_signal_connect(keys,"key-pressed",G_CALLBACK(key_pressed),NULL); gtk_widget_add_controller(lcdDA,keys);
   drag=gtk_gesture_drag_new(); gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag),1); g_signal_connect(drag,"drag-begin",G_CALLBACK(drag_begin),NULL); g_signal_connect(drag,"drag-update",G_CALLBACK(drag_update),NULL); g_signal_connect(drag,"drag-end",G_CALLBACK(drag_end),NULL); gtk_widget_add_controller(lcdDA,GTK_EVENT_CONTROLLER(drag));
   clicker=gtk_gesture_click_new(); gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(clicker),0); g_signal_connect(clicker,"pressed",G_CALLBACK(click),NULL); gtk_widget_add_controller(lcdDA,GTK_EVENT_CONTROLLER(clicker));
   gtk_box_append(GTK_BOX(parent),lcdDA); gtk_widget_grab_focus(lcdDA); return lcdDA;
}

static void resize_buffer(int width,int height){
   int neww=(width-2*BORDER)/fontW,newh=(height-2*BORDER)/fontH,i; char *buf;
   if(neww<1) neww=1;
   if(newh<1) newh=1;
   if(neww==lcdWidth&&newh==lcdHeight&&lcdText) return;
   if(lcdText){free(lcdText[0]);free(lcdText);} lcdWidth=neww;lcdHeight=newh;
   lcdText=malloc((lcdHeight+1)*sizeof(*lcdText));buf=calloc((size_t)lcdHeight*lcdWidth+1,1);if(!lcdText||!buf){perror("malloc");exit(EXIT_FAILURE);} lcdText[0]=buf;
   for(i=0;i<lcdHeight;i++)lcdText[i]=buf+i*lcdWidth;
   lcd_dirty=TRUE;
}
void clearLCD(void){ clearStack();resetError();cancelEditor();initUndoSystem();lcd_dirty=TRUE;queue_draw(); }
void redrawLCD(void){ lcd_dirty=TRUE;queue_draw(); }
void setThousandsSeparators(int enabled){ thousandsSeparators=enabled!=0; }
int getThousandsSeparators(void){return thousandsSeparators;}
void setDisplayDigits(int n){ if(n<MIN_DISPLAY_DIGITS)n=MIN_DISPLAY_DIGITS; else if(n>MAX_DISPLAY_DIGITS)n=MAX_DISPLAY_DIGITS; displayDigits=n; }
int getDisplayDigits(void){return displayDigits;}
void setEngNotation(int enabled){ engNotation=enabled!=0; }
int getEngNotation(void){return engNotation;}
void setLCDFontSize(int size){
   int columns,rows;
   if(size<8) size=8; else if(size>36) size=36;
   if(size==fontSize) return;
   columns=lcdWidth; rows=lcdHeight; fontSize=size;
   if(lcdDA){
      measure_font(lcdDA);
      gtk_widget_set_size_request(lcdDA,2*BORDER+columns*fontW,2*BORDER+rows*fontH);
      gtk_widget_queue_resize(lcdDA); lcd_dirty=TRUE; queue_draw();
   }
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

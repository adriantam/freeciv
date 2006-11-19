/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

/**********************************************************************
                          messagewin.c  -  description
                             -------------------
    begin                : Feb 2 2003
    copyright            : (C) 2003 by Rafał Bursig
    email                : Rafał Bursig <bursig@poczta.fm>
 **********************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <SDL/SDL.h>

/* utility */
#include "fcintl.h"
#include "log.h"

/* client */
#include "options.h"

/* gui-sdl */
#include "citydlg.h"
#include "colors.h"
#include "graphics.h"
#include "gui_id.h"
#include "gui_main.h"
#include "gui_tilespec.h"
#include "mapview.h"
#include "themespec.h"
#include "widget.h"

#include "messagewin.h"


#ifdef SMALL_SCREEN
#define N_MSG_VIEW               3    /* max before scrolling happens */
#else
#define N_MSG_VIEW		 6          
#endif

#define PTSIZE_LOG_FONT		adj_font(10)

static struct ADVANCED_DLG *pMsg_Dlg = NULL;

/**************************************************************************
 Called from default clicks on a message.
**************************************************************************/
static int msg_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct message *pMsg = (struct message *)pWidget->data.ptr;
      
    pWidget->string16->fgcol.r += 128;
    unsellect_widget_action();
    
    if (pMsg->city_ok
        && is_city_event(pMsg->event)) {
          
      struct city *pCity = tile_get_city(pMsg->tile);
  
      if (center_when_popup_city) {
        center_tile_mapcanvas(pMsg->tile);
      }
  
      if (pCity) {
        /* If the event was the city being destroyed, pcity will be NULL
         * and we'd better not try to pop it up.  In this case, it would
         * be better if the popup button weren't highlighted at all, but
         * that's OK. */
        popup_city_dialog(pCity);
      }
      
      if (center_when_popup_city || pCity) {
        flush_dirty();
      }
          
    } else if (pMsg->location_ok) {
      center_tile_mapcanvas(pMsg->tile);
      flush_dirty();
    }
  }
  return -1;
}

/**************************************************************************
 Called from default clicks on a messages window.
**************************************************************************/
static int move_msg_window_callback(struct widget *pWindow)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    move_window_group(pMsg_Dlg->pBeginWidgetList, pWindow);
  }
  return -1;
}

/* ======================================================================
				Public
   ====================================================================== */

/**************************************************************************
 ...
**************************************************************************/
void real_update_meswin_dialog(void)
{
  int msg_count;
  int i;
  struct message *pMsg = NULL;
  struct widget *pBuf = NULL, *pWindow = NULL;
  SDL_String16 *pStr = NULL;
  SDL_Rect area = {0, 0, 0, 0};
  bool create;
  int label_width;

  msg_count = get_num_messages();
  i = pMsg_Dlg->pScroll->count;
  
  if ((i > 0) && (msg_count <= i)) {
    del_group_of_widgets_from_gui_list(pMsg_Dlg->pBeginActiveWidgetList,
					pMsg_Dlg->pEndActiveWidgetList);
    pMsg_Dlg->pBeginActiveWidgetList = NULL;
    pMsg_Dlg->pEndActiveWidgetList = NULL;
    pMsg_Dlg->pActiveWidgetList = NULL;
    /* hide scrollbar */
    hide_scrollbar(pMsg_Dlg->pScroll);
    pMsg_Dlg->pScroll->count = 0;
    i = 0;
  }
  create = (i == 0);

  pWindow = pMsg_Dlg->pEndWidgetList;
  
  area.x = pTheme->FR_Left->w;
  area.y = pTheme->FR_Top->h + WINDOW_TITLE_HEIGHT + 1;
  area.w = pWindow->size.w - pTheme->FR_Left->w - pTheme->FR_Right->w;
  area.h = pWindow->size.h - pTheme->FR_Top->h - pTheme->FR_Bottom->h;

  label_width = area.w - pMsg_Dlg->pScroll->pUp_Left_Button->size.w - adj_size(3);
  
  if (msg_count > 0) {
    for(; i<msg_count; i++)
    {
      pMsg = get_message(i);
      pStr = create_str16_from_char(pMsg->descr , PTSIZE_LOG_FONT);
      	
      pBuf = create_iconlabel(NULL, pWindow->dst, pStr, 
    		(WF_RESTORE_BACKGROUND|WF_DRAW_TEXT_LABEL_WITH_SPACE));
    
      pBuf->string16->bgcol = (SDL_Color) {0, 0, 0, 0};

      pBuf->size.w = label_width;
      pBuf->data.ptr = (void *)pMsg;	
      pBuf->action = msg_callback;
      if(pMsg->tile) {
        set_wstate(pBuf, FC_WS_NORMAL);
	pBuf->string16->fgcol = *get_game_colorRGB(COLOR_THEME_MESWIN_ACTIVE_TEXT);
      }
      
      pBuf->ID = ID_LABEL;

      widget_set_area(pBuf, area);
      
      /* add to widget list */
      if(create) {
        add_widget_to_vertical_scroll_widget_list(pMsg_Dlg,
				pBuf, pWindow, FALSE,
				area.x, area.y);
	 create = FALSE;
      } else {
	add_widget_to_vertical_scroll_widget_list(pMsg_Dlg,
				pBuf, pMsg_Dlg->pBeginActiveWidgetList, FALSE,
				area.x, area.y);
      }
      
      
    } /* for */
  } /* if */

  redraw_group(pMsg_Dlg->pBeginWidgetList, pWindow, 0);
  widget_flush(pWindow);
}

/**************************************************************************
  Popup (or raise) the message dialog; typically triggered by 'F10'.
**************************************************************************/
void popup_meswin_dialog(bool raise)
{
  SDL_String16 *pStr;
  SDL_Rect area = {0, 0, 0, 0};
  int label_width, scrollbar_width, i = 0;
  struct message *pMsg = NULL;
  struct widget *pWindow = NULL, *pBuf = NULL;
  int msg_count;
  SDL_Surface *pBackground;
  
  if(pMsg_Dlg) {
    return;
  }
  
  pMsg_Dlg = fc_calloc(1, sizeof(struct ADVANCED_DLG));

  pStr = create_str16_from_char(_("Log"), adj_font(12));
  pStr->style = TTF_STYLE_BOLD;
  
  /* create window */
  pWindow = create_window(NULL, pStr, 1, 1, 0);

  pWindow->action = move_msg_window_callback;
  set_wstate(pWindow, FC_WS_NORMAL);
  add_to_gui_list(ID_CHATLINE_WINDOW, pWindow);
  pMsg_Dlg->pEndWidgetList = pWindow;

  area.x = pTheme->FR_Left->w;
  area.y = pTheme->FR_Top->h + WINDOW_TITLE_HEIGHT + 1;
  area.w = (adj_size(520) - pTheme->FR_Right->w - pTheme->FR_Left->w);
  area.h = N_MSG_VIEW * str16size(pStr).h;

  /* create window background */
  pBackground = theme_get_background(theme, BACKGROUND_MESSAGEWIN);
  if (resize_window(pWindow, pBackground, NULL,
      (area.x + area.w + pTheme->FR_Right->w),
      (area.y + area.h + pTheme->FR_Bottom->h))) {
    FREESURFACE(pBackground);
  }
  
  widget_set_position(pWindow, (pWindow->area.w - pWindow->size.w)/2, adj_size(25));

  /* ------------------------------- */
  
  msg_count = get_num_messages();
  
  if (msg_count) {
    for(i=0; i<msg_count; i++)
    {
      pMsg = get_message(i);
      pStr = create_str16_from_char(pMsg->descr , PTSIZE_LOG_FONT);
      	
      pBuf = create_iconlabel(NULL, pWindow->dst, pStr, 
    		(WF_RESTORE_BACKGROUND|WF_DRAW_TEXT_LABEL_WITH_SPACE));
    
      pBuf->string16->bgcol = (SDL_Color) {0, 0, 0, 0};
      pBuf->size.w = area.w;      
      widget_set_area(pBuf, area);
      widget_set_position(pBuf, area.x, pBuf->size.y);
      pBuf->data.ptr = (void *)pMsg;	
      pBuf->action = msg_callback;
      if(pMsg->tile) {
        set_wstate(pBuf, FC_WS_NORMAL);
	pBuf->string16->fgcol = *get_game_colorRGB(COLOR_THEME_MESWIN_ACTIVE_TEXT2);
      }
      
      if(i>N_MSG_VIEW-1) {
        set_wflag(pBuf, WF_HIDDEN);
      }
      
      add_to_gui_list(ID_LABEL, pBuf);
    }
    pMsg_Dlg->pEndActiveWidgetList = pWindow->prev;
    pMsg_Dlg->pBeginActiveWidgetList = pBuf;
    pMsg_Dlg->pBeginWidgetList = pBuf;
    
  } else {
    pMsg_Dlg->pBeginWidgetList = pWindow;
  }
  
  scrollbar_width = create_vertical_scrollbar(pMsg_Dlg, 1, N_MSG_VIEW, TRUE, TRUE);
  setup_vertical_scrollbar_area(pMsg_Dlg->pScroll,
		area.x + area.w - 1, area.y,
                area.h, TRUE);
  
  if(i>N_MSG_VIEW-1) {
    /* find pActiveWidgetList to draw last seen part of list */
    /* pBuf her has pointer to last created widget */
    pBuf = pMsg_Dlg->pBeginActiveWidgetList;
    for(i = 0; i < N_MSG_VIEW; i++) {
      clear_wflag(pBuf, WF_HIDDEN);
      pBuf = pBuf->next;
    }
    pMsg_Dlg->pActiveWidgetList = pBuf->prev;
    /* hide others (not-seen) widgets */
    while(pBuf != pMsg_Dlg->pEndActiveWidgetList->next) {
      set_wflag(pBuf, WF_HIDDEN);
      pBuf = pBuf->next;
    }
    /* set new scrollbar position */
    pMsg_Dlg->pScroll->pScrollBar->size.y = pMsg_Dlg->pScroll->max -
				    pMsg_Dlg->pScroll->pScrollBar->size.h;
  } else {
    hide_scrollbar(pMsg_Dlg->pScroll);
  }
    
  /* ------------------------------------- */
  if (msg_count > 0) {

    /* correct label widths */
    label_width = area.w - scrollbar_width - adj_size(3);        
    
    pBuf = pMsg_Dlg->pEndActiveWidgetList;
    while (pBuf) {
      pBuf->size.w = label_width;
      if (pBuf == pMsg_Dlg->pBeginActiveWidgetList) {
        break;
      }
      pBuf = pBuf->prev;
    }
    
    /* find if scrollbar is active */
    if(pMsg_Dlg->pActiveWidgetList) {
      pBuf = pMsg_Dlg->pActiveWidgetList;
    } else {
      pBuf = pMsg_Dlg->pEndActiveWidgetList;
    }
    
    setup_vertical_widgets_position(1,
	area.x, area.y, 0, 0,
	pMsg_Dlg->pBeginActiveWidgetList, pBuf);
  }

  redraw_group(pMsg_Dlg->pBeginWidgetList,
		  pMsg_Dlg->pEndWidgetList, 1);
  flush_dirty();
}

/**************************************************************************
  Popdown the messages dialog; called by void popdown_all_game_dialogs(void)
**************************************************************************/
void popdown_meswin_dialog(void)
{
  if(pMsg_Dlg) {
    popdown_window_group_dialog(pMsg_Dlg->pBeginWidgetList,
				  pMsg_Dlg->pEndWidgetList);
    FC_FREE(pMsg_Dlg->pScroll);
    FC_FREE(pMsg_Dlg);
  }
  
}

/**************************************************************************
  Return whether the message dialog is open.
**************************************************************************/
bool is_meswin_open(void)
{
  return (pMsg_Dlg != NULL);
}

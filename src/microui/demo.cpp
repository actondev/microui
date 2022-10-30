#include "microui.hpp"
#include <stdio.h>
#include <string.h>

float mu_demo_bg[3] = {90, 95, 100};

static char logbuf[64000];
static int logbuf_updated = 0;

static void write_log(const char *text) {
  if(logbuf[0]) {
    strcat(logbuf, "\n");
  }
  strcat(logbuf, text);
  logbuf_updated = 1;
}

static void test_window(mu_Context *ctx) {
  /* do window */
  if(mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 450))) {
    mu_Container *win = mu_get_current_container(ctx);
    win->rect.w = mu_max(win->rect.w, 240);
    win->rect.h = mu_max(win->rect.h, 300);

    /* window info */
    if(mu_header(ctx, "Window Info")) {
      mu_Container *win = mu_get_current_container(ctx);
      char buf[64];
      mu_layout_row(ctx, 2, (const int[]){54, -1}, 0);
      mu_label(ctx, "Position:");
      sprintf(buf, "%d, %d", win->rect.x, win->rect.y);
      mu_label(ctx, buf);
      mu_label(ctx, "Size:");
      sprintf(buf, "%d, %d", win->rect.w, win->rect.h);
      mu_label(ctx, buf);
    }

    /* labels + buttons */
    if(mu_header_ex(ctx, "Test Buttons", MU_OPT_EXPANDED)) {
      mu_layout_row(ctx, 3, (const int[]){86, -110, -1}, 0);
      mu_label(ctx, "Test buttons 1:");
      if(mu_button(ctx, "Button 1")) {
        write_log("Pressed button 1");
      }
      if(mu_button(ctx, "Button 2")) {
        write_log("Pressed button 2");
      }
      mu_label(ctx, "Test buttons 2:");
      if(mu_button(ctx, "Button 3")) {
        write_log("Pressed button 3");
      }
      if(mu_button(ctx, "Popup")) {
        mu_open_popup(ctx, "Test Popup");
      }
      if(mu_begin_popup(ctx, "Test Popup")) {
        mu_button(ctx, "Hello");
        mu_button(ctx, "World");
        mu_end_popup(ctx);
      }
    }

    /* tree */
    if(mu_header_ex(ctx, "Tree and Text", MU_OPT_EXPANDED)) {
      mu_layout_row(ctx, 2, (const int[]){140, -1}, 0);
      mu_layout_begin_column(ctx);
      if(mu_begin_treenode(ctx, "Test 1")) {
        if(mu_begin_treenode(ctx, "Test 1a")) {
          mu_label(ctx, "Hello");
          mu_label(ctx, "world");
          mu_end_treenode(ctx);
        }
        if(mu_begin_treenode(ctx, "Test 1b")) {
          if(mu_button(ctx, "Button 1")) {
            write_log("Pressed button 1");
          }
          if(mu_button(ctx, "Button 2")) {
            write_log("Pressed button 2");
          }
          mu_end_treenode(ctx);
        }
        mu_end_treenode(ctx);
      }
      if(mu_begin_treenode(ctx, "Test 2")) {
        mu_layout_row(ctx, 2, (const int[]){54, 54}, 0);
        if(mu_button(ctx, "Button 3")) {
          write_log("Pressed button 3");
        }
        if(mu_button(ctx, "Button 4")) {
          write_log("Pressed button 4");
        }
        if(mu_button(ctx, "Button 5")) {
          write_log("Pressed button 5");
        }
        if(mu_button(ctx, "Button 6")) {
          write_log("Pressed button 6");
        }
        mu_end_treenode(ctx);
      }
      if(mu_begin_treenode(ctx, "Test 3")) {
        static int checks[3] = {1, 0, 1};
        mu_checkbox(ctx, "Checkbox 1", &checks[0]);
        mu_checkbox(ctx, "Checkbox 2", &checks[1]);
        mu_checkbox(ctx, "Checkbox 3", &checks[2]);
        mu_end_treenode(ctx);
      }
      mu_layout_end_column(ctx);

      mu_layout_begin_column(ctx);
      mu_layout_row(ctx, 1, (const int[]){-1}, 0);
      mu_text(ctx, "Lorem ipsum dolor sit amet, consectetur adipiscing "
                   "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
                   "ipsum, eu varius magna felis a nulla.");
      mu_layout_end_column(ctx);
    }

    /* background color sliders */
    if(mu_header_ex(ctx, "Background Color", MU_OPT_EXPANDED)) {
      static int do_button = 0;
      mu_checkbox(ctx, "use button for the right layout", &do_button);
      int content_height = 10;
      mu_Style *style = mu_get_style(ctx);
      int row_height = 3 * (10 * content_height + style->padding.top + style->padding.bottom + style->margin.y);
      (void)row_height;
      // we either pass the calculated row_height from above, or 0 (for default height)
      // In the second case, we need to calculate & set the layout height (as done below).
      // In the first case, the r.h = {..} could be avoided.
      mu_layout_row(ctx, 2, (const int[]){-78, -1}, 0);
      /* sliders */
      mu_layout_begin_column(ctx);
      mu_layout_row(ctx, 2, (const int[]){46, -1}, 0);
      mu_Rect rect1 = mu_layout_next(ctx);
      mu_layout_set_next(ctx, rect1, 0);
      mu_label(ctx, "Red:");
      mu_slider_float(ctx, &mu_demo_bg[0], 0, 255);
      mu_label(ctx, "Green:");
      mu_slider_float(ctx, &mu_demo_bg[1], 0, 255);
      mu_label(ctx, "Blue:");
      mu_slider_float(ctx, &mu_demo_bg[2], 0, 255);
      mu_Rect rect2 = mu_layout_next(ctx);
      mu_layout_set_next(ctx, rect2, 0);
      mu_layout_end_column(ctx);
      /* color preview */
      mu_Rect r = mu_layout_next(ctx);
      // if the row_height was used (instead of 0) in the
      // mu_layout_row (the one before the layout_bein_column), the
      // next line could be skipped (as the r.h will already have the
      // correct value)
      r.h = rect2.y - rect1.y - style->margin.y;
      char buf[32];
      sprintf(buf, "#%02X%02X%02X", (int)mu_demo_bg[0], (int)mu_demo_bg[1], (int)mu_demo_bg[2]);
      if(do_button) {
        mu_layout_set_next(ctx, r, 0);
        mu_Color *color = &style->colors[MU_COLOR_BUTTON];
        mu_Color prev_color = *color;
        color->r = mu_demo_bg[0];
        color->g = mu_demo_bg[1];
        color->b = mu_demo_bg[2];
        mu_button(ctx, buf);
        *color = prev_color;
      } else {
        mu_draw_rect(ctx, r, mu_color(mu_demo_bg[0], mu_demo_bg[1], mu_demo_bg[2], 255));
        mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
      }
    }

    mu_end_window(ctx);
  }
}

static void log_window(mu_Context *ctx) {
  if(mu_begin_window(ctx, "Log Window", mu_rect(350, 40, 300, 200))) {
    /* output text panel */
    mu_layout_row(ctx, 1, (const int[]){-1}, -25);
    mu_begin_panel(ctx, "Log Output");
    mu_Container *panel = mu_get_current_container(ctx);
    mu_layout_row(ctx, 1, (const int[]){-1}, -1);
    mu_text(ctx, logbuf);
    mu_end_panel(ctx);
    if(logbuf_updated) {
      panel->scroll.y = panel->content_size.y;
      logbuf_updated = 0;
    }

    /* input textbox + submit button */
    static char buf[128];
    int submitted = 0;
    mu_layout_row(ctx, 2, (const int[]){-70, -1}, 0);
    if(mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
      mu_set_focus(ctx, mu_get_current_id(ctx));
      submitted = 1;
    }
    if(mu_button(ctx, "Submit")) {
      submitted = 1;
    }
    if(submitted) {
      write_log(buf);
      buf[0] = '\0';
    }

    mu_end_window(ctx);
  }
}

static int uint8_slider(mu_Context *ctx, unsigned char *value, int low, int high) {
  static float tmp;
  mu_push_id(ctx, &value, sizeof(value));
  tmp = *value;
  int res = mu_slider_float_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
  *value = tmp;
  mu_pop_id(ctx);
  return res;
}

static void style_window(mu_Context *ctx) {
  static struct {
    const char *label;
    int idx;
  } colors[] = {{"text:", MU_COLOR_TEXT},
                {"border:", MU_COLOR_BORDER},
                {"windowbg:", MU_COLOR_WINDOWBG},
                {"titlebg:", MU_COLOR_TITLEBG},
                {"titletext:", MU_COLOR_TITLETEXT},
                {"panelbg:", MU_COLOR_PANELBG},
                {"button:", MU_COLOR_BUTTON},
                {"buttonhover:", MU_COLOR_BUTTONHOVER},
                {"buttonfocus:", MU_COLOR_BUTTONFOCUS},
                {"base:", MU_COLOR_BASE},
                {"basehover:", MU_COLOR_BASEHOVER},
                {"basefocus:", MU_COLOR_BASEFOCUS},
                {"scrollbase:", MU_COLOR_SCROLLBASE},
                {"scrollthumb:", MU_COLOR_SCROLLTHUMB},
                {NULL}};

  if(mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 240))) {
    int sw = mu_get_current_container(ctx)->body.w * 0.14;
    int widths[] = {80, sw, sw, sw, sw, -1};
    mu_layout_row(ctx, 6, widths, 0);
    mu_Style *style = mu_get_style(ctx);
    for(int i = 0; colors[i].label; i++) {
      mu_label(ctx, colors[i].label);
      uint8_slider(ctx, &style->colors[i].r, 0, 255);
      uint8_slider(ctx, &style->colors[i].g, 0, 255);
      uint8_slider(ctx, &style->colors[i].b, 0, 255);
      uint8_slider(ctx, &style->colors[i].a, 0, 255);
      mu_draw_rect(ctx, mu_layout_next(ctx), style->colors[i]);
    }
    mu_end_window(ctx);
  }
}

void mu_demo(mu_Context *ctx) {
  style_window(ctx);
  log_window(ctx);
  test_window(ctx);
}

/*
** Copyright (c) 2020 rxi
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to
** deal in the Software without restriction, including without limitation the
** rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
** sell copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
** IN THE SOFTWARE.
*/

#include "microui.hpp"
#include <algorithm>
#include <assert.h>
#include <range/v3/all.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MICROUI_LOG 1

#if MICROUI_LOG
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#define LOG(format, ...)                                                                                               \
  fmt::print("[microui] ");                                                                                            \
  fmt::print(format, ##__VA_ARGS__);                                                                                   \
  fmt::print("\n");
#else
#define LOG(format, ...) {};
#endif

#define MICROUI_LOG 1

#if MICROUI_LOG
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#define LOG(format, ...)                                                                                               \
  fmt::print("[microui] ");                                                                                            \
  fmt::print(format, ##__VA_ARGS__);                                                                                   \
  fmt::print("\n");
#else
#define LOG(format, ...) {};
#endif

#define unused(x) ((void)(x))

#define expect(x)                                                                                                      \
  do {                                                                                                                 \
    if(!(x)) {                                                                                                         \
      fprintf(stderr, "Fatal error: %s:%d: assertion '%s' failed\n", __FILE__, __LINE__, #x);                          \
      abort();                                                                                                         \
    }                                                                                                                  \
  } while(0)

template <typename T>
inline void push(std::vector<T> &stk, T &&val) {
  stk.push_back(val);
}

template <typename T>
inline void push(std::vector<T> &stk, const T &val) {
  stk.push_back(val);
}

template <typename T>
inline void pop(std::vector<T> &stk) {
  expect(!stk.empty());
  stk.pop_back();
}

static mu_Rect unclipped_rect = {{0, 0, 0x1000000, 0x1000000}};

static mu_Style default_style = {
    // Note:
    // - spacing of 1 would cause the 1px border of adjacent items to align
    // - item row content height is size.y (see below)
    -1,                             /* font */
    12,                             /* font_size */
    -1,                             /* icon_font */
    12,                             /* icon_font_size */
    {{0}, {0}, {0}, {0}, {0}, {0}}, /* icons_utf8 */
    {{68, 44}},                     /* size */
    {0, 0, 0, 0},                   // padding
    {10, 10},                       // margin
    24,                             // indent
    24,                             // title_height
    20,                             // footer_height
    12,                             // scrollbar_size
    8,                              // thumb_size
    {
        {230, 230, 230, 255}, /* MU_COLOR_TEXT */
        {25, 25, 25, 255},    /* MU_COLOR_BORDER */
        {50, 50, 50, 255},    /* MU_COLOR_WINDOWBG */
        {25, 25, 25, 255},    /* MU_COLOR_TITLEBG */
        {115, 115, 115, 255}, /* MU_COLOR_FOOTERBG */
        {240, 240, 240, 255}, /* MU_COLOR_TITLETEXT */
        {0, 0, 0, 0},         /* MU_COLOR_PANELBG */
        {75, 75, 75, 255},    /* MU_COLOR_BUTTON */
        {95, 95, 95, 255},    /* MU_COLOR_BUTTONHOVER */
        {115, 115, 115, 255}, /* MU_COLOR_BUTTONFOCUS */
        {30, 30, 30, 255},    /* MU_COLOR_BASE */
        {35, 35, 35, 255},    /* MU_COLOR_BASEHOVER */
        {40, 40, 40, 255},    /* MU_COLOR_BASEFOCUS */
        {43, 43, 43, 255},    /* MU_COLOR_SCROLLBASE */
        {30, 30, 30, 255},    /* MU_COLOR_SCROLLTHUMB */
        {0, 255, 255, 100}    /* MU_COLOR_FOCUS_BORDER */
    }};

static void default_draw_frame(mu_Context *ctx, mu_Rect rect, int colorid);

template <typename Fn>
struct mu_Handler {
  mu_Id id;
  Fn handler;
};

struct Event {
  mu_EventType type;
  bool propagate{true};
  union {
    mu_KeyEvent key;
    mu_MouseButtonEvent mousebutton;
    mu_MouseMoveEvent mousemove;
  };
};

struct EventHandlerWrapper {
  mu_Id container_id;
  mu_EventType type;
  mu_EventHandler fn;
};

struct mu_Context {
  vgir_ctx *vgir;
  vgir_jump_t vgir_begin, vgir_end; // begin: head, end: tail
  /* callbacks */
  int (*text_width)(mu_Font font, int font_size, const char *str, int len);
  int (*text_height)(mu_Font font, int font_size);
  void (*draw_frame)(mu_Context *ctx, mu_Rect rect, int colorid);
  /* core state */
  mu_Style _style;
  mu_Style *style{nullptr};
  mu_Id hover{0};
  mu_Id focus{0};
  mu_Id last_focus{0};
  bool should_focus_next{false};
  mu_Id prev_id{0};
  mu_Id cur_id{0};
  mu_Rect last_rect;
  int last_zindex{0};
  bool updated_focus{false};
  int frame{0};
  mu_Container *hover_root{nullptr};
  mu_Container *next_hover_root{nullptr};
  mu_Container *scroll_target{nullptr};
  char number_edit_buf[MU_MAX_FMT];
  mu_Id number_edit{0};
  /* stacks */
  std::vector<mu_Container *> root_list;
  std::vector<mu_Container *> container_stack;
  std::vector<mu_Rect> clip_stack;
  std::vector<mu_Id> id_stack;
  std::vector<mu_Layout> layout_stack;
  std::vector<mu_Id> hovered_container_stack;
  // containes the current or last focus element stack (might be from
  // clicking an element or by tabbing to cycle through elements)
  std::vector<mu_Id> focus_stack;

  std::vector<mu_Event> events;

  std::vector<EventHandlerWrapper> event_handlers;
  std::vector<EventHandlerWrapper> global_event_handlers;

  /* retained state pools */
  mu_PoolItem container_pool[MU_CONTAINERPOOL_SIZE];
  mu_Container containers[MU_CONTAINERPOOL_SIZE];
  mu_PoolItem treenode_pool[MU_TREENODEPOOL_SIZE];
  /* input state */
  mu_Vec2 mouse_pos;
  mu_Vec2 last_mouse_pos;
  mu_Vec2 mouse_delta;
  mu_Vec2 scroll_delta;
  int mouse_down{0};
  /// Delta: was mouse pressed (not pressed -> pressed) in THIS frame
  int mouse_pressed{0};
  int key_down{0};
  int key_pressed{0};
  char input_text[32];

  mu_Context(vgir_ctx *vgir) : vgir(vgir) {
    expect(vgir);
    root_list.reserve(MU_ROOTLIST_SIZE);
    container_stack.reserve(MU_CONTAINERSTACK_SIZE);
    clip_stack.reserve(MU_CLIPSTACK_SIZE);
    id_stack.reserve(MU_IDSTACK_SIZE);
    layout_stack.reserve(MU_LAYOUTSTACK_SIZE);

    draw_frame = default_draw_frame;
    _style = default_style;
    style = &_style;
  }
};

mu_Vec2 mu_vec2(int x, int y) {
  mu_Vec2 res;
  res.x = x;
  res.y = y;
  return res;
}

mu_Rect mu_rect(int x, int y, int w, int h) {
  mu_Rect res;
  res.x = x;
  res.y = y;
  res.w = w;
  res.h = h;
  return res;
}

mu_Color mu_color(int r, int g, int b, int a) {
  mu_Color res;
  res.r = r;
  res.g = g;
  res.b = b;
  res.a = a;
  return res;
}

static mu_Rect expand_rect(mu_Rect rect, int n) {
  return mu_rect(rect.x - n, rect.y - n, rect.w + n * 2, rect.h + n * 2);
}

static mu_Rect expand_rect(mu_Rect rect, mu_Box box) {
  return mu_rect(rect.x - box.top, rect.y - box.top, rect.w + box.left + box.right, rect.h + box.top + box.bottom);
}

static mu_Rect intersect_rects(mu_Rect r1, mu_Rect r2) {
  int x1 = mu_max(r1.x, r2.x);
  int y1 = mu_max(r1.y, r2.y);
  int x2 = mu_min(r1.x + r1.w, r2.x + r2.w);
  int y2 = mu_min(r1.y + r1.h, r2.y + r2.h);
  if(x2 < x1) {
    x2 = x1;
  }
  if(y2 < y1) {
    y2 = y1;
  }
  return mu_rect(x1, y1, x2 - x1, y2 - y1);
}

static int rect_overlaps_vec2(mu_Rect r, mu_Vec2 p) {
  return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

static void default_draw_frame(mu_Context *ctx, mu_Rect rect, int colorid) {
  mu_draw_rect(ctx, rect, ctx->style->colors[colorid]);
  if(colorid == MU_COLOR_SCROLLBASE || colorid == MU_COLOR_SCROLLTHUMB || colorid == MU_COLOR_TITLEBG) {
    return;
  }
  /* draw border */
  if(ctx->style->colors[MU_COLOR_BORDER].a) {
    mu_draw_box(ctx, expand_rect(rect, 1), ctx->style->colors[MU_COLOR_BORDER]);
  }
}

mu_Context *mu_init(vgir_ctx *vgir) { return new mu_Context(vgir); }

void mu_free(mu_Context *ctx) { delete ctx; }

void mu_set_vgir(mu_Context *ctx, vgir_ctx *vgir) { ctx->vgir = vgir; }

vgir_ctx *mu_get_vgir(mu_Context *ctx) { return ctx->vgir; }

void mu_set_text_width_cb(mu_Context *ctx, mu_TextWidthCb cb) { ctx->text_width = cb; }

void mu_set_text_height_cb(mu_Context *ctx, mu_TextHeightCb cb) { ctx->text_height = cb; }

void mu_begin(mu_Context *ctx) {
  expect(ctx->text_width && ctx->text_height);
  ctx->root_list.clear();
  ctx->hovered_container_stack.clear();

  ctx->event_handlers.clear();
  ctx->global_event_handlers.clear();

  ctx->scroll_target = nullptr;
  ctx->hover_root = ctx->next_hover_root;
  ctx->next_hover_root = nullptr;
  ctx->mouse_delta.x = ctx->mouse_pos.x - ctx->last_mouse_pos.x;
  ctx->mouse_delta.y = ctx->mouse_pos.y - ctx->last_mouse_pos.y;
  ctx->frame++;
}

static auto make_is_handler_applicable(const mu_Event &event, const std::vector<mu_Id> &stack, mu_Id target_id) {
  return [=](const auto &handler) {
    if(!(handler.type & event.type)) {
      return false;
    }
    if(handler.container_id == target_id) {
      return true; // exact match
    }
    if(handler.container_id == 0) {
      return true; // fallback
    }
    // matching parents
    return std::any_of(stack.rbegin(), stack.rend(), [&](const auto parentFocusId) {
      const bool handlerMatchesFocusParent = handler.container_id == parentFocusId;
      return handlerMatchesFocusParent;
    });
  };
}

static void handle_event(mu_Context *ctx, const mu_Event &event, std::vector<mu_Id> &id_stack, mu_Id target_id) {
  using namespace ranges;

  auto make_event_handlers = [&](auto &handlers, const mu_Event &event) {
    return handlers | views::reverse | views::filter(make_is_handler_applicable(event, id_stack, target_id));
  };

  for(const auto &handler : make_event_handlers(ctx->global_event_handlers, event)) {
    if(handler.fn(event))
      return;
  }

  for(const auto &handler : make_event_handlers(ctx->event_handlers, event)) {
    if(handler.fn(event))
      return;
  }
}

static void handle_events(mu_Context *ctx) {
  if(ctx->events.empty())
    return;

  const auto focus_id = ctx->focus ? ctx->focus : ctx->last_focus;
  const auto hover_id = !ctx->hovered_container_stack.empty() ? ctx->hovered_container_stack.back() : 0;

  for(const auto &event : ctx->events) {
    if(event.type & (mu_EventType::MOUSEDOWN | mu_EventType::MOUSEUP | mu_EventType::MOUSEMOVE)) {
      handle_event(ctx, event, ctx->hovered_container_stack, hover_id);
    } else if(event.type & (mu_EventType::KEYDOWN | mu_EventType::KEYUP | mu_EventType::KEYPRESS)) {
      handle_event(ctx, event, ctx->focus_stack, focus_id);
    }
    // TODO implement global event listeneres who get called no matter the propagation
    // if(!event.propagate) continue;
  }
}

void mu_end(mu_Context *ctx) {
  int i, n;
  /* check stacks */
  expect(ctx->container_stack.empty());
  expect(ctx->clip_stack.empty());
  expect(ctx->id_stack.empty());
  expect(ctx->layout_stack.empty());

  handle_events(ctx);
  ctx->events.clear();

  /* handle scroll input */
  if(ctx->scroll_target) {
    ctx->scroll_target->scroll.x += ctx->scroll_delta.x;
    ctx->scroll_target->scroll.y += ctx->scroll_delta.y;
  }

  /* unset focus if focus id was not touched this frame */
  if(!ctx->updated_focus) {
    ctx->focus = 0;
  }
  ctx->updated_focus = false;

  /* bring hover root to front if mouse was pressed */
  if(ctx->mouse_pressed && ctx->next_hover_root && ctx->next_hover_root->zindex < ctx->last_zindex &&
     ctx->next_hover_root->zindex >= 0) {
    mu_bring_to_front(ctx, ctx->next_hover_root);
  }

  /* reset input state */
  ctx->key_pressed = 0;
  ctx->input_text[0] = '\0';
  ctx->mouse_pressed = 0;
  ctx->scroll_delta = mu_vec2(0, 0);
  ctx->last_mouse_pos = ctx->mouse_pos;

  /* sort root containers by zindex */
  n = ctx->root_list.size();
  if(!n)
    return;
  // qsort(ctx->root_list.items, n, sizeof(mu_Container*), compare_zindex);
  std::sort(ctx->root_list.begin(), ctx->root_list.end(),
            [&](const auto a, const auto b) { return a->zindex < b->zindex; });

  vgir_ctx *vgir = ctx->vgir;
  mu_Container *first = ctx->root_list[0];
  vgir_set_jump_dst(vgir, ctx->vgir_begin, first->vgir_begin);
  for(i = 0; i < n - 1; i++) {
    mu_Container *current = ctx->root_list[i];
    mu_Container *next = ctx->root_list[i + 1];
    vgir_set_jump_dst(vgir, current->vgir_end, next->vgir_begin);
  }
  mu_Container *last = ctx->root_list[n - 1];
  vgir_set_jump_dst(vgir, last->vgir_end, ctx->vgir_end);
}

void mu_set_focus(mu_Context *ctx, mu_Id id) {
  ctx->last_focus = ctx->focus;
  ctx->focus = id;
  ctx->updated_focus = true;
  if(id) {
    ctx->focus_stack = ctx->id_stack;
    LOG("setting focus to id {}, stack", id);
    for(auto id : ctx->focus_stack) {
      LOG("  {}", id);
    }
  }
}

/* 32bit fnv-1a hash */
#define HASH_INITIAL 2166136261

static void hash(mu_Id *hash, const void *data, int size) {
  const unsigned char *p = (unsigned char *)data;
  while(size--) {
    *hash = (*hash ^ *p++) * 16777619;
  }
}

mu_Id mu_get_id(mu_Context *ctx, const void *data, int size) {
  int idx = ctx->id_stack.size();
  mu_Id res = (idx > 0) ? ctx->id_stack[idx - 1] : HASH_INITIAL;
  hash(&res, data, size);
  ctx->prev_id = ctx->cur_id;
  ctx->cur_id = res;
  return res;
}

void mu_push_id(mu_Context *ctx, mu_Id id) { push(ctx->id_stack, id); }

void mu_push_id(mu_Context *ctx, const void *data, int size) { mu_push_id(ctx, mu_get_id(ctx, data, size)); }

void mu_pop_id(mu_Context *ctx) { pop(ctx->id_stack); }

mu_Id mu_get_current_id(mu_Context *ctx) { return ctx->cur_id; }

void mu_push_clip_rect(mu_Context *ctx, mu_Rect rect) {
  mu_Rect last = mu_get_clip_rect(ctx);
  push(ctx->clip_stack, intersect_rects(rect, last));
}

void mu_pop_clip_rect(mu_Context *ctx) { pop(ctx->clip_stack); }

mu_Rect mu_get_clip_rect(mu_Context *ctx) {
  expect(!ctx->clip_stack.empty());
  return ctx->clip_stack.back();
}

int mu_check_clip(mu_Context *ctx, mu_Rect r) {
  mu_Rect cr = mu_get_clip_rect(ctx);
  if(r.x > cr.x + cr.w || r.x + r.w < cr.x || r.y > cr.y + cr.h || r.y + r.h < cr.y) {
    return MU_CLIP_ALL;
  }
  if(r.x >= cr.x && r.x + r.w <= cr.x + cr.w && r.y >= cr.y && r.y + r.h <= cr.y + cr.h) {
    return 0;
  }
  return MU_CLIP_PART;
}

static void push_layout(mu_Context *ctx, mu_Rect body, mu_Vec2 scroll) {
  mu_Layout layout;
  int width = 0;
  layout.body = mu_rect(body.x - scroll.x, body.y - scroll.y, body.w, body.h);
  layout.max = mu_vec2(-0x1000000, -0x1000000);
  push(ctx->layout_stack, layout);
  mu_layout_row(ctx, 1, &width, 0);
}

// TODO if layout_stack gets resized, we'll have an invalid pointer
mu_Layout *mu_get_layout(mu_Context *ctx) { return &ctx->layout_stack.back(); }

static void pop_container(mu_Context *ctx) {
  mu_Container *cnt = mu_get_current_container(ctx);
  mu_Layout *layout = mu_get_layout(ctx);
  cnt->content_size.x = layout->max.x - layout->body.x + ctx->style->margin.x;
  cnt->content_size.y = layout->max.y - layout->body.y + ctx->style->margin.y;
  /* pop container, layout and id */
  pop(ctx->container_stack);
  pop(ctx->layout_stack);
  mu_pop_id(ctx);
  vgir_pop_scissor(ctx->vgir);
}

mu_Container *mu_get_current_container(mu_Context *ctx) {
  expect(!ctx->container_stack.empty());
  return ctx->container_stack.back();
}

static mu_Container *get_container(mu_Context *ctx, mu_Id id, int opt) {
  mu_Container *cnt;
  /* try to get existing container from pool */
  int idx = mu_pool_get(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  if(idx >= 0) {
    if(ctx->containers[idx].open || ~opt & MU_OPT_CLOSED) {
      mu_pool_update(ctx, ctx->container_pool, idx);
    }
    mu_Container *cnt = &ctx->containers[idx];
    cnt->id = id; // guess not needed? not sure how the container pool works
    return cnt;
  }
  if(opt & MU_OPT_CLOSED) {
    return nullptr;
  }
  /* container not found in pool: init new container */
  idx = mu_pool_init(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  cnt = &ctx->containers[idx];
  memset(cnt, 0, sizeof(*cnt));
  cnt->open = 1;
  cnt->id = id;
  mu_bring_to_front(ctx, cnt);
  return cnt;
}

mu_Container *mu_get_container(mu_Context *ctx, const char *name) {
  mu_Id id = mu_get_id(ctx, name, strlen(name));
  return get_container(ctx, id, 0);
}

void mu_bring_to_front(mu_Context *ctx, mu_Container *cnt) { cnt->zindex = ++ctx->last_zindex; }

mu_Style *mu_get_style(mu_Context *ctx) { return ctx->style; }

/*============================================================================
** pool
**============================================================================*/

int mu_pool_init(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i, n = -1, f = ctx->frame;
  for(i = 0; i < len; i++) {
    if(items[i].last_update < f) {
      f = items[i].last_update;
      n = i;
    }
  }
  expect(n > -1);
  items[n].id = id;
  mu_pool_update(ctx, items, n);
  return n;
}

int mu_pool_get(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i;
  unused(ctx);
  for(i = 0; i < len; i++) {
    if(items[i].id == id) {
      return i;
    }
  }
  return -1;
}

void mu_pool_update(mu_Context *ctx, mu_PoolItem *items, int idx) { items[idx].last_update = ctx->frame; }

/*============================================================================
** input handlers
**============================================================================*/
bool mu_has_event(mu_Context *ctx, mu_EventType type) {
  auto it =
      std::find_if(ctx->events.begin(), ctx->events.end(), [type](const auto &event) { return event.type & type; });
  return it != ctx->events.end();
}

void mu_event_handler(mu_Context *ctx, mu_EventType type, mu_EventHandler handler_fn) {
  mu_Id id = 0;
  if(!ctx->id_stack.empty()) {
    id = ctx->id_stack.back();
  }
  EventHandlerWrapper handler_wrapper;
  handler_wrapper.type = type;
  handler_wrapper.container_id = id;
  handler_wrapper.fn = handler_fn;

  ctx->event_handlers.push_back(handler_wrapper);
}

void mu_global_event_handler(mu_Context *ctx, mu_EventType type, mu_EventHandler handler_fn) {
  mu_Id id = 0;
  EventHandlerWrapper handler_wrapper;
  handler_wrapper.type = type;
  handler_wrapper.container_id = id;
  handler_wrapper.fn = handler_fn;

  ctx->global_event_handlers.push_back(handler_wrapper);
}

void mu_input_mousemove(mu_Context *ctx, int x, int y) {
  mu_Event ev;
  ev.type = MOUSEMOVE;
  mu_MouseMoveEvent data;
  data.x = x;
  data.y = y;
  data.dx = x - ctx->mouse_pos.x;
  data.dy = y - ctx->mouse_pos.y;
  ev.data = data;
  ctx->events.push_back(ev);
  ctx->mouse_pos = mu_vec2(x, y);
}
mu_Vec2 mu_get_mouse_pos(mu_Context *ctx) { return ctx->mouse_pos; }

void mu_input_mousedown(mu_Context *ctx, int x, int y, int btn) {
  mu_Event ev;
  ev.type = MOUSEDOWN;
  mu_MouseButtonEvent data;
  data.button = btn;
  ev.data = data;
  ctx->events.push_back(ev);

  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down |= btn;
  ctx->mouse_pressed |= btn;
}

void mu_input_mouseup(mu_Context *ctx, int x, int y, int btn) {
  mu_Event ev;
  ev.type = MOUSEUP;
  mu_MouseButtonEvent data;
  data.button = btn;
  ev.data = data;
  ctx->events.push_back(ev);

  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down &= ~btn;
}

void mu_input_scroll(mu_Context *ctx, int x, int y) {
  ctx->scroll_delta.x += x;
  ctx->scroll_delta.y += y;
}

void mu_input_keydown(mu_Context *ctx, int key) {
  mu_Event ev;
  ev.type = KEYDOWN;
  mu_KeyEvent data;
  data.key = key;
  ev.data = data;
  ctx->events.push_back(ev);

  ctx->key_pressed |= key;
  ctx->key_down |= key;
}

void mu_input_keyup(mu_Context *ctx, int key) {
  mu_Event ev;
  ev.type = KEYUP;
  mu_KeyEvent data;
  data.key = key;
  ev.data = data;
  ctx->events.push_back(ev);

  ctx->key_down &= ~key;
}

void mu_input_text(mu_Context *ctx, const char *text) {
  int len = strlen(ctx->input_text);
  int size = strlen(text) + 1;
  expect(len + size <= (int)sizeof(ctx->input_text));
  memcpy(ctx->input_text + len, text, size);
}

/*============================================================================
** Clip
**============================================================================*/

static void mu_push_clip_draw(mu_Context *ctx, mu_Rect rect) {
  vgir_push_scissor(ctx->vgir, rect.x, rect.y, rect.w, rect.h);
}

static void mu_pop_clip_draw(mu_Context *ctx) { vgir_pop_scissor(ctx->vgir); }

/*============================================================================
** Drawing
**============================================================================*/

void mu_draw_rect(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  rect = intersect_rects(rect, mu_get_clip_rect(ctx));
  if(rect.w <= 0 || rect.h <= 0)
    return;
  vgir_begin_path(ctx->vgir);
  vgir_fill_color(ctx->vgir, color.r / 255.0, color.g / 255.0, color.b / 255.0, color.a / 255.0);
  vgir_rect(ctx->vgir, rect.x, rect.y, rect.w, rect.h);
  vgir_fill(ctx->vgir);
}

void mu_draw_box(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_Rect clip = mu_get_clip_rect(ctx);
  mu_Rect intersected = intersect_rects(rect, clip);
  if(intersected.w <= 0 || intersected.h <= 0)
    return;

  vgir_ctx *vgir = ctx->vgir;
  vgir_begin_path(vgir);
  vgir_stroke_color(vgir, color.r / 255.0, color.g / 255.0, color.b / 255.0, color.a / 255.0);
  vgir_stroke_width(vgir, 1);
  vgir_rect(vgir, rect.x, rect.y, rect.w, rect.h);
  vgir_stroke(vgir);
}

void mu_draw_text(mu_Context *ctx, mu_Font font, int font_size, const char *str, int len, mu_Vec2 pos, mu_Color color) {
  mu_Rect rect = mu_rect(pos.x, pos.y, ctx->text_width(font, font_size, str, len), ctx->text_height(font, font_size));
  int clipped = mu_check_clip(ctx, rect);
  if(clipped == MU_CLIP_ALL) {
    return;
  }

  vgir_begin_path(ctx->vgir); // before clipping!
  if(clipped == MU_CLIP_PART) {
    mu_push_clip_draw(ctx, mu_get_clip_rect(ctx));
  }

  if(len < 0) {
    len = strlen(str);
  }
  vgir_begin_path(ctx->vgir);
  vgir_ctx *vgir = ctx->vgir;
  vgir_font_face_id(vgir, font);
  vgir_font_size(vgir, font_size);
  vgir_fill_color(ctx->vgir, color.r / 255.0, color.g / 255.0, color.b / 255.0, color.a / 255.0);
  const char *end = str + len;
  vgir_text_align(vgir, (vgir_align)(LEFT | TOP));
  vgir_text(vgir, pos.x, pos.y, str, end);
  vgir_fill(vgir);

  /* reset clipping if it was set */
  if(clipped == MU_CLIP_PART) {
    mu_pop_clip_draw(ctx);
  }
}

void mu_draw_icon(mu_Context *ctx, int id, mu_Rect rect, mu_Color color) {
  /* do clip command if the rect isn't fully contained within the cliprect */
  int clipped = mu_check_clip(ctx, rect);
  if(clipped == MU_CLIP_ALL) {
    return;
  }
  if(clipped == MU_CLIP_PART) {
    mu_push_clip_draw(ctx, mu_get_clip_rect(ctx));
  }
  /* do icon command */
  const char *text = nullptr;
  switch(id) {
  case MU_ICON_CLOSE:
    text = ctx->style->icons_utf8.close;
    break;
  case MU_ICON_RESIZE:
    text = ctx->style->icons_utf8.resize;
    break;
  case MU_ICON_CHECK:
    text = ctx->style->icons_utf8.check;
    break;
  case MU_ICON_COLLAPSED:
    text = ctx->style->icons_utf8.collapsed;
    break;
  case MU_ICON_EXPANDED:
    text = ctx->style->icons_utf8.expanded;
    break;
  case MU_ICON_MAX:
    text = ctx->style->icons_utf8.max;
    break;
  }
  expect(text);
  int icon_width = ctx->text_width(ctx->style->icon_font, ctx->style->icon_font_size, text, -1);
  int icon_height = ctx->text_height(ctx->style->icon_font, ctx->style->icon_font_size);
  mu_Vec2 pos = {{rect.x + (rect.w - icon_width) / 2, rect.y + (rect.h - icon_height) / 2}};
  mu_draw_text(ctx, ctx->style->icon_font, ctx->style->icon_font_size, text, -1, pos, color);

  /* reset clipping if it was set */
  if(clipped == MU_CLIP_PART) {
    mu_pop_clip_draw(ctx);
  }
}

/*============================================================================
** layout
**============================================================================*/

enum { RELATIVE = 1, ABSOLUTE = 2 };

void mu_layout_begin_column(mu_Context *ctx) { push_layout(ctx, mu_layout_next(ctx), mu_vec2(0, 0)); }

void mu_layout_end_column(mu_Context *ctx) {
  mu_Layout *a, *b;
  b = mu_get_layout(ctx);
  pop(ctx->layout_stack);
  /* inherit position/next_row/max from child layout if they are greater */
  a = mu_get_layout(ctx);
  a->position.x = mu_max(a->position.x, b->position.x + b->body.x - a->body.x);
  a->next_row = mu_max(a->next_row, b->next_row + b->body.y - a->body.y);
  a->max.x = mu_max(a->max.x, b->max.x);
  a->max.y = mu_max(a->max.y, b->max.y);
}

void mu_layout_row(mu_Context *ctx, int items, const int *widths, int height) {
  mu_Layout *layout = mu_get_layout(ctx);
  if(widths) {
    expect(items <= MU_MAX_WIDTHS);
    memcpy(layout->widths, widths, items * sizeof(widths[0]));
  }
  layout->items = items;
  layout->position = mu_vec2(layout->indent, layout->next_row);
  layout->size.y = height;
  layout->item_index = 0;
}

void mu_layout_width(mu_Context *ctx, int width) { mu_get_layout(ctx)->size.x = width; }

void mu_layout_height(mu_Context *ctx, int height) { mu_get_layout(ctx)->size.y = height; }

void mu_layout_set_next(mu_Context *ctx, mu_Rect r, int relative) {
  mu_Layout *layout = mu_get_layout(ctx);
  layout->next = r;
  layout->next_type = relative ? RELATIVE : ABSOLUTE;
}

void mu_layout_set_next_size(mu_Context *ctx, mu_Vec2 size) {
  mu_Layout *layout = mu_get_layout(ctx);
  layout->next_size = size;
}

mu_Rect mu_layout_next(mu_Context *ctx) {
  mu_Layout *layout = mu_get_layout(ctx);
  mu_Style *style = ctx->style;
  mu_Rect res;

  if(layout->next_type) {
    /* handle rect set by `mu_layout_set_next` */
    int type = layout->next_type;
    layout->next_type = 0;
    res = layout->next;
    if(type == ABSOLUTE) {
      return (ctx->last_rect = res);
    }
  } else {
    /* handle next row */
    if(layout->item_index == layout->items) {
      mu_layout_row(ctx, layout->items, nullptr, layout->size.y);
    }

    /* position */
    res.x = layout->position.x + style->margin.x;
    res.y = layout->position.y + style->margin.y;

    /* size */
    if(layout->next_size.has_value()) {
      const auto &next_size = layout->next_size.value();
      res.w = next_size.x;
      res.h = next_size.y;
      layout->next_size = std::nullopt;
    } else {
      // Note: if layout items are set (ie their widths), this width includes padding
      res.w = layout->items > 0 ? layout->widths[layout->item_index] : layout->size.x;
      res.h = layout->size.y;
    }
    if(res.w == 0) {
      res.w = style->size.x + style->padding.left + style->padding.right;
    }
    if(res.h == 0) {
      res.h = style->size.y + style->padding.top + style->padding.bottom;
    }
    if(res.w < 0) {
      res.w += layout->body.w - res.x + 1;
    }
    if(res.h < 0) {
      res.h += layout->body.h - res.y + 1;
    }
    // subtracting margin: it's taken into account (margin-box box model)
    res.w -= style->margin.x * 2;
    res.h -= style->margin.y * 2;

    layout->item_index++;
  }

  /* update position */
  layout->position.x += res.w + style->margin.x * 2;
  layout->next_row = mu_max(layout->next_row, res.y + res.h + style->margin.y);

  /* apply body offset */
  res.x += layout->body.x;
  res.y += layout->body.y;

  /* update max position */
  layout->max.x = mu_max(layout->max.x, res.x + res.w);
  layout->max.y = mu_max(layout->max.y, res.y + res.h);

  return (ctx->last_rect = res);
}

/*============================================================================
** controls
**============================================================================*/

static int in_hover_root(mu_Context *ctx) {
  int i = ctx->container_stack.size();
  while(i--) {
    if(ctx->container_stack[i] == ctx->hover_root) {
      return 1;
    }
    /* only root containers have their `head` field set; stop searching if we've
    ** reached the current root container */
    // TODO what about this? removed mu commands so I don't have this anymore
    // if (ctx->container_stack[i]->head) { break; }
  }
  return 0;
}

void mu_draw_control_frame(mu_Context *ctx, mu_Id id, mu_Rect rect, int colorid, int opt) {
  if(opt & MU_OPT_NOFRAME) {
    return;
  }
  colorid += (ctx->focus == id) ? 2 : (ctx->hover == id) ? 1 : 0;
  ctx->draw_frame(ctx, rect, colorid);
}

static bool has_focus(mu_Context *ctx, mu_Id id) { return (ctx->focus && ctx->focus == id) || ctx->last_focus == id; }

static inline void draw_focus(mu_Context *ctx, mu_Id id, mu_Rect rect) {
  if(has_focus(ctx, id)) {
    mu_draw_box(ctx, rect, ctx->style->colors[MU_COLOR_FOCUS_BORDER]);
  }
}

void mu_draw_control_text(mu_Context *ctx, const char *str, mu_Rect rect, int colorid, int opt) {
  mu_Vec2 pos;
  mu_Font font = ctx->style->font;
  int font_size = ctx->style->font_size;

  int tw = ctx->text_width(font, font_size, str, -1);
  mu_push_clip_rect(ctx, rect);
  pos.y = rect.y + (rect.h - ctx->text_height(font, font_size)) / 2;
  if(opt & MU_OPT_ALIGNCENTER) {
    pos.x = rect.x + (rect.w - tw) / 2;
  } else if(opt & MU_OPT_ALIGNRIGHT) {
    pos.x = rect.x + rect.w - tw - ctx->style->padding.left;
  } else {
    pos.x = rect.x + ctx->style->padding.left;
  }
  mu_draw_text(ctx, font, font_size, str, -1, pos, ctx->style->colors[colorid]);
  mu_pop_clip_rect(ctx);
}

int mu_mouse_over(mu_Context *ctx, mu_Rect rect) {
  return rect_overlaps_vec2(rect, ctx->mouse_pos) && rect_overlaps_vec2(mu_get_clip_rect(ctx), ctx->mouse_pos) &&
         in_hover_root(ctx);
}

// TODO reintroduce this
static void stop_events_propagation(mu_Context *ctx, int types) {
  for(auto &ev : ctx->events) {
    if(ev.type & types) {
      // ev.propagate = false;
    }
  }
}

/// Called from: button, checkbox, textbox, number, header, scrollbar, window title/close/resize
void mu_update_control(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt) {
  bool handled_focus_next = false;
  if(ctx->should_focus_next) {
    mu_set_focus(ctx, id);
    printf("set focus next %d\n", id);
    ctx->should_focus_next = false;
    handled_focus_next = true;
  }
  int mouseover = mu_mouse_over(ctx, rect);
  if(mouseover) {
    stop_events_propagation(ctx, MOUSEDOWN | MOUSEUP);
  }

  if(ctx->focus == id) {
    ctx->updated_focus = true;
  }
  if(opt & MU_OPT_NOINTERACT) {
    return;
  }
  if(mouseover && !ctx->mouse_down) {
    ctx->hover = id;
  }

  if(ctx->focus == id) {
    if(ctx->mouse_pressed && !mouseover) {
      mu_set_focus(ctx, 0);
    }
    if(!ctx->mouse_down && ~opt & MU_OPT_HOLDFOCUS) {
      mu_set_focus(ctx, 0);
    }
  }

  if(ctx->hover == id) {
    if(ctx->mouse_pressed) {
      mu_set_focus(ctx, id);
    } else if(!mouseover) {
      ctx->hover = 0;
    }
  }

  if(ctx->key_pressed & MU_KEY_TAB && !handled_focus_next && has_focus(ctx, id)) {
    if(ctx->key_down & MU_KEY_SHIFT) {
      mu_set_focus(ctx, ctx->prev_id);
    } else {
      ctx->should_focus_next = true;
    }
  }
}

void mu_text(mu_Context *ctx, const char *text) {
  const char *start, *end, *p = text;
  int width = -1;
  mu_Font font = ctx->style->font;
  int font_size = ctx->style->font_size;
  mu_Color color = ctx->style->colors[MU_COLOR_TEXT];
  mu_layout_begin_column(ctx);
  mu_layout_row(ctx, 1, &width, ctx->text_height(font, font_size));
  do {
    mu_Rect r = mu_layout_next(ctx);
    int w = 0;
    start = end = p;
    do {
      const char *word = p;
      while(*p && *p != ' ' && *p != '\n') {
        p++;
      }
      w += ctx->text_width(font, font_size, word, p - word);
      if(w > r.w && end != start) {
        break;
      }
      w += ctx->text_width(font, font_size, p, 1);
      end = p++;
    } while(*end && *end != '\n');
    mu_draw_text(ctx, font, font_size, start, end - start, mu_vec2(r.x, r.y), color);
    p = end + 1;
  } while(*end);
  mu_layout_end_column(ctx);
}

void mu_label(mu_Context *ctx, const char *text) {
  mu_draw_control_text(ctx, text, mu_layout_next(ctx), MU_COLOR_TEXT, 0);
}

int mu_button_ex(mu_Context *ctx, const char *label, int icon, int opt) {
  int res = 0;
  mu_Id id = label ? mu_get_id(ctx, label, strlen(label)) : mu_get_id(ctx, &icon, sizeof(icon));
  mu_Rect r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, opt);
  /* handle click */
  if(ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
    res |= MU_RES_SUBMIT;
  }
  /* draw */
  mu_draw_control_frame(ctx, id, r, MU_COLOR_BUTTON, opt);
  if(label) {
    mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, opt);
  }
  if(icon) {
    mu_draw_icon(ctx, icon, r, ctx->style->colors[MU_COLOR_TEXT]);
  }
  draw_focus(ctx, id, r);
  return res;
}

int mu_checkbox(mu_Context *ctx, const char *label, int *state) {
  int res = 0;
  mu_Id id = mu_get_id(ctx, label, strlen(label));
  mu_Rect r = mu_layout_next(ctx);
  mu_Rect box = mu_rect(r.x, r.y, r.h, r.h);
  mu_update_control(ctx, id, r, 0);
  /* handle click */
  if(ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
    res |= MU_RES_CHANGE;
    *state = !*state;
  }
  /* draw */
  mu_draw_control_frame(ctx, id, box, MU_COLOR_BASE, 0);
  if(*state) {
    mu_draw_icon(ctx, MU_ICON_CHECK, box, ctx->style->colors[MU_COLOR_TEXT]);
  }
  r = mu_rect(r.x + box.w, r.y, r.w - box.w, r.h);
  mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, 0);
  draw_focus(ctx, id, r);
  return res;
}

int mu_textbox_raw(mu_Context *ctx, char *buf, int bufsz, mu_Id id, mu_Rect r, int opt) {
  int res = 0;
  mu_update_control(ctx, id, r, opt | MU_OPT_HOLDFOCUS);

  if(ctx->focus == id) {
    /* handle text input */
    int len = strlen(buf);
    int n = mu_min(bufsz - len - 1, (int)strlen(ctx->input_text));
    if(n > 0) {
      memcpy(buf + len, ctx->input_text, n);
      len += n;
      buf[len] = '\0';
      res |= MU_RES_CHANGE;
    }
    /* handle backspace */
    if(ctx->key_pressed & MU_KEY_BACKSPACE && len > 0) {
      /* skip utf-8 continuation bytes */
      while((buf[--len] & 0xc0) == 0x80 && len > 0)
        ;
      buf[len] = '\0';
      res |= MU_RES_CHANGE;
    }
    /* handle return */
    if(ctx->key_pressed & MU_KEY_RETURN) {
      mu_set_focus(ctx, 0);
      res |= MU_RES_SUBMIT;
    }
  }

  /* draw */
  mu_draw_control_frame(ctx, id, r, MU_COLOR_BASE, opt);
  if(ctx->focus == id) {
    mu_Color color = ctx->style->colors[MU_COLOR_TEXT];
    mu_Font font = ctx->style->font;
    int font_size = ctx->style->font_size;
    int textw = ctx->text_width(font, font_size, buf, -1);
    int texth = ctx->text_height(font, font_size);
    int ofx = r.w - ctx->style->padding.left - textw - 1;
    int textx = r.x + mu_min(ofx, ctx->style->padding.left);
    int texty = r.y + (r.h - texth) / 2;
    mu_push_clip_rect(ctx, r);
    mu_draw_text(ctx, font, font_size, buf, -1, mu_vec2(textx, texty), color);
    mu_draw_rect(ctx, mu_rect(textx + textw, texty, 1, texth), color);
    mu_pop_clip_rect(ctx);
  } else {
    mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, opt);
  }
  draw_focus(ctx, id, r);

  return res;
}

static int number_textbox(mu_Context *ctx, mu_Real *value, mu_Rect r, mu_Id id) {
  if(ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->key_down & MU_KEY_SHIFT && ctx->hover == id) {
    ctx->number_edit = id;
    sprintf(ctx->number_edit_buf, MU_REAL_FMT, *value);
  }
  if(ctx->number_edit == id) {
    int res = mu_textbox_raw(ctx, ctx->number_edit_buf, sizeof(ctx->number_edit_buf), id, r, 0);
    if(res & MU_RES_SUBMIT || ctx->focus != id) {
      *value = strtod(ctx->number_edit_buf, nullptr);
      ctx->number_edit = 0;
    } else {
      return 1;
    }
  }
  return 0;
}

int mu_textbox_ex(mu_Context *ctx, char *buf, int bufsz, int opt) {
  mu_Id id = mu_get_id(ctx, &buf, sizeof(buf));
  mu_Rect r = mu_layout_next(ctx);
  return mu_textbox_raw(ctx, buf, bufsz, id, r, opt);
}

#define MU_TYPE int
#include "mu_slider_ex.inl"
#undef MU_TYPE

#define MU_TYPE float
#include "mu_slider_ex.inl"
#undef MU_TYPE

#define MU_TYPE double
#include "mu_slider_ex.inl"
#undef MU_TYPE

int mu_number_ex(mu_Context *ctx, mu_Real *value, mu_Real step, const char *fmt, int opt) {
  char buf[MU_MAX_FMT + 1];
  int res = 0;
  mu_Id id = mu_get_id(ctx, &value, sizeof(value));
  mu_Rect base = mu_layout_next(ctx);
  mu_Real last = *value;

  /* handle text input mode */
  if(number_textbox(ctx, value, base, id)) {
    return res;
  }

  /* handle normal mode */
  mu_update_control(ctx, id, base, opt);

  /* handle input */
  if(ctx->focus == id && ctx->mouse_down == MU_MOUSE_LEFT) {
    *value += ctx->mouse_delta.x * step;
  }
  /* set flag if value changed */
  if(*value != last) {
    res |= MU_RES_CHANGE;
  }

  /* draw base */
  mu_draw_control_frame(ctx, id, base, MU_COLOR_BASE, opt);
  /* draw text  */
  sprintf(buf, fmt, *value);
  mu_draw_control_text(ctx, buf, base, MU_COLOR_TEXT, opt);
  draw_focus(ctx, id, base);

  return res;
}

static int header(mu_Context *ctx, const char *label, int istreenode, int opt) {
  mu_Rect r;
  int active, expanded;
  mu_Id id = mu_get_id(ctx, label, strlen(label));
  int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
  int width = -1;
  mu_layout_row(ctx, 1, &width, 0);

  active = (idx >= 0);
  expanded = (opt & MU_OPT_EXPANDED) ? !active : active;
  r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, 0);

  /* handle click */
  active ^= (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id);

  /* update pool ref */
  if(idx >= 0) {
    if(active) {
      mu_pool_update(ctx, ctx->treenode_pool, idx);
    } else {
      memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem));
    }
  } else if(active) {
    mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
  }

  /* draw */
  if(istreenode) {
    if(ctx->hover == id) {
      ctx->draw_frame(ctx, r, MU_COLOR_BUTTONHOVER);
    }
  } else {
    mu_draw_control_frame(ctx, id, r, MU_COLOR_BUTTON, 0);
  }
  mu_draw_icon(ctx, expanded ? MU_ICON_EXPANDED : MU_ICON_COLLAPSED, mu_rect(r.x, r.y, r.h, r.h),
               ctx->style->colors[MU_COLOR_TEXT]);
  r.x += r.h - ctx->style->padding.left;
  r.w -= r.h - ctx->style->padding.left;
  mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, 0);

  draw_focus(ctx, id, r);

  return expanded ? MU_RES_ACTIVE : 0;
}

int mu_header_ex(mu_Context *ctx, const char *label, int opt) { return header(ctx, label, 0, opt); }

int mu_begin_treenode_ex(mu_Context *ctx, const char *label, int opt) {
  int res = header(ctx, label, 1, opt);
  if(res & MU_RES_ACTIVE) {
    mu_get_layout(ctx)->indent += ctx->style->indent;
    push(ctx->id_stack, ctx->cur_id);
  }
  return res;
}

void mu_end_treenode(mu_Context *ctx) {
  mu_get_layout(ctx)->indent -= ctx->style->indent;
  mu_pop_id(ctx);
}

static void scrollbar(mu_Context *ctx, mu_Container *cnt, mu_Rect *b, mu_Vec2 cs, int axis) {
  static const char *scrollbar_ids[] = {
      "!scrollbarx",
      "!scrollbary",
  };

  // Explanatory comments in this scope assume axis == MU_AXIS_Y (ie
  // the vertical scrollbar on the right)

  int size = axis + 2; // x->w, y->h. size means height

  /* only add scrollbar if content size is larger than body */
  int maxscroll = cs.data[axis] - b->data[size]; // ie cs.y - b->h
  if(maxscroll <= 0 || b->data[size] <= 0) {
    cnt->scroll.data[axis] = 0;
    return;
  }
  mu_Rect base, thumb; // base is the whole scrollbar (ie its background)
  mu_Id id = mu_get_id(ctx, scrollbar_ids[axis], 11);

  /* get sizing / positioning */
  int other_axis = !axis;          // otheraxis (x)
  int other_size = other_axis + 2; // width
  base = *b;
  // base.x = b->w + b->w. The scrollbar position
  base.data[other_axis] = b->data[other_axis] + b->data[other_size];
  base.data[other_size] = ctx->style->scrollbar_size; // base.w

  /* handle input */
  mu_update_control(ctx, id, base, 0);
  if(ctx->focus == id && ctx->mouse_down == MU_MOUSE_LEFT) {
    cnt->scroll.data[axis] += ctx->mouse_delta.data[axis] * cs.data[axis] / base.data[size];
  }
  /* clamp scroll to limits */
  cnt->scroll.data[axis] = mu_clamp(cnt->scroll.data[axis], 0, maxscroll);

  /* draw base and thumb */
  ctx->draw_frame(ctx, base, MU_COLOR_SCROLLBASE);
  thumb = base;
  thumb.data[size] = mu_max(ctx->style->thumb_size, base.data[size] * b->data[size] / cs.data[axis]);
  thumb.data[axis] += cnt->scroll.data[axis] * (base.data[size] - thumb.data[size]) / maxscroll;
  ctx->draw_frame(ctx, thumb, MU_COLOR_SCROLLTHUMB);

  /* set this as the scroll_target (will get scrolled on mousewheel) */
  /* if the mouse is over it */
  if(mu_mouse_over(ctx, *b)) {
    ctx->scroll_target = cnt;
  }
  draw_focus(ctx, id, base);
}

static void scrollbars(mu_Context *ctx, mu_Container *cnt, mu_Rect *body) {
  int sz = ctx->style->scrollbar_size;
  mu_Vec2 cs = cnt->content_size;
  cs.x += ctx->style->padding.left + ctx->style->padding.right;
  cs.y += ctx->style->padding.top + ctx->style->padding.bottom;
  mu_push_clip_rect(ctx, *body);
  /* resize body to make room for scrollbars */
  if(cs.y > cnt->body.h) {
    body->w -= sz;
  }
  if(cs.x > cnt->body.w) {
    body->h -= sz;
  }
  scrollbar(ctx, cnt, body, cs, MU_AXIS_Y);
  scrollbar(ctx, cnt, body, cs, MU_AXIS_X);
  mu_pop_clip_rect(ctx);
}

static void push_container_body(mu_Context *ctx, mu_Container *cnt, mu_Rect body, int opt) {
  if(~opt & MU_OPT_NOSCROLL) {
    scrollbars(ctx, cnt, &body);
  }
  push_layout(ctx, expand_rect(body, ctx->style->padding), cnt->scroll);
  cnt->body = body;
  vgir_push_scissor(ctx->vgir, cnt->body.x, cnt->body.y, cnt->body.w, cnt->body.h);
  if(mu_mouse_over(ctx, body)) {
    ctx->hovered_container_stack.push_back(cnt->id);
  }
}

static void begin_root_container(mu_Context *ctx, mu_Container *cnt) {
  push(ctx->container_stack, cnt);
  /* push container to roots list and push head command */
  push(ctx->root_list, cnt);

  if(ctx->root_list.size() == 1) {
    // first window (from code / not z-index)
    ctx->vgir_begin = vgir_store_jump_src(ctx->vgir);
  }
  cnt->vgir_begin = vgir_store_jump_src(ctx->vgir);

  /* set as hover root if the mouse is overlapping this container and it has a
  ** higher zindex than the current hover root */
  if(rect_overlaps_vec2(cnt->rect, ctx->mouse_pos) &&
     (!ctx->next_hover_root || cnt->zindex > ctx->next_hover_root->zindex)) {
    ctx->next_hover_root = cnt;
  }
  /* clipping is reset here in case a root-container is made within
  ** another root-containers's begin/end block; this prevents the inner
  ** root-container being clipped to the outer */
  push(ctx->clip_stack, unclipped_rect);
}

static void end_root_container(mu_Context *ctx) {
  /* push tail 'goto' jump command and set head 'skip' command. the final steps
  ** on initing these are done in mu_end() */
  mu_Container *cnt = mu_get_current_container(ctx);
  /* pop base clip rect and container */
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
  cnt->vgir_end = vgir_store_jump_src(ctx->vgir);
  ctx->vgir_end = vgir_store_jump_src(ctx->vgir); // 1 past the window end
}

// TODO pass bool pointer for window open
int mu_begin_window_ex(mu_Context *ctx, const char *title, mu_Rect rect, int opt) {
  mu_Rect body, titlerect, footer_rect;
  mu_Id id = mu_get_id(ctx, title, strlen(title));
  mu_Container *cnt = get_container(ctx, id, opt);
  if(!cnt || !cnt->open) {
    return 0;
  }
  mu_push_id(ctx, id);

  // cnt->rect.w == 0 evaluates to true only on first run (uninitialized window)
  if(cnt->rect.w == 0 || opt & MU_OPT_FIXED_SIZE) {
    cnt->rect = rect;
  }
  begin_root_container(ctx, cnt);
  rect = cnt->rect;
  body = rect;

  /* draw frame */
  if(~opt & MU_OPT_NOFRAME) {
    ctx->draw_frame(ctx, rect, MU_COLOR_WINDOWBG);
  }

  /* do title bar */
  titlerect = rect;
  titlerect.h = ctx->style->title_height;
  if(~opt & MU_OPT_NOTITLE) {
    ctx->draw_frame(ctx, titlerect, MU_COLOR_TITLEBG);

    /* do title text */
    if(~opt & MU_OPT_NOTITLE) {
      mu_Id id = mu_get_id(ctx, "!title", 6);
      mu_update_control(ctx, id, titlerect, opt);
      mu_draw_control_text(ctx, title, titlerect, MU_COLOR_TITLETEXT, opt);
      draw_focus(ctx, id, titlerect);
      if(id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT) {
        cnt->rect.x += ctx->mouse_delta.x;
        cnt->rect.y += ctx->mouse_delta.y;
      }
      body.y += titlerect.h;
      body.h -= titlerect.h;
    }

    /* do `close` button */
    if(~opt & MU_OPT_NOCLOSE) {
      mu_Id id = mu_get_id(ctx, "!close", 6);
      mu_Rect r = mu_rect(titlerect.x + titlerect.w - titlerect.h, titlerect.y, titlerect.h, titlerect.h);
      titlerect.w -= r.w;
      mu_draw_icon(ctx, MU_ICON_CLOSE, r, ctx->style->colors[MU_COLOR_TITLETEXT]);
      mu_update_control(ctx, id, r, opt);
      draw_focus(ctx, id, r);
      if(ctx->mouse_pressed == MU_MOUSE_LEFT && id == ctx->focus) {
        cnt->open = 0;
      }
    }
  }

  /* do `resize` notch */
  if(~opt & MU_OPT_NORESIZE) {
    int sz = ctx->style->footer_height;
    mu_Id id = mu_get_id(ctx, "!resize", 7);
    footer_rect.x = rect.x;
    footer_rect.y = rect.y + rect.h - sz;
    footer_rect.w = rect.w;
    footer_rect.h = sz;
    ctx->draw_frame(ctx, footer_rect, MU_COLOR_FOOTERBG);
    mu_Rect r = mu_rect(rect.x + rect.w - sz, rect.y + rect.h - sz, sz, sz);
    mu_update_control(ctx, id, r, opt);
    mu_draw_icon(ctx, MU_ICON_RESIZE, r, ctx->style->colors[MU_COLOR_TEXT]);
    draw_focus(ctx, id, r);
    if(id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT) {
      cnt->rect.w += ctx->mouse_delta.x;
      cnt->rect.h += ctx->mouse_delta.y;
      cnt->rect.w = mu_max(96, cnt->rect.w);
      cnt->rect.h = mu_max(64, cnt->rect.h);
    }
    body.h -= sz;
  }

  /* do scrollbars and init clipping.
     Note: the scrollbars are drawn beneath the body.
  */
  push_container_body(ctx, cnt, body, opt);

  /* resize to content size */
  if(opt & MU_OPT_AUTOSIZE) {
    mu_Rect r = mu_get_layout(ctx)->body;
    cnt->rect.w = cnt->content_size.x + (cnt->rect.w - r.w);
    cnt->rect.h = cnt->content_size.y + (cnt->rect.h - r.h);
  }

  /* close if this is a popup window and elsewhere was clicked */
  if(opt & MU_OPT_POPUP && ctx->mouse_pressed && ctx->hover_root != cnt) {
    cnt->open = 0;
  }

  mu_push_clip_rect(ctx, cnt->body);
  return MU_RES_ACTIVE;
}

void mu_end_window(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  end_root_container(ctx);
}

void mu_open_popup(mu_Context *ctx, const char *name) {
  mu_Container *cnt = mu_get_container(ctx, name);
  /* set as hover root so popup isn't closed in begin_window_ex()  */
  ctx->hover_root = ctx->next_hover_root = cnt;
  /* position at mouse cursor, open and bring-to-front */
  cnt->rect = mu_rect(ctx->mouse_pos.x, ctx->mouse_pos.y, 1, 1);
  cnt->open = 1;
  mu_bring_to_front(ctx, cnt);
}

int mu_begin_popup(mu_Context *ctx, const char *name) {
  int opt = MU_OPT_POPUP | MU_OPT_AUTOSIZE | MU_OPT_NORESIZE | MU_OPT_NOSCROLL | MU_OPT_NOTITLE | MU_OPT_CLOSED;
  return mu_begin_window_ex(ctx, name, mu_rect(0, 0, 0, 0), opt);
}

void mu_end_popup(mu_Context *ctx) { mu_end_window(ctx); }

void mu_begin_panel_ex(mu_Context *ctx, const char *name, int opt) {
  mu_Container *cnt;
  mu_Id id = mu_get_id(ctx, name, strlen(name));
  mu_push_id(ctx, id);
  cnt = get_container(ctx, ctx->cur_id, opt);
  cnt->rect = mu_layout_next(ctx);
  if(~opt & MU_OPT_NOFRAME) {
    ctx->draw_frame(ctx, cnt->rect, MU_COLOR_PANELBG);
  }
  push(ctx->container_stack, cnt);
  push_container_body(ctx, cnt, cnt->rect, opt);
  mu_push_clip_rect(ctx, cnt->body);
}

void mu_end_panel(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}

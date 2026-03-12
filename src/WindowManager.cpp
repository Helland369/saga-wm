#include "includes/WindowManager.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <utility>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

WindowManager::WindowManager()
{
  screen_n_ = 0;

  conn_ = xcb_connect(nullptr, &screen_n_);
  if (xcb_connection_has_error(conn_))
    utils_.die("xcb_connection");

  setup_ = xcb_get_setup(conn_);

  iter_ = xcb_setup_roots_iterator(setup_);
  for (int i = 0; i < screen_n_; ++i)
    xcb_screen_next(&iter_);
  screen_ = iter_.data;
  if (!screen_)
    utils_.die("screen");

  root_ = screen_->root;

  keysyms_ = xcb_key_symbols_alloc(conn_);
  if (!keysyms_)
    utils_.die("xcb_key_symbols_alloc");

  cookie_ = xcb_change_window_attributes_checked(conn_,
                                                 root_,
                                                 XCB_CW_EVENT_MASK,
                                                 values_);
}

WindowManager::~WindowManager()
{
}

std::optional<std::pair<int, int>> WindowManager::get_window_xy(xcb_connection_t *conn, xcb_window_t win)
{
  auto geometry = xcb_get_geometry(conn, win);
  xcb_get_geometry_reply_t *r = xcb_get_geometry_reply(conn, geometry, nullptr);
  if (!r)
    return std::nullopt;
  int x = r->x;
  int y = r->y;
  free(r);
  return std::make_pair(x, y);
}

void WindowManager::loop()
{
  err_ = xcb_request_check(conn_, cookie_);
  if (err_)
  {
    std::cerr << "Another WM is already running!\n";
    free(err_);
    xcb_disconnect(conn_);
    return;
  }

  enter_codes_ = xcb_key_symbols_get_keycode(keysyms_, enter_sym_);

  if (enter_codes_)
  {
    for (xcb_keycode_t *kc = enter_codes_; *kc != XCB_NO_SYMBOL; ++kc)
    {
      xcb_grab_key(conn_,
                   1,
                   root_,
                   XCB_MOD_MASK_1,
                   *kc,
                   XCB_GRAB_MODE_ASYNC,
                   XCB_GRAB_MODE_ASYNC);
    }
    free(enter_codes_);
  }
  else
  {
    std::cerr << "Warning: could not map Enter keysym to key code!\n";
  }

  xcb_grab_button(conn_,
                  0,
                  root_,
                  XCB_EVENT_MASK_BUTTON_PRESS |
                    XCB_EVENT_MASK_BUTTON_RELEASE |
                    XCB_EVENT_MASK_POINTER_MOTION,
                  XCB_GRAB_MODE_ASYNC,
                  XCB_GRAB_MODE_ASYNC,
                  XCB_WINDOW_NONE,
                  XCB_CURSOR_NONE,
                  1,
                  XCB_MOD_MASK_1);

  xcb_flush(conn_);

  while ((event_ = xcb_wait_for_event(conn_)))
  {
    switch (event_->response_type & ~0x80)
    {
      case XCB_MAP_REQUEST:
        {
          auto *e = reinterpret_cast<xcb_map_request_event_t *>(event_);
          xcb_map_window(conn_, e->window);

          xcb_flush(conn_);
          break;
        }
      case XCB_CONFIGURE_REQUEST:
        {
          auto *e = reinterpret_cast<xcb_configure_request_event_t *>(event_);

          uint16_t mask = 0;
          uint32_t values[7];
          int      i = 0;

          if (e->value_mask & XCB_CONFIG_WINDOW_X)
          {
            mask |= XCB_CONFIG_WINDOW_X;
            values[i++] = e->x;
          }
          if (e->value_mask & XCB_CONFIG_WINDOW_Y)
          {
            mask |= XCB_CONFIG_WINDOW_Y;
            values[i++] = e->y;
          }
          if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
          {
            mask |= XCB_CONFIG_WINDOW_WIDTH;
            values[i++] = e->width;
          }
          if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
          {
            mask |= XCB_CONFIG_WINDOW_HEIGHT;
            values[i++] = e->height;
          }
          if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
          {
            mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
            values[i++] = e->border_width;
          }
          if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING)
          {
            mask |= XCB_CONFIG_WINDOW_SIBLING;
            values[i++] = e->sibling;
          }
          if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
          {
            mask |= XCB_CONFIG_WINDOW_STACK_MODE;
            values[i++] = e->stack_mode;
          }

          xcb_configure_window(conn_, e->window, mask, values);
          xcb_flush(conn_);
          break;
        }
      case XCB_BUTTON_PRESS:
        {
          auto        *e = reinterpret_cast<xcb_button_press_event_t *>(event_);
          xcb_window_t win = e->child ? e->child : e->event;
          if (win != root_ && win != XCB_WINDOW_NONE)
          {
            focused_ = win;
            xcb_set_input_focus(conn_, XCB_INPUT_FOCUS_POINTER_ROOT, win, XCB_CURRENT_TIME);
            uint32_t values[] = {XCB_STACK_MODE_ABOVE};
            xcb_configure_window(conn_,
                                 win,
                                 XCB_CONFIG_WINDOW_STACK_MODE,
                                 values);
            xcb_set_input_focus(conn_,
                                XCB_INPUT_FOCUS_POINTER_ROOT,
                                win,
                                XCB_CURRENT_TIME);
            xcb_flush(conn_);

            if (e->detail == 1 && (e->state & XCB_MOD_MASK_1) && win != root_ && win != XCB_WINDOW_NONE)
            {
              auto xy = get_window_xy(conn_, win);
              if (xy)
              {
                drag_.active = true;
                drag_.win = win;
                drag_.start_root_x = e->root_x;
                drag_.start_root_y = e->root_y;
                drag_.win_x = xy->first;
                drag_.win_y = xy->second;
              }
            }
          }
          break;
        }
      case XCB_MOTION_NOTIFY:
        {
          auto *e = reinterpret_cast<xcb_motion_notify_event_t *>(event_);
          if (drag_.active && drag_.win != XCB_WINDOW_NONE)
          {
            int dx = e->root_x - drag_.start_root_x;
            int dy = e->root_y - drag_.start_root_y;

            uint32_t values[2];
            values[0] = static_cast<uint32_t>(drag_.win_x + dx);
            values[1] = static_cast<uint32_t>(drag_.win_y + dy);

            xcb_configure_window(conn_,
                                 drag_.win,
                                 XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                                 values);

            xcb_flush(conn_);
          }
          break;
        }
      case XCB_BUTTON_RELEASE:
        {
          auto *e = reinterpret_cast<xcb_button_release_event_t *>(event_);
          (void)e;
          drag_.active = false;
          drag_.win = XCB_WINDOW_NONE;
          break;
        }
      case XCB_KEY_PRESS:
        {
          auto *e = reinterpret_cast<xcb_key_press_event_t *>(event_);
          auto  sym = xcb_key_symbols_get_keysym(keysyms_, e->detail, 0);

          if ((e->state & XCB_MOD_MASK_1) && sym == enter_sym_ && !alt_ret_down_)
          {
            alt_ret_down_ = true;
            utils_.spawn("kitty");
          }

          break;
        }
      case XCB_KEY_RELEASE:
        {
          auto *e = reinterpret_cast<xcb_key_release_event_t *>(event_);
          auto  sym = xcb_key_symbols_get_keysym(keysyms_, e->detail, 0);

          if (sym == enter_sym_)
            alt_ret_down_ = false;
          break;
        }
      case XCB_DESTROY_NOTIFY:
        {
          auto *e = reinterpret_cast<xcb_destroy_notify_event_t *>(event_);
          if (focused_ == e->window)
            focused_ = XCB_WINDOW_NONE;
          if (drag_.win == e->window)
          {
            drag_.active = false;
            drag_.win = XCB_WINDOW_NONE;
          }
          break;
        }
      default:
        // do some thing for debugging
        break;
    }
    free(event_);
  }
  xcb_key_symbols_free(keysyms_);
  xcb_disconnect(conn_);
}

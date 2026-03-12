#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include "Utils.hpp"

struct DragState
{
  bool         active = false;
  xcb_window_t win = XCB_WINDOW_NONE;
  int          start_root_x = 0, start_root_y = 0;
  int          win_x = 0, win_y = 0;
};

class WindowManager
{
private:
  int            screen_n_;
  bool           alt_ret_down_; // for testing
  const uint32_t root_mask_ = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                              XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                              XCB_EVENT_MASK_BUTTON_PRESS |
                              XCB_EVENT_MASK_BUTTON_RELEASE |
                              XCB_EVENT_MASK_POINTER_MOTION |
                              XCB_EVENT_MASK_KEY_PRESS |
                              XCB_EVENT_MASK_STRUCTURE_NOTIFY;

  uint32_t              values_[1] = {root_mask_};
  xcb_connection_t     *conn_;
  const xcb_setup_t    *setup_;
  xcb_screen_iterator_t iter_;
  xcb_screen_t         *screen_;
  xcb_window_t          root_;
  xcb_window_t          focused_;
  xcb_key_symbols_t    *keysyms_;
  const xcb_keysym_t    enter_sym_ = XKB_KEY_Return;
  xcb_keycode_t        *enter_codes_;
  xcb_generic_event_t  *event_;
  xcb_void_cookie_t     cookie_;
  xcb_generic_error_t  *err_;

  DragState drag_;
  Utils     utils_;

  static std::optional<std::pair<int, int>> get_window_xy(xcb_connection_t *conn, xcb_window_t win);

public:
  WindowManager();
  ~WindowManager();

  void loop();
};

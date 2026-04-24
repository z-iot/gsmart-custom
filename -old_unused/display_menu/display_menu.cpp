#include "display_menu.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome
{
  namespace display_menu
  {

    static const char *const TAG = "display_menu";

    void DispBufMenuComponent::dump_config()
    {
      ESP_LOGCONFIG(TAG, "Display Menu");
    }

    void DispBufMenuComponent::draw_item(const display_menu_base::MenuItem *item, uint8_t row, bool selected)
    {
      auto width = this->display_->get_width();
      std::string value;

      //         it.printf(3, 30, id(font1), TextAlign::BOTTOM_LEFT, "%s", WiFi.localIP().toString().c_str());
      //         it.printf(250 / 2, 30, id(font2), TextAlign::BOTTOM_CENTER, "%s", id(state).state.c_str());
      //         it.filled_rectangle(210 - 35, 5, 70, 24);
      //         it.printf(210, 30, id(font2), COLOR_OFF, TextAlign::BOTTOM_CENTER, "%s", id(mode).state.c_str());

      //         it.printf(5, 35, id(font2), "Cleaned today: %.2f m3", id(today_m3));

      //         it.printf(250 / 2, 122 - 40, id(sseg), TextAlign::CENTER, "Sibra %s", get_mac_address().substr(6).c_str());

      //       void get_text_bounds(int x, int y, const char *text, Font *font, TextAlign align, int *x1, int *y1, int *width,
      //                      int *height);

      // char data[this->columns_ + 1];  // Bounded to 65 through the config

      // memset(data, ' ', this->columns_);

      bool editing = selected && (this->editing_ || (this->mode_ == display_menu_base::MENU_MODE_JOYSTICK && item->get_immediate_edit()));

      // if (selected) {
      //   data[0] = (this->editing_ || (this->mode_ == display_menu_base::MENU_MODE_JOYSTICK && item->get_immediate_edit()))
      //                 ? this->mark_editing_
      //                 : this->mark_selected_;
      // }

      switch (item->get_type())
      {
      case display_menu_base::MENU_ITEM_MENU:
        value = this->mark_submenu_;
        break;
      case display_menu_base::MENU_ITEM_BACK:
        value = this->mark_back_;
        break;
      default:
        break;
      }

      std::string text = item->get_text().substr(0, this->columns_ - 2);

      if (item->has_value())
      {
        value = item->get_value_text().substr(0, this->columns_ - 7);
        value = '[' + value + ']';
      }

      int textTop = (row * this->row_height_) -3; 
      if (selected)
      {
        this->display_->filled_rectangle(0, row * this->row_height_, width, this->row_height_);
        this->display_->print(2, textTop, this->font_, display::COLOR_OFF, display::TextAlign::TOP_LEFT, text.c_str());

        if (editing)
        {
          // this->display_->get_text_bounds(int x, int y, const char *text, Font *font, TextAlign align, int *x1, int *y1, int *width, int *height);
          value = '<' + value + '>';
        }
        this->display_->print(width - 2, textTop, this->font_, display::COLOR_OFF, display::TextAlign::TOP_RIGHT, value.c_str());
      }
      else
      {
        this->display_->print(2, textTop, this->font_, display::COLOR_ON, display::TextAlign::TOP_LEFT, text.c_str());
        this->display_->print(width - 2, textTop, this->font_, display::COLOR_ON, display::TextAlign::TOP_RIGHT, value.c_str());
      }
    }

  } // namespace display_menu
} // namespace esphome

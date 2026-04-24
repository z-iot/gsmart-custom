#pragma once

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display_menu_base/display_menu_base.h"

#include <forward_list>
#include <vector>

namespace esphome
{
  namespace display_menu
  {

    class DispBufMenuComponent : public display_menu_base::DisplayMenuComponent
    {
    public:
      void set_display(display::DisplayBuffer *display) { this->display_ = display; }
      void set_font(display::BaseFont *font) { this->font_ = font; }
      // void set_font(display::Font *font) { this->font_ = font; }
      void set_row_height(uint8_t row_height)
      {
        this->row_height_ = row_height;
        int fw, fos, fbl, fh;
        this->font_->measure("0", &fw, &fos, &fbl, &fh);
        this->columns_ = this->display_->get_width() / fw;
        set_rows(this->display_->get_height() / this->row_height_);
      }

      void dump_config() override;

    protected:
      void draw_item(const display_menu_base::MenuItem *item, uint8_t row, bool selected) override;
      void update() override
      {
        // auto comp = reinterpret_cast<PollingComponent*>(this->display_);
        // comp->update();
        // if (this->display_ is *Component)
        //   ((*Component)this->display_)->update();
      }

      display::DisplayBuffer *display_;
      display::BaseFont *font_{nullptr};
      //display::Font *font_{nullptr};
      uint8_t row_height_;
      uint8_t columns_;

      char mark_selected_ = '>';
      char mark_editing_ = '*';
      char mark_submenu_ = '~';
      char mark_back_ = '^';
    };

  } // namespace display_menu
} // namespace esphome

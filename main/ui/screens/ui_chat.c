#include "../ui_init.h"

static const char *TAG="chat_screen";

lv_obj_t * chatcreen_create(void)
{
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_t *chat_screen = lv_obj_create(NULL);

    lv_obj_t *chat_contianer = lv_obj_create(chat_screen);
    lv_obj_set_size(chat_contianer, 240, 240);
    lv_obj_set_style_bg_color(chat_contianer, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_flex_flow(chat_contianer, LV_FLEX_FLOW_COLUMN);

    chat_list = lv_list_create(chat_contianer);
    lv_obj_set_size(chat_list, 220,220);
    lv_obj_center(chat_list);
    lv_obj_set_style_pad_row(chat_list, 10, 0);   //每个对话框间隔10像素

    // chat_button1 = lv_button_create(chat_list);
    // lv_obj_align(chat_button1, LV_ALIGN_LEFT_MID, 10, 0);
    // lv_obj_set_width(chat_button1, LV_PCT(100));

    // chat_textarea1 = lv_textarea_create(chat_button1);
    // lv_obj_set_width(chat_textarea1, LV_PCT(100));
    // lv_obj_set_height(chat_textarea1, LV_SIZE_CONTENT); // 动态高度
    // lv_textarea_set_one_line(chat_textarea1, false);    // 开启多行模式
    // //lv_textarea_add_text(chat_textarea1, "laiyang jdoqhfnie djqojfoevh owddjnoovh");
    // lv_obj_scroll_to_view(chat_textarea1, LV_ANIM_ON);
    // lv_obj_set_style_text_font(chat_textarea1, &ui_test_24_data, 0);

    // chat_button2 = lv_button_create(chat_list);
    // lv_obj_set_width(chat_button2, LV_PCT(100));
    // lv_obj_align(chat_button2, LV_ALIGN_RIGHT_MID, -10, 0);

    // chat_textarea2 = lv_textarea_create(chat_button2);
    // //lv_obj_align(ta2, LV_ALIGN_RIGHT_MID, -10, 0);
    // lv_obj_set_width(chat_textarea2, LV_PCT(100));
    // lv_obj_set_height(chat_textarea2, LV_SIZE_CONTENT); // 动态高度
    // lv_textarea_set_one_line(chat_textarea2, false);    // 开启多行模式
    // //lv_textarea_add_text(chat_textarea2, "QODMLOWOQNO\nDJMOWNDWNFI\nKDIOWFNLP");
    // lv_obj_set_style_text_font(chat_textarea2, &ui_test_24_data, 0);
    // lv_obj_scroll_to_view(chat_button2, LV_ANIM_ON);


    lvgl_port_unlock();
    return chat_screen;
}
//对话界面创建单个对话框
void chatcreen_create_chat(uint8_t chat_index,const char *text)
{
    lvgl_port_lock(portMAX_DELAY);
    chat_button[chat_index] = lv_button_create(chat_list);             //基于chat_list建立chat_button
    lv_obj_align(chat_button[chat_index], LV_ALIGN_LEFT_MID, 10, 0);   
    lv_obj_set_width(chat_button[chat_index], LV_PCT(100));
    lv_obj_scroll_to_view(chat_button[chat_index], LV_ANIM_OFF);

    chat_textarea[chat_index] = lv_textarea_create(chat_button[chat_index]);      //基于chat_button建立chat_textarea
    lv_obj_set_width(chat_textarea[chat_index], LV_PCT(100));
    lv_obj_set_height(chat_textarea[chat_index], LV_SIZE_CONTENT);     // 动态高度
    lv_textarea_set_one_line(chat_textarea[chat_index], false);        // 开启多行模式
    lv_obj_set_style_text_font(chat_textarea[chat_index], &ui_test_24_data, 0);
    lv_obj_scroll_to_view(chat_textarea[chat_index], LV_ANIM_OFF);
    if(text!=NULL)
    {
        lv_textarea_add_text(chat_textarea[chat_index], text);
    }
    lvgl_port_unlock();
}
//向对话框中添加文本内容
void chatcreen_chat_add_text(uint8_t chat_index,const char *text)
{
    if(chat_button[chat_index]==NULL||chat_textarea[chat_index]==NULL)
    {
        ESP_LOGE(TAG,"chat_button or chat_textarea is NULL");
        return;
    }
    if(text==NULL)
    {
        ESP_LOGE(TAG,"chat text is NULL");
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    lv_textarea_add_text(chat_textarea[chat_index], text);
    // lv_obj_scroll_to_y(chat_list, lv_obj_get_scroll_bottom(chat_list), LV_ANIM_ON);
    lvgl_port_unlock();
}
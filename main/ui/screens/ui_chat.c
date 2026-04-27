#include "../ui_init.h"

static const char *TAG="chat_screen";
static const int32_t CHAT_TEXTAREA_MAX_HEIGHT = 140;

static void chatscreen_scroll_latest(uint8_t chat_index, lv_anim_enable_t anim_en)
{
    if (chat_button[chat_index] == NULL || chat_textarea[chat_index] == NULL || chat_list == NULL) {
        return;
    }

    lv_obj_move_foreground(chat_button[chat_index]);
    lv_textarea_set_cursor_pos(chat_textarea[chat_index], LV_TEXTAREA_CURSOR_LAST);

    lv_obj_update_layout(chat_textarea[chat_index]);
    lv_obj_update_layout(chat_button[chat_index]);
    lv_obj_update_layout(chat_list);

    lv_obj_scroll_to_view_recursive(chat_button[chat_index], anim_en);
}

lv_obj_t * chatcreen_create(void)
{
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_t *chat_screen = lv_obj_create(NULL);

    lv_obj_t *chat_contianer = lv_obj_create(chat_screen);
    lv_obj_set_size(chat_contianer, 240, 240);
    lv_obj_set_style_bg_color(chat_contianer, lv_color_hex(0x030303), 0);
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
    chat_index=chat_index%chat_index_max;                           //对话索引，对chat_index_max取余
    if(chat_button[chat_index]!=NULL)   //说明该index已经有对话框了
    {
        if(text==NULL)
        {
            lv_textarea_set_text(chat_textarea[chat_index],"");//重新设置该文本区域的文本为空
        }
        else
        {
            lv_textarea_set_text(chat_textarea[chat_index], text);//重新设置该文本区域的文本
        }
    }
    else{   //没有对应的chat_list和chat_button，则创建
        chat_button[chat_index] = lv_button_create(chat_list);             //基于chat_list建立chat_button
        lv_obj_align(chat_button[chat_index], LV_ALIGN_LEFT_MID, 10, 0);   
        lv_obj_set_width(chat_button[chat_index], LV_PCT(100));
        lv_obj_set_height(chat_button[chat_index], LV_SIZE_CONTENT);
        lv_obj_scroll_to_view_recursive(chat_button[chat_index], LV_ANIM_OFF);
        //ESP_LOGE(TAG,"index:%d,value:%d",chat_index,lv_obj_get_scroll_bottom( chat_button[chat_index]));

        chat_textarea[chat_index] = lv_textarea_create(chat_button[chat_index]);      //基于chat_button建立chat_textarea
        lv_obj_set_width(chat_textarea[chat_index], LV_PCT(100));
        lv_obj_set_height(chat_textarea[chat_index], LV_SIZE_CONTENT);     // 动态高度
        lv_obj_set_style_max_height(chat_textarea[chat_index], CHAT_TEXTAREA_MAX_HEIGHT, 0);
        lv_obj_set_scroll_dir(chat_textarea[chat_index], LV_DIR_VER);
        lv_obj_set_scrollbar_mode(chat_textarea[chat_index], LV_SCROLLBAR_MODE_AUTO);
        lv_textarea_set_one_line(chat_textarea[chat_index], false);        // 开启多行模式
        lv_obj_set_style_text_font(chat_textarea[chat_index], &ui_font_20, 0);
        // lv_obj_scroll_to_view(chat_textarea[chat_index], LV_ANIM_OFF);
        //lv_obj_scroll_to_view_recursive(chat_textarea[chat_index], LV_ANIM_OFF);
        if(text!=NULL)
        {
            lv_textarea_add_text(chat_textarea[chat_index], text);
        }
    }
    chatscreen_scroll_latest(chat_index, LV_ANIM_ON);
    lvgl_port_unlock();
}
// //向对话框中添加文本内容
// void chatcreen_chat_add_text(uint8_t chat_index,const char *text)
// {
//     chat_index=chat_index%chat_index_max;              //对话索引，对chat_index_max取余
//     if(chat_button[chat_index]==NULL||chat_textarea[chat_index]==NULL)
//     {
//         ESP_LOGE(TAG,"chat_button or chat_textarea is NULL");
//         return;
//     }
//     if(text==NULL)
//     {
//         ESP_LOGE(TAG,"chat text is NULL");
//         return;
//     }
//     lvgl_port_lock(portMAX_DELAY);
//     lv_textarea_add_text(chat_textarea[chat_index], text);
//     lv_obj_move_foreground(chat_button[chat_index]);
//     //lv_textarea_set_cursor_pos(chat_textarea[chat_index], LV_TEXTAREA_CURSOR_LAST);
//     lv_obj_scroll_to_view(chat_button[chat_index], LV_ANIM_ON);
//     lv_obj_scroll_to_view(chat_textarea[chat_index], LV_ANIM_ON);
//     //lv_obj_scroll_to_y(chat_list, lv_obj_get_scroll_bottom(chat_list), LV_ANIM_ON);//list列表有scroll的属性，才可以获取，不然返回0
//     // lv_obj_scroll_to_y(chat_list, lv_obj_get_scroll_bottom(chat_list), LV_ANIM_ON);
//     lvgl_port_unlock();
// }
//向对话框中添加文本内容
void chatcreen_chat_add_text(uint8_t chat_index,const char *text)
{
    chat_index=chat_index%chat_index_max;              //对话索引，对chat_index_max取余
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
    chatscreen_scroll_latest(chat_index, LV_ANIM_ON);
    lvgl_port_unlock();
}

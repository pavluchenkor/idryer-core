/**
 * @file card_menu_bridge.h
 * @brief Чтение меню продукта для параметров действий карточки.
 *
 * Header-only, как menu_publisher.h: подключается из продукта ПОСЛЕ его
 * menu_meta.h / menu_cache.h — у каждого продукта своё меню, у ядра его нет.
 *
 *   #include <menu_cache.h>
 *   #include <card/card_menu_bridge.h>
 *   idryer::card_menu::attach(link.card());
 *
 * Из меню берутся только числа: min/max/step пункта и текущее значение юнита
 * как значение по умолчанию. Карточка в меню не пишет.
 */

#pragma once

#include "card_builder.h"

namespace idryer {
namespace card_menu {

inline bool read(uint16_t id, uint8_t unit, CardBuilder::MenuValue& out) {
    if (id >= MENU_META_COUNT) return false;
    const MenuMeta* m = &g_menu_meta[id];
    if (m->type != META_VALUE && m->type != META_TOGGLE) return false;
    out.min   = m->min_val;
    out.max   = m->max_val;
    out.step  = m->step;
    out.value = g_menu_cache.getFloat(id, unit);
    // Единица — английская форма: манифест уходит на портал как есть.
    out.unit  = (MENU_LANG_COUNT > 1 && m->unit[1]) ? m->unit[1] : m->unit[0];
    // Заголовок на всех языках меню — порядок совпадает с NAME_LANG_CODES.
    for (uint8_t i = 0; i < CardBuilder::NAME_LANGS && i < MENU_LANG_COUNT; ++i) {
        out.name[i] = m->title[i];
    }
    return true;
}

inline void attach(CardBuilder& card) { card.setMenuReader(&read); }

} // namespace card_menu
} // namespace idryer

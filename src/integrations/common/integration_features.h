#pragma once

/**
 * @file integration_features.h
 * @brief Какие интеграции собираются в образ прошивки.
 *
 * Отключённой интеграции у прибора не существует: клиент не создаётся, код
 * на него не ссылается и вычищается компоновщиком, команда портала на неё
 * отклоняется, в `integrations/status` она не объявляется. Флаги задаются
 * в platformio.ini конкретной прошивки, по умолчанию собирается всё.
 *
 * @code
 * build_flags = -DIDRYER_WITH_BAMBU=0 -DIDRYER_WITH_MOONRAKER=0
 * @endcode
 */

#ifndef IDRYER_WITH_HA
#define IDRYER_WITH_HA 1
#endif

#ifndef IDRYER_WITH_BAMBU
#define IDRYER_WITH_BAMBU 1
#endif

#ifndef IDRYER_WITH_MOONRAKER
#define IDRYER_WITH_MOONRAKER 1
#endif

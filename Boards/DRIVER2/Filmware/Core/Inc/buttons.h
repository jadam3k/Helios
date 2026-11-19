#ifndef FILMWARE_BUTTONS_H
#define FILMWARE_BUTTONS_H

#include <stdint.h>

enum
{
    BUTTON_UP   = 1U << 0,
    BUTTON_DOWN = 1U << 1,
    BUTTON_OK   = 1U << 2
};

/* Call after MX_GPIO_Init(). */
void Buttons_Init(void);

/* Returns a mask of newly pressed buttons. Call from the main loop. */
uint8_t Buttons_Poll(uint32_t now_ms);

#endif

#include "buttons.h"
#include "main.h"

#define BUTTON_DEBOUNCE_MS 25U

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t mask;
    GPIO_PinState candidate;
    GPIO_PinState stable;
    uint32_t candidate_since_ms;
} Button;

static Button buttons[] =
{
    { .port = BTN_UP_GPIO_Port,   .pin = BTN_UP_Pin,   .mask = BUTTON_UP },
    { .port = BTN_DOWN_GPIO_Port, .pin = BTN_DOWN_Pin, .mask = BUTTON_DOWN },
    { .port = BTN_OK_GPIO_Port,   .pin = BTN_OK_Pin,   .mask = BUTTON_OK }
};

void Buttons_Init(void)
{
    const uint32_t now_ms = HAL_GetTick();

    for (uint32_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
    {
        Button *button = &buttons[i];
        button->candidate = HAL_GPIO_ReadPin(button->port, button->pin);
        button->stable = button->candidate;
        button->candidate_since_ms = now_ms;
    }
}

uint8_t Buttons_Poll(uint32_t now_ms)
{
    uint8_t pressed = 0U;

    for (uint32_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
    {
        Button *button = &buttons[i];
        const GPIO_PinState raw = HAL_GPIO_ReadPin(button->port, button->pin);

        if (raw != button->candidate)
        {
            button->candidate = raw;
            button->candidate_since_ms = now_ms;
        }

        if (button->candidate != button->stable &&
            (now_ms - button->candidate_since_ms) >= BUTTON_DEBOUNCE_MS)
        {
            button->stable = button->candidate;
            if (button->stable == GPIO_PIN_RESET)
            {
                pressed |= button->mask;
            }
        }
    }

    return pressed;
}

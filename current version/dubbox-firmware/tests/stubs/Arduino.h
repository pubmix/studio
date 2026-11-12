#pragma once
#include <cstdint>
#include <cstring>
#include <cstddef>
inline bool irq_enabled = true;
#define IRQ_SOFTWARE 0
#define NVIC_IS_ENABLED(x) irq_enabled
#define NVIC_DISABLE_IRQ(x) (irq_enabled = false)
#define NVIC_ENABLE_IRQ(x) (irq_enabled = true)

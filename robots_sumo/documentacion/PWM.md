11/8/26
¿que es PWM?
La Modulación por Ancho de Pulso (PWM) es una técnica utilizada para simular una señal analógica variando el tiempo en que una señal digital permanece en alto (ON) o bajo (OFF). El porcentaje de tiempo en estado alto se conoce como Ciclo de Trabajo (Duty Cycle).
¿cuales son las reglas basicas de conexion?￼
Reglas básicas de conexión
 Pines PWM del microcontrolador: Identifica los pines compatibles con PWM. En plataformas como Arduino UNO, están marcados con el símbolo tilde (⁠~⁠) en los pines 3, 5, 6, 9, 10 y 11.
 Masa Común (GND): Si usas una fuente de alimentación externa para alimentar motores o tiras LED, debes unir la masa (GND) del microcontrolador con la masa de la fuente externa.

El periférico de hardware PWM en la Raspberry Pi Pico se divide en 8 Slices, y cada slice tiene 2 Canales (A y B). Cada pin GPIO está asignado fijamente a un slice y a un canal.

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define PIN_PWM 15  // Pin GPIO que utilizaremos para la señal PWM

int main() {
    stdio_init_all();

    // 1. Asignar la función PWM al pin GPIO
    gpio_set_function(PIN_PWM, GPIO_FUNC_PWM);

    // 2. Obtener el 'slice' y el 'canal' de hardware asignados a este pin
    uint slice_num = pwm_gpio_to_slice_num(PIN_PWM);
    uint canal = pwm_gpio_to_channel(PIN_PWM);

    // 3. Configurar la Frecuencia de la señal PWM
    // El reloj base de la Pico es de 125 MHz.
    // Frecuencia PWM = 125,000,000 / (clkdiv * (wrap + 1))
    
    pwm_set_clkdiv(slice_num, 125.0f);  // Divisor de reloj -> Reloj PWM = 1 MHz
    pwm_set_wrap(slice_num, 999);       // Límite de conteo (0 a 999) -> Frecuencia = 1 kHz

    // 4. Establecer el Ciclo de Trabajo (Duty Cycle) inicial
    // Un valor de 500 en un rango de 0-999 representa el 50% de ciclo de trabajo
    pwm_set_chan_level(slice_num, canal, 500);

    // 5. Habilitar la generación de la señal PWM en el slice
    pwm_set_enabled(slice_num, true);

    // Bucle principal: Efecto de variación de brillo (fading)
    while (true) {
        // Aumentar brillo del 0% al 100%
        for (int duty = 0; duty <= 1000; duty += 10) {
            pwm_set_chan_level(slice_num, canal, duty);
            sleep_ms(10);
        }

        // Disminuir brillo del 100% al 0%
        for (int duty = 1000; duty >= 0; duty -= 10) {
            pwm_set_chan_level(slice_num, canal, duty);
            sleep_ms(10);
        }
    }

    return 0;
}

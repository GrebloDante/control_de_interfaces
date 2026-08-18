11/8/26
¿que es PWM?
La Modulación por Ancho de Pulso (PWM) es una técnica utilizada para simular una señal analógica variando el tiempo en que una señal digital permanece en alto (ON) o bajo (OFF). El porcentaje de tiempo en estado alto se conoce como Ciclo de Trabajo (Duty Cycle).
¿cuales son las reglas basicas de conexion?￼
Reglas básicas de conexión
 Pines PWM del microcontrolador: Identifica los pines compatibles con PWM. En plataformas como Arduino UNO, están marcados con el símbolo tilde (⁠~⁠) en los pines 3, 5, 6, 9, 10 y 11.
 Masa Común (GND): Si usas una fuente de alimentación externa para alimentar motores o tiras LED, debes unir la masa (GND) del microcontrolador con la masa de la fuente externa.

El periférico de hardware PWM en la Raspberry Pi Pico se divide en 8 Slices, y cada slice tiene 2 Canales (A y B). Cada pin GPIO está asignado fijamente a un slice y a un canal.

#include "pico/stdlib.h"
#include "hardware/pwm.h"
int main() {

    gpio_init(16);
    gpio_init(17);

    gpio_set_dir(16, GPIO_OUT);
    gpio_set_dir(17, GPIO_OUT);

    gpio_set_function(15, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(15);

    pwm_set_wrap(slice, 999);
    pwm_set_gpio_level(15, 500);
    pwm_set_enabled(slice, true);

    gpio_put(16, 1);
    gpio_put(17, 0);

    sleep_ms(2000)
    pwm_set_wrap(slice, 999);
    pwm_set_gpio_level(15, 250);
    pwm_set_enabled(slice, true);

    while(true) {}
}

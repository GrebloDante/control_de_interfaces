#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"   // Librería para manejar el chip WiFi CYW43439 de la Pico W
#include "hardware/pwm.h"       // Para generar señales PWM (control de velocidad de motores)
#include "lwip/udp.h"           // Stack de red lwIP - funciones UDP
#include "lwip/pbuf.h"          // Manejo de buffers de paquetes de red

// ==================== CONFIGURACIÓN WiFi ====================
#define WIFI_SSID "NOMBRE_DE_TU_RED"
#define WIFI_PASSWORD "TU_PASSWORD"
#define UDP_PORT 4444            // Puerto donde la Pico va a "escuchar" comandos

// ==================== PINES MOTOR A ====================
#define ENA_PIN 2   // Pin PWM que controla la VELOCIDAD del motor A
#define IN1_PIN 3   // Pin digital que controla el SENTIDO de giro (junto con IN2)
#define IN2_PIN 4

// ==================== PINES MOTOR B ====================
#define ENB_PIN 6   // Pin PWM que controla la VELOCIDAD del motor B
#define IN3_PIN 7   // Pin digital que controla el SENTIDO de giro (junto con IN4)
#define IN4_PIN 8

// PWM_WRAP define la resolución del PWM: el contador interno cuenta de 0 a PWM_WRAP
// y después vuelve a 0. Con clock de 125MHz y wrap=4999, la frecuencia de PWM
// queda en ~25kHz, que es rápido y silencioso para motores DC.
#define PWM_WRAP 4999

// ---------- Variables globales para el manejo del PWM ----------
// Cada pin PWM en la Pico pertenece a un "slice" (bloque de hardware) y un "channel"
// (A o B dentro de ese slice). Necesitamos guardarlos para poder ajustar la velocidad
// después, dentro del loop principal.
uint slice_a, slice_b, chan_a, chan_b;

// ==================== INICIALIZACIÓN DE PWM ====================
// Configura un pin GPIO para que funcione como salida PWM (en vez de digital simple)
void motor_pwm_init(uint gpio, uint *slice, uint *chan) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);      // Le decimos al pin que va a hacer PWM, no GPIO normal
    *slice = pwm_gpio_to_slice_num(gpio);        // Averiguamos a qué slice de hardware pertenece este pin
    *chan = pwm_gpio_to_channel(gpio);           // Averiguamos a qué canal (A o B) pertenece
    pwm_set_wrap(*slice, PWM_WRAP);              // Configuramos la resolución/frecuencia del PWM
    pwm_set_chan_level(*slice, *chan, 0);        // Arrancamos con el motor apagado (0% duty cycle)
    pwm_set_enabled(*slice, true);               // Activamos el PWM en ese slice
}

// Inicializa todos los pines relacionados a los motores: los de dirección (digitales)
// y los de velocidad (PWM)
void motors_init() {
    // Pines de dirección: son GPIO normales, salida digital ON/OFF
    gpio_init(IN1_PIN); gpio_set_dir(IN1_PIN, GPIO_OUT);
    gpio_init(IN2_PIN); gpio_set_dir(IN2_PIN, GPIO_OUT);
    gpio_init(IN3_PIN); gpio_set_dir(IN3_PIN, GPIO_OUT);
    gpio_init(IN4_PIN); gpio_set_dir(IN4_PIN, GPIO_OUT);

    // Pines de velocidad: se configuran como PWM
    motor_pwm_init(ENA_PIN, &slice_a, &chan_a);
    motor_pwm_init(ENB_PIN, &slice_b, &chan_b);
}

// ==================== CONTROL DE MOTORES ====================
// Mueve el motor A. "forward" define el sentido (1 = adelante, 0 = atrás)
// "speed" es un porcentaje de 0 a 100
void set_motor_a(int forward, int speed) {
    // El sentido de giro en un puente H se logra invirtiendo la polaridad
    // entre IN1 e IN2. Si uno está en HIGH y el otro en LOW, gira para un lado;
    // si se invierten, gira para el otro lado.
    gpio_put(IN1_PIN, forward ? 1 : 0);
    gpio_put(IN2_PIN, forward ? 0 : 1);

    // Convertimos el porcentaje (0-100) a un valor real dentro del rango del PWM (0-PWM_WRAP)
    pwm_set_chan_level(slice_a, chan_a, (PWM_WRAP * speed) / 100);
}

// Mismo concepto pero para el motor B
void set_motor_b(int forward, int speed) {
    gpio_put(IN3_PIN, forward ? 1 : 0);
    gpio_put(IN4_PIN, forward ? 0 : 1);
    pwm_set_chan_level(slice_b, chan_b, (PWM_WRAP * speed) / 100);
}

// Frena ambos motores llevando el PWM a 0 (deja de darles corriente por el puente H)
void motors_stop() {
    pwm_set_chan_level(slice_a, chan_a, 0);
    pwm_set_chan_level(slice_b, chan_b, 0);
}

// ==================== INTERPRETACIÓN DE COMANDOS ====================
// Traduce un comando de una sola letra (F, B, L, R, S) a movimientos concretos
// de los dos motores. Esta es la "lógica de movimiento" del robot.
//
// F = Forward (adelante): ambos motores giran hacia adelante
// B = Backward (atrás): ambos motores giran hacia atrás
// L = Left (giro izquierda): un motor adelante, el otro atrás (gira en el lugar)
// R = Right (giro derecha): al revés que L
// S = Stop: frena todo
void handle_command(char cmd, int speed) {
    switch (cmd) {
        case 'F':
            set_motor_a(1, speed);
            set_motor_b(1, speed);
            break;
        case 'B':
            set_motor_a(0, speed);
            set_motor_b(0, speed);
            break;
        case 'L':
            set_motor_a(0, speed);
            set_motor_b(1, speed);
            break;
        case 'R':
            set_motor_a(1, speed);
            set_motor_b(0, speed);
            break;
        case 'S':
        default:
            motors_stop();
            break;
    }
}

// ==================== CALLBACK DE RECEPCIÓN UDP ====================
// Esta función se ejecuta AUTOMÁTICAMENTE cada vez que llega un paquete UDP
// al puerto que configuramos. lwIP la llama por nosotros - no la llamamos manualmente.
void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                        const ip_addr_t *addr, u16_t port) {
    if (p != NULL) {
        // Copiamos el contenido del paquete (el "payload") a un buffer local de texto
        char buf[32] = {0};
        int len = p->len < 31 ? p->len : 31;  // Evitamos desbordar el buffer
        memcpy(buf, p->payload, len);
        buf[len] = '\0';  // Terminamos el string en null para poder tratarlo como texto

        printf("Comando recibido: %s desde %s\n", buf, ipaddr_ntoa(addr));

        // Protocolo definido por nosotros: primer caracter = comando,
        // resto = velocidad en porcentaje. Ej: "F80" = Forward al 80%
        char cmd = buf[0];
        int speed = 70; // velocidad por defecto si no se especifica
        if (len > 1) {
            speed = atoi(&buf[1]);
            // Nos aseguramos que la velocidad quede en un rango válido
            if (speed < 0) speed = 0;
            if (speed > 100) speed = 100;
        }

        // Ejecutamos el comando ya interpretado
        handle_command(cmd, speed);

        // MUY IMPORTANTE: liberar el buffer del paquete una vez que lo usamos,
        // sino se va llenando la memoria y el programa termina crasheando
        pbuf_free(p);
    }
}

// ==================== PROGRAMA PRINCIPAL ====================
int main() {
    stdio_init_all();   // Inicializa entrada/salida (USB serie para printf)
    sleep_ms(2000);     // Pequeña espera para poder abrir el monitor serie a tiempo

    // Preparamos los motores ANTES de conectar el WiFi, así el robot
    // arranca siempre detenido y no se mueve solo por accidente
    motors_init();
    motors_stop();

    printf("Iniciando WiFi...\n");

    // Inicializa el chip WiFi (CYW43439). Si falla, no tiene sentido seguir.
    if (cyw43_arch_init()) {
        printf("Error al iniciar el chip WiFi\n");
        return -1;
    }

    // Modo "estación" (STA) = la Pico se conecta a una red existente,
    // como si fuera un celular más conectándose al router de casa.
    // (La alternativa sería modo AP, donde la Pico crea su propia red)
    cyw43_arch_enable_sta_mode();
    printf("Conectando a %s...\n", WIFI_SSID);

    // Intenta conectarse a la red WiFi, con timeout de 10 segundos
    if (cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID, WIFI_PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Fallo la conexion WiFi\n");
        return -1;
    }

    // Si llegamos acá, ya estamos conectados. Imprimimos la IP asignada
    // por DHCP - esta es la IP que vas a usar desde la PC/celular para
    // mandarle comandos al robot
    printf("Conectado! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

    // Prendemos el LED de la propia placa como indicador visual de "conectado OK"
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    // ---------- Configuración del servidor UDP ----------
    struct udp_pcb *pcb = udp_new();              // Creamos un "socket" UDP
    udp_bind(pcb, IP_ADDR_ANY, UDP_PORT);          // Lo ligamos al puerto definido, aceptando de cualquier IP
    udp_recv(pcb, udp_recv_callback, NULL);        // Le decimos qué función llamar cuando llegue un paquete

    printf("Escuchando UDP en puerto %d\n", UDP_PORT);

    // ---------- Loop principal ----------
    while (true) {
        // cyw43_arch_poll() es CLAVE: es lo que efectivamente procesa
        // los eventos de red (recibir paquetes, ejecutar el callback, etc).
        // Sin esta línea, el callback UDP nunca se dispara.
        cyw43_arch_poll();
        sleep_ms(1); // pequeña pausa para no saturar la CPU en el loop
    }

    cyw43_arch_deinit();
    return 0;
}

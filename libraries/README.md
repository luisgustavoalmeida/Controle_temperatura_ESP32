# Bibliotecas locais (Arduino IDE)

Estas pastas têm prioridade sobre `Documents/Arduino/libraries/` quando o sketch está na mesma árvore do projeto.

## OneWire 2.3.8 (Paul Stoffregen)

Versão compatível com **ESP32 core 3.x**. Se a compilação falhar com erros em `GPIO` / `OneWire_direct_gpio.h`, remova bibliotecas antigas duplicadas no Arduino IDE, por exemplo:

- `Documents/Arduino/libraries/arduino_514513` (apagar ou renomear)

O teste `teste_ds18b20` também traz cópia em `teste_ds18b20/libraries/OneWire/` ao abrir só esse sketch.

**DallasTemperature** continua vindo do Gerenciador de bibliotecas (Miles Burton).

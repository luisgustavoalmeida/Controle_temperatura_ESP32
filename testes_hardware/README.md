# Testes de hardware (Arduino IDE)

Sketches para validar cada periférico **antes** do firmware principal. Todos usam os pinos de [`../config.h`](../config.h).

**Acompanhe todos os resultados no Monitor Serial: 115200 baud** (Ferramentas → Monitor Serial).

## Configuração na Arduino IDE

1. Instale o suporte **ESP32** (Gerenciador de placas → Espressif).
2. Instale as bibliotecas do sketch (Gerenciador de bibliotecas):
   - **DallasTemperature** (Miles Burton) — teste DS18B20  
   - **OneWire**: use a pasta `libraries/OneWire` do repositório (já incluída). Apague `arduino_514513` em Documentos/Arduino/libraries se der erro de compilação no ESP32 3.x.
   - **NewLiquidCrystal** (ou **LiquidCrystal I2C** marcoschwartz) — teste LCD; veja `LCD_USE_NEW_LIQUIDCRYSTAL` em `config.h`
3. Abra **somente** o `.ino` da pasta do teste (Arquivo → Abrir).
4. Placa: **ESP32 Dev Module** | Porta: COM do seu ESP32.
5. Após gravar, abra o **Monitor Serial** em **115200** e newline **NL+CR** ou **Somente NL**.

Grave **um teste por vez**. Pode deixar só o hardware daquele teste ligado para isolar falhas.

## Ordem sugerida

| # | Pasta | Monitor Serial mostra | Confirme também |
|---|-------|------------------------|-----------------|
| 1 | `teste_buzzer/` | Início dos bipes / ciclo OK | Ouve 3 bipes a cada 2 s |
| 2 | `teste_lcd/` | Inicialização I2C + contador | Texto nas 4 linhas do LCD |
| 3 | `teste_ds18b20/` | °C ou mensagem de erro | Valor coerente com ambiente |
| 4 | `teste_encoder/` | Rotação com/sem botão e cliques | Gire, segure SW e gire, ou clique rápido |
| 5 | `teste_x9c503/` | X9C104S — afericao completa (`a`) e validacao (`v`) | Multímetro VL–VW |
| 6 | `teste_x9c104_exemplo/` | **Exemplo limpo** — API documentada, incrementos e kΩ | Multímetro VL–VW |

## O que esperar no Monitor Serial

### teste_buzzer
```
[OK] Bipes 1/3 ... 3/3 concluidos — escute o buzzer
[OK] Ciclo completo. Proximo em 2 s.
```

### teste_lcd
```
[INFO] Varredura I2C ...
[OK] Endereco LCD_I2C_ADDR responde.
[OK] Contador LCD: 1, 2, 3 ...
```
Se o display **piscar sem texto**: em `config.h` use `LCD_PCF8574_LAYOUT_YWROBOT 1` e confira o endereco I2C (`0x27` ou `0x3F` na varredura). Ajuste o **trimpot de contraste**.

### teste_ds18b20
```
[OK] Sensor encontrado: 1
[OK] Temperatura: 25.50 C
```
ou `[ERRO]` se não houver sensor.

### teste_encoder
```
[OK] Rotacao sem botao: +1 | Total sem botao: 5
[OK] Rotacao COM botao: -1 | Total com botao: 3
[OK] Botao SW: clique #1 (sem rotacao)
```
Segure SW e gire para validar rotacao com botao pressionado.

### teste_x9c503 (X9C104S)

**Validação rápida (recomendado agora):** digite **`v`** (~5 min, 9 pontos).

**Aferição completa:** digite **`a`** (tabela longa, ~20 min).

1. O firmware move o wiper e imprime uma **linha da tabela** a cada passo.
2. Meça **VL–VW** (pinos 6–5) e digite o valor em kΩ (`45,5`) + Enter.
3. Enter vazio = pular; **`q`** = abortar (tabela parcial ainda é impressa).
4. No fim, copie o bloco **TABELA AFERICAO** e cole no chat.

**Busca manual do fim/início:** `b max` (a partir do passo 99) ou `b min` (a partir do 0) — use `+`, `-`, `+5` com pulsos extras; `t` imprime linha para anotar; `q` sai.

Modo interativo normal:
```
40      -> ajusta VL-VW ~ passo equivalente
+ / -   -> pulsos (envia mesmo no 0 ou 99)
p50     -> passo 50
h       -> homing
m up    -> so borda de subida (padrao)
```

Depois de todos os testes OK, grave o firmware principal: `Controle_temperatura_ESP32.ino`.

### teste_x9c104_exemplo (referencia de codigo)

Sketch **limpo** com driver documentado (`x9c_exemplo.h` / `.cpp`) — use como modelo no seu projeto.

```
h       -> zero
m       -> maximo (~91,8 kΩ)
p50     -> passo 50
45.5    -> alvo em kΩ
+ / -   -> incremento (funciona no 0 e 99)
?       -> ajuda
```

Arquivos:
- `teste_x9c104_exemplo.ino` — Serial e exemplos de uso
- `x9c_exemplo.h` / `x9c_exemplo.cpp` — driver calibrado (SUBIDA, ancora MIN/MAX, +1 extra em cada extremo)

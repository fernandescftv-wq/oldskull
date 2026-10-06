# Regras de Codificação — OldSkull JC4880P433C BSP

Projeto: `/home/e/esp/esp32_p4_jc4880p433c_bsp`
ESP-IDF: v5.5.4
Alvo: `esp32p4`

---

## Regra 1 — Causa raiz antes de qualquer patch

Quando algo dá erro (build, linker, runtime):
1. Ler o arquivo real afetado (não só este doc)
2. Achcar a linha exata, a condição, o que o código faz vs o que deveria fazer
3. Dizer o que encontrou
4. Patchear só o necessário

**Não regenere o arquivo inteiro.** Se o fix é 3 linhas, não mexer nas outras 2000.

---

## Regra 2 — Patch cirúrgico

- Use edição direcionada (`patch` com `old_string`/`new_string` ou equivalente)
- Não reescreva arquivos inteiros "só por via das dúvidas"
- Se uma tentativa anterior reescreveu algo e quebrou, volte ao estado conhecido bom antes de NovoPatch

---

## Regra 3 — Validar no hardware antes de declarar pronto

- Build com `idf.py build`
- Flash com `idf.py -p /dev/ttyACM0 flash` (ou porta correta)
- Monitor com `idf.py -p /dev/ttyACM0 monitor`
- Verificar no display real que o que foi alterado funcionou

---

## Regra 4 — Dump grande = escrever em partes

Arquivos grandes (>~8K tokens) podem ser truncados na gravação. Escrever em partes: base com `write_file`, depois `patch` para acrescentar.

Vários "defined but not used" no linker = suspeitar truncamento do `.c` gerado.

---

## Regra 5 — WiFi não mexer sem motivo

WiFi não está integrado neste build. Não adicionar integração WiFi sem pedido explícito e planejamento. Qualquer mudança fora display/touch/amps/video/de build deve ser discutida primeiro.

---

## Regra 6 — Não quebrar o que já funciona

Estado atual (confirmado funcionando):
- Display ST7701 landscape 800×480 via `sw_rotate=true` + `lv_disp_set_rotation(display, LV_DISP_ROT_90)`
- `swap_xy = false` no `disp_cfg.rotation` (não duplicar rotação)
- Touch GT911 funcional
- Carrossel de 3 amps com swipe vertical
- 3 telas com swipe horizontal
- Video loop 121 frames @ 24fps
- `on_color_trans_done` no lugar de `on_frame_buf_complete` no esp_lvgl_port

Qualquer mudança nesses pontos exige racional claro e teste no hardware.

---

## Regra 7 — Backups antes de mexer

Antes de modificar um arquivo que já tem várias versões (`app_main.c`, `bsp_display.c`), verificar os backups existentes e garantir que há um ponta segura para voltar.

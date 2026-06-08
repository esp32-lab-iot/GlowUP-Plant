## Script do Google Apps Script

O arquivo `apps_script/Code.gs` contém o código usado no Google Apps Script.

Ele faz quatro tarefas principais:

1. recebe dados enviados pelo ESP32;
2. grava os dados na aba `dados`;
3. lê os indicadores da aba `resumo`;
4. gera um diagnóstico com Gemini e grava nas abas `Diagnóstico` e `diagnosticoIA`.

A chave da API Gemini não deve ser colocada no código. Ela deve ser configurada nas propriedades do Apps Script com o nome:

`GEMINI_API_KEY`

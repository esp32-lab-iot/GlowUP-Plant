/**
 * GlowUP Plant - Google Apps Script
 *
 * Este script recebe dados enviados por um ESP32 via requisição HTTP POST,
 * grava as leituras em uma aba do Google Sheets e gera um diagnóstico textual
 * usando a API Gemini.
 *
 * Abas utilizadas:
 * - dados: recebe o histórico de leituras enviadas pelo ESP32.
 * - resumo: contém os dados consolidados usados para gerar o diagnóstico.
 * - Diagnóstico: guarda o histórico de diagnósticos gerados pela IA.
 * - diagnosticoIA: guarda apenas o diagnóstico mais recente, facilitando o uso no Looker Studio.
 *
 * Propriedade de script necessária:
 * - GEMINI_API_KEY: chave da API Gemini armazenada em Propriedades do script.
 */


/**
 * Função executada automaticamente quando o Web App recebe uma requisição POST.
 *
 * No projeto GlowUP Plant, o ESP32 envia um JSON para a URL do Web App.
 * Esta função lê esse JSON e adiciona uma nova linha na aba "dados".
 *
 * Exemplo de JSON esperado:
 * {
 *   "umidade_solo": 55,
 *   "leitura_adc": 1830,
 *   "estado": "solo adequado",
 *   "planta": "Jiboia"
 * }
 *
 * Campos gravados na aba "dados":
 * - Data/hora do recebimento
 * - Umidade do solo em porcentagem
 * - Leitura ADC bruta
 * - Estado calculado pelo ESP32
 * - Nome da planta, se enviado
 */
function doPost(e) {
  try {
    // Converte o corpo da requisição HTTP, recebido como texto, para um objeto JavaScript.
    var data = JSON.parse(e.postData.contents);

    // Obtém a planilha vinculada ao script.
    var planilha = SpreadsheetApp.getActiveSpreadsheet();

    // Obtém a aba onde as leituras serão armazenadas.
    var sheet = planilha.getSheetByName("dados");

    // Registra a data e a hora no momento em que a leitura chega ao Web App.
    var timestamp = new Date();

    // Adiciona uma nova linha ao final da aba "dados".
    // O operador || "" evita erro caso o campo planta não seja enviado pelo ESP32.
    sheet.appendRow([
      timestamp,
      data.umidade_solo,
      data.leitura_adc,
      data.estado,
      data.planta || ""
    ]);

    // Retorna uma mensagem simples para o ESP32 ou para quem chamou o Web App.
    return ContentService.createTextOutput("Dados salvos com sucesso!");

  } catch (error) {
    // Em caso de erro, retorna a mensagem para facilitar o diagnóstico.
    // Exemplos comuns: JSON inválido, aba "dados" inexistente ou erro de permissão.
    return ContentService.createTextOutput("Erro no script: " + error.toString());
  }
}


/**
 * Função executada automaticamente quando o Web App recebe uma requisição GET.
 *
 * Serve como teste simples para verificar se o Web App está publicado e acessível.
 * Ao abrir a URL do Web App no navegador, esta mensagem deve aparecer.
 */
function doGet(e) {
  return ContentService.createTextOutput("Web App da planta funcionando!");
}


/**
 * Gera um diagnóstico usando a API Gemini a partir dos dados consolidados na aba "resumo".
 *
 * Esta função não é chamada diretamente pelo ESP32.
 * Ela deve ser executada manualmente no Apps Script ou por um acionador de tempo,
 * por exemplo, todos os dias às 08h e às 20h.
 *
 * Fluxo geral:
 * 1. Lê a chave GEMINI_API_KEY nas propriedades do script.
 * 2. Lê os dados consolidados na aba "resumo".
 * 3. Monta um prompt com os dados da planta.
 * 4. Envia o prompt para o modelo Gemini 2.5 Flash.
 * 5. Limpa formatações indesejadas do texto retornado.
 * 6. Grava o resultado na aba "Diagnóstico" e atualiza a aba "diagnosticoIA".
 */
function gerarDiagnosticoGemini() {
  // Recupera a chave da API Gemini armazenada nas propriedades do script.
  // A chave não deve ser escrita diretamente no código.
  var apiKey = PropertiesService.getScriptProperties().getProperty("GEMINI_API_KEY");

  // Interrompe a execução caso a chave não tenha sido configurada.
  if (!apiKey) {
    throw new Error("Chave GEMINI_API_KEY não encontrada nas propriedades do script.");
  }

  // Obtém a planilha vinculada ao script.
  var planilha = SpreadsheetApp.getActiveSpreadsheet();

  // Obtém as abas utilizadas pelo diagnóstico.
  var resumo = planilha.getSheetByName("resumo");
  var diagnostico = planilha.getSheetByName("Diagnóstico");
  var diagnosticoIA = planilha.getSheetByName("diagnosticoIA");

  // A aba "resumo" é obrigatória, pois contém os indicadores usados no prompt.
  if (!resumo) {
    throw new Error("Aba resumo não encontrada.");
  }

  // Cria a aba de histórico de diagnósticos caso ela ainda não exista.
  if (!diagnostico) {
    diagnostico = planilha.insertSheet("Diagnóstico");
    diagnostico.getRange("A1:C1").setValues([
      ["Data/Hora", "Planta", "Diagnóstico IA"]
    ]);
  }

  // Cria a aba com o último diagnóstico caso ela ainda não exista.
  // Esta aba é útil para exibir apenas o texto mais recente no Looker Studio.
  if (!diagnosticoIA) {
    diagnosticoIA = planilha.insertSheet("diagnosticoIA");
    diagnosticoIA.getRange("A1").setValue("Diagnóstico IA");
  }

  // Lê os indicadores consolidados da aba "resumo".
  // As posições B2 até B15 precisam ser mantidas conforme a estrutura da planilha.
  var nomePlanta = resumo.getRange("B2").getValue();
  var ultimaLeitura = resumo.getRange("B3").getValue();
  var umidadeAtual = resumo.getRange("B4").getValue();
  var estadoAtual = resumo.getRange("B5").getValue();
  var media12h = resumo.getRange("B6").getValue();
  var media24h = resumo.getRange("B7").getValue();
  var menor24h = resumo.getRange("B8").getValue();
  var maior24h = resumo.getRange("B9").getValue();
  var leituras24h = resumo.getRange("B10").getValue();
  var criticas24h = resumo.getRange("B11").getValue();
  var percentualCritico = resumo.getRange("B12").getValue();
  var mediaHoje = resumo.getRange("B13").getValue();
  var tendencia = resumo.getRange("B14").getValue();
  var diagnosticoSimples = resumo.getRange("B15").getValue();

  // Prompt enviado ao Gemini.
  // O texto orienta o modelo a responder de forma curta, natural e sem formatação em markdown.
  var prompt =
    "Você é um assistente para cuidado de plantas em um projeto educacional com ESP32. " +
    "Analise os dados abaixo e escreva uma orientação curta, clara e natural, em português do Brasil. " +
    "Escreva em texto corrido, sem títulos, sem subtítulos, sem listas, sem numeração, sem marcadores e sem markdown. " +
    "Não escreva as palavras 'Diagnóstico', 'Recomendações', 'Recomendação', 'Análise' ou similares como cabeçalho. " +
    "Não use negrito, asteriscos ou formatação especial. " +
    "Mantenha o conteúdo em no máximo 5 frases. " +
    "Primeiro explique brevemente o estado atual da planta. Depois diga o que o usuário deve fazer agora. " +
    "Termine a recomendação com uma frase de efeito cômica, ou uma citação de chaves ou chapolin." +
    "Não invente informações que não estejam nos dados.\n\n" +

    "Nome da planta: " + nomePlanta + "\n" +
    "Última leitura: " + ultimaLeitura + "\n" +
    "Umidade atual do solo: " + umidadeAtual + "%\n" +
    "Estado atual: " + estadoAtual + "\n" +
    "Média das últimas 12h: " + media12h + "%\n" +
    "Média das últimas 24h: " + media24h + "%\n" +
    "Menor umidade nas últimas 24h: " + menor24h + "%\n" +
    "Maior umidade nas últimas 24h: " + maior24h + "%\n" +
    "Número de leituras nas últimas 24h: " + leituras24h + "\n" +
    "Número de leituras críticas nas últimas 24h: " + criticas24h + "\n" +
    "Percentual crítico nas últimas 24h: " + percentualCritico + "%\n" +
    "Média de hoje: " + mediaHoje + "%\n" +
    "Tendência: " + tendencia + "\n" +
    "Diagnóstico simples: " + diagnosticoSimples + "\n";

  // Endpoint da API Gemini usando o modelo Gemini 2.5 Flash.
  var url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + apiKey;

  // Corpo da requisição enviado para a API.
  var payload = {
    contents: [
      {
        parts: [
          {
            text: prompt
          }
        ]
      }
    ]
  };

  // Configurações da requisição HTTP.
  var options = {
    method: "post",
    contentType: "application/json",
    payload: JSON.stringify(payload),
    muteHttpExceptions: true
  };

  // Envia a requisição HTTP para o Gemini.
  var resposta = UrlFetchApp.fetch(url, options);
  var codigo = resposta.getResponseCode();
  var texto = resposta.getContentText();

  // Caso a API retorne erro, registra o erro nas abas de diagnóstico.
  if (codigo !== 200) {
    var mensagemErro = "Erro ao chamar Gemini. Código HTTP: " + codigo + " | Resposta: " + texto;

    diagnostico.appendRow([
      new Date(),
      nomePlanta,
      mensagemErro
    ]);

    diagnosticoIA.getRange("A2").setValue(mensagemErro);

    return;
  }

  // Converte a resposta JSON da API para objeto JavaScript.
  var json = JSON.parse(texto);

  // Extrai o texto gerado pelo Gemini.
  var textoGerado = json.candidates[0].content.parts[0].text;

  // Remove formatações e cabeçalhos indesejados, caso o modelo gere mesmo assim.
  textoGerado = textoGerado
    .replace(/\*\*/g, "")
    .replace(/Diagnóstico:/gi, "")
    .replace(/Recomendações:/gi, "")
    .replace(/Recomendação:/gi, "")
    .replace(/Análise:/gi, "")
    .replace(/^\s*[-*]\s*/gm, "")
    .replace(/^\s*\d+\.\s*/gm, "")
    .trim();

  // Registra o diagnóstico no histórico.
  diagnostico.appendRow([
    new Date(),
    nomePlanta,
    textoGerado
  ]);

  // Atualiza a aba que contém apenas o diagnóstico mais recente.
  diagnosticoIA.getRange("A2").setValue(textoGerado);
}


/**
 * Testa se a chave Gemini foi configurada nas propriedades do script.
 *
 * Esta função não chama a API Gemini.
 * Ela apenas verifica se a propriedade GEMINI_API_KEY existe e grava o resultado
 * na célula C1 da aba "diagnosticoIA".
 */
function testarChaveGemini() {
  // Tenta recuperar a chave Gemini nas propriedades do script.
  var apiKey = PropertiesService.getScriptProperties().getProperty("GEMINI_API_KEY");

  // Obtém a planilha ativa.
  var planilha = SpreadsheetApp.getActiveSpreadsheet();

  // Obtém a aba usada para mostrar o último diagnóstico.
  var diagnosticoIA = planilha.getSheetByName("diagnosticoIA");

  // Cria a aba caso ela ainda não exista.
  if (!diagnosticoIA) {
    diagnosticoIA = planilha.insertSheet("diagnosticoIA");
    diagnosticoIA.getRange("A1").setValue("Diagnóstico IA");
  }

  // Grava o resultado do teste na célula C1.
  if (apiKey) {
    diagnosticoIA.getRange("C1").setValue("Chave Gemini encontrada");
  } else {
    diagnosticoIA.getRange("C1").setValue("Chave Gemini NAO encontrada");
  }
}

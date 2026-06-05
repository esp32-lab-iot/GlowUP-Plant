#!/bin/bash

set -e

echo "Configurando arquivos secrets.h..."

if [ ! -f "fake_secrets.h" ]; then
  echo "ERRO: fake_secrets.h não encontrado na raiz do projeto."
  exit 1
fi

# Cria secrets.h na raiz, se ainda não existir
if [ ! -f "secrets.h" ]; then
  cp fake_secrets.h secrets.h
  echo "Criado: ./secrets.h"
else
  echo "Já existe: ./secrets.h"
fi

# Cria secrets.h nas pastas que possuem .ino com include de secrets.h
find . -name "*.ino" -type f | while read -r ino_file; do
  dir=$(dirname "$ino_file")

  if grep -q '#include "secrets.h"' "$ino_file"; then
    if [ ! -f "$dir/secrets.h" ]; then
      cp fake_secrets.h "$dir/secrets.h"
      echo "Criado: $dir/secrets.h"
    else
      echo "Já existe: $dir/secrets.h"
    fi
  fi
done

echo ""
echo "Pronto."
echo "Agora edite os arquivos secrets.h criados e coloque seus dados reais:"
echo ""
echo "  WIFI_SSID"
echo "  WIFI_PASS"
echo "  GOOGLE_SCRIPT_URL"
echo ""
echo "ATENÇÃO: os arquivos secrets.h são ignorados pelo Git e não serão enviados ao GitHub."

# File Integrity Audit

Inventário recursivo de arquivos com SHA-256, comparação de tamanho e identificação de inclusões, alterações e remoções.

## Executar

Requisitos: C17 e OpenSSL.

```sh
make
build/integrity snapshot ./arquivos > ./baseline.txt
build/integrity verify ./arquivos ./baseline.txt > resultado.json
```

## Funcionamento

Mantenha o manifesto fora da pasta auditada. Links simbólicos e arquivos especiais são recusados. Retorno: 0 para igualdade, 1 para diferenças e 2 para erro. O manifesto deve permanecer sob seu controle.

## Persistência de resultados

O arquivo de operações está em [vercel-home-telemetry-api.vercel.app](https://vercel-home-telemetry-api.vercel.app/laboratory.html?project=c-file-integrity-audit). As migrações Supabase estão no [repositório da API](https://github.com/brunnojob/vercel-home-telemetry-api/tree/main/supabase/migrations).

```sh
python cloud/sync.py enqueue resultado.json --project c-file-integrity-audit
python cloud/sync.py sync
```

Defina `BRUNNODEV_ACCESS_TOKEN` com sua sessão. A fila SQLite conserva os relatórios até confirmação do servidor; o mesmo conteúdo não gera registros duplicados. Tokens não são gravados no código.

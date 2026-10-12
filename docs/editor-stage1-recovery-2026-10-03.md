# Etapa 1 — incremento 2/2: journal e recuperação controlada

Autorização direta: “Inicie o incremento 2”, em 2026-10-03. Continuação do
candidato de escala; não inventa aceite humano individualizado do incremento 1.
Base f155bd866d32c9326e5853a2882e6adbfca6ba23 + cumulativo de escala
03825ef9aa435b1a6b6fa5ee3aeced03847a9d20c91a8470907d492de81ce0ce.
Fonte independente `work/editor-etapa1/recuperacao/inde`, branch
`codex/editor-stage1-recovery-2026-10-03`. Originais/predecessores congelados.

Journal versionado de checkpoints completos em SQLite auxiliar por identidade
de Projeto, sem mudar project.sqlite3/schema 15 e sem WAL no banco principal.
Transações DELETE/synchronous=EXTRA, checksum SHA-256, revisão monotônica,
baseline completa e seleção Unicode. Não é change_log, histórico de undo
persistente ou replay de teclas. Último checkpoint confirmado é a unidade de
recuperação; existe janela de perda antes dessa confirmação.

Worker isolado e fila limitada para checkpoints. Lease local impede recuperar
sessão de outro processo ainda ativo. Descoberta explica pendências; aplicar,
criar cópia de resgate sem vínculos e descartar exigem decisão humana. Conflitos,
identidade incompatível, versão inválida e corrupção falham fechado, conservando
evidência. Falha do journal não deve impedir salvamento normal no banco principal.

Incremento 2/2 exige revisão técnica ampla nos dois Projetos e gate humano antes
de outro incremento/Etapa 2. Sem commit, push, release, migração narrativa,
semântica automática, catálogo novo ou expansão de bindings. Configuração:
GPT-6.1 Sol / high, custo alto, mesma tarefa, integridade/persistência/interrupção.
Testes: CTest, SIGKILL e transação interrompida em subprocessos descartáveis,
GTK em display isolado, regressão consolidada, latências e checksums.

## Contrato operacional e limites

- Local: `<Projeto>/recovery/editor-v1/<project_uuid>.sqlite3`; schema auxiliar 1.
  Não remover, editar ou transportar arquivos isolados do journal manualmente;
  preservar o Projeto completo para manter identidade e evidência.
- Checkpoint completo guarda Documento atual, baseline, seleção dirigida em
  offsets Unicode, revisão e revisão confirmada pelo banco principal. SHA-256
  detecta corrupção acidental; não é assinatura/autenticação contra adulteração.
- Debounce de 350 ms e tentativa periódica de 1.000 ms durante escrita, mais
  checkpoint antes do autosave. Esses tempos são agendamento, não garantia de
  confirmação dentro de um prazo. A confirmação requer commit concluído.
- Fila: um ativo e um pendente substituível. Worker tem conexões e objetos de
  serviço próprios. Somente GTK captura/consome snapshots; workers não acessam
  widgets. Lease local Linux via flock, não simples marcador “sessão fechada”.
- Autosave: checkpoint -> CAS do Documento -> ACK do journal. Checkpoint mais
  antigo não rebaixa revisão/base. ACK rebasa a baseline preservando texto mais
  recente. Última confirmação não mascara edições posteriores ou erro de save.
- Recuperação: lease inativa -> validar versão/identidade/checksum -> prévia ->
  escolha -> CAS transacional ou cópia independente -> registrar decisão. Não
  existe merge silencioso. Registro já aplicado mas não confirmado pode ser
  reconhecido por igualdade exata de texto e camadas, sem ignorar outras edições.
- Bancos separados não têm commit conjunto. Se a gravação principal terminar
  mas arquivar o journal falhar, a mensagem informa o original/cópia já criado,
  mantendo o checkpoint. Nova cópia explicitamente solicitada pode duplicar um
  resgate anterior; verificar a Biblioteca antes de repetir.
- 64 MiB de JSON por registro (atual + base), 256 sessões retidas, até 64
  pendências por listagem, 512 MiB de páginas SQLite por conexão do componente.
  Limites conservam evidência e desabilitam novas gravações no journal; não
  substituem a capacidade de salvar normalmente o Documento. Registros clean,
  restaurados e descartados continuam ocupando espaço/sessões. Exportação/purge
  explícitos ainda não existem: retenção longa é uma limitação deste candidato,
  não uma manutenção diária considerada concluída.
- Identidade/versão/corrupção desconhecidas, lease ausente ou caminhos simbólicos
  são recusados. Não há reparo automático, coleta de dados externos, vínculo
  inferido ou reaproveitamento silencioso de journal de outro Projeto.
- Não reconstrói undo, IME pendente, teclas anteriores ao primeiro checkpoint,
  sessão sem journal disponível ou seleção posterior ao checkpoint confirmado.
- Validar SIGKILL não valida desligamento físico, fsync/hardware/FS defeituosos,
  NFS/compartilhamentos, Windows/macOS, GTK completo com sanitizers, LSan/TSan,
  sessão longa ou experiência humana. Review por programa não é aceite humano.

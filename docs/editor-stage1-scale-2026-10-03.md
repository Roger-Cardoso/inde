# Etapa 1 — corte de escala: autosave, restauração e clipboard

Autorização direta em 2026-10-03: “aprovado. Inicie o próximo corte em grande escala”.
Registra aceite do grande corte anterior e liberação do próximo incremento;
não inventa resultados humanos individualizados. Reinicia o ciclo de revisão:
este é o primeiro incremento após aquele portão aprovado.

Base: HEAD f155bd866d32c9326e5853a2882e6adbfca6ba23 + patch cumulativo do grande
corte. Clone independente em `work/editor-etapa1/escala/inde`, branch
`codex/editor-stage1-scale-2026-10-03`. Predecessor/originais ficam congelados.

Escopo integrado: autosave em worker isolado com fila limitada e comparação
otimista atômica no banco; confirmação sem limpar edição mais nova; barreira
de gravação para salvar/trocar/fechar; restauração textual incremental com
fallback; clipboard explícito de texto/estilos, sem âncoras ou bindings copiados.

Sem schema/WAL novo, journal, semântica automática, catálogo alterado, paginação,
exportação, commit, push ou release. Esperar fechamento/troca pode bloquear a
interface; não fingir que toda transição virou assíncrona. Journal e recuperação
após queda continuam pendentes. GTK e serviços mutáveis não cruzam para workers.

Verificação: CTest e testes focados de concorrência/falha/conflito/barreira,
testes GTK de edição/clipboard/seleção e regressão em Projeto vazio/Atlas,
manuscrito longo, medições separadas de captura/UI versus worker e checksums.
Clipboard automatizado em display isolado para não sobrescrever o do usuário.

Configuração: GPT-6.1 Sol / high, custo alto, mesma tarefa, por integridade,
concorrência, persistência e fronteira assíncrona. Sem escalada automática.

## Contratos implementados

`DocumentSaveCoordinator` é o único escritor assíncrono da sessão de Escrita:
thread própria, valores copiados (Projeto, nós estruturais, Documento, baseline,
epoch/revisão), serviços próprios e conexão SQLite própria. Não compartilha
GtkWidget, ProjectSession mutável nem conexão da interface. Há no máximo um
trabalho ativo e um pendente; o pendente mais recente substitui o anterior.
Trabalho iniciado nunca é cancelado. Destruição/limpeza drenam gravações.

Cada gravação revalida os contratos existentes e usa `save_if_unchanged`:
identidade e estado autoral completo são comparados sob a mesma transação
SQLite que grava texto, camadas e change_log. A comparação independe da ordem
de spans/âncoras/referências, mas não ignora conteúdo ou timestamps. Detecção
não depende só de segundos do updated_at. Documento apagado não é recriado.
O snapshot pendente é rebaseado apenas no último estado persistido pelo próprio
worker; nenhuma alteração externa é mesclada silenciosamente.

`acknowledge_saved` atualiza a baseline durável e timestamps, não o texto atual
ou o histórico. Sucesso antigo seguido de falha mantém a baseline daquele sucesso.
A UI recebe resultados a cada 30 ms, checa identidade/epoch, mostra salvamento
em andamento ou falha não modal e conserva alterações locais. Salvar/trocar/fechar
esperam a fila e recusam a transição em caso de erro. A cópia independente de
Projeto reinicia a UI de Escrita após a barreira, para não reutilizar contexto antigo.

Restauração incremental altera somente a faixa diferente, em limites UTF-8.
Ainda captura/compara o texto inteiro e restaura camadas completas; portanto não
é uma promessa de custo constante, piece table ou pintura incremental. Seleção
direcionada e identidades de vínculos persistem no histórico.

Clipboard v1 aceita texto UTF-8 e spans de estilo, até 16 MiB e 10 mil spans.
JSON privado tem versão e campos estritos, profundidade limitada e offsets em
caracteres Unicode. Não contém IDs de Projeto/Documento, âncoras, entidades ou
bindings. Texto puro é padrão também nos sinais nativos de copy/cut/paste.
Estilos exigem ação explícita e substituem apenas estilos na área inserida;
texto puro pode herdar estilo já existente no destino. Recortar só altera texto
depois de adquirir clipboard. Paste é uma operação única no histórico INDE.
Leitura GIO é assíncrona, cancelável e limitada em blocos; checa vida útil,
Documento, epoch, revisão e seleção antes de aplicar. Troca/edição invalida a
leitura e mudança de seleção tardia é explicada sem inserir em posição inesperada.

## Limites e próxima revisão

Clipboard com preservação deliberada de bindings ainda não foi autorizado/
implementado; não equiparar este recorte conservador a todo o contrato semântico.
Conflito externo não tem fusão automática ou UI de resolução: mantenha o app
aberto e copie o texto local antes de reconciliar. Não abrir Projetos reais neste
candidato até revisar as cópias descartáveis. Migrações seguem em schema 15;
CMake continua em 0.1.0 e o binário é candidato local, não release/AppImage.
Gate humano próprio permanece pendente, ciclo 1/2. Não começar Etapa 2.

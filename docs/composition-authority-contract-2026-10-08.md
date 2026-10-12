# Composição persistente — contrato de autoridade v1

Registrado antes da migração correspondente. Base aceita: bf05f632c318f3d481bb41f382c7df41ad2905c5.

1. Documento continua Documento. Importar inteiro ou recortar uma seleção cria
   um bloco com versão independente e uma origem documental congelada completa.
   Não apaga, arquiva, renomeia nem transfere autoridade do Documento original.
2. A Composição controla ordem, ocorrências e perfil de leitura. Cada bloco
   controla seu conteúdo/versionamento. `live` acompanha a versão atual do
   **bloco**, nunca alterações posteriores no Documento de origem. Fixar conserva
   uma versão; bifurcar cria identidade independente; referência incorpora zero texto.
3. A leitura composta é projeção somente leitura. Não existe double-write em
   Documento e bloco. Preparar saída/materializar cria um Documento independente,
   sem destruir a Composição, e exige reconhecimento de avisos de conservação.
4. Toda origem importada preserva texto, organização e camadas em snapshot
   integral identificado por conteúdo e metadados. Pode ser consultada mesmo
   depois de a origem mudar ou desaparecer. Seus IDs são proveniência histórica,
   não promessa de existência ou binding canônico atual.
5. Cortes usam coordenadas Unicode. Estilos são recortados por interseção;
   âncoras inteiramente contidas são trasladadas. Âncoras, relações e declarações
   que cruzam cortes ficam na origem congelada, com avisos, sem reassociação
   silenciosa. Edição usa o motor de conservação de camadas e marca incertezas.
6. Revisões aplicadas são imutáveis. Rascunho é um estado separado da leitura;
   salvar o rascunho não o aplica. Após reinício, há escolha explícita Aplicar ou
   Descartar. Escrita pendente deve ser persistida antes de navegação/fechamento;
   falha impede descarte, sem alegar que bytes ainda em memória sobreviveram.
7. Persistência usa transação SQLite com conferência da revisão esperada e
   SHA-256 do payload. Sessões obsoletas não sobrescrevem a atual. Erro mantém a
   evidência e exige recarregar, nunca merge ou vencedor automático.
8. Metadados, blocos, revisões, origens e perfil são um agregado versionado,
   bounded e validado. Não substituem backups. Save As leva o agregado inteiro
   para a nova identidade sem mudar as origens congeladas.
9. Exportação usa snapshot preparado, nunca rascunhos não aplicados. Antes de
   materializar/exportar, qualquer mudança no agregado invalida a prévia e o
   reconhecimento. Decisões editoriais da origem não viram aprovação da saída.
10. Interface na área de Escrever, com páginas exclusivas: lista, ordem, edição,
    leitura, origem e saída. Sem novo workspace principal nem múltiplos editores
    concorrendo. Operações menos frequentes ficam em menu, não numa parede de botões.

Limites v1: payload completo até 2 MiB; máximo 256 blocos, 2048 ocorrências,
4096 revisões, 32 variantes e 32 camadas. O limite total inclui camadas, origens
e rascunhos; falha fechado, sem truncar. Não há poda automática. Não há adoção
automática de montagens voláteis já abertas no laboratório. O laboratório
permanece separado; suas regras puras de blocos/camadas são reutilizadas e
verificadas, sua interface e seus estados em memória não viram canônicos.

Gates: migração aditiva/rollback; roundtrip integral; concorrência; corrupção;
rascunho/reinício; fixação/bifurcação; camadas e cortes Unicode; origem alterada
e removida; Save As; projeção/materialização sem mutar origens; leitura de
exportação; GTK vazio/Atlas, temas e geometria do rodapé. Desempenho em bases
grandes, IME físico, HiDPI e tecnologias assistivas continuam gates separados.

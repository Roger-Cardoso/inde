# Comic no INDE · espaço de trabalho

COMIC-02, 02/11/2026.

## Abrir

```sh
bash Abrir-demo-comic.sh
```

O lançador compila incrementalmente, prepara uma amostra sintética em `demo-comic-workspace/` quando necessário e abre diretamente o espaço Comic. A amostra anterior em `demo-comic/` permanece preservada. Para trabalhar em um Projeto existente, use Abrir no próprio INDE e escolha Criar → Comic.

As dependências são as mesmas do INDE especial: Linux compatível, compilador C++20, CMake/Ninja, GTKmm 4.8+, GLib, SQLite, UUID, ICU e Python 3.12+. O script verifica as fontes e não instala pacotes ou acessa a rede.

## Experimentar

1. Confira a barra: **Criar, Planejar, Revisar, Manuscrito, Cronologia, Mapas e Obras**. Abra Criar e alterne entre **Escrita** e **Comic**.
2. Na lateral de Comic, selecione um quadro da amostra ilustrada. Use as setas para ajustar a sequência.
3. No centro, selecione o elemento, arraste para mover e use o canto inferior direito para redimensionar. O encaixe central apresenta uma guia. Use as setas do teclado para ajustes; Shift amplia o passo.
4. Altere o zoom entre Ajustar quadro e uma escala. A rolagem da área de composição permite alcançar as partes fora da janela.
5. No inspetor Elemento, edite a fala, cores, forma, alinhamentos, tamanho, idioma e direção. No quadro da onomatopeia, confira a legenda de sentido. O inspetor Quadro contém título, descrição da arte e proporção; Obra contém o vínculo, título, idioma e fontes complementares.
6. Experimente **Desfazer** e **Refazer**. Ctrl+Z e Ctrl+Shift+Z atuam na composição quando o foco está no espaço Comic. F9 alterna o inspetor; F6 percorre as regiões pelo sistema de comandos do INDE.
7. Deixe uma edição pendente, vá a Criar → Escrita e volte a Comic. O rascunho e o contexto de composição permanecem. Antes de fechar ou trocar o Projeto, salve ou descarte.
8. Use **Salvar comic** ou o comando de salvar o Projeto. O trabalho fica na extensão de comics do `.inde`.
9. Na aba Publicação do inspetor, selecione um Site e use Preparar comic → Abrir prévia. O painel geral Publicação continua oferecendo Site, Edição e Catálogo, sem hospedar o editor.

## Integração com a escrita

Copiar texto do Documento cria uma cópia explícita para a comic e registra sua origem. Ela não altera o Documento e não sincroniza mudanças posteriores automaticamente.

O cálculo tipográfico exato e as fontes incorporadas devem ser conferidos na prévia web. O canvas GTK usa fontes instaladas. A marcação permite tradução, mas não certifica o resultado do motor do navegador nem a tradução de um som inventado.

## Fronteira desta etapa

O espaço de trabalho e a navegação são nativos. A equivalência integral com o estúdio 0.4 e o site público reunindo novels/comics continuam na sequência de migração, detalhada em CONTRATO-COMIC-02.md. Ainda não importe arquivos 0.4 por este espaço; o tradutor desse formato não está implementado.

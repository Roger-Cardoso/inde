# Demo nativa — INDE Publicação 0.4

Esta demo usa uma compilação especial da janela e dos workspaces reais do INDE aceito. A pasta sintética `demo-nativa/Publicacao.inde` é criada na primeira abertura; ela não contém manuscritos do acervo nem usa seu perfil habitual.

## Abrir

```sh
bash Abrir-INDE-especial.sh
```

Use o caminho completo se estiver em outra pasta. O binário Linux deste workspace já está compilado. Para começar sem a amostra, use `bash Abrir-INDE-especial.sh --vazio`: o próprio INDE permite criar um Projeto vazio e cadastrar sua Obra, estrutura e Documentos.

## Experimentar em poucos minutos

1. Na janela do INDE, abra **Escrever**, selecione a Obra fictícia e o Documento “A porta”. Você pode escrever nele usando o editor nativo. Aguarde o salvamento ou use **Salvar**.
2. Clique em **Publicação**, no cabeçalho. Texto pendente de salvamento impede a abertura; o painel não dispara um salvamento por conta própria.
3. Na aba **Site**, preencha nome, autoria e descrição. Escolha um dos temas. Para experimentar em subdiretório, use `/biblioteca` no caminho base. Deixe origem vazia e destino Local para começar sem conta. Clique em **Salvar Site**.
4. Na aba **Edição e fontes**, preencha nome da Edição e um endereço como `estacao-inde`. Escolha a Obra e a estrutura editorial. Marque “O farol”, “A porta” e Mara explicitamente. Clique em **Salvar Edição**.
5. Clique em **Gerar Edição**. A geração usa o leitor Literature incluído e um retrato dos serviços desta mesma sessão. O status informa o caminho do ZIP público. Clique em **Abrir prévia** para navegar pelo site local e conferir os Auxílios de fala, pensamento e POV.
6. Use **Voltar ao INDE**. O mesmo editor, buffer, seleção e histórico permanecem. Altere o texto pelo editor, salve e prepare uma nova saída para incluir a mudança. A geração anterior permanece como retrato anterior, sem substituir a nova.
7. Reabra **Publicação**, feche e reabra o aplicativo: Site e Edição continuam dentro do Projeto. Para transportá-los, feche o aplicativo e copie a pasta `.inde` completa.
8. Para experimentar CRUD, altere os campos e salve; crie outro Site ou Edição pelos botões próprios. **Retirar Edição** e **Retirar Site** pedem conferência. Sites com Edições ativas têm a retirada bloqueada. Marque **Mostrar retirados** para selecionar e restaurar. A retirada da Edição preserva os Documentos do INDE e as reservas de endereço.
9. **Descartar alterações** abandona apenas o formulário pendente. Troca de seleção, geração e retorno exigem que o formulário seja salvo ou descartado. Se outro processo tiver alterado a revisão, recarregue antes de salvar.

O destino GitHub permite configurar origem HTTPS, repositório e branch. Esta entrega prepara os arquivos localmente; não faz autenticação, push ou implantação. **Salvar cópia**, no INDE, preserva as escolhas mas devolve o destino da cópia ao modo Local para reconexão independente.

## Onde fica cada coisa

- `data/project.sqlite3`: banco autoral do INDE, schema 28.
- `data/publicacao.sqlite3`: extensão local de Site/Edição, schema 2, vinculada à identidade do Projeto. Contém revisões, histórico e retratos preparados.
- `data/publicacao-retratos/`: entradas privadas materializadas para o gerador. Não publicar.
- `outputs/web/<preparação>/site/`: somente o site público gerado.
- `outputs/web/<preparação>/site-publico.zip`: pacote para inspecionar e hospedar.
- `perfil-especial/`: preferências isoladas desta demo, fora do seu perfil habitual.

Para compartilhar o site, use apenas o ZIP público ou sua pasta `site/`. A pasta `.inde` completa contém texto, histórico e material privado.

## Limites deste corte

Cada Edição seleciona uma Obra e uma estrutura editorial. Vários Sites e Edições podem ser cadastrados. A aba Catálogo do Site reúne as Edições escolhidas em uma biblioteca; leia `ROTEIRO-CATALOGO.md` para selecionar, ordenar, retirar vínculos e gerar o Site combinado. O projetor atual aceita capítulos planos com um Documento por unidade, texto simples, personagens, fala/pensamento simples, perspectiva documental explícita e âncoras compatíveis. Recursos ou marcações sem equivalência são recusados pelo gerador, sem conversão silenciosa.

Os comandos de escrita continuam sendo os do INDE. Esta integração não acrescenta um segundo editor ou importa o acervo Literature. A escolha de outra estrutura ou Obra limpa a seleção de Documentos do formulário e precisa ser salva explicitamente. Reservas antigas de números não são recicladas. A lista de seleção mostra os primeiros 200 Documentos, preservando escolhas já salvas fora da lista.

A leitura da configuração não cria automaticamente o banco da extensão. Preparar guarda um retrato privado; gerar não implanta. Uso simultâneo em vários processos, sessões prolongadas, outros sistemas e instalador com runtime incorporado ainda não foram validados. O laboratório exige Linux com as dependências da base e Python 3.12+. Leia `README.md` para compilar sem depender do workspace original.

O catálogo múltiplo tem uma amostra própria: `bash Abrir-demo-catalogo.sh`. A entrega 0.4 lê a extensão antiga sem alterá-la e faz a migração com cópia de recuperação na primeira gravação.

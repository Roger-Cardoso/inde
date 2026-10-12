# Fase contínua — tempo contextual e consulta gráfica

Base aprovada pelo autor: candidato privado `61525492724b843de3406bffca27884dd83ba824`.
Escopo autorizado: as cinco frentes anunciadas, executadas numa fase contínua.
Schema 25, sem migração. Identificação de entidades permanece por último;
reforma gráfica/técnica de cartografia e fusos não foram antecipados.

## 1. Criação e vínculo dentro da escrita

Camadas Semânticas → anotação do trecho/perspectiva → Vincular data / horário.
Reutilizar uma referência ou ativar **Criar nova data ou intervalo aqui**.
Escolher eixo, sistema, nome, data e papel textual: cena, menção, memória ou
referência. Precisão parcial e horário desconhecido permanecem desconhecidos.

`WritingService::create_date_from_text` chama a gravação composta do repositório.
Documento, referência, extensão de precisão e vínculo são gravados na mesma
transação, com comparação do Documento esperado, identidade do Projeto,
calendário revisado, eixo existente, capacidade e invariantes semânticas.
Falhas revertem também a criação; cancelar não escreve. Cadastro órfão não é
aceito pela API composta. Uma captura registra a operação inteira no histórico.

O editor preserva seleção Unicode e histórico de edição, drena o autosave e
invalida seu baseline antigo somente após sucesso. A recuperação usa os codecs
existentes. **Desfazer textual retira o vínculo, mas conserva a referência**,
como acontece com entidades criadas pelo texto; refazer reutiliza sua identidade.
O histórico do Projeto tem reversão própria e suas guardas de conflito.

O comando **Usar outra data no lugar de um vínculo deste trecho** permite
selecionar explicitamente o vínculo anterior e trocá-lo na mesma operação;
desfazer restaura o vínculo anterior, sem editar nenhuma referência compartilhada.

Uma referência compartilhada não é editada ao vincular outra ao trecho. Edição
da referência continua exigindo revisão dos seus usos. Avisos de continuidade
continuam consultivos: viagem no tempo não impede salvar.

## 2. Atalhos contextuais e marcos

- Entidade: botão Datas e horários no resumo e ação correspondente no menu.
- Relação: ação Datas e horários no menu da relação e no menu de contexto do card.
- Linhas/unidades: uma ação compacta para o item selecionado no detalhe da
  estrutura narrativa, sem repetir um botão em cada linha da lista.

A consulta mostra vínculos, referências em ordem contextual, significado e nota;
permite reutilizar ou criar uma referência sem sair do elemento. Criação e
associação também são atômicas. Retirar um vínculo requer confirmação e não
apaga a referência. O papel formal opcional não é inferido do significado livre.

Seletores de marcos do Planejamento oferecem **Escolher marco no calendário**,
carregado somente ao expandir. O dia revela marcos existentes; escolher o marco
preserva sua identidade. Não há conversão automática de data em acontecimento,
nem escolha arbitrária quando há vários marcos no dia. A busca textual permanece
disponível para marcos sem calendário e resultados acima do limite visual.

## 3. Densidade progressiva

O rascunho novo usa uma única superfície compartilhada entre escrita e detalhes.
Depois da escolha, a data/intervalo aparece no título compacto do seletor;
recolher mantém o rascunho e expandir permite alterá-lo. Precisão, incerteza,
papel formal e nota são opções progressivas. Nenhum painel permanente foi
acrescentado à escrita. Validação malsucedida mantém os campos para correção.
O rodapé do modal permanece separado da área rolável.
Seletores com nomes extensos mantêm largura limitada, com o texto integral em
tooltip e descrição acessível. O corpo rolável reserva espaço final para que
o último campo não fique encoberto pelo rodapé; a matriz verifica sua geometria.

## 4. Consulta mensal

Cronologia → ações do eixo → Ver mês do calendário. A apresentação seleciona
origem ou adoção; o filtro permite todos, vigência principal, apresentação
paralela ou fora da vigência. O filtro refere-se à **apresentação escolhida**,
não declara que os fatos da obra deixaram de existir.

A grade conta marcos, acontecimentos e referências autorais distintas em cada
dia. Referências com precisão de mês/ano e intervalos cobrem os dias que
intersectam, respeitando limites exclusivos e relógios próprios. † e a descrição
acessível da célula indicam cortes de adoção. O dia abre registros com suas datas
de origem, sem converter ou reescrever o cadastro canônico.

Datas usam alinhamentos declarados no eixo ou correlação computacional terrestre
ancorada no calendário desse eixo. Isso é conversão de consulta, não inferência
de papel temporal. Definição reutilizada com alinhamentos diferentes não escolhe
um vencedor. Aproximações, estações e datas sem alinhamento comprovável ficam
na seção separada de referências sem posição, **não atribuídas ao mês**.
Limites de 500 marcos/acontecimentos são informados; mais de 500 referências
falham explicitamente, sem declarar uma consulta truncada como completa.

## 5. Navegação e ordenação

A posição mensal é uma preferência local por UUID da versão do calendário,
salva atomicamente fora do banco autoral, inclusive para reabrir o aplicativo.
Uma escolha explícita já existente prevalece sobre a posição lembrada. Não são
memorizadas datas escolhidas implicitamente. Cache ausente, inválido ou sem
permissão de escrita não bloqueia trabalho; novos sistemas têm outra chave.

Referências comparáveis do mesmo eixo são ordenadas pela posição temporal exata,
incluindo subdivisões do relógio. Não se comparam eixos distintos por suposição.
Referências não comparáveis ficam separadas na ordenação e mantêm uma ordem
determinística por sistema/data/nome/identidade, sem afirmar sua cronologia.

## Revisão humana desta fase

Testes técnicos e capturas não substituem este passe, nem constituem publicação:

1. No Projeto vazio, criar eixo/calendário e marcar um trecho e uma perspectiva.
2. Criar data com hora parcial, intervalo e mês sem dia; cancelar outra criação.
3. Recolher/abrir o seletor; testar Tab, Escape, grade pelo teclado e rolamento.
4. Confirmar, desfazer/refazer, digitar mais, salvar e reabrir no mesmo trecho.
5. Vincular a mesma data a entidade/relação/unidade/linha e retirar só um vínculo.
6. No Atlas, consultar intervalos e cortes de adoção; alternar apresentação e
   filtros sem perder a distinção entre origem, vigência e conversão.
7. Navegar num sistema, mudar para outro e voltar; reiniciar e conferir o mês.
8. Conferir contraste e foco em claro/escuro, em janela menor e documento longo.

O manifesto da entrega registrará configurações, testes, escopo dos sanitizers,
capturas efetivamente inspecionadas, identidade dos binários e limites restantes.

# INDE

Ambiente de criação, planejamento e publicação de narrativas. A edição 0.2 usa o Especial INT-01 (schema 37) e reúne editor unificado, planejamento contextual, espaço Comic e geração local de Sites e Edições.

## Começar no Linux

Instale as [dependências de compilação](docs/guias/GUIA.md#transportar-a-demo-e-preparar-a-ponte). Depois, na raiz do repositório:

```sh
sh Preparar-INDE-especial.sh
sh Abrir-INDE-especial.sh
```

Use `sh Abrir-INDE-especial.sh --vazio` para iniciar sem abrir o projeto de demonstração.

## Estrutura do repositório

| Pasta | Conteúdo |
| --- | --- |
| [`cpp/`](cpp/) | Aplicativo nativo GTK, integração com o núcleo e configuração CMake |
| [`motor/`](motor/) | Processamento editorial e geração de publicações |
| [`interface/`](interface/) e [`recursos/`](recursos/) | Interface web e recursos dos sites gerados |
| [`fontes/`](fontes/) e [`metadata/`](metadata/) | Fontes de referência, manifestos e proveniência |
| [`scripts/`](scripts/) | Demonstrações, importação e manutenção |
| [`tests/`](tests/) | Testes de integração e ferramentas de verificação |
| [`docs/`](docs/) | Guias, arquitetura, roteiros e validação |

## Documentação

- [Guia do INDE](docs/guias/GUIA.md)
- [Roteiro de uso no GTK](docs/roteiros/ROTEIRO-GTK.md)
- [Arquitetura e integração](docs/arquitetura/INTEGRACAO-01.md)
- [Validação da edição integrada](docs/validacao/VALIDACAO-INTEGRACAO01.md)

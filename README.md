# INDE 0.2.0

Esta versão publica o INDE Especial INT-01 (laboratório 0.10, schema 37) na branch principal. Inclui planejamento contextual e visual, editor unificado P03c, espaço Comic e geração local de Sites e Edições.

## Executar no Linux

O código da edição especial está em [especial/](especial/). Consulte os [requisitos e roteiros](especial/README.md). Em um ambiente com as dependências de compilação instaladas:

```sh
cd especial
sh Preparar-INDE-especial.sh
sh Abrir-INDE-especial.sh
```

O diretório raiz preserva a base histórica schema 28. As fontes congeladas que o Especial verifica e extrai estão em `especial/fontes/`. O [contrato de integração](especial/INTEGRACAO-01.md) descreve o conteúdo e os limites desta entrega. A distribuição 0.2.0 é em código-fonte.

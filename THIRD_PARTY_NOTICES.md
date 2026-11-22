# INDE — créditos e licenças de terceiros

Este arquivo identifica as principais ferramentas e bibliotecas usadas para
compilar ou executar o INDE. O código original do INDE é distribuído sob a
licença restrita não comercial/educacional em [`LICENSE`](LICENSE). Cada
componente de terceiros continua sob sua própria licença; esta lista não
substitui os textos legais instalados pelos pacotes da sua distribuição.

| Componente | Uso | Licença / atribuição |
|---|---|---|
| C++20 / GNU libstdc++ | linguagem e biblioteca padrão | GPL com GCC Runtime Library Exception; consulte a instalação do GCC/libstdc++ |
| CMake | configuração e geração | BSD 3-Clause; <https://cmake.org/licensing/> |
| Ninja (opcional) | execução do build | Apache License 2.0; <https://github.com/ninja-build/ninja/blob/master/COPYING> |
| GTK 4 | toolkit da interface | GNU LGPL 2.1 ou posterior; <https://www.gtk.org/> |
| gtkmm 4 | bindings C++ de GTK | GNU LGPL 2.1 ou posterior; <https://gtkmm.gnome.org/> |
| KaTeX 0.19.1 | somente o piloto web separado de desenvolvimento, JavaScript/CSS/fontes | MIT; [licença incluída](tools/obj01-pilot/vendor/katex/LICENSE), [proveniência e integridade](tools/obj01-pilot/vendor/katex/PROVENANCE.json) |
| GLib, Gio, Pango e Cairo | runtime, texto e desenho | LGPL 2.1 ou posterior, conforme cada componente e pacote da distribuição |
| SQLite 3 | banco local no `.inde` | domínio público; <https://www.sqlite.org/copyright.html> |
| libuuid (util-linux) | UUIDs persistentes | BSD 3-Clause; consulte o pacote `uuid-dev`/util-linux |
| Enchant 2 | provedor ortográfico opcional | GNU LGPL 2.1 ou posterior; <https://github.com/AbiWord/enchant> |
| Hunspell pt-BR | dicionário em runtime | licença tripla GPL/LGPL/MPL; <https://github.com/LibreOffice/dictionaries> |

`clang-format`, `pkg-config`, CTest e os compiladores do sistema são apenas
ferramentas de desenvolvimento e mantêm as licenças de suas distribuições.

As fixtures `examples/` e os geradores em `tools/` são conteúdo original e
procedural do projeto. A fixture colossal é criada localmente e permanece fora
do Git; nenhuma obra protegida de terceiros foi incorporada.

Ao redistribuir o INDE, preserve este arquivo e os avisos/licenças de terceiros.
A licença restrita do INDE não relicencia componentes externos nem impede seu
uso conforme os termos próprios.

O editor de objetos no INDE usa C++/GTK, sem WebKit/JavaScript. A matemática nativa usa [MicroTeX](https://github.com/NanoMichael/MicroTeX), núcleo MIT ([licença](cpp/vendor/microtex/LICENSE), [proveniência](cpp/vendor/microtex/PROVENANCE.json), [ajustes documentados](cpp/vendor/microtex/PATCHES.json)), e [TinyXML2](https://github.com/leethomason/tinyxml2), MIT ([licença](cpp/vendor/tinyxml2/LICENSE.txt), [proveniência](cpp/vendor/tinyxml2/PROVENANCE.json)). As licenças próprias das fontes são conservadas em `cpp/vendor/microtex/res/fonts/licences/`, `res/greek/LICENSE` e `res/cyrillic/LICENSE`; não devem ser confundidas com a licença do núcleo. A ponte Cairo foi adaptada para cairomm 1.16/pangomm 2.48. As licenças e fontes são incluídas na instalação.

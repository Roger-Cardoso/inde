#include "object_math_native.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
int main() {
  try {
    const std::vector<std::string> good = {
        R"(x^2+y_1)",
        R"(\frac{-b\pm\sqrt{b^2-4ac}}{2a})",
        R"(\sum_{i=1}^n i)",
        R"(\int_0^1 x^2\,dx)",
        R"(\begin{pmatrix}a&b\\c&d\end{pmatrix})",
        R"(\begin{aligned}x+y&=3\\x-y&=1\end{aligned})",
        R"(\text{área} = \pi r^2)"};
    for (const auto &s : good)
      inde::ui::validate_native_math(s);
    const std::vector<std::string> bad = {"",
                                          R"(\frac{)",
                                          R"(\input{/etc/passwd})",
                                          R"(\includegraphics{x})",
                                          R"(\href{https://example.com}{x})",
                                          R"(\def\x{\x}\x)",
                                          R"(\newcommand{\x}{x})",
                                          R"(\begin{document}x\end{document})",
                                          R"(\begin{matrix}x\end{cases})",
                                          std::string(33, '{') + "x" +
                                              std::string(33, '}'),
                                          std::string(16385, 'x'),
                                          std::string("x\xff", 2)};
    for (const auto &s : bad) {
      bool refused = false;
      try {
        inde::ui::validate_native_math(s);
      } catch (const std::exception &) {
        refused = true;
      }
      if (!refused)
        throw std::runtime_error("Fonte insegura/incompleta não recusada");
    }
    std::cout << "Native math input boundaries: " << good.size()
              << " accepted and " << bad.size()
              << " refused; no TeX processes or web runtime\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
